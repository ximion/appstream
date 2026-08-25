/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
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
#ifdef G_OS_WIN32
#include <io.h>
#endif

#include "as-profile.h"
#include "as-utils-private.h"

#include "ascli-utils.h"
#include "ascli-actions-mdata.h"
#include "ascli-actions-validate.h"
#include "ascli-actions-pkgmgr.h"
#include "ascli-actions-misc.h"

#define ASCLI_BIN_NAME "appstreamcli"

typedef struct _AsCliCommand AsCliCommand;

/**
 * AsCliCommandFunc:
 * @cmd: the #AsCliCommand that is being run
 * @argc: argument count, with the subcommand name as first argument
 * @argv: argument vector, with the subcommand name as first argument
 *
 * Handler for an appstreamcli subcommand.
 */
typedef gint (*AsCliCommandFunc) (AsCliCommand *cmd, gint argc, gchar **argv);

/**
 * AsCliCommand:
 *
 * A subcommand of appstreamcli.
 */
struct _AsCliCommand {
	gchar *name;
	gchar *alias;
	gchar *arguments;
	gchar *summary;
	guint block_id;
	AsCliCommandFunc func;
};

/* global options which affect all commands */
static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;
static gboolean optn_no_color = FALSE;
static gboolean optn_enable_profiling = FALSE;

/**
 * Global options which are valid for all subcommands.
 */
const GOptionEntry ascli_global_options[] = {
	{ "version",
	  0, 0,
	  G_OPTION_ARG_NONE, &optn_show_version,
	  /* TRANSLATORS: ascli flag description for: --version */
	  N_ ("Show the program version."),
	  NULL },
	{ "verbose",
	  0, 0,
	  G_OPTION_ARG_NONE, &optn_verbose_mode,
	  /* TRANSLATORS: ascli flag description for: --verbose */
	  N_ ("Show extra debugging information."),
	  NULL },
	{ "no-color",
	  0, 0,
	  G_OPTION_ARG_NONE, &optn_no_color,
	  /* TRANSLATORS: ascli flag description for: --no-color */
	  N_ ("Don\'t show colored output."),
	  NULL },
	{ "profile",
	  0, 0,
	  G_OPTION_ARG_NONE, &optn_enable_profiling,
	  /* TRANSLATORS: ascli flag description for: --profile */
	  N_ ("Enable profiling"),
	  NULL },
	{ NULL }
};

/*** COMMAND OPTIONS ***/

/* for data_catalog_options */
static gchar *optn_cachepath = NULL;
static gchar *optn_datapath = NULL;
static gboolean optn_no_cache = FALSE;

/**
 * General options used for any operations on
 * metadata catalogs and the cache.
 */
const GOptionEntry data_catalog_options[] = {
	{ "cachepath",
	  0, 0,
	  G_OPTION_ARG_STRING, &optn_cachepath,
	  /* TRANSLATORS: ascli flag description for: --cachepath */
	  N_ ("Manually selected location of AppStream cache."),
	  NULL },
	{ "datapath",
	  0, 0,
	  G_OPTION_ARG_STRING, &optn_datapath,
	  /* TRANSLATORS: ascli flag description for: --datapath */
	  N_ ("Manually selected location of AppStream metadata to scan."),
	  NULL },
	{ "no-cache",
	  0, 0,
	  G_OPTION_ARG_NONE, &optn_no_cache,
	  /* TRANSLATORS: ascli flag description for: --no-cache */
	  N_ ("Ignore cache age and build a fresh cache before performing the query."),
	  NULL },
	{ NULL }
};

/* used by format_options */
static gchar *optn_format = NULL;

/**
 * The format option.
 */
const GOptionEntry format_options[] = {
	{ "format",
	  0, 0,
	  G_OPTION_ARG_STRING, &optn_format,
	  /* TRANSLATORS: ascli flag description for: --format */
	  N_ ("Default metadata format (valid values are 'xml' and 'yaml')."),
	  NULL },
	{ NULL }
};

/* used by find_options */
static gboolean optn_details = FALSE;

/**
 * General options for finding & displaying data.
 */
const GOptionEntry find_options[] = {
	{ "details",
	  0, 0,
	  G_OPTION_ARG_NONE, &optn_details,
	  /* TRANSLATORS: ascli flag description for: --details */
	  N_ ("Print detailed output about found components."),
	  NULL },
	{ NULL }
};

/* used by reviews_options */
static gchar *optn_reviews_server = NULL;
static gchar *optn_reviews_locale = NULL;
static gint optn_reviews_start = 0;
static gint optn_reviews_limit = 15;

/**
 * Options used when fetching user reviews.
 */
