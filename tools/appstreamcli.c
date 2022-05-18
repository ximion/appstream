/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <locale.h>
#include <stdio.h>

#include "as-profile.h"
#include "as-utils-private.h"

#include "ascli-utils.h"
#include "ascli-actions-mdata.h"
#include "ascli-actions-validate.h"
#include "ascli-actions-pkgmgr.h"
#include "ascli-actions-misc.h"

#define ASCLI_BIN_NAME "appstreamcli"

/* global options which affect all commands */
static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;
static gboolean optn_no_color = FALSE;

/*** COMMAND OPTIONS ***/

/* for data_collection_options */
static gchar *optn_cachepath = NULL;
static gchar *optn_datapath = NULL;
static gboolean optn_no_cache = FALSE;

/**
 * General options used for any operations on
 * metadata collections and the cache.
 */
const GOptionEntry data_collection_options[] = {
	{ "cachepath", 0, 0,
		G_OPTION_ARG_STRING,
		&optn_cachepath,
		/* TRANSLATORS: ascli flag description for: --cachepath */
		N_("Manually selected location of AppStream cache."), NULL },
	{ "datapath", 0, 0,
		G_OPTION_ARG_STRING,
		&optn_datapath,
		/* TRANSLATORS: ascli flag description for: --datapath */
		N_("Manually selected location of AppStream metadata to scan."), NULL },
	{ "no-cache", 0, 0,
		G_OPTION_ARG_NONE,
		&optn_no_cache,
		/* TRANSLATORS: ascli flag description for: --no-cache */
		N_("Ignore cache age and build a fresh cache before performing the query."),
		NULL },
	{ NULL }
};

/* used by format_options */
static gchar *optn_format = NULL;

/**
 * The format option.
 */
const GOptionEntry format_options[] = {
	{ "format", 0, 0,
		G_OPTION_ARG_STRING,
		&optn_format,
		/* TRANSLATORS: ascli flag description for: --format */
		N_("Default metadata format (valid values are 'xml' and 'yaml')."), NULL },
	{ NULL }
};

/* used by find_options */
static gboolean optn_details = FALSE;

/**
 * General options for finding & displaying data.
 */
const GOptionEntry find_options[] = {
	{ "details", 0, 0,
		G_OPTION_ARG_NONE,
		&optn_details,
		/* TRANSLATORS: ascli flag description for: --details */
		N_("Print detailed output about found components."),
		NULL },
	{ NULL }
};

/* used by validate_options */
static gboolean optn_pedantic = FALSE;
static gboolean optn_explain = FALSE;
static gboolean optn_no_net = FALSE;
static gboolean optn_validate_strict = FALSE;
static gchar *optn_issue_overrides = NULL;

/**
 * General options for validation.
 */
const GOptionEntry validate_options[] = {
	{ "pedantic", (gchar) 0, 0,
		G_OPTION_ARG_NONE,
		&optn_pedantic,
		/* TRANSLATORS: ascli flag description for: --pedantic (used by the "validate" command) */
		N_("Also show pedantic hints."), NULL },
	{ "explain", (gchar) 0, 0,
		G_OPTION_ARG_NONE,
		&optn_explain,
		/* TRANSLATORS: ascli flag description for: --explain (used by the "validate" command) */
		N_("Print detailed explanation for found issues."), NULL },
	{ "no-net", (gchar) 0, 0,
		G_OPTION_ARG_NONE,
		&optn_no_net,
		/* TRANSLATORS: ascli flag description for: --no-net (used by the "validate" command) */
		N_("Do not use network access."), NULL },
	{ "strict", (gchar) 0, 0,
		G_OPTION_ARG_NONE,
		&optn_validate_strict,
		/* TRANSLATORS: ascli flag description for: --strict (used by the "validate" command) */
		N_("Fail validation if any issue is emitted that is not of pedantic severity."), NULL },
	{ "format", 0, 0,
		G_OPTION_ARG_STRING,
		&optn_format,
		/* TRANSLATORS: ascli flag description for: --format  when validating XML files */
		N_("Format of the generated report (valid values are 'text' and 'yaml')."), NULL },
	{ "override", 0, 0,
		G_OPTION_ARG_STRING,
		&optn_issue_overrides,
		/* TRANSLATORS: ascli flag description for: --override  when validating XML files */
		N_("Override the severities of selected issue tags."), NULL },

	/* DEPRECATED */
	{ "nonet", (gchar) 0, G_OPTION_FLAG_HIDDEN,
		G_OPTION_ARG_NONE,
		&optn_no_net,
		NULL, NULL },
	{ NULL }
};

/*** HELPER METHODS ***/

/**
 * as_client_get_summary_for:
 **/
static gchar*
as_client_get_summary_for (const gchar *command)
{
	GString *string;
	string = g_string_new ("");

	/* TRANSLATORS: This is the header to the --help menu for subcommands */
	g_string_append_printf (string, "%s\n", _("AppStream command-line interface"));

	g_string_append (string, " ");
	g_string_append_printf (string, _("'%s' command"), command);

	return g_string_free (string, FALSE);
}

