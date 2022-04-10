/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "config.h"

#include <appstream-compose.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "ascli-utils.h"

typedef enum {
	ASC_REPORT_MODE_UNKNOWN,
	ASC_REPORT_MODE_NONE,
	ASC_REPORT_MODE_ERROR_SUMMARY,
	ASC_REPORT_MODE_SHORT,
	ASC_REPORT_MODE_FULL
} AscReportMode;

void
composecli_add_report_hint (GString *report, AscHint *hint)
{
	const gchar *tag = asc_hint_get_tag (hint);
	switch (asc_hint_get_severity (hint)) {
		case AS_ISSUE_SEVERITY_ERROR:
			g_string_append (report, "E: ");
			break;
		case AS_ISSUE_SEVERITY_WARNING:
			g_string_append (report, "W: ");
			break;
		case AS_ISSUE_SEVERITY_INFO:
			g_string_append (report, "I: ");
			break;
		case AS_ISSUE_SEVERITY_PEDANTIC:
			g_string_append (report, "P: ");
			break;
		default:
			g_string_append (report, "U: ");
	}
	if (ascli_get_output_colored ()) {
		switch (asc_hint_get_severity (hint)) {
			case AS_ISSUE_SEVERITY_ERROR:
				g_string_append_printf (report, "%c[%dm%s%c[%dm", 0x1B, 31, tag, 0x1B, 0);
				break;
			case AS_ISSUE_SEVERITY_WARNING:
				g_string_append_printf (report, "%c[%dm%s%c[%dm", 0x1B, 33, tag, 0x1B, 0);
				break;
			case AS_ISSUE_SEVERITY_INFO:
				g_string_append_printf (report, "%c[%dm%s%c[%dm", 0x1B, 32, tag, 0x1B, 0);
				break;
			case AS_ISSUE_SEVERITY_PEDANTIC:
				g_string_append_printf (report, "%c[%dm%s%c[%dm", 0x1B, 37, tag, 0x1B, 0);
				break;
			default:
				g_string_append_printf (report, "%c[%dm%s%c[%dm", 0x1B, 35, tag, 0x1B, 0);
		}
	} else {
		g_string_append (report, tag);
	}
}

void
composecli_print_hints_report (GPtrArray *results, const gchar *title, AscReportMode mode)
{
	g_autoptr(GString) report = NULL;
	g_return_if_fail (results != NULL);

	if (mode == ASC_REPORT_MODE_NONE)
		return;

	report = g_string_new ("");
	for (guint i = 0; i < results->len; i++) {
		g_autofree const gchar **issue_cids = NULL;
		AscResult *result = ASC_RESULT (g_ptr_array_index (results, i));

		issue_cids = asc_result_get_component_ids_with_hints (result);
		for (guint j = 0; issue_cids[j] != NULL; j++) {
			gboolean entry_added = FALSE;
			gsize start_len = 0;
			GPtrArray *hints = asc_result_get_hints (result, issue_cids[j]);

			start_len = report->len;
			if (ascli_get_output_colored ())
				g_string_append_printf (report, "\n%c[%dm%s%c[%dm\n", 0x1B, 1, issue_cids[j], 0x1B, 0);
			else
				g_string_append_printf (report, "\n%s\n", issue_cids[j]);

			for (guint k = 0; k < hints->len; k++) {
				AscHint *hint = ASC_HINT (g_ptr_array_index (hints, k));
				if (mode == ASC_REPORT_MODE_ERROR_SUMMARY && !asc_hint_is_error (hint))
					continue;
				/* pedantic hints are usually not important enough to be displayed here */
				if (asc_hint_get_severity (hint) == AS_ISSUE_SEVERITY_PEDANTIC)
					continue;
				g_string_append (report, "  ");
				composecli_add_report_hint (report, hint);
				g_string_append_c (report, '\n');
				entry_added = TRUE;

				if (mode == ASC_REPORT_MODE_FULL) {
					g_autofree gchar *text = NULL;
					g_autofree gchar *text_md_wrap = NULL;
					g_autoptr(GString) text_md = NULL;

					text = asc_hint_format_explanation (hint);
					text_md = g_string_new (text);
					as_gstring_replace (text_md, "<code>", "`");
					as_gstring_replace (text_md, "</code>", "`");
					as_gstring_replace (text_md, "<br/>", "\n");
					as_gstring_replace (text_md, "<em>", "");
					as_gstring_replace (text_md, "</em>", "");
					as_gstring_replace (text_md, "&lt;", "<");
					as_gstring_replace (text_md, "&gt;", ">");

					text_md_wrap = ascli_format_long_output (text_md->str, 100, 5);
					g_string_append (report, text_md_wrap);
					g_string_append_c (report, '\n');
				}
			}
			if (!entry_added)
				g_string_truncate (report, start_len);
		}
	}

	/* don't print anything if we have no report */
	if (report->len < 2)
		return;

	/* trim trailing newline */
	g_string_erase (report, 0, 1);
	if (title != NULL) {
		g_string_prepend_c (report, '\n');
		g_string_prepend (report, title);
	}
	g_print ("%s", report->str);
}

