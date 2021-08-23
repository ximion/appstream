/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2019-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "config.h"

#include <appstream-compose.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "ascli-utils.h"

int
main (int argc, char **argv)
{
	g_autoptr(GOptionContext) option_context = NULL;
	gboolean ret;
	gboolean verbose = FALSE;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *origin = NULL;
	g_autofree gchar *res_root_dir = NULL;
	g_autofree gchar *mdata_dir = NULL;
	g_autofree gchar *icons_dir = NULL;
	g_autofree gchar *prefix = NULL;
	g_autofree gchar *components_str = NULL;
	GPtrArray *results;
	g_autoptr(AscCompose) compose = NULL;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "prefix", '\0', 0, G_OPTION_ARG_FILENAME, &prefix,
			/* TRANSLATORS: command line option */
			_("Override the default prefix"), "DIR" },
		{ "result-root", 'o', 0, G_OPTION_ARG_FILENAME, &res_root_dir,
			/* TRANSLATORS: command line option */
			_("Set the result output directory"), "DIR" },
		{ "data-dir", 'o', 0, G_OPTION_ARG_FILENAME, &mdata_dir,
			/* TRANSLATORS: command line option, `collection metadata` is an AppStream term */
			_("Override the collection metadata output directory"), "DIR" },
		{ "icons-dir", 'o', 0, G_OPTION_ARG_FILENAME, &icons_dir,
			/* TRANSLATORS: command line option */
			_("Override the icon output directory"), "DIR" },
		{ "origin", '\0', 0, G_OPTION_ARG_STRING, &origin,
			/* TRANSLATORS: command line option */
			_("Set the origin name"), "NAME" },
		{ "components", '\0', 0, G_OPTION_ARG_STRING, &components_str,
			/* TRANSLATORS: command line option */
			_("A comma-separated list of component-IDs to accept"), "COMPONENT-IDs" },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	option_context = g_option_context_new (" - SOURCE-DIRECTORIES");

	g_option_context_add_main_entries (option_context, options, NULL);
	ret = g_option_context_parse (option_context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_print ("%s: %s\n", _("Failed to parse arguments"), error->message);
		return EXIT_FAILURE;
	}

	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	/* create compose engine */
	compose = asc_compose_new ();

	/* sanity checks & defaults */
	if (prefix == NULL)
		prefix = g_strdup ("/usr");
	asc_compose_set_prefix (compose, prefix);

	if (res_root_dir == NULL && (mdata_dir == NULL || icons_dir == NULL)) {
		if (argc == 2) {
			/* we have only one unit as parameter, assume it as target path for convenience & compatibility */
			res_root_dir = g_strdup (argv[1]);
			ascli_print_stdout (_("Automatically selected '%s' as data output location."), res_root_dir);
		} else {
			/* TRANSLATORS: we could not auto-add the provides */
			g_printerr ("%s\n", _("No destination directory set, please provide a data output location!"));
			return EXIT_FAILURE;
		}
	}
	if (origin == NULL) {
		origin = g_strdup ("example");
		/* TRANSLATORS: information message of appstream-compose */
		ascli_print_stderr (_("WARNING: Metadata origin not set, using '%s'"), origin);
	}
	asc_compose_set_origin (compose, origin);

	if (mdata_dir == NULL)
		mdata_dir = g_build_filename (res_root_dir, prefix, "share/app-info/xmls", NULL);
	asc_compose_set_data_result_dir (compose, mdata_dir);
	if (icons_dir == NULL)
		icons_dir = g_build_filename (res_root_dir, prefix, "share/app-info/icons", origin, NULL);
	asc_compose_set_icons_result_dir (compose, icons_dir);

	if (argc == 1) {
		g_autofree gchar *tmp = NULL;
		tmp = g_option_context_get_help (option_context, TRUE, NULL);
		g_print ("%s", tmp);
		return  EXIT_FAILURE;
	}

	/* add locations for data processing */
	for (guint i = 1; i < (guint) argc; i++) {
		g_autoptr(AscDirectoryUnit) dirunit = NULL;
		const gchar *dir_path = argv[i];

		if (!g_file_test (dir_path, G_FILE_TEST_IS_DIR)) {
			/* TRANSLATORS: error message */
			g_print ("%s: %s\n", _("Can not process invalid directory"), dir_path);
			return EXIT_FAILURE;
		}
		dirunit = asc_directory_unit_new (dir_path);
		asc_compose_add_unit (compose, ASC_UNIT (dirunit));
	}

	/* TRANSLATORS: information message */
	g_print ("%s\n", _("Composing metadata..."));

	results = asc_compose_run (compose, NULL, &error);
	if (results == NULL) {
		/* TRANSLATORS: error message */
		g_print ("%s: %s\n", _("Failed to compose AppStream metadata"), error->message);
		return EXIT_FAILURE;
	}

	/* TRANSLATORS: information message */
	g_print ("%s\n", _("Done!"));

	return EXIT_SUCCESS;
}
