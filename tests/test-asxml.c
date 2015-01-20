/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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
#include "data-providers/appstream-xml.h"
#include "as-component-private.h"

static gchar *datadir = NULL;

void
msg (const gchar *s)
{
	g_printf ("%s\n", s);
}

void
test_appstream_xml_provider ()
{
	AsProviderXML *asxml;
	GFile *file;
	gchar *path;
	asxml = as_provider_xml_new ();

	path = g_build_filename (datadir, "appstream-dxml.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_provider_xml_process_file (asxml, file);
	g_object_unref (file);

	path = g_build_filename (datadir, "appstream-dxml.xml.gz", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_provider_xml_process_compressed_file (asxml, file);
	g_object_unref (file);
	g_object_unref (asxml);
}

static AsComponent *found_cpt;
void
test_cptprov_cb (gpointer sender, AsComponent* cpt, gpointer user_data)
{
	g_return_if_fail (cpt != NULL);

	if (found_cpt != NULL)
		g_object_unref (found_cpt);
	found_cpt = g_object_ref (cpt);
}

void
test_screenshot_handling ()
{
	AsProviderXML *asxml;
	AsComponent *cpt;
	GFile *file;
	gchar *path;
	gchar *xml_data;
	GPtrArray *screenshots;
	guint i;

	asxml = as_provider_xml_new ();

	found_cpt = NULL;
	g_signal_connect_object (asxml, "component", (GCallback) test_cptprov_cb, 0, 0);

	path = g_build_filename (datadir, "appstream-dxml.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_provider_xml_process_file (asxml, file);
	g_object_unref (file);
	cpt = found_cpt;
	g_assert (cpt != NULL);

	xml_data = as_component_dump_screenshot_data_xml (cpt);
	g_debug ("%s", xml_data);

	// dirty...
	g_debug ("%s", as_component_to_string (cpt));
	screenshots = as_component_get_screenshots (cpt);
	g_assert (screenshots->len > 0);
	g_ptr_array_remove_range (screenshots, 0, screenshots->len);

	as_component_load_screenshots_from_internal_xml (cpt, xml_data);
	g_assert (screenshots->len > 0);

	for (i = 0; i < screenshots->len; i++) {
		GPtrArray *imgs;
		AsScreenshot *sshot = (AsScreenshot*) g_ptr_array_index (screenshots, i);

		imgs = as_screenshot_get_images (sshot);
		g_assert (imgs->len == 2);
		g_debug ("%s", as_screenshot_get_caption (sshot));
	}
	g_object_unref (cpt);
	g_object_unref (asxml);
}

void
test_appstream_parser_legacy ()
{
	AsMetadata *metad;
	GFile *file;
	gchar *path;
	AsComponent *cpt;
	GError *error = NULL;

	metad = as_metadata_new ();

	path = g_build_filename (datadir, "appdata-legacy.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_metadata_parse_file (metad, file, &error);
	cpt = as_metadata_get_component (metad);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (cpt != NULL);

	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Application manager for GNOME");
	g_assert (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP);

	g_object_unref (metad);
}

void
test_appstream_parser_locale ()
{
	AsMetadata *metad;
	GFile *file;
	gchar *path;
	AsComponent *cpt;
	GError *error = NULL;

	metad = as_metadata_new ();

	path = g_build_filename (datadir, "appdata.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	/* check german only locale */
	as_metadata_set_locale (metad, "de_DE");
	as_metadata_parse_file (metad, file, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);
	g_assert (cpt != NULL);

	g_assert (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP);
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Feuerfuchs");
	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");
	/* no french, so fallback */
	as_component_set_active_locale (cpt, "fr_FR");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");

	/* check all locale */
	as_metadata_set_locale (metad, "ALL");
	as_metadata_parse_file (metad, file, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);

	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");
	as_component_set_active_locale (cpt, "de_DE");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Feuerfuchs");
	/* no french, so fallback */
	as_component_set_active_locale (cpt, "fr_FR");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefoux");

	g_object_unref (file);
	g_object_unref (metad);
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
	datadir = g_build_filename (datadir, "data", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/AppStream/ASXMLProvider", test_appstream_xml_provider);
	g_test_add_func ("/AppStream/Screenshots{dbimexport}", test_screenshot_handling);
	g_test_add_func ("/AppStream/LegacyData", test_appstream_parser_legacy);
	g_test_add_func ("/AppStream/XMLParserLocale", test_appstream_parser_locale);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
