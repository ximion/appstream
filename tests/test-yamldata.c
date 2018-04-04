/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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
 * test_basic:
 *
 * Test basic functions related to YAML processing.
 */
static void
test_basic (void)
{
	g_autoptr(AsMetadata) mdata = NULL;
	gchar *path;
	GFile *file;
	GPtrArray *cpts;
	guint i;
	AsComponent *cpt_tomatoes = NULL;
	GError *error = NULL;

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
	g_assert_cmpint (cpts->len, ==, 6);

	for (i = 0; i < cpts->len; i++) {
		AsComponent *cpt;
		cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));

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
 * as_yaml_test_serialize:
 *
 * Helper function for other tests.
 */
static gchar*
as_yaml_test_serialize (AsComponent *cpt)
{
	gchar *data;
	g_autoptr(AsMetadata) metad = NULL;
	GError *error = NULL;

	metad = as_metadata_new ();
	as_metadata_add_component (metad, cpt);
	as_metadata_set_write_header (metad, TRUE);

	data = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_YAML, &error);
	g_assert_no_error (error);

	return data;
}

/**
 * test_yamlwrite:
 *
 * Test writing a YAML document.
 */
static void
test_yamlwrite_general (void)
{
	guint i;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(AsScreenshot) scr = NULL;
	g_autoptr(AsRelease) rel1 = NULL;
	g_autoptr(AsRelease) rel2 = NULL;
	g_autoptr(AsBundle) bdl = NULL;
	g_autofree gchar *resdata = NULL;
	AsComponent *cpt = NULL;
	GError *error = NULL;
	gchar *_PKGNAME1[2] = {"fwdummy", NULL};
	gchar *_PKGNAME2[2] = {"foobar-pkg", NULL};

	const gchar *expected_yaml = "---\n"
				"File: DEP-11\n"
				"Version: '0.12'\n"
				"---\n"
				"Type: firmware\n"
				"ID: org.example.test.firmware\n"
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
				"- version: '1.2'\n"
				"  type: stable\n"
				"  unix-timestamp: 1462288512\n"
				"  urgency: medium\n"
				"  description:\n"
				"    C: >-\n"
				"      <p>The CPU no longer overheats when you hold down spacebar.</p>\n"
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
	as_component_add_release (cpt, rel1);

	rel2 = as_release_new ();
	as_release_set_version (rel2, "1.2");
	as_release_set_timestamp (rel2, 1462288512);
	as_release_set_description (rel2, "<p>The CPU no longer overheats when you hold down spacebar.</p>", "C");
	as_release_set_urgency (rel2, AS_URGENCY_KIND_MEDIUM);
	as_component_add_release (cpt, rel2);

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

	g_assert (as_test_compare_lines (resdata, expected_yaml));
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

	metad = as_metadata_new ();
	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_COLLECTION);

	if (error == NULL) {
		g_autoptr(GError) local_error = NULL;
		as_metadata_parse (metad, data, AS_FORMAT_KIND_YAML, &local_error);
		g_assert_no_error (local_error);

		g_assert_cmpint (as_metadata_get_components (metad)->len, >, 0);
		cpt = AS_COMPONENT (g_ptr_array_index (as_metadata_get_components (metad), 0));

		return g_object_ref (cpt);
	} else {
		as_metadata_parse (metad, data, AS_FORMAT_KIND_YAML, error);

		if (as_metadata_get_components (metad)->len > 0) {
			cpt = AS_COMPONENT (g_ptr_array_index (as_metadata_get_components (metad), 0));
			return g_object_ref (cpt);
		} else {
			return NULL;
		}
	}
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

	const gchar *yamldata_icons_legacy = "---\n"
					"ID: org.example.Test\n"
					"Icon:\n"
					"  cached: test_test.png\n"
					"  stock: test\n";
	const gchar *yamldata_icons_current = "---\n"
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
	const gchar *yamldata_icons_single = "---\n"
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
	const gchar *yamldata_languages = "---\n"
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
	const gchar *yamldata_urls = "---\n"
				     "ID: org.example.Test\n"
				     "Url:\n"
				     "  homepage: https://example.org\n"
				     "  faq: https://example.org/faq\n"
				     "  donation: https://example.org/donate\n";

	cpt = as_yaml_test_read_data (yamldata_urls, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.Test");

	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "https://example.org");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_FAQ), ==, "https://example.org/faq");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_DONATION), ==, "https://example.org/donate");
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
	const gchar *yamldata_corrupt = "---\n"
					"ID: org.example.Test\n"
					"\007\n";

	cpt = as_yaml_test_read_data (yamldata_corrupt, &error);

	g_assert_error (error, AS_METADATA_ERROR, AS_METADATA_ERROR_PARSE);
	g_assert_null (cpt);
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
	const gchar *expected_sug_yaml = "---\n"
					 "File: DEP-11\n"
					 "Version: '0.12'\n"
					 "---\n"
					 "Type: generic\n"
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
	g_assert (as_test_compare_lines (res, expected_sug_yaml));
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
	const gchar *yamldata_suggests = "---\n"
					 "ID: org.example.Test\n"
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
	g_assert (as_suggested_get_kind (sug) == AS_SUGGESTED_KIND_UPSTREAM);
	cpt_ids = as_suggested_get_ids (sug);
	g_assert_cmpint (cpt_ids->len, ==, 3);

	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 0), ==, "org.example.Awesome");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 1), ==, "org.example.test1");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 2), ==, "org.example.test2");

	sug = AS_SUGGESTED (g_ptr_array_index (suggestions, 1));
	g_assert (as_suggested_get_kind (sug) == AS_SUGGESTED_KIND_HEURISTIC);
	cpt_ids = as_suggested_get_ids (sug);
	g_assert_cmpint (cpt_ids->len, ==, 1);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 0), ==, "org.example.test3");
}

