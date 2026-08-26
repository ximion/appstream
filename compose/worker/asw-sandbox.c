/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:asw-sandbox
 * @short_description: Sandbox restrictions of the AppStream media worker.
 *
 * The media worker parses untrusted images, fonts and videos, so we reduce what
 * an exploit against one of those parsers would be able to reach.
 *
 * This is possible because the worker needs no filesystem  write access whatsoever:
 * all input arrives as sealed memfds, and every result is encoded into a writable
 * descriptor that the client opened for us and passes along with the request.
 *
 * Reads and execute are deliberately left untouched, so libvips can still dlopen
 * its modules, fontconfig can read its configuration, and ffprobe can still be
 * spawned. Sandboxing is best-effort: on a kernel without Landlock, the worker
 * simply runs unrestricted, because refusing to work at all would break media
 * processing on a large number of perfectly ordinary systems.
 */

#define _GNU_SOURCE
#include "config.h"
#include "asw-sandbox.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_LANDLOCK
#include <linux/landlock.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

/* Landlock ABI levels that introduced the features we use */
#define ASW_LANDLOCK_ABI_REFER	  2
#define ASW_LANDLOCK_ABI_TRUNCATE 3
#define ASW_LANDLOCK_ABI_NET	  4

/**
 * asw_landlock_create_ruleset:
 */
static int
asw_landlock_create_ruleset (const struct landlock_ruleset_attr *attr, size_t size, guint32 flags)
{
	return (int) syscall (SYS_landlock_create_ruleset, attr, size, flags);
}

/**
 * asw_landlock_restrict_self:
 *
 * Enter the domain described by @ruleset_fd. Irreversible, and it only affects the
 * calling thread and its future children.
 */
static int
asw_landlock_restrict_self (int ruleset_fd, guint32 flags)
{
	return (int) syscall (SYS_landlock_restrict_self, ruleset_fd, flags);
}

/**
 * asw_landlock_fs_write_mask:
 *
 * Every filesystem access right that can modify something, as far as the running
 * kernel knows them.
 *
 * READ_FILE, READ_DIR and EXECUTE are intentionally absent: leaving them unhandled
 * is what keeps module loading, font data and the ffprobe helper working.
 * IOCTL_DEV (ABI 5) and the ABI 6 scope flags are left for a later iteration, as
 * each needs its own round of validation.
 */
static guint64
asw_landlock_fs_write_mask (guint abi)
{
	guint64 mask = LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_MAKE_REG |
		       LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_SYM |
		       LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO |
		       LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_BLOCK |
		       LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR;

	if (abi >= ASW_LANDLOCK_ABI_REFER)
		mask |= LANDLOCK_ACCESS_FS_REFER;
	if (abi >= ASW_LANDLOCK_ABI_TRUNCATE)
		mask |= LANDLOCK_ACCESS_FS_TRUNCATE;

	return mask;
}

/**
 * asw_sandbox_count_threads:
 *
 * Count the threads of this process, or 0 if we can not tell.
 */
static guint
asw_sandbox_count_threads (void)
{
	g_autoptr(GDir) task_dir = g_dir_open ("/proc/self/task", 0, NULL);
	guint count = 0;

	if (task_dir == NULL)
		return 0;
	while (g_dir_read_name (task_dir) != NULL)
		count++;

	return count;
}
#endif /* HAVE_LANDLOCK */

/**
 * asw_sandbox_probe_abi:
 *
 * Ask the kernel which Landlock ABI it implements.
 *
 * Sandboxes and older kernels may not have Landlock at all, so we try it rather
 * than guessing from the kernel version. The result is cached, and the query does
 * not create a ruleset, so there is nothing to clean up.
 *
 * Returns: the Landlock ABI version, or 0 if Landlock is unavailable.
 */
