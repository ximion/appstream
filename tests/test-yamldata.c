/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include <glib/gprintf.h>

#include "appstream.h"
#include "as-metadata.h"
#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * as_yaml_test_serialize:
 *
 * Helper function for other tests.
 */
static gchar*
as_yaml_test_serialize (AsComponent *cpt)
{
	gchar *data;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;

	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "ALL");
	as_metadata_add_component (metad, cpt);
	as_metadata_set_write_header (metad, TRUE);

	data = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_YAML, &error);
	g_assert_no_error (error);

	return data;
}

/**
 * as_yaml_test_read_data:
 *
 * Helper function to read a single component from YAML data.
 */
static AsComponent*
as_yaml_test_read_data (const gchar *data, GError **error)
{
	AsComponent *cpt;
	g_autoptr(AsMetadata) metad = NULL;
	g_autofree gchar *data_full = NULL;

	data_full = g_strdup_printf ("---\n"
				     "File: DEP-11\n"
				     "Version: '0.14'\n"
				     "---\n%s", data);

	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "ALL");
	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_COLLECTION);

	if (error == NULL) {
		g_autoptr(GError) local_error = NULL;
		as_metadata_parse (metad, data_full, AS_FORMAT_KIND_YAML, &local_error);
		g_assert_no_error (local_error);

		g_assert_cmpint (as_metadata_get_components (metad)->len, >, 0);
		cpt = AS_COMPONENT (g_ptr_array_index (as_metadata_get_components (metad), 0));

		return g_object_ref (cpt);
	} else {
		as_metadata_parse (metad, data_full, AS_FORMAT_KIND_YAML, error);

		if (as_metadata_get_components (metad)->len > 0) {
			cpt = AS_COMPONENT (g_ptr_array_index (as_metadata_get_components (metad), 0));
			return g_object_ref (cpt);
		} else {
			return NULL;
		}
	}
}

/**
 * as_yaml_test_compare_yaml:
 *
 * Compare generated YAML line-by-line, prefixing the generated
 * data with the DEP-11 preamble first.
 */
static gboolean
as_yaml_test_compare_yaml (const gchar *result, const gchar *expected)
{
	g_autofree gchar *expected_full = NULL;
	expected_full = g_strdup_printf ("---\n"
					 "File: DEP-11\n"
					 "Version: '0.14'\n"
					 "---\n%s", expected);
	return as_test_compare_lines (result, expected_full);
}

/**
 * test_yaml_basic:
 *
 * Test basic functions related to YAML processing.
 */
static void
test_yaml_basic (void)
{
	g_autoptr(AsMetadata) mdata = NULL;
	gchar *path;
	GFile *file;
	GPtrArray *cpts;
	guint i;
	AsComponent *cpt_tomatoes = NULL;
	g_autoptr(GError) error = NULL;

	mdata = as_metadata_new ();
	as_metadata_set_locale (mdata, "C");
	as_metadata_set_format_style (mdata, AS_FORMAT_STYLE_COLLECTION);

	path = g_build_filename (datadir, "dep11-0.8.yml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_metadata_parse_file (mdata, file, AS_FORMAT_KIND_YAML, &error);
	g_object_unref (file);
	g_assert_no_error (error);

	cpts = as_metadata_get_components (mdata);
	g_assert_cmpint (cpts->len, ==, 8);

	for (i = 0; i < cpts->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));
		g_assert_true (as_component_is_valid (cpt));

		if (g_strcmp0 (as_component_get_name (cpt), "I Have No Tomatoes") == 0)
			cpt_tomatoes = cpt;
	}

	/* just check one of the components... */
	g_assert_nonnull (cpt_tomatoes);
	g_assert_cmpstr (as_component_get_summary (cpt_tomatoes), ==, "How many tomatoes can you smash in ten short minutes?");
	g_assert_cmpstr (as_component_get_pkgnames (cpt_tomatoes)[0], ==, "tomatoes");
}

static AsScreenshot*
test_h_create_dummy_screenshot (void)
{
	AsScreenshot *scr;
	AsImage *img;

	scr = as_screenshot_new ();
	as_screenshot_set_caption (scr, "The FooBar mainwindow", "C");
	as_screenshot_set_caption (scr, "Le FooBar mainwindow", "fr");

	img = as_image_new ();
	as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
	as_image_set_width (img, 840);
	as_image_set_height (img, 560);
	as_image_set_url (img, "https://example.org/images/foobar-full.png");
	as_screenshot_add_image (scr, img);
	g_object_unref (img);

	img = as_image_new ();
	as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
	as_image_set_width (img, 400);
	as_image_set_height (img, 200);
	as_image_set_url (img, "https://example.org/images/foobar-small.png");
	as_screenshot_add_image (scr, img);
	g_object_unref (img);

	img = as_image_new ();
	as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
	as_image_set_width (img, 210);
	as_image_set_height (img, 120);
	as_image_set_url (img, "https://example.org/images/foobar-smaller.png");
	as_screenshot_add_image (scr, img);
	g_object_unref (img);

	return scr;
}

/**
 * test_yamlwrite:
 *
 * Test writing a YAML document.
 */
