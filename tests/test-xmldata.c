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
#include "as-xml.h"
#include "as-component-private.h"
#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * as_xml_test_read_data:
 *
 * Helper function for other tests.
 */
static AsComponent*
as_xml_test_read_data (const gchar *data, AsFormatStyle mode)
{
	AsComponent *cpt = NULL;
	GPtrArray *cpts;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *data_full = NULL;

	data_full = g_strdup_printf ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n%s", data);

	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "ALL");
	as_metadata_parse (metad, data_full, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);

	cpts = as_metadata_get_components (metad);
	g_assert_cmpint (cpts->len, >, 0);
	if (mode == AS_FORMAT_STYLE_METAINFO)
		g_assert_cmpint (cpts->len, ==, 1);
	cpt = AS_COMPONENT (g_ptr_array_index (cpts, 0));

	return g_object_ref (cpt);
}

/**
 * as_xml_test_serialize:
 *
 * Helper function for other tests.
 */
static gchar*
as_xml_test_serialize (AsComponent *cpt, AsFormatStyle mode)
{
	gchar *data;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;

	metad = as_metadata_new ();
	as_metadata_add_component (metad, cpt);

	if (mode == AS_FORMAT_STYLE_METAINFO) {
		data = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, &error);
		g_assert_no_error (error);
	} else {
		data = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, &error);
		g_assert_no_error (error);
	}

	return data;
}

/**
 * as_xml_test_compare_xml:
 *
 * Compare generated XML line-by-line, prefixing the generated
 * data with the XML preamble first.
 */
static gboolean
as_xml_test_compare_xml (const gchar *result, const gchar *expected)
{
	g_autofree gchar *expected_full = NULL;
	expected_full = g_strdup_printf ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n%s", expected);
	return as_test_compare_lines (result, expected_full);
}

/**
 * test_appstream_parser_legacy:
 *
 * Test parsing legacy metainfo files.
 */
static void
test_appstream_parser_legacy ()
{
	AsMetadata *metad;
	GFile *file;
	gchar *path;
	AsComponent *cpt;
	GPtrArray *screenshots;
	g_autoptr(GError) error = NULL;

	metad = as_metadata_new ();

	path = g_build_filename (datadir, "appdata-legacy.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	cpt = as_metadata_get_component (metad);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert_true (cpt != NULL);

	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Application manager for GNOME");
	g_assert_true (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP);

	screenshots = as_component_get_screenshots (cpt);
	g_assert_cmpint (screenshots->len, ==, 5);

	g_object_unref (metad);
}

/**
 * test_appstream_parser_locale:
 *
 * Test reading localized tags.
 */
static void
test_appstream_parser_locale ()
{
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GError) error = NULL;
	gchar *path;
	AsComponent *cpt;

	GPtrArray *trs;
	AsTranslation *tr;

	metad = as_metadata_new ();

	path = g_build_filename (datadir, "appdata.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	/* check german only locale */
	as_metadata_set_locale (metad, "de_DE");
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);
	g_assert_true (cpt != NULL);

	g_assert_true (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP);
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Feuerfuchs");
	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");
	/* no french, so fallback */
	as_component_set_active_locale (cpt, "fr_FR");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");

	/* check all locale */
	as_metadata_clear_components (metad);
	as_metadata_set_locale (metad, "ALL");
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);

	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");
	as_component_set_active_locale (cpt, "de_DE");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Feuerfuchs");
	/* no french, so fallback */
	as_component_set_active_locale (cpt, "fr_FR");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Renard de feu");

	/* check if reading <translation/> tag succeeded */
	trs = as_component_get_translations (cpt);
	g_assert_cmpint (trs->len, ==, 1);
	tr = AS_TRANSLATION (g_ptr_array_index (trs, 0));
	g_assert_cmpstr (as_translation_get_id (tr), ==, "firefox");
	g_assert_cmpstr (as_translation_get_source_locale (tr), ==, "de");

	/* check if we loaded the right amount of icons */
	g_assert_cmpint (as_component_get_icons (cpt)->len, ==, 2);
}

/**
 * test_appstream_write_locale:
 *
 * Test writing fully localized entries.
 */
static void
test_appstream_write_locale ()
{
	AsMetadata *metad;
	GFile *file;
	gchar *tmp;
	AsComponent *cpt;
	g_autoptr(GError) error = NULL;

	const gchar *EXPECTED_XML = "<component type=\"desktop-application\">\n"
				    "  <id>org.mozilla.firefox</id>\n"
				    "  <name>Firefox</name>\n"
				    "  <name xml:lang=\"de_DE\">Feuerfuchs</name>\n"
				    "  <name xml:lang=\"fr_FR\">Renard de feu</name>\n"
				    "  <summary>Web browser</summary>\n"
				    "  <summary xml:lang=\"fr_FR\">Navigateur web</summary>\n"
				    "  <pkgname>firefox-bin</pkgname>\n"
				    "  <icon type=\"stock\">web-browser</icon>\n"
				    "  <icon type=\"cached\" width=\"64\" height=\"64\">firefox_web-browser.png</icon>\n"
				    "  <url type=\"homepage\">http://www.mozilla.com</url>\n"
				    "  <categories>\n"
				    "    <category>network</category>\n"
				    "    <category>web</category>\n"
				    "  </categories>\n"
				    "  <provides>\n"
				    "    <mediatype>application/vnd.mozilla.xul+xml</mediatype>\n"
				    "    <mediatype>application/x-xpinstall</mediatype>\n"
				    "    <mediatype>application/xhtml+xml</mediatype>\n"
				    "    <mediatype>text/html</mediatype>\n"
				    "    <mediatype>text/mml</mediatype>\n"
				    "    <mediatype>text/xml</mediatype>\n"
				    "    <mediatype>x-scheme-handler/http</mediatype>\n"
				    "    <mediatype>x-scheme-handler/https</mediatype>\n"
				    "  </provides>\n"
				    "  <translation type=\"gettext\" source_locale=\"de\">firefox</translation>\n"
				    "  <keywords>\n"
				    "    <keyword>internet</keyword>\n"
				    "    <keyword>web</keyword>\n"
				    "    <keyword>browser</keyword>\n"
				    "    <keyword xml:lang=\"fr_FR\">navigateur</keyword>\n"
				    "  </keywords>\n"
				    "</component>\n";

	metad = as_metadata_new ();

	tmp = g_build_filename (datadir, "appdata.xml", NULL);
	file = g_file_new_for_path (tmp);
	g_free (tmp);

	as_metadata_set_locale (metad, "ALL");
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);
	g_assert_true (cpt != NULL);
	g_object_unref (file);

	as_component_sort_values (cpt);

	tmp = as_metadata_component_to_metainfo (metad,
						 AS_FORMAT_KIND_XML,
						 &error);
	g_assert_no_error (error);

	g_assert_true (as_xml_test_compare_xml (tmp, EXPECTED_XML));
	g_free (tmp);

	g_object_unref (metad);
}

/**
 * test_appstream_write_description:
 *
 * Test writing the description tag for catalog and metainfo XML.
 */
