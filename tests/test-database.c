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
	g_assert (g_strcmp0 (as_component_get_pkgnames (cpt)[0], "kig") == 0);
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
	g_assert (g_strcmp0 (as_component_get_name (cpt), "Inkscape") == 0);
	g_ptr_array_unref (cpts);

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
