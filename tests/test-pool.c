/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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
#include "../src/as-utils-private.h"


static gchar *datadir = NULL;

#if 0
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
	g_autoptr(AsCacheBuilder) builder = NULL;
	GError *error = NULL;
	g_autofree gchar *db_path = NULL;
	g_auto(GStrv) strv = NULL;
	gboolean ret;

	g_mkdir_with_parents ("/var/tmp/appstream-tests/", 0755);
	db_path = g_strdup ("/var/tmp/appstream-tests/libas-dbtest-XXXXXX");
	db_path = g_mkdtemp (db_path);
	g_assert_nonnull (db_path);

	/* we use some sample-data to simulate loading distro data */
	strv = g_new0 (gchar*, 2);
	strv[0] = g_build_filename (datadir, "distro", NULL);

	builder = as_cache_builder_new ();
	as_cache_builder_set_data_source_directories (builder, strv);

	ret = as_cache_builder_setup (builder, db_path, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* build the cache for the first time (enforcing build) */
	ret = as_cache_builder_refresh (builder, TRUE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* if we refresh the cache for the second time, we should receive FALSE, as we don't need to update it again */
	ret = as_cache_builder_refresh (builder, FALSE, &error);
	g_assert_no_error (error);
	g_assert (!ret);

	return g_strdup (db_path);
}

void
test_database_read (const gchar *dbpath)
{
	AsDatabase *db;
	GPtrArray *cpts = NULL;
	GPtrArray *rels;
	AsRelease *rel;
	AsComponent *cpt;
	g_autoptr(GError) error = NULL;

	db = as_database_new ();
	as_database_set_location (db, dbpath);
	as_database_open (db, &error);
	g_assert_no_error (error);

	cpts = as_database_get_all_components (db, &error);
	g_assert_no_error (error);
	g_assert (cpts != NULL);

	print_cptarray (cpts);

	msg ("==============================");

	cpts = as_database_find_components (db, "kig", NULL, &error);
	g_assert_no_error (error);
	print_cptarray (cpts);
	g_assert (cpts->len == 1);
	cpt = (AsComponent*) g_ptr_array_index (cpts, 0);
	g_assert_cmpstr (as_component_get_pkgnames (cpt)[0], ==, "kig");
	g_ptr_array_unref (cpts);

	cpts = as_database_find_components (db, NULL, "science", &error);
	g_assert_no_error (error);
	print_cptarray (cpts);
	g_assert (cpts->len == 3);
	g_ptr_array_unref (cpts);

	cpts = as_database_find_components (db, "logic", "science", &error);
	g_assert_no_error (error);
	print_cptarray (cpts);
	g_assert (cpts->len == 1);
	g_ptr_array_unref (cpts);

	cpts = as_database_find_components (db, "logic", NULL, &error);
	g_assert_no_error (error);
	print_cptarray (cpts);
	g_assert (cpts->len == 2);
	g_ptr_array_unref (cpts);

	cpts = as_database_get_components_by_provided_item (db, AS_PROVIDED_KIND_BINARY, "inkscape", &error);
	g_assert_no_error (error);
	print_cptarray (cpts);
	g_assert (cpts->len == 1);
	cpt = (AsComponent*) g_ptr_array_index (cpts, 0);

	g_assert_cmpstr (as_component_get_name (cpt), ==, "Inkscape");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "https://inkscape.org/");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_FAQ), ==, "https://inkscape.org/learn/faq/");

	g_ptr_array_unref (cpts);

	/* test a component in a different file, with no package but a bundle instead */
	cpt = as_database_get_component_by_id (db, "neverball.desktop", &error);
	g_assert_no_error (error);
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

#endif

void
test_cache ()
{
	g_autoptr(AsDataPool) dpool = NULL;
	g_autoptr(AsComponent) cpt1 = NULL;
	g_autoptr(AsComponent) cpt2 = NULL;
	g_autoptr(GError) error = NULL;

	/* prepare our components */
	cpt1 = as_component_new ();
	as_component_set_kind (cpt1, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt1, "org.example.FooBar1");
	as_component_set_name (cpt1, "FooBar App 1", NULL);
	as_component_set_summary (cpt1, "A unit-test dummy entry", NULL);

	cpt2 = as_component_new ();
	as_component_set_kind (cpt2, AS_COMPONENT_KIND_DESKTOP_APP);
	as_component_set_id (cpt2, "org.example.NewFooBar");
	as_component_set_name (cpt2, "Second FooBar App", NULL);
	as_component_set_summary (cpt2, "Another unit-test dummy entry", NULL);

	/* add data to the pool */
	dpool = as_data_pool_new ();
	as_data_pool_add_component (dpool, cpt1, &error);
	g_assert_no_error (error);

	as_data_pool_add_component (dpool, cpt2, &error);
	g_assert_no_error (error);

	/* export cache file and destroy old data pool */
	as_data_pool_save_cache_file (dpool, "/tmp/as-unittest-cache.pb", &error);
	g_assert_no_error (error);
	g_object_unref (dpool);
	g_object_unref (cpt1);
	g_object_unref (cpt2);

	/* load cache file */
	dpool = as_data_pool_new ();
	as_data_pool_load_cache_file (dpool, "/tmp/as-unittest-cache.pb", &error);
	g_assert_no_error (error);

	/* validate */
	cpt1 = as_data_pool_get_component_by_id (dpool, "org.example.FooBar1");
	g_assert_nonnull (cpt1);

	cpt2 = as_data_pool_get_component_by_id (dpool, "org.example.NewFooBar");
	g_assert_nonnull (cpt2);

	g_assert_cmpint (as_component_get_kind (cpt1), ==, AS_COMPONENT_KIND_GENERIC);
	g_assert_cmpstr (as_component_get_name (cpt1), ==, "FooBar App 1");
	g_assert_cmpstr (as_component_get_summary (cpt1), ==, "A unit-test dummy entry");

	g_assert_cmpint (as_component_get_kind (cpt2), ==, AS_COMPONENT_KIND_DESKTOP_APP);
	g_assert_cmpstr (as_component_get_name (cpt2), ==, "Second FooBar App");
	g_assert_cmpstr (as_component_get_summary (cpt2), ==, "Another unit-test dummy entry");
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

	g_test_add_func ("/AppStream/Cache", test_cache);

	ret = g_test_run ();
	g_free (datadir);

	return ret;
}
