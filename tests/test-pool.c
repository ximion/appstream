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
#include "as-pool-private.h"
#include "as-test-utils.h"
#include "../src/as-utils-private.h"
#include "../src/as-component-private.h"


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

/**
 * _as_get_single_component_by_cid:
 *
 * Internal helper to get a single #AsComponent by its
 * component identifier.
 */
static AsComponent*
_as_get_single_component_by_cid (AsPool *pool, const gchar *cid)
{
	g_autoptr(GPtrArray) result = NULL;

	result = as_pool_get_components_by_id (pool, cid);
	if (result->len == 0)
		return NULL;
	return g_object_ref (AS_COMPONENT (g_ptr_array_index (result, 0)));
}

/**
 * test_cache:
 *
 * Test reading data from cache files.
 */
static void
test_cache ()
{
	g_autoptr(AsPool) dpool = NULL;
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
	as_component_insert_custom_value (cpt2, "mykey", "stuff");

	/* add data to the pool */
	dpool = as_pool_new ();
	as_pool_add_component (dpool, cpt1, &error);
	g_assert_no_error (error);

	as_pool_add_component (dpool, cpt2, &error);
	g_assert_no_error (error);

	/* export cache file and destroy old data pool */
	as_pool_save_cache_file (dpool, "/tmp/as-unittest-cache.gvz", &error);
	g_assert_no_error (error);
	g_object_unref (dpool);
	g_object_unref (cpt1);
	g_object_unref (cpt2);

	/* load cache file */
	dpool = as_pool_new ();
	as_pool_load_cache_file (dpool, "/tmp/as-unittest-cache.gvz", &error);
	g_assert_no_error (error);

	/* validate */
	cpt1 = _as_get_single_component_by_cid (dpool, "org.example.FooBar1");
	g_assert_nonnull (cpt1);

	cpt2 = _as_get_single_component_by_cid (dpool, "org.example.NewFooBar");
	g_assert_nonnull (cpt2);

	g_assert_cmpint (as_component_get_kind (cpt1), ==, AS_COMPONENT_KIND_GENERIC);
	g_assert_cmpstr (as_component_get_name (cpt1), ==, "FooBar App 1");
	g_assert_cmpstr (as_component_get_summary (cpt1), ==, "A unit-test dummy entry");

	g_assert_cmpint (as_component_get_kind (cpt2), ==, AS_COMPONENT_KIND_DESKTOP_APP);
	g_assert_cmpstr (as_component_get_name (cpt2), ==, "Second FooBar App");
	g_assert_cmpstr (as_component_get_summary (cpt2), ==, "Another unit-test dummy entry");
	g_assert_cmpstr (as_component_get_custom_value (cpt2, "mykey"), ==, "stuff");
}

/**
 * test_get_sampledata_pool:
 *
 * Internal helper to get a pool with the sample data locations set.
 */
static AsPool*
test_get_sampledata_pool (gboolean use_caches)
{
	AsPool *pool;
	AsPoolFlags flags;
	g_autofree gchar *mdata_dir = NULL;

	/* create AsPool and load sample metadata */
	mdata_dir = g_build_filename (datadir, "collection", NULL);

	pool = as_pool_new ();
	as_pool_clear_metadata_locations (pool);
	as_pool_add_metadata_location (pool, mdata_dir);
	as_pool_set_locale (pool, "C");

	flags = as_pool_get_flags (pool);
	as_flags_remove (flags, AS_POOL_FLAG_READ_DESKTOP_FILES);
	as_pool_set_flags (pool, flags);

	if (!use_caches)
		as_pool_set_cache_flags (pool, AS_CACHE_FLAG_NONE);

	return pool;
}

/**
 * as_assert_component_lists_equal:
 *
 * Check if the components present in the two #GPtrArray are equal.
 */
static void
as_assert_component_lists_equal (GPtrArray *cpts_a, GPtrArray *cpts_b)
{
	guint i;
	g_autofree gchar *cpts_a_xml = NULL;
	g_autofree gchar *cpts_b_xml = NULL;
	GError *error = NULL;
	g_autoptr(AsMetadata) metad = as_metadata_new ();

	/* sort */
	as_sort_components (cpts_a);
	as_sort_components (cpts_b);

	for (i = 0; i < cpts_a->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts_a, i));
		/* we ignore keywords for now */
		as_component_set_keywords (cpt, NULL, "C");
		/* FIXME: And languages, because their ordering on serialization is random. */
		g_hash_table_remove_all (as_component_get_languages_table (cpt));

		as_metadata_add_component (metad, cpt);
	}

	cpts_a_xml = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);

	as_metadata_clear_components (metad);
	for (i = 0; i < cpts_b->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts_b, i));
		/* we ignore keywords for now */
		as_component_set_keywords (cpt, NULL, "C");
		/* FIXME: And languages, because their ordering on serialization is random. */
		g_hash_table_remove_all (as_component_get_languages_table (cpt));

		as_metadata_add_component (metad, cpt);
	}

	cpts_b_xml = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);

	g_assert (as_test_compare_lines (cpts_a_xml, cpts_b_xml));
}

