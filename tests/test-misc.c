/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <locale.h>

#include "appstream.h"
#include "as-news-convert.h"
#include "as-utils-private.h"
#include "as-system-info-private.h"
#include "as-reviews-client-private.h"

#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * asx_load_sample_metainfo:
 *
 * Helper to fetch a component from a file in the samples directory.
 */
static AsComponent *
asx_load_sample_metainfo (const gchar *subpath)
{
	g_autofree gchar *path = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsMetadata) mdata = as_metadata_new ();

	path = g_build_filename (datadir, subpath, NULL);
	file = g_file_new_for_path (path);

	as_metadata_parse_file (mdata, file, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);

	return g_object_ref (as_metadata_get_component (mdata));
}

/**
 * test_readwrite_yaml_news:
 *
 * Read & write YAML NEWS file.
 */
static void
test_readwrite_yaml_news (void)
{
	static const gchar *yaml_news_data = "---\n"
					     "Version: \"1.2\"\n"
					     "Date: 2019-04-18\n"
					     "Type: development\n"
					     "Description:\n"
					     "- Improved A & X\n"
					     "- Fixed B\n"
					     "Resolved:\n"
					     "- name: bz#12345\n"
					     "  url: https://example.com/bugzilla/12345\n"
					     "- cve: CVE-2019-123456\n"
					     "- gcve: GCVE-1-2025-0001\n"
					     "---\n"
					     "Version: \"1.1\"\n"
					     "Date: 2019-04-12\n"
					     "Description: |-\n"
					     "  A freeform description text.\n"
					     "  \n"
					     "  Second paragraph. XML <> YAML\n"
					     "   * List item 1\n"
					     "   * List item 2\n"
					     "     Line two of list item.\n"
					     "  \n"
					     "  Third paragraph.\n"
					     "---\n"
					     "Version: \"1.0\"\n"
					     "Date: 2019-02-24\n"
					     "Description:\n"
					     "- Introduced feature A\n"
					     "- Introduced feature B\n"
					     "- Fixed X, Y and Z\n";

	static const gchar *expected_xml_releases_data =
	    "  <releases>\n"
	    "    <release type=\"development\" version=\"1.2\" date=\"2019-04-18T00:00:00Z\">\n"
	    "      <description>\n"
	    "        <ul>\n"
	    "          <li>Improved A &amp; X</li>\n"
	    "          <li>Fixed B</li>\n"
	    "        </ul>\n"
	    "      </description>\n"
	    "      <issues>\n"
	    "        <issue url=\"https://example.com/bugzilla/12345\">bz#12345</issue>\n"
	    "        <issue type=\"cve\">CVE-2019-123456</issue>\n"
	    "        <issue type=\"gcve\">GCVE-1-2025-0001</issue>\n"
	    "      </issues>\n"
	    "    </release>\n"
	    "    <release type=\"stable\" version=\"1.1\" date=\"2019-04-12T00:00:00Z\">\n"
	    "      <description>\n"
	    "        <p>A freeform description text.</p>\n"
	    "        <p>Second paragraph. XML &lt;&gt; YAML</p>\n"
	    "        <ul>\n"
	    "          <li>List item 1</li>\n"
	    "          <li>List item 2 Line two of list item.</li>\n"
	    "        </ul>\n"
	    "        <p>Third paragraph.</p>\n"
	    "      </description>\n"
	    "    </release>\n"
	    "    <release type=\"stable\" version=\"1.0\" date=\"2019-02-24T00:00:00Z\">\n"
	    "      <description>\n"
	    "        <ul>\n"
	    "          <li>Introduced feature A</li>\n"
	    "          <li>Introduced feature B</li>\n"
	    "          <li>Fixed X, Y and Z</li>\n"
	    "        </ul>\n"
	    "      </description>\n"
	    "    </release>\n"
	    "  </releases>";

	static const gchar *expected_xml_releases_data_notranslate =
	    "  <releases>\n"
	    "    <release type=\"development\" version=\"1.2\" date=\"2019-04-18T00:00:00Z\">\n"
	    "      <description>\n"
	    "        <ul>\n"
	    "          <li>Improved A &amp; X</li>\n"
	    "          <li>Fixed B</li>\n"
	    "        </ul>\n"
	    "      </description>\n"
	    "      <issues>\n"
	    "        <issue url=\"https://example.com/bugzilla/12345\">bz#12345</issue>\n"
	    "        <issue type=\"cve\">CVE-2019-123456</issue>\n"
	    "        <issue type=\"gcve\">GCVE-1-2025-0001</issue>\n"
	    "      </issues>\n"
	    "    </release>\n"
	    "    <release type=\"stable\" version=\"1.1\" date=\"2019-04-12T00:00:00Z\">\n"
	    "      <description translate=\"no\">\n"
	    "        <p>A freeform description text.</p>\n"
	    "        <p>Second paragraph. XML &lt;&gt; YAML</p>\n"
	    "        <ul>\n"
	    "          <li>List item 1</li>\n"
	    "          <li>List item 2 Line two of list item.</li>\n"
	    "        </ul>\n"
	    "        <p>Third paragraph.</p>\n"
	    "      </description>\n"
	    "    </release>\n"
	    "    <release type=\"stable\" version=\"1.0\" date=\"2019-02-24T00:00:00Z\">\n"
	    "      <description translate=\"no\">\n"
	    "        <ul>\n"
	    "          <li>Introduced feature A</li>\n"
	    "          <li>Introduced feature B</li>\n"
	    "          <li>Fixed X, Y and Z</li>\n"
	    "        </ul>\n"
	    "      </description>\n"
	    "    </release>\n"
	    "  </releases>";

	g_autoptr(GPtrArray) releases = NULL;
	g_autoptr(GError) error = NULL;
	gchar *tmp;
	gboolean ret;
	g_autofree gchar *yaml_news_data_brclean = as_str_replace (yaml_news_data,
								   "item 2\n     Line two",
								   "item 2 Line two",
								   -1);

	/* read */
	releases = as_news_to_releases_from_data (yaml_news_data,
						  AS_NEWS_FORMAT_KIND_YAML,
						  -1,
						  -1,
						  &error);
	g_assert_no_error (error);
	g_assert_nonnull (releases);

	/* verify */
	tmp = as_releases_to_metainfo_xml_chunk (releases, &error);
	g_assert_no_error (error);
	g_assert_nonnull (tmp);
	g_assert_true (as_test_compare_lines (tmp, expected_xml_releases_data));
	g_free (tmp);

	/* write */
	ret = as_releases_to_news_data (releases, AS_NEWS_FORMAT_KIND_YAML, &tmp, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_true (as_test_compare_lines (tmp, yaml_news_data_brclean));
	g_free (tmp);

	/* read for translatable test */
	g_ptr_array_unref (releases);
	releases = as_news_to_releases_from_data (yaml_news_data,
						  AS_NEWS_FORMAT_KIND_YAML,
						  -1,
						  1,
						  &error);
	g_assert_no_error (error);
	g_assert_nonnull (releases);

	/* verify for non-translatable */
	tmp = as_releases_to_metainfo_xml_chunk (releases, &error);
	g_assert_no_error (error);
	g_assert_nonnull (tmp);
	g_assert_true (as_test_compare_lines (tmp, expected_xml_releases_data_notranslate));
	g_free (tmp);
}

/**
 * test_readwrite_text_news:
 *
 * Read & write text NEWS file.
 */
static void
test_readwrite_text_news (void)
{
	static const gchar *text_news_data =
	    "Version 0.12.8\n"
	    "--------------\n"
	    "Released: 2019-08-16\n"
	    "\n"
	    "Notes:\n"
	    " * This release changes the output of appstreamcli\n"
	    " * It is very important that\n"
	    "   multiple lines are supported in notes, which we now do!\n"
	    "\n"
	    " * Enjoy using the new release!\n"
	    "\n"
	    "Features:\n"
	    " * Alpha\n"
	    "\n"
	    " * Beta\n"
	    " * Even in bullet points you may\n"
	    "   use multiple lines.\n"
	    " * Issues may be referenced with Markdown "
	    "[#12345](https://example.com/bugzilla/12345)\n"
	    " * CVE references like [CVE-2026-41651](https://example.com/cve/CVE-2026-41651) are "
	    "auto-detected\n"
	    " * Bare GCVE references like GCVE-1-2026-1234 are detected as well\n"
	    " * Lowercase cve-2026-9999 references are normalized\n"
	    "\n"
	    "Bugfixes:\n"
	    " * Restore compatibility with GLib < 2.58\n"
	    "  * Gamma\n"
	    " * Delta\n"
	    "\n"
	    "Version 0.12.7\n"
	    "~~~~~~~~~~~~~~\n"
	    "Released: 2019-07-06\n"
	    "\n"
	    "Notes:\n"
	    " * A maintenance release.\n"
	    "\n"
	    "Bugfixes:\n"
	    " * Fixed a crash, see [issue#42](https://example.com/issues/42)\n"
	    " * Consult the [manual](https://example.com/manual) for details\n"
	    " * Hardened against CVE-2020-0001, also reported as "
	    "[CVE-2020-0001](https://www.cve.org/CVERecord?id=CVE-2020-0001)\n"
	    "\n"
	    "Resolved Issues:\n"
	    " * CVE-2020-0001\n"
	    " * #99: https://example.com/bugzilla/99\n"
	    " * General cleanup, no specific issue\n"
	    "\n"
	    "Version 0.12.6\n"
	    "~~~~~~~~~~~~~~\n"
	    "Released: 2019-06-01\n"
	    "\n"
	    "This is a freeform intro paragraph describing the release.\n"
	    "\n"
	    "It continues with a second paragraph after a blank line.\n";
	static const gchar *expected_xml_releases_data =
	    "  <releases>\n"
	    "    <release type=\"stable\" version=\"0.12.8\" date=\"2019-08-16T00:00:00Z\">\n"
	    "      <description>\n"
	    "        <p>This release changes the output of appstreamcli</p>\n"
	    "        <p>It is very important that multiple lines are supported in notes, which we "
	    "now "
	    "do!</p>\n"
	    "        <p>Enjoy using the new release!</p>\n"
	    "        <p>This release adds the following features:</p>\n"
	    "        <ul>\n"
	    "          <li>Alpha</li>\n"
	    "          <li>Beta</li>\n"
	    "          <li>Even in bullet points you may use multiple lines.</li>\n"
	    "          <li>Issues may be referenced with Markdown #12345</li>\n"
	    "          <li>CVE references like CVE-2026-41651 are auto-detected</li>\n"
	    "          <li>Bare GCVE references like GCVE-1-2026-1234 are detected as well</li>\n"
	    "          <li>Lowercase cve-2026-9999 references are normalized</li>\n"
	    "        </ul>\n"
	    "        <p>This release fixes the following bugs:</p>\n"
	    "        <ul>\n"
	    "          <li>Restore compatibility with GLib &lt; 2.58</li>\n"
	    "          <li>Gamma</li>\n"
	    "          <li>Delta</li>\n"
	    "        </ul>\n"
	    "      </description>\n"
	    "      <issues>\n"
	    "        <issue url=\"https://example.com/bugzilla/12345\">#12345</issue>\n"
	    "        <issue type=\"cve\" "
	    "url=\"https://example.com/cve/CVE-2026-41651\">CVE-2026-41651</issue>\n"
	    "        <issue type=\"gcve\">GCVE-1-2026-1234</issue>\n"
	    "        <issue type=\"cve\">CVE-2026-9999</issue>\n"
	    "      </issues>\n"
	    "    </release>\n"
	    "    <release type=\"stable\" version=\"0.12.7\" date=\"2019-07-06T00:00:00Z\">\n"
	    "      <description>\n"
	    "        <p>A maintenance release.</p>\n"
	    "        <p>This release fixes the following bugs:</p>\n"
	    "        <ul>\n"
	    "          <li>Fixed a crash, see issue#42</li>\n"
	    "          <li>Consult the manual for details</li>\n"
	    "          <li>Hardened against CVE-2020-0001, also reported as CVE-2020-0001</li>\n"
	    "        </ul>\n"
	    "        <ul>\n"
	    "          <li>General cleanup, no specific issue</li>\n"
	    "        </ul>\n"
	    "      </description>\n"
	    "      <issues>\n"
	    "        <issue url=\"https://example.com/issues/42\">issue#42</issue>\n"
	    "        <issue type=\"cve\">CVE-2020-0001</issue>\n"
	    "        <issue url=\"https://example.com/bugzilla/99\">#99</issue>\n"
	    "      </issues>\n"
	    "    </release>\n"
	    "    <release type=\"stable\" version=\"0.12.6\" date=\"2019-06-01T00:00:00Z\">\n"
	    "      <description>\n"
	    "        <p>This is a freeform intro paragraph describing the release.</p>\n"
	    "        <p>It continues with a second paragraph after a blank line.</p>\n"
	    "      </description>\n"
	    "    </release>\n"
	    "  </releases>";

	static const gchar *expected_generated_news_txt =
	    "Version 0.12.8\n"
	    "~~~~~~~~~~~~~~\n"
	    "Released: 2019-08-16\n"
	    "\n"
	    "This release changes the output of appstreamcli\n"
	    "\n"
	    "It is very important that multiple lines are supported in notes, which we now do!\n"
	    "\n"
	    "Enjoy using the new release!\n"
	    "\n"
	    "This release adds the following features:\n"
	    " * Alpha\n"
	    " * Beta\n"
	    " * Even in bullet points you may use multiple lines.\n"
	    " * Issues may be referenced with Markdown #12345\n"
	    " * CVE references like CVE-2026-41651 are auto-detected\n"
	    " * Bare GCVE references like GCVE-1-2026-1234 are detected as well\n"
	    " * Lowercase cve-2026-9999 references are normalized\n"
	    "\n"
	    "This release fixes the following bugs:\n"
	    " * Restore compatibility with GLib < 2.58\n"
	    " * Gamma\n"
	    " * Delta\n"
	    "\n"
	    "Resolved Issues:\n"
	    " * #12345: https://example.com/bugzilla/12345\n"
	    " * CVE-2026-41651: https://example.com/cve/CVE-2026-41651\n"
	    " * GCVE-1-2026-1234: https://db.gcve.eu/vuln/GCVE-1-2026-1234\n"
	    " * CVE-2026-9999: https://www.cve.org/CVERecord?id=CVE-2026-9999\n"
	    "\n"
	    "Version 0.12.7\n"
	    "~~~~~~~~~~~~~~\n"
	    "Released: 2019-07-06\n"
	    "\n"
	    "A maintenance release.\n"
	    "\n"
	    "This release fixes the following bugs:\n"
	    " * Fixed a crash, see issue#42\n"
	    " * Consult the manual for details\n"
	    " * Hardened against CVE-2020-0001, also reported as CVE-2020-0001\n"
	    " * General cleanup, no specific issue\n"
	    "\n"
	    "Resolved Issues:\n"
	    " * issue#42: https://example.com/issues/42\n"
	    " * CVE-2020-0001: https://www.cve.org/CVERecord?id=CVE-2020-0001\n"
	    " * #99: https://example.com/bugzilla/99\n"
	    "\n"
	    "Version 0.12.6\n"
	    "~~~~~~~~~~~~~~\n"
	    "Released: 2019-06-01\n"
	    "\n"
	    "This is a freeform intro paragraph describing the release.\n"
	    "\n"
	    "It continues with a second paragraph after a blank line.\n";

	g_autoptr(GPtrArray) releases = NULL;
	g_autoptr(GError) error = NULL;
	gchar *tmp;

	/* read */
	releases = as_news_to_releases_from_data (text_news_data,
						  AS_NEWS_FORMAT_KIND_TEXT,
						  -1,
						  -1,
						  &error);
	g_assert_no_error (error);
	g_assert_nonnull (releases);

	/* verify */
	tmp = as_releases_to_metainfo_xml_chunk (releases, &error);
	g_assert_no_error (error);
	g_assert_nonnull (tmp);
	g_assert_true (as_test_compare_lines (tmp, expected_xml_releases_data));
	g_free (tmp);

	/* write */
	as_releases_to_news_data (releases, AS_NEWS_FORMAT_KIND_TEXT, &tmp, &error);
	g_assert_no_error (error);
	g_assert_true (as_test_compare_lines (tmp, expected_generated_news_txt));
	g_free (tmp);
}

/**
 * test_locale_strip_encoding:
 */
static void
test_locale_strip_encoding (void)
{
	g_autofree gchar *c = NULL;
	g_autofree gchar *cutf8 = NULL;
	g_autofree gchar *cutf8valencia = NULL;

	c = as_locale_strip_encoding ("C");
	cutf8 = as_locale_strip_encoding ("C.UTF-8");
	cutf8valencia = as_locale_strip_encoding ("C.UTF-8@valencia");

	g_assert_cmpstr (c, ==, "C");
	g_assert_cmpstr (cutf8, ==, "C");
	g_assert_cmpstr (cutf8valencia, ==, "C@valencia");
}

/**
 * test_relation_satisfy_check:
 */
static void
test_relation_satisfy_check (void)
{
	g_autoptr(AsSystemInfo) sysinfo = NULL;
	g_autoptr(AsRelation) relation = NULL;
	g_autoptr(AsRelationCheckResult) rcr = NULL;
	g_autofree gchar *osrelease_fname = NULL;
	g_autoptr(GError) error = NULL;

	sysinfo = as_system_info_new ();
	relation = as_relation_new ();

	osrelease_fname = g_build_filename (datadir, "os-release-1", NULL);
	as_system_info_load_os_release (sysinfo, osrelease_fname);
	as_system_info_set_kernel (sysinfo, "Linux", "6.2.0-1");
	as_system_info_set_memory_total (sysinfo, 4096);
	as_system_info_set_display_length (sysinfo, AS_DISPLAY_SIDE_KIND_LONGEST, 3840);
	as_system_info_set_display_length (sysinfo, AS_DISPLAY_SIDE_KIND_SHORTEST, 2160);

	/* test memory */
	as_relation_set_kind (relation, AS_RELATION_KIND_RECOMMENDS);
	as_relation_set_item_kind (relation, AS_RELATION_ITEM_KIND_MEMORY);
	as_relation_set_value_int (relation, 2500);

	rcr = as_relation_is_satisfied (relation, sysinfo, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (as_relation_check_result_get_status (rcr),
			 ==,
			 AS_RELATION_STATUS_SATISFIED);
	g_clear_pointer (&rcr, g_object_unref);

	as_relation_set_value_int (relation, 8000);
	rcr = as_relation_is_satisfied (relation, sysinfo, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (as_relation_check_result_get_status (rcr),
			 ==,
			 AS_RELATION_STATUS_NOT_SATISFIED);
	g_clear_pointer (&rcr, g_object_unref);

#ifndef G_OS_WIN32
	/* test kernel */
	as_relation_set_kind (relation, AS_RELATION_KIND_REQUIRES);
	as_relation_set_item_kind (relation, AS_RELATION_ITEM_KIND_KERNEL);
	as_relation_set_value_str (relation, "Linux");
	as_relation_set_version (relation, "6.2");
	as_relation_set_compare (relation, AS_RELATION_COMPARE_GE);

	rcr = as_relation_is_satisfied (relation, sysinfo, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (as_relation_check_result_get_status (rcr),
			 ==,
			 AS_RELATION_STATUS_SATISFIED);
	g_clear_pointer (&rcr, g_object_unref);

	as_relation_set_value_str (relation, "FreeBSD");
	rcr = as_relation_is_satisfied (relation, sysinfo, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (as_relation_check_result_get_status (rcr),
			 ==,
			 AS_RELATION_STATUS_NOT_SATISFIED);
	g_clear_pointer (&rcr, g_object_unref);

	as_relation_set_value_str (relation, "Linux");
	as_relation_set_compare (relation, AS_RELATION_COMPARE_LT);
	rcr = as_relation_is_satisfied (relation, sysinfo, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (as_relation_check_result_get_status (rcr),
			 ==,
			 AS_RELATION_STATUS_NOT_SATISFIED);
	g_clear_pointer (&rcr, g_object_unref);
#endif

	/* test display length */
	as_relation_set_kind (relation, AS_RELATION_KIND_RECOMMENDS);
	as_relation_set_item_kind (relation, AS_RELATION_ITEM_KIND_DISPLAY_LENGTH);
	as_relation_set_display_side_kind (relation, AS_DISPLAY_SIDE_KIND_LONGEST);
	as_relation_set_compare (relation, AS_RELATION_COMPARE_LE);
	as_relation_set_value_int (relation, 640);

	rcr = as_relation_is_satisfied (relation, sysinfo, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (as_relation_check_result_get_status (rcr),
			 ==,
			 AS_RELATION_STATUS_NOT_SATISFIED);
	g_clear_pointer (&rcr, g_object_unref);

	as_relation_set_compare (relation, AS_RELATION_COMPARE_GE);
	rcr = as_relation_is_satisfied (relation, sysinfo, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (as_relation_check_result_get_status (rcr),
			 ==,
			 AS_RELATION_STATUS_SATISFIED);
	g_clear_pointer (&rcr, g_object_unref);
}

static gint
asx_cpt_get_syscompat_score (AsComponent *cpt, AsSystemInfo *sysinfo)
{
	g_autoptr(GPtrArray) rc_results = NULL;
	gint score;

	score = as_component_get_system_compatibility_score (cpt, sysinfo, TRUE, &rc_results);
	return score;
}

/**
 * test_syscompat_scores:
 */
static void
test_syscompat_scores (void)
{
	g_autoptr(AsComponent) cpt_desktop_im = NULL;
	g_autoptr(AsComponent) cpt_desktop_ex = NULL;
	g_autoptr(AsComponent) cpt_multi = NULL;
	g_autoptr(AsComponent) cpt_phone = NULL;
	g_autoptr(AsSystemInfo) sysinfo = NULL;

	cpt_desktop_im = asx_load_sample_metainfo (
	    "syscompat/org.example.desktopapp_implicit.metainfo.xml");
	g_assert_cmpstr (as_component_get_name (cpt_desktop_im), ==, "DesktopApp (implicit)");

	cpt_desktop_ex = asx_load_sample_metainfo (
	    "syscompat/org.example.desktopapp_explicit.metainfo.xml");
	g_assert_cmpstr (as_component_get_name (cpt_desktop_ex), ==, "DesktopApp (explicit)");

	cpt_multi = asx_load_sample_metainfo ("syscompat/org.example.multiapp.metainfo.xml");
	g_assert_cmpstr (as_component_get_name (cpt_multi), ==, "MultiApp");

	cpt_phone = asx_load_sample_metainfo ("syscompat/org.example.phoneapp.metainfo.xml");
	g_assert_cmpstr (as_component_get_name (cpt_phone), ==, "PhoneApp");

	/* test compatibility with desktop systems */
	sysinfo = as_system_info_new_template_for_chassis (AS_CHASSIS_KIND_DESKTOP, NULL);
	g_assert_nonnull (sysinfo);

	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_desktop_im, sysinfo), ==, 100);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_desktop_ex, sysinfo), ==, 100);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_multi, sysinfo), ==, 100);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_phone, sysinfo), ==, 30);

	/* test compatibility with tablet systems */
	g_clear_pointer (&sysinfo, g_object_unref);
	sysinfo = as_system_info_new_template_for_chassis (AS_CHASSIS_KIND_TABLET, NULL);
	g_assert_nonnull (sysinfo);

	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_desktop_im, sysinfo), ==, 0);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_desktop_ex, sysinfo), ==, 0);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_multi, sysinfo), ==, 100);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_phone, sysinfo), ==, 100);

	/* test compatibility with handset systems */
	g_clear_pointer (&sysinfo, g_object_unref);
	sysinfo = as_system_info_new_template_for_chassis (AS_CHASSIS_KIND_HANDSET, NULL);
	g_assert_nonnull (sysinfo);

	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_desktop_im, sysinfo), ==, 0);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_desktop_ex, sysinfo), ==, 0);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_multi, sysinfo), ==, 100);
	g_assert_cmpint (asx_cpt_get_syscompat_score (cpt_phone, sysinfo), ==, 100);
}

