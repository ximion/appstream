/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <locale.h>

#include "ascli-utils.h"
#include "ascli-actions-mdata.h"
#include "ascli-actions-validate.h"
#include "ascli-actions-pkgmgr.h"
#include "ascli-actions-misc.h"

static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;
static gboolean optn_no_color = FALSE;
static gboolean optn_force = FALSE;
static gboolean optn_details = FALSE;
static gboolean optn_pedantic = FALSE;
static gboolean optn_no_cache = FALSE;
static gchar *optn_cachepath = NULL;
static gchar *optn_datapath = NULL;
static gchar *optn_format = NULL;

/**
 * as_client_get_summary:
 **/
static gchar *
as_client_get_summary ()
{
	GString *string;
	string = g_string_new ("");

	/* TRANSLATORS: This is the header to the --help menu */
	g_string_append_printf (string, "%s\n\n%s\n", _("AppStream command-line interface"),
				/* these are commands we can use with appstreamcli */
				_("Subcommands:"));

	g_string_append_printf (string, "  %s - %s\n", "search TERM     ", _("Search the component database."));
	g_string_append_printf (string, "  %s - %s\n", "get COMPONENT-ID", _("Get information about a component by its ID."));
	g_string_append_printf (string, "  %s - %s\n", "what-provides TYPE VALUE", _("Get components which provide the given item."));
	g_string_append_printf (string, "    %s - %s\n", "TYPE ", _("An item type (e.g. lib, bin, python3, ...)"));
	g_string_append_printf (string, "    %s - %s\n", "VALUE", _("Value of the item that should be found."));
	g_string_append (string, "\n");
	g_string_append_printf (string, "  %s - %s\n", "dump COMPONENT-ID", _("Dump raw XML metadata for a component matching the ID."));
	g_string_append_printf (string, "  %s - %s\n", "refresh-index    ", _("Rebuild the component metadata cache."));
	g_string_append (string, "\n");
	g_string_append_printf (string, "  %s - %s\n", "validate FILE          ", _("Validate AppStream XML files for issues."));
	g_string_append_printf (string, "  %s - %s\n", "validate-tree DIRECTORY", _("Validate an installed file-tree of an application for valid metadata."));
	g_string_append (string, "\n");
	g_string_append_printf (string, "  %s - %s\n", "install COMPONENT-ID", _("Install software matching the component-id."));
	g_string_append_printf (string, "  %s - %s\n", "remove  COMPONENT-ID", _("Remove software matching the component-id."));
	g_string_append (string, "\n");
	g_string_append_printf (string, "  %s - %s\n", "status           ", _("Display status information about available AppStream metadata."));
	g_string_append_printf (string, "  %s - %s\n", "put FILE         ", _("Install a metadata file into the right location."));
	/* TRANSLATORS: "convert" command in ascli. "Collection XML" is a term describing a specific type of AppStream XML data. */
	g_string_append_printf (string, "  %s - %s\n", "convert FILE FILE", _("Convert collection XML to YAML or vice versa."));

	return g_string_free (string, FALSE);
}

/**
 * as_client_run:
 */
