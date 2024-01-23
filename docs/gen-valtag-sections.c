/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include <appstream.h>

#include "as-validator-issue-tag.h"
#include "asc-hint-tags.h"

static GString *
load_doc_template (const gchar *dir_path, const gchar *tmpl_name)
{
	g_autofree gchar *path = NULL;
	g_autofree gchar *contents = NULL;
	g_autoptr(GError) error = NULL;

	path = g_build_filename (dir_path, tmpl_name, NULL);

	if (!g_file_get_contents (path, &contents, NULL, &error)) {
		g_error ("Failed to load template file '%s': %s", tmpl_name, error->message);
		return NULL;
	}

	return g_string_new (contents);
}

static gchar *
make_valtag_entry (const gchar *ns_prefix,
		   const gchar *tag,
		   AsIssueSeverity severity,
		   const gchar *explanation)
{
	GString *entry = NULL;
	g_autofree gchar *explanation_xml = NULL;

	const gchar *entry_tmpl =
	    "			<varlistentry id=\"{{prefix}}-{{tag}}\">\n"
	    "			<term>{{tag}}</term>\n"
	    "			<listitem>\n"
	    "				<para>Severity: <emphasis>{{severity}}</emphasis></para>\n"
	    "				<para>\n"
	    "				{{explanation}}\n"
	    "				</para>\n"
	    "			</listitem>\n"
	    "			</varlistentry>";

	entry = g_string_new (entry_tmpl);

	as_gstring_replace (entry, "{{prefix}}", ns_prefix, -1);
	as_gstring_replace (entry, "{{tag}}", tag, -1);
	as_gstring_replace (entry, "{{severity}}", as_issue_severity_to_string (severity), -1);
	explanation_xml = g_markup_escape_text (explanation, -1);
	as_gstring_replace (entry, "{{explanation}}", explanation_xml, -1);

	return g_string_free (entry, FALSE);
}

static gboolean
process_validator_tag_lists (const gchar *work_dir)
{
	g_autoptr(GString) val_contents = NULL;
	g_autoptr(GString) coval_contents = NULL;
	g_autoptr(GString) valtags = NULL;
	g_autofree gchar *val_fname = NULL;
	g_autofree gchar *coval_fname = NULL;
	g_autoptr(GError) error = NULL;

	val_contents = load_doc_template (work_dir, "validator-issues.xml.tmpl");
	coval_contents = load_doc_template (work_dir, "validator-compose-hints.xml.tmpl");

	/* process validator hint tags */
	valtags = g_string_new ("");
	for (guint i = 0; as_validator_issue_tag_list[i].tag != NULL; i++) {
		g_autofree gchar *entry_str = NULL;

		entry_str = make_valtag_entry ("asv",
					       as_validator_issue_tag_list[i].tag,
					       as_validator_issue_tag_list[i].severity,
					       as_validator_issue_tag_list[i].explanation);
		g_string_append_printf (valtags, "\n%s\n", entry_str);
	}
	as_gstring_replace (val_contents, "{{issue_list}}", valtags->str, -1);
	g_string_truncate (valtags, 0);

	/* process compose hint tags */
	for (guint i = 0; asc_hint_tag_list[i].tag != NULL; i++) {
		g_autofree gchar *entry_str = NULL;

		entry_str = make_valtag_entry ("asc",
					       asc_hint_tag_list[i].tag,
					       asc_hint_tag_list[i].severity,
					       asc_hint_tag_list[i].explanation);
		g_string_append_printf (valtags, "\n%s\n", entry_str);
	}
	as_gstring_replace (coval_contents, "{{hints_list}}", valtags->str, -1);

	/* save result */
	val_fname = g_build_filename (work_dir, "xml", "validator-issues.xml", NULL);
	coval_fname = g_build_filename (work_dir, "xml", "validator-compose-hints.xml", NULL);

	if (!g_file_set_contents (val_fname, val_contents->str, -1, &error)) {
		g_error ("Failed to save generated documentation: %s", error->message);
		return FALSE;
	}

	if (!g_file_set_contents (coval_fname, coval_contents->str, -1, &error)) {
		g_error ("Failed to save generated documentation: %s", error->message);
		return FALSE;
	}

	return TRUE;
}

int
main (int argc, char **argv)
{
	g_autoptr(GOptionContext) option_context = NULL;
	gboolean ret;
	gboolean verbose = FALSE;
	g_autoptr(GError) error = NULL;

	const GOptionEntry options[] = {
		{ "verbose",
		  'v', 0,
		  G_OPTION_ARG_NONE, &verbose,
		  "Show extra debugging information", NULL },
		{ NULL }
	};

	option_context = g_option_context_new (" - DOCDIR");

	g_option_context_add_main_entries (option_context, options, NULL);
	ret = g_option_context_parse (option_context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: Error message of appstream-compose */
		g_print ("%s: %s\n", "Failed to parse arguments", error->message);
		return EXIT_FAILURE;
	}

	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	if (argc <= 1) {
		g_autofree gchar *tmp = NULL;
		tmp = g_option_context_get_help (option_context, TRUE, NULL);
		g_print ("%s", tmp);
		return EXIT_FAILURE;
	}

	process_validator_tag_lists (argv[1]);

	return EXIT_SUCCESS;
}
