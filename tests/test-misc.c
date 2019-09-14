/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2019 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-news-convert.h"

#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * test_readwrite_yaml_news:
 *
 * Read & write YAML NEWS file.
 */
static void
test_readwrite_yaml_news ()
{
	static const gchar *yaml_news_data =
		"---\n"
		"Version: 1.2\n"
		"Date: 2019-04-18\n"
		"Type: development\n"
		"Description:\n"
		"- Improved A & X\n"
		"- Fixed B\n"
		"---\n"
		"Version: 1.1\n"
		"Date: 2019-04-12\n"
		"Description: |-\n"
		"  A freeform description text.\n"
		"\n"
		"  Second paragraph. XML <> YAML\n"
		"---\n"
		"Version: 1.0\n"
		"Date: 2019-02-24\n"
		"Description:\n"
		"- Introduced feature A\n"
		"- Introduced feature B\n"
		"- Fixed X, Y and Z\n";
	g_autoptr(GPtrArray) releases = NULL;
	g_autoptr(GError) error = NULL;
	gchar *tmp;
	gboolean ret;

	/* read */
	releases = as_news_yaml_to_releases (yaml_news_data, &error);
	g_assert_no_error (error);
	g_assert_nonnull (releases);

	/* write */
	ret = as_news_releases_to_yaml (releases, &tmp);
	g_assert (ret);
	g_assert (as_test_compare_lines (tmp, yaml_news_data));
	g_free (tmp);
}

/**
 * test_readwrite_text_news:
 *
 * Read & write text NEWS file.
 */
static void
test_readwrite_text_news ()
{
	static const gchar *text_news_data =
			"Version 0.12.8\n"
			"~~~~~~~~~~~~~~\n"
			"Released: 2019-08-16\n"
			"\n"
			"Notes:\n"
			" * This release changes the output of appstreamcli\n"
			"\n"
			"Features:\n"
			" * Alpha\n"
			" * Beta\n"
			"\n"
			"Bugfixes:\n"
			" * Restore compatibility with GLib < 2.58\n"
			" * Gamma\n"
			" * Delta\n";
	static const gchar *expected_xml_releases_data =
			"  <releases>\n"
			"    <release type=\"stable\" version=\"0.12.8\" date=\"2019-08-16T00:00:00Z\">\n"
			"      <description>\n"
			"        <p>This release changes the output of appstreamcli</p>\n"
			"        <p>This release adds the following features:</p>\n"
			"        <ul>\n"
			"          <li>Alpha</li>\n"
			"          <li>Beta</li>\n"
			"        </ul>\n"
			"        <p>This release fixes the following bugs:</p>\n"
			"        <ul>\n"
			"          <li>Restore compatibility with GLib &lt; 2.58</li>\n"
			"          <li>Gamma</li>\n"
			"          <li>Delta</li>\n"
			"        </ul>\n"
			"      </description>\n"
			"    </release>\n"
			"  </releases>";

	g_autoptr(GPtrArray) releases = NULL;
	g_autoptr(GError) error = NULL;
	gchar *tmp;

	/* read */
	releases = as_news_text_to_releases (text_news_data, &error);
	g_assert_no_error (error);
	g_assert_nonnull (releases);

	/* write */
	tmp = as_releases_to_metainfo_xml_chunk (releases, &error);
	g_assert_no_error (error);
	g_assert_nonnull (tmp);
	g_assert (as_test_compare_lines (tmp, expected_xml_releases_data));
	g_free (tmp);
}

int
main (int argc, char **argv)
{
	int ret;

	if (argc == 0) {
		g_error ("No test directory specified!");
		return 1;
	}

	g_assert (argv[1] != NULL);
	datadir = g_build_filename (argv[1], "samples", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/AppStream/Misc/YAMLNews", test_readwrite_yaml_news);
	g_test_add_func ("/AppStream/Misc/TextNews", test_readwrite_text_news);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