static void
test_yamlwrite_misc (void)
{
	guint i;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(AsScreenshot) scr = NULL;
	g_autoptr(AsRelease) rel1 = NULL;
	g_autoptr(AsRelease) rel2 = NULL;
	g_autoptr(AsBundle) bdl = NULL;
	g_autoptr(GError) error = NULL;
	AsIssue *issue;
	g_autofree gchar *resdata = NULL;
	AsComponent *cpt = NULL;

	gchar *_PKGNAME1[2] = {"fwdummy", NULL};
	gchar *_PKGNAME2[2] = {"foobar-pkg", NULL};

	const gchar *expected_yaml =
				"Type: firmware\n"
				"ID: org.example.test.firmware\n"
				"DateEOL: 2022-02-22T00:00:00Z\n"
				"Package: fwdummy\n"
				"Extends:\n"
				"- org.example.alpha\n"
				"- org.example.beta\n"
				"Name:\n"
				"  de_DE: Ünittest Fürmwäre (dummy Eintrag)\n"
				"  C: Unittest Firmware\n"
				"Summary:\n"
				"  C: Just part of an unittest.\n"
				"Url:\n"
				"  homepage: https://example.com\n"
				"---\n"
				"Type: desktop-application\n"
				"ID: org.freedesktop.foobar.desktop\n"
				"Package: foobar-pkg\n"
				"Name:\n"
				"  C: TEST!!\n"
				"Summary:\n"
				"  C: Just part of an unittest.\n"
				"Icon:\n"
				"  cached:\n"
				"  - name: test_writetest.png\n"
				"    width: 20\n"
				"    height: 20\n"
				"  - name: test_writetest.png\n"
				"    width: 40\n"
				"    height: 40\n"
				"  stock: yml-writetest\n"
				"Bundles:\n"
				"- type: flatpak\n"
				"  id: foobar\n"
				"Screenshots:\n"
				"- caption:\n"
				"    fr: Le FooBar mainwindow\n"
				"    C: The FooBar mainwindow\n"
				"  thumbnails:\n"
				"  - url: https://example.org/images/foobar-small.png\n"
				"    width: 400\n"
				"    height: 200\n"
				"  - url: https://example.org/images/foobar-smaller.png\n"
				"    width: 210\n"
				"    height: 120\n"
				"  source-image:\n"
				"    url: https://example.org/images/foobar-full.png\n"
				"    width: 840\n"
				"    height: 560\n"
				"Languages:\n"
				"- locale: de_DE\n"
				"  percentage: 84\n"
				"- locale: en_GB\n"
				"  percentage: 100\n"
				"Releases:\n"
				"- version: '1.2'\n"
				"  type: stable\n"
				"  unix-timestamp: 1462288512\n"
				"  urgency: medium\n"
				"  description:\n"
				"    C: >-\n"
				"      <p>The CPU no longer overheats when you hold down spacebar.</p>\n"
				"  issues:\n"
				"  - id: bz#12345\n"
				"    url: https://example.com/bugzilla/12345\n"
				"  - type: cve\n"
				"    id: CVE-2019-123456\n"
				"- version: '1.0'\n"
				"  type: development\n"
				"  unix-timestamp: 1460463132\n"
				"  description:\n"
				"    de_DE: >-\n"
				"      <p>Großartige erste Veröffentlichung.</p>\n"
				"\n"
				"      <p>Zweite zeile.</p>\n"
				"    C: >-\n"
				"      <p>Awesome initial release.</p>\n"
				"\n"
				"      <p>Second paragraph.</p>\n"
				"  url:\n"
				"    details: https://example.org/releases/1.0.html\n"
				"---\n"
				"Type: generic\n"
				"ID: org.example.ATargetComponent\n"
				"Merge: replace\n"
				"Name:\n"
				"  C: ReplaceThis!\n";

	metad = as_metadata_new ();

	/* firmware component */
	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_FIRMWARE);
	as_component_set_id (cpt, "org.example.test.firmware");
	as_component_set_date_eol (cpt, "2022-02-22");
	as_component_set_pkgnames (cpt, _PKGNAME1);
	as_component_set_name (cpt, "Unittest Firmware", "C");
	as_component_set_name (cpt, "Ünittest Fürmwäre (dummy Eintrag)", "de_DE");
	as_component_set_summary (cpt, "Just part of an unittest.", "C");
	as_component_add_extends (cpt, "org.example.alpha");
	as_component_add_extends (cpt, "org.example.beta");
	as_component_add_url (cpt, AS_URL_KIND_HOMEPAGE, "https://example.com");
	as_metadata_add_component (metad, cpt);
	g_object_unref (cpt);

	/* component with icons, screenshots and release descriptions */
	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);
	as_component_set_id (cpt, "org.freedesktop.foobar.desktop");
	as_component_set_pkgnames (cpt, _PKGNAME2);
	as_component_set_name (cpt, "TEST!!", "C");
	as_component_set_summary (cpt, "Just part of an unittest.", "C");
	as_component_add_language (cpt, "en_GB", 100);
	as_component_add_language (cpt, "de_DE", 84);
	scr = test_h_create_dummy_screenshot ();
	as_component_add_screenshot (cpt, scr);

	for (i = 1; i <= 3; i++) {
		g_autoptr(AsIcon) icon = NULL;

		icon = as_icon_new ();
		if (i != 3)
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
		else
			as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
		as_icon_set_width (icon, i * 20);
		as_icon_set_height (icon, i * 20);

		if (i != 3)
			as_icon_set_filename (icon, "test_writetest.png");
		else
			as_icon_set_filename (icon, "yml-writetest");

		as_component_add_icon (cpt, icon);
	}

	rel1 = as_release_new ();
	as_release_set_version (rel1, "1.0");
	as_release_set_kind (rel1, AS_RELEASE_KIND_DEVELOPMENT);
	as_release_set_timestamp (rel1, 1460463132);
	as_release_set_description (rel1, "<p>Awesome initial release.</p>\n<p>Second paragraph.</p>", "C");
	as_release_set_description (rel1, "<p>Großartige erste Veröffentlichung.</p>\n<p>Zweite zeile.</p>", "de_DE");
	as_release_set_url (rel1, AS_RELEASE_URL_KIND_DETAILS, "https://example.org/releases/1.0.html");
	as_component_add_release (cpt, rel1);

	rel2 = as_release_new ();
	as_release_set_version (rel2, "1.2");
	as_release_set_timestamp (rel2, 1462288512);
	as_release_set_description (rel2, "<p>The CPU no longer overheats when you hold down spacebar.</p>", "C");
	as_release_set_urgency (rel2, AS_URGENCY_KIND_MEDIUM);
	as_component_add_release (cpt, rel2);

	/* issues */
	issue = as_issue_new ();
	as_issue_set_id (issue, "bz#12345");
	as_issue_set_url (issue, "https://example.com/bugzilla/12345");
	as_release_add_issue (rel2, issue);
	g_object_unref (issue);

	issue = as_issue_new ();
	as_issue_set_kind (issue, AS_ISSUE_KIND_CVE);
	as_issue_set_id (issue, "CVE-2019-123456");
	as_release_add_issue (rel2, issue);
	g_object_unref (issue);

	/* bundle */
	bdl = as_bundle_new ();
	as_bundle_set_kind (bdl, AS_BUNDLE_KIND_FLATPAK);
	as_bundle_set_id (bdl, "foobar");
	as_component_add_bundle (cpt, bdl);

	as_metadata_add_component (metad, cpt);
	g_object_unref (cpt);

	/* merge component */
	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_merge_kind (cpt, AS_MERGE_KIND_REPLACE);
	as_component_set_id (cpt, "org.example.ATargetComponent");
	as_component_set_name (cpt, "ReplaceThis!", "C");
	as_metadata_add_component (metad, cpt);
	g_object_unref (cpt);

	/* serialize and validate */
	resdata = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_YAML, &error);
	g_assert_no_error (error);

	g_assert_true (as_yaml_test_compare_yaml (resdata, expected_yaml));
}

/**
 * test_yaml_read_icons:
 *
 * Test reading the Icons field.
 */
static void
test_yaml_read_icons (void)
{
	guint i;
	GPtrArray *icons;
	g_autoptr(AsComponent) cpt = NULL;

	const gchar *yamldata_icons_legacy =
					"ID: org.example.Test\n"
					"Icon:\n"
					"  cached: test_test.png\n"
					"  stock: test\n";
	const gchar *yamldata_icons_current =
					"ID: org.example.Test\n"
					"Icon:\n"
					"  cached:\n"
					"    - width: 64\n"
					"      height: 64\n"
					"      name: test_test.png\n"
					"    - width: 64\n"
					"      height: 64\n"
					"      name: test_test.png\n"
					"      scale: 2\n"
					"    - width: 128\n"
					"      height: 128\n"
					"      name: test_test.png\n"
					"  stock: test\n";
	const gchar *yamldata_icons_single =
					"ID: org.example.Test\n"
					"Icon:\n"
					"  cached:\n"
					"    - width: 64\n"
					"      height: 64\n"
					"      name: single_test.png\n";

	/* check the legacy icons */
	cpt = as_yaml_test_read_data (yamldata_icons_legacy, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.Test");

	icons = as_component_get_icons (cpt);
	g_assert_cmpint (icons->len, ==, 2);
	for (i = 0; i < icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		if (as_icon_get_kind (icon) == AS_ICON_KIND_CACHED)
			g_assert_cmpstr (as_icon_get_filename (icon), ==, "test_test.png");
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK)
			g_assert_cmpstr (as_icon_get_name (icon), ==, "test");
	}

	/* check the new style icons tag */
	g_object_unref (cpt);
	cpt = as_yaml_test_read_data (yamldata_icons_current, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.Test");

	icons = as_component_get_icons (cpt);
	g_assert_cmpint (icons->len, ==, 4);
	for (i = 0; i < icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		if (as_icon_get_kind (icon) == AS_ICON_KIND_CACHED)
			g_assert_cmpstr (as_icon_get_filename (icon), ==, "test_test.png");
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK)
			g_assert_cmpstr (as_icon_get_name (icon), ==, "test");
	}

	g_assert_nonnull (as_component_get_icon_by_size (cpt, 64, 64));
	g_assert_nonnull (as_component_get_icon_by_size (cpt, 128, 128));

	/* check a component with just a single icon */
	g_object_unref (cpt);
	cpt = as_yaml_test_read_data (yamldata_icons_single, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.Test");

	icons = as_component_get_icons (cpt);
	g_assert_cmpint (icons->len, ==, 1);
	g_assert_cmpstr (as_icon_get_filename (AS_ICON (g_ptr_array_index (icons, 0))), ==, "single_test.png");
}

/**
 * test_yaml_read_languages:
 *
 * Test if reading the Languages field works.
 */
static void
test_yaml_read_languages (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *yamldata_languages =
					"ID: org.example.Test\n"
					"Languages:\n"
					"  - locale: de_DE\n"
					"    percentage: 48\n"
					"  - locale: en_GB\n"
					"    percentage: 100\n";

	cpt = as_yaml_test_read_data (yamldata_languages, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.Test");

	g_assert_cmpint (as_component_get_language (cpt, "de_DE"), ==, 48);
	g_assert_cmpint (as_component_get_language (cpt, "en_GB"), ==, 100);
	g_assert_cmpint (as_component_get_language (cpt, "invalid_C"), ==, -1);
}

/**
 * test_yaml_read_url:
 *
 * Test if reading the Url field works.
 */
static void
test_yaml_read_url (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *yamldata_urls =
				"ID: org.example.Test\n"
				"Url:\n"
				"  homepage: https://example.org\n"
				"  faq: https://example.org/faq\n"
				"  donation: https://example.org/donate\n"
				"  contact: https://example.org/contact\n"
				"  vcs-browser: https://example.org/source\n"
				"  contribute: https://example.org/contribute\n";

	cpt = as_yaml_test_read_data (yamldata_urls, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.Test");

	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "https://example.org");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_FAQ), ==, "https://example.org/faq");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_DONATION), ==, "https://example.org/donate");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_CONTACT), ==, "https://example.org/contact");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_VCS_BROWSER), ==, "https://example.org/source");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_CONTRIBUTE), ==, "https://example.org/contribute");
}