/**
 * as_client_new_subcommand_option_context:
 *
 * Create a new option context for an ascli subcommand.
 */
static GOptionContext*
as_client_new_subcommand_option_context (const gchar *command, const GOptionEntry *entries)
{
	GOptionContext *opt_context = NULL;
	g_autofree gchar *summary = NULL;

	opt_context = g_option_context_new ("- AppStream CLI.");
	g_option_context_set_help_enabled (opt_context, TRUE);
	if (entries != NULL)
		g_option_context_add_main_entries (opt_context, entries, NULL);

	/* set the summary text */
	summary = as_client_get_summary_for (command);
	g_option_context_set_summary (opt_context, summary);

	return opt_context;
}

/**
 * as_client_print_help_hint:
 */
static void
as_client_print_help_hint (const gchar *subcommand, const gchar *unknown_option)
{
	if (unknown_option != NULL) {
		/* TRANSLATORS: An unknown option was passed to appstreamcli. */
		ascli_print_stderr (_("Option '%s' is unknown."), unknown_option);
	}

	if (subcommand == NULL)
		ascli_print_stderr (_("Run '%s --help' to see a full list of available command line options."), ASCLI_BIN_NAME);
	else
		ascli_print_stderr (_("Run '%s --help' to see a list of available commands and options, and '%s %s --help' to see a list of options specific for this subcommand."),
				    ASCLI_BIN_NAME, ASCLI_BIN_NAME, subcommand);
}

/**
 * as_client_option_context_parse:
 *
 * Parse the options, print errors.
 */
static int
as_client_option_context_parse (GOptionContext *opt_context, const gchar *subcommand, int *argc, char ***argv)
{
	g_autoptr(GError) error = NULL;

	g_option_context_parse (opt_context, argc, argv, &error);
	if (error != NULL) {
		gchar *msg;
		msg = g_strconcat (error->message, "\n", NULL);
		g_print ("%s", msg);
		g_free (msg);

		as_client_print_help_hint (subcommand, NULL);
		return 1;
	}

	return 0;
}

/*** SUBCOMMANDS ***/

/**
 * as_client_run_refresh_cache:
 *
 * Refresh the AppStream caches.
 */
