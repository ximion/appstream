/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "ascli-actions-validate.h"

#include <config.h>
#include <locale.h>
#include <glib/gi18n-lib.h>
#include <appstream.h>

#include "ascli-utils.h"

/**
 * create_issue_info_print_string:
 **/
static gchar*
create_issue_info_print_string (AsValidatorIssue *issue, guint indent)
{
	GString *str;
	g_autofree gchar *location = NULL;
	const AsIssueSeverity severity = as_validator_issue_get_severity (issue);
	const gchar *tag = as_validator_issue_get_tag (issue);
	const gchar *cid = as_validator_issue_get_cid (issue);
	const gchar *hint = as_validator_issue_get_hint (issue);
	glong line = as_validator_issue_get_line (issue);

	if (cid == NULL) {
		if (line >= 0)
			location = g_strdup_printf ("~:%li", line);
		else
			location = g_strdup ("~:~");
	} else {
		if (line >= 0)
			location = g_strdup_printf ("%s:%li", cid, line);
		else
			location = g_strdup_printf ("%s:~", cid);
	}

	str = g_string_new ("");
	g_string_append_printf (str, "%*s", indent, "");
	switch (severity) {
		case AS_ISSUE_SEVERITY_ERROR:
			g_string_append_printf (str, "E: %s: ", location);
			break;
		case AS_ISSUE_SEVERITY_WARNING:
			g_string_append_printf (str, "W: %s: ", location);
			break;
		case AS_ISSUE_SEVERITY_INFO:
			g_string_append_printf (str, "I: %s: ", location);
			break;
		case AS_ISSUE_SEVERITY_PEDANTIC:
			g_string_append_printf (str, "P: %s: ", location);
			break;
		default:
			g_string_append_printf (str, "U: %s: ", location);
	}

	if (ascli_get_output_colored ()) {
		switch (severity) {
			case AS_ISSUE_SEVERITY_ERROR:
				g_string_append_printf (str, "%c[%dm%s%c[%dm", 0x1B, 31, tag, 0x1B, 0);
				break;
			case AS_ISSUE_SEVERITY_WARNING:
				g_string_append_printf (str, "%c[%dm%s%c[%dm", 0x1B, 33, tag, 0x1B, 0);
				break;
			case AS_ISSUE_SEVERITY_INFO:
				g_string_append_printf (str, "%c[%dm%s%c[%dm", 0x1B, 32, tag, 0x1B, 0);
				break;
			case AS_ISSUE_SEVERITY_PEDANTIC:
				g_string_append_printf (str, "%c[%dm%s%c[%dm", 0x1B, 37, tag, 0x1B, 0);
				break;
			default:
				g_string_append_printf (str, "%c[%dm%s%c[%dm", 0x1B, 35, tag, 0x1B, 0);
		}
	} else {
		g_string_append (str, tag);
	}

	if (hint != NULL) {
		if ((str->len + strlen (hint)) > 100) {
			g_autofree gchar *wrap = NULL;
			/* the hint string is too long, move it to the next line and indent it,
			 * to visually separate it from a possible explanation text */
			wrap = ascli_format_long_output (hint, 100 - (indent + 3 + 2), indent + 3 + 2);
			g_string_append_printf (str, "\n%s", wrap);
		} else {
			g_string_append_printf (str, " %s", hint);
		}
	}

	return g_string_free (str, FALSE);
}

/**
 * print_single_issue:
 **/
static gboolean
print_single_issue (AsValidatorIssue *issue, gboolean pedantic, gboolean explained, gint indent,
		    gulong *error_count, gulong *warning_count, gulong *info_count, gulong *pedantic_count)
{
	AsIssueSeverity severity;
	gboolean no_errors = TRUE;
	g_autofree gchar *title = NULL;

	/* if there are errors or warnings, we consider the validation to be failed */
	severity = as_validator_issue_get_severity (issue);
	switch (severity) {
		case AS_ISSUE_SEVERITY_ERROR:
			(*error_count)++;
			no_errors = FALSE;
			break;
		case AS_ISSUE_SEVERITY_WARNING:
			(*warning_count)++;
			no_errors = FALSE;
			break;
		case AS_ISSUE_SEVERITY_INFO:
			(*info_count)++;
			break;
		case AS_ISSUE_SEVERITY_PEDANTIC:
			(*pedantic_count)++;
			break;
		default: break;
	}

	/* skip pedantic issues if we should not show them */
	if ((!pedantic) && (severity == AS_ISSUE_SEVERITY_PEDANTIC))
		return no_errors;

	title = create_issue_info_print_string (issue, indent);
	if (explained) {
		g_autofree gchar *explanation = ascli_format_long_output (as_validator_issue_get_explanation (issue),
									  100, indent + 3);
		g_print ("%s\n%s\n\n",
			 title,
			 explanation);
	} else {
		g_print ("%s\n", title);
	}


	return no_errors;
}