/**
 * test_yaml_corrupt_data:
 *
 * Test reading of a broken YAML document.
 */
static void
test_yaml_corrupt_data (void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *yamldata_corrupt = "ID: org.example.Test\n"
					"\007\n";

	cpt = as_yaml_test_read_data (yamldata_corrupt, &error);

	g_assert_error (error, AS_METADATA_ERROR, AS_METADATA_ERROR_PARSE);
	g_assert_null (cpt);
}

static const gchar *yamldata_simple_fields =
					"Type: generic\n"
					"ID: org.example.SimpleTest\n"
					"DateEOL: 2022-02-22T00:00:00Z\n"
					"Name:\n"
					"  C: TestComponent\n"
					"Summary:\n"
					"  C: Just part of an unittest\n"
					"NameVariantSuffix:\n"
					"  C: Generic\n";

/**
 * test_yaml_write_simple:
 *
 * Test writing some arbitrary fields
 */
static void
test_yaml_write_simple (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.SimpleTest");
	as_component_set_date_eol (cpt, "2022-02-22");

	as_component_set_name (cpt, "TestComponent", "C");
	as_component_set_summary (cpt, "Just part of an unittest", "C");
	as_component_set_name_variant_suffix (cpt, "Generic", "C");

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_simple_fields));
}

/**
 * test_yaml_read_simple:
 *
 * Test reading some arbitrary fields
 */
static void
test_yaml_read_simple (void)
{
	g_autoptr(AsComponent) cpt = NULL;

	cpt = as_yaml_test_read_data (yamldata_simple_fields, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.SimpleTest");

	g_assert_cmpstr (as_component_get_name (cpt), ==, "TestComponent");
	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Just part of an unittest");
	g_assert_cmpstr (as_component_get_name_variant_suffix (cpt), ==, "Generic");
	g_assert_cmpstr (as_component_get_date_eol (cpt), ==, "2022-02-22T00:00:00Z");
	g_assert_cmpint (as_component_get_timestamp_eol (cpt), ==, 1645488000);
}

/**
 * test_yaml_write_provides:
 *
 * Test writing the Provides field.
 */
static void
test_yaml_write_provides (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsProvided) prov_mime = NULL;
	g_autoptr(AsProvided) prov_bin = NULL;
	g_autoptr(AsProvided) prov_dbus = NULL;
	g_autoptr(AsProvided) prov_firmware_runtime = NULL;
	g_autoptr(AsProvided) prov_firmware_flashed = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_prov_yaml = "Type: generic\n"
					  "ID: org.example.ProvidesTest\n"
					  "Provides:\n"
					  "  mediatypes:\n"
                                          "  - text/plain\n"
                                          "  - application/xml\n"
                                          "  - image/png\n"
					  "  binaries:\n"
					  "  - foobar\n"
					  "  - foobar-viewer\n"
					  "  dbus:\n"
					  "  - type: system\n"
					  "    service: org.example.ProvidesTest.Modify\n"
					  "  firmware:\n"
					  "  - type: runtime\n"
					  "    file: ipw2200-bss.fw\n"
					  "  - type: flashed\n"
					  "    guid: 84f40464-9272-4ef7-9399-cd95f12da696\n";

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.ProvidesTest");

	prov_mime = as_provided_new ();
	as_provided_set_kind (prov_mime, AS_PROVIDED_KIND_MIMETYPE);
	as_provided_add_item (prov_mime, "text/plain");
	as_provided_add_item (prov_mime, "application/xml");
	as_provided_add_item (prov_mime, "image/png");
	as_component_add_provided (cpt, prov_mime);

	prov_bin = as_provided_new ();
	as_provided_set_kind (prov_bin, AS_PROVIDED_KIND_BINARY);
	as_provided_add_item (prov_bin, "foobar");
	as_provided_add_item (prov_bin, "foobar-viewer");
	as_component_add_provided (cpt, prov_bin);

	prov_dbus = as_provided_new ();
	as_provided_set_kind (prov_dbus, AS_PROVIDED_KIND_DBUS_SYSTEM);
	as_provided_add_item (prov_dbus, "org.example.ProvidesTest.Modify");
	as_component_add_provided (cpt, prov_dbus);

	prov_firmware_runtime = as_provided_new ();
	as_provided_set_kind (prov_firmware_runtime, AS_PROVIDED_KIND_FIRMWARE_RUNTIME);
	as_provided_add_item (prov_firmware_runtime, "ipw2200-bss.fw");
	as_component_add_provided (cpt, prov_firmware_runtime);

	prov_firmware_flashed = as_provided_new ();
	as_provided_set_kind (prov_firmware_flashed, AS_PROVIDED_KIND_FIRMWARE_FLASHED);
	as_provided_add_item (prov_firmware_flashed, "84f40464-9272-4ef7-9399-cd95f12da696");
	as_component_add_provided (cpt, prov_firmware_flashed);

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, expected_prov_yaml));
}

/**
 * test_yaml_read_provides:
 *
 * Test if reading the Provides field works.
 */
static void
test_yaml_read_provides (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *provides;
	GPtrArray *cpt_items;
	AsProvided *prov;
	const gchar *yamldata_provides = "ID: org.example.ProvidesTest\n"
					 "Provides:\n"
					 "  mediatypes:\n"
                                         "  - text/plain\n"
                                         "  - application/xml\n"
                                         "  - image/png\n"
					 "  binaries:\n"
					 "  - foobar\n"
					 "  - foobar-viewer\n"
					 "  dbus:\n"
					 "  - type: system\n"
					 "    service: org.example.ProvidesTest.Modify\n"
					 "  firmware:\n"
					 "  - type: runtime\n"
					 "    file: ipw2200-bss.fw\n"
					 "  - type: flashed\n"
					 "    guid: 84f40464-9272-4ef7-9399-cd95f12da696\n";

	cpt = as_yaml_test_read_data (yamldata_provides, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ProvidesTest");

	provides = as_component_get_provided (cpt);
	g_assert_cmpint (provides->len, ==, 5);

	prov = AS_PROVIDED (g_ptr_array_index (provides, 0));
	g_assert_true (as_provided_get_kind (prov) == AS_PROVIDED_KIND_MIMETYPE);
	cpt_items = as_provided_get_items (prov);
	g_assert_cmpint (cpt_items->len, ==, 3);

	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_items, 0), ==, "text/plain");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_items, 1), ==, "application/xml");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_items, 2), ==, "image/png");

	prov = AS_PROVIDED (g_ptr_array_index (provides, 1));
	g_assert_true (as_provided_get_kind (prov) == AS_PROVIDED_KIND_BINARY);
	cpt_items = as_provided_get_items (prov);
	g_assert_cmpint (cpt_items->len, ==, 2);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_items, 0), ==, "foobar");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_items, 1), ==, "foobar-viewer");

	prov = AS_PROVIDED (g_ptr_array_index (provides, 2));
	g_assert_true (as_provided_get_kind (prov) == AS_PROVIDED_KIND_DBUS_SYSTEM);
	cpt_items = as_provided_get_items (prov);
	g_assert_cmpint (cpt_items->len, ==, 1);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_items, 0), ==, "org.example.ProvidesTest.Modify");

	prov = AS_PROVIDED (g_ptr_array_index (provides, 3));
	g_assert_true (as_provided_get_kind (prov) == AS_PROVIDED_KIND_FIRMWARE_RUNTIME);
	cpt_items = as_provided_get_items (prov);
	g_assert_cmpint (cpt_items->len, ==, 1);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_items, 0), ==, "ipw2200-bss.fw");

	prov = AS_PROVIDED (g_ptr_array_index (provides, 4));
	g_assert_true (as_provided_get_kind (prov) == AS_PROVIDED_KIND_FIRMWARE_FLASHED);
	cpt_items = as_provided_get_items (prov);
	g_assert_cmpint (cpt_items->len, ==, 1);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_items, 0), ==, "84f40464-9272-4ef7-9399-cd95f12da696");
}

/**
 * test_yaml_write_suggests:
 *
 * Test writing the Suggests field.
 */