static int
as_client_run_refresh_cache (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	gboolean optn_force = FALSE;
	g_auto(GStrv) optn_sources = NULL;
	g_auto(GStrv) optn_sources_real = NULL;

	const GOptionEntry refresh_options[] = {
		{ "force", (gchar) 0, 0,
			G_OPTION_ARG_NONE,
			&optn_force,
			/* TRANSLATORS: ascli flag description for: --force */
			_("Enforce a cache refresh."),
			NULL },
		{ "source", (gchar) 0, 0,
			G_OPTION_ARG_STRING_ARRAY,
			&optn_sources,
			/* TRANSLATORS: ascli flag description for: --source in a refresh action. Don't translate strings in backticks: `name` */
			_("Limit cache refresh to data from a specific source, e.g. `os` or `flatpak`. May be specified multiple times."),
			NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (command, refresh_options);
	g_option_context_add_main_entries (opt_context, data_collection_options, NULL);

	ret = as_client_option_context_parse (opt_context,
					      command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (optn_sources != NULL) {
		if (g_strv_length (optn_sources) == 1)
			optn_sources_real = g_strsplit (optn_sources[0], ",", -1);
		else
			optn_sources_real = g_steal_pointer (&optn_sources);
	}

	return ascli_refresh_cache (optn_cachepath,
				    optn_datapath,
				    (const gchar * const*) optn_sources_real,
				    optn_force);
}

/**
 * as_client_run_search:
 *
 * Search for AppStream metadata.
 */
static int
as_client_run_search (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GString) search = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (command, find_options);
	g_option_context_add_main_entries (opt_context, data_collection_options, NULL);

	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	search = g_string_new ("");
	if (argc > 2) {
		for (gint i = 2; i < argc; i++) {
			g_string_append (search, argv[i]);
			g_string_append_c (search, ' ');
		}
		/* drop trailing space */
		if (search->len > 0)
			g_string_truncate (search, search->len - 1);
	}

	return ascli_search_component (optn_cachepath,
					(search->len == 0)? NULL : search->str,
					optn_details,
					optn_no_cache);
}

/**
 * as_client_run_get:
 *
 * Get components by its ID.
 */
static int
as_client_run_get (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *value = NULL;

	opt_context = as_client_new_subcommand_option_context (command, find_options);
	g_option_context_add_main_entries (opt_context, data_collection_options, NULL);

	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		value = argv[2];

	return ascli_get_component (optn_cachepath,
					value,
					optn_details,
					optn_no_cache);
}

/**
 * as_client_run_dump:
 *
 * Dump the raw component metadata to the console.
 */
static int
as_client_run_dump (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *value = NULL;
	AsFormatKind mformat;

	opt_context = as_client_new_subcommand_option_context (command, data_collection_options);
	g_option_context_add_main_entries (opt_context, format_options, NULL);

	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		value = argv[2];

	mformat = as_format_kind_from_string (optn_format);
	return ascli_dump_component (optn_cachepath,
					value,
					mformat,
					optn_no_cache);
}

/**
 * as_client_run_what_provides:
 *
 * Find components that provide a certain item.
 */
static int
as_client_run_what_provides (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *vtype = NULL;
	const gchar *vvalue = NULL;

	opt_context = as_client_new_subcommand_option_context (command, find_options);
	g_option_context_add_main_entries (opt_context, data_collection_options, NULL);

	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		vtype = argv[2];
	if (argc > 3)
		vvalue = argv[3];

	return ascli_what_provides (optn_cachepath,
				    vtype,
				    vvalue,
				    optn_details);
}

/**
 * as_client_run_validate:
 *
 * Validate single metadata files.
 */
static int
as_client_run_validate (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (command, validate_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (optn_format == NULL) {
		return ascli_validate_files (&argv[2],
					     argc-2,
					     optn_pedantic,
					     optn_explain,
					     optn_validate_strict,
					     !optn_no_net,
					     optn_issue_overrides);
	} else {
		return ascli_validate_files_format (&argv[2],
						    argc-2,
						    optn_format,
						    optn_validate_strict,
						    !optn_no_net,
						    optn_issue_overrides);
	}
}

/**
 * as_client_run_validate_tree:
 *
 * Validate an installed filesystem tree for correct AppStream metadata
 * and .desktop files.
 */
static int
as_client_run_validate_tree (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *value = NULL;

	opt_context = as_client_new_subcommand_option_context (command, validate_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		value = argv[2];

	if (optn_format == NULL) {
		return ascli_validate_tree (value,
					    optn_pedantic,
					    optn_explain,
					    optn_validate_strict,
					    !optn_no_net,
					    optn_issue_overrides);
	} else {
		return ascli_validate_tree_format (value,
						   optn_format,
						   optn_validate_strict,
						   !optn_no_net,
						   optn_issue_overrides);
	}
}

/**
 * as_client_run_check_license:
 *
 * Print license information.
 */
static int
as_client_run_check_license (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (command, NULL);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc != 3) {
		/* TRANSLATORS: ascli check-license is missing its parameter */
		ascli_print_stderr (_("No license, license expression or license exception string was provided."));
		return 4;
	}
	return ascli_check_license (argv[2]);
}

/**
 * as_client_run_put:
 *
 * Place a metadata file in the right directory.
 */
static int
as_client_run_put (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *fname = NULL;
	const gchar *optn_origin = NULL;
	gboolean optn_usermode = FALSE;
	gint ret;

	const GOptionEntry put_file_options[] = {
		{ "origin", 0, 0,
			G_OPTION_ARG_STRING,
			&optn_origin,
			/* TRANSLATORS: ascli flag description for: --origin (part of the "put" subcommand) */
			N_("Set the data origin for the installed metadata collection file."), NULL },
		{ "user", 0, 0,
			G_OPTION_ARG_NONE,
			&optn_usermode,
			/* TRANSLATORS: ascli flag description for: --user (part of the "put" subcommand) */
			N_("Install the file for the current user, instead of globally."),
			NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (command, put_file_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		fname = argv[2];
	if (argc > 3) {
		as_client_print_help_hint (command, argv[3]);
		return 1;
	}

	return ascli_put_metainfo (fname, optn_origin, optn_usermode);
}

static const gchar *optn_bundle_type = NULL;
static gboolean optn_choose_first = FALSE;

const GOptionEntry pkgmanage_options[] = {
	{ "bundle-type", 0, 0,
		G_OPTION_ARG_STRING,
		&optn_bundle_type,
		/* TRANSLATORS: ascli flag description for: --bundle-type (part of the "remove" and "install" subcommands) */
		N_("Limit the command to use only components from the given bundling system (`flatpak` or `package`)."), NULL },
	{ "first", 0, 0,
		G_OPTION_ARG_NONE,
		&optn_choose_first,
		/* TRANSLATORS: ascli flag description for: --first (part of the "remove" and "install" subcommands) */
		N_("Do not ask for which software component should be used and always choose the first entry."),
		NULL },
	{ NULL }
};

/**
 * as_client_run_install:
 *
 * Install a component by its ID.
 */
static int
as_client_run_install (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *value = NULL;
	AsBundleKind bundle_kind;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (command, pkgmanage_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		value = argv[2];
	if (argc > 3) {
		as_client_print_help_hint (command, argv[3]);
		return 1;
	}

	bundle_kind = as_bundle_kind_from_string (optn_bundle_type);
	if (optn_bundle_type != NULL && bundle_kind == AS_BUNDLE_KIND_UNKNOWN) {
		/* TRANSLATORS: ascli install currently only supports two values for --bundle-type. */
		ascli_print_stderr (_("No valid bundle kind was specified. Only `package` and `flatpak` are currently recognized."));
		return ASCLI_EXIT_CODE_BAD_INPUT;
	}

	return ascli_install_component (value,
					bundle_kind,
					optn_choose_first);
}

/**
 * as_client_run_remove:
 *
 * Uninstall a component by its ID.
 */
static int
as_client_run_remove (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *value = NULL;
	AsBundleKind bundle_kind;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (command, pkgmanage_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		value = argv[2];
	if (argc > 3) {
		as_client_print_help_hint (command, argv[3]);
		return 1;
	}

	bundle_kind = as_bundle_kind_from_string (optn_bundle_type);
	if (optn_bundle_type != NULL && bundle_kind == AS_BUNDLE_KIND_UNKNOWN) {
		/* TRANSLATORS: ascli install currently only supports two values for --bundle-type. */
		ascli_print_stderr (_("No valid bundle kind was specified. Only `package` and `flatpak` are currently recognized."));
		return ASCLI_EXIT_CODE_BAD_INPUT;
	}

	return ascli_remove_component (value,
					bundle_kind,
					optn_choose_first);
}

/**
 * as_client_run_status:
 *
 * Show diagnostic information.
 */
static int
as_client_run_status (const gchar *command, char **argv, int argc)
{
	if (argc > 2) {
		as_client_print_help_hint (command, argv[3]);
		return 1;
	}

	return ascli_show_status ();
}

/**
 * as_client_run_os_info:
 *
 * Show information about the current operating system.
 */
static int
as_client_run_os_info (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (command, find_options);
	g_option_context_add_main_entries (opt_context, data_collection_options, NULL);

	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2) {
		as_client_print_help_hint (command, argv[3]);
		return 1;
	}

	return ascli_show_os_info (optn_cachepath, optn_no_cache);
}

/**
 * as_client_run_convert:
 *
 * Convert metadata.
 */
static int
as_client_run_convert (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *fname1 = NULL;
	const gchar *fname2 = NULL;
	AsFormatKind mformat;

	opt_context = as_client_new_subcommand_option_context (command, format_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		fname1 = argv[2];
	if (argc > 3)
		fname2 = argv[3];

	mformat = as_format_kind_from_string (optn_format);
	return ascli_convert_data (fname1,
				   fname2,
				   mformat);
}

/**
 * as_client_run_compare_versions:
 *
 * Compare versions using AppStream's version comparison algorithm.
 */
static int
as_client_run_compare_versions (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (command, format_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc < 4) {
		ascli_print_stderr (_("You need to provide at least two version numbers to compare as parameters."));
		return 2;
	}

	if (argc == 4) {
		const gchar *ver1 = argv[2];
		const gchar *ver2 = argv[3];
		gint comp_res = as_vercmp_simple (ver1, ver2);

		if (comp_res == 0)
			g_print ("%s == %s\n", ver1, ver2);
		else if (comp_res > 0)
			g_print ("%s >> %s\n", ver1, ver2);
		else if (comp_res < 0)
			g_print ("%s << %s\n", ver1, ver2);

		return 0;
	} else if (argc == 5) {
		AsRelationCompare compare;
		gint rc;
		gboolean res;
		const gchar *ver1 = argv[2];
		const gchar *comp_str = argv[3];
		const gchar *ver2 = argv[4];

		compare = as_relation_compare_from_string (comp_str);
		if (compare == AS_RELATION_COMPARE_UNKNOWN) {
			guint i;
			/** TRANSLATORS: The user tried to compare version numbers, but the comparison operator (greater-then, equal, etc.) was invalid. */
			ascli_print_stderr (_("Unknown compare relation '%s'. Valid values are:"), comp_str);
			for (i = 1; i < AS_RELATION_COMPARE_LAST; i++)
				g_printerr (" • %s\n", as_relation_compare_to_string (i));
			return 2;
		}

		rc = as_vercmp_simple (ver1, ver2);
		switch (compare) {
		case AS_RELATION_COMPARE_EQ:
			res = rc == 0;
			break;
		case AS_RELATION_COMPARE_NE:
			res = rc != 0;
			break;
		case AS_RELATION_COMPARE_LT:
			res = rc < 0;
			break;
		case AS_RELATION_COMPARE_GT:
			res = rc > 0;
			break;
		case AS_RELATION_COMPARE_LE:
			res = rc <= 0;
			break;
		case AS_RELATION_COMPARE_GE:
			res = rc >= 0;
			break;
		default:
			res = FALSE;
		}

		g_print ("%s: ", res? "true" : "false");
		if (rc == 0)
			g_print ("%s == %s\n", ver1, ver2);
		else if (rc > 0)
			g_print ("%s >> %s\n", ver1, ver2);
		else if (rc < 0)
			g_print ("%s << %s\n", ver1, ver2);

		return res? 0 : 1;
	} else {
		ascli_print_stderr (_("Too many parameters: Need two version numbers or version numbers and a comparison operator."));
		return 2;
	}
}

/**
 * as_client_run_new_template:
 *
 * Convert metadata.
 */
static int
as_client_run_new_template (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GString) desc_str = NULL;
	guint i;
	gint ret;
	const gchar *out_fname = NULL;
	const gchar *cpt_kind_str = NULL;
	const gchar *optn_desktop_file = NULL;

	const GOptionEntry newtemplate_options[] = {
		{ "from-desktop", 0, 0,
			G_OPTION_ARG_STRING,
			&optn_desktop_file,
			/* TRANSLATORS: ascli flag description for: --from-desktop (part of the new-template subcommand) */
			N_("Use the given .desktop file to fill in the basic values of the metainfo file."), NULL },
		{ NULL }
	};

	/* TRANSLATORS: Additional help text for the 'new-template' ascli subcommand */
	desc_str = g_string_new (_("This command takes optional TYPE and FILE positional arguments, FILE being a file to write to (or \"-\" for standard output)."));
	g_string_append (desc_str, "\n");
	/* TRANSLATORS: Additional help text for the 'new-template' ascli subcommand, a bullet-pointed list of types follows */
	g_string_append_printf (desc_str, _("The TYPE must be a valid component-type, such as: %s"), "\n");
	for (i = 1; i < AS_COMPONENT_KIND_LAST; i++)
		g_string_append_printf (desc_str, " • %s\n", as_component_kind_to_string (i));

	opt_context = as_client_new_subcommand_option_context (command, newtemplate_options);
	g_option_context_set_description (opt_context, desc_str->str);

	ret = as_client_option_context_parse (opt_context,
					      command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		cpt_kind_str = argv[2];
	if (argc > 3)
		out_fname = argv[3];

	return ascli_create_metainfo_template (out_fname,
					       cpt_kind_str,
					       optn_desktop_file);
}

/**
 * as_client_run_make_desktop_file:
 *
 * Create desktop-entry file from metainfo file.
 */
static int
as_client_run_make_desktop_file (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *optn_exec_command = NULL;
	const gchar *mi_fname = NULL;
	const gchar *de_fname = NULL;
	gint ret;

	const GOptionEntry make_desktop_file_options[] = {
		{ "exec", 0, 0,
			G_OPTION_ARG_STRING,
			&optn_exec_command,
			/* TRANSLATORS: ascli flag description for: --exec (part of the make-desktop-file subcommand) */
			N_("Use the specified line for the 'Exec=' key of the desktop-entry file."), NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (command, make_desktop_file_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		mi_fname = argv[2];
	if (argc > 3)
		de_fname = argv[3];

	return ascli_make_desktop_entry_file (mi_fname,
					      de_fname,
					      optn_exec_command);
}

/**
 * as_client_run_news_to_metainfo:
 *
 * Convert NEWS file to metainfo data.
 */
static int
as_client_run_news_to_metainfo (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *optn_format_text = NULL;
	gint optn_limit = 0;
	gint optn_translatable_n = -1;
	const gchar *mi_fname = NULL;
	const gchar *news_fname = NULL;
	const gchar *out_fname = NULL;
	gint ret;

	const GOptionEntry news_to_metainfo_options[] = {
		{ "format", 0, 0,
			G_OPTION_ARG_STRING,
			&optn_format_text,
			/* TRANSLATORS: ascli flag description for: --format as part of the news-to-metainfo command */
			N_("Assume the input file is in the selected format ('yaml' or 'text')."),
			NULL },
		{ "limit", 'l', 0,
			G_OPTION_ARG_INT,
			&optn_limit,
			/* TRANSLATORS: ascli flag description for: --limit as part of the news-to-metainfo command */
			N_("Limit the number of release entries that end up in the metainfo file (<= 0 for unlimited)."),
			NULL },
		{ "translatable-count", 't', 0,
			G_OPTION_ARG_INT,
			&optn_translatable_n,
			/* TRANSLATORS: ascli flag description for: --translatable-count as part of the news-to-metainfo command */
			N_("Set the number of releases that should have descriptions marked for translation (latest releases are translated first, -1 for unlimited)."),
			NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (command, news_to_metainfo_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		news_fname = argv[2];
	if (argc > 3)
		mi_fname = argv[3];
	if (argc > 4)
		out_fname = argv[4];

	return ascli_news_to_metainfo (news_fname,
					mi_fname,
					out_fname,
					optn_limit,
					optn_translatable_n,
					optn_format_text);
}

/**
 * as_client_run_metainfo_to_news:
 *
 * Convert metainfo data to NEWS file.
 */
static int
as_client_run_metainfo_to_news (const gchar *command, char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *optn_format_text = NULL;
	const gchar *mi_fname = NULL;
	const gchar *news_fname = NULL;
	gint ret;

	const GOptionEntry metainfo_to_news_options[] = {
		{ "format", 0, 0,
			G_OPTION_ARG_STRING,
			&optn_format_text,
			/* TRANSLATORS: ascli flag description for: --format as part of the metainfo-to-news command */
			N_("Generate the output in the selected format ('yaml' or 'text')."), NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (command, metainfo_to_news_options);
	ret = as_client_option_context_parse (opt_context, command, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 2)
		mi_fname = argv[2];
	if (argc > 3)
		news_fname = argv[3];

	return ascli_metainfo_to_news (mi_fname,
					news_fname,
					optn_format_text);
}

/**
 * as_client_check_compose_available:
 */
static gboolean
as_client_check_compose_available (void)
{
	return g_file_test (LIBEXECDIR "/appstreamcli-compose", G_FILE_TEST_EXISTS);
}

/**
 * as_client_run_compose:
 *
 * Delegate the "compose" command to the appstream-compose binary,
 * if it is available.
 */
static int
as_client_run_compose (const gchar *command, char **argv, int argc)
{
	const gchar *ascompose_exe = LIBEXECDIR "/appstreamcli-compose";
	g_autofree const gchar **asc_argv = NULL;

	if (!g_file_test (ascompose_exe, G_FILE_TEST_EXISTS)) {
		/* TRANSLATORS: appstreamcli-compose was not found */
		ascli_print_stderr (_("Compose binary '%s' was not found! Can not continue."), ascompose_exe);
		ascli_print_stderr (_("You may be able to install the AppStream Compose addon via: `%s`"),
				    "sudo appstreamcli install org.freedesktop.appstream.compose");
		return 4;
	}

	asc_argv = g_new0 (const gchar*, argc + 2);
	asc_argv[0] = ascompose_exe;
	if (argc < 2) {
		/* TRANSLATORS: Unexpected number of parameters on the command-line */
		ascli_print_stderr (_("Invalid number of parameters"));
		return 5;
	}
	for (gint i = 2; i < argc; i++)
		asc_argv[i-1] = argv[i];

	return execv(ascompose_exe, (gchar * const*) asc_argv);
}

typedef gboolean (*AsCliCommandCb) (const gchar *command,
				    gchar **argv,
				    gint argc);

typedef struct {
	gchar		*name;
	gchar		*alias;
	gchar		*arguments;
	gchar		*summary;
	guint		 block_id;
	AsCliCommandCb	 callback;
} AsCliCommandItem;

/**
 * ascli_command_item_free:
 */
static void
ascli_command_item_free (AsCliCommandItem *item)
{
	g_free (item->name);
	g_free (item->alias);
	g_free (item->arguments);
	g_free (item->summary);
	g_free (item);
}

/**
 * ascli_add_cmd:
 */
static void
ascli_add_cmd (GPtrArray *commands,
		guint block_id,
		const gchar *name,
		const gchar *alias,
		const gchar *arguments,
		const gchar *summary,
		AsCliCommandCb callback)
{
	AsCliCommandItem *item;

	g_return_if_fail (name != NULL);
	g_return_if_fail (summary != NULL);
	g_return_if_fail (callback != NULL);

	item = g_new0 (AsCliCommandItem, 1);
	item->block_id = block_id;
	item->name = g_strdup (name);
	if (alias != NULL) {
		g_autofree gchar *tmp = NULL;
		/* TRANSLATORS: this is a (usually shorter) command alias, shown after the command summary text */
		tmp = g_strdup_printf (_("(Alias: '%s')"), alias);
		item->summary = g_strconcat (summary, " ", tmp, NULL);
		item->alias = g_strdup (alias);
	} else {
		item->summary = g_strdup (summary);
	}
	if (arguments == NULL)
		item->arguments = g_strdup ("");
	else
		item->arguments = g_strdup (arguments);
	item->callback = callback;
	g_ptr_array_add (commands, item);
}

/**
 * as_client_get_help_summary:
 **/
static gchar*
as_client_get_help_summary (GPtrArray *commands)
{
	guint current_block_id = 0;
	gboolean compose_available = FALSE;
	g_autoptr(GArray) blocks_maxlen = NULL;
	GString *string = g_string_new ("");

	/* TRANSLATORS: This is the header to the --help menu */
	g_string_append_printf (string, "%s\n\n%s\n", _("AppStream command-line interface"),
				/* these are commands we can use with appstreamcli */
				_("Subcommands:"));

	compose_available = as_client_check_compose_available ();
	blocks_maxlen = g_array_new (FALSE, FALSE, sizeof (guint));
	for (guint i = 0; i < commands->len; i++) {
		guint nlen;
		guint *elen_p;
		AsCliCommandItem *item = (AsCliCommandItem *) g_ptr_array_index (commands, i);

		while (blocks_maxlen->len < (item->block_id + 1)) {
			guint min_len = 26;
			g_array_append_val (blocks_maxlen, min_len);
		}
		nlen = strlen (item->name) + strlen (item->arguments);
		elen_p = &g_array_index (blocks_maxlen, guint, item->block_id);
		if (nlen > *elen_p)
			*elen_p = nlen;
	}

	for (guint i = 0; i < commands->len; i++) {
		guint term_len;
		guint block_maxlen;
		guint synopsis_len;
		g_autofree gchar *summary_wrap = NULL;
		AsCliCommandItem *item = (AsCliCommandItem *) g_ptr_array_index (commands, i);

		/* don't display compose help if ascompose binary was not found */
		if (!compose_available && g_strcmp0 (item->name, "compose") == 0)
			continue;

		if (item->block_id != current_block_id) {
			current_block_id = item->block_id;
			g_string_append (string, "\n");
		}

		block_maxlen = g_array_index (blocks_maxlen, guint, item->block_id);
		term_len = strlen (item->name) + strlen (item->arguments);

		g_string_append_printf (string, "  %s %s%*s",
					item->name,
					item->arguments,
					(block_maxlen - term_len) + 1, "");
		synopsis_len = block_maxlen + 3 + 1 ;
		summary_wrap = ascli_format_long_output (item->summary,
							 synopsis_len + 72,
							 synopsis_len + 2);
		g_strstrip (summary_wrap);
		g_string_append_printf (string, "- %s\n", summary_wrap);
	}

	g_string_append (string, "\n");
	g_string_append (string, _("You can find information about subcommand-specific options by passing \"--help\" to the subcommand."));

	return g_string_free (string, FALSE);
}

/**
 * ascli_run_command:
 *
 * Run a subcommand with the given parameters.
 */
static gint
ascli_run_command (GPtrArray *commands, const gchar *command, char **argv, int argc)
{
	for (guint i = 0; i < commands->len; i++) {
		AsCliCommandItem *item = (AsCliCommandItem *) g_ptr_array_index (commands, i);

		if (g_strcmp0 (command, item->name) == 0)
			return item->callback (item->name, argv, argc);
		if ((item->alias != NULL) && (g_strcmp0 (command, item->alias) == 0))
			return item->callback (item->name, argv, argc);
	}

	/* TRANSLATORS: ascli has been run with unknown command. '%s --help' is the command to receive help and should not be translated. */
	ascli_print_stderr (_("Command '%s' is unknown. Run '%s --help' for a list of available commands."), command, argv[0]);
	return 1;
}

/**
 * as_client_run:
 */
static int
as_client_run (char **argv, int argc)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GPtrArray) commands = NULL;
	g_autoptr(AsProfile) profile = NULL;
	AsProfileTask *ptask;
	gboolean enable_profiling = FALSE;
	gint retval = 0;
	const gchar *command = NULL;


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
			_("Don\'t show colored output."), NULL },
		{ "profile", '\0', 0, G_OPTION_ARG_NONE, &enable_profiling,
			/* TRANSLATORS: ascli flag description for: --profile */
			_("Enable profiling"), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- AppStream CLI.");
	g_option_context_add_main_entries (opt_context, client_options, NULL);

	/* register all available subcommands */
	commands = g_ptr_array_new_with_free_func ((GDestroyNotify) ascli_command_item_free);
	ascli_add_cmd (commands,
			0, "search", "s", "TERM",
			/* TRANSLATORS: `appstreamcli search` command description. */
			_("Search the component database."),
			as_client_run_search);
	ascli_add_cmd (commands,
			0, "get", NULL, "COMPONENT-ID",
			/* TRANSLATORS: `appstreamcli get` command description. */
			_("Get information about a component by its ID."),
			as_client_run_get);
	ascli_add_cmd (commands,
			0, "what-provides", NULL, "TYPE VALUE",
			/* TRANSLATORS: `appstreamcli what-provides` command description. */
			_("Get components which provide the given item. Needs an item type (e.g. lib, bin, python3, …) and item value as parameter."),
			as_client_run_what_provides);

	ascli_add_cmd (commands,
			1, "dump", NULL, "COMPONENT-ID",
			/* TRANSLATORS: `appstreamcli dump` command description. */
			_("Dump raw XML metadata for a component matching the ID."),
			as_client_run_dump);
	ascli_add_cmd (commands,
			1, "refresh-cache", "refresh", NULL,
			/* TRANSLATORS: `appstreamcli refresh-cache` command description. */
			_("Rebuild the component metadata cache."),
			as_client_run_refresh_cache);

	ascli_add_cmd (commands,
			2, "validate", NULL, "FILE",
			/* TRANSLATORS: `appstreamcli validate` command description. */
			_("Validate AppStream XML files for issues."),
			as_client_run_validate);
	ascli_add_cmd (commands,
			2, "validate-tree", NULL, "DIRECTORY",
			/* TRANSLATORS: `appstreamcli validate-tree` command description. */
			_("Validate an installed file-tree of an application for valid metadata."),
			as_client_run_validate_tree);
	ascli_add_cmd (commands,
			2, "check-license", NULL, "LICENSE",
			/* TRANSLATORS: `appstreamcli `check-license` command description. */
			_("Check license string for validity and print details about it."),
			as_client_run_check_license);

	ascli_add_cmd (commands,
			3, "install", NULL, "COMPONENT-ID",
			/* TRANSLATORS: `appstreamcli install` command description. */
			_("Install software matching the component-ID."),
			as_client_run_install);
	ascli_add_cmd (commands,
			3, "remove", NULL, "COMPONENT-ID",
			/* TRANSLATORS: `appstreamcli remove` command description. */
			_("Remove software matching the component-ID."),
			as_client_run_remove);

	ascli_add_cmd (commands,
			4, "status", NULL, NULL,
			/* TRANSLATORS: `appstreamcli status` command description. */
			_("Display status information about available AppStream metadata."),
			as_client_run_status);
	ascli_add_cmd (commands,
			4, "os-info", NULL, NULL,
			/* TRANSLATORS: `appstreamcli os-info` command description. */
			_("Show information about the current operating system from the metadata index."),
			as_client_run_os_info);
	ascli_add_cmd (commands,
			4, "put", NULL, "FILE",
			/* TRANSLATORS: `appstreamcli put` command description. */
			_("Install a metadata file into the right location."),
			as_client_run_put);
	ascli_add_cmd (commands,
			4, "convert", NULL, "FILE FILE",
			/* TRANSLATORS: `appstreamcli convert` command description. "Collection XML" is a term describing a specific type of AppStream XML data. */
			_("Convert collection XML to YAML or vice versa."),
			as_client_run_convert);
	ascli_add_cmd (commands,
			4, "vercmp", "compare-versions", "VER1 [COMP] VER2",
			/* TRANSLATORS: `appstreamcli vercmp` command description. */
			_("Compare two version numbers."),
			as_client_run_compare_versions);

	ascli_add_cmd (commands,
			5, "new-template", NULL, "TYPE FILE",
			/* TRANSLATORS: `appstreamcli new-template` command description. */
			_("Create a template for a metainfo file (to be filled out by the upstream project)."),
			as_client_run_new_template);
	ascli_add_cmd (commands,
			5, "make-desktop-file", NULL, "MI_FILE DESKTOP_FILE",
			/* TRANSLATORS: `appstreamcli make-desktop-file` command description. */
			_("Create a desktop-entry file from a metainfo file."),
			as_client_run_make_desktop_file);
	ascli_add_cmd (commands,
			5, "news-to-metainfo", NULL, "NEWS_FILE MI_FILE [OUT_FILE]",
			/* TRANSLATORS: `appstreamcli news-to-metainfo` command description. */
			_("Convert a YAML or text NEWS file into metainfo releases."),
			as_client_run_news_to_metainfo);
	ascli_add_cmd (commands,
			5, "metainfo-to-news", NULL, "MI_FILE NEWS_FILE",
			/* TRANSLATORS: `appstreamcli metainfo-to-news` command description. */
			_("Write NEWS text or YAML file with information from a metainfo file."),
			as_client_run_metainfo_to_news);
	ascli_add_cmd (commands,
			5, "compose", NULL, NULL,
			/* TRANSLATORS: `appstreamcli compose` command description. */
			_("Compose AppStream collection metadata from directory trees."),
			as_client_run_compose);

	/* we handle the unknown options later in the individual subcommands */
	g_option_context_set_ignore_unknown_options (opt_context, TRUE);

	if (argc < 2) {
		/* TRANSLATORS: ascli has been run without command. */
		g_printerr ("%s\n", _("You need to specify a command."));
		ascli_print_stderr (_("Run '%s --help' to see a full list of available command line options."), argv[0]);
		return 1;
	}
	command = argv[1];

	/* only attempt to show global help if we don't have a subcommand as first parameter (subcommands are never prefixed with "-") */
	if (g_str_has_prefix (command, "-")) {
		/* set the summary text */
		g_autofree gchar *summary = NULL;
		summary = as_client_get_help_summary (commands);
		g_option_context_set_summary (opt_context, summary) ;
		g_option_context_set_help_enabled (opt_context, TRUE);
	} else {
		g_option_context_set_help_enabled (opt_context, FALSE);
	}

	retval = as_client_option_context_parse (opt_context, NULL, &argc, &argv);
	if (retval != 0)
		return retval;

	if (optn_show_version) {
		if (g_strcmp0 (as_version_string (), PACKAGE_VERSION) == 0) {
			/* TRANSLATORS: Output if appstreamcli --version is executed. */
			ascli_print_stdout (_("AppStream version: %s"), PACKAGE_VERSION);
		} else {
			/* TRANSLATORS: Output if appstreamcli --version is run and the CLI and libappstream versions differ. */
			ascli_print_stdout (_("AppStream CLI tool version: %s\nAppStream library version: %s"), PACKAGE_VERSION, as_version_string ());
		}
		return 0;
	}

	/* just a hack, we might need proper message handling later */
	if (optn_verbose_mode) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}

	/* allow disabling network access via an environment variable */
	if (g_getenv ("AS_VALIDATE_NONET") != NULL) {
		g_debug ("Disabling network usage: Environment variable AS_VALIDATE_NONET is set.");
		optn_no_net = TRUE;
	}

	/* set some global defaults, in case we run as root in an unsafe environment */
	if (as_utils_is_root ()) {
		/* users umask shouldn't interfere with us creating new files when we are root */
		as_reset_umask ();

		/* ensure we never start gvfsd as root: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=852696 */
		g_setenv ("GIO_USE_VFS", "local", TRUE);
	}

	ascli_set_output_colored (!optn_no_color);

	/* if out terminal is no tty, disable colors automatically */
	if (!isatty (fileno (stdout)))
		ascli_set_output_colored (FALSE);

	/* don't let gvfsd start its own session bus: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=852696 */
	g_setenv ("GIO_USE_VFS", "local", TRUE);

	/* prepare profiler */
	profile = as_profile_new ();

	/* run subcommand */
	ptask = as_profile_start (profile, "%s: %s", argv[0], command);
	retval = ascli_run_command (commands, command, argv, argc);
	as_profile_task_free (ptask);

	/* profile */
	if (enable_profiling)
		as_profile_dump (profile);

	return retval;
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
