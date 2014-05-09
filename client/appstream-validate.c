/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include <locale.h>
#include <glib/gi18n-lib.h>
#include <appstream.h>

typedef struct {
	GOptionContext	*context;
} AsValidateToolPrivate;

/**
 * importance_to_string_id:
 **/
static const gchar*
importance_to_string_id (AsIssueImportance importance)
{
	switch (importance) {
		case AS_ISSUE_IMPORTANCE_ERROR:
			return "E";
		case AS_ISSUE_IMPORTANCE_WARNING:
			return "W";
		case AS_ISSUE_IMPORTANCE_INFO:
			return "I";
		case AS_ISSUE_IMPORTANCE_PEDANTIC:
			return "P";
		default:
			return "X";
	}
}

/**
 * print_report:
 **/
static void
print_report (GPtrArray *issues)
{
	guint i;
	AsValidatorIssue *issue;
	AsIssueImportance importance;

	for (i = 0; i < issues->len; i++) {
		issue = (AsValidatorIssue*) g_ptr_array_index (issues, i);
		importance = as_validator_issue_get_importance (issue);

		g_print ("%s: %s\n",
				 importance_to_string_id (importance),
				 as_validator_issue_get_message (issue));
	}
}

/**
 * validate_file:
 **/
static gboolean
validate_file (gchar *fname)
{
	GFile *file;
	gboolean ret;
	AsValidator *validator;
	GPtrArray *issues;

	file = g_file_new_for_path (fname);
	if (!g_file_query_exists (file, NULL)) {
		g_print ("File '%s' does not exist.", fname);
		g_print ("\n");
		g_object_unref (file);
		return FALSE;
	}

	validator = as_validator_new ();
	ret = as_validator_validate_file (validator, file);
	issues = as_validator_get_issues (validator);

	print_report (issues);

	g_object_unref (file);
	g_object_unref (validator);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	AsValidateToolPrivate *priv;
	gboolean ret;
	gboolean verbose = FALSE;
	gboolean version = FALSE;
	GError *error = NULL;
	guint retval = 1;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
			/* TRANSLATORS: command line option */
			_("Show client and daemon versions"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	/* create helper object */
	priv = g_new0 (AsValidateToolPrivate, 1);

	/* get a list of the commands */
	priv->context = g_option_context_new (NULL);

	g_set_application_name (_("AppStream Validation Utility"));
	g_option_context_add_main_entries (priv->context, options, NULL);
	ret = g_option_context_parse (priv->context, &argc, &argv, &error);
	if (!ret) {
		g_print ("%s: %s\n",
			 _("Failed to parse arguments"),
			 error->message);
		g_error_free (error);
		goto out;
	}

	if (argc <= 1) {
		g_print ("%s\n",
				 _("You need to specify a file to validate!"));
		goto out;
	}

	ret = validate_file (argv[1]);
	if (!ret) {
		g_print ("%s\n",
				 _("There have been some critical errors while validating the file."));
		retval = 2;
	}

	/* success? */
	if (ret)
		retval = 0;
out:
	if (priv != NULL) {
		g_option_context_free (priv->context);
		g_free (priv);
	}
	return retval;
}
