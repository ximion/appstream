/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include <glib.h>
#include "appstream.h"
#include "as-component-private.h"

#include "as-test-utils.h"

static gchar *datadir = NULL;

typedef struct {
	const gchar	*tag;
	const gchar	*hint;
	glong		line;
	AsIssueSeverity	severity;
} AsVResultCheck;

/**
 * _astest_validate_sample_fname:
 *
 * Validate file from sample directory with the given validator.
 */
static gboolean
_astest_validate_sample_fname (AsValidator *validator, const gchar *basename)
{
	g_autofree gchar *fname;
	g_autoptr(GFile) file = NULL;

	fname = g_build_filename (datadir, basename, NULL);
	file = g_file_new_for_path (fname);
	g_assert_true (g_file_query_exists (file, NULL));

	return as_validator_validate_file (validator, file);
}

/**
 * print_single_issue:
 **/
static gchar*
_astest_issue_info_to_string (const gchar *tag, const gchar *hint, glong line, AsIssueSeverity severity)
{
	return g_strdup_printf ("%s:%s:%li:%s",
				tag,
				hint == NULL? "" : hint,
				line,
				as_issue_severity_to_string (severity));
}

/**
 * _astest_validate_check_results:
 *
 * Ensure the specified checks pass for the given validator results.
 */
static void
_astest_check_validate_issues (GList *issues, AsVResultCheck *checks_all)
{
	g_autoptr(GHashTable) checks = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      g_free,
							      NULL);

	for (guint i = 0; checks_all[i].tag != NULL; i++)
		g_hash_table_insert (checks,
				     g_strconcat (checks_all[i].tag, ":", checks_all[i].hint, NULL),
				     &checks_all[i]);

	for (GList *l = issues; l != NULL; l = l->next) {
		g_autofree gchar *issue_idstr = NULL;
		g_autofree gchar *expected_idstr = NULL;
		g_autofree gchar *check_key = NULL;
		AsVResultCheck *check;
		AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (l->data);
		const gchar *tag = as_validator_issue_get_tag (issue);
		const gchar *hint = as_validator_issue_get_hint (issue);

		check_key = g_strconcat (tag, ":", hint == NULL? "" : hint, NULL);
		issue_idstr = _astest_issue_info_to_string (tag,
							    hint,
							    as_validator_issue_get_line (issue),
							    as_validator_issue_get_severity (issue));
		check = g_hash_table_lookup (checks, check_key);
		if (check == NULL) {
			g_error ("Encountered unexpected validation issue: %s", issue_idstr);
			g_assert_not_reached ();
		}
		expected_idstr = _astest_issue_info_to_string (check->tag,
								check->hint,
								check->line,
								check->severity);

		g_assert_cmpstr (expected_idstr, ==, issue_idstr);
		g_hash_table_remove (checks, check_key);
	}

	if (g_hash_table_size (checks) != 0) {
		g_autofree gchar *tmp = NULL;
		g_autofree gchar **strv = NULL;
		strv = (gchar**) g_hash_table_get_keys_as_array (checks, NULL);
		tmp = g_strjoinv ("; ", strv);
		g_error ("Expected validation issues were not found: %s", tmp);
		g_assert_not_reached ();
	}
}

/**
 * test_validator_manyerrors_desktopapp:
 *
 * Test desktop-application metainfo file with many issues.
 */