static void
test_appstream_write_description ()
{
	guint i;
	gchar *tmp;
	AsRelease *rel;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(AsComponent) cpt = NULL;

	const gchar *EXPECTED_XML = "<component>\n"
				    "  <id>org.example.Test</id>\n"
				    "  <name>Test</name>\n"
				    "  <summary>Just a unittest.</summary>\n"
				    "  <description>\n"
				    "    <p>First paragraph</p>\n"
				    "    <ol>\n"
				    "      <li>One</li>\n"
				    "      <li>Two</li>\n"
				    "      <li>Three is &gt; 2 &amp; 1</li>\n"
				    "    </ol>\n"
				    "    <p>Paragraph2</p>\n"
				    "    <ul>\n"
				    "      <li>First</li>\n"
				    "      <li>Second</li>\n"
				    "    </ul>\n"
				    "    <p>Paragraph3 &amp; the last one</p>\n"
				    "  </description>\n"
				    "  <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
				    "  <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
				    "  <icon type=\"stock\">xml-writetest</icon>\n"
				    "  <releases>\n"
				    "    <release type=\"stable\" version=\"1.0\" date=\"2016-04-11T22:00:00Z\"/>\n"
				    "  </releases>\n"
				    "</component>\n";

	const gchar *EXPECTED_XML_LOCALIZED =   "<component>\n"
						"  <id>org.example.Test</id>\n"
						"  <name>Test</name>\n"
						"  <summary>Just a unittest.</summary>\n"
						"  <summary xml:lang=\"de\">Nur ein Unittest.</summary>\n"
						"  <description>\n"
						"    <p>First paragraph</p>\n"
						"    <p xml:lang=\"de\">Erster paragraph</p>\n"
						"    <ol>\n"
						"      <li>One</li>\n"
						"      <li xml:lang=\"de\">Eins</li>\n"
						"      <li>Two</li>\n"
						"      <li xml:lang=\"de\">Zwei</li>\n"
						"      <li>Three is &gt; 2 &amp; 1</li>\n"
						"      <li xml:lang=\"de\">Drei</li>\n"
						"    </ol>\n"
						"    <p>Paragraph2</p>\n"
						"    <p xml:lang=\"de\">Zweiter Paragraph</p>\n"
						"    <ul>\n"
						"      <li>First</li>\n"
						"      <li xml:lang=\"de\">Erstens</li>\n"
						"      <li>Second</li>\n"
						"      <li xml:lang=\"de\">Zweitens</li>\n"
						"    </ul>\n"
						"    <p>Paragraph3 &amp; the last one</p>\n"
						"    <p xml:lang=\"de\">Paragraph3</p>\n"
						"  </description>\n"
						"  <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
						"  <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
						"  <icon type=\"stock\">xml-writetest</icon>\n"
						"  <releases>\n"
						"    <release type=\"stable\" version=\"1.0\" date=\"2016-04-11T22:00:00Z\"/>\n"
						"  </releases>\n"
						"</component>\n";

	const gchar *EXPECTED_XML_DISTRO = "<components version=\"0.14\">\n"
					   "  <component>\n"
					   "    <id>org.example.Test</id>\n"
					   "    <name>Test</name>\n"
					   "    <summary>Just a unittest.</summary>\n"
					   "    <summary xml:lang=\"de\">Nur ein Unittest.</summary>\n"
					   "    <description>\n"
					   "      <p>First paragraph</p>\n"
					   "      <ol>\n"
					   "        <li>One</li>\n"
					   "        <li>Two</li>\n"
					   "        <li>Three is &gt; 2 &amp; 1</li>\n"
					   "      </ol>\n"
					   "      <p>Paragraph2</p>\n"
					   "      <ul>\n"
					   "        <li>First</li>\n"
					   "        <li>Second</li>\n"
					   "      </ul>\n"
					   "      <p>Paragraph3 &amp; the last one</p>\n"
					   "    </description>\n"
					   "    <description xml:lang=\"de\">\n"
					   "      <p>Erster paragraph</p>\n"
					   "      <ol>\n"
					   "        <li>Eins</li>\n"
					   "        <li>Zwei</li>\n"
					   "        <li>Drei</li>\n"
					   "      </ol>\n"
					   "      <p>Zweiter Paragraph</p>\n"
					   "      <ul>\n"
					   "        <li>Erstens</li>\n"
					   "        <li>Zweitens</li>\n"
					   "      </ul>\n"
					   "      <p>Paragraph3</p>\n"
					   "    </description>\n"
					   "    <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
					   "    <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
					   "    <icon type=\"stock\">xml-writetest</icon>\n"
					   "    <releases>\n"
					   "      <release type=\"stable\" version=\"1.0\" timestamp=\"1460412000\"/>\n"
					   "    </releases>\n"
					   "  </component>\n"
					   "</components>\n";

	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "ALL");

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.Test");
	as_component_set_name (cpt, "Test", "C");
	as_component_set_summary (cpt, "Just a unittest.", "C");
	as_component_set_description (cpt,
				"<p>First paragraph</p>\n<ol><li>One</li><li>Two</li><li>Three is &gt; 2 &amp; 1</li></ol>\n<p>Paragraph2</p><ul><li>First</li><li>Second</li></ul><p>Paragraph3 &amp; the last one</p>",
				NULL);
	rel = as_release_new ();
	as_release_set_version (rel, "1.0");
	as_release_set_timestamp (rel, 1460412000);
	as_component_add_release (cpt, rel);
	g_object_unref (rel);

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
			as_icon_set_filename (icon, "xml-writetest");

		as_component_add_icon (cpt, icon);
	}

	as_metadata_add_component (metad, cpt);

	tmp = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert_true (as_xml_test_compare_xml (tmp, EXPECTED_XML));
	g_free (tmp);

	/* add localization */
	as_component_set_summary (cpt, "Nur ein Unittest.", "de");
	as_component_set_description (cpt,
				"<p>Erster paragraph</p>\n<ol><li>Eins</li><li>Zwei</li><li>Drei</li></ol><p>Zweiter Paragraph</p><ul><li>Erstens</li><li>Zweitens</li></ul><p>Paragraph3</p>",
				"de");

	/* test localization */
	tmp = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert_true (as_xml_test_compare_xml (tmp, EXPECTED_XML_LOCALIZED));
	g_free (tmp);

	/* test collection-xml conversion */
	tmp = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert_true (as_xml_test_compare_xml (tmp, EXPECTED_XML_DISTRO));
	g_free (tmp);

	/* test collection XMl -> metainfo XML */
	as_metadata_clear_components (metad);
	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_COLLECTION);
	as_metadata_parse (metad, EXPECTED_XML_DISTRO, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);
	tmp = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert_true (as_xml_test_compare_xml (tmp, EXPECTED_XML_LOCALIZED));
	g_free (tmp);

	/* test metainfo XMl -> collection XML */
	as_metadata_clear_components (metad);
	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_METAINFO);
	as_metadata_parse (metad, EXPECTED_XML_LOCALIZED, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);
	tmp = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert_true (as_xml_test_compare_xml (tmp, EXPECTED_XML_DISTRO));
	g_free (tmp);
}

/**
 * test_appstream_description_l10n_cleanup:
 */
static void
test_appstream_description_l10n_cleanup (void)
{
	const gchar *DESC_L10N_XML = "<component>\n"
				    "  <id>org.example.Test</id>\n"
				    "  <name>Test</name>\n"
				    "  <summary>Just a unittest.</summary>\n"
				    "  <description>\n"
				    "    <p>First paragraph</p>\n"
				    "    <p xml:lang=\"de\">Erster Absatz</p>\n"
				    "    <p>Second paragraph</p>\n"
				    "    <p xml:lang=\"de\">Zweiter Absatz</p>\n"
				    "    <p>Features:</p>\n"
				    "    <p xml:lang=\"ca\">Característiques:</p>\n"
				    "    <p xml:lang=\"cs\">Vlastnosti:</p>\n"
				    "    <p xml:lang=\"de\">Funktionen:</p>\n"
				    "    <p xml:lang=\"nn\">Funksjonar:</p>\n"
				    "    <ul>\n"
				    "      <li>Browse the maps clicking in a map division to see its name, capital and flag</li>\n"
				    "      <li xml:lang=\"ca\">Busqueu en els mapes fent clic a sobre d'una zona del mapa per a veure el seu nom, capital i bandera</li>\n"
				    "      <li xml:lang=\"de\">Landkarte erkunden, indem Sie in der Karte auf ein Land klicken und dessen Name, Hauptstadt und Flagge angezeigt wird</li>\n"
				    "      <li>The game tells you a map division name and you have to click on it</li>\n"
				    "      <li xml:lang=\"ca\">El joc mostra el nom d'una zona en el mapa i heu de fer clic sobre el lloc on està</li>\n"
				    "    </ul>\n"
				    "  </description>\n"
				    "</component>\n";
	const gchar *ENGLISH_DESC_TEXT = "<p>First paragraph</p>\n"
					 "<p>Second paragraph</p>\n"
					 "<p>Features:</p>\n"
					 "<ul>\n"
					 "  <li>Browse the maps clicking in a map division to see its name, capital and flag</li>\n"
					 "  <li>The game tells you a map division name and you have to click on it</li>\n"
					 "</ul>\n";
	g_autoptr(AsComponent) cpt = NULL;

	cpt = as_xml_test_read_data (DESC_L10N_XML, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.Test");

	g_assert_cmpstr (as_component_get_name (cpt), ==, "Test");
	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Just a unittest.");

	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_description (cpt), ==, ENGLISH_DESC_TEXT);
	as_component_set_active_locale (cpt, "de");
	g_assert_cmpstr (as_component_get_description (cpt), ==, "<p>Erster Absatz</p>\n"
								 "<p>Zweiter Absatz</p>\n"
								 "<p>Funktionen:</p>\n"
								 "<ul>\n"
								 "  <li>Landkarte erkunden, indem Sie in der Karte auf ein Land klicken und dessen Name, Hauptstadt und Flagge angezeigt wird</li>\n"
								 "</ul>\n");
	as_component_set_active_locale (cpt, "ca");
	g_assert_cmpstr (as_component_get_description (cpt), ==, "<p>Característiques:</p>\n"
								 "<ul>\n"
								 "  <li>Busqueu en els mapes fent clic a sobre d'una zona del mapa per a veure el seu nom, capital i bandera</li>\n"
								 "  <li>El joc mostra el nom d'una zona en el mapa i heu de fer clic sobre el lloc on està</li>\n"
								 "</ul>\n");

	/* not enough translation for these, we should have fallen back to English */
	as_component_set_active_locale (cpt, "cs");
	g_assert_cmpstr (as_component_get_description (cpt), ==, ENGLISH_DESC_TEXT);
	as_component_set_active_locale (cpt, "nn");
	g_assert_cmpstr (as_component_get_description (cpt), ==, ENGLISH_DESC_TEXT);
}