int
main (int argc, char **argv)
{
	g_autoptr(GOptionContext) option_context = NULL;
	gboolean ret;
	gboolean verbose = FALSE;
	gboolean no_color = FALSE;
	gboolean show_version = FALSE;
	g_autoptr(GError) error = NULL;
	gboolean no_net = FALSE;
	AscReportMode report_mode;
	g_autofree gchar *report_mode_str = NULL;
	g_autofree gchar *origin = NULL;
	g_autofree gchar *res_root_dir = NULL;
	g_autofree gchar *mdata_dir = NULL;
	g_autofree gchar *icons_dir = NULL;
	g_autofree gchar *media_dir = NULL;
	g_autofree gchar *hints_dir = NULL;
	g_autofree gchar *media_baseurl = NULL;
	g_autofree gchar *prefix = NULL;
	g_autofree gchar *components_str = NULL;
	g_autoptr(AscCompose) compose = NULL;
	AscComposeFlags compose_flags;
	GPtrArray *results;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: ascompose flag description for: --verbose */
			_("Show extra debugging information"), NULL },
		{ "no-color", '\0', 0, G_OPTION_ARG_NONE, &no_color,
			/* TRANSLATORS: ascompose flag description for: --no-color */
			_("Don\'t show colored output."), NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &show_version,
			/* TRANSLATORS: ascompose flag description for: --version */
			_("Show the program version."), NULL },
		{ "no-net", '\0', 0, G_OPTION_ARG_NONE, &no_net,
			/* TRANSLATORS: ascompose flag description for: --no-net */
			_("Do not use the network at all, not even for URL validity checks."), NULL },
		{ "print-report", '\0', 0, G_OPTION_ARG_STRING, &report_mode_str,
			/* TRANSLATORS: ascompose flag description for: --full-report */
			_("Set mode of the issue report that is printed to the console"), "MODE" },
		{ "prefix", '\0', 0, G_OPTION_ARG_FILENAME, &prefix,
			/* TRANSLATORS: ascompose flag description for: --prefix */
			_("Override the default prefix (`/usr` by default)"), "DIR" },
		{ "result-root", '\0', 0, G_OPTION_ARG_FILENAME, &res_root_dir,
			/* TRANSLATORS: ascompose flag description for: --result-root */
			_("Set the result output directory"), "DIR" },
		{ "data-dir", '\0', 0, G_OPTION_ARG_FILENAME, &mdata_dir,
			/* TRANSLATORS: ascompose flag description for: --data-dir, `collection metadata` is an AppStream term */
			_("Override the collection metadata output directory"), "DIR" },
		{ "icons-dir", '\0', 0, G_OPTION_ARG_FILENAME, &icons_dir,
			/* TRANSLATORS: ascompose flag description for: --icons-dir */
			_("Override the icon output directory"), "DIR" },
		{ "media-dir", '\0', 0, G_OPTION_ARG_FILENAME, &media_dir,
			/* TRANSLATORS: ascompose flag description for: --media-dir */
			_("Set the media output directory (for media data to be served by a webserver)"), "DIR" },
		{ "hints-dir", '\0', 0, G_OPTION_ARG_FILENAME, &hints_dir,
			/* TRANSLATORS: ascompose flag description for: --hints-dir */
			_("Set a directory where HTML and text issue reports will be stored"), "DIR" },
		{ "origin", '\0', 0, G_OPTION_ARG_STRING, &origin,
			/* TRANSLATORS: ascompose flag description for: --origin */
			_("Set the origin name"), "NAME" },
		{ "media-baseurl", '\0', 0, G_OPTION_ARG_STRING, &media_baseurl,
			/* TRANSLATORS: ascompose flag description for: --media-baseurl */
			_("Set the URL where the exported media content will be hosted"), "NAME" },
		{ "components", '\0', 0, G_OPTION_ARG_STRING, &components_str,
			/* TRANSLATORS: ascompose flag description for: --components */
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
	ascli_set_output_colored (!no_color);

	if (show_version) {
		if (g_strcmp0 (as_version_string (), PACKAGE_VERSION) == 0) {
			/* TRANSLATORS: Output if appstreamcli --version is executed. */
			ascli_print_stdout (_("AppStream version: %s"), PACKAGE_VERSION);
		} else {
			/* TRANSLATORS: Output if appstreamcli --version is run and the CLI and libappstream versions differ. */
			ascli_print_stdout (_("AppStream CLI tool version: %s\nAppStream library version: %s"), PACKAGE_VERSION, as_version_string ());
		}
		return EXIT_SUCCESS;
	}

	/* determine report mode */
	report_mode = ASC_REPORT_MODE_UNKNOWN;
	if (report_mode_str == NULL)
		report_mode = ASC_REPORT_MODE_ERROR_SUMMARY;
	else if (g_strcmp0 (report_mode_str, "full") == 0)
		report_mode = ASC_REPORT_MODE_FULL;
	else if (g_strcmp0 (report_mode_str, "short") == 0)
		report_mode = ASC_REPORT_MODE_SHORT;
	else if (g_strcmp0 (report_mode_str, "on-error") == 0)
		report_mode = ASC_REPORT_MODE_ERROR_SUMMARY;
	else if (g_strcmp0 (report_mode_str, "none") == 0)
		report_mode = ASC_REPORT_MODE_NONE;
	if (report_mode == ASC_REPORT_MODE_UNKNOWN) {
		/* TRANSLATORS: invalid value for the --print-report CLI option */
		ascli_print_stderr (_("Invalid value for `--print-report` option: %s\n"
				      "Possible values are:\n"
				      "`on-error` - only prints a short report if the run failed (default)\n"
				      "`short` - generates an abridged report\n"
				      "`full` - a detailed report will be printed"),
				    report_mode_str);
		return EXIT_FAILURE;
	}

	/* create compose engine */
	compose = asc_compose_new ();

	/* modify flags */
	compose_flags = asc_compose_get_flags (compose);
	if (no_net)
		as_flags_remove (compose_flags, ASC_COMPOSE_FLAG_ALLOW_NET);
	asc_compose_set_flags (compose, compose_flags);

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
			/* TRANSLATORS: we don't have a destination directory for compose */
			g_printerr ("%s\n", _("No destination directory set, please provide a data output location!"));
			return EXIT_FAILURE;
		}
	}
	if (origin == NULL) {
		g_autofree gchar *tmp = NULL;
		origin = g_strdup ("example");
		/* TRANSLATORS: information message of appstream-compose */
		tmp = g_strdup_printf ("Metadata origin not set, using '%s'", origin);
		if (ascli_get_output_colored ())
			ascli_print_stderr ("%c[%dm%s%c[%dm: %s", 0x1B, 33, _("WARNING"), 0x1B, 0, tmp);
		else
			ascli_print_stderr ("%s: %s", _("WARNING"), tmp);
	}
	asc_compose_set_origin (compose, origin);

	if (mdata_dir == NULL)
		mdata_dir = g_build_filename (res_root_dir, prefix, "share/swcatalog/xml", NULL);
	asc_compose_set_data_result_dir (compose, mdata_dir);
	if (icons_dir == NULL)
		icons_dir = g_build_filename (res_root_dir, prefix, "share/swcatalog/icons", origin, NULL);
	asc_compose_set_icons_result_dir (compose, icons_dir);

	/* optional */
	asc_compose_set_hints_result_dir (compose, hints_dir);
	asc_compose_set_media_result_dir (compose, media_dir);
	asc_compose_set_media_baseurl (compose, media_baseurl);

	/* we need at least one unit to process */
	if (argc == 1) {
		g_autofree gchar *tmp = NULL;
		tmp = g_option_context_get_help (option_context, TRUE, NULL);
		g_print ("%s", tmp);
		return  EXIT_FAILURE;
	}

	/* add allowlist for components */
	if (components_str != NULL) {
		g_auto(GStrv) cid_allowlist = NULL;
		g_autofree gchar *cid_list = NULL;
		guint i;

		cid_allowlist = g_strsplit (components_str, ",", -1);
		for (i = 0; cid_allowlist[i] != NULL; i++)
			asc_compose_add_allowed_cid (compose, cid_allowlist[i]);

		cid_list = g_strjoinv (", ", cid_allowlist);
		if (i > 1)
			/* TRANSLATORS: information about as-compose allowlist */
			ascli_print_stdout (_("Only accepting components: %s"), cid_list);
		else
			/* TRANSLATORS: information about as-compose allowlist */
			ascli_print_stdout (_("Only accepting component: %s"), cid_list);
	}

	if (argc > 2)
		/* TRANSLATORS: information about as-compose units to be processed */
		g_print ("%s\n", _("Processing directories:"));
	else
		/* TRANSLATORS: information about as-compose units to be processed */
		g_print ("%s ", _("Processing directory:"));

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
		if (argc > 2)
			g_print ("  • %s\n", dir_path);
		else
			g_print ("%s\n", dir_path);
	}

	/* TRANSLATORS: information message */
	g_print ("%s\n", _("Composing metadata..."));

	results = asc_compose_run (compose, NULL, &error);
	if (results == NULL) {
		/* TRANSLATORS: error message */
		g_print ("%s: %s\n", _("Failed to compose AppStream metadata"), error->message);
		return EXIT_FAILURE;
	}

	if (asc_compose_has_errors (compose)) {
		/* TRANSLATORS: appstream-compose failed to include all data */
		g_print ("%s\n", _("Run failed, some data was ignored."));
		/* TRANSLATORS: information message of appstream-compose */
		composecli_print_hints_report (results, _("Errors were raised during this compose run:"), report_mode);
		g_print ("%s\n", _("Refer to the generated issue report data for details on the individual problems."));
		return EXIT_FAILURE;
	} else {
		composecli_print_hints_report (results, _("Overview of generated hints:"), report_mode);
		/* TRANSLATORS: information message */
		g_print ("%s\n", _("Success!"));

		return EXIT_SUCCESS;
	}
}