/**
 * test_cache_file:
 *
 * Test if cache file (de)serialization works.
 */
static void
test_cache_file ()
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(GPtrArray) cpts_prev = NULL;
	g_autoptr(GPtrArray) cpts = NULL;
	GError *error = NULL;

	pool = test_get_sampledata_pool (FALSE);
	as_pool_load (pool, NULL, &error);
	g_assert_no_error (error);

	cpts_prev = as_pool_get_components (pool);
	g_assert_cmpint (cpts_prev->len, ==, 18);

	as_cache_file_save ("/tmp/as-unittest-dummy.gvz", "C", cpts_prev, &error);
	g_assert_no_error (error);

	cpts = as_cache_file_read ("/tmp/as-unittest-dummy.gvz", &error);
	g_assert_no_error (error);
	g_assert_cmpint (cpts->len, ==, 18);

	as_assert_component_lists_equal (cpts, cpts_prev);
}

/**
 * test_pool_read:
 *
 * Test reading information from the metadata pool.
 */
static void
test_pool_read ()
{
	g_autoptr(AsPool) dpool = NULL;
	g_autoptr(GPtrArray) all_cpts = NULL;
	g_autoptr(GPtrArray) result = NULL;
	g_autoptr(GPtrArray) categories = NULL;
	gchar **strv;
	GPtrArray *rels;
	AsRelease *rel;
	AsComponent *cpt;
	AsBundle *bundle;
	guint i;
	g_autoptr(GError) error = NULL;

	/* load sample data */
	dpool = test_get_sampledata_pool (FALSE);

	as_pool_load (dpool, NULL, &error);
	g_assert_no_error (error);

	all_cpts = as_pool_get_components (dpool);
	g_assert_nonnull (all_cpts);
	g_assert_cmpint (all_cpts->len, ==, 18);

	result = as_pool_search (dpool, "kig");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	cpt = AS_COMPONENT (g_ptr_array_index (result, 0));
	g_assert_cmpstr (as_component_get_pkgnames (cpt)[0], ==, "kig");
	g_ptr_array_unref (result);

	result = as_pool_search (dpool, "web");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_ptr_array_unref (result);

	result = as_pool_search (dpool, "logic");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 2);
	g_ptr_array_unref (result);

	/* search for mixed-case strings */
	result = as_pool_search (dpool, "bIoChemistrY");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_ptr_array_unref (result);

	/* test searching for multiple words */
	result = as_pool_search (dpool, "scalable graphics");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_ptr_array_unref (result);

	/* we return all components if the search string is too short */
	result = as_pool_search (dpool, "sh");
	g_assert_cmpint (result->len, ==, 18);
	g_ptr_array_unref (result);

	strv = g_strsplit ("Science", ";", 0);
	result = as_pool_get_components_by_categories (dpool, strv);
	g_strfreev (strv);
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 3);
	g_ptr_array_unref (result);

	result = as_pool_get_components_by_provided_item (dpool, AS_PROVIDED_KIND_BINARY, "inkscape");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	cpt = AS_COMPONENT (g_ptr_array_index (result, 0));

	g_assert_cmpstr (as_component_get_name (cpt), ==, "Inkscape");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "https://inkscape.org/");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_FAQ), ==, "https://inkscape.org/learn/faq/");

	g_ptr_array_unref (result);

	/* test a component in a different file, with no package but a bundle instead */
	cpt = _as_get_single_component_by_cid (dpool, "org.neverball.Neverball");
	g_assert_nonnull (cpt);

	g_assert_cmpstr (as_component_get_name (cpt), ==, "Neverball");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "http://neverball.org/");
	bundle = as_component_get_bundle (cpt, AS_BUNDLE_KIND_LIMBA);
	g_assert_nonnull (bundle);
	g_assert_cmpstr (as_bundle_get_id (bundle), ==, "neverball-1.6.0");

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

	/* check categorization */
	categories = as_get_default_categories (TRUE);
	as_utils_sort_components_into_categories (all_cpts, categories, FALSE);
	for (i = 0; i < categories->len; i++) {
		const gchar *cat_id;
		gint cpt_count;
		AsCategory *cat = AS_CATEGORY (g_ptr_array_index (categories, i));

		cat_id = as_category_get_id (cat);
		cpt_count = as_category_get_components (cat)->len;

		if (g_strcmp0 (cat_id, "communication") == 0)
			g_assert_cmpint (cpt_count, ==, 3);
		else if (g_strcmp0 (cat_id, "utilities") == 0)
			g_assert_cmpint (cpt_count, ==, 3);
		else if (g_strcmp0 (cat_id, "audio-video") == 0)
			g_assert_cmpint (cpt_count, ==, 0);
		else if (g_strcmp0 (cat_id, "developer-tools") == 0)
			g_assert_cmpint (cpt_count, ==, 2);
		else if (g_strcmp0 (cat_id, "education") == 0)
			g_assert_cmpint (cpt_count, ==, 4);
		else if (g_strcmp0 (cat_id, "games") == 0)
			g_assert_cmpint (cpt_count, ==, 4);
		else if (g_strcmp0 (cat_id, "graphics") == 0)
			g_assert_cmpint (cpt_count, ==, 1);
		else if (g_strcmp0 (cat_id, "office") == 0)
			g_assert_cmpint (cpt_count, ==, 0);
		else if (g_strcmp0 (cat_id, "addons") == 0)
			g_assert_cmpint (cpt_count, ==, 0);
		else if (g_strcmp0 (cat_id, "science") == 0)
			g_assert_cmpint (cpt_count, ==, 3);
		else {
			g_error ("Unhandled category: %s", cat_id);
			g_assert_not_reached ();
		}

		if (g_strcmp0 (cat_id, "graphics") == 0) {
			cpt = g_ptr_array_index (as_category_get_components (cat), 0);
			g_assert_cmpstr (as_component_get_id (cpt), ==, "org.inkscape.Inkscape");
		}
	}

	/* test fetching components by launchable */
	result = as_pool_get_components_by_launchable (dpool, AS_LAUNCHABLE_KIND_DESKTOP_ID, "linuxdcpp.desktop");
	g_assert_cmpint (result->len, ==, 1);
	g_ptr_array_unref (result);

	result = as_pool_get_components_by_launchable (dpool, AS_LAUNCHABLE_KIND_DESKTOP_ID, "inkscape.desktop");
	g_assert_cmpint (result->len, ==, 1);
	cpt = AS_COMPONENT (g_ptr_array_index (result, 0));
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.inkscape.Inkscape");
	g_ptr_array_unref (result);
}