/**
 * test_appstream_read_description:
 *
 * Test reading the description tag.
 */
static void
test_appstream_read_description (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *xmldata_desc_mi1 = "<component>\n"
					"  <id>org.example.DescTestMI-1</id>\n"
					"  <description>\n"
					"    <p>Agenda is a simple, slick, <em>speedy</em> and no-nonsense task manager. Use it to keep track of the tasks that matter most.</p>\n"
					"    <p>This paragraph makes use of <code>code markup</code>.</p>\n"
					"    <ul>\n"
					"      <li>Blazingly <em>fast</em> and light</li>\n"
					"      <li>Remembers your list until you clear completed tasks</li>\n"
					"      <li>Some <code>code</code> item</li>\n"
					"      <li>...</li>\n"
					"    </ul>\n"
					"    <p>I dare you to find an easier, faster, more beautiful task manager for elementary OS.</p>\n"
					"  </description>\n"
					"</component>\n";

	const gchar *xmldata_desc_mi2 = "<component>\n"
					"  <id>org.example.DescTestMI-2</id>\n"
					"  <description>\n"
					"    <ul>\n"
					"      <li>I start with bullet points</li>\n"
					"      <li>Yes, this is allowed now</li>\n"
					"      <li>...</li>\n"
					"    </ul>\n"
					"    <p>Paragraph</p>\n"
					"  </description>\n"
					"</component>\n";

	cpt = as_xml_test_read_data (xmldata_desc_mi1, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.DescTestMI-1");

	g_assert_true (as_test_compare_lines (as_component_get_description (cpt),
					 "<p>Agenda is a simple, slick, <em>speedy</em> and no-nonsense task manager. Use it to keep track of the tasks that matter most.</p>\n"
					 "<p>This paragraph makes use of <code>code markup</code>.</p>\n"
					 "<ul>\n"
					 "  <li>Blazingly <em>fast</em> and light</li>\n"
					 "  <li>Remembers your list until you clear completed tasks</li>\n"
					 "  <li>Some <code>code</code> item</li>\n"
					 "  <li>...</li>\n"
					 "</ul>\n"
					 "<p>I dare you to find an easier, faster, more beautiful task manager for elementary OS.</p>\n"));

	g_object_unref (cpt);
	cpt = as_xml_test_read_data (xmldata_desc_mi2, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.DescTestMI-2");

	g_assert_cmpstr (as_component_get_description (cpt), ==, "<ul>\n"
								 "  <li>I start with bullet points</li>\n"
								 "  <li>Yes, this is allowed now</li>\n"
								 "  <li>...</li>\n"
								 "</ul>\n"
								 "<p>Paragraph</p>\n");
}

static const gchar *xmldata_simple =    "<component date_eol=\"2022-02-22T00:00:00Z\">\n"
					"  <id>org.example.SimpleTest</id>\n"
					"  <name>TestComponent</name>\n"
					"  <name_variant_suffix>Generic</name_variant_suffix>\n"
					"  <summary>Just part of an unittest</summary>\n"
					"</component>\n";

/**
 * test_xml_read_simple:
 *
 * Test reading basic tags
 */
static void
test_xml_read_simple (void)
{
	g_autoptr(AsComponent) cpt = NULL;

	cpt = as_xml_test_read_data (xmldata_simple, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.SimpleTest");

	g_assert_cmpstr (as_component_get_name (cpt), ==, "TestComponent");
	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Just part of an unittest");
	g_assert_cmpstr (as_component_get_name_variant_suffix (cpt), ==, "Generic");
	g_assert_cmpstr (as_component_get_date_eol (cpt), ==, "2022-02-22T00:00:00Z");
	g_assert_cmpint (as_component_get_timestamp_eol (cpt), ==, 1645488000);
}

/**
 * test_xml_write_simple:
 *
 * Test writing basic tags.
 */
static void
test_xml_write_simple (void)
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

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_simple));
}

/**
 * test_xml_read_url:
 *
 * Test reading the url tag.
 */
static void
test_xml_read_url (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *xmldata_languages = "<component>\n"
					 "  <id>org.example.UrlTest</id>\n"
					 "  <url type=\"homepage\">https://example.org</url>\n"
					 "  <url type=\"faq\">https://example.org/faq</url>\n"
					 "  <url type=\"donation\">https://example.org/donate</url>\n"
					 "  <url type=\"contact\">https://example.org/contact</url>\n"
					 "  <url type=\"vcs-browser\">https://example.org/source</url>\n"
					 "  <url type=\"contribute\">https://example.org/contribute</url>\n"
					 "</component>\n";

	cpt = as_xml_test_read_data (xmldata_languages, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.UrlTest");

	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "https://example.org");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_FAQ), ==, "https://example.org/faq");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_DONATION), ==, "https://example.org/donate");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_CONTACT), ==, "https://example.org/contact");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_VCS_BROWSER), ==, "https://example.org/source");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_CONTRIBUTE), ==, "https://example.org/contribute");
}

/**
 * test_xml_read_languages:
 *
 * Test reading the languages tag.
 */
static void
test_xml_read_languages (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *xmldata_languages = "<component>\n"
					 "  <id>org.example.LangTest</id>\n"
					 "  <languages>\n"
					 "    <lang percentage=\"48\">de_DE</lang>\n"
					 "    <lang percentage=\"100\">en_GB</lang>\n"
					 "  </languages>\n"
					 "</component>\n";

	cpt = as_xml_test_read_data (xmldata_languages, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.LangTest");

	g_assert_cmpint (as_component_get_language (cpt, "de_DE"), ==, 48);
	g_assert_cmpint (as_component_get_language (cpt, "en_GB"), ==, 100);
	g_assert_cmpint (as_component_get_language (cpt, "invalid_C"), ==, -1);
}

/**
 * test_xml_write_languages:
 *
 * Test writing the languages tag.
 */
static void
test_xml_write_languages (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_lang_xml = "<component>\n"
					 "  <id>org.example.LangTest</id>\n"
					 "  <languages>\n"
					 "    <lang percentage=\"86\">de_DE</lang>\n"
					 "    <lang percentage=\"98\">en_GB</lang>\n"
					 "  </languages>\n"
					 "</component>\n";

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.LangTest");
	as_component_add_language (cpt, "de_DE", 86);
	as_component_add_language (cpt, "en_GB", 98);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, expected_lang_xml));
}

/**
 * test_xml_write_provides:
 *
 * Test writing the provides tag.
 */
static void
test_xml_write_provides (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsProvided) prov_mime = NULL;
	g_autoptr(AsProvided) prov_bin = NULL;
	g_autoptr(AsProvided) prov_dbus = NULL;
	g_autoptr(AsProvided) prov_firmware_runtime = NULL;
	g_autoptr(AsProvided) prov_firmware_flashed = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_prov_xml = "<component>\n"
					 "  <id>org.example.ProvidesTest</id>\n"
					 "  <provides>\n"
					 "    <mediatype>text/plain</mediatype>\n"
					 "    <mediatype>application/xml</mediatype>\n"
					 "    <mediatype>image/png</mediatype>\n"
					 "    <binary>foobar</binary>\n"
					 "    <binary>foobar-viewer</binary>\n"
					 "    <dbus type=\"system\">org.example.ProvidesTest.Modify</dbus>\n"
					 "    <firmware type=\"runtime\">ipw2200-bss.fw</firmware>\n"
					 "    <firmware type=\"flashed\">84f40464-9272-4ef7-9399-cd95f12da696</firmware>\n"
					 "  </provides>\n"
					 "</component>\n";

	cpt = as_component_new ();
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

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, expected_prov_xml));
}

/**
 * test_xml_write_suggests:
 *
 * Test writing the suggests tag.
 */
static void
test_xml_write_suggests (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsSuggested) sug_us = NULL;
	g_autoptr(AsSuggested) sug_hr = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_sug_xml_mi = "<component>\n"
					   "  <id>org.example.SuggestsTest</id>\n"
					   "  <suggests type=\"upstream\">\n"
					   "    <id>org.example.Awesome</id>\n"
					   "  </suggests>\n"
					   "</component>\n";
	const gchar *expected_sug_xml_coll = "<components version=\"0.14\">\n"
					"  <component>\n"
					"    <id>org.example.SuggestsTest</id>\n"
					"    <suggests type=\"upstream\">\n"
					"      <id>org.example.Awesome</id>\n"
					"    </suggests>\n"
					"    <suggests type=\"heuristic\">\n"
					"      <id>org.example.MachineLearning</id>\n"
					"      <id>org.example.Stuff</id>\n"
					"    </suggests>\n"
					"  </component>\n"
					"</components>\n";

	cpt = as_component_new ();
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

	/* test metainfo serialization */
	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, expected_sug_xml_mi));
	g_free (res);

	/* test collection serialization */
	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_COLLECTION);
	g_assert_true (as_xml_test_compare_xml (res, expected_sug_xml_coll));
}

