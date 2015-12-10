/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ascli-install-actions.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <unistd.h>
#include <errno.h>

#include "ascli-utils.h"

/**
 * exec_pm_install:
 *
 * Run the native package manager to install an application.
 */
static int
exec_pm_install (gchar **pkgnames)
{
	int ret;
	const gchar *exe = NULL;
	guint i;
	g_auto(GStrv) cmd = NULL;

#ifdef APT_SUPPORT
	if (g_file_test ("/usr/bin/apt", G_FILE_TEST_EXISTS))
		exe = "/usr/bin/apt";
#endif
	if (exe == NULL) {
		if (g_file_test ("/usr/bin/pkcon", G_FILE_TEST_EXISTS)) {
			exe = "/usr/bin/pkcon";
		} else {
			g_printerr ("%s\n", _("No suitable package manager CLI found. Please make sure that e.g. \"pkcon\" (part of PackageKit) is available."));
			return 1;
		}
	}

	cmd = g_new0 (gchar*, 3 + g_strv_length (pkgnames) + 1);
	cmd[0] = g_strdup (exe);
	cmd[1] = g_strdup ("install");
	for (i = 0; pkgnames[i] != NULL; i++) {
		cmd[2+i] = g_strdup (pkgnames[i]);
	}

	ret = execv (exe, cmd);
	if (ret != 0)
		ascli_print_stderr (_("Unable to spawn package manager: %s"), g_strerror (errno));
	return ret;
}

/**
 * ascli_install_component:
 *
 * Install a component matching the given ID.
 */
int
ascli_install_component (const gchar *identifier)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(AsDatabase) db = NULL;
	AsComponent *cpt = NULL;
	gchar **pkgnames;
	gint exit_code = 0;

	if (identifier == NULL) {
		ascli_print_stderr (_("You need to specify a component-id."));
		return 2;
	}

	db = as_database_new ();

	as_database_open (db, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		exit_code = 1;
		goto out;
	}

	cpt = as_database_get_component_by_id (db, identifier, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		exit_code = 1;
		goto out;
	}

	if (cpt == NULL) {
		ascli_print_stderr (_("Unable to find component with id '%s'!"), identifier);
		exit_code = 4;
		goto out;
	}

	pkgnames = as_component_get_pkgnames (cpt);
	if (pkgnames == NULL) {
		ascli_print_stderr (_("Component '%s' has no installation candidate."), identifier);
		exit_code = 1;
		goto out;
	}

	exit_code = exec_pm_install (pkgnames);
out:
	if (cpt != NULL)
		g_object_unref (cpt);

	return exit_code;
}
