/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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
#include "appstream.h"

static gchar *datadir = NULL;

void
test_menuparser ()
{
	AsMenuParser *parser;
	GList *menu_dirs;
	gchar *path;

	path = g_build_filename (datadir, "categories.xml", NULL);
	parser = as_menu_parser_new_from_file (path);
	g_free (path);

	menu_dirs = as_menu_parser_parse (parser);
	g_assert (g_list_length (menu_dirs) > 4);

	g_object_unref (parser);
	g_list_free (menu_dirs);
}

void
test_simplemarkup ()
{
	gchar *str;
	str = as_description_markup_convert_simple ("<p>Test!</p><p>Blah.</p><ul><li>A</li><li>B</li></ul><p>End.</p>");
	g_debug ("%s", str);
	g_assert (g_strcmp0 (str, "Test!\n\nBlah.\n • A\n • B\n\nEnd.") == 0);
	g_free (str);
}

gchar**
_get_dummy_strv (const gchar *value)
{
	gchar **strv;

	strv = g_new0 (gchar*, 1 + 2);
	strv[0] = g_strdup (value);
	strv[1] = NULL;

	return strv;
}

void
test_component ()
{
	AsComponent *cpt;
	AsMetadata *metad;
	gchar *str;
	gchar *str2;
	gchar **strv;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);

	as_component_set_name (cpt, "Test", NULL);
	as_component_set_summary (cpt, "It does things", NULL);

	strv = _get_dummy_strv ("fedex");
	as_component_set_pkgnames (cpt, strv);
	g_strfreev (strv);

	metad = as_metadata_new ();
	as_metadata_add_component (metad, cpt);
	str = as_metadata_component_to_upstream_xml (metad);
	str2 = as_metadata_components_to_distro_xml (metad);
	g_object_unref (metad);
	g_debug ("%s", str2);

	g_assert_cmpstr (str, ==, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				  "<component type=\"desktop\">\n"
				  "  <name>Test</name>\n"
				  "  <summary>It does things</summary>\n"
				  "  <pkgname>fedex</pkgname>\n"
				  "</component>\n");
	g_assert_cmpstr (str2, ==, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				   "<components version=\"0.8\">\n"
				   "  <component type=\"desktop\">\n"
				   "    <name>Test</name>\n"
				   "    <summary>It does things</summary>\n"
				   "    <pkgname>fedex</pkgname>\n"
				   "  </component>\n"
				   "</components>\n");

	g_free (str);
	g_free (str2);
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

	g_test_add_func ("/AppStream/MenuParser", test_menuparser);
	g_test_add_func ("/AppStream/SimpleMarkupConvert", test_simplemarkup);
	g_test_add_func ("/AppStream/Component", test_component);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