static const gchar *xmldata_custom = "<component>\n"
					 "  <id>org.example.CustomTest</id>\n"
					 "  <custom>\n"
					 "    <value key=\"command\">myapp --go</value>\n"
					 "    <value key=\"foo\">bar</value>\n"
					 "  </custom>\n"
					 "</component>\n";
/**
 * test_xml_read_custom:
 *
 * Test reading the custom tag.
 */
static void
test_xml_read_custom (void)
{
	g_autoptr(AsComponent) cpt = NULL;

	cpt = as_xml_test_read_data (xmldata_custom, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.CustomTest");

	g_assert_cmpstr (as_component_get_custom_value (cpt, "command"), ==, "myapp --go");
	g_assert_cmpstr (as_component_get_custom_value (cpt, "foo"), ==, "bar");
	g_assert_null (as_component_get_custom_value (cpt, NULL));
}

/**
 * test_xml_write_custom:
 *
 * Test writing the custom tag.
 */
static void
test_xml_write_custom (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.CustomTest");
	as_component_insert_custom_value (cpt, "command", "myapp");
	as_component_insert_custom_value (cpt, "command", "myapp --go");
	as_component_insert_custom_value (cpt, "foo", "bar");
	as_component_insert_custom_value (cpt, NULL, "dummy");

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_custom));
}

static const gchar *xmldata_content_rating = "<component>\n"
						"  <id>org.example.ContentRatingTest</id>\n"
						"  <content_rating type=\"oars-1.0\">\n"
						"    <content_attribute id=\"drugs-alcohol\">moderate</content_attribute>\n"
						"    <content_attribute id=\"language-humor\">mild</content_attribute>\n"
						"  </content_rating>\n"
						"</component>\n";
/**
 * test_xml_read_content_rating:
 *
 * Test reading the content_rating tag.
 */
static void
test_xml_read_content_rating (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsContentRating *rating;

	cpt = as_xml_test_read_data (xmldata_content_rating, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ContentRatingTest");

	rating = as_component_get_content_rating (cpt, "oars-1.0");
	g_assert_nonnull (rating);

	g_assert_cmpint (as_content_rating_get_value (rating, "drugs-alcohol"), ==, AS_CONTENT_RATING_VALUE_MODERATE);
	g_assert_cmpint (as_content_rating_get_value (rating, "language-humor"), ==, AS_CONTENT_RATING_VALUE_MILD);
	g_assert_cmpint (as_content_rating_get_value (rating, "violence-bloodshed"), ==, AS_CONTENT_RATING_VALUE_NONE);
	g_assert_cmpint (as_content_rating_get_minimum_age (rating), ==, 13);
}

/**
 * test_xml_write_content_rating:
 *
 * Test writing the content_rating tag.
 */
static void
test_xml_write_content_rating (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsContentRating) rating = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.ContentRatingTest");

	rating = as_content_rating_new ();
	as_content_rating_set_kind (rating, "oars-1.0");

	as_content_rating_set_value (rating, "drugs-alcohol", AS_CONTENT_RATING_VALUE_MODERATE);
	as_content_rating_set_value (rating, "language-humor", AS_CONTENT_RATING_VALUE_MILD);

	as_component_add_content_rating (cpt, rating);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_content_rating));
}

/* Test that parsing an empty content rating correctly returns `none` as the
 * value for all the ratings defined by that particular kind of content rating,
 * and `unknown` for everything else. */
static void
test_content_rating_empty (void)
{
	const gchar *src = "<component>\n"
			   "  <id>org.example.ContentRatingEmptyTest</id>\n"
			   "  <content_rating type=\"oars-1.0\"/>\n"
			   "</component>\n";
	g_autoptr(AsComponent) cpt = NULL;
	AsContentRating *content_rating;

	cpt = as_xml_test_read_data (src, AS_FORMAT_STYLE_METAINFO);

	content_rating = as_component_get_content_rating (cpt, "oars-1.0");

	/* verify */
	g_assert_cmpstr (as_content_rating_get_kind (content_rating), ==, "oars-1.0");
	g_assert_cmpint (as_content_rating_get_value (content_rating, "drugs-alcohol"), ==,
			 AS_CONTENT_RATING_VALUE_NONE);
	g_assert_cmpint (as_content_rating_get_value (content_rating, "violence-cartoon"), ==,
			 AS_CONTENT_RATING_VALUE_NONE);
	g_assert_cmpint (as_content_rating_get_value (content_rating, "violence-bloodshed"), ==,
			 AS_CONTENT_RATING_VALUE_NONE);

	/* This one was only added in OARS-1.1, so it shouldn’t have a value of `none`. */
	g_assert_cmpint (as_content_rating_get_value (content_rating, "sex-adultery"), ==,
			 AS_CONTENT_RATING_VALUE_UNKNOWN);
}

static const gchar *xmldata_launchable = "<component>\n"
					 "  <id>org.example.LaunchTest</id>\n"
					 "  <launchable type=\"desktop-id\">org.example.Test.desktop</launchable>\n"
					 "  <launchable type=\"desktop-id\">kde4-kool.desktop</launchable>\n"
					 "</component>\n";
/**
 * test_xml_read_launchable:
 *
 * Test reading the "launchable" tag.
 */
static void
test_xml_read_launchable (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsLaunchable *launch;

	cpt = as_xml_test_read_data (xmldata_launchable, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.LaunchTest");

	launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_nonnull (launch);

	g_assert_cmpint (as_launchable_get_entries (launch)->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (as_launchable_get_entries (launch), 0), ==, "org.example.Test.desktop");
	g_assert_cmpstr (g_ptr_array_index (as_launchable_get_entries (launch), 1), ==, "kde4-kool.desktop");
}

/**
 * test_xml_write_launchable:
 *
 * Test writing the "launchable" tag.
 */
static void
test_xml_write_launchable (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsLaunchable) launch = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.LaunchTest");

	launch = as_launchable_new ();
	as_launchable_set_kind (launch, AS_LAUNCHABLE_KIND_DESKTOP_ID);

	as_launchable_add_entry (launch, "org.example.Test.desktop");
	as_launchable_add_entry (launch, "kde4-kool.desktop");

	as_component_add_launchable (cpt, launch);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_launchable));
}

/**
 * test_appstream_write_metainfo_to_collection:
 */