/**
 * ascli_validate_file:
 **/
static gboolean
ascli_validate_file (gchar *fname, gboolean print_filename, gboolean pedantic, gboolean explain, gboolean use_net,
		     gulong *error_count, gulong *warning_count, gulong *info_count, gulong *pedantic_count)
{
	GFile *file;
	gboolean errors_found = FALSE;
	AsValidator *validator;
	GList *issues;
	GList *l;

	file = g_file_new_for_path (fname);
	if (!g_file_query_exists (file, NULL)) {
		g_print (_("File '%s' does not exist."), fname);
		g_print ("\n");
		g_object_unref (file);
		return FALSE;
	}

	validator = as_validator_new ();
	as_validator_set_check_urls (validator, use_net);

	if (!as_validator_validate_file (validator, file))
		errors_found = TRUE;
	issues = as_validator_get_issues (validator);

	if (print_filename) {
		if (ascli_get_output_colored ())
			g_print ("%c[%dm%s%c[%dm\n", 0x1B, 1, fname, 0x1B, 0);
		else
			g_print ("%s\n", fname);
	}

	for (l = issues; l != NULL; l = l->next) {
		AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (l->data);
		if (!print_single_issue (issue,
					 pedantic,
					 explain,
					 print_filename? 2 : 0,
					 error_count,
					 warning_count,
					 info_count,
					 pedantic_count))
			errors_found = TRUE;
	}

	g_list_free (issues);
	g_object_unref (file);
	g_object_unref (validator);

	return !errors_found;
}

/**
 * ascli_validate_print_stats:
 *
 * Print issue statistic to stdout.
 */
static void
ascli_validate_print_stats (gulong error_count, gulong warning_count, gulong info_count, gulong pedantic_count)
{
	gboolean add_spacer = FALSE;

	if (error_count > 0) {
		/* TRANSLATORS: Used for small issue-statistics in appstreamcli-validate, shows amount of "error"-type hints */
		g_print (_("errors: %lu"), error_count);
		add_spacer = TRUE;
	}
	if (warning_count > 0) {
		if (add_spacer)
			g_print (", ");
		/* TRANSLATORS: Used for small issue-statistics in appstreamcli-validate, shows amount of "warning"-type hints */
		g_print (_("warnings: %lu"), warning_count);
		add_spacer = TRUE;
	}
	if (info_count > 0) {
		if (add_spacer)
			g_print (", ");
		/* TRANSLATORS: Used for small issue-statistics in appstreamcli-validate, shows amount of "info"-type hints */
		g_print (_("infos: %lu"), info_count);
		add_spacer = TRUE;
	}
	if (pedantic_count > 0) {
		if (add_spacer)
			g_print (", ");
		/* TRANSLATORS: Used for small issue-statistics in appstreamcli-validate, shows amount of "pedantic"-type hints */
		g_print (_("pedantic: %lu"), pedantic_count);
		add_spacer = TRUE;
	}
}

/**
 * ascli_validate_files:
 */