static void
test_yaml_write_suggests (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsSuggested) sug_us = NULL;
	g_autoptr(AsSuggested) sug_hr = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_sug_yaml = "Type: generic\n"
					 "ID: org.example.SuggestsTest\n"
					 "Suggests:\n"
					 "- type: upstream\n"
					 "  ids:\n"
					 "  - org.example.Awesome\n"
					 "- type: heuristic\n"
					 "  ids:\n"
					 "  - org.example.MachineLearning\n"
					 "  - org.example.Stuff\n";

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.SuggestsTest");

	sug_us = as_suggested_new ();
	as_suggested_set_kind (sug_us, AS_SUGGESTED_KIND_UPSTREAM);
	as_suggested_add_id (sug_us, "org.example.Awesome");
	as_component_add_suggested (cpt, sug_us);

	sug_hr = as_suggested_new ();
	as_suggested_set_kind (sug_hr, AS_SUGGESTED_KIND_HEURISTIC);
	as_suggested_add_id (sug_hr, "org.example.MachineLearning");
	as_suggested_add_id (sug_hr, "org.example.Stuff");
	as_component_add_suggested (cpt, sug_hr);

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, expected_sug_yaml));
}

/**
 * test_yaml_read_suggests:
 *
 * Test if reading the Suggests field works.
 */
static void
test_yaml_read_suggests (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *suggestions;
	GPtrArray *cpt_ids;
	AsSuggested *sug;
	const gchar *yamldata_suggests = "ID: org.example.Test\n"
					 "Suggests:\n"
					 "  - type: upstream\n"
					 "    ids:\n"
					 "      - org.example.Awesome\n"
					 "      - org.example.test1\n"
					 "      - org.example.test2\n"
					 "  - type: heuristic\n"
					 "    ids:\n"
					 "      - org.example.test3\n";

	cpt = as_yaml_test_read_data (yamldata_suggests, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.Test");

	suggestions = as_component_get_suggested (cpt);
	g_assert_cmpint (suggestions->len, ==, 2);

	sug = AS_SUGGESTED (g_ptr_array_index (suggestions, 0));
	g_assert_true (as_suggested_get_kind (sug) == AS_SUGGESTED_KIND_UPSTREAM);
	cpt_ids = as_suggested_get_ids (sug);
	g_assert_cmpint (cpt_ids->len, ==, 3);

	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 0), ==, "org.example.Awesome");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 1), ==, "org.example.test1");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 2), ==, "org.example.test2");

	sug = AS_SUGGESTED (g_ptr_array_index (suggestions, 1));
	g_assert_true (as_suggested_get_kind (sug) == AS_SUGGESTED_KIND_HEURISTIC);
	cpt_ids = as_suggested_get_ids (sug);
	g_assert_cmpint (cpt_ids->len, ==, 1);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 0), ==, "org.example.test3");
}

static const gchar *yamldata_custom_field =
				"Type: generic\n"
				"ID: org.example.CustomTest\n"
				"Custom:\n"
				"  executable: myapp --test\n"
				"  foo bar: value-with space\n"
				"  Oh::Snap::Punctuation!: Awesome!\n";
/**
 * test_yaml_write_custom:
 *
 * Test writing the Custom fields.
 */
static void
test_yaml_write_custom (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.CustomTest");

	as_component_insert_custom_value (cpt, "executable", "myapp --test");
	as_component_insert_custom_value (cpt, "foo bar", "value-with space");
	as_component_insert_custom_value (cpt, "Oh::Snap::Punctuation!", "Awesome!");

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_custom_field));
}

/**
 * test_yaml_read_custom:
 *
 * Test if reading the Custom field works.
 */
static void
test_yaml_read_custom (void)
{
	g_autoptr(AsComponent) cpt = NULL;

	cpt = as_yaml_test_read_data (yamldata_custom_field, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.CustomTest");

	g_assert_cmpstr (as_component_get_custom_value (cpt, "executable"), ==, "myapp --test");
	g_assert_cmpstr (as_component_get_custom_value (cpt, "foo bar"), ==, "value-with space");
	g_assert_cmpstr (as_component_get_custom_value (cpt, "Oh::Snap::Punctuation!"), ==, "Awesome!");
}

static const gchar *yamldata_content_rating_field =
					"Type: generic\n"
					"ID: org.example.ContentRatingTest\n"
					"ContentRating:\n"
					"  oars-1.0:\n"
					"    drugs-alcohol: moderate\n"
					"    language-humor: mild\n";

/**
 * test_yaml_write_content_rating:
 *
 * Test writing the ContentRating field.
 */
static void
test_yaml_write_content_rating (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsContentRating) rating = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.ContentRatingTest");

	rating = as_content_rating_new ();
	as_content_rating_set_kind (rating, "oars-1.0");

	as_content_rating_set_value (rating, "drugs-alcohol", AS_CONTENT_RATING_VALUE_MODERATE);
	as_content_rating_set_value (rating, "language-humor", AS_CONTENT_RATING_VALUE_MILD);

	as_component_add_content_rating (cpt, rating);

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_content_rating_field));
}

/**
 * test_yaml_read_content_rating:
 *
 * Test if reading the ContentRating field works.
 */
static void
test_yaml_read_content_rating (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsContentRating *rating;

	cpt = as_yaml_test_read_data (yamldata_content_rating_field, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ContentRatingTest");

	rating = as_component_get_content_rating (cpt, "oars-1.0");
	g_assert_nonnull (rating);
	g_assert_cmpint (as_content_rating_get_value (rating, "drugs-alcohol"), ==, AS_CONTENT_RATING_VALUE_MODERATE);
	g_assert_cmpint (as_content_rating_get_value (rating, "language-humor"), ==, AS_CONTENT_RATING_VALUE_MILD);
	g_assert_cmpint (as_content_rating_get_value (rating, "violence-bloodshed"), ==, AS_CONTENT_RATING_VALUE_NONE);
}

static const gchar *yamldata_launchable_field =
					"Type: generic\n"
					"ID: org.example.LaunchTest\n"
					"Launchable:\n"
					"  desktop-id:\n"
					"  - org.example.Test.desktop\n"
					"  - kde4-kool.desktop\n";

/**
 * test_yaml_write_launchable:
 *
 * Test writing the Launchable field.
 */
static void
test_yaml_write_launchable (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsLaunchable) launch = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.LaunchTest");

	launch = as_launchable_new ();
	as_launchable_set_kind (launch, AS_LAUNCHABLE_KIND_DESKTOP_ID);

	as_launchable_add_entry (launch, "org.example.Test.desktop");
	as_launchable_add_entry (launch, "kde4-kool.desktop");

	as_component_add_launchable (cpt, launch);

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_launchable_field));
}

/**
 * test_yaml_read_launchable:
 *
 * Test if reading the Launchable field works.
 */
