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
#include <glib/gprintf.h>

#include "appstream.h"
#include "../src/as-cache-builder.h"
#include "../src/as-utils-private.h"

static gchar *datadir = NULL;

void
msg (const gchar *s)
{
	g_printf ("%s\n", s);
}

void
print_cptarray (GPtrArray *cpt_array)
{
	guint i;

	g_printf ("----\n");
	for (i = 0; i < cpt_array->len; i++) {
		AsComponent *cpt;
		cpt = (AsComponent*) g_ptr_array_index (cpt_array, i);

		g_printf ("  - %s\n",
				  as_component_to_string (cpt));
	}
	g_printf ("----\n");
}

gchar*
test_database_create ()
{
	AsBuilder *builder;
	GError *error = NULL;
	gchar *db_path;
	gchar **strv;

	as_utils_touch_dir ("/var/tmp/appstream-tests/");
	db_path = g_strdup ("/var/tmp/appstream-tests/libas-dbtest-XXXXXX");
	db_path = g_mkdtemp (db_path);
	g_assert (db_path != NULL);

	/* we use some sample-data to simulate loading distro data */
	strv = g_new0 (gchar*, 2);
	strv[0] = g_build_filename (datadir, "distro", NULL);

	builder = as_builder_new_path (db_path);
	as_builder_set_data_source_directories (builder, strv);
	g_strfreev (strv);

	as_builder_initialize (builder);
	as_builder_refresh_cache (builder, TRUE, &error);

	if (error != NULL) {
		g_error ("%s", error->message);
		g_error_free (error);
	}

	return db_path;
}

void
test_database_read (const gchar *dbpath)
{
	AsDatabase *db;
	AsSearchQuery *query;
	GPtrArray *cpts = NULL;
	GPtrArray *rels;
	AsRelease *rel;
	AsComponent *cpt;

	db = as_database_new ();
	as_database_set_database_path (db, dbpath);
	as_database_open (db);

	cpts = as_database_get_all_components (db);
	g_assert (cpts != NULL);

	print_cptarray (cpts);

	msg ("==============================");

	query = as_search_query_new ("kig");
	cpts = as_database_find_components (db, query);
	print_cptarray (cpts);
	g_assert (cpts->len == 1);
	cpt = (AsComponent*) g_ptr_array_index (cpts, 0);
	g_assert_cmpstr (as_component_get_pkgnames (cpt)[0], ==, "kig");
	g_ptr_array_unref (cpts);

	query = as_search_query_new ("");
	as_search_query_set_categories_from_string (query, "science");
	cpts = as_database_find_components (db, query);
	print_cptarray (cpts);
	g_assert (cpts->len == 3);
	g_ptr_array_unref (cpts);
	g_object_unref (query);

	query = as_search_query_new ("logic");
	as_search_query_set_categories_from_string (query, "science");
	cpts = as_database_find_components (db, query);
	print_cptarray (cpts);
	g_assert (cpts->len == 1);
	g_ptr_array_unref (cpts);
	g_object_unref (query);

	query = as_search_query_new ("logic");
	cpts = as_database_find_components (db, query);
	print_cptarray (cpts);
	g_assert (cpts->len == 2);
	g_ptr_array_unref (cpts);
	g_object_unref (query);

	cpts = as_database_get_components_by_provides (db, AS_PROVIDES_KIND_BINARY, "inkscape", NULL);
	print_cptarray (cpts);
	g_assert (cpts->len == 1);
	cpt = (AsComponent*) g_ptr_array_index (cpts, 0);

	g_assert_cmpstr (as_component_get_name (cpt), ==, "Inkscape");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "https://inkscape.org/");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_FAQ), ==, "https://inkscape.org/learn/faq/");

	g_ptr_array_unref (cpts);

	/* test a component in a different file, with no package but a bundle instead */
	cpt = as_database_get_component_by_id (db, "neverball.desktop");
	g_assert_nonnull (cpt);

	g_assert_cmpstr (as_component_get_name (cpt), ==, "Neverball");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "http://neverball.org/");
	g_assert_cmpstr (as_component_get_bundle_id (cpt, AS_BUNDLE_KIND_LIMBA), ==, "neverball-1.6.0");

	rels = as_component_get_releases (cpt);
	g_assert_cmpint (rels->len, ==, 2);

	rel = AS_RELEASE (g_ptr_array_index (rels, 0));
	g_assert_cmpstr  (as_release_get_version (rel), ==, "1.6.1");
	g_assert_cmpuint (as_release_get_timestamp (rel), ==, 123465888);
	g_assert (as_release_get_urgency (rel) == AS_URGENCY_KIND_LOW);
	g_assert_cmpuint (as_release_get_size (rel, AS_SIZE_KIND_DOWNLOAD), ==, 112358);
	g_assert_cmpuint (as_release_get_size (rel, AS_SIZE_KIND_INSTALLED), ==, 42424242);

	rel = AS_RELEASE (g_ptr_array_index (rels, 1));
	g_assert_cmpstr  (as_release_get_version (rel), ==, "1.6.0");
	g_assert_cmpuint (as_release_get_timestamp (rel), ==, 123456789);
	g_assert_cmpuint (as_release_get_size (rel, AS_SIZE_KIND_DOWNLOAD), ==, 0);

	g_object_unref (cpt);
	g_object_unref (db);
}

void
test_database ()
{
	gchar *path;

	path = test_database_create ();
	test_database_read (path);

	g_free (path);
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

	g_test_add_func ("/AppStream/Database", test_database);

	ret = g_test_run ();
	g_free (datadir);

	return ret;
}