static const gchar *yamldata_custom_field = "---\n"
					 "File: DEP-11\n"
					 "Version: '0.12'\n"
					 "---\n"
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
	g_assert (as_test_compare_lines (res, yamldata_custom_field));
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

static const gchar *yamldata_content_rating_field = "---\n"
						"File: DEP-11\n"
						"Version: '0.12'\n"
						"---\n"
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
	g_assert (as_test_compare_lines (res, yamldata_content_rating_field));
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
}

static const gchar *yamldata_launchable_field = "---\n"
						"File: DEP-11\n"
						"Version: '0.12'\n"
						"---\n"
						"Type: generic\n"
						"ID: org.example.LaunchTest\n"
						"Launchable:\n"
						"  desktop-id:\n"
						"  - org.example.Test.desktop\n"
						"  - kde4-kool.desktop\n";

/**
 * test_yaml_write_launchable:
 *
 * Test writing the Launch field.
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
	g_assert (as_test_compare_lines (res, yamldata_launchable_field));
}

/**
 * test_yaml_read_launchable:
 *
 * Test if reading the Launch field works.
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

static const gchar *yamldata_requires_recommends_field = "---\n"
						"File: DEP-11\n"
						"Version: '0.12'\n"
						"---\n"
						"Type: generic\n"
						"ID: org.example.RelationsTest\n"
						"Recommends:\n"
						"- memory: '2500'\n"
						"- modalias: usb:v1130p0202d*\n"
						"Requires:\n"
						"- kernel: Linux\n"
						"  version: '>= 4.15'\n"
						"- id: org.example.TestDependency\n"
						"  version: == 1.2\n";

/**
 * test_yaml_write_requires_recommends:
 *
 * Test writing the Requires/Recommends fields.
 */
static void
test_yaml_write_requires_recommends (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	g_autoptr(AsRelation) mem_relation = NULL;
	g_autoptr(AsRelation) moda_relation = NULL;
	g_autoptr(AsRelation) kernel_relation = NULL;
	g_autoptr(AsRelation) id_relation = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.RelationsTest");

	mem_relation = as_relation_new ();
	moda_relation = as_relation_new ();
	kernel_relation = as_relation_new ();
	id_relation = as_relation_new ();

	as_relation_set_kind (mem_relation, AS_RELATION_KIND_RECOMMENDS);
	as_relation_set_kind (moda_relation, AS_RELATION_KIND_RECOMMENDS);
	as_relation_set_kind (kernel_relation, AS_RELATION_KIND_REQUIRES);
	as_relation_set_kind (id_relation, AS_RELATION_KIND_REQUIRES);

	as_relation_set_item_kind (mem_relation, AS_RELATION_ITEM_KIND_MEMORY);
	as_relation_set_value (mem_relation, "2500");
	as_relation_set_item_kind (moda_relation, AS_RELATION_ITEM_KIND_MODALIAS);
	as_relation_set_value (moda_relation, "usb:v1130p0202d*");

	as_relation_set_item_kind (kernel_relation, AS_RELATION_ITEM_KIND_KERNEL);
	as_relation_set_value (kernel_relation, "Linux");
	as_relation_set_version (kernel_relation, "4.15");
	as_relation_set_compare (kernel_relation, AS_RELATION_COMPARE_GE);

	as_relation_set_item_kind (id_relation, AS_RELATION_ITEM_KIND_ID);
	as_relation_set_value (id_relation, "org.example.TestDependency");
	as_relation_set_version (id_relation, "1.2");
	as_relation_set_compare (id_relation, AS_RELATION_COMPARE_EQ);

	as_component_add_relation (cpt, mem_relation);
	as_component_add_relation (cpt, moda_relation);
	as_component_add_relation (cpt, kernel_relation);
	as_component_add_relation (cpt, id_relation);

	/* test collection serialization */
	res = as_yaml_test_serialize (cpt);
	g_assert (as_test_compare_lines (res, yamldata_requires_recommends_field));
}

