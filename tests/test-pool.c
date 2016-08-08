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

static void
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

void
test_pool_read ()
{
	g_autoptr(AsDataPool) dpool = NULL;
	g_auto(GStrv) datadirs = NULL;
	g_autoptr(GPtrArray) all_cpts = NULL;
	g_autoptr(GPtrArray) result = NULL;
	GPtrArray *rels;
	AsRelease *rel;
	AsComponent *cpt;
	g_autoptr(GError) error = NULL;

	/* create DataPool and load sample metadata */
	datadirs = g_new0 (gchar*, 1 + 1);
	datadirs[0] = g_build_filename (datadir, "distro", NULL);

	dpool = as_data_pool_new ();
	as_data_pool_set_metadata_locations (dpool, datadirs);
	as_data_pool_set_locale (dpool, "C");

	/* TODO: as_data_pool_load (dpool, NULL, &error);
	g_assert_no_error (error); */
	as_data_pool_load_metadata (dpool);

	all_cpts = as_data_pool_get_components (dpool);
	g_assert_nonnull (all_cpts);

	print_cptarray (all_cpts);

	g_print ("==============================\n");

	result = as_data_pool_search (dpool, "kig");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	cpt = AS_COMPONENT (g_ptr_array_index (result, 0));
	g_assert_cmpstr (as_component_get_pkgnames (cpt)[0], ==, "kig");
	g_ptr_array_unref (result);

	result = as_data_pool_search (dpool, "web");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_ptr_array_unref (result);

	result = as_data_pool_search (dpool, "logic");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 2);
	g_ptr_array_unref (result);

	/* search for mixed-case strings */
	result = as_data_pool_search (dpool, "bIoChemistrY");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_ptr_array_unref (result);

	/* test searching for multiple words */
	result = as_data_pool_search (dpool, "scalable graphics");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_ptr_array_unref (result);

	/* we return all components if the search string is too short */
	result = as_data_pool_search (dpool, "sh");
	g_assert_cmpint (result->len, ==, 18);
	g_ptr_array_unref (result);

	result = as_data_pool_get_components_by_categories (dpool, "Science");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 3);
	g_ptr_array_unref (result);

	result = as_data_pool_get_components_by_provided_item (dpool, AS_PROVIDED_KIND_BINARY, "inkscape", &error);
	g_assert_no_error (error);
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	cpt = AS_COMPONENT (g_ptr_array_index (result, 0));

	g_assert_cmpstr (as_component_get_name (cpt), ==, "Inkscape");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "https://inkscape.org/");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_FAQ), ==, "https://inkscape.org/learn/faq/");

	g_ptr_array_unref (result);

	/* test a component in a different file, with no package but a bundle instead */
	cpt = as_data_pool_get_component_by_id (dpool, "neverball.desktop");
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
}

void
test_merge_components ()
{
	guint i;

	g_autoptr(GPtrArray) cpts = NULL;
	g_autoptr(AsDataPool) dpool = NULL;
	g_autoptr(GError) error = NULL;
	g_auto(GStrv) strv = NULL;
	g_auto(GStrv) expected_cpts_id = NULL;
	g_autoptr(GPtrArray) suggestions = NULL;

	GPtrArray* cpts_id = NULL;
	AsComponent *cpt = NULL;
	AsSuggested *suggested = NULL;
	AsSuggestedKind suggested_kind;
	gchar *cpt_id = NULL;

	dpool = as_data_pool_new ();

	strv = g_new0 (gchar*, 2);
	strv[0] = g_build_filename (datadir, "distro", NULL);

	as_data_pool_set_metadata_locations (dpool, strv);
	as_data_pool_load_metadata (dpool);

	cpt = as_data_pool_get_component_by_id (dpool, "test.desktop");
	suggestions = as_component_get_suggestions (cpt);
	suggested = (AsSuggested *) g_ptr_array_index (suggestions, 0);
	cpts_id = as_suggested_get_components_id (suggested);
	suggested_kind = as_suggested_get_kind (suggested);

	expected_cpts_id = g_new0 (gchar*, 3);
	expected_cpts_id[0] = g_strdup ("test1.desktop");
	expected_cpts_id[1] = g_strdup ("test2.desktop");

	g_assert_cmpint (suggestions->len, ==, 2);
	g_assert_cmpint (suggested_kind, ==, AS_SUGGESTED_KIND_HEURISTIC);

	for (i = 0; i < cpts_id->len; i++) {
		cpt_id = (gchar*) g_ptr_array_index (cpts_id, i);
		g_assert_cmpstr (cpt_id, ==, expected_cpts_id[i]);
	}

	cpt = as_data_pool_get_component_by_id (dpool, "test1.desktop");
	suggestions = as_component_get_suggestions (cpt);
	suggested = (AsSuggested *) g_ptr_array_index (suggestions, 0);
	cpts_id = as_suggested_get_components_id (suggested);
	suggested_kind = as_suggested_get_kind (suggested);

	expected_cpts_id = g_new0 (gchar*, 3);
	expected_cpts_id[0] = g_strdup ("test5.desktop");
	expected_cpts_id[1] = g_strdup ("test6.desktop");

	g_assert_cmpint (suggestions->len, ==, 2);
	g_assert_cmpint (suggested_kind, ==, AS_SUGGESTED_KIND_HEURISTIC);

	for (i = 0; i < cpts_id->len; i++) {
		cpt_id = (gchar*) g_ptr_array_index (cpts_id, i);
		g_assert_cmpstr (cpt_id, ==, expected_cpts_id[i]);
	}

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
	g_test_add_func ("/AppStream/PoolRead", test_pool_read);
	g_test_add_func ("/AppStream/ComponentMerge", test_merge_components);

	ret = g_test_run ();
	g_free (datadir);

	return ret;
}
