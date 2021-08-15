/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include <config.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "appstream.h"
#include "as-pool-private.h"
#include "as-test-utils.h"
#include "as-stemmer.h"
#include "as-cache.h"
#include "../src/as-utils-private.h"
#include "../src/as-component-private.h"

static gchar *datadir = NULL;

static void
print_cptarray (GPtrArray *cpt_array)
{
	g_printf ("----\n");
	for (guint i = 0; i < cpt_array->len; i++) {
		g_autofree gchar *tmp = NULL;
		AsComponent *cpt = (AsComponent*) g_ptr_array_index (cpt_array, i);

		tmp = as_component_to_string (cpt);
		g_printf ("  - %s\n", tmp);
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
	as_flags_remove (flags, AS_POOL_FLAG_READ_METAINFO);
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
	g_autoptr(GError) error = NULL;
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

	g_assert_true (as_test_compare_lines (cpts_a_xml, cpts_b_xml));
}

/**
 * test_cache:
 *
 * Test if cache file (de)serialization works.
 */
static void
test_cache ()
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(GPtrArray) cpts_prev = NULL;
	g_autoptr(GPtrArray) cpts_post = NULL;
	g_autoptr(AsMetadata) mdata = NULL;
	g_autoptr(AsCache) cache = NULL;
	g_autoptr(AsComponent) ccpt = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *xmldata_precache = NULL;
	g_autofree gchar *xmldata_postcache = NULL;
	gboolean ret;
	static const gchar *cache_testpath = "/tmp/as-unittest-cache.cache";

	pool = test_get_sampledata_pool (FALSE);
	as_pool_set_cache_location (pool, cache_testpath);

	as_pool_load (pool, NULL, &error);
	g_assert_no_error (error);

	/* get XML representation of the data currently in the pool */
	mdata = as_metadata_new ();
	cpts_prev = as_pool_get_components (pool);
	as_sort_components (cpts_prev);
	g_assert_cmpint (cpts_prev->len, ==, 19);
	for (guint i = 0; i < cpts_prev->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts_prev, i));

		/* keywords are not cached explicitly, they are stored in the search terms list instead. Therefore, we don't
		 * serialize them here */
		as_component_set_keywords (cpt, NULL, NULL);

		/* FIXME: language lists are not deterministic yet, so we ignore them for now */
		g_hash_table_remove_all (as_component_get_languages_table (cpt));

		as_metadata_add_component (mdata, cpt);
	}

	xmldata_precache = as_metadata_components_to_collection (mdata, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);

	/* ensure we get the same result back that we cached before */
	g_object_unref (pool);
	pool = test_get_sampledata_pool (FALSE);
	as_pool_clear_metadata_locations (pool);
	as_pool_set_cache_flags (pool, as_pool_get_cache_flags (pool) | AS_CACHE_FLAG_NO_CLEAR);

	as_pool_set_cache_location (pool, cache_testpath);
	as_pool_load (pool, NULL, &error);
	g_assert_no_error (error);

	cpts_post = as_pool_get_components (pool);
	g_assert_cmpint (cpts_post->len, ==, 19);
	as_assert_component_lists_equal (cpts_post, cpts_prev);

	as_metadata_clear_components (mdata);
	as_sort_components (cpts_post);
	for (guint i = 0; i < cpts_post->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts_post, i));
		as_metadata_add_component (mdata, cpt);
	}

	xmldata_postcache = as_metadata_components_to_collection (mdata, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);
	g_assert_true (as_test_compare_lines (xmldata_precache, xmldata_postcache));

	/* load an "modify" read-only cache */
	cache = as_cache_new ();
	as_cache_set_readonly (cache, TRUE);
	as_cache_open (cache, cache_testpath, "C", &error);
	g_assert_no_error (error);

	ccpt = as_cache_get_component_by_data_id (cache, "system/package/os/org.inkscape.Inkscape/*", &error);
	g_assert_no_error (error);
	g_assert_nonnull (ccpt);

	g_assert_cmpstr (as_component_get_name (ccpt), ==, "Inkscape");
	g_object_unref (ccpt);

	ret = as_cache_remove_by_data_id (cache, "system/package/os/org.inkscape.Inkscape/*", &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	ccpt = as_cache_get_component_by_data_id (cache, "system/package/os/org.inkscape.Inkscape/*", &error);
	g_assert_no_error (error);
	g_assert_null (ccpt);
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
	g_autoptr(AsComponent) cpt_a = NULL;
	AsComponent *cpt_s = NULL;
	gchar **strv;
	GPtrArray *rels;
	AsRelease *rel;
	GPtrArray *artifacts;
	AsBundle *bundle;
	g_autoptr(GError) error = NULL;

	/* load sample data */
	dpool = test_get_sampledata_pool (FALSE);

	as_pool_load (dpool, NULL, &error);
	g_assert_no_error (error);

	/* enusre DeleteMe component was removed via its remove-component merge request */
	result = as_pool_get_components_by_id (dpool, "org.example.DeleteMe");
	g_assert_cmpint (result->len, ==, 0);
	g_clear_pointer (&result, g_ptr_array_unref);

	/* check total pool component count */
	all_cpts = as_pool_get_components (dpool);
	g_assert_nonnull (all_cpts);
	g_assert_cmpint (all_cpts->len, ==, 19);

	/* generic tests */
	result = as_pool_search (dpool, "kig");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	cpt_s = AS_COMPONENT (g_ptr_array_index (result, 0));
	g_assert_cmpstr (as_component_get_pkgnames (cpt_s)[0], ==, "kig");
	g_clear_pointer (&result, g_ptr_array_unref);

	result = as_pool_search (dpool, "web");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_clear_pointer (&result, g_ptr_array_unref);

	result = as_pool_search (dpool, "logic");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 2);
	g_clear_pointer (&result, g_ptr_array_unref);

	/* search for mixed-case strings */
	result = as_pool_search (dpool, "bIoChemistrY");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_clear_pointer (&result, g_ptr_array_unref);

	/* test searching for multiple words */
	result = as_pool_search (dpool, "scalable graphics");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	g_clear_pointer (&result, g_ptr_array_unref);

	/* test searching for multiple words, multiple results */
	result = as_pool_search (dpool, "strategy game");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 2);
	g_clear_pointer (&result, g_ptr_array_unref);

	/* we return all components if the search string is too short */
	result = as_pool_search (dpool, "s");
	g_assert_cmpint (result->len, ==, 19);
	g_clear_pointer (&result, g_ptr_array_unref);

	strv = g_strsplit ("Science", ";", 0);
	result = as_pool_get_components_by_categories (dpool, strv);
	g_strfreev (strv);
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 3);
	g_clear_pointer (&result, g_ptr_array_unref);

	result = as_pool_get_components_by_provided_item (dpool, AS_PROVIDED_KIND_BINARY, "inkscape");
	print_cptarray (result);
	g_assert_cmpint (result->len, ==, 1);
	cpt_s = AS_COMPONENT (g_ptr_array_index (result, 0));

	g_assert_cmpstr (as_component_get_name (cpt_s), ==, "Inkscape");
	g_assert_cmpstr (as_component_get_url (cpt_s, AS_URL_KIND_HOMEPAGE), ==, "https://inkscape.org/");
	g_assert_cmpstr (as_component_get_url (cpt_s, AS_URL_KIND_FAQ), ==, "https://inkscape.org/learn/faq/");

	g_clear_pointer (&result, g_ptr_array_unref);

	/* test a component in a different file, with no package but a bundle instead */
	cpt_a = _as_get_single_component_by_cid (dpool, "org.neverball.Neverball");
	g_assert_nonnull (cpt_a);

	g_assert_cmpstr (as_component_get_name (cpt_a), ==, "Neverball");
	g_assert_cmpstr (as_component_get_url (cpt_a, AS_URL_KIND_HOMEPAGE), ==, "http://neverball.org/");
	bundle = as_component_get_bundle (cpt_a, AS_BUNDLE_KIND_LIMBA);
	g_assert_nonnull (bundle);
	g_assert_cmpstr (as_bundle_get_id (bundle), ==, "neverball-1.6.0");

	rels = as_component_get_releases (cpt_a);
	g_assert_cmpint (rels->len, ==, 2);

	rel = AS_RELEASE (g_ptr_array_index (rels, 0));
	g_assert_cmpstr  (as_release_get_version (rel), ==, "1.6.1");
	g_assert_cmpuint (as_release_get_timestamp (rel), ==, 123465888);
	g_assert_true (as_release_get_urgency (rel) == AS_URGENCY_KIND_LOW);

	artifacts = as_release_get_artifacts (rel);
	g_assert_cmpint (artifacts->len, ==, 2);
	for (guint i = 0; i < artifacts->len; i++) {
		AsArtifact *artifact = AS_ARTIFACT (g_ptr_array_index (artifacts, i));
		if (as_artifact_get_kind (artifact) == AS_ARTIFACT_KIND_BINARY) {
			g_assert_cmpuint (as_artifact_get_size (artifact, AS_SIZE_KIND_DOWNLOAD), ==, 112358);
			g_assert_cmpuint (as_artifact_get_size (artifact, AS_SIZE_KIND_INSTALLED), ==, 42424242);
		}
	}

	rel = AS_RELEASE (g_ptr_array_index (rels, 1));
	g_assert_cmpstr  (as_release_get_version (rel), ==, "1.6.0");
	g_assert_cmpuint (as_release_get_timestamp (rel), ==, 123456789);

	/* check categorization */
	categories = as_get_default_categories (TRUE);
	as_utils_sort_components_into_categories (all_cpts, categories, FALSE);
	for (guint i = 0; i < categories->len; i++) {
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
			AsComponent *tmp_cpt = g_ptr_array_index (as_category_get_components (cat), 0);
			g_assert_cmpstr (as_component_get_id (tmp_cpt), ==, "org.inkscape.Inkscape");
		}
	}

	/* test fetching components by launchable */
	result = as_pool_get_components_by_launchable (dpool, AS_LAUNCHABLE_KIND_DESKTOP_ID, "linuxdcpp.desktop");
	g_assert_cmpint (result->len, ==, 1);
	g_clear_pointer (&result, g_ptr_array_unref);

	result = as_pool_get_components_by_launchable (dpool, AS_LAUNCHABLE_KIND_DESKTOP_ID, "inkscape.desktop");
	g_assert_cmpint (result->len, ==, 1);
	cpt_s = AS_COMPONENT (g_ptr_array_index (result, 0));
	g_assert_cmpstr (as_component_get_id (cpt_s), ==, "org.inkscape.Inkscape");
	g_clear_pointer (&result, g_ptr_array_unref);
}

