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
#define ASW_LANDLOCK_ABI_NET_TCP  4
#define ASW_LANDLOCK_ABI_TSYNC	  8
#define ASW_LANDLOCK_ABI_NET_UDP  10

#ifndef LANDLOCK_RESTRICT_SELF_TSYNC
#define LANDLOCK_RESTRICT_SELF_TSYNC (1U << 3)
#endif

/* The presence of LANDLOCK_ACCESS_NET_BIND_TCP tells us whether the ruleset attribute
 * struct has a handled_access_net member at all, so the UDP bits hang off the same
 * check rather than getting one of their own. */
#ifdef LANDLOCK_ACCESS_NET_BIND_TCP
#ifndef LANDLOCK_ACCESS_NET_BIND_UDP
#define LANDLOCK_ACCESS_NET_BIND_UDP (1ULL << 2)
#endif
#ifndef LANDLOCK_ACCESS_NET_CONNECT_SEND_UDP
#define LANDLOCK_ACCESS_NET_CONNECT_SEND_UDP (1ULL << 3)
#endif
#endif

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
 * Enter the domain described by @ruleset_fd. Irreversible. Without
 * %LANDLOCK_RESTRICT_SELF_TSYNC this only affects the calling thread and its future
 * children, with it every thread of the process.
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
 * IOCTL_DEV (ABI 5), the ABI 6 scope flags and RESOLVE_UNIX (ABI 9) are left for a
 * later iteration, as each needs its own round of validation.
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

#ifdef LANDLOCK_ACCESS_NET_BIND_TCP
/**
 * asw_landlock_net_mask:
 *
 * Every network access right that the running kernel knows about.
 *
 * No worker code opens a socket at all, but libcurl, GnuTLS and OpenSSL are in our
 * address space through libappstream, so an exploit in a media parser could. UDP only
 * became restrictable with ABI 10; below that we can deny TCP alone.
 *
 * Landlock covers IP sockets only - UNIX, netlink and raw sockets are untouched by
 * this, which is why we never describe the result as "network denied".
 */
static guint64
asw_landlock_net_mask (guint abi)
{
	guint64 mask = LANDLOCK_ACCESS_NET_BIND_TCP | LANDLOCK_ACCESS_NET_CONNECT_TCP;

	if (abi >= ASW_LANDLOCK_ABI_NET_UDP)
		mask |= LANDLOCK_ACCESS_NET_BIND_UDP | LANDLOCK_ACCESS_NET_CONNECT_SEND_UDP;

	return mask;
}
#endif

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
 * filesystem, and (on new enough kernels) using TCP and UDP.
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
		int ruleset_fd;
		int saved_errno;
		gboolean res;

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

		/* Handle every way of modifying the filesystem, then add no rule at all:
		 * an empty ruleset denies all of it. The same trick denies IP networking. */
		memset (&attr, 0, sizeof (attr));
		attr.handled_access_fs = asw_landlock_fs_write_mask (info->abi_version);
#ifdef LANDLOCK_ACCESS_NET_BIND_TCP
		if (info->abi_version >= ASW_LANDLOCK_ABI_NET_TCP)
			attr.handled_access_net = asw_landlock_net_mask (info->abi_version);
#endif

		ruleset_fd = asw_landlock_create_ruleset (&attr, sizeof (attr), 0);
		if (ruleset_fd < 0) {
			info->state = ASW_SANDBOX_STATE_FAILED;
			g_warning ("Unable to create the media worker sandbox: %s",
				   g_strerror (errno));
			return;
		}

		/* From ABI 8 on, TSYNC applies the domain to every thread of the process
		 * at once and propagates no_new_privs to them, instead of only binding the
		 * calling thread and whatever it creates afterwards. We are single-threaded
		 * here anyway, but this removes a whole class of future mistakes. */
		res = info->abi_version >= ASW_LANDLOCK_ABI_TSYNC &&
		      asw_landlock_restrict_self (ruleset_fd, LANDLOCK_RESTRICT_SELF_TSYNC) == 0;
		if (!res && asw_landlock_restrict_self (ruleset_fd, 0) != 0) {
			/* the flags argument must be zero here: kernels below ABI 7 reject
			 * anything else */
			saved_errno = errno;
			close (ruleset_fd);
			info->state = ASW_SANDBOX_STATE_FAILED;
			g_warning ("Unable to enter the media worker sandbox: %s",
				   g_strerror (saved_errno));
			return;
		}
		close (ruleset_fd);

		info->fs_writes_denied = TRUE;
#ifdef LANDLOCK_ACCESS_NET_BIND_TCP
		info->tcp_denied = attr.handled_access_net != 0;
		info->udp_denied = (attr.handled_access_net & LANDLOCK_ACCESS_NET_BIND_UDP) != 0;
#endif
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
	case ASW_SANDBOX_STATE_ENFORCED: {
		/* deliberately never just "network denied": Landlock only covers IP
		 * sockets, UNIX, netlink and raw sockets are still reachable */
		const gchar *denied = "filesystem writes";

		if (info->udp_denied)
			denied = "filesystem writes, TCP and UDP";
		else if (info->tcp_denied)
			denied = "filesystem writes and TCP";

		return g_strdup_printf ("Landlock ABI %u, %s denied", info->abi_version, denied);
	}
	case ASW_SANDBOX_STATE_UNSUPPORTED:
	default:
		return g_strdup ("unavailable, not sandboxed");
	}
}