/**
 * test_merge_components:
 *
 * Test merging of component data via the "merge" pseudo-component.
 */
static void
test_merge_components ()
{
	g_autoptr(AsPool) dpool = NULL;
	AsComponent *cpt;
	GPtrArray *suggestions;
	AsSuggested *suggested;
	GPtrArray *cpt_ids;
	GError *error = NULL;

	/* load the data pool with sample data */
	dpool = test_get_sampledata_pool (FALSE);
	as_pool_load (dpool, NULL, &error);
	g_assert_no_error (error);

	/* test injection of suggests tags */
	cpt = _as_get_single_component_by_cid (dpool, "links2.desktop");
	g_assert_nonnull (cpt);

	suggestions = as_component_get_suggested (cpt);
	suggested = AS_SUGGESTED (g_ptr_array_index (suggestions, 0));
	g_assert_cmpint (suggestions->len, ==, 1);
	g_assert_cmpint (as_suggested_get_kind (suggested), ==, AS_SUGGESTED_KIND_HEURISTIC);

	cpt_ids = as_suggested_get_ids (suggested);
	g_assert_cmpint (cpt_ids->len, ==, 2);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 0), ==, "org.example.test1");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 1), ==, "org.example.test2");

	cpt = _as_get_single_component_by_cid (dpool, "literki.desktop");
	g_assert_nonnull (cpt);
	suggestions = as_component_get_suggested (cpt);
	suggested = AS_SUGGESTED (g_ptr_array_index (suggestions, 0));
	g_assert_cmpint (suggestions->len, ==, 1);
	g_assert_cmpint (as_suggested_get_kind (suggested), ==, AS_SUGGESTED_KIND_HEURISTIC);

	cpt_ids = as_suggested_get_ids (suggested);
	g_assert_cmpint (cpt_ids->len, ==, 2);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 0), ==, "org.example.test3");
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (cpt_ids, 1), ==, "org.example.test4");

	/* test if names get overridden */
	cpt = _as_get_single_component_by_cid (dpool, "kiki.desktop");
	g_assert_nonnull (cpt);
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Kiki (name changed by merge)");
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

	g_test_add_func ("/AppStream/PoolRead", test_pool_read);
	g_test_add_func ("/AppStream/CacheFile", test_cache_file);
	g_test_add_func ("/AppStream/Cache", test_cache);
	g_test_add_func ("/AppStream/Merges", test_merge_components);

	ret = g_test_run ();
	g_free (datadir);

	return ret;
}