static void
test_appstream_write_metainfo_to_collection (void)
{
	gchar *tmp;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;

	const gchar *METAINFO_XML =	"<component>\n"
					"  <id>org.example.Test</id>\n"
					"  <name>Test</name>\n"
					"  <name xml:lang=\"eo\">Testo</name>\n"
					"  <name xml:lang=\"x-test\">I am a cruft entry</name>\n"
					"  <name xml:lang=\"de\">Test</name>\n"
					"  <summary>Just a unittest.</summary>\n"
					"  <summary xml:lang=\"de\">Nur ein Unittest.</summary>\n"
					"  <description>\n"
					"    <p>First paragraph</p>\n"
					"    <ol>\n"
					"      <li>One</li>\n"
					"      <li>Two</li>\n"
					"      <li>Three is &gt; 2 &amp; 1</li>\n"
					"    </ol>\n"
					"    <p>Paragraph2</p>\n"
					"    <ul>\n"
					"      <li>First</li>\n"
					"      <li>Second</li>\n"
					"    </ul>\n"
					"    <p>Paragraph3 &amp; the last one</p>\n"
					"    <p xml:lang=\"de\">Erster Absatz</p>\n"
					"    <ol>\n"
					"      <li>One</li>\n"
					"      <li xml:lang=\"de\">Eins</li>\n"
					"      <li xml:lang=\"de\">Zwei</li>\n"
					"      <li xml:lang=\"de\">Drei</li>\n"
					"    </ol>\n"
					"    <ul>\n"
					"      <li xml:lang=\"de\">Erster</li>\n"
					"      <li xml:lang=\"de\">Zweiter</li>\n"
					"    </ul>\n"
					"    <p xml:lang=\"de\">Absatz2</p>\n"
					"  </description>\n"
					"  <keywords>\n"
					"    <keyword>supercalifragilisticexpialidocious</keyword>\n"
					"    <keyword xml:lang=\"de\">Superkalifragilistischexpiallegetisch</keyword>\n"
					"  </keywords>\n"
					"  <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
					"  <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
					"  <icon type=\"stock\">xml-writetest</icon>\n"
					"  <releases>\n"
					"    <release version=\"1.0\" date=\"2016-04-11T22:00:00+00:00\"/>\n"
					"  </releases>\n"
					"</component>\n";

	const gchar *EXPECTED_XML_COLL =   "<components version=\"0.14\">\n"
					   "  <component>\n"
					   "    <id>org.example.Test</id>\n"
					   "    <name>Test</name>\n"
					   "    <name xml:lang=\"de\">Test</name>\n"
					   "    <name xml:lang=\"eo\">Testo</name>\n"
					   "    <summary>Just a unittest.</summary>\n"
					   "    <summary xml:lang=\"de\">Nur ein Unittest.</summary>\n"
					   "    <description>\n"
					   "      <p>First paragraph</p>\n"
					   "      <ol>\n"
					   "        <li>One</li>\n"
					   "        <li>Two</li>\n"
					   "        <li>Three is &gt; 2 &amp; 1</li>\n"
					   "      </ol>\n"
					   "      <p>Paragraph2</p>\n"
					   "      <ul>\n"
					   "        <li>First</li>\n"
					   "        <li>Second</li>\n"
					   "      </ul>\n"
					   "      <p>Paragraph3 &amp; the last one</p>\n"
					   "      <ol>\n"
					   "        <li>One</li>\n"
					   "      </ol>\n"
					   "      <ul/>\n"
					   "    </description>\n"
					   "    <description xml:lang=\"de\">\n"
					   "      <p>Erster Absatz</p>\n"
					   "      <ol>\n"
					   "        <li>Eins</li>\n"
					   "        <li>Zwei</li>\n"
					   "        <li>Drei</li>\n"
					   "      </ol>\n"
					   "      <ul>\n"
					   "        <li>Erster</li>\n"
					   "        <li>Zweiter</li>\n"
					   "      </ul>\n"
					   "      <p>Absatz2</p>\n"
					   "    </description>\n"
					   "    <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
					   "    <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
					   "    <icon type=\"stock\">xml-writetest</icon>\n"
					   "    <keywords>\n"
					   "      <keyword>supercalifragilisticexpialidocious</keyword>\n"
					   "    </keywords>\n"
					   "    <keywords xml:lang=\"de\">\n"
					   "      <keyword>Superkalifragilistischexpiallegetisch</keyword>\n"
					   "    </keywords>\n"
					   "    <releases>\n"
					   "      <release type=\"stable\" version=\"1.0\" timestamp=\"1460412000\"/>\n"
					   "    </releases>\n"
					   "  </component>\n"
					   "</components>\n";

	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "ALL");

	as_metadata_parse (metad, METAINFO_XML, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);

	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_COLLECTION);

	tmp = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert_true (as_xml_test_compare_xml (tmp, EXPECTED_XML_COLL));
	g_free (tmp);
}


static const gchar *xmldata_screenshots = "<component>\n"
					  "  <id>org.example.ScreenshotTest</id>\n"
					  "  <screenshots>\n"
					  "    <screenshot type=\"default\">\n"
					  "      <caption>The main window displaying a thing</caption>\n"
					  "      <caption xml:lang=\"de_DE\">Das Hauptfenster, welches irgendwas zeigt</caption>\n"
					  "      <image type=\"source\" width=\"1916\" height=\"1056\">https://example.org/alpha.png</image>\n"
					  "      <image type=\"thumbnail\" width=\"800\" height=\"600\">https://example.org/alpha_small.png</image>\n"
					  "    </screenshot>\n"
					  "    <screenshot>\n"
					  "      <image type=\"source\" width=\"1916\" height=\"1056\">https://example.org/beta.png</image>\n"
					  "      <image type=\"thumbnail\" width=\"800\" height=\"600\">https://example.org/beta_small.png</image>\n"
					  "      <image type=\"source\" xml:lang=\"de_DE\">https://example.org/localized_de.png</image>\n"
					  "    </screenshot>\n"
					  "    <screenshot>\n"
					  "      <video codec=\"av1\" container=\"matroska\" width=\"1916\" height=\"1056\">https://example.org/screencast.mkv</video>\n"
					  "      <video codec=\"av1\" container=\"matroska\" width=\"1916\" height=\"1056\" xml:lang=\"de_DE\">https://example.org/screencast_de.mkv</video>\n"
					  "    </screenshot>\n"
					  "  </screenshots>\n"
					  "</component>\n";

/**
 * test_xml_read_screenshots:
 *
 * Test reading the "screenshots" tag.
 */
static void
test_xml_read_screenshots (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *screenshots;
	AsScreenshot *scr1;
	AsScreenshot *scr2;
	AsScreenshot *scr3;
	GPtrArray *images;
	GPtrArray *videos;
	AsImage *img;
	AsVideo *vid;

	const gchar *xmldata_screenshot_legacy = "<component>\n"
						 "  <id>org.example.ScreenshotAncient</id>\n"
						 "  <screenshots>\n"
						 "    <screenshot type=\"default\">https://example.org/alpha.png</screenshot>\n"
						 "  </screenshots>\n"
						 "</component>\n";

	cpt = as_xml_test_read_data (xmldata_screenshots, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ScreenshotTest");

	screenshots = as_component_get_screenshots (cpt);
	g_assert_cmpint (screenshots->len, ==, 3);

	scr1 = AS_SCREENSHOT (g_ptr_array_index (screenshots, 0));
	scr2 = AS_SCREENSHOT (g_ptr_array_index (screenshots, 1));
	scr3 = AS_SCREENSHOT (g_ptr_array_index (screenshots, 2));

	/* screenshot 1 */
	g_assert_cmpint (as_screenshot_get_kind (scr1), ==, AS_SCREENSHOT_KIND_DEFAULT);
	g_assert_cmpint (as_screenshot_get_media_kind (scr1), ==, AS_SCREENSHOT_MEDIA_KIND_IMAGE);
	as_screenshot_set_active_locale (scr1, "C");
	g_assert_cmpstr (as_screenshot_get_caption (scr1), ==, "The main window displaying a thing");
	as_screenshot_set_active_locale (scr1, "de_DE");
	g_assert_cmpstr (as_screenshot_get_caption (scr1), ==, "Das Hauptfenster, welches irgendwas zeigt");

	images = as_screenshot_get_images_all (scr1);
	g_assert_cmpint (images->len, ==, 2);

	img = AS_IMAGE (g_ptr_array_index (images, 0));
	g_assert_cmpint (as_image_get_kind (img), ==, AS_IMAGE_KIND_SOURCE);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/alpha.png");
	g_assert_cmpint (as_image_get_width (img), ==, 1916);
	g_assert_cmpint (as_image_get_height (img), ==, 1056);

	img = AS_IMAGE (g_ptr_array_index (images, 1));
	g_assert_cmpint (as_image_get_kind (img), ==, AS_IMAGE_KIND_THUMBNAIL);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/alpha_small.png");
	g_assert_cmpint (as_image_get_width (img), ==, 800);
	g_assert_cmpint (as_image_get_height (img), ==, 600);

	/* get closest images */
	img = as_screenshot_get_image (scr1, 120, 120);
	g_assert_nonnull (img);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/alpha_small.png");
	img = as_screenshot_get_image (scr1, 1400, 1000);
	g_assert_nonnull (img);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/alpha.png");

	/* screenshot 2 */
	g_assert_cmpint (as_screenshot_get_kind (scr2), ==, AS_SCREENSHOT_KIND_EXTRA);
	g_assert_cmpint (as_screenshot_get_media_kind (scr2), ==, AS_SCREENSHOT_MEDIA_KIND_IMAGE);
	as_screenshot_set_active_locale (scr2, "C");
	images = as_screenshot_get_images (scr2);
	g_assert_cmpint (images->len, ==, 2);
	as_screenshot_set_active_locale (scr2, "de_DE");
	images = as_screenshot_get_images (scr2);
	g_assert_cmpint (images->len, ==, 1);
	img = AS_IMAGE (g_ptr_array_index (images, 0));
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/localized_de.png");

	images = as_screenshot_get_images_all (scr2);
	g_assert_cmpint (images->len, ==, 3);

	img = AS_IMAGE (g_ptr_array_index (images, 0));
	g_assert_cmpint (as_image_get_kind (img), ==, AS_IMAGE_KIND_SOURCE);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/beta.png");
	g_assert_cmpint (as_image_get_width (img), ==, 1916);
	g_assert_cmpint (as_image_get_height (img), ==, 1056);

	img = AS_IMAGE (g_ptr_array_index (images, 1));
	g_assert_cmpint (as_image_get_kind (img), ==, AS_IMAGE_KIND_THUMBNAIL);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/beta_small.png");
	g_assert_cmpint (as_image_get_width (img), ==, 800);
	g_assert_cmpint (as_image_get_height (img), ==, 600);

	/* screenshot 3 */
	g_assert_cmpint (as_screenshot_get_kind (scr3), ==, AS_SCREENSHOT_KIND_EXTRA);
	g_assert_cmpint (as_screenshot_get_media_kind (scr3), ==, AS_SCREENSHOT_MEDIA_KIND_VIDEO);
	as_screenshot_set_active_locale (scr3, "C");
	g_assert_cmpint (as_screenshot_get_images (scr3)->len, ==, 0);
	videos = as_screenshot_get_videos (scr3);
	g_assert_cmpint (videos->len, ==, 1);
	as_screenshot_set_active_locale (scr3, "de_DE");
	videos = as_screenshot_get_videos (scr3);
	g_assert_cmpint (videos->len, ==, 1);
	vid = AS_VIDEO (g_ptr_array_index (videos, 0));
	g_assert_cmpstr (as_video_get_url (vid), ==, "https://example.org/screencast_de.mkv");

	as_screenshot_set_active_locale (scr3, "ALL");
	videos = as_screenshot_get_videos (scr3);
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

	/* test a legacy screenshot entry that we briefly supported in an older AppStream release */
	g_object_unref (cpt);
	cpt = as_xml_test_read_data (xmldata_screenshot_legacy, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ScreenshotAncient");

	screenshots = as_component_get_screenshots (cpt);
	g_assert_cmpint (screenshots->len, ==, 1);

	scr1 = AS_SCREENSHOT (g_ptr_array_index (screenshots, 0));
	g_assert_cmpint (as_screenshot_get_kind (scr1), ==, AS_SCREENSHOT_KIND_DEFAULT);
	images = as_screenshot_get_images_all (scr1);
	g_assert_cmpint (images->len, ==, 1);

	img = AS_IMAGE (g_ptr_array_index (images, 0));
	g_assert_cmpint (as_image_get_kind (img), ==, AS_IMAGE_KIND_SOURCE);
	g_assert_cmpstr (as_image_get_url (img), ==, "https://example.org/alpha.png");
}

/**
 * test_xml_write_screenshots:
 *
 * Test writing the "screenshots" tag.
 */
static void
test_xml_write_screenshots (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	g_autoptr(AsScreenshot) scr1 = NULL;
	g_autoptr(AsScreenshot) scr2 = NULL;
	g_autoptr(AsScreenshot) scr3 = NULL;
	AsImage *img;
	AsVideo *vid;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.ScreenshotTest");

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
	img = as_image_new ();
	as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
	as_image_set_width (img, 1916);
	as_image_set_height (img, 1056);
	as_image_set_url (img, "https://example.org/beta.png");
	as_screenshot_add_image (scr2, img);
	g_object_unref (img);

	img = as_image_new ();
	as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
	as_image_set_width (img, 800);
	as_image_set_height (img, 600);
	as_image_set_url (img, "https://example.org/beta_small.png");
	as_screenshot_add_image (scr2, img);
	g_object_unref (img);

	img = as_image_new ();
	as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
	as_image_set_locale (img, "de_DE");
	as_image_set_url (img, "https://example.org/localized_de.png");
	as_screenshot_add_image (scr2, img);
	g_object_unref (img);

	scr3 = as_screenshot_new ();
	vid = as_video_new ();
	as_video_set_codec_kind (vid, AS_VIDEO_CODEC_KIND_AV1);
	as_video_set_container_kind (vid, AS_VIDEO_CONTAINER_KIND_MKV);
	as_video_set_width (vid, 1916);
	as_video_set_height (vid, 1056);
	as_video_set_url (vid, "https://example.org/screencast.mkv");
	as_screenshot_add_video (scr3, vid);
	g_object_unref (vid);

	vid = as_video_new ();
	as_video_set_codec_kind (vid, AS_VIDEO_CODEC_KIND_AV1);
	as_video_set_container_kind (vid, AS_VIDEO_CONTAINER_KIND_MKV);
	as_video_set_locale (vid, "de_DE");
	as_video_set_width (vid, 1916);
	as_video_set_height (vid, 1056);
	as_video_set_url (vid, "https://example.org/screencast_de.mkv");
	as_screenshot_add_video (scr3, vid);
	g_object_unref (vid);

	as_component_add_screenshot (cpt, scr1);
	as_component_add_screenshot (cpt, scr2);
	as_component_add_screenshot (cpt, scr3);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_screenshots));
}