/**
 * test_syscompat_score_error_status:
 */
static void
test_syscompat_score_error_status (void)
{
	g_autoptr(GPtrArray) rc_results = g_ptr_array_new_with_free_func (g_object_unref);
	g_autoptr(AsRelation) relation = as_relation_new ();
	AsRelationCheckResult *rcr = NULL;

	/* a required relation whose check failed with an error (e.g. an unknown display
	 * size on a real system) must not zero the score, but instead receive the same
	 * penalty as a relation with unknown status */
	as_relation_set_kind (relation, AS_RELATION_KIND_REQUIRES);
	as_relation_set_item_kind (relation, AS_RELATION_ITEM_KIND_DISPLAY_LENGTH);
	as_relation_set_value_px (relation, 768);

	rcr = as_relation_check_result_new ();
	as_relation_check_result_set_relation (rcr, relation);
	as_relation_check_result_set_status (rcr, AS_RELATION_STATUS_ERROR);
	g_ptr_array_add (rc_results, rcr);

	g_assert_cmpint (as_relation_check_results_get_compatibility_score (rc_results), ==, 70);
}

/**
 * test_reviews_client_parse_review:
 *
 * Test parsing of an ODRS-style reviews JSON document.
 */
static void
test_reviews_client_parse_review (void)
{
	g_autoptr(AsReviewsClient) rrc = as_reviews_client_new ();
	g_autoptr(GPtrArray) reviews = NULL;
	g_autoptr(GError) error = NULL;
	AsReview *review;
	GDateTime *date;

	const gchar *json_data =
	    "[{\"score\": 0, \"app_id\": \"org.example.App\","
	    "  \"user_hash\": \"0123456789abcdef\", \"user_skey\": \"secret-key\"},"
	    " {\"review_id\": 100, \"date_created\": 1622130000.0, \"app_id\": \"org.example.App\","
	    "  \"user_hash\": \"0123456789abcdef\", \"user_display\": \" Jane Example \","
	    "  \"user_skey\": \"secret-key\", \"summary\": \"Amazing!\","
	    "  \"description\": \"Would use again.\", \"version\": \"1.2.0\", \"locale\": "
	    "\"en_US.UTF-8\","
	    "  \"distro\": \"Fedora Linux\", \"reported\": 0,"
	    "  \"rating\": 80, \"score\": 12, \"karma_up\": 1, \"karma_down\": 0, \"vote_id\": 1},"
	    " {\"review_id\": 101, \"date_created\": 1622130001.0, \"app_id\": \"org.example.App\","
	    "  \"user_hash\": \"fedcba9876543210\", \"user_display\": \"John Doe\","
	    "  \"summary\": \"Not so great\", \"description\": \"Could be better.\","
	    "  \"rating\": 40, \"karma_up\": 10, \"karma_down\": 2},"
	    " {\"review_id\": 102, \"app_id\": \"org.example.App\","
	    "  \"user_hash\": \"fedcba9876543210\", \"summary\": \"Duplicate user\", \"rating\": "
	    "20}]";

	as_reviews_client_set_user_hash (rrc, "0123456789abcdef");

	reviews = as_reviews_client_parse_reviews (rrc, json_data, -1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (reviews);

	/* the fake skey-item was skipped, the duplicate user review was ignored */
	g_assert_cmpint (reviews->len, ==, 2);

	review = AS_REVIEW (g_ptr_array_index (reviews, 0));
	g_assert_cmpstr (as_review_get_id (review), ==, "100");
	g_assert_cmpstr (as_review_get_reviewer_id (review), ==, "0123456789abcdef");
	g_assert_cmpstr (as_review_get_reviewer_name (review), ==, "Jane Example");
	g_assert_cmpstr (as_review_get_summary (review), ==, "Amazing!");
	g_assert_cmpstr (as_review_get_description (review), ==, "Would use again.");
	g_assert_cmpstr (as_review_get_version (review), ==, "1.2.0");
	g_assert_cmpstr (as_review_get_locale (review), ==, "en_US");
	g_assert_cmpint (as_review_get_rating (review), ==, 80);
	/* the server-provided score must win over the karma-derived value */
	g_assert_cmpint (as_review_get_priority (review), ==, 12);
	g_assert_cmpstr (as_review_get_metadata_item (review, "ODRS::user_skey"), ==, "secret-key");
	g_assert_cmpstr (as_review_get_metadata_item (review, "ODRS::app_id"),
			 ==,
			 "org.example.App");
	date = as_review_get_date (review);
	g_assert_nonnull (date);
	g_assert_cmpint (g_date_time_to_unix (date), ==, 1622130000);
	/* this review was written by us and voted on */
	g_assert_cmpint (as_review_get_flags (review),
			 ==,
			 AS_REVIEW_FLAG_SELF | AS_REVIEW_FLAG_VOTED);

	review = AS_REVIEW (g_ptr_array_index (reviews, 1));
	g_assert_cmpstr (as_review_get_id (review), ==, "101");
	g_assert_cmpstr (as_review_get_reviewer_name (review), ==, "John Doe");
	g_assert_cmpint (as_review_get_rating (review), ==, 40);
	g_assert_cmpint (as_review_get_flags (review), ==, AS_REVIEW_FLAG_NONE);
	/* priority is the Wilson score computed from the karma up/down votes */
	g_assert_cmpint (as_review_get_priority (review), ==, 55);
}

/**
 * test_reviews_client_parse_rating:
 *
 * Test parsing of an ODRS-style star-rating summary JSON document.
 */
static void
test_reviews_client_parse_rating (void)
{
	g_autoptr(AsReviewsClient) rrc = as_reviews_client_new ();
	g_autoptr(GError) error = NULL;
	gint rating;
	const gchar *json_data = "{\"star0\": 3, \"star1\": 1, \"star2\": 2, \"star3\": 5,"
				 " \"star4\": 10, \"star5\": 20, \"total\": 41}";
	const gchar *json_data_empty = "{\"star0\": 0, \"star1\": 0, \"star2\": 0, \"star3\": 0,"
				       " \"star4\": 0, \"star5\": 0, \"total\": 0}";

	rating = as_reviews_client_parse_rating (rrc, json_data, -1, &error);
	g_assert_no_error (error);
	g_assert_cmpint (rating, ==, 80);

	/* no rating can be determined if nobody voted */
	rating = as_reviews_client_parse_rating (rrc, json_data_empty, -1, &error);
	g_assert_no_error (error);
	g_assert_cmpint (rating, ==, -1);

	/* the ODRS sends an empty array for components it knows nothing about */
	rating = as_reviews_client_parse_rating (rrc, "[]", -1, &error);
	g_assert_no_error (error);
	g_assert_cmpint (rating, ==, -1);
}

int
main (int argc, char **argv)
{
	int ret;

	setlocale (LC_ALL, "");

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

	g_test_add_func ("/AppStream/Misc/YAMLNews", test_readwrite_yaml_news);
	g_test_add_func ("/AppStream/Misc/TextNews", test_readwrite_text_news);
	g_test_add_func ("/AppStream/Misc/StripLocaleEncoding", test_locale_strip_encoding);
	g_test_add_func ("/AppStream/Misc/RelationSatisfyCheck", test_relation_satisfy_check);
	g_test_add_func ("/AppStream/Misc/SysCompatScores", test_syscompat_scores);
	g_test_add_func ("/AppStream/Misc/SysCompatScoreErrorStatus",
			 test_syscompat_score_error_status);
	g_test_add_func ("/AppStream/Misc/ReviewsClientParseReview",
			 test_reviews_client_parse_review);
	g_test_add_func ("/AppStream/Misc/ReviewsClientParseRating",
			 test_reviews_client_parse_rating);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
