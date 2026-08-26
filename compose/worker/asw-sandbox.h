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

#pragma once

#include <glib.h>

#include "as-macros-private.h"

G_BEGIN_DECLS

/**
 * AswSandboxState:
 * @ASW_SANDBOX_STATE_UNSUPPORTED:	No Landlock support was built in, or the running
 *					kernel does not provide it.
 * @ASW_SANDBOX_STATE_DISABLED:		Sandboxing was switched off via the environment.
 * @ASW_SANDBOX_STATE_FAILED:		Landlock is available, but we could not enter a domain.
 * @ASW_SANDBOX_STATE_PARTIAL:		A domain was entered, but the kernel is too old to
 *					also deny network access.
 * @ASW_SANDBOX_STATE_ENFORCED:		Filesystem writes and TCP are denied.
 *
 * How much of its intended self-restriction the worker actually achieved.
 *
 * Newer kernels can additionally deny UDP and cover every thread of the process at
 * once. Those are refinements of an already complete policy rather than a part of it,
 * so they do not decide between %ASW_SANDBOX_STATE_PARTIAL and
 * %ASW_SANDBOX_STATE_ENFORCED - check #AswSandboxInfo for what was actually achieved.
 */
typedef enum {
	ASW_SANDBOX_STATE_UNSUPPORTED,
	ASW_SANDBOX_STATE_DISABLED,
	ASW_SANDBOX_STATE_FAILED,
	ASW_SANDBOX_STATE_PARTIAL,
	ASW_SANDBOX_STATE_ENFORCED
} AswSandboxState;

/**
 * AswSandboxInfo:
 * @state: What was achieved, as an #AswSandboxState.
 * @abi_version: Landlock ABI version of the running kernel, 0 if unavailable.
 * @fs_writes_denied: %TRUE if the worker can no longer write to the filesystem.
 * @tcp_denied: %TRUE if the worker can no longer bind or connect TCP sockets.
 * @udp_denied: %TRUE if the worker can no longer bind UDP sockets or send datagrams.
 * @threads_synced: %TRUE if the restrictions apply to every thread of the process,
 *		    rather than only to the calling thread and its future children.
 *
 * The result of an attempt to sandbox the worker process.
 */
typedef struct {
	AswSandboxState state;
	guint		abi_version;
	gboolean	fs_writes_denied;
	gboolean	tcp_denied;
	gboolean	udp_denied;
} AswSandboxInfo;

void	     asw_sandbox_apply (AswSandboxInfo *info);
guint	     asw_sandbox_probe_abi (void);

const gchar *asw_sandbox_state_to_token (AswSandboxState state);
gchar	    *asw_sandbox_info_describe (const AswSandboxInfo *info);

G_END_DECLS
