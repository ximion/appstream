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

void
println (const gchar *s)
{
	g_printf ("%s\n", s);
}

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

void
test_yamlwrite (void)
{
	g_autoptr(AsYAMLData) ydata = NULL;
	g_autoptr(GPtrArray) cpts = NULL;
	g_autoptr(AsScreenshot) scr = NULL;
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
	scr = test_h_create_dummy_screenshot ();
	as_component_add_screenshot (cpt, scr);

	g_ptr_array_add (cpts, cpt);

	resdata = as_yamldata_serialize_to_distro (ydata, cpts, TRUE, FALSE, &error);
	g_assert_no_error (error);
	g_debug ("%s", resdata);

	/* TODO: Actually test the resulting output */
}

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

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