/**
 * test_yaml_read_requires_recommends:
 *
 * Test if reading the Requires/Recommends fields works.
 */
static void
test_yaml_read_requires_recommends (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *recommends;
	GPtrArray *requires;
	AsRelation *relation;

	cpt = as_yaml_test_read_data (yamldata_requires_recommends_field, NULL);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.RelationsTest");

	recommends = as_component_get_recommends (cpt);
	requires = as_component_get_requires (cpt);

	g_assert_cmpint (recommends->len, ==, 2);
	g_assert_cmpint (requires->len, ==, 2);

	/* memory relation */
	relation = AS_RELATION (g_ptr_array_index (recommends, 0));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_RECOMMENDS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_MEMORY);
	g_assert_cmpint (as_relation_get_value_int (relation), ==, 2500);

	/* modalias relation */
	relation = AS_RELATION (g_ptr_array_index (recommends, 1));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_RECOMMENDS);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_MODALIAS);
	g_assert_cmpstr (as_relation_get_value (relation), ==, "usb:v1130p0202d*");

	/* kernel relation */
	relation = AS_RELATION (g_ptr_array_index (requires, 0));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_REQUIRES);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_KERNEL);
	g_assert_cmpstr (as_relation_get_value (relation), ==, "Linux");
	g_assert_cmpstr (as_relation_get_version (relation), ==, "4.15");
	g_assert_cmpint (as_relation_get_compare (relation), ==, AS_RELATION_COMPARE_GE);

	/* ID relation */
	relation = AS_RELATION (g_ptr_array_index (requires, 1));
	g_assert_cmpint (as_relation_get_kind (relation), ==, AS_RELATION_KIND_REQUIRES);
	g_assert_cmpint (as_relation_get_item_kind (relation), ==, AS_RELATION_ITEM_KIND_ID);
	g_assert_cmpstr (as_relation_get_value (relation), ==, "org.example.TestDependency");
	g_assert_cmpstr (as_relation_get_version (relation), ==, "1.2");
	g_assert_cmpint (as_relation_get_compare (relation), ==, AS_RELATION_COMPARE_EQ);
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
	g_assert (datadir != NULL);
	datadir = g_build_filename (datadir, "samples", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/YAML/Basic", test_basic);
	g_test_add_func ("/YAML/Write/General", test_yamlwrite_general);

	g_test_add_func ("/YAML/Read/CorruptData", test_yaml_corrupt_data);
	g_test_add_func ("/YAML/Read/Icons", test_yaml_read_icons);
	g_test_add_func ("/YAML/Read/Url", test_yaml_read_url);
	g_test_add_func ("/YAML/Read/Languages", test_yaml_read_languages);

	g_test_add_func ("/YAML/Read/Suggests", test_yaml_read_suggests);
	g_test_add_func ("/YAML/Write/Suggests", test_yaml_write_suggests);

	g_test_add_func ("/YAML/Read/Custom", test_yaml_read_custom);
	g_test_add_func ("/YAML/Write/Custom", test_yaml_write_custom);

	g_test_add_func ("/YAML/Read/ContentRating", test_yaml_read_content_rating);
	g_test_add_func ("/YAML/Write/ContentRating", test_yaml_write_content_rating);

	g_test_add_func ("/YAML/Read/Launchable", test_yaml_read_launchable);
	g_test_add_func ("/YAML/Write/Launchable", test_yaml_write_launchable);

	g_test_add_func ("/YAML/Read/RequiresRecommends", test_yaml_read_requires_recommends);
	g_test_add_func ("/YAML/Write/RequiresRecommends", test_yaml_write_requires_recommends);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