static void
test_yaml_read_launchable (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsLaunchable *launch;

	cpt = as_yaml_test_read_data (yamldata_launchable_field, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.LaunchTest");

	launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_nonnull (launch);

	g_assert_cmpint (as_launchable_get_entries (launch)->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (as_launchable_get_entries (launch), 0), ==, "org.example.Test.desktop");
	g_assert_cmpstr (g_ptr_array_index (as_launchable_get_entries (launch), 1), ==, "kde4-kool.desktop");
}

static const gchar *yamldata_relations_field =
				"Type: generic\n"
				"ID: org.example.RelationsTest\n"
				"Replaces:\n"
				"- id: org.example.old_test\n"
				"Requires:\n"
				"- kernel: Linux\n"
				"  version: '>= 4.15'\n"
				"- id: org.example.TestDependency\n"
				"  version: == 1.2\n"
				"- display_length: 4200\n"
				"- internet: always\n"
				"  bandwidth_mbitps: 2\n"
				"Recommends:\n"
				"- memory: 2500\n"
				"- modalias: usb:v1130p0202d*\n"
				"- display_length: <= xlarge\n"
				"  side: longest\n"
				"- internet: first-run\n"
				"Supports:\n"
				"- control: gamepad\n"
				"- control: keyboard\n"
				"- internet: offline-only\n";

/**
 * test_yaml_write_relations:
 *
 * Test writing the Requires/Recommends fields.
 */
static void
test_yaml_write_relations (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	g_autoptr(AsRelation) mem_relation = NULL;
	g_autoptr(AsRelation) moda_relation = NULL;
	g_autoptr(AsRelation) kernel_relation = NULL;
	g_autoptr(AsRelation) id_relation = NULL;
	g_autoptr(AsRelation) dl_relation1 = NULL;
	g_autoptr(AsRelation) dl_relation2 = NULL;
	g_autoptr(AsRelation) ctl_relation1 = NULL;
	g_autoptr(AsRelation) ctl_relation2 = NULL;
	g_autoptr(AsRelation) internet_relation1 = NULL;
	g_autoptr(AsRelation) internet_relation2 = NULL;
	g_autoptr(AsRelation) internet_relation3 = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.RelationsTest");

	mem_relation = as_relation_new ();
	moda_relation = as_relation_new ();
	kernel_relation = as_relation_new ();
	id_relation = as_relation_new ();
	dl_relation1 = as_relation_new ();
	dl_relation2 = as_relation_new ();
	ctl_relation1 = as_relation_new ();
	ctl_relation2 = as_relation_new ();
	internet_relation1 = as_relation_new ();
	internet_relation2 = as_relation_new ();
	internet_relation3 = as_relation_new ();

	as_relation_set_kind (mem_relation, AS_RELATION_KIND_RECOMMENDS);
	as_relation_set_kind (moda_relation, AS_RELATION_KIND_RECOMMENDS);
	as_relation_set_kind (kernel_relation, AS_RELATION_KIND_REQUIRES);
	as_relation_set_kind (id_relation, AS_RELATION_KIND_REQUIRES);
	as_relation_set_kind (dl_relation1, AS_RELATION_KIND_RECOMMENDS);
	as_relation_set_kind (dl_relation2, AS_RELATION_KIND_REQUIRES);
	as_relation_set_kind (ctl_relation1, AS_RELATION_KIND_SUPPORTS);
	as_relation_set_kind (ctl_relation2, AS_RELATION_KIND_SUPPORTS);
	as_relation_set_kind (internet_relation1, AS_RELATION_KIND_REQUIRES);
	as_relation_set_kind (internet_relation2, AS_RELATION_KIND_RECOMMENDS);
	as_relation_set_kind (internet_relation3, AS_RELATION_KIND_SUPPORTS);

	as_relation_set_item_kind (mem_relation, AS_RELATION_ITEM_KIND_MEMORY);
	as_relation_set_value_int (mem_relation, 2500);
	as_relation_set_item_kind (moda_relation, AS_RELATION_ITEM_KIND_MODALIAS);
	as_relation_set_value_str (moda_relation, "usb:v1130p0202d*");

	as_relation_set_item_kind (kernel_relation, AS_RELATION_ITEM_KIND_KERNEL);
	as_relation_set_value_str (kernel_relation, "Linux");
	as_relation_set_version (kernel_relation, "4.15");
	as_relation_set_compare (kernel_relation, AS_RELATION_COMPARE_GE);

	as_relation_set_item_kind (id_relation, AS_RELATION_ITEM_KIND_ID);
	as_relation_set_value_str (id_relation, "org.example.TestDependency");
	as_relation_set_version (id_relation, "1.2");
	as_relation_set_compare (id_relation, AS_RELATION_COMPARE_EQ);

	as_relation_set_item_kind (dl_relation1, AS_RELATION_ITEM_KIND_DISPLAY_LENGTH);
	as_relation_set_value_display_length_kind (dl_relation1, AS_DISPLAY_LENGTH_KIND_XLARGE);
	as_relation_set_display_side_kind (dl_relation1, AS_DISPLAY_SIDE_KIND_LONGEST);
	as_relation_set_compare (dl_relation1, AS_RELATION_COMPARE_LE);

	as_relation_set_item_kind (dl_relation2, AS_RELATION_ITEM_KIND_DISPLAY_LENGTH);
	as_relation_set_value_int (dl_relation2, 4200);
	as_relation_set_compare (dl_relation2, AS_RELATION_COMPARE_GE);

	as_relation_set_item_kind (internet_relation1, AS_RELATION_ITEM_KIND_INTERNET);
	as_relation_set_value_internet_kind (internet_relation1, AS_INTERNET_KIND_ALWAYS);
	as_relation_set_value_internet_bandwidth (internet_relation1, 2);

	as_relation_set_item_kind (internet_relation2, AS_RELATION_ITEM_KIND_INTERNET);
	as_relation_set_value_internet_kind (internet_relation2, AS_INTERNET_KIND_FIRST_RUN);

	as_relation_set_item_kind (internet_relation3, AS_RELATION_ITEM_KIND_INTERNET);
	as_relation_set_value_internet_kind (internet_relation3, AS_INTERNET_KIND_OFFLINE_ONLY);

	as_relation_set_item_kind (ctl_relation1, AS_RELATION_ITEM_KIND_CONTROL);
	as_relation_set_item_kind (ctl_relation2, AS_RELATION_ITEM_KIND_CONTROL);
	as_relation_set_value_control_kind (ctl_relation1, AS_CONTROL_KIND_GAMEPAD);
	as_relation_set_value_control_kind (ctl_relation2, AS_CONTROL_KIND_KEYBOARD);

	as_component_add_relation (cpt, mem_relation);
	as_component_add_relation (cpt, moda_relation);
	as_component_add_relation (cpt, kernel_relation);
	as_component_add_relation (cpt, id_relation);
	as_component_add_relation (cpt, dl_relation1);
	as_component_add_relation (cpt, dl_relation2);
	as_component_add_relation (cpt, ctl_relation1);
	as_component_add_relation (cpt, ctl_relation2);
	as_component_add_relation (cpt, internet_relation1);
	as_component_add_relation (cpt, internet_relation2);
	as_component_add_relation (cpt, internet_relation3);

	as_component_add_replaces (cpt, "org.example.old_test");

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_relations_field));
}

/**
 * test_yaml_read_relations:
 *
 * Test if reading the Requires/Recommends fields works.
 */
static void
test_yaml_read_relations (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *recommends;
	GPtrArray *requires;
	GPtrArray *supports;
	AsRelation *relation;

	cpt = as_yaml_test_read_data (yamldata_relations_field, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.RelationsTest");

	requires = as_component_get_requires (cpt);
	recommends = as_component_get_recommends (cpt);
	supports = as_component_get_supports (cpt);

	g_assert_cmpint (requires->len, ==, 4);
	g_assert_cmpint (recommends->len, ==, 4);
	g_assert_cmpint (supports->len, ==, 3);

	/* component replacement */
	g_assert_cmpint (as_component_get_replaces (cpt)->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (as_component_get_replaces (cpt), 0), ==, "org.example.old_test");

	/* memory relation */
	relation = AS_RELATION (g_ptr_array_index (recommends, 0));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_RECOMMENDS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_MEMORY);
	g_assert_cmpint (as_relation_get_value_int (relation), ==, 2500);

	/* modalias relation */
	relation = AS_RELATION (g_ptr_array_index (recommends, 1));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_RECOMMENDS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_MODALIAS);
	g_assert_cmpstr (as_relation_get_value_str (relation), ==, "usb:v1130p0202d*");

	/* display_length relation (REC) */
	relation = AS_RELATION (g_ptr_array_index (recommends, 2));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_RECOMMENDS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_DISPLAY_LENGTH);
	g_assert_cmpint (as_relation_get_value_display_length_kind (relation), ==, AS_DISPLAY_LENGTH_KIND_XLARGE);
	g_assert_cmpint (as_relation_get_compare (relation), ==, AS_RELATION_COMPARE_LE);

	/* internet relation (REC) */
	relation = AS_RELATION (g_ptr_array_index (recommends, 3));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_RECOMMENDS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_INTERNET);
	g_assert_cmpint (as_relation_get_value_internet_kind (relation), ==, AS_INTERNET_KIND_FIRST_RUN);
	g_assert_cmpint (as_relation_get_value_internet_bandwidth (relation), ==, 0);

	/* kernel relation */
	relation = AS_RELATION (g_ptr_array_index (requires, 0));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_REQUIRES);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_KERNEL);
	g_assert_cmpstr (as_relation_get_value_str (relation), ==, "Linux");
	g_assert_cmpstr (as_relation_get_version (relation), ==, "4.15");
	g_assert_cmpint (as_relation_get_compare (relation), ==, AS_RELATION_COMPARE_GE);

	/* ID relation */
	relation = AS_RELATION (g_ptr_array_index (requires, 1));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_REQUIRES);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_ID);
	g_assert_cmpstr (as_relation_get_value_str (relation), ==, "org.example.TestDependency");
	g_assert_cmpstr (as_relation_get_version (relation), ==, "1.2");
	g_assert_cmpint (as_relation_get_compare (relation), ==, AS_RELATION_COMPARE_EQ);

	/* display_length relation (REQ) */
	relation = AS_RELATION (g_ptr_array_index (requires, 2));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_REQUIRES);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_DISPLAY_LENGTH);
	g_assert_cmpint (as_relation_get_value_px (relation), ==, 4200);
	g_assert_cmpint (as_relation_get_compare (relation), ==, AS_RELATION_COMPARE_GE);

	/* internet relation (REQ) */
	relation = AS_RELATION (g_ptr_array_index (requires, 3));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_REQUIRES);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_INTERNET);
	g_assert_cmpint (as_relation_get_value_internet_kind (relation), ==, AS_INTERNET_KIND_ALWAYS);
	g_assert_cmpint (as_relation_get_value_internet_bandwidth (relation), ==, 2);

	/* control relation */
	relation = AS_RELATION (g_ptr_array_index (supports, 0));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_SUPPORTS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_CONTROL);
	g_assert_cmpint (as_relation_get_value_control_kind (relation), ==, AS_CONTROL_KIND_GAMEPAD);
	relation = AS_RELATION (g_ptr_array_index (supports, 1));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_SUPPORTS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_CONTROL);
	g_assert_cmpint (as_relation_get_value_control_kind (relation), ==, AS_CONTROL_KIND_KEYBOARD);

	/* internet relation (supports) */
	relation = AS_RELATION (g_ptr_array_index (supports, 2));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_SUPPORTS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_INTERNET);
	g_assert_cmpint (as_relation_get_value_internet_kind (relation), ==, AS_INTERNET_KIND_OFFLINE_ONLY);
	g_assert_cmpint (as_relation_get_value_internet_bandwidth (relation), ==, 0);
}