/**
 * test_pool_read_async_ready_cb:
 *
 * Callback invoked by test_pool_read_async()
 */
static void
test_pool_read_async_ready_cb (AsPool *pool, GAsyncResult *result, gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) all_cpts = NULL;
	GMainLoop **loop = (GMainLoop**) user_data;

	g_debug ("AsPool-Async-Load: Received ready callback.");
	as_pool_load_finish (pool, result, &error);
	g_assert_no_error (error);

	g_debug ("AsPool-Async-Load: Checking component count (after ready)");

	/* check total retrieved component count */
	all_cpts = as_pool_get_components (pool);
	g_assert_nonnull (all_cpts);
	g_assert_cmpint (all_cpts->len, ==, 19);

	/* we received the callback, so quite the loop */
	g_main_loop_quit (*loop);
	g_clear_pointer (loop, g_main_loop_unref);
}

/**
 * test_log_allow_warnings:
 *
 * Some warnings emitted when querying the pool while it is being loaded
 * are important, but we specifically want to ignore them for this
 * particular test case.
 */
gboolean
test_log_allow_warnings (const gchar *log_domain,
			     GLogLevelFlags log_level,
			     const gchar *message,
			     gpointer user_data)
{
	return ((log_level & G_LOG_LEVEL_MASK) <= G_LOG_LEVEL_CRITICAL);
}