const GOptionEntry reviews_options[] = {
	{ "server",
	  0, 0,
	  G_OPTION_ARG_STRING, &optn_reviews_server,
	  /* TRANSLATORS: ascli flag description for: --server (used by the "list-reviews" command) */
	  N_ ("URL of the ODRS-compatible reviews server to use."),
	  NULL },
	{ "start",
	  0, 0,
	  G_OPTION_ARG_INT, &optn_reviews_start,
	  /* TRANSLATORS: ascli flag description for: --start (used by the "list-reviews" command) */
	  N_ ("Index of the first review to fetch, to page through all reviews."),
	  NULL },
	{ "limit",
	  0, 0,
	  G_OPTION_ARG_INT, &optn_reviews_limit,
	  /* TRANSLATORS: ascli flag description for: --limit (used by the "list-reviews" command) */
	  N_ ("Maximum number of reviews to fetch."),
	  NULL },
	{ "locale",
	  0, 0,
	  G_OPTION_ARG_STRING, &optn_reviews_locale,
	  /* TRANSLATORS: ascli flag description for: --locale (used by the "list-reviews" and "submit-review" commands) */
	  N_ ("Locale to prefer for reviews, instead of the current system locale."),
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
	{ "pedantic",
	  (gchar) 0,
	  0, G_OPTION_ARG_NONE,
	  &optn_pedantic,
	  /* TRANSLATORS: ascli flag description for: --pedantic (used by the "validate" command) */
	  N_ ("Also show pedantic hints."),
	  NULL },
	{ "explain",
	  (gchar) 0,
	  0, G_OPTION_ARG_NONE,
	  &optn_explain,
	  /* TRANSLATORS: ascli flag description for: --explain (used by the "validate" command) */
	  N_ ("Print detailed explanation for found issues."),
	  NULL },
	{ "no-net",
	  (gchar) 0,
	  0, G_OPTION_ARG_NONE,
	  &optn_no_net,
	  /* TRANSLATORS: ascli flag description for: --no-net (used by the "validate" command) */
	  N_ ("Do not use network access."),
	  NULL },
	{ "strict",
	  (gchar) 0,
	  0, G_OPTION_ARG_NONE,
	  &optn_validate_strict,
	  /* TRANSLATORS: ascli flag description for: --strict (used by the "validate" command) */
	  N_ ("Fail validation if any issue is emitted that is not of pedantic severity."),
	  NULL },
	{ "format",
	  0, 0,
	  G_OPTION_ARG_STRING, &optn_format,
	  /* TRANSLATORS: ascli flag description for: --format  when validating XML files */
	  N_ ("Format of the generated report (valid values are 'text' and 'yaml')."),
	  NULL },
	{ "override",
	  0, 0,
	  G_OPTION_ARG_STRING, &optn_issue_overrides,
	  /* TRANSLATORS: ascli flag description for: --override  when validating XML files */
	  N_ ("Override the severities of selected issue tags."),
	  NULL },

	{ NULL }
};

/*** HELPER METHODS ***/

/**
 * ascli_command_free:
 */
static void
ascli_command_free (AsCliCommand *cmd)
{
	g_free (cmd->name);
	g_free (cmd->alias);
	g_free (cmd->arguments);
	g_free (cmd->summary);
	g_free (cmd);
}

/**
 * ascli_add_cmd:
 *
 * Register a new subcommand.
 */
static void
ascli_add_cmd (GPtrArray *commands,
	       guint block_id,
	       const gchar *name,
	       const gchar *alias,
	       const gchar *arguments,
	       const gchar *summary,
	       AsCliCommandFunc func)
{
	AsCliCommand *cmd;

	g_return_if_fail (name != NULL);
	g_return_if_fail (summary != NULL);
	g_return_if_fail (func != NULL);

	cmd = g_new0 (AsCliCommand, 1);
	cmd->block_id = block_id;
	cmd->name = g_strdup (name);
	if (alias != NULL) {
		g_autofree gchar *tmp = NULL;
		/* TRANSLATORS: this is a (usually shorter) command alias, shown after the command summary text */
		tmp = g_strdup_printf (_("(Alias: '%s')"), alias);
		cmd->summary = g_strconcat (summary, " ", tmp, NULL);
		cmd->alias = g_strdup (alias);
	} else {
		cmd->summary = g_strdup (summary);
	}
	if (arguments == NULL)
		cmd->arguments = g_strdup ("");
	else
		cmd->arguments = g_strdup (arguments);
	cmd->func = func;
	g_ptr_array_add (commands, cmd);
}

/**
 * ascli_find_command:
 *
 * Find a registered subcommand by its name or alias.
 *
 * Returns: (transfer none): the #AsCliCommand, or %NULL if it was not found.
 */
static AsCliCommand *
ascli_find_command (GPtrArray *commands, const gchar *name)
{
	if (name == NULL)
		return NULL;

	for (guint i = 0; i < commands->len; i++) {
		AsCliCommand *cmd = g_ptr_array_index (commands, i);

		if (g_strcmp0 (cmd->name, name) == 0)
			return cmd;
		if (cmd->alias != NULL && g_strcmp0 (cmd->alias, name) == 0)
			return cmd;
	}

	return NULL;
}

/**
 * as_client_get_summary_for:
 **/
static gchar *
as_client_get_summary_for (AsCliCommand *cmd)
{
	GString *string;
	string = g_string_new ("");

	/* TRANSLATORS: This is the header to the --help menu for subcommands */
	g_string_append_printf (string, "%s\n", _("AppStream command-line interface"));

	g_string_append (string, " ");
	g_string_append_printf (string, _("'%s' command"), cmd->name);
	g_string_append_printf (string, "\n %s", cmd->summary);

	return g_string_free (string, FALSE);
}

/**
 * as_client_new_subcommand_option_context:
 *
 * Create a new option context for an ascli subcommand.
 */
static GOptionContext *
as_client_new_subcommand_option_context (AsCliCommand *cmd, const GOptionEntry *entries)
{
	GOptionContext *opt_context = NULL;
	g_autofree gchar *summary = NULL;
	g_autofree gchar *parameter_str = NULL;

	if (as_is_empty (cmd->arguments))
		parameter_str = g_strdup (cmd->name);
	else
		parameter_str = g_strconcat (cmd->name, " ", cmd->arguments, NULL);

	opt_context = g_option_context_new (parameter_str);
	g_option_context_set_help_enabled (opt_context, TRUE);
	if (entries != NULL)
		g_option_context_add_main_entries (opt_context, entries, NULL);

	/* set the summary text */
	summary = as_client_get_summary_for (cmd);
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
		ascli_print_stderr (
		    _("Run '%s --help' to see a full list of available command line options."),
		      ASCLI_BIN_NAME);
	else
		ascli_print_stderr (
		    _("Run '%s --help' to see a list of available commands and options, and '%s %s --help' to see a list of options specific for this subcommand."),
		      ASCLI_BIN_NAME,
		      ASCLI_BIN_NAME,
		      subcommand);
}

/**
 * as_client_option_context_parse:
 *
 * Parse the options, print errors.
 */
static gint
as_client_option_context_parse (GOptionContext *opt_context,
				AsCliCommand *cmd,
				gint *argc,
				gchar ***argv)
{
	g_autoptr(GError) error = NULL;

	g_option_context_parse (opt_context, argc, argv, &error);
	if (error != NULL) {
		gchar *msg;
		msg = g_strconcat (error->message, "\n", NULL);
		g_print ("%s", msg);
		g_free (msg);

		as_client_print_help_hint (cmd == NULL ? NULL : cmd->name, NULL);
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
static gint
as_client_run_refresh_cache (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	gboolean optn_force = FALSE;
	g_auto(GStrv) optn_sources = NULL;
	g_auto(GStrv) optn_sources_real = NULL;

	const GOptionEntry refresh_options[] = {
		{ "force",
		  (gchar) 0,
		  0, G_OPTION_ARG_NONE,
		  &optn_force,
		  /* TRANSLATORS: ascli flag description for: --force */
		  _("Enforce a cache refresh."), NULL },
		  { "source",
		    (gchar) 0,
		    0, G_OPTION_ARG_STRING_ARRAY,
		    &optn_sources,
		    /* TRANSLATORS: ascli flag description for: --source in a refresh action. Don't translate strings in backticks: `name` */
		    _("Limit cache refresh to data from a specific source, e.g. `os` or `flatpak`. May be specified multiple times."), NULL },
		    { NULL }
	     };

	opt_context = as_client_new_subcommand_option_context (cmd, refresh_options);
	g_option_context_add_main_entries (opt_context, data_catalog_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
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
				    (const gchar *const *) optn_sources_real,
				    optn_force);
}

/**
 * as_client_run_search:
 *
 * Search for AppStream metadata.
 */
static gint
as_client_run_search (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GString) search = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, find_options);
	g_option_context_add_main_entries (opt_context, data_catalog_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	search = g_string_new ("");
	if (argc > 1) {
		for (gint i = 1; i < argc; i++) {
			g_string_append (search, argv[i]);
			g_string_append_c (search, ' ');
		}
		/* drop trailing space */
		if (search->len > 0)
			g_string_truncate (search, search->len - 1);
	}

	return ascli_search_component (optn_cachepath,
				       (search->len == 0) ? NULL : search->str,
				       optn_details,
				       optn_no_cache);
}

/**
 * as_client_run_get:
 *
 * Get components by its ID.
 */
static gint
as_client_run_get (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *value = NULL;

	opt_context = as_client_new_subcommand_option_context (cmd, find_options);
	g_option_context_add_main_entries (opt_context, data_catalog_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		value = argv[1];

	return ascli_get_component (optn_cachepath, value, optn_details, optn_no_cache);
}

/**
 * as_client_run_dump:
 *
 * Dump the raw component metadata to the console.
 */
static gint
as_client_run_dump (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *value = NULL;
	AsFormatKind mformat;

	opt_context = as_client_new_subcommand_option_context (cmd, data_catalog_options);
	g_option_context_add_main_entries (opt_context, format_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		value = argv[1];

	mformat = as_format_kind_from_string (optn_format);
	return ascli_dump_component (optn_cachepath, value, mformat, optn_no_cache);
}

/**
 * as_client_run_what_provides:
 *
 * Find components that provide a certain item.
 */
static gint
as_client_run_what_provides (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *vtype = NULL;
	const gchar *vvalue = NULL;

	opt_context = as_client_new_subcommand_option_context (cmd, find_options);
	g_option_context_add_main_entries (opt_context, data_catalog_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		vtype = argv[1];
	if (argc > 2)
		vvalue = argv[2];

	return ascli_what_provides (optn_cachepath, vtype, vvalue, optn_details);
}

/**
 * as_client_run_list_categories:
 *
 * Find components that are in the listed categories.
 */
static gint
as_client_run_list_categories (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	g_auto(GStrv) categories = NULL;

	opt_context = as_client_new_subcommand_option_context (cmd, find_options);
	g_option_context_add_main_entries (opt_context, data_catalog_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1) {
		categories = g_new0 (gchar *, argc);
		for (gint i = 0; i < (argc - 1); i++)
			categories[i] = g_strdup (argv[i + 1]);
	}

	return ascli_list_categories (optn_cachepath, categories, optn_details, optn_no_cache);
}

/**
 * as_client_run_validate:
 *
 * Validate single metadata files.
 */
static gint
as_client_run_validate (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, validate_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (optn_format == NULL) {
		return ascli_validate_files (&argv[1],
					     argc - 1,
					     optn_pedantic,
					     optn_explain,
					     optn_validate_strict,
					     !optn_no_net,
					     optn_issue_overrides);
	} else {
		return ascli_validate_files_format (&argv[1],
						    argc - 1,
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
static gint
as_client_run_validate_tree (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *value = NULL;

	opt_context = as_client_new_subcommand_option_context (cmd, validate_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		value = argv[1];

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
static gint
as_client_run_check_license (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, NULL);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc != 2) {
		ascli_print_stderr (
		    /* TRANSLATORS: ascli check-license is missing its parameter */
		    _("No license, license expression or license exception string was provided."));
		return 4;
	}
	return ascli_check_license (argv[1]);
}

/**
 * as_client_run_is_satisfied:
 *
 * Test if a component has its relations satisfied on the current system.
 */
static gint
as_client_run_is_satisfied (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *fname_or_cid = NULL;

	opt_context = as_client_new_subcommand_option_context (cmd, find_options);
	g_option_context_add_main_entries (opt_context, data_catalog_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		fname_or_cid = argv[1];

	return ascli_check_is_satisfied (fname_or_cid, optn_cachepath, optn_no_cache);
}

/**
 * as_client_run_check_syscompat:
 *
 * Check component against a variety of system types.
 */
static gint
as_client_run_check_syscompat (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *fname_or_cid = NULL;
	gboolean optn_sc_details = FALSE;

	const GOptionEntry check_syscompat_options[] = {
		{ "details",
		  0, 0,
		  G_OPTION_ARG_NONE, &optn_sc_details,
		  /* TRANSLATORS: ascli flag description for: --details (part of the "check-syscompat" subcommand) */
		  N_ ("Print more detailed output on why incompatibilities exist."),
		  NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (cmd, check_syscompat_options);
	g_option_context_add_main_entries (opt_context, data_catalog_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		fname_or_cid = argv[1];

	return ascli_check_syscompat (fname_or_cid, optn_cachepath, optn_no_cache, optn_sc_details);
}

/**
 * as_client_run_put:
 *
 * Place a metadata file in the right directory.
 */
static gint
as_client_run_put (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *fname = NULL;
	const gchar *optn_origin = NULL;
	gboolean optn_usermode = FALSE;
	gint ret;

	const GOptionEntry put_file_options[] = {
		{ "origin",
		  0, 0,
		  G_OPTION_ARG_STRING, &optn_origin,
		  /* TRANSLATORS: ascli flag description for: --origin (part of the "put" subcommand) */
		  N_ ("Set the data origin for the installed metadata catalog file."),
		  NULL },
		{ "user",
		  0, 0,
		  G_OPTION_ARG_NONE, &optn_usermode,
		  /* TRANSLATORS: ascli flag description for: --user (part of the "put" subcommand) */
		  N_ ("Install the file for the current user, instead of globally."),
		  NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (cmd, put_file_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		fname = argv[1];
	if (argc > 2) {
		as_client_print_help_hint (cmd->name, argv[2]);
		return 1;
	}

	return ascli_put_metainfo (fname, optn_origin, optn_usermode);
}

static const gchar *optn_bundle_type = NULL;
static gboolean optn_choose_first = FALSE;

const GOptionEntry pkgmanage_options[] = {
	{ "bundle-type",
	  0, 0,
	  G_OPTION_ARG_STRING, &optn_bundle_type,
	  /* TRANSLATORS: ascli flag description for: --bundle-type (part of the "remove" and "install" subcommands) */
	  N_ ("Limit the command to use only components from the given bundling system (`flatpak` "
	      "or `package`)."),
	  NULL },
	{ "first",
	  0, 0,
	  G_OPTION_ARG_NONE, &optn_choose_first,
	  /* TRANSLATORS: ascli flag description for: --first (part of the "remove" and "install" subcommands) */
	  N_ ("Do not ask for which software component should be used and always choose the first "
	      "entry."),
	  NULL },
	{ NULL }
};

/**
 * as_client_run_install:
 *
 * Install a component by its ID.
 */
static gint
as_client_run_install (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *value = NULL;
	AsBundleKind bundle_kind;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, pkgmanage_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		value = argv[1];
	if (argc > 2) {
		as_client_print_help_hint (cmd->name, argv[2]);
		return 1;
	}

	bundle_kind = as_bundle_kind_from_string (optn_bundle_type);
	if (optn_bundle_type != NULL && bundle_kind == AS_BUNDLE_KIND_UNKNOWN) {
		/* TRANSLATORS: ascli install currently only supports two values for --bundle-type. */
		ascli_print_stderr (_("No valid bundle kind was specified. Only `package` and `flatpak` are currently recognized."));
		return ASCLI_EXIT_CODE_BAD_INPUT;
	}

	return ascli_install_component (value, bundle_kind, optn_choose_first);
}

/**
 * as_client_run_remove:
 *
 * Uninstall a component by its ID.
 */
static gint
as_client_run_remove (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *value = NULL;
	AsBundleKind bundle_kind;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, pkgmanage_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		value = argv[1];
	if (argc > 2) {
		as_client_print_help_hint (cmd->name, argv[2]);
		return 1;
	}

	bundle_kind = as_bundle_kind_from_string (optn_bundle_type);
	if (optn_bundle_type != NULL && bundle_kind == AS_BUNDLE_KIND_UNKNOWN) {
		/* TRANSLATORS: ascli install currently only supports two values for --bundle-type. */
		ascli_print_stderr (_("No valid bundle kind was specified. Only `package` and `flatpak` are currently recognized."));
		return ASCLI_EXIT_CODE_BAD_INPUT;
	}

	return ascli_remove_component (value, bundle_kind, optn_choose_first);
}

/**
 * as_client_run_status:
 *
 * Show diagnostic information.
 */
static gint
as_client_run_status (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, NULL);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1) {
		as_client_print_help_hint (cmd->name, argv[1]);
		return 1;
	}

	return ascli_show_status ();
}

/**
 * as_client_run_sysinfo:
 *
 * Show information about the current operating system and device.
 */
static gint
as_client_run_sysinfo (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, find_options);
	g_option_context_add_main_entries (opt_context, data_catalog_options, NULL);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1) {
		as_client_print_help_hint (cmd->name, argv[1]);
		return 1;
	}

	return ascli_show_sysinfo (optn_cachepath, optn_no_cache, optn_details);
}

/**
 * as_client_run_list_reviews:
 *
 * Fetch and display user reviews for a software component.
 */
static gint
as_client_run_list_reviews (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *cpt_id = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, reviews_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		cpt_id = argv[1];
	if (argc > 2) {
		as_client_print_help_hint (cmd->name, argv[2]);
		return 1;
	}

	return ascli_list_reviews (cpt_id,
				   optn_reviews_server,
				   optn_reviews_locale,
				   optn_reviews_start > 0 ? (guint) optn_reviews_start : 0,
				   optn_reviews_limit > 0 ? (guint) optn_reviews_limit : 0);
}

/**
 * as_client_run_submit_review:
 *
 * Interactively compose and submit a review for a software component.
 */
static gint
as_client_run_submit_review (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *cpt_id = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, reviews_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		cpt_id = argv[1];
	if (argc > 2) {
		as_client_print_help_hint (cmd->name, argv[2]);
		return 1;
	}

	return ascli_submit_review (cpt_id, optn_reviews_server, optn_reviews_locale);
}

/**
 * as_client_run_convert:
 *
 * Convert metadata.
 */
static gint
as_client_run_convert (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;
	const gchar *fname1 = NULL;
	const gchar *fname2 = NULL;
	AsFormatKind mformat;

	opt_context = as_client_new_subcommand_option_context (cmd, format_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		fname1 = argv[1];
	if (argc > 2)
		fname2 = argv[2];

	mformat = as_format_kind_from_string (optn_format);
	return ascli_convert_data (fname1, fname2, mformat);
}

/**
 * as_client_run_compare_versions:
 *
 * Compare versions using AppStream's version comparison algorithm.
 */
static gint
as_client_run_compare_versions (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	gint ret;

	opt_context = as_client_new_subcommand_option_context (cmd, format_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc < 3) {
		ascli_print_stderr (_("You need to provide at least two version numbers to compare as parameters."));
		return 2;
	}

	if (argc == 3) {
		const gchar *ver1 = argv[1];
		const gchar *ver2 = argv[2];
		gint comp_res = as_vercmp_simple (ver1, ver2);

		if (comp_res == 0)
			g_print ("%s == %s\n", ver1, ver2);
		else if (comp_res > 0)
			g_print ("%s >> %s\n", ver1, ver2);
		else if (comp_res < 0)
			g_print ("%s << %s\n", ver1, ver2);

		return 0;
	} else if (argc == 4) {
		AsRelationCompare compare;
		gint rc;
		gboolean res;
		const gchar *ver1 = argv[1];
		const gchar *comp_str = argv[2];
		const gchar *ver2 = argv[3];

		compare = as_relation_compare_from_string (comp_str);
		if (compare == AS_RELATION_COMPARE_UNKNOWN) {
			guint i;
			/** TRANSLATORS: The user tried to compare version numbers, but the comparison operator (greater-then, equal, etc.) was invalid. */
			ascli_print_stderr (_("Unknown compare relation '%s'. Valid values are:"),
					      comp_str);
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

		g_print ("%s: ", res ? "true" : "false");
		if (rc == 0)
			g_print ("%s == %s\n", ver1, ver2);
		else if (rc > 0)
			g_print ("%s >> %s\n", ver1, ver2);
		else if (rc < 0)
			g_print ("%s << %s\n", ver1, ver2);

		return res ? 0 : 1;
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
static gint
as_client_run_new_template (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GString) desc_str = NULL;
	guint i;
	gint ret;
	const gchar *out_fname = NULL;
	const gchar *cpt_kind_str = NULL;
	const gchar *optn_desktop_file = NULL;

	const GOptionEntry newtemplate_options[] = {
		{ "from-desktop",
		  0, 0,
		  G_OPTION_ARG_STRING, &optn_desktop_file,
		  /* TRANSLATORS: ascli flag description for: --from-desktop (part of the new-template subcommand) */
		  N_ ("Use the given .desktop file to fill in the basic values of the metainfo "
		      "file."),
		  NULL },
		{ NULL }
	};

	desc_str = g_string_new (
	    /* TRANSLATORS: Additional help text for the 'new-template' ascli subcommand */
	    _("This command takes optional TYPE and FILE positional arguments, FILE being a file to write to (or \"-\" for standard output)."));
	g_string_append (desc_str, "\n");
	g_string_append_printf (
	    desc_str,
	    /* TRANSLATORS: Additional help text for the 'new-template' ascli subcommand, a bullet-pointed list of types follows */
	    _("The TYPE must be a valid component-type, such as: %s"), "\n");
	for (i = 1; i < AS_COMPONENT_KIND_LAST; i++)
		g_string_append_printf (desc_str, " • %s\n", as_component_kind_to_string (i));

	opt_context = as_client_new_subcommand_option_context (cmd, newtemplate_options);
	g_option_context_set_description (opt_context, desc_str->str);

	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		cpt_kind_str = argv[1];
	if (argc > 2)
		out_fname = argv[2];

	return ascli_create_metainfo_template (out_fname, cpt_kind_str, optn_desktop_file);
}

/**
 * as_client_run_make_desktop_file:
 *
 * Create desktop-entry file from metainfo file.
 */
static gint
as_client_run_make_desktop_file (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *optn_exec_command = NULL;
	const gchar *mi_fname = NULL;
	const gchar *de_fname = NULL;
	gint ret;

	const GOptionEntry make_desktop_file_options[] = {
		{ "exec",
		  0, 0,
		  G_OPTION_ARG_STRING, &optn_exec_command,
		  /* TRANSLATORS: ascli flag description for: --exec (part of the make-desktop-file subcommand) */
		  N_ ("Use the specified line for the 'Exec=' key of the desktop-entry file."),
		  NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (cmd, make_desktop_file_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		mi_fname = argv[1];
	if (argc > 2)
		de_fname = argv[2];

	return ascli_make_desktop_entry_file (mi_fname, de_fname, optn_exec_command);
}

/**
 * as_client_run_news_to_metainfo:
 *
 * Convert NEWS file to metainfo data.
 */
static gint
as_client_run_news_to_metainfo (AsCliCommand *cmd, gint argc, gchar **argv)
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
		{ "format",
		  0, 0,
		  G_OPTION_ARG_STRING, &optn_format_text,
		  /* TRANSLATORS: ascli flag description for: --format as part of the news-to-metainfo command */
		  N_ ("Assume the input file is in the selected format ('yaml', 'text' or "
		      "'markdown')."),
		  NULL },
		{ "limit",
		  'l', 0,
		  G_OPTION_ARG_INT, &optn_limit,
		  /* TRANSLATORS: ascli flag description for: --limit as part of the news-to-metainfo command */
		  N_ ("Limit the number of release entries that end up in the metainfo file (<= 0 "
		      "for unlimited)."),
		  NULL },
		{ "translatable-count",
		  't', 0,
		  G_OPTION_ARG_INT, &optn_translatable_n,
		  /* TRANSLATORS: ascli flag description for: --translatable-count as part of the news-to-metainfo command */
		  N_ ("Set the number of releases that should have descriptions marked for "
		      "translation (latest releases are translated first, -1 for unlimited)."),
		  NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (cmd, news_to_metainfo_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		news_fname = argv[1];
	if (argc > 2)
		mi_fname = argv[2];
	if (argc > 3)
		out_fname = argv[3];

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
static gint
as_client_run_metainfo_to_news (AsCliCommand *cmd, gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	const gchar *optn_format_text = NULL;
	const gchar *mi_fname = NULL;
	const gchar *news_fname = NULL;
	gint ret;

	const GOptionEntry metainfo_to_news_options[] = {
		{ "format",
		  0, 0,
		  G_OPTION_ARG_STRING, &optn_format_text,
		  /* TRANSLATORS: ascli flag description for: --format as part of the metainfo-to-news command */
		  N_ ("Generate the output in the selected format ('yaml', 'text' or 'markdown')."),
		  NULL },
		{ NULL }
	};

	opt_context = as_client_new_subcommand_option_context (cmd, metainfo_to_news_options);
	ret = as_client_option_context_parse (opt_context, cmd, &argc, &argv);
	if (ret != 0)
		return ret;

	if (argc > 1)
		mi_fname = argv[1];
	if (argc > 2)
		news_fname = argv[2];

	return ascli_metainfo_to_news (mi_fname, news_fname, optn_format_text);
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
static gint
as_client_run_compose (AsCliCommand *cmd, gint argc, gchar **argv)
{
	const gchar *ascompose_exe = LIBEXECDIR "/appstreamcli-compose";
	g_autofree const gchar **asc_argv = NULL;
#ifdef G_OS_WIN32
	gint wait_status = 0;
	g_autoptr(GError) error = NULL;
#endif
	if (!g_file_test (ascompose_exe, G_FILE_TEST_EXISTS)) {
		ascli_print_stderr (
		    /* TRANSLATORS: appstreamcli-compose was not found */
		    _("AppStream Compose binary '%s' was not found! Can not continue."),
		      ascompose_exe);
		ascli_print_stderr (
		    /* TRANSLATORS: appstreamcli-compose was not found - info text */
		    _("You may be able to install the AppStream Compose addon via: `%s`"),
		      "sudo appstreamcli install org.freedesktop.appstream.compose");
		return 4;
	}

	asc_argv = g_new0 (const gchar *, argc + 1);
	asc_argv[0] = ascompose_exe;
	for (gint i = 1; i < argc; i++)
		asc_argv[i] = argv[i];

#ifdef G_OS_WIN32
	if (!g_spawn_sync (ascompose_exe,
			   (gchar **) asc_argv,
			   NULL,
			   G_SPAWN_DEFAULT,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   &wait_status,
			   &error)) {
		/* TRANSLATORS: "Compose" is a command of appstreamcli to build metadata catalogs. */
		ascli_print_stderr (_("Compose operation failed to execute: %s"), error->message);
		return 6;
	}

#if GLIB_CHECK_VERSION(2, 70, 0)
	if (!g_spawn_check_wait_status (wait_status, &error))
#else
	if (!g_spawn_check_exit_status (wait_status, &error))
#endif
	{
		ascli_print_stderr (_("Compose failed: %s"), error->message);
		return error->code;
	}
	return 0;
#else
	return execv (ascompose_exe, (char *const *) asc_argv);
#endif
}

/**
 * as_client_get_help_summary:
 **/
static gchar *
as_client_get_help_summary (GPtrArray *commands)
{
	guint current_block_id = 0;
	gboolean compose_available = FALSE;
	g_autoptr(GArray) blocks_maxlen = NULL;
	GString *string = g_string_new ("");

	g_string_append_printf (string,
				"%s\n\n%s\n",
				/* TRANSLATORS: This is the header to the --help menu */
				_("AppStream command-line interface"),
				  /* these are commands we can use with appstreamcli */
				  _("Subcommands:"));

	compose_available = as_client_check_compose_available ();
	blocks_maxlen = g_array_new (FALSE, FALSE, sizeof (guint));
	for (guint i = 0; i < commands->len; i++) {
		guint nlen;
		guint *elen_p;
		AsCliCommand *cmd = g_ptr_array_index (commands, i);

		while (blocks_maxlen->len < (cmd->block_id + 1)) {
			guint min_len = 26;
			g_array_append_val (blocks_maxlen, min_len);
		}
		nlen = strlen (cmd->name) + strlen (cmd->arguments);
		elen_p = &g_array_index (blocks_maxlen, guint, cmd->block_id);
		if (nlen > *elen_p)
			*elen_p = nlen;
	}

	for (guint i = 0; i < commands->len; i++) {
		guint term_len;
		guint block_maxlen;
		guint synopsis_len;
		g_autofree gchar *summary_wrap = NULL;
		AsCliCommand *cmd = g_ptr_array_index (commands, i);

		/* don't display compose help if ascompose binary was not found */
		if (!compose_available && g_strcmp0 (cmd->name, "compose") == 0)
			continue;

		if (cmd->block_id != current_block_id) {
			current_block_id = cmd->block_id;
			g_string_append (string, "\n");
		}

		block_maxlen = g_array_index (blocks_maxlen, guint, cmd->block_id);
		term_len = strlen (cmd->name) + strlen (cmd->arguments);

		g_string_append_printf (string,
					"  %s %s%*s",
					cmd->name,
					cmd->arguments,
					(block_maxlen - term_len) + 1,
					"");
		synopsis_len = block_maxlen + 3 + 1;
		summary_wrap = ascli_format_long_output (cmd->summary,
							 synopsis_len + 72,
							 synopsis_len + 2);
		g_strstrip (summary_wrap);
		g_string_append_printf (string, "- %s\n", summary_wrap);
	}

	g_string_append (string, "\n");
	g_string_append (string,
			 _("You can find information about subcommand-specific options by passing \"--help\" to the subcommand."));

	return g_string_free (string, FALSE);
}

/**
 * ascli_dispatch_command:
 *
 * Run the subcommand selected by the command-line arguments.
 */
static gint
ascli_dispatch_command (GPtrArray *commands, gint argc, gchar **argv)
{
	AsCliCommand *cmd = ascli_find_command (commands, argv[1]);

	if (cmd == NULL) {
		ascli_print_stderr (
		    /* TRANSLATORS: ascli has been run with unknown command. '%s --help' is the command to receive help and should not be translated. */
		    _("Command '%s' is unknown. Run '%s --help' for a list of available commands."),
		      argv[1],
		      ASCLI_BIN_NAME);
		return 1;
	}

	/* let the subcommand see its own name as first argument, just like main() does */
	return cmd->func (cmd, argc - 1, argv + 1);
}

/**
 * as_client_register_commands:
 *
 * Register all subcommands that appstreamcli supports.
 */
static void
as_client_register_commands (GPtrArray *commands)
{
	ascli_add_cmd (commands,
		       0,
		       "search",
		       "s",
		       "TERM",
		       /* TRANSLATORS: `appstreamcli search` command description. */
		       _("Search the component database."), as_client_run_search);
	ascli_add_cmd (commands,
		       0,
		       "get",
		       NULL,
		       "COMPONENT-ID",
		       /* TRANSLATORS: `appstreamcli get` command description. */
		       _("Get information about a component by its ID."), as_client_run_get);
	ascli_add_cmd (commands,
		       0,
		       "what-provides",
		       NULL,
		       "TYPE VALUE",
		       /* TRANSLATORS: `appstreamcli what-provides` command description. */
		       _("Get components which provide the given item. Needs an item type (e.g. lib, bin, python3, …) and item value as parameter."),
			 as_client_run_what_provides);
	ascli_add_cmd (commands,
		       0,
		       "list-categories",
		       NULL,
		       "NAMES",
		       /* TRANSLATORS: `appstreamcli list-categories` command description. */
		       _("List components that are part of the specified categories."),
			 as_client_run_list_categories);

	ascli_add_cmd (commands,
		       1,
		       "dump",
		       NULL,
		       "COMPONENT-ID",
		       /* TRANSLATORS: `appstreamcli dump` command description. */
		       _("Dump raw XML metadata for a component matching the ID."),
			 as_client_run_dump);
	ascli_add_cmd (commands,
		       1,
		       "refresh-cache",
		       "refresh",
		       NULL,
		       /* TRANSLATORS: `appstreamcli refresh-cache` command description. */
		       _("Rebuild the component metadata cache."), as_client_run_refresh_cache);

	ascli_add_cmd (commands,
		       2,
		       "validate",
		       NULL,
		       "FILE",
		       /* TRANSLATORS: `appstreamcli validate` command description. */
		       _("Validate AppStream XML files for issues."), as_client_run_validate);
	ascli_add_cmd (commands,
		       2,
		       "validate-tree",
		       NULL,
		       "DIRECTORY",
		       /* TRANSLATORS: `appstreamcli validate-tree` command description. */
		       _("Validate an installed file-tree of an application for valid metadata."),
			 as_client_run_validate_tree);
	ascli_add_cmd (commands,
		       2,
		       "check-license",
		       NULL,
		       "LICENSE",
		       /* TRANSLATORS: `appstreamcli `check-license` command description. */
		       _("Check license string for validity and print details about it."),
			 as_client_run_check_license);
	ascli_add_cmd (commands,
		       2,
		       "is-satisfied",
		       NULL,
		       "FILE|COMPONENT-ID",
		       /* TRANSLATORS: `appstreamcli `check-license` command description. */
		       _("Check if requirements of a component (via its ID or MetaInfo file) are satisfied on this system."),
			 as_client_run_is_satisfied);
	ascli_add_cmd (commands,
		       2,
		       "check-syscompat",
		       NULL,
		       "FILE|COMPONENT-ID",
		       /* TRANSLATORS: `appstreamcli `check-syscompat` command description. */
		       _("Check compatibility of a component (via its ID or MetaInfo file) with common system and chassis types."),
			 as_client_run_check_syscompat);

	ascli_add_cmd (commands,
		       3,
		       "install",
		       NULL,
		       "COMPONENT-ID",
		       /* TRANSLATORS: `appstreamcli install` command description. */
		       _("Install software matching the component-ID."), as_client_run_install);
	ascli_add_cmd (commands,
		       3,
		       "remove",
		       NULL,
		       "COMPONENT-ID",
		       /* TRANSLATORS: `appstreamcli remove` command description. */
		       _("Remove software matching the component-ID."), as_client_run_remove);

	ascli_add_cmd (commands,
		       4,
		       "status",
		       NULL,
		       NULL,
		       /* TRANSLATORS: `appstreamcli status` command description. */
		       _("Display status information about available AppStream metadata."),
			 as_client_run_status);
	ascli_add_cmd (commands,
		       4,
		       "sysinfo",
		       NULL,
		       NULL,
		       /* TRANSLATORS: `appstreamcli sysinfo` command description. */
		       _("Show information about the current device and used operating system."),
			 as_client_run_sysinfo);
	ascli_add_cmd (commands,
		       4,
		       "put",
		       NULL,
		       "FILE",
		       /* TRANSLATORS: `appstreamcli put` command description. */
		       _("Install a metadata file into the right location."), as_client_run_put);
	ascli_add_cmd (
	    commands,
	    4,
	    "convert",
	    NULL,
	    "FILE FILE",
	    /* TRANSLATORS: `appstreamcli convert` command description. "Catalog XML" is a term describing a specific type of AppStream XML data. */
	    _("Convert metadata catalog XML to YAML or vice versa."), as_client_run_convert);
	ascli_add_cmd (commands,
		       4,
		       "vercmp",
		       "compare-versions",
		       "VER1 [COMP] VER2",
		       /* TRANSLATORS: `appstreamcli vercmp` command description. */
		       _("Compare two version numbers."), as_client_run_compare_versions);

	ascli_add_cmd (
	    commands,
	    5,
	    "new-template",
	    NULL,
	    "TYPE FILE",
	    /* TRANSLATORS: `appstreamcli new-template` command description. */
	    _("Create a template for a metainfo file (to be filled out by the upstream project)."),
	      as_client_run_new_template);
	ascli_add_cmd (commands,
		       5,
		       "make-desktop-file",
		       NULL,
		       "MI_FILE DESKTOP_FILE",
		       /* TRANSLATORS: `appstreamcli make-desktop-file` command description. */
		       _("Create a desktop-entry file from a metainfo file."),
			 as_client_run_make_desktop_file);
	ascli_add_cmd (commands,
		       5,
		       "news-to-metainfo",
		       NULL,
		       "NEWS_FILE MI_FILE [OUT_FILE]",
		       /* TRANSLATORS: `appstreamcli news-to-metainfo` command description. */
		       _("Convert a YAML or text NEWS file into metainfo releases."),
			 as_client_run_news_to_metainfo);
	ascli_add_cmd (commands,
		       5,
		       "metainfo-to-news",
		       NULL,
		       "MI_FILE NEWS_FILE",
		       /* TRANSLATORS: `appstreamcli metainfo-to-news` command description. */
		       _("Write NEWS text or YAML file with information from a metainfo file."),
			 as_client_run_metainfo_to_news);
	ascli_add_cmd (commands,
		       5,
		       "compose",
		       NULL,
		       NULL,
		       /* TRANSLATORS: `appstreamcli compose` command description. */
		       _("Compose AppStream metadata catalog from directory trees."),
			 as_client_run_compose);

	ascli_add_cmd (commands,
		       6,
		       "list-reviews",
		       NULL,
		       "COMPONENT-ID",
		       /* TRANSLATORS: `appstreamcli list-reviews` command description. */
		       _("List online user reviews for a software component."),
			 as_client_run_list_reviews);
	if (g_strcmp0 (g_getenv ("AS_SELF_TEST"), "1") == 0) {
		/* NOTE: We do not expose this functionality to end-users, because it creates less-useful
		 * reports, for example, we may not know the exact installed software version.
		 * This feature is however kinda nice to use for testing ODRS implementations and to
		 * debug AppStream itself, which is why it is left in, for now. */
		ascli_add_cmd (commands,
			       6,
			       "submit-review",
			       NULL,
			       "COMPONENT-ID",
			       /* TRANSLATORS: `appstreamcli submit-review` command description. */
			       _("Compose and submit an online review for a software component."),
				 as_client_run_submit_review);
	}
}

/**
 * as_client_wants_subcommand_help:
 *
 * Check whether help was requested for a subcommand, rather than for
 * appstreamcli itself. Help belongs to a subcommand if it was requested
 * after the subcommand name was given.
 */
static gboolean
as_client_wants_subcommand_help (gint argc, gchar **argv)
{
	gboolean have_command = FALSE;

	for (gint i = 1; i < argc; i++) {
		if (!g_str_has_prefix (argv[i], "-")) {
			/* subcommands are never prefixed with "-" */
			have_command = TRUE;
			continue;
		}
		if (have_command &&
		    (g_strcmp0 (argv[i], "--help") == 0 || g_strcmp0 (argv[i], "-h") == 0))
			return TRUE;
	}

	return FALSE;
}

/**
 * as_client_run:
 */
static gint
as_client_run (gint argc, gchar **argv)
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GPtrArray) commands = NULL;
	g_autoptr(AsProfile) profile = NULL;
	AsProfileTask *ptask;
	gboolean show_global_help;
	gint retval = 0;

	/* register all available subcommands */
	commands = g_ptr_array_new_with_free_func ((GDestroyNotify) ascli_command_free);
	as_client_register_commands (commands);

	opt_context = g_option_context_new ("COMMAND");
	g_option_context_add_main_entries (opt_context, ascli_global_options, NULL);

	/* we handle the unknown options later in the individual subcommands */
	g_option_context_set_ignore_unknown_options (opt_context, TRUE);

	/* a subcommand handles its own --help, we only display the global help here */
	show_global_help = !as_client_wants_subcommand_help (argc, argv);
	g_option_context_set_help_enabled (opt_context, show_global_help);
	if (show_global_help) {
		g_autofree gchar *summary = as_client_get_help_summary (commands);
		g_option_context_set_summary (opt_context, summary);
	}

	retval = as_client_option_context_parse (opt_context, NULL, &argc, &argv);
	if (retval != 0)
		return retval;

	if (optn_show_version) {
		if (g_strcmp0 (as_version_string (), PACKAGE_VERSION) == 0) {
			/* TRANSLATORS: Output if appstreamcli --version is executed. */
			ascli_print_stdout (_("AppStream version: %s"), PACKAGE_VERSION);
		} else {
			ascli_print_stdout (
			    /* TRANSLATORS: Output if appstreamcli --version is run and the CLI and libappstream versions differ. */
			    _("AppStream CLI tool version: %s\nAppStream library version: %s"),
			      PACKAGE_VERSION,
			      as_version_string ());
		}
		return 0;
	}

	if (argc < 2) {
		/* TRANSLATORS: ascli has been run without command. */
		g_printerr ("%s\n", _("You need to specify a command."));
		ascli_print_stderr (
		    _("Run '%s --help' to see a full list of available command line options."),
		      ASCLI_BIN_NAME);
		return 1;
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
#ifdef G_OS_WIN32
	if (!_isatty (fileno (stdout)))
#else
	if (!isatty (fileno (stdout)))
#endif
		ascli_set_output_colored (FALSE);

	/* don't let gvfsd start its own session bus: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=852696 */
	g_setenv ("GIO_USE_VFS", "local", TRUE);

	/* prepare profiler */
	profile = as_profile_new ();

	/* run subcommand */
	ptask = as_profile_start (profile, "%s: %s", ASCLI_BIN_NAME, argv[1]);
	retval = ascli_dispatch_command (commands, argc, argv);
	as_profile_task_free (ptask);

	/* profile */
	if (optn_enable_profiling)
		as_profile_dump (profile);

	return retval;
}

int
main (int argc, char **argv)
{
	gint code = 0;

	/* bind locale */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* run the application */
	code = as_client_run (argc, argv);

	return code;
}