static const gchar *yamldata_agreements =
				"Type: generic\n"
				"ID: org.example.AgreementsTest\n"
				"Agreements:\n"
				"- type: eula\n"
				"  version-id: 1.2.3a\n"
				"  sections:\n"
				"  - type: intro\n"
				"    name:\n"
				"      C: Intro\n"
				"      xde_DE: Einführung\n"
				"    description:\n"
				"      C: >-\n"
				"        <p>Mighty Fine</p>\n";


/**
 * test_yaml_write_agreements:
 *
 * Test writing the Agreements field.
 */
static void
test_yaml_write_agreements (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	g_autoptr(AsAgreement) agreement = NULL;
	g_autoptr(AsAgreementSection) sect = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.AgreementsTest");

	agreement = as_agreement_new ();
	sect = as_agreement_section_new ();

	as_agreement_set_kind (agreement, AS_AGREEMENT_KIND_EULA);
	as_agreement_set_version_id (agreement, "1.2.3a");

	as_agreement_section_set_kind (sect, "intro");
	as_agreement_section_set_name (sect, "Intro", "C");
	as_agreement_section_set_name (sect, "Einführung", "xde_DE");

	as_agreement_section_set_description (sect, "<p>Mighty Fine</p>", "C");

	as_agreement_add_section (agreement, sect);
	as_component_add_agreement (cpt, agreement);

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_agreements));
}

/**
 * test_yaml_read_agreements:
 *
 * Test if reading the Agreement field works.
 */
static void
test_yaml_read_agreements (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsAgreement *agreement;
	AsAgreementSection *sect;

	cpt = as_yaml_test_read_data (yamldata_agreements, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.AgreementsTest");

	agreement = as_component_get_agreement_by_kind (cpt, AS_AGREEMENT_KIND_EULA);
	g_assert_nonnull (agreement);

	g_assert_cmpint (as_agreement_get_kind (agreement), ==, AS_AGREEMENT_KIND_EULA);
	g_assert_cmpstr (as_agreement_get_version_id (agreement), ==, "1.2.3a");
	sect = as_agreement_get_section_default (agreement);
	g_assert_nonnull (sect);

	as_agreement_section_set_active_locale (sect, "C");
	g_assert_cmpstr (as_agreement_section_get_kind (sect), ==, "intro");
	g_assert_cmpstr (as_agreement_section_get_name (sect), ==, "Intro");
	g_assert_cmpstr (as_agreement_section_get_description (sect), ==, "<p>Mighty Fine</p>");

	as_agreement_section_set_active_locale (sect, "xde_DE");
	g_assert_cmpstr (as_agreement_section_get_name (sect), ==, "Einführung");
}

static const gchar *yamldata_screenshots =
				"Type: generic\n"
				"ID: org.example.ScreenshotsTest\n"
				"Screenshots:\n"
				"- default: true\n"
				"  caption:\n"
				"    de_DE: Das Hauptfenster, welches irgendwas zeigt\n"
				"    C: The main window displaying a thing\n"
				"  thumbnails:\n"
				"  - url: https://example.org/alpha_small.png\n"
				"    width: 800\n"
				"    height: 600\n"
				"  source-image:\n"
				"    url: https://example.org/alpha.png\n"
				"    width: 1916\n"
				"    height: 1056\n"
				"- caption:\n"
				"    C: A screencast of this app\n"
				"  videos:\n"
				"  - codec: av1\n"
				"    container: matroska\n"
				"    url: https://example.org/screencast.mkv\n"
				"    width: 1916\n"
				"    height: 1056\n"
				"  - codec: av1\n"
				"    container: matroska\n"
				"    url: https://example.org/screencast_de.mkv\n"
				"    width: 1916\n"
				"    height: 1056\n"
				"    lang: de_DE\n";

/**
 * test_yaml_write_screenshots:
 *
 * Test writing the Screenshots field.
 */
static void
test_yaml_write_screenshots (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	g_autoptr(AsScreenshot) scr1 = NULL;
	g_autoptr(AsScreenshot) scr2 = NULL;
	AsImage *img;
	AsVideo *vid;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.ScreenshotsTest");

	scr1 = as_screenshot_new ();
	as_screenshot_set_kind (scr1, AS_SCREENSHOT_KIND_DEFAULT);
	as_screenshot_set_caption (scr1, "The main window displaying a thing", "C");
	as_screenshot_set_caption (scr1, "Das Hauptfenster, welches irgendwas zeigt", "de_DE");
	img = as_image_new ();
	as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
	as_image_set_width (img, 1916);
	as_image_set_height (img, 1056);
	as_image_set_url (img, "https://example.org/alpha.png");
	as_screenshot_add_image (scr1, img);
	g_object_unref (img);

	img = as_image_new ();
	as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
	as_image_set_width (img, 800);
	as_image_set_height (img, 600);
	as_image_set_url (img, "https://example.org/alpha_small.png");
	as_screenshot_add_image (scr1, img);
	g_object_unref (img);

	scr2 = as_screenshot_new ();
	as_screenshot_set_caption (scr2, "A screencast of this app", "C");
	vid = as_video_new ();
	as_video_set_codec_kind (vid, AS_VIDEO_CODEC_KIND_AV1);
	as_video_set_container_kind (vid, AS_VIDEO_CONTAINER_KIND_MKV);
	as_video_set_width (vid, 1916);
	as_video_set_height (vid, 1056);
	as_video_set_url (vid, "https://example.org/screencast.mkv");
	as_screenshot_add_video (scr2, vid);
	g_object_unref (vid);

	vid = as_video_new ();
	as_video_set_codec_kind (vid, AS_VIDEO_CODEC_KIND_AV1);
	as_video_set_container_kind (vid, AS_VIDEO_CONTAINER_KIND_MKV);
	as_video_set_locale (vid, "de_DE");
	as_video_set_width (vid, 1916);
	as_video_set_height (vid, 1056);
	as_video_set_url (vid, "https://example.org/screencast_de.mkv");
	as_screenshot_add_video (scr2, vid);
	g_object_unref (vid);

	as_component_add_screenshot (cpt, scr1);
	as_component_add_screenshot (cpt, scr2);

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_screenshots));
}

/**
 * test_yaml_read_screenshots:
 *
 * Test if reading the Screenshots field works.
 */