/**
 * test_pool_read_async:
 *
 * Test reading information from the metadata pool asynchronously.
 */
static void
test_pool_read_async ()
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(GPtrArray) cpts = NULL;
	g_autoptr(GPtrArray) result = NULL;
	g_autoptr(GMainLoop) loop = g_main_loop_new (NULL, FALSE);

	/* load sample data */
	pool = test_get_sampledata_pool (FALSE);

	g_debug ("AsPool-Async-Load: Requesting pool data loading.");
	as_pool_load_async (pool,
			    NULL, /* cancellable */
			    (GAsyncReadyCallback) test_pool_read_async_ready_cb,
			    &loop);

	g_debug ("AsPool-Async-Load: Searching for components (immediately)");

	/* ignore some warnings by the following functions
	 * (they may complain, as the cache isn't loaded yet) */
	g_test_log_set_fatal_handler (test_log_allow_warnings, NULL);

	result = as_pool_search (pool, "web");
	g_assert_cmpint (result->len, ==, 0);
	g_clear_pointer (&result, g_ptr_array_unref);

	cpts = as_pool_get_components (pool);
	g_assert_nonnull (cpts);
	g_assert_cmpint (cpts->len, ==, 0);
	g_ptr_array_unref (cpts);

	/* wait for the callback to be run (unless it already has!) */
	if (loop != NULL)
		g_main_loop_run (loop);

	/* reset handler */
	g_test_log_set_fatal_handler (NULL, NULL);

	g_debug ("AsPool-Async-Load: Checking component count (after loaded)");
	cpts = as_pool_get_components (pool);
	g_assert_nonnull (cpts);
	g_assert_cmpint (cpts->len, ==, 19);
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
	g_autoptr(AsComponent) cpt = NULL;
	GPtrArray *suggestions;
	AsSuggested *suggested;
	GPtrArray *cpt_ids;
	g_autoptr(GError) error = NULL;

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
	g_clear_pointer (&cpt, g_object_unref);

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
	g_clear_pointer (&cpt, g_object_unref);

	/* test if names get overridden */
	cpt = _as_get_single_component_by_cid (dpool, "kiki.desktop");
	g_assert_nonnull (cpt);
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Kiki (name changed by merge)");
}

