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
#include "as-xmldata.h"
#include "as-component-private.h"

static gchar *datadir = NULL;

void
msg (const gchar *s)
{
	g_printf ("%s\n", s);
}

void
test_screenshot_handling ()
{
	AsMetadata *metad;
	GError *error = NULL;
	AsComponent *cpt;
	GFile *file;
	gchar *path;
	GPtrArray *screenshots;
	guint i;

	metad = as_metadata_new ();
	as_metadata_set_parser_mode (metad, AS_PARSER_MODE_DISTRO);

	path = g_build_filename (datadir, "appstream-dxml.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_metadata_parse_file (metad, file, &error);
	g_object_unref (file);
	g_assert_no_error (error);

	cpt = as_metadata_get_component (metad);
	g_assert (cpt != NULL);

	// dirty...
	g_debug ("%s", as_component_to_string (cpt));
	screenshots = as_component_get_screenshots (cpt);
	g_assert_cmpint (screenshots->len, >, 0);

	for (i = 0; i < screenshots->len; i++) {
		GPtrArray *imgs;
		AsScreenshot *sshot = (AsScreenshot*) g_ptr_array_index (screenshots, i);

		imgs = as_screenshot_get_images (sshot);
		g_assert_cmpint (imgs->len, ==, 2);
		g_debug ("%s", as_screenshot_get_caption (sshot));
	}

	g_object_unref (metad);
}

void
test_appstream_parser_legacy ()
{
	AsMetadata *metad;
	GFile *file;
	gchar *path;
	AsComponent *cpt;
	GPtrArray *screenshots;
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

	screenshots = as_component_get_screenshots (cpt);
	g_assert_cmpint (screenshots->len, ==, 5);

	g_object_unref (metad);
}

void
test_appstream_parser_locale ()
{
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GFile) file = NULL;
	gchar *path;
	AsComponent *cpt;

	GPtrArray *trs;
	AsTranslation *tr;

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
	as_metadata_clear_components (metad);
	as_metadata_set_locale (metad, "ALL");
	as_metadata_parse_file (metad, file, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);

	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");
	as_component_set_active_locale (cpt, "de_DE");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Feuerfuchs");
	/* no french, so fallback */
	as_component_set_active_locale (cpt, "fr_FR");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefoux");

	/* check if reading <translation/> tag succeeded */
	trs = as_component_get_translations (cpt);
	g_assert_cmpint (trs->len, ==, 1);
	tr = AS_TRANSLATION (g_ptr_array_index (trs, 0));
	g_assert_cmpstr (as_translation_get_id (tr), ==, "firefox");
}

void
test_appstream_write_locale ()
{
	AsMetadata *metad;
	GFile *file;
	gchar *tmp;
	AsComponent *cpt;
	GError *error = NULL;

	metad = as_metadata_new ();

	tmp = g_build_filename (datadir, "appdata.xml", NULL);
	file = g_file_new_for_path (tmp);
	g_free (tmp);

	as_metadata_set_locale (metad, "ALL");
	as_metadata_parse_file (metad, file, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);
	g_assert (cpt != NULL);
	g_object_unref (file);

	tmp = as_metadata_component_to_upstream_xml (metad);
	//g_debug ("Generated XML: %s", as_metadata_component_to_upstream_xml (metad));
	g_free (tmp);

	g_object_unref (metad);
}