static void
test_yaml_read_screenshots (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *screenshots;
	AsScreenshot *scr1;
	AsScreenshot *scr2;
	GPtrArray *images;
	GPtrArray *videos;
	AsImage *img;
	AsVideo *vid;

	cpt = as_yaml_test_read_data (yamldata_screenshots, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ScreenshotsTest");

	screenshots = as_component_get_screenshots (cpt);
	g_assert_cmpint (screenshots->len, ==, 2);

	scr1 = AS_SCREENSHOT (g_ptr_array_index (screenshots, 0));
	scr2 = AS_SCREENSHOT (g_ptr_array_index (screenshots, 1));

	/* screenshot 1 */
	g_assert_cmpint (as_screenshot_get_kind (scr1), ==, AS_SCREENSHOT_KIND_DEFAULT);
	g_assert_cmpint (as_screenshot_get_media_kind (scr1), ==, AS_SCREENSHOT_MEDIA_KIND_IMAGE);
	as_screenshot_set_active_locale (scr1, "C");
	g_assert_cmpstr (as_screenshot_get_caption (scr1), ==, "The main window displaying a thing");
	as_screenshot_set_active_locale (scr1, "de_DE");
	g_assert_cmpstr (as_screenshot_get_caption (scr1), ==, "Das Hauptfenster, welches irgendwas zeigt");

	images = as_screenshot_get_images_all (scr1);
	g_assert_cmpint (images->len, ==, 2);

	img = AS_IMAGE (g_ptr_array_index (images, 1));
	g_assert_cmpint (as_image_get_kind (img), ==, AS_IMAGE_KIND_SOURCE);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/alpha.png");
	g_assert_cmpint (as_image_get_width (img), ==, 1916);
	g_assert_cmpint (as_image_get_height (img), ==, 1056);

	img = AS_IMAGE (g_ptr_array_index (images, 0));
	g_assert_cmpint (as_image_get_kind (img), ==, AS_IMAGE_KIND_THUMBNAIL);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/alpha_small.png");
	g_assert_cmpint (as_image_get_width (img), ==, 800);
	g_assert_cmpint (as_image_get_height (img), ==, 600);

	/* screenshot 2 */
	as_screenshot_set_active_locale (scr2, "C");
	g_assert_cmpint (as_screenshot_get_kind (scr2), ==, AS_SCREENSHOT_KIND_EXTRA);
	g_assert_cmpint (as_screenshot_get_media_kind (scr2), ==, AS_SCREENSHOT_MEDIA_KIND_VIDEO);
	g_assert_cmpstr (as_screenshot_get_caption (scr2), ==, "A screencast of this app");
	as_screenshot_set_active_locale (scr2, "C");
	g_assert_cmpint (as_screenshot_get_images (scr2)->len, ==, 0);
	videos = as_screenshot_get_videos (scr2);
	g_assert_cmpint (videos->len, ==, 1);
	as_screenshot_set_active_locale (scr2, "de_DE");
	videos = as_screenshot_get_videos (scr2);
	g_assert_cmpint (videos->len, ==, 1);
	vid = AS_VIDEO (g_ptr_array_index (videos, 0));
	g_assert_cmpstr (as_video_get_url (vid), ==, "https://example.org/screencast_de.mkv");

	as_screenshot_set_active_locale (scr2, "ALL");
	videos = as_screenshot_get_videos (scr2);
	g_assert_cmpint (videos->len, ==, 2);

	vid = AS_VIDEO (g_ptr_array_index (videos, 0));
	g_assert_cmpint (as_video_get_codec_kind (vid), ==, AS_VIDEO_CODEC_KIND_AV1);
	g_assert_cmpint (as_video_get_container_kind (vid), ==, AS_VIDEO_CONTAINER_KIND_MKV);
	g_assert_cmpstr (as_video_get_url (vid), ==, "https://example.org/screencast.mkv");
	g_assert_cmpint (as_video_get_width (vid), ==, 1916);
	g_assert_cmpint (as_video_get_height (vid), ==, 1056);

	vid = AS_VIDEO (g_ptr_array_index (videos, 1));
	g_assert_cmpint (as_video_get_codec_kind (vid), ==, AS_VIDEO_CODEC_KIND_AV1);
	g_assert_cmpint (as_video_get_container_kind (vid), ==, AS_VIDEO_CONTAINER_KIND_MKV);
	g_assert_cmpstr (as_video_get_url (vid), ==, "https://example.org/screencast_de.mkv");
	g_assert_cmpint (as_video_get_width (vid), ==, 1916);
	g_assert_cmpint (as_video_get_height (vid), ==, 1056);
}

static const gchar *yamldata_releases_field =
				"Type: generic\n"
				"ID: org.example.ReleasesTest\n"
				"Releases:\n"
				"- version: '1.2'\n"
				"  type: stable\n"
				"  unix-timestamp: 1462288512\n"
				"  urgency: medium\n"
				"  description:\n"
				"    C: >-\n"
				"      <p>The CPU no longer overheats when you hold down spacebar.</p>\n"
				"  issues:\n"
				"  - id: bz#12345\n"
				"    url: https://example.com/bugzilla/12345\n"
				"  - type: cve\n"
				"    id: CVE-2019-123456\n"
				"  artifacts:\n"
				"  - type: source\n"
				"    bundle: tarball\n"
				"    locations:\n"
				"    - https://example.com/source.tar.xz\n"
				"    checksum:\n"
				"      blake2b: 8b28f613fa1ccdb1d303704839a0bb196424f425badfa4e4f43808f6812b6bcc0ae43374383bb6e46294d08155a64acbad92084387c73f696f00368ea106ebb4\n"
				"    size: {}\n"
				"  - type: binary\n"
				"    platform: x86_64-linux-gnu\n"
				"    bundle: flatpak\n"
				"    locations:\n"
				"    - https://example.com/binary_amd64.flatpak\n"
				"    filename: binary-1.2.0_amd64.flatpak\n"
				"    checksum:\n"
				"      blake2b: 04839a\n"
				"    size:\n"
				"      download: 24084\n"
				"      installed: 42052\n"
				"- version: '1.0'\n"
				"  type: development\n"
				"  unix-timestamp: 1460463132\n"
				"  description:\n"
				"    de_DE: >-\n"
				"      <p>Großartige erste Veröffentlichung.</p>\n"
				"\n"
				"      <p>Zweite zeile.</p>\n"
				"    C: >-\n"
				"      <p>Awesome initial release.</p>\n"
				"\n"
				"      <p>Second paragraph.</p>\n"
				"  url:\n"
				"    details: https://example.org/releases/1.0.html\n";

/**
 * test_yaml_write_releases:
 *
 * Test writing the Releases field.
 */
static void
test_yaml_write_releases (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsRelease) rel1 = NULL;
	g_autoptr(AsRelease) rel2 = NULL;
	g_autoptr(AsArtifact) af1 = NULL;
	g_autoptr(AsArtifact) af2 = NULL;
	AsIssue *issue = NULL;
	AsChecksum *cs = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.ReleasesTest");

	rel1 = as_release_new ();
	as_release_set_version (rel1, "1.0");
	as_release_set_kind (rel1, AS_RELEASE_KIND_DEVELOPMENT);
	as_release_set_timestamp (rel1, 1460463132);
	as_release_set_description (rel1, "<p>Awesome initial release.</p>\n<p>Second paragraph.</p>", "C");
	as_release_set_description (rel1, "<p>Großartige erste Veröffentlichung.</p>\n<p>Zweite zeile.</p>", "de_DE");
	as_release_set_url (rel1, AS_RELEASE_URL_KIND_DETAILS, "https://example.org/releases/1.0.html");
	as_component_add_release (cpt, rel1);

	rel2 = as_release_new ();
	as_release_set_version (rel2, "1.2");
	as_release_set_timestamp (rel2, 1462288512);
	as_release_set_description (rel2, "<p>The CPU no longer overheats when you hold down spacebar.</p>", "C");
	as_release_set_urgency (rel2, AS_URGENCY_KIND_MEDIUM);
	as_component_add_release (cpt, rel2);

	/* issues */
	issue = as_issue_new ();
	as_issue_set_id (issue, "bz#12345");
	as_issue_set_url (issue, "https://example.com/bugzilla/12345");
	as_release_add_issue (rel2, issue);
	g_object_unref (issue);

	issue = as_issue_new ();
	as_issue_set_kind (issue, AS_ISSUE_KIND_CVE);
	as_issue_set_id (issue, "CVE-2019-123456");
	as_release_add_issue (rel2, issue);
	g_object_unref (issue);

	/* artifacts */
	af1 = as_artifact_new ();
	as_artifact_set_kind (af1, AS_ARTIFACT_KIND_SOURCE);
	as_artifact_add_location (af1, "https://example.com/source.tar.xz");
	as_artifact_set_bundle_kind (af1, AS_BUNDLE_KIND_TARBALL);
	cs = as_checksum_new_for_kind_value (AS_CHECKSUM_KIND_BLAKE2B, "8b28f613fa1ccdb1d303704839a0bb196424f425badfa4e4f43808f6812b6bcc0ae43374383bb6e46294d08155a64acbad92084387c73f696f00368ea106ebb4");
	as_artifact_add_checksum (af1, cs);
	g_object_unref (cs);
	as_release_add_artifact (rel2, af1);

	af2 = as_artifact_new ();
	as_artifact_set_kind (af2, AS_ARTIFACT_KIND_BINARY);
	as_artifact_add_location (af2, "https://example.com/binary_amd64.flatpak");
	as_artifact_set_filename (af2, "binary-1.2.0_amd64.flatpak");
	as_artifact_set_bundle_kind (af2, AS_BUNDLE_KIND_FLATPAK);
	cs = as_checksum_new_for_kind_value (AS_CHECKSUM_KIND_BLAKE2B, "04839a");
	as_artifact_add_checksum (af2, cs);
	g_object_unref (cs);
	as_artifact_set_size (af2, 42052, AS_SIZE_KIND_INSTALLED);
	as_artifact_set_size (af2, 24084, AS_SIZE_KIND_DOWNLOAD);
	as_artifact_set_platform (af2, "x86_64-linux-gnu");
	as_release_add_artifact (rel2, af2);

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_releases_field));
}