static const gchar *xmldata_relations = "<component>\n"
					"  <id>org.example.RelationsTest</id>\n"
					"  <replaces>\n"
					"    <id>org.example.old_test</id>\n"
					"  </replaces>\n"
					"  <requires>\n"
					"    <kernel version=\"4.15\" compare=\"ge\">Linux</kernel>\n"
					"    <id version=\"1.2\" compare=\"eq\">org.example.TestDependency</id>\n"
					"    <display_length>small</display_length>\n"
					"    <internet bandwidth_mbitps=\"2\">always</internet>\n"
					"  </requires>\n"
					"  <recommends>\n"
					"    <memory>2500</memory>\n"
					"    <modalias>usb:v1130p0202d*</modalias>\n"
					"    <display_length side=\"longest\" compare=\"le\">4200</display_length>\n"
					"    <internet>first-run</internet>\n"
					"  </recommends>\n"
					"  <supports>\n"
					"    <control>gamepad</control>\n"
					"    <control>keyboard</control>\n"
					"    <internet>offline-only</internet>\n"
					"  </supports>\n"
					"</component>\n";
/**
 * test_xml_read_relations:
 *
 * Test reading the recommends/requires/supports tags.
 */
static void
test_xml_read_relations (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *recommends;
	GPtrArray *requires;
	GPtrArray *supports;
	AsRelation *relation;

	cpt = as_xml_test_read_data (xmldata_relations, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.RelationsTest");

	recommends = as_component_get_recommends (cpt);
	requires = as_component_get_requires (cpt);
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
	g_assert_cmpint (as_relation_get_value_px (relation), ==, 4200);
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
	g_assert_cmpint (as_relation_get_value_display_length_kind (relation), ==, AS_DISPLAY_LENGTH_KIND_SMALL);
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

/**
 * test_xml_write_relations:
 *
 * Test writing the recommends/requires/supports tags.
 */
static void
test_xml_write_relations (void)
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
	as_relation_set_value_px (dl_relation1, 4200);
	as_relation_set_display_side_kind (dl_relation1, AS_DISPLAY_SIDE_KIND_LONGEST);
	as_relation_set_compare (dl_relation1, AS_RELATION_COMPARE_LE);

	as_relation_set_item_kind (dl_relation2, AS_RELATION_ITEM_KIND_DISPLAY_LENGTH);
	as_relation_set_value_display_length_kind (dl_relation2, AS_DISPLAY_LENGTH_KIND_SMALL);
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

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_relations));
}


static const gchar *xmldata_agreements = 	"<component>\n"
						"  <id>org.example.AgreementsTest</id>\n"
						"  <agreement type=\"eula\" version_id=\"1.2.3a\">\n"
						"    <agreement_section type=\"intro\">\n"
						"      <name>Intro</name>\n"
						"      <name xml:lang=\"de_DE\">Einführung</name>\n"
						"      <description>\n"
						"        <p>Mighty Fine</p>\n"
						"      </description>\n"
						"    </agreement_section>\n"
						"  </agreement>\n"
						"</component>\n";
/**
 * test_xml_read_agreements:
 *
 * Test reading the agreement tag and related elements.
 */
static void
test_xml_read_agreements (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsAgreement *agreement;
	AsAgreementSection *sect;

	cpt = as_xml_test_read_data (xmldata_agreements, AS_FORMAT_STYLE_METAINFO);
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

	as_agreement_section_set_active_locale (sect, "de_DE");
	g_assert_cmpstr (as_agreement_section_get_name (sect), ==, "Einführung");
}

/**
 * test_xml_write_agreements:
 *
 * Test writing the agreement tag.
 */
static void
test_xml_write_agreements (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	g_autoptr(AsAgreement) agreement = NULL;
	g_autoptr(AsAgreementSection) sect = NULL;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.AgreementsTest");

	agreement = as_agreement_new ();
	sect = as_agreement_section_new ();

	as_agreement_set_kind (agreement, AS_AGREEMENT_KIND_EULA);
	as_agreement_set_version_id (agreement, "1.2.3a");

	as_agreement_section_set_kind (sect, "intro");
	as_agreement_section_set_name (sect, "Intro", "C");
	as_agreement_section_set_name (sect, "Einführung", "de_DE");

	as_agreement_section_set_description (sect, "<p>Mighty Fine</p>", "C");

	as_agreement_add_section (agreement, sect);
	as_component_add_agreement (cpt, agreement);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_agreements));
}

