/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 Eugenio De Palo
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

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <locale.h>

#include "tools/ascli-utils.h"

static gchar *ascli_path = NULL;

static gint
run_validate_missing (const gchar *format)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GSubprocess) subprocess = NULL;
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *missing_path = NULL;
	const gchar *argv[] = {
		ascli_path, "validate", "--no-net", format, NULL, NULL,
	};
	gint exit_status;

	tmpdir = g_dir_make_tmp ("appstream-test-cli-XXXXXX", &error);
	g_assert_no_error (error);
	missing_path = g_build_filename (tmpdir, "missing.metainfo.xml", NULL);
	argv[4] = missing_path;

	subprocess = g_subprocess_newv (argv,
					G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
					    G_SUBPROCESS_FLAGS_STDERR_SILENCE,
					&error);
	g_assert_no_error (error);
	g_assert_true (g_subprocess_wait (subprocess, NULL, &error));
	g_assert_no_error (error);
	g_assert_true (g_subprocess_get_if_exited (subprocess));
	exit_status = g_subprocess_get_exit_status (subprocess);
	g_assert_cmpint (g_rmdir (tmpdir), ==, 0);

	return exit_status;
}

static void
test_validate_missing (void)
{
	g_assert_cmpint (run_validate_missing ("--format=text"), ==, ASCLI_EXIT_CODE_BAD_INPUT);
	g_assert_cmpint (run_validate_missing ("--format=yaml"), ==, ASCLI_EXIT_CODE_BAD_INPUT);
}

int
main (int argc, char **argv)
{
	int ret;

	setlocale (LC_ALL, "");
	g_assert_cmpint (argc, >=, 2);
	ascli_path = g_strdup (argv[1]);
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/AppStream/CLI/ValidateMissing", test_validate_missing);

	ret = g_test_run ();
	g_free (ascli_path);
	return ret;
}