gint
ascli_validate_files (gchar **argv, gint argc, gboolean pedantic, gboolean explain, gboolean use_net)
{
	gint i;
	gboolean ret = TRUE;
	gulong error_count = 0;
	gulong warning_count = 0;
	gulong info_count = 0;
	gulong pedantic_count = 0;

	if (argc < 1) {
		g_print ("%s\n", _("You need to specify at least one file to validate!"));
		return 1;
	}

	for (i = 0; i < argc; i++) {
		gboolean tmp_ret;
		tmp_ret = ascli_validate_file (argv[i],
						argc >= 2, /* print filenames if we validate multiple files */
						pedantic,
						explain,
						use_net,
						&error_count,
						&warning_count,
						&info_count,
						&pedantic_count);
		if (!tmp_ret)
			ret = FALSE;

		/* space out contents from different files a bit more if we only show tags */
		if (!explain)
			g_print("\n");
	}

	if (ret) {
		if ((error_count == 0) && (warning_count == 0) &&
		    (info_count == 0) && (pedantic_count == 0)) {
			g_print ("%s\n", _("Validation was successful."));
		} else {
			g_print (_("Validation was successful: %s"), "");
			ascli_validate_print_stats (error_count,
						    warning_count,
						    info_count,
						    pedantic_count);
			g_print ("\n");
		}

		return 0;
	} else {
		g_print (_("Validation failed: %s"), "");
		ascli_validate_print_stats (error_count,
					    warning_count,
					    info_count,
					    pedantic_count);
		g_print ("\n");

		return 3;
	}
}

/**
 * ascli_validate_files_format:
 *
 * Validate files and return result in a machine-readable format.
 */
gint
ascli_validate_files_format (gchar **argv, gint argc, const gchar *format, gboolean use_net)
{
	if (g_strcmp0 (format, "text") == 0) {
		/* "text" is pretty much the default output,
		 * only without colors, with explanations enabled and in pedantic mode */
		ascli_set_output_colored (FALSE);
		return ascli_validate_files (argv,
					     argc,
					     TRUE, /* pedantic */
					     TRUE, /* explain */
					     use_net);
	}

	if (g_strcmp0 (format, "yaml") == 0) {
		gboolean validation_passed = TRUE;
		g_autoptr(AsValidator) validator = NULL;
		g_autofree gchar *yaml_result = NULL;

		if (argc < 1) {
			g_print ("%s\n", _("You need to specify at least one file to validate!"));
			return 1;
		}

		validator = as_validator_new ();
		as_validator_set_check_urls (validator, use_net);

		for (gint i = 0; i < argc; i++) {
			g_autoptr(GFile) file = NULL;

			file = g_file_new_for_path (argv[i]);
			if (!g_file_query_exists (file, NULL)) {
				g_print (_("File '%s' does not exist."), argv[i]);
				g_print ("\n");
				return FALSE;
			}

			if (!as_validator_validate_file (validator, file))
				validation_passed = FALSE;
		}

		validation_passed = as_validator_get_report_yaml (validator, &yaml_result);
		g_print ("%s", yaml_result);
		return validation_passed? 0 : 3;
	}

	g_print (_("The validator can not create reports in the '%s' format. You may select 'yaml' or 'text' instead."), format);
	g_print ("\n");
	return 1;
}

/**
 * ascli_validate_tree:
 */
gint
ascli_validate_tree (const gchar *root_dir, gboolean pedantic, gboolean explain, gboolean use_net)
{
	gboolean no_errors = TRUE;
	AsValidator *validator;
	GHashTable *issues_files;
	GHashTableIter hiter;
	gpointer hkey, hvalue;
	gulong error_count = 0;
	gulong warning_count = 0;
	gulong info_count = 0;
	gulong pedantic_count = 0;

	if (root_dir == NULL) {
		g_print ("%s\n", _("You need to specify a root directory to start validation!"));
		return 1;
	}

	validator = as_validator_new ();
	as_validator_set_check_urls (validator, use_net);

	as_validator_validate_tree (validator, root_dir);
	issues_files = as_validator_get_issues_per_file (validator);

	g_hash_table_iter_init (&hiter, issues_files);
	while (g_hash_table_iter_next (&hiter, &hkey, &hvalue)) {
		const gchar *filename = (const gchar*) hkey;
		const GPtrArray *issues = (const GPtrArray*) hvalue;

		if (filename != NULL) {
			if (ascli_get_output_colored ())
				g_print ("%c[%dm%s%c[%dm\n", 0x1B, 1, filename, 0x1B, 0);
			else
				g_print ("%s\n", filename);
		}

		for (guint i = 0; i < issues->len; i++) {
			AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (g_ptr_array_index (issues, i));

			if (!print_single_issue (issue,
						 pedantic,
						 explain,
						 2,
						 &error_count,
						 &warning_count,
						 &info_count,
						 &pedantic_count))
			no_errors = FALSE;
		}

		/* space out contents from different files a bit more if we only show tags */
		if (!explain)
			g_print("\n");
	}
	g_object_unref (validator);

	if (no_errors) {
		if ((error_count == 0) && (warning_count == 0) &&
		    (info_count == 0) && (pedantic_count == 0)) {
			g_print ("%s\n", _("Validation was successful."));
		} else {
			g_print (_("Validation was successful: %s"), "");
			ascli_validate_print_stats (error_count,
						    warning_count,
						    info_count,
						    pedantic_count);
			g_print ("\n");
		}

		return 0;
	} else {
		g_print (_("Validation failed: %s"), "");
		ascli_validate_print_stats (error_count,
					    warning_count,
					    info_count,
					    pedantic_count);
		g_print ("\n");

		return 3;
	}

	return 0;
}