static int
as_client_run (char **argv, int argc)
{
	GOptionContext *opt_context;
	GError *error = NULL;

	gint exit_code = 0;
	gchar *command = NULL;
	gchar *value1 = NULL;
	gchar *value2 = NULL;

	gchar *summary;
	gchar *options_help = NULL;
	AsFormatKind mformat;

	const GOptionEntry client_options[] = {
		{ "version", 0, 0,
			G_OPTION_ARG_NONE,
			&optn_show_version,
			/* TRANSLATORS: ascli flag description for: --version */
			_("Show the program version."),
			NULL },
		{ "verbose", (gchar) 0, 0,
			G_OPTION_ARG_NONE,
			&optn_verbose_mode,
			/* TRANSLATORS: ascli flag description for: --verbose */
			_("Show extra debugging information."),
			NULL },
		{ "no-color", (gchar) 0, 0,
			G_OPTION_ARG_NONE, &optn_no_color,
			/* TRANSLATORS: ascli flag description for: --no-color */
			_("Don't show colored output."), NULL },
		{ "force", (gchar) 0, 0,
			G_OPTION_ARG_NONE,
			&optn_force,
			/* TRANSLATORS: ascli flag description for: --force */
			_("Enforce a cache refresh."),
			NULL },
		{ "details", 0, 0,
			G_OPTION_ARG_NONE,
			&optn_details,
			/* TRANSLATORS: ascli flag description for: --details */
			_("Print detailed output about found components."),
			NULL },
		{ "no-cache", 0, 0,
			G_OPTION_ARG_NONE,
			&optn_no_cache,
			/* TRANSLATORS: ascli flag description for: --no-cache */
			_("Do not use the Xapian cache when performing the request."),
			NULL },
		{ "cachepath", 0, 0,
			G_OPTION_ARG_STRING,
			&optn_cachepath,
			/* TRANSLATORS: ascli flag description for: --cachepath */
			_("Manually set the location of the AppStream cache."), NULL },
		{ "datapath", 0, 0,
			G_OPTION_ARG_STRING,
			&optn_datapath,
			/* TRANSLATORS: ascli flag description for: --datapath */
			_("Manually set the location of AppStream metadata to scan."), NULL },
		{ "format", 0, 0,
			G_OPTION_ARG_STRING,
			&optn_format,
			/* TRANSLATORS: ascli flag description for: --format */
			_("Default to the given metadata format (valid values are 'xml' and 'yaml')."), NULL },
		{ "pedantic", (gchar) 0, 0,
			G_OPTION_ARG_NONE,
			&optn_pedantic,
			/* TRANSLATORS: ascli flag description for: --pedantic */
			_("Also print pedantic hints when validating."), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- AppStream CLI.");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, client_options, NULL);

	/* set the summary text */
	summary = as_client_get_summary ();
	g_option_context_set_summary (opt_context, summary) ;
	options_help = g_option_context_get_help (opt_context, TRUE, NULL);
	g_free (summary);

	g_option_context_parse (opt_context, &argc, &argv, &error);
	if (error != NULL) {
		gchar *msg;
		msg = g_strconcat (error->message, "\n", NULL);
		g_print ("%s", msg);
		g_free (msg);
		ascli_print_stderr (_("Run '%s --help' to see a full list of available command line options."), argv[0]);
		exit_code = 1;
		g_error_free (error);
		goto out;
	}

	if (optn_show_version) {
		/* TRANSLATORS: Output if appstreamcli --version is executed. */
		ascli_print_stdout (_("AppStream CLI tool version: %s"), VERSION);
		goto out;
	}

	/* just a hack, we might need proper message handling later */
	if (optn_verbose_mode) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}

	if (argc < 2) {
		/* TRANSLATORS: ascli has been run without command. */
		g_printerr ("%s\n", _("You need to specify a command."));
		ascli_print_stderr (_("Run '%s --help' to see a full list of available command line options."), argv[0]);
		exit_code = 1;
		goto out;
	}

	ascli_set_colored_output (!optn_no_color);

	mformat = as_format_kind_from_string (optn_format);
	command = argv[1];
	if (argc > 2)
		value1 = argv[2];
	if (argc > 3)
		value2 = argv[3];

	if ((g_strcmp0 (command, "search") == 0) || (g_strcmp0 (command, "s") == 0)) {
		exit_code = ascli_search_component (optn_cachepath,
							value1,
							optn_details,
							optn_no_cache);
	} else if ((g_strcmp0 (command, "refresh-cache") == 0) || (g_strcmp0 (command, "refresh") == 0)) {
		exit_code = ascli_refresh_cache (optn_cachepath,
						 optn_datapath,
						 optn_force);
	} else if (g_strcmp0 (command, "get") == 0) {
		exit_code = ascli_get_component (optn_cachepath,
						 value1,
						 optn_details,
						 optn_no_cache);
	} else if (g_strcmp0 (command, "dump") == 0) {
		exit_code = ascli_dump_component (optn_cachepath,
						  value1,
						  mformat,
						  optn_no_cache);
	} else if (g_strcmp0 (command, "what-provides") == 0) {
		exit_code = ascli_what_provides (optn_cachepath, value1, value2, optn_details);
	} else if (g_strcmp0 (command, "validate") == 0) {
		exit_code = ascli_validate_files (&argv[2], argc-2, optn_no_color, optn_pedantic);
	} else if (g_strcmp0 (command, "validate-tree") == 0) {
		exit_code = ascli_validate_tree (value1, optn_no_color, optn_pedantic);
	} else if (g_strcmp0 (command, "install") == 0) {
		exit_code = ascli_install_component (value1);
	} else if (g_strcmp0 (command, "remove") == 0) {
		exit_code = ascli_remove_component (value1);
	} else if (g_strcmp0 (command, "put") == 0) {
		exit_code = ascli_put_metainfo (value1);
	} else if (g_strcmp0 (command, "status") == 0) {
		exit_code = ascli_show_status ();
	} else if (g_strcmp0 (command, "convert") == 0) {
		exit_code = ascli_convert_data (value1, value2, mformat);
	} else {
		/* TRANSLATORS: ascli has been run with unknown command. */
		ascli_print_stderr (_("Command '%s' is unknown."), command);
		exit_code = 1;
		goto out;
	}

out:
	g_option_context_free (opt_context);
	if (options_help != NULL)
		g_free (options_help);

	return exit_code;
}

int
main (int argc, char ** argv)
{
	gint code = 0;

	/* bind locale */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* run the application */
	code = as_client_run (argv, argc);

	return code;
}