static void
test_validator_manyerrors_desktopapp ()
{
	gboolean ret;
	g_autoptr(GList) issues = NULL;
	g_autoptr(AsValidator) validator = as_validator_new ();

	AsVResultCheck expected_results[] =  {
		{ "metadata-license-invalid",
		  "GPL-2.0+", 8,
		  AS_ISSUE_SEVERITY_ERROR,
		},
		{ "spdx-license-unknown",
		  "weird", 9,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "summary-has-dot-suffix",
		  "Too short, ends with dot.", 12,
		  AS_ISSUE_SEVERITY_INFO,
		},
		{ "name-has-dot-suffix",
		  "A name.", 11,
		  AS_ISSUE_SEVERITY_PEDANTIC,
		},
		{ "cid-contains-hyphen",
		  "7-bad-ID", 7,
		  AS_ISSUE_SEVERITY_INFO,
		},
		{ "cid-contains-uppercase-letter",
		  "7-bad-ID", 7,
		  AS_ISSUE_SEVERITY_PEDANTIC,
		},
		{ "cid-has-number-prefix",
		  "7-bad-ID: 7-bad-ID → _7-bad-ID", 7,
		  AS_ISSUE_SEVERITY_INFO,
		},
		{ "cid-desktopapp-is-not-rdns",
		  "7-bad-ID", 7,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "description-first-para-too-short",
		  "Have some invalid markup as well as some valid one.", 15,
		  AS_ISSUE_SEVERITY_INFO,
		},
		{ "description-para-markup-invalid",
		  "b", 16,
		  AS_ISSUE_SEVERITY_ERROR,
		},
		{ "url-not-secure",
		  "http://www.example.org/insecure-url", 21,
		  AS_ISSUE_SEVERITY_INFO,
		},
		{ "web-url-expected",
		  "not a link", 20,
		  AS_ISSUE_SEVERITY_ERROR,
		},
		{ "release-type-invalid",
		  "unstable", 48,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "release-urgency-invalid",
		  "superduperhigh", 26,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "web-url-expected",
		  "not an URL", 31,
		  AS_ISSUE_SEVERITY_ERROR,
		},
		{ "artifact-invalid-platform-triplet",
		  "OS/Kernel invalid: lunix", 38,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "artifact-filename-not-basename",
		  "/root/file.dat", 44,
		  AS_ISSUE_SEVERITY_ERROR,
		},
		{ "release-issue-is-cve-but-no-cve-id",
		  "hmm...", 33,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "content-rating-missing",
		  "", -1,
		  AS_ISSUE_SEVERITY_INFO,
		},

		{ NULL, NULL, 0, AS_ISSUE_SEVERITY_UNKNOWN }
	};

	ret = _astest_validate_sample_fname (validator, "validate_many-errors-desktopapp.xml");

	issues = as_validator_get_issues (validator);
	_astest_check_validate_issues (issues,
				       (AsVResultCheck*) &expected_results);
	g_assert_false (ret);
}

/**
 * test_validator_relationissues:
 *
 * Test requires/recommends & Co.
 */
static void
test_validator_relationissues ()
{
	gboolean ret;
	g_autoptr(GList) issues = NULL;
	g_autoptr(AsValidator) validator = as_validator_new ();

	AsVResultCheck expected_results[] =  {
		{ "relation-control-value-invalid",
		  "telekinesis", 22,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "relation-item-has-vercmp",
		  "gt", 23,
		  AS_ISSUE_SEVERITY_INFO,
		},
		{ "relation-item-invalid-vercmp",
		  "gl", 24,
		  AS_ISSUE_SEVERITY_ERROR,
		},
		{ "relation-display-length-side-property-invalid",
		  "alpha", 27,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "relation-display-length-value-invalid",
		  "bleh", 25,
		  AS_ISSUE_SEVERITY_WARNING,
		},
		{ "releases-info-missing",
		  "", -1,
		  AS_ISSUE_SEVERITY_PEDANTIC,
		},

		{ NULL, NULL, 0, AS_ISSUE_SEVERITY_UNKNOWN }
	};

	ret = _astest_validate_sample_fname (validator, "validate_relationissues.xml");

	issues = as_validator_get_issues (validator);
	_astest_check_validate_issues (issues,
				       (AsVResultCheck*) &expected_results);
	g_assert_false (ret);
}

int
main (int argc, char **argv)
{
	int ret;

	if (argc == 0) {
		g_error ("No test directory specified!");
		return 1;
	}

	g_assert_nonnull (argv[1]);
	datadir = g_build_filename (argv[1], "samples", NULL);
	g_assert_true (g_file_test (datadir, G_FILE_TEST_EXISTS));

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/AppStream/Validate/DesktopAppManyErrors", test_validator_manyerrors_desktopapp);
	g_test_add_func ("/AppStream/Validate/RelationIssues", test_validator_relationissues);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