/**
 * test_yaml_read_releases:
 *
 * Test if reading the Releases field works.
 */
static void
test_yaml_read_releases (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsRelease *rel;
	AsArtifact *af;
	AsChecksum *cs;
	GPtrArray *issues;
	GPtrArray *artifacts;

	cpt = as_yaml_test_read_data (yamldata_releases_field, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ReleasesTest");

	g_assert_cmpint (as_component_get_releases (cpt)->len, ==, 2);

	rel = AS_RELEASE (g_ptr_array_index (as_component_get_releases (cpt), 0));
	g_assert_cmpint (as_release_get_kind (rel), ==, AS_RELEASE_KIND_STABLE);
	g_assert_cmpstr (as_release_get_version (rel), ==,  "1.2");

	issues = as_release_get_issues (rel);
	g_assert_cmpint (issues->len, ==, 2);
	for (guint i = 0; i < issues->len; i++) {
		AsIssue *issue = AS_ISSUE (g_ptr_array_index (issues, i));

		if (as_issue_get_kind (issue) == AS_ISSUE_KIND_GENERIC) {
			g_assert_cmpstr (as_issue_get_id (issue), ==, "bz#12345");
			g_assert_cmpstr (as_issue_get_url (issue), ==, "https://example.com/bugzilla/12345");

		} else if (as_issue_get_kind (issue) == AS_ISSUE_KIND_CVE) {
			g_assert_cmpstr (as_issue_get_id (issue), ==, "CVE-2019-123456");
			g_assert_cmpstr (as_issue_get_url (issue), ==, "https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2019-123456");

		} else {
			g_assert_not_reached ();
		}
	}

	artifacts = as_release_get_artifacts (rel);
	g_assert_cmpint (artifacts->len, ==, 2);

	/* artifact 1 */
	af = AS_ARTIFACT (g_ptr_array_index (artifacts, 0));
	g_assert_cmpint (as_artifact_get_kind (af), ==, AS_ARTIFACT_KIND_SOURCE);
	g_assert_cmpstr (g_ptr_array_index (as_artifact_get_locations (af), 0), ==, "https://example.com/source.tar.xz");
	g_assert_cmpint (as_artifact_get_bundle_kind (af), ==, AS_BUNDLE_KIND_TARBALL);

	cs = as_artifact_get_checksum (af, AS_CHECKSUM_KIND_BLAKE2B);
	g_assert_nonnull (cs);
	g_assert_cmpstr (as_checksum_get_value (cs), ==, "8b28f613fa1ccdb1d303704839a0bb196424f425badfa4e4f43808f6812b6bcc0ae43374383bb6e46294d08155a64acbad92084387c73f696f00368ea106ebb4");

	/* artifact 2 */
	af = AS_ARTIFACT (g_ptr_array_index (artifacts, 1));
	g_assert_cmpint (as_artifact_get_kind (af), ==, AS_ARTIFACT_KIND_BINARY);
	g_assert_cmpstr (g_ptr_array_index (as_artifact_get_locations (af), 0), ==, "https://example.com/binary_amd64.flatpak");
	g_assert_cmpstr (as_artifact_get_filename (af), ==, "binary-1.2.0_amd64.flatpak");
	g_assert_cmpint (as_artifact_get_bundle_kind (af), ==, AS_BUNDLE_KIND_FLATPAK);

	cs = as_artifact_get_checksum (af, AS_CHECKSUM_KIND_BLAKE2B);
	g_assert_nonnull (cs);
	g_assert_cmpstr (as_checksum_get_value (cs), ==, "04839a");

	g_assert_cmpint (as_artifact_get_size (af, AS_SIZE_KIND_INSTALLED), ==, 42052);
	g_assert_cmpint (as_artifact_get_size (af, AS_SIZE_KIND_DOWNLOAD), ==, 24084);

	g_assert_cmpstr (as_artifact_get_platform (af), ==, "x86_64-linux-gnu");
}

/**
 * test_yaml_rw_tags:
 */
static void
test_yaml_rw_tags (void)
{
	static const gchar *yamldata_tags =
			"Type: generic\n"
			"ID: org.example.TagsTest\n"
			"Tags:\n"
			"- namespace: lvfs\n"
			"  tag: vendor-2021q1\n"
			"- namespace: plasma\n"
			"  tag: featured\n";
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;

	/* read */
	cpt = as_yaml_test_read_data (yamldata_tags, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.TagsTest");

	/* validate */
	g_assert_true (as_component_has_tag (cpt, "lvfs", "vendor-2021q1"));
	g_assert_true (as_component_has_tag (cpt, "plasma", "featured"));

	/* write */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_tags));
}

/**
 * test_yaml_rw_branding:
 */
static void
test_yaml_rw_branding (void)
{
	static const gchar *yamldata_tags =
			"Type: generic\n"
			"ID: org.example.BrandingTest\n"
			"Branding:\n"
			"  colors:\n"
			"  - type: primary\n"
			"    scheme-preference: light\n"
			"    value: '#ff00ff'\n"
			"  - type: primary\n"
			"    scheme-preference: dark\n"
			"    value: '#993d3d'\n";
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	AsBranding *branding;

	/* read */
	cpt = as_yaml_test_read_data (yamldata_tags, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.BrandingTest");

	/* validate */
	branding = as_component_get_branding (cpt);
	g_assert_nonnull (branding);

	g_assert_cmpstr (as_branding_get_color (branding, AS_COLOR_KIND_PRIMARY, AS_COLOR_SCHEME_KIND_LIGHT),
			 ==, "#ff00ff");
	g_assert_cmpstr (as_branding_get_color (branding, AS_COLOR_KIND_PRIMARY, AS_COLOR_SCHEME_KIND_DARK),
			 ==, "#993d3d");

	/* write */
	res = as_yaml_test_serialize (cpt);
	g_assert_true (as_yaml_test_compare_yaml (res, yamldata_tags));
}

/**
 * main:
 */
int
main (int argc, char **argv)
{
	int ret;

	if (argc == 0) {
		g_error ("No test directory specified!");
		return 1;
	}

	datadir = argv[1];
	g_assert_nonnull (datadir);
	datadir = g_build_filename (datadir, "samples", NULL);
	g_assert_true (g_file_test (datadir, G_FILE_TEST_EXISTS));

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/YAML/Basic", test_yaml_basic);
	g_test_add_func ("/YAML/Write/Misc", test_yamlwrite_misc);

	g_test_add_func ("/YAML/Read/CorruptData", test_yaml_corrupt_data);
	g_test_add_func ("/YAML/Read/Icons", test_yaml_read_icons);
	g_test_add_func ("/YAML/Read/Url", test_yaml_read_url);
	g_test_add_func ("/YAML/Read/Languages", test_yaml_read_languages);

	g_test_add_func ("/YAML/Read/Simple", test_yaml_read_simple);
	g_test_add_func ("/YAML/Write/Simple", test_yaml_write_simple);

	g_test_add_func ("/YAML/Read/Provides", test_yaml_read_provides);
	g_test_add_func ("/YAML/Write/Provides", test_yaml_write_provides);

	g_test_add_func ("/YAML/Read/Suggests", test_yaml_read_suggests);
	g_test_add_func ("/YAML/Write/Suggests", test_yaml_write_suggests);

	g_test_add_func ("/YAML/Read/Custom", test_yaml_read_custom);
	g_test_add_func ("/YAML/Write/Custom", test_yaml_write_custom);

	g_test_add_func ("/YAML/Read/ContentRating", test_yaml_read_content_rating);
	g_test_add_func ("/YAML/Write/ContentRating", test_yaml_write_content_rating);

	g_test_add_func ("/YAML/Read/Launchable", test_yaml_read_launchable);
	g_test_add_func ("/YAML/Write/Launchable", test_yaml_write_launchable);

	g_test_add_func ("/YAML/Read/Relations", test_yaml_read_relations);
	g_test_add_func ("/YAML/Write/Relations", test_yaml_write_relations);

	g_test_add_func ("/YAML/Read/Agreements", test_yaml_read_agreements);
	g_test_add_func ("/YAML/Write/Agreements", test_yaml_write_agreements);

	g_test_add_func ("/YAML/Read/Screenshots", test_yaml_read_screenshots);
	g_test_add_func ("/YAML/Write/Screenshots", test_yaml_write_screenshots);

	g_test_add_func ("/YAML/Read/Releases", test_yaml_read_releases);
	g_test_add_func ("/YAML/Write/Releases", test_yaml_write_releases);

	g_test_add_func ("/YAML/ReadWrite/Tags", test_yaml_rw_tags);
	g_test_add_func ("/YAML/ReadWrite/Branding", test_yaml_rw_branding);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