static const gchar *xmldata_releases =  "<component>\n"
					"  <id>org.example.ReleaseTest</id>\n"
					"  <releases>\n"
					"    <release type=\"stable\" version=\"1.2\">\n"
					"      <description>\n"
					"        <p>A release description.</p>\n"
					"        <p xml:lang=\"de\">Eine Beschreibung der Veröffentlichung.</p>\n"
					"      </description>\n"
					"      <url>https://example.org/releases/1.2.html</url>\n"
					"      <issues>\n"
					"        <issue url=\"https://example.com/bugzilla/12345\">bz#12345</issue>\n"
					"        <issue type=\"cve\">CVE-2019-123456</issue>\n"
					"      </issues>\n"
					"      <artifacts>\n"
					"        <artifact type=\"binary\" platform=\"x86_64-linux-gnu\" bundle=\"tarball\">\n"
					"          <location>https://example.com/mytarball.bin.tar.xz</location>\n"
					"          <filename>mytarball-1.2.0.bin.tar.xz</filename>\n"
					"          <checksum type=\"sha256\">f7dd28d23679b5cd6598534a27cd821cf3375c385a10a633f104d9e4841991a8</checksum>\n"
					"          <size type=\"download\">112358</size>\n"
					"          <size type=\"installed\">42424242</size>\n"
					"        </artifact>\n"
					"        <artifact type=\"source\">\n"
					"          <location>https://example.com/mytarball.tar.xz</location>\n"
					"          <checksum type=\"sha256\">95c0a7733b2ec76cf52ba2fa8db31cf3ad6ede7140d675e218c86720e97d9ac1</checksum>\n"
					"        </artifact>\n"
					"      </artifacts>\n"
					"    </release>\n"
					"  </releases>\n"
					"</component>\n";
/**
 * test_xml_read_releases:
 *
 * Test reading the releases tag.
 */
static void
test_xml_read_releases (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsRelease *rel;
	GPtrArray *artifacts;
	GPtrArray *issues;

	cpt = as_xml_test_read_data (xmldata_releases, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ReleaseTest");

	g_assert_cmpint (as_component_get_releases (cpt)->len, ==, 1);

	rel = AS_RELEASE (g_ptr_array_index (as_component_get_releases (cpt), 0));
	g_assert_cmpint (as_release_get_kind (rel), ==, AS_RELEASE_KIND_STABLE);
	g_assert_cmpstr (as_release_get_version (rel), ==,  "1.2");

	as_release_set_active_locale (rel, "de");
	g_assert_cmpstr (as_release_get_description (rel), ==, "<p>Eine Beschreibung der Veröffentlichung.</p>\n");

	as_release_set_active_locale (rel, "C");
	g_assert_cmpstr (as_release_get_description (rel), ==, "<p>A release description.</p>\n");

	g_assert_cmpstr (as_release_get_url (rel, AS_RELEASE_URL_KIND_DETAILS), ==, "https://example.org/releases/1.2.html");

	artifacts = as_release_get_artifacts (rel);
	g_assert_cmpint (artifacts->len, ==, 2);
	for (guint i = 0; i < artifacts->len; i++) {
		AsArtifact *artifact = AS_ARTIFACT (g_ptr_array_index (artifacts, i));
		AsChecksum *cs;

		if (as_artifact_get_kind (artifact) == AS_ARTIFACT_KIND_BINARY) {
			g_assert_cmpstr (as_artifact_get_platform (artifact), ==, "x86_64-linux-gnu");
			g_assert_cmpint (as_artifact_get_bundle_kind (artifact), ==, AS_BUNDLE_KIND_TARBALL);

			g_assert_cmpint (as_artifact_get_locations (artifact)->len, ==, 1);
			g_assert_cmpstr (g_ptr_array_index (as_artifact_get_locations (artifact), 0), ==, "https://example.com/mytarball.bin.tar.xz");

			g_assert_cmpstr (as_artifact_get_filename (artifact), ==, "mytarball-1.2.0.bin.tar.xz");

			cs = as_artifact_get_checksum (artifact, AS_CHECKSUM_KIND_SHA256);
			g_assert_cmpstr (as_checksum_get_value (cs), ==, "f7dd28d23679b5cd6598534a27cd821cf3375c385a10a633f104d9e4841991a8");

			g_assert_cmpuint (as_artifact_get_size (artifact, AS_SIZE_KIND_DOWNLOAD), ==, 112358);
			g_assert_cmpuint (as_artifact_get_size (artifact, AS_SIZE_KIND_INSTALLED), ==, 42424242);
		} else if (as_artifact_get_kind (artifact) == AS_ARTIFACT_KIND_SOURCE) {
			g_assert_cmpint (as_artifact_get_locations (artifact)->len, ==, 1);
			g_assert_cmpstr (g_ptr_array_index (as_artifact_get_locations (artifact), 0), ==, "https://example.com/mytarball.tar.xz");

			cs = as_artifact_get_checksum (artifact, AS_CHECKSUM_KIND_SHA256);
			g_assert_cmpstr (as_checksum_get_value (cs), ==, "95c0a7733b2ec76cf52ba2fa8db31cf3ad6ede7140d675e218c86720e97d9ac1");
		} else {
			g_assert_not_reached ();
		}
	}

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
}

/**
 * test_xml_write_releases:
 *
 * Test writing the releases tag.
 */
static void
test_xml_write_releases (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsRelease) rel = NULL;
	g_autofree gchar *res = NULL;
	AsArtifact *artifact;
	AsChecksum *cs;
	AsIssue *issue;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.ReleaseTest");

	rel = as_release_new ();
	as_release_set_version (rel, "1.2");
	as_release_set_description (rel, "<p>A release description.</p>", "C");
	as_release_set_description (rel, "<p>Eine Beschreibung der Veröffentlichung.</p>", "de");
	as_release_set_url (rel, AS_RELEASE_URL_KIND_DETAILS, "https://example.org/releases/1.2.html");

	/* artifacts */
	artifact = as_artifact_new ();
	as_artifact_set_kind (artifact, AS_ARTIFACT_KIND_BINARY);
	as_artifact_set_platform (artifact, "x86_64-linux-gnu");
	as_artifact_set_bundle_kind (artifact, AS_BUNDLE_KIND_TARBALL);
	as_artifact_add_location (artifact, "https://example.com/mytarball.bin.tar.xz");
	as_artifact_set_filename (artifact, "mytarball-1.2.0.bin.tar.xz");
	cs = as_checksum_new ();
	as_checksum_set_kind (cs, AS_CHECKSUM_KIND_SHA256);
	as_checksum_set_value (cs, "f7dd28d23679b5cd6598534a27cd821cf3375c385a10a633f104d9e4841991a8");
	as_artifact_add_checksum (artifact, cs);
	g_object_unref (cs);
	as_artifact_set_size (artifact, 112358, AS_SIZE_KIND_DOWNLOAD);
	as_artifact_set_size (artifact, 42424242, AS_SIZE_KIND_INSTALLED);
	as_release_add_artifact (rel, artifact);
	g_object_unref (artifact);

	artifact = as_artifact_new ();
	as_artifact_set_kind (artifact, AS_ARTIFACT_KIND_SOURCE);
	as_artifact_add_location (artifact, "https://example.com/mytarball.tar.xz");
	cs = as_checksum_new ();
	as_checksum_set_kind (cs, AS_CHECKSUM_KIND_SHA256);
	as_checksum_set_value (cs, "95c0a7733b2ec76cf52ba2fa8db31cf3ad6ede7140d675e218c86720e97d9ac1");
	as_artifact_add_checksum (artifact, cs);
	g_object_unref (cs);
	as_release_add_artifact (rel, artifact);
	g_object_unref (artifact);

	/* issues */
	issue = as_issue_new ();
	as_issue_set_id (issue, "bz#12345");
	as_issue_set_url (issue, "https://example.com/bugzilla/12345");
	as_release_add_issue (rel, issue);
	g_object_unref (issue);

	issue = as_issue_new ();
	as_issue_set_kind (issue, AS_ISSUE_KIND_CVE);
	as_issue_set_id (issue, "CVE-2019-123456");
	as_release_add_issue (rel, issue);
	g_object_unref (issue);

	as_component_add_release (cpt, rel);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_releases));
}

/**
 * test_xml_read_releases_legacy:
 *
 * Test reading the releases tag with legacy artifacts.
 */