/**
 * ascli_validate_tree_format:
 *
 * Validate directory tree and return result in a machine-readable format.
 */
gint
ascli_validate_tree_format (const gchar *root_dir, const gchar *format, gboolean use_net)
{
	if (g_strcmp0 (format, "text") == 0) {
		/* "text" is pretty much the default output,
		 * only without colors, with explanations enabled and in pedantic mode */
		ascli_set_output_colored (FALSE);
		return ascli_validate_tree (root_dir,
					    TRUE, /* pedantic */
					    TRUE, /* explain */
					    use_net);
	}

	if (g_strcmp0 (format, "yaml") == 0) {
		gboolean validation_passed;
		g_autoptr(AsValidator) validator = NULL;
		g_autofree gchar *yaml_result = NULL;

		if (root_dir == NULL) {
			g_print ("%s\n", _("You need to specify a root directory to start validation!"));
			return 1;
		}

		validator = as_validator_new ();
		as_validator_set_check_urls (validator, use_net);
		as_validator_validate_tree (validator, root_dir);

		validation_passed = as_validator_get_report_yaml (validator, &yaml_result);
		g_print ("%s", yaml_result);
		return validation_passed? 0 : 3;
	}

	g_print (_("The validator can not create reports in the '%s' format. You may select 'yaml' or 'text' instead."), format);
	g_print ("\n");
	return 1;
}

/**
 * ascli_check_license:
 *
 * Test license for validity and print information about it.
 */
gint
ascli_check_license (const gchar *license)
{
	gboolean valid = TRUE;
	gboolean is_expression = FALSE;
	const gchar *type_str = NULL;
	g_autofree gchar *license_id = NULL;

	is_expression = as_is_spdx_license_expression (license);
	if (is_expression)
		license_id = g_strdup (license);
	else
		license_id = as_license_to_spdx_id (license);

	if (as_is_spdx_license_id (license_id)) {
		is_expression = FALSE;
		/* TRANSLATORS: A plain license ID */
		type_str = _("license");
	} else if (as_is_spdx_license_exception_id (license_id))
		/* TRANSLATORS: A license exception */
		type_str = _("license exception");
	else if (is_expression)
		/* TRANSLATORS: A complex license expression */
		type_str = _("license expression");

	if (type_str == NULL) {
		/* TRANSLATORS: An invalid license */
		type_str = _("invalid");
		valid = FALSE;
	}

	/* TRANSLATORS: The license string type (single license, expression, exception, invalid) */
	ascli_print_key_value (_("License Type"), type_str, FALSE);

	if (!is_expression && valid) {
		/* TRANSLATORS: Canonical identifier of a software license */
		ascli_print_key_value (_("Canonical ID"), license_id, FALSE);
	}

	/* TRANSLATORS: Whether a license is suitable for AppStream metadata */
	ascli_print_key_value (_("Suitable for AppStream metadata"), as_license_is_metadata_license (license_id)? _("yes") : _("no"), FALSE);

	/* TRANSLATORS: Whether a license considered suitable for Free and Open Source software */
	ascli_print_key_value (_("Free and Open Source"), as_license_is_free_license (license_id)? _("yes") : _("no"), FALSE);

	if (!is_expression) {
		g_autofree gchar *url = as_get_license_url (license_id);
		/* TRANSLATORS: Software license URL */
		ascli_print_key_value (_("URL"), url, FALSE);
	}

	return valid? 0 : 1;
}