void
test_appstream_write_description ()
{
	AsMetadata *metad;
	gchar *tmp;
	AsRelease *rel;
	AsComponent *cpt;

	const gchar *EXPECTED_XML = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				    "<component>\n"
				    "  <name>Test</name>\n"
				    "  <description><p>First paragraph</p>\n"
				    "<ol><li>One</li><li>Two</li><li>Three</li></ol>\n"
				    "<p>Paragraph2</p><ul><li>First</li><li>Second</li></ul><p>Paragraph3</p></description>\n"
				    "  <releases>\n"
				    "    <release version=\"1.0\" date=\"2016-04-11T22:00:00Z\"><description/></release>\n"
				    "  </releases>\n"
				    "</component>\n";

	const gchar *EXPECTED_XML_LOCALIZED = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					      "<component>\n"
					      "  <name>Test</name>\n"
					      "  <description><p>First paragraph</p>\n"
					      "<ol><li>One</li><li>Two</li><li>Three</li></ol>\n"
					      "<p>Paragraph2</p><ul><li>First</li><li>Second</li></ul><p>Paragraph3</p><p xml:lang=\"de\">First paragraph</p>\n"
					      "<ol><li xml:lang=\"de\">One</li><li xml:lang=\"de\">Two</li><li xml:lang=\"de\">Three</li></ol><ul>"
					      "<li xml:lang=\"de\">First</li><li xml:lang=\"de\">Second</li></ul><p xml:lang=\"de\">Paragraph2</p></description>\n"
					      "  <releases>\n"
					      "    <release version=\"1.0\" date=\"2016-04-11T22:00:00Z\"><description/></release>\n"
					      "  </releases>\n"
					      "</component>\n";

	const gchar *EXPECTED_XML_DISTRO = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					   "<components version=\"0.8\">\n"
					   "  <component>\n"
					   "    <name>Test</name>\n"
					   "    <description><p>First paragraph</p>\n"
					   "<ol><li>One</li><li>Two</li><li>Three</li></ol>\n"
					   "<p>Paragraph2</p><ul><li>First</li><li>Second</li></ul><p>Paragraph3</p></description>\n"
					   "    <description xml:lang=\"de\"><p>First paragraph</p>\n"
					   "<ol><li>One</li><li>Two</li><li>Three</li></ol><ul><li>First</li><li>Second</li></ul><p>Paragraph2</p></description>\n"
					   "    <releases>\n"
					   "      <release version=\"1.0\" timestamp=\"1460412000\"><description/></release>\n"
					   "    </releases>\n"
					   "  </component>\n"
					   "</components>\n";

	metad = as_metadata_new ();

	cpt = as_component_new ();
	as_component_set_name (cpt, "Test", NULL);
	as_component_set_description (cpt,
				"<p>First paragraph</p>\n<ol><li>One</li><li>Two</li><li>Three</li></ol>\n<p>Paragraph2</p><ul><li>First</li><li>Second</li></ul><p>Paragraph3</p>",
				NULL);
	rel = as_release_new ();
	as_release_set_version (rel, "1.0");
	as_release_set_timestamp (rel, 1460412000);
	as_component_add_release (cpt, rel);
	g_object_unref (rel);

	as_metadata_add_component (metad, cpt);

	tmp = as_metadata_component_to_upstream_xml (metad);
	g_assert_cmpstr (tmp, ==, EXPECTED_XML);
	g_free (tmp);

	/* add localization */
	as_component_set_description (cpt,
				"<p>First paragraph</p>\n<ol><li>One</li><li>Two</li><li>Three</li></ol><ul><li>First</li><li>Second</li></ul><p>Paragraph2</p>",
				"de");

	tmp = as_metadata_component_to_upstream_xml (metad);
	g_assert_cmpstr (tmp, ==, EXPECTED_XML_LOCALIZED);
	g_free (tmp);

	tmp = as_metadata_components_to_distro_xml (metad);
	g_assert_cmpstr (tmp, ==, EXPECTED_XML_DISTRO);
	g_free (tmp);

	g_object_unref (metad);
	g_object_unref (cpt);
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

	g_test_add_func ("/AppStream/Screenshots{dbimexport}", test_screenshot_handling);
	g_test_add_func ("/AppStream/LegacyData", test_appstream_parser_legacy);
	g_test_add_func ("/AppStream/XMLParserLocale", test_appstream_parser_locale);
	g_test_add_func ("/AppStream/XMLWriterLocale", test_appstream_write_locale);
	g_test_add_func ("/AppStream/XMLWriterDescription", test_appstream_write_description);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
