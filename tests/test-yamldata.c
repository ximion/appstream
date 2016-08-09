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
#include "as-yamldata.h"

static gchar *datadir = NULL;

/**
 * test_basic:
 *
 * Test basic functions related to YAML processing.
 */
void
test_basic (void)
{
	g_autoptr(AsMetadata) mdata = NULL;
	gchar *path;
	GFile *file;
	GPtrArray *cpts;
	guint i;
	AsComponent *cpt_tomatoes;
	GError *error = NULL;

	mdata = as_metadata_new ();
	as_metadata_set_locale (mdata, "C");
	as_metadata_set_parser_mode (mdata, AS_PARSER_MODE_DISTRO);

	path = g_build_filename (datadir, "dep11-0.8.yml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_metadata_parse_file (mdata, file, &error);
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
	g_assert (cpt_tomatoes != NULL);
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
void
test_yamlwrite (void)
{
	guint i;
	g_autoptr(AsYAMLData) ydata = NULL;
	g_autoptr(GPtrArray) cpts = NULL;
	g_autoptr(AsScreenshot) scr = NULL;
	g_autoptr(AsRelease) rel1 = NULL;
	g_autoptr(AsRelease) rel2 = NULL;
	g_autofree gchar *resdata = NULL;
	AsComponent *cpt = NULL;
	GError *error = NULL;
	gchar *_PKGNAME1[2] = {"fwdummy", NULL};
	gchar *_PKGNAME2[2] = {"foobar-pkg", NULL};

	ydata = as_yamldata_new ();
	cpts = g_ptr_array_new_with_free_func (g_object_unref);

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
	g_ptr_array_add (cpts, cpt);

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

	g_ptr_array_add (cpts, cpt);

	resdata = as_yamldata_serialize_to_distro (ydata, cpts, TRUE, FALSE, &error);
	g_assert_no_error (error);
	g_debug ("%s", resdata);

	/* TODO: Actually test the resulting output */
}

/**
 * as_yaml_test_read_data:
 *
 * Helper function to read a single component from YAML data.
 */
AsComponent*
as_yaml_test_read_data (const gchar *data, GError **error)
{
	AsComponent *cpt;
	g_autoptr(GPtrArray) cpts = NULL;
	g_autoptr(AsYAMLData) ydt = NULL;

	ydt = as_yamldata_new ();

	if (error == NULL) {
		g_autoptr(GError) local_error = NULL;

		cpts = as_yamldata_parse_distro_data (ydt, data, &local_error);
		g_assert_no_error (local_error);
		g_assert_nonnull (cpts);
	} else {
		cpts = as_yamldata_parse_distro_data (ydt, data, error);
		if (cpts == NULL)
			return NULL;
	}

	cpt = AS_COMPONENT (g_ptr_array_index (cpts, 0));
	return g_object_ref (cpt);
}

/**
 * test_yaml_read_icons:
 *
 * Test reading the Icons field.
 */
void
test_yaml_read_icons (void)
{
	guint i;
	GPtrArray *icons;
	g_autoptr(AsComponent) cpt = NULL;

	g_autoptr(AsYAMLData) ydt = NULL;
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
					"    - width: 128\n"
					"      height: 128\n"
					"      name: test_test.png\n"
					"  stock: test\n";

	ydt = as_yamldata_new ();

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
	g_assert_cmpint (icons->len, ==, 3);
	for (i = 0; i < icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		if (as_icon_get_kind (icon) == AS_ICON_KIND_CACHED)
			g_assert_cmpstr (as_icon_get_filename (icon), ==, "test_test.png");
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK)
			g_assert_cmpstr (as_icon_get_name (icon), ==, "test");
	}

	g_assert_nonnull (as_component_get_icon_by_size (cpt, 64, 64));
	g_assert_nonnull (as_component_get_icon_by_size (cpt, 128, 128));
}

/**
 * test_yaml_read_languages:
 *
 * Test if reading the Languages field works.
 */
void
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
 * test_yaml_corrupt_data:
 *
 * Test reading of a broken YAML document.
 */
void
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
	g_test_add_func ("/YAML/Write", test_yamlwrite);
	g_test_add_func ("/YAML/Read/Icons", test_yaml_read_icons);
	g_test_add_func ("/YAML/Read/Languages", test_yaml_read_languages);
	g_test_add_func ("/YAML/Read/CorruptData", test_yaml_corrupt_data);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
