/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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
 * asc-mediaworker:
 *
 * Media processing worker for libappstream-compose.
 * This process performs all parsing and rendering of untrusted media data
 * (images, fonts, videos) on behalf of the AppStream compose library, so
 * that this work is isolated from the main process and can be sandboxed.
 *
 * The worker is spawned by libappstream-compose with its communication
 * socket on a well-known file descriptor - it is not useful to run manually.
 */

#include "config.h"

#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#ifdef __FreeBSD__
#include <sys/procctl.h>
#else
#include <sys/prctl.h>
#endif

#include "asc-media-ipc.h"
#include "asw-worker.h"

int
main (int argc, char **argv)
{
	g_autoptr(AswWorker) worker = NULL;
	g_autoptr(GError) error = NULL;

	if (argc == 2 &&
	    (g_strcmp0 (argv[1], "--version") == 0 || g_strcmp0 (argv[1], "version") == 0)) {
		printf ("AppStream compose media worker, version: %s\n", PACKAGE_VERSION);
		return 0;
	}

	/* we only ever write to a socket held by our parent process */
	signal (SIGPIPE, SIG_IGN);

	/* terminate if the process that spawned us is gone */
#ifdef __FreeBSD__
	{
		int sig = SIGTERM;
		procctl (P_PID, 0, PROC_PDEATHSIG_CTL, &sig);
	}
#else
	prctl (PR_SET_PDEATHSIG, SIGTERM);
#endif

	worker = asw_worker_new_for_fd (ASC_MEDIA_SOCKET_FD, &error);
	if (worker == NULL) {
		fprintf (stderr,
			 "asc-mediaworker: Unable to create worker (this program is spawned by "
			 "libappstream-compose and is not useful on its own): %s\n",
			 error->message);
		return 1;
	}

	if (!asw_worker_send_hello (worker, &error)) {
		fprintf (stderr,
			 "asc-mediaworker: Unable to greet our parent process: %s\n",
			 error->message);
		return 1;
	}

	return asw_worker_run (worker);
}