guint
asw_sandbox_probe_abi (void)
{
#ifdef HAVE_LANDLOCK
	static gsize initialized = 0;
	static guint abi_version = 0;

	if (g_once_init_enter (&initialized)) {
		int ret = asw_landlock_create_ruleset (NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
		if (ret > 0) {
			abi_version = (guint) ret;
		} else {
			/* ENOSYS means the kernel predates Landlock, EOPNOTSUPP means it
			 * was built with Landlock but does not have it in its active LSM list */
			g_debug ("Landlock is unavailable (%s), the media worker will run "
				 "without a sandbox.",
				 g_strerror (errno));
		}
		g_once_init_leave (&initialized, 1);
	}

	return abi_version;
#else
	return 0;
#endif
}

/**
 * asw_sandbox_apply:
 * @info: (out): Receives what was actually achieved.
 *
 * Give up every ability this process does not need any more: writing to the
 * filesystem, and (on new enough kernels) using TCP.
 *
 * This never fails in a way the caller has to handle - a worker that could not
 * sandbox itself still works correctly, it is just not hardened - so the outcome
 * is reported through @info for logging rather than as an error.
 */
void
asw_sandbox_apply (AswSandboxInfo *info)
{
	g_return_if_fail (info != NULL);

	memset (info, 0, sizeof (AswSandboxInfo));
	info->state = ASW_SANDBOX_STATE_UNSUPPORTED;

	/* An escape hatch for debugging and for sites that hit unexpected fallout. */
	if (g_strcmp0 (g_getenv ("ASC_NO_SANDBOX"), "1") == 0) {
		info->state = ASW_SANDBOX_STATE_DISABLED;
		g_debug ("Media worker sandbox disabled via ASC_NO_SANDBOX.");
		return;
	}

#ifdef HAVE_LANDLOCK
	{
		struct landlock_ruleset_attr attr;
		guint n_threads;
		int ruleset_fd;
		int saved_errno;

		/* required to enter a Landlock domain without CAP_SYS_ADMIN, and a good
                 * idea in its own right: it also stops the helpers we spawn from gaining
                 * privileges through setuid binaries or file capabilities */
		if (prctl (PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
			info->state = ASW_SANDBOX_STATE_FAILED;
			g_warning ("Unable to forbid privilege gains for the media worker: %s",
				   g_strerror (errno));
			return;
		}

		info->abi_version = asw_sandbox_probe_abi ();
		if (info->abi_version == 0) {
			/* asw_sandbox_probe_abi() logs a message if it fails */
			return;
		}

		/* A Landlock domain only binds the calling thread and whatever it creates
		 * afterwards, so entering one from a process that is already multi-threaded
		 * would leave the other threads unrestricted.
		 * This is just a safeguard, so we get a warning in case we make that mistake
		 * in future. */
		n_threads = asw_sandbox_count_threads ();
		if (n_threads > 1)
			g_warning ("The media worker was already running %u threads when it tried "
				   "to sandbox itself!",
				   n_threads);

		/* Handle every way of modifying the filesystem, then add no rule at all:
		 * an empty ruleset denies all of it. The same trick denies TCP outright. */
		memset (&attr, 0, sizeof (attr));
		attr.handled_access_fs = asw_landlock_fs_write_mask (info->abi_version);
#ifdef LANDLOCK_ACCESS_NET_BIND_TCP
		if (info->abi_version >= ASW_LANDLOCK_ABI_NET)
			attr.handled_access_net = LANDLOCK_ACCESS_NET_BIND_TCP |
						  LANDLOCK_ACCESS_NET_CONNECT_TCP;
#endif

		ruleset_fd = asw_landlock_create_ruleset (&attr, sizeof (attr), 0);
		if (ruleset_fd < 0) {
			info->state = ASW_SANDBOX_STATE_FAILED;
			g_warning ("Unable to create the media worker sandbox: %s",
				   g_strerror (errno));
			return;
		}

		/* the flags argument must be zero: kernels below ABI 7 reject anything else */
		if (asw_landlock_restrict_self (ruleset_fd, 0) != 0) {
			saved_errno = errno;
			close (ruleset_fd);
			info->state = ASW_SANDBOX_STATE_FAILED;
			g_warning ("Unable to enter the media worker sandbox: %s",
				   g_strerror (saved_errno));
			return;
		}
		close (ruleset_fd);

		info->fs_writes_denied = TRUE;
		info->tcp_denied = attr.handled_access_net != 0;
		info->state = info->tcp_denied ? ASW_SANDBOX_STATE_ENFORCED
					       : ASW_SANDBOX_STATE_PARTIAL;

		{
			g_autofree gchar *desc = asw_sandbox_info_describe (info);
			g_debug ("Media worker sandbox: %s", desc);
		}
	}
#else
	g_debug ("This build has no Landlock support, the media worker is not sandboxed.");
#endif
}

/**
 * asw_sandbox_state_to_token:
 *
 * Convert a sandbox state into the short, stable identifier that we report to
 * the client over IPC.
 */
const gchar *
asw_sandbox_state_to_token (AswSandboxState state)
{
	switch (state) {
	case ASW_SANDBOX_STATE_ENFORCED:
		return "landlock";
	case ASW_SANDBOX_STATE_PARTIAL:
		return "landlock-partial";
	case ASW_SANDBOX_STATE_UNSUPPORTED:
	case ASW_SANDBOX_STATE_DISABLED:
	case ASW_SANDBOX_STATE_FAILED:
	default:
		return "none";
	}
}

/**
 * asw_sandbox_info_describe:
 *
 * Render a sandbox result as a short line for humans to read in a log.
 *
 * Returns: (transfer full): a newly allocated description.
 */
gchar *
asw_sandbox_info_describe (const AswSandboxInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);

	switch (info->state) {
	case ASW_SANDBOX_STATE_DISABLED:
		return g_strdup ("disabled on request");
	case ASW_SANDBOX_STATE_FAILED:
		return g_strdup ("unavailable, sandboxing failed");
	case ASW_SANDBOX_STATE_PARTIAL:
	case ASW_SANDBOX_STATE_ENFORCED:
		/* deliberately not "network denied": Landlock only covers TCP here,
		 * UDP and other socket families are still reachable */
		return g_strdup_printf ("Landlock ABI %u, filesystem writes%s denied",
					info->abi_version,
					info->tcp_denied ? " and TCP" : "");
	case ASW_SANDBOX_STATE_UNSUPPORTED:
	default:
		return g_strdup ("unavailable, not sandboxed");
	}
}