static void
test_xml_read_releases_legacy (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsRelease *rel;
	GPtrArray *artifacts;
	AsArtifact *artifact;
	AsChecksum *cs;

	static const gchar *xmldata_releases_legacy =
					"<component>\n"
					"  <id>org.example.ReleaseTestLegacy</id>\n"
					"  <releases>\n"
					"    <release type=\"stable\" version=\"1.2\">\n"
					"      <description>\n"
					"        <p>A release description.</p>\n"
					"        <p xml:lang=\"de\">Eine Beschreibung der Veröffentlichung.</p>\n"
					"      </description>\n"
					"      <url>https://example.org/releases/1.2.html</url>\n"
					"      <location>https://example.com/mytarball.bin.tar.xz</location>\n"
					"      <checksum type=\"sha256\">f7dd28d23679b5cd6598534a27cd821cf3375c385a10a633f104d9e4841991a8</checksum>\n"
					"      <size type=\"download\">112358</size>\n"
					"      <size type=\"installed\">42424242</size>\n"
					"    </release>\n"
					"  </releases>\n"
					"</component>\n";

	cpt = as_xml_test_read_data (xmldata_releases_legacy, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ReleaseTestLegacy");

	g_assert_cmpint (as_component_get_releases (cpt)->len, ==, 1);

	rel = AS_RELEASE (g_ptr_array_index (as_component_get_releases (cpt), 0));

	artifacts = as_release_get_artifacts (rel);
	g_assert_cmpint (artifacts->len, ==, 1);
	artifact = AS_ARTIFACT (g_ptr_array_index (artifacts, 0));

	g_assert_cmpint (as_artifact_get_locations (artifact)->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (as_artifact_get_locations (artifact), 0), ==, "https://example.com/mytarball.bin.tar.xz");

	cs = as_artifact_get_checksum (artifact, AS_CHECKSUM_KIND_SHA256);
	g_assert_cmpstr (as_checksum_get_value (cs), ==, "f7dd28d23679b5cd6598534a27cd821cf3375c385a10a633f104d9e4841991a8");

	g_assert_cmpuint (as_artifact_get_size (artifact, AS_SIZE_KIND_DOWNLOAD), ==, 112358);
	g_assert_cmpuint (as_artifact_get_size (artifact, AS_SIZE_KIND_INSTALLED), ==, 42424242);
}

/**
 * test_xml_rw_reviews:
 */
static void
test_xml_rw_reviews (void)
{
	static const gchar *xmldata_reviews =
			"<component>\n"
			"  <id>org.example.ReviewTest</id>\n"
			"  <reviews>\n"
			"    <review id=\"17\" date=\"2016-09-15\" rating=\"80\">\n"
			"      <priority>5</priority>\n"
			"      <summary>Hello world</summary>\n"
			"      <description>\n"
			"        <p>Mighty Fine</p>\n"
			"      </description>\n"
			"      <version>1.2.3</version>\n"
			"      <reviewer_id>deadbeef</reviewer_id>\n"
			"      <reviewer_name>Richard Hughes</reviewer_name>\n"
			"      <lang>en_GB</lang>\n"
			"      <metadata>\n"
			"        <value key=\"foo\">bar</value>\n"
			"      </metadata>\n"
			"    </review>\n"
			"  </reviews>\n"
			"</component>\n";
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *reviews;
	AsReview *review;
	g_autofree gchar *res = NULL;

	/* read */
	cpt = as_xml_test_read_data (xmldata_reviews, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ReviewTest");

	reviews = as_component_get_reviews (cpt);
	g_assert_cmpint (reviews->len, ==, 1);
	review = AS_REVIEW (g_ptr_array_index (reviews, 0));

	/* validate */
	g_assert_cmpint (as_review_get_priority (review), ==, 5);
	g_assert_true (as_review_get_date (review) != NULL);
	g_assert_cmpstr (as_review_get_id (review), ==, "17");
	g_assert_cmpstr (as_review_get_version (review), ==, "1.2.3");
	g_assert_cmpstr (as_review_get_reviewer_id (review), ==, "deadbeef");
	g_assert_cmpstr (as_review_get_reviewer_name (review), ==, "Richard Hughes");
	g_assert_cmpstr (as_review_get_summary (review), ==, "Hello world");
	g_assert_cmpstr (as_review_get_locale (review), ==, "en_GB");
	g_assert_cmpstr (as_review_get_description (review), ==, "<p>Mighty Fine</p>");
	g_assert_cmpstr (as_review_get_metadata_item (review, "foo"), ==, "bar");

	/* write */
	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_reviews));
}

/**
 * test_xml_rw_tags:
 */
static void
test_xml_rw_tags (void)
{
	static const gchar *xmldata_tags =
			"<component>\n"
			"  <id>org.example.TagsTest</id>\n"
			"  <tags>\n"
			"    <tag namespace=\"lvfs\">vendor-2021q1</tag>\n"
			"    <tag namespace=\"plasma\">featured</tag>\n"
			"  </tags>\n"
			"</component>\n";
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;

	/* read */
	cpt = as_xml_test_read_data (xmldata_tags, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.TagsTest");

	/* validate */
	g_assert_true (as_component_has_tag (cpt, "lvfs", "vendor-2021q1"));
	g_assert_true (as_component_has_tag (cpt, "plasma", "featured"));

	/* write */
	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_tags));
}

/**
 * test_xml_rw_branding:
 */
static void
test_xml_rw_branding (void)
{
	static const gchar *xmldata_tags =
			"<component>\n"
			"  <id>org.example.BrandingTest</id>\n"
			"  <branding>\n"
			"    <color type=\"primary\" scheme_preference=\"light\">#ff00ff</color>\n"
			"    <color type=\"primary\" scheme_preference=\"dark\">#993d3d</color>\n"
			"  </branding>\n"
			"</component>\n";
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	AsBranding *branding;

	/* read */
	cpt = as_xml_test_read_data (xmldata_tags, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.BrandingTest");

	/* validate */
	branding = as_component_get_branding (cpt);
	g_assert_nonnull (branding);

	g_assert_cmpstr (as_branding_get_color (branding, AS_COLOR_KIND_PRIMARY, AS_COLOR_SCHEME_KIND_LIGHT),
			 ==, "#ff00ff");
	g_assert_cmpstr (as_branding_get_color (branding, AS_COLOR_KIND_PRIMARY, AS_COLOR_SCHEME_KIND_DARK),
			 ==, "#993d3d");

	/* write */
	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert_true (as_xml_test_compare_xml (res, xmldata_tags));
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

	g_test_add_func ("/XML/LegacyData", test_appstream_parser_legacy);
	g_test_add_func ("/XML/Read/ParserLocale", test_appstream_parser_locale);
	g_test_add_func ("/XML/Write/WriterLocale", test_appstream_write_locale);

	g_test_add_func ("/XML/Read/Description", test_appstream_read_description);
	g_test_add_func ("/XML/Write/Description", test_appstream_write_description);
	g_test_add_func ("/XML/DescriptionL10NCleanup", test_appstream_description_l10n_cleanup);

	g_test_add_func ("/XML/Write/MetainfoToCollection", test_appstream_write_metainfo_to_collection);

	g_test_add_func ("/XML/Read/Simple", test_xml_read_simple);
	g_test_add_func ("/XML/Write/Simple", test_xml_write_simple);

	g_test_add_func ("/XML/Read/Url", test_xml_read_url);

	g_test_add_func ("/XML/Write/Provides", test_xml_write_provides);
	g_test_add_func ("/XML/Write/Suggests", test_xml_write_suggests);

	g_test_add_func ("/XML/Read/Languages", test_xml_read_languages);
	g_test_add_func ("/XML/Write/Languages", test_xml_write_languages);

	g_test_add_func ("/XML/Read/Custom", test_xml_read_custom);
	g_test_add_func ("/XML/Write/Custom", test_xml_write_custom);

	g_test_add_func ("/XML/Read/ContentRating", test_xml_read_content_rating);
	g_test_add_func ("/XML/Write/ContentRating", test_xml_write_content_rating);
	g_test_add_func ("/XML/Read/ContentRating/Empty", test_content_rating_empty);

	g_test_add_func ("/XML/Read/Launchable", test_xml_read_launchable);
	g_test_add_func ("/XML/Write/Launchable", test_xml_write_launchable);

	g_test_add_func ("/XML/Read/Screenshots", test_xml_read_screenshots);
	g_test_add_func ("/XML/Write/Screenshots", test_xml_write_screenshots);

	g_test_add_func ("/XML/Read/Relations", test_xml_read_relations);
	g_test_add_func ("/XML/Write/Relations", test_xml_write_relations);

	g_test_add_func ("/XML/Read/Agreements", test_xml_read_agreements);
	g_test_add_func ("/XML/Write/Agreements", test_xml_write_agreements);

	g_test_add_func ("/XML/Read/Releases", test_xml_read_releases);
	g_test_add_func ("/XML/Write/Releases", test_xml_write_releases);
	g_test_add_func ("/XML/Read/ReleasesLegacy", test_xml_read_releases_legacy);

	g_test_add_func ("/XML/ReadWrite/Reviews", test_xml_rw_reviews);
	g_test_add_func ("/XML/ReadWrite/Tags", test_xml_rw_tags);
	g_test_add_func ("/XML/ReadWrite/Branding", test_xml_rw_branding);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