#ifdef HAVE_STEMMING
/**
 * test_search_stemming:
 *
 * Test if stemming works as expected.
 */
static void
test_search_stemming ()
{
	gchar *tmp;
	AsStemmer *stemmer = as_stemmer_get ();
	as_stemmer_reload (stemmer, "en");

	tmp = as_stemmer_stem (stemmer, "calculator");
	g_assert_cmpstr (tmp, ==, "calcul");
	g_free (tmp);

	tmp = as_stemmer_stem (stemmer, "gimping");
	g_assert_cmpstr (tmp, ==, "gimp");
	g_free (tmp);
}
#endif

/**
 * test_pool_empty:
 *
 * Test if working on a fresh, empty pool works.
 */
static void
test_pool_empty ()
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(GPtrArray) result = NULL;
	g_autoptr(GError) error = NULL;
	AsComponent *cpt = NULL;
	gboolean ret;

	pool = as_pool_new ();
	as_pool_clear_metadata_locations (pool);
	as_pool_set_locale (pool, "C");

	/* test reading from the pool when it wasn't loaded yet */
	result = as_pool_get_components_by_id (pool, "org.example.NotThere");
	g_assert_cmpint (result->len, ==, 0);
	g_clear_pointer (&result, g_ptr_array_unref);

	result = as_pool_search (pool, "web");
	g_assert_cmpint (result->len, ==, 0);
	g_clear_pointer (&result, g_ptr_array_unref);

	/* create dummy app to add */
	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);
	as_component_set_id (cpt, "org.freedesktop.FooBar");
	as_component_set_name (cpt, "A fooish bar", "C");
	as_component_set_summary (cpt, "Foo the bar.", "C");

	ret = as_pool_add_component (pool, cpt, &error);
	g_object_unref (cpt);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* try to retrieve the dummy component */
	result = as_pool_search (pool, "foo");
	g_assert_cmpint (result->len, ==, 1);
	cpt = AS_COMPONENT (g_ptr_array_index (result, 0));
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.freedesktop.FooBar");
	g_clear_pointer (&result, g_ptr_array_unref);
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

	g_test_add_func ("/AppStream/PoolRead", test_pool_read);
	g_test_add_func ("/AppStream/PoolReadAsync", test_pool_read_async);
	g_test_add_func ("/AppStream/PoolEmpty", test_pool_empty);
	g_test_add_func ("/AppStream/Cache", test_cache);
	g_test_add_func ("/AppStream/Merges", test_merge_components);

#ifdef HAVE_STEMMING
	g_test_add_func ("/AppStream/Stemming", test_search_stemming);
#endif

	ret = g_test_run ();
	g_free (datadir);

	return ret;
}
