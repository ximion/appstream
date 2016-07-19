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

/**
 * SECTION:as-data-pool
 * @short_description: Provides access to the AppStream metadata pool.
 *
 * This class loads AppStream metadata from various sources and refines it with existing
 * knowledge about the system (e.g. by setting absolute pazhs for cached icons).
 * An #AsDataPool will use an on-disk cache to store metadata is has read and refined to
 * speed up the loading time when the same data is requested a second time.
 *
 * You can find AppStream metadata matching farious criteria, and also add new metadata to
 * the pool.
 * The caching behavior can be controlled by the application using #AsDataPool.
 *
 * An AppStream cache object can also be created and read using the appstreamcli(1) utility.
 *
 * See also: #AsComponent
 */

#include "config.h"
#include "as-data-pool.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pb/as-cache-internal.h"
#include "as-utils.h"
#include "as-utils-private.h"
#include "as-component-private.h"
#include "as-distro-details.h"
#include "as-settings-private.h"
#include "as-distro-extras.h"
#include "as-stemmer.h"

#include "as-metadata.h"
#include "as-yamldata.h"

typedef struct
{
	GHashTable *cpt_table;
	gchar *screenshot_service_url;
	gchar *locale;
	gchar *current_arch;

	GPtrArray *mdata_dirs;

	gchar **icon_paths;
	gchar **term_greylist;

	AsCacheFlags cflags;
	gchar *sys_cache_path;
	gchar *user_cache_path;
	time_t cache_ctime;
} AsDataPoolPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsDataPool, as_data_pool, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_data_pool_get_instance_private (o))

/**
 * AS_APPSTREAM_METADATA_PATHS:
 *
 * Locations where system AppStream metadata can be stored.
 */
const gchar *AS_APPSTREAM_METADATA_PATHS[4] = { "/usr/share/app-info",
						"/var/lib/app-info",
						"/var/cache/app-info",
						NULL};

/* TRANSLATORS: List of "grey-listed" words sperated with ";"
 * Do not translate this list directly. Instead,
 * provide a list of words in your language that people are likely
 * to include in a search but that should normally be ignored in
 * the search.
 */
#define AS_SEARCH_GREYLIST_STR _("app;application;package;program;programme;suite;tool")

/**
 * as_data_pool_check_cache_ctime:
 * @dpool: An instance of #AsCacheBuilder
 *
 * Update the cached cache-ctime. We need to cache it prior to potentially
 * creating a new database, so we will always rebuild the database in case
 * none existed previously.
 */
static void
as_data_pool_check_cache_ctime (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	struct stat cache_sbuf;
	g_autofree gchar *fname = NULL;

	fname = g_strdup_printf ("%s/%s.pb", priv->sys_cache_path, priv->locale);
	if (stat (fname, &cache_sbuf) < 0)
		priv->cache_ctime = 0;
	else
		priv->cache_ctime = cache_sbuf.st_ctime;
}

/**
 * as_data_pool_init:
 **/
static void
as_data_pool_init (AsDataPool *dpool)
{
	guint i;
	g_autoptr(AsDistroDetails) distro = NULL;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	/* set active locale */
	priv->locale = as_get_current_locale ();

	priv->cpt_table = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						(GDestroyNotify) g_object_unref);

	distro = as_distro_details_new ();
	priv->screenshot_service_url = as_distro_details_get_str (distro, "ScreenshotUrl");

	/* set watched default directories for AppStream metadata */
	priv->mdata_dirs = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; AS_APPSTREAM_METADATA_PATHS[i] != NULL; i++) {
		g_ptr_array_add (priv->mdata_dirs,
					g_strdup (AS_APPSTREAM_METADATA_PATHS[i]));
	}

	/* set default icon search locations */
	priv->icon_paths = as_get_icon_repository_paths ();

	/* set the current architecture */
	priv->current_arch = as_get_current_arch ();

	/* set up our localized search-term greylist */
	priv->term_greylist = g_strsplit (AS_SEARCH_GREYLIST_STR, ";", -1);

	/* system-wide cache locations */
	priv->sys_cache_path = g_build_filename (AS_APPSTREAM_CACHE_PATH, "pb", NULL);

	/* users umask shouldn't interfere with us creating new files when we are root */
	if (as_utils_is_root ())
		as_reset_umask ();

	/* check the ctime of the cache directory, if it exists at all */
	as_data_pool_check_cache_ctime (dpool);
}

/**
 * as_data_pool_finalize:
 **/
static void
as_data_pool_finalize (GObject *object)
{
	AsDataPool *dpool = AS_DATA_POOL (object);
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	g_free (priv->screenshot_service_url);
	g_hash_table_unref (priv->cpt_table);

	g_ptr_array_unref (priv->mdata_dirs);
	g_strfreev (priv->icon_paths);

	g_free (priv->locale);
	g_free (priv->current_arch);

	g_strfreev (priv->term_greylist);

	g_free (priv->sys_cache_path);
	g_free (priv->user_cache_path);

	G_OBJECT_CLASS (as_data_pool_parent_class)->finalize (object);
}

/**
 * as_data_pool_class_init:
 **/
static void
as_data_pool_class_init (AsDataPoolClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_data_pool_finalize;
}

/**
 * as_data_pool_merge_components:
 *
 * Merge selected data from two components.
 */
static void
as_data_pool_merge_components (AsDataPool *dpool, AsComponent *src_cpt, AsComponent *dest_cpt)
{
	guint i;
	gchar **cats;
	gchar **pkgnames;

	/* FIXME: We only do this for GNOME Software compatibility. In future, we need better rules on what to merge how, and
	 * whether we want to merge stuff at all. */

	cats = as_component_get_categories (src_cpt);
	if (cats != NULL) {
		g_autoptr(GHashTable) cat_table = NULL;
		gchar **new_cats = NULL;

		cat_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		for (i = 0; cats[i] != NULL; i++) {
			g_hash_table_add (cat_table, g_strdup (cats[i]));
		}
		cats = as_component_get_categories (dest_cpt);
		if (cats != NULL) {
			for (i = 0; cats[i] != NULL; i++) {
				g_hash_table_add (cat_table, g_strdup (cats[i]));
			}
		}

		new_cats = (gchar**) g_hash_table_get_keys_as_array (cat_table, NULL);
		as_component_set_categories (dest_cpt, new_cats);
	}

	pkgnames = as_component_get_pkgnames (src_cpt);
	if ((pkgnames != NULL) && (pkgnames[0] != '\0'))
		as_component_set_pkgnames (dest_cpt, as_component_get_pkgnames (src_cpt));

	if (as_component_has_bundle (src_cpt))
		as_component_set_bundles_table (dest_cpt, as_component_get_bundles_table (src_cpt));
}

/**
 * as_data_pool_add_component:
 * @dpool: An instance of #AsDataPool
 * @cpt: The #AsComponent to add to the pool.
 * @error: A #GError or %NULL
 *
 * Register a new component in the AppStream metadata pool.
 *
 * Returns: %TRUE if the new component was successfully added to the pool.
 */
gboolean
as_data_pool_add_component (AsDataPool *dpool, AsComponent *cpt, GError **error)
{
	const gchar *cpt_id;
	AsComponent *existing_cpt;
	int priority;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_return_val_if_fail (cpt != NULL, FALSE);

	cpt_id = as_component_get_id (cpt);
	existing_cpt = g_hash_table_lookup (priv->cpt_table, cpt_id);

	/* add additional data to the component, e.g. external screenshots. Also refines
	 * the component's icon paths */
	as_component_complete (cpt, priv->screenshot_service_url, priv->icon_paths);

	if (existing_cpt == NULL) {
		g_hash_table_insert (priv->cpt_table,
					g_strdup (cpt_id),
					g_object_ref (cpt));
		return TRUE;
	}

	/* if we are here, we have duplicates */
	priority = as_component_get_priority (existing_cpt);
	if (priority < as_component_get_priority (cpt)) {
		g_hash_table_replace (priv->cpt_table,
					g_strdup (cpt_id),
					g_object_ref (cpt));
		g_debug ("Replaced '%s' with data of higher priority.", cpt_id);
	} else {
		gboolean ecpt_has_name;
		gboolean ncpt_has_name;

		if ((!as_component_has_bundle (existing_cpt)) && (as_component_has_bundle (cpt))) {
			GHashTable *bundles;
			/* propagate bundle information to existing component */
			bundles = as_component_get_bundles_table (cpt);
			as_component_set_bundles_table (existing_cpt, bundles);
			return TRUE;
		}

		ecpt_has_name = as_component_get_name (existing_cpt) != NULL;
		ncpt_has_name = as_component_get_name (cpt) != NULL;
		if ((ecpt_has_name) && (!ncpt_has_name)) {
			/* existing component is updated with data from the new one */
			as_data_pool_merge_components (dpool, cpt, existing_cpt);
			g_debug ("Merged stub data for component %s (<-, based on name)", cpt_id);
			return TRUE;
		}

		if ((!ecpt_has_name) && (ncpt_has_name)) {
			/* new component is updated with data from the existing component,
			 * then it replaces the prior metadata */
			as_data_pool_merge_components (dpool, existing_cpt, cpt);
			g_hash_table_replace (priv->cpt_table,
					g_strdup (cpt_id),
					g_object_ref (cpt));
			g_debug ("Merged stub data for component %s (->, based on name)", cpt_id);
			return TRUE;
		}

		if (as_component_get_architecture (cpt) != NULL) {
			if (as_arch_compatible (as_component_get_architecture (cpt), priv->current_arch)) {
				const gchar *earch;
				/* this component is compatible with our current architecture */

				earch = as_component_get_architecture (existing_cpt);
				if (earch != NULL) {
					if (as_arch_compatible (earch, priv->current_arch)) {
						g_hash_table_replace (priv->cpt_table,
									g_strdup (cpt_id),
									g_object_ref (cpt));
						g_debug ("Preferred component for native architecture for %s (was %s)", cpt_id, earch);
						return TRUE;
					} else {
						g_debug ("Ignored additional entry for '%s' on architecture %s.", cpt_id, earch);
						return FALSE;
					}
				}
			}
		}

		if (priority == as_component_get_priority (cpt)) {
			g_set_error (error,
					AS_DATA_POOL_ERROR,
					AS_DATA_POOL_ERROR_COLLISION,
					"Detected colliding ids: %s was already added with the same priority.", cpt_id);
			return FALSE;
		} else {
			g_set_error (error,
					AS_DATA_POOL_ERROR,
					AS_DATA_POOL_ERROR_COLLISION,
					"Detected colliding ids: %s was already added with a higher priority.", cpt_id);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * as_data_pool_update_extension_info:
 *
 * Populate the "extensions" property of an #AsComponent, using the
 * "extends" information from other components.
 */
static void
as_data_pool_update_extension_info (AsDataPool *dpool, AsComponent *cpt)
{
	guint i;
	GPtrArray *extends;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	extends = as_component_get_extends (cpt);
	if ((extends == NULL) || (extends->len == 0))
		return;

	for (i = 0; i < extends->len; i++) {
		AsComponent *extended_cpt;
		const gchar *extended_cid = (const gchar*) g_ptr_array_index (extends, i);

		extended_cpt = g_hash_table_lookup (priv->cpt_table, extended_cid);
		if (extended_cpt == NULL) {
			g_debug ("%s extends %s, but %s was not found.", as_component_get_id (cpt), extended_cid, extended_cid);
			return;
		}

		as_component_add_extension (extended_cpt, as_component_get_id (cpt));
	}
}

/**
 * as_data_pool_refine_data:
 *
 * Automatically refine the data we have about software components in the pool.
 *
 * Returns: %TRUE if all metadata was used, %FALSE if we skipped some stuff.
 */
static gboolean
as_data_pool_refine_data (AsDataPool *dpool)
{
	GHashTableIter iter;
	gpointer key, value;
	GHashTable *refined_cpts;
	gboolean ret = TRUE;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	/* since we might remove stuff from the pool, we need a new table to store the result */
	refined_cpts = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						(GDestroyNotify) g_object_unref);

	g_hash_table_iter_init (&iter, priv->cpt_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		AsComponent *cpt;
		const gchar *cid;
		cpt = AS_COMPONENT (value);
		cid = (const gchar*) key;

		/* validate the component */
		if (!as_component_is_valid (cpt)) {
			g_debug ("WARNING: Skipped component '%s': The component is invalid.", as_component_get_id (cpt));
			ret = FALSE;
			continue;
		}

		/* set the "extension" information */
		as_data_pool_update_extension_info (dpool, cpt);

		/* add to results table */
		g_hash_table_insert (refined_cpts,
					g_strdup (cid),
					g_object_ref (cpt));
	}

	/* set refined components as new pool content */
	g_hash_table_unref (priv->cpt_table);
	priv->cpt_table = refined_cpts;

	return ret;
}

/**
 * as_data_pool_load_metadata:
 *
 * Load fresh metadata from AppStream directories.
 */
gboolean
as_data_pool_load_metadata (AsDataPool *dpool)
{
	GPtrArray *cpts;
	guint i;
	gboolean ret;
	g_autoptr(AsMetadata) metad = NULL;
	GError *error = NULL;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	/* prepare metadata parser */
	metad = as_metadata_new ();
	as_metadata_set_parser_mode (metad, AS_PARSER_MODE_DISTRO);
	as_metadata_set_locale (metad, priv->locale);

	/* find AppStream XML and YAML metadata */
	ret = TRUE;
	for (i = 0; i < priv->mdata_dirs->len; i++) {
		const gchar *root_path;
		g_autofree gchar *xml_path = NULL;
		g_autofree gchar *yaml_path = NULL;
		g_autoptr(GPtrArray) mdata_files = NULL;
		gboolean subdir_found = FALSE;
		guint j;

		root_path = (const gchar *) g_ptr_array_index (priv->mdata_dirs, i);
		if (!g_file_test (root_path, G_FILE_TEST_EXISTS))
			continue;

		xml_path = g_build_filename (root_path, "xmls", NULL);
		yaml_path = g_build_filename (root_path, "yaml", NULL);
		mdata_files = g_ptr_array_new_with_free_func (g_free);

		/* find XML data */
		if (g_file_test (xml_path, G_FILE_TEST_IS_DIR)) {
			g_autoptr(GPtrArray) xmls = NULL;

			subdir_found = TRUE;
			g_debug ("Searching for data in: %s", xml_path);
			xmls = as_utils_find_files_matching (xml_path, "*.xml*", FALSE, NULL);
			if (xmls != NULL) {
				for (j = 0; j < xmls->len; j++) {
					const gchar *val;
					val = (const gchar *) g_ptr_array_index (xmls, j);
					g_ptr_array_add (mdata_files,
								g_strdup (val));
				}
			}
		}

		/* find YAML data */
		if (g_file_test (yaml_path, G_FILE_TEST_IS_DIR)) {
			g_autoptr(GPtrArray) yamls = NULL;

			subdir_found = TRUE;
			g_debug ("Searching for data in: %s", yaml_path);
			yamls = as_utils_find_files_matching (yaml_path, "*.yml*", FALSE, NULL);
			if (yamls != NULL) {
				for (j = 0; j < yamls->len; j++) {
					const gchar *val;
					val = (const gchar *) g_ptr_array_index (yamls, j);
					g_ptr_array_add (mdata_files,
								g_strdup (val));
				}
			}
		}

		if (!subdir_found) {
			g_debug ("No metadata directories found in '%s'", root_path);
			continue;
		}

		/* parse the found data */
		for (j = 0; j < mdata_files->len; j++) {
			g_autoptr(GFile) infile = NULL;
			const gchar *fname;

			fname = (const gchar*) g_ptr_array_index (mdata_files, j);
			g_debug ("Reading: %s", fname);

			infile = g_file_new_for_path (fname);
			if (!g_file_query_exists (infile, NULL)) {
				g_warning ("Metadata file '%s' does not exist.", fname);
				continue;
			}

			as_metadata_parse_file (metad, infile, &error);
			if (error != NULL) {
				g_debug ("WARNING: %s", error->message);
				g_error_free (error);
				error = NULL;
				ret = FALSE;
			}
		}
	}

	cpts = as_metadata_get_components (metad);
	for (i = 0; i < cpts->len; i++) {
		as_data_pool_add_component (dpool,
					    AS_COMPONENT (g_ptr_array_index (cpts, i)),
					    &error);
		if (error != NULL) {
			g_debug ("Data ignored: %s", error->message);
			g_error_free (error);
			error = NULL;
		}
	}

	return ret;
}

/**
 * as_data_pool_clear:
 * @dpool: An #AsDataPool.
 *
 * Remove all metadat from the pool.
 */
void
as_data_pool_clear (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	if (g_hash_table_size (priv->cpt_table) > 0) {
		g_hash_table_unref (priv->cpt_table);
		priv->cpt_table = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 g_free,
							 (GDestroyNotify) g_object_unref);
	}
}

/**
 * as_data_pool_ctime_newer:
 *
 * Returns: %TRUE if ctime of file is newer than the cached time.
 */
static gboolean
as_data_pool_ctime_newer (AsDataPool *dpool, const gchar *dir)
{
	struct stat sb;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	if (stat (dir, &sb) < 0)
		return FALSE;

	if (sb.st_ctime > priv->cache_ctime)
		return TRUE;

	return FALSE;
}

/**
 * as_data_pool_appstream_data_changed:
 */
static gboolean
as_data_pool_metadata_changed (AsDataPool *dpool)
{
	guint i;
	GPtrArray *locations;

	locations = as_data_pool_get_metadata_locations (dpool);
	for (i = 0; i < locations->len; i++) {
		g_autofree gchar *xml_dir = NULL;
		g_autofree gchar *yaml_dir = NULL;
		const gchar *dir_root = (const gchar*) g_ptr_array_index (locations, i);

		if (as_data_pool_ctime_newer (dpool, dir_root))
			return TRUE;

		xml_dir = g_build_filename (dir_root, "xmls", NULL);
		if (as_data_pool_ctime_newer (dpool, xml_dir))
			return TRUE;

		yaml_dir = g_build_filename (dir_root, "yaml", NULL);
		if (as_data_pool_ctime_newer (dpool, yaml_dir))
			return TRUE;
	}

	return FALSE;
}

/**
 * as_data_pool_load:
 * @dpool: An instance of #AsDataPool.
 * @error: A #GError or %NULL.
 *
 * Builds an index of all found components in the watched locations.
 * The function will try to get as much data into the pool as possible, so even if
 * the update completes with %FALSE, it might still have added components to the pool.
 *
 * The function will load from all possible data sources, preferring caches if they
 * are up to date.
 *
 * Returns: %TRUE if update completed without error.
 **/
gboolean
as_data_pool_load (AsDataPool *dpool, GCancellable *cancellable, GError **error)
{
	gboolean ret;
	gboolean ret2;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	/* load means to reload, so we get rid of all the old data */
	if (g_hash_table_size (priv->cpt_table) > 0) {
		g_hash_table_unref (priv->cpt_table);
		priv->cpt_table = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 g_free,
							 (GDestroyNotify) g_object_unref);
	}

	if (!as_data_pool_metadata_changed (dpool)) {
		g_autofree gchar *fname = NULL;
		g_debug ("Caches are up to date, using existing metadata.");

		fname = g_strdup_printf ("%s/%s.pb", priv->sys_cache_path, priv->locale);
		if (g_file_test (fname, G_FILE_TEST_EXISTS)) {
			return as_data_pool_load_cache_file (dpool, fname, error);
		} else {
			g_debug ("Missing cache for language '%s', attempting to load fresh data.", priv->locale);
		}
	}

	/* read all AppStream metadata that we can find */
	ret = as_data_pool_load_metadata (dpool);

	/* automatically refine the metadata we have in the pool */
	ret2 = as_data_pool_refine_data (dpool);

	return ret && ret2;
}

/**
 * as_data_pool_load_cache_file:
 * @dpool: An instance of #AsDataPool.
 * @fname: Filename of the cache file to load into the pool.
 * @error: A #GError or %NULL.
 *
 * Load AppStream metadata from a cache file.
 */
gboolean
as_data_pool_load_cache_file (AsDataPool *dpool, const gchar *fname, GError **error)
{
	g_autoptr(GPtrArray) cpts = NULL;
	guint i;
	GError *tmp_error = NULL;

	/* load list of components in cache */
	cpts = as_cache_read (fname, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* add cache objects to the pool */
	for (i = 0; i < cpts->len; i++) {
		as_data_pool_add_component (dpool, AS_COMPONENT (g_ptr_array_index (cpts, i)), &tmp_error);
		if (tmp_error != NULL) {
			g_warning ("Cached data ignored: %s", tmp_error->message);
			g_error_free (tmp_error);
			tmp_error = NULL;
		}
	}

	return TRUE;
}

/**
 * as_data_pool_save_cache_file:
 * @dpool: An instance of #AsDataPool.
 * @fname: Filename of the cache file the pool contents should be dumped to.
 * @error: A #GError or %NULL.
 *
 * Serialize AppStream metadata to a cache file.
 */
gboolean
as_data_pool_save_cache_file (AsDataPool *dpool, const gchar *fname, GError **error)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_autoptr(GPtrArray) cpts = NULL;

	cpts = as_data_pool_get_components (dpool);
	as_cache_write (fname, priv->locale, cpts, error);
	g_ptr_array_unref (cpts);

	return TRUE;
}

/**
 * as_data_pool_get_components:
 * @dpool: An instance of #AsDataPool.
 *
 * Get a list of found components.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent instances.
 */
GPtrArray*
as_data_pool_get_components (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	GHashTableIter iter;
	gpointer value;
	GPtrArray *cpts;

	cpts = g_ptr_array_new_with_free_func (g_object_unref);
	g_hash_table_iter_init (&iter, priv->cpt_table);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		AsComponent *cpt = AS_COMPONENT (value);
		g_ptr_array_add (cpts, g_object_ref (cpt));
	}

	return cpts;
}

/**
 * as_data_pool_get_component_by_id:
 * @dpool: An instance of #AsDataPool.
 * @id: The AppStream-ID to look for.
 *
 * Get a specific component by its ID.
 *
 * Returns: (transfer full): An #AsComponent
 */
AsComponent*
as_data_pool_get_component_by_id (AsDataPool *dpool, const gchar *id)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	gpointer match;

	if (id == NULL)
		return NULL;

	match = g_hash_table_lookup (priv->cpt_table, id);
	if (match == NULL)
		return NULL;

	return g_object_ref (AS_COMPONENT (match));
}

/**
 * as_data_pool_get_components_by_provided_item:
 * @dpool: An instance of #AsDataPool.
 * @kind: An #AsProvidesKind
 * @item: The value of the provided item.
 * @error: A #GError or %NULL.
 *
 * Find components in the AppStream data pool whcih provide a certain item.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found.
 */
GPtrArray*
as_data_pool_get_components_by_provided_item (AsDataPool *dpool,
					      AsProvidedKind kind,
					      const gchar *item,
					      GError **error)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	GHashTableIter iter;
	gpointer value;
	GPtrArray *results;

	if (item == NULL) {
		g_set_error_literal (error,
					AS_DATA_POOL_ERROR,
					AS_DATA_POOL_ERROR_TERM_INVALID,
					"Search term must not be NULL.");
		return NULL;
	}

	results = g_ptr_array_new_with_free_func (g_object_unref);
	g_hash_table_iter_init (&iter, priv->cpt_table);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		g_autoptr(GList) provided = NULL;
		GList *l;
		AsComponent *cpt = AS_COMPONENT (value);

		provided = as_component_get_provided (cpt);
		for (l = provided; l != NULL; l = l->next) {
			AsProvided *prov = AS_PROVIDED (l->data);
			if (kind != AS_PROVIDED_KIND_UNKNOWN) {
				/* check if the kind matches. an unknown kind matches all provides types */
				if (as_provided_get_kind (prov) != kind)
					continue;
			}

			if (as_provided_has_item (prov, item))
				g_ptr_array_add (results, g_object_ref (cpt));
		}
	}

	return results;
}

/**
 * as_data_pool_get_components_by_kind:
 * @dpool: An instance of #AsDatabase.
 * @kind: An #AsComponentKind.
 * @error: A #GError or %NULL.
 *
 * Return a list of all components in the pool which are of a certain kind.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found.
 */
GPtrArray*
as_data_pool_get_components_by_kind (AsDataPool *dpool,
				     AsComponentKind kind,
				     GError **error)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	GHashTableIter iter;
	gpointer value;
	GPtrArray *results;

	if (kind >= AS_COMPONENT_KIND_LAST) {
		g_set_error_literal (error,
					AS_DATA_POOL_ERROR,
					AS_DATA_POOL_ERROR_TERM_INVALID,
					_("Can not search for unknown component type."));
		return NULL;
	}

	if (kind == AS_COMPONENT_KIND_UNKNOWN) {
		g_set_error_literal (error,
					AS_DATA_POOL_ERROR,
					AS_DATA_POOL_ERROR_TERM_INVALID,
					_("Can not search for unknown component type."));
		return NULL;
	}

	results = g_ptr_array_new_with_free_func (g_object_unref);
	g_hash_table_iter_init (&iter, priv->cpt_table);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		AsComponent *cpt = AS_COMPONENT (value);

		if (as_component_get_kind (cpt) == kind)
				g_ptr_array_add (results, g_object_ref (cpt));
	}

	return results;
}

/**
 * as_data_pool_get_components_by_categories:
 * @dpool: An instance of #AsDatabase.
 * @categories: A semicolon-separated list of XDG categories to include.
 *
 * Return a list of components which are in one of the categories.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found.
 */
GPtrArray*
as_data_pool_get_components_by_categories (AsDataPool *dpool, const gchar *categories)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	GHashTableIter iter;
	gpointer value;
	g_auto(GStrv) cats = NULL;
	guint i;
	GPtrArray *results;

	cats = g_strsplit (categories, ";", -1);
	results = g_ptr_array_new_with_free_func (g_object_unref);

	/* sanity check */
	for (i = 0; cats[i] != NULL; i++) {
		if (!as_utils_is_category_name (cats[i])) {
			g_warning ("'%s' is not a valid XDG category name, search results might be invalid or empty.", cats[i]);
		}
	}

	g_hash_table_iter_init (&iter, priv->cpt_table);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		gchar **cpt_cats;
		guint j;
		AsComponent *cpt = AS_COMPONENT (value);

		cpt_cats = as_component_get_categories (cpt);
		if (cpt_cats == NULL)
			continue;

		for (i = 0; cats[i] != NULL; i++) {
			for (j = 0; cpt_cats[j] != NULL; j++) {
				if (g_strcmp0 (cats[i], cpt_cats[j]) == 0)
					g_ptr_array_add (results, g_object_ref (cpt));
			}
		}
	}

	return results;
}

/**
 * as_data_pool_sanitize_search_term:
 *
 * Improve the search term slightly, by stripping whitespaces and
 * removing greylist words.
 */
static gchar*
as_data_pool_sanitize_search_term (AsDataPool *dpool, const gchar *term)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_autoptr(AsStemmer) stemmer = NULL;
	g_autofree gchar *tmp_term = NULL;
	gchar *term_stemmed = NULL;
	guint i;

	if (term == NULL)
		return NULL;
	tmp_term = g_utf8_strdown (term, -1);

	/* filter query by greylist (to avoid overly generic search terms) */
	for (i = 0; priv->term_greylist[i] != NULL; i++) {
		gchar *str;
		str = as_str_replace (tmp_term, priv->term_greylist[i], "");
		g_free (tmp_term);
		tmp_term = str;
	}

	/* restore query if it was just greylist words */
	if (g_strcmp0 (tmp_term, "") == 0) {
		g_debug ("grey-list replaced all terms, restoring");
		g_free (tmp_term);
		tmp_term = g_utf8_strdown (term, -1);
	}

	/* we have to strip the leading and trailing whitespaces to avoid having
	 * different results for e.g. 'font ' and 'font' (LP: #506419)
	 */
	g_strstrip (tmp_term);

	/* stem the string */
	stemmer = as_stemmer_new ();
	term_stemmed = as_stemmer_stem (stemmer, tmp_term);

	return term_stemmed;
}

/**
 * as_data_pool_search:
 * @dpool: An instance of #AsDataPool
 * @term: A search term/string
 *
 * Search for a list of components matching the search terms.
 * The list will be unordered.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of the found #AsComponent objects.
 *
 * Since: 0.9.7
 */
GPtrArray*
as_data_pool_search (AsDataPool *dpool, const gchar *term)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_autofree gchar *sane_term = NULL;
	GPtrArray *results;
	GHashTableIter iter;
	gpointer value;

	/* sanitize user's search term */
	sane_term = as_data_pool_sanitize_search_term (dpool, term);
	results = g_ptr_array_new_with_free_func (g_object_unref);
	g_debug ("Searching for: %s", sane_term);

	g_hash_table_iter_init (&iter, priv->cpt_table);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		guint res;
		AsComponent *cpt = AS_COMPONENT (value);

		res = as_component_search_matches (cpt, sane_term);
		if (res == 0)
			continue;

		g_ptr_array_add (results, g_object_ref (cpt));
	}

	return results;
}

/**
 * as_data_pool_refresh_cache:
 * @dpool: An instance of #AsDataPool.
 * @force: Enforce refresh, even if source data has not changed.
 *
 * Update the AppStream cache. There is normally no need to call this function manually, because cache updates are handled
 * transparently in the background.
 *
 * Returns: %TRUE if the cache was updated, %FALSE on error or if the cache update was not necessary and has been skipped.
 */
gboolean
as_data_pool_refresh_cache (AsDataPool *dpool, gboolean force, GError **error)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	gboolean ret = FALSE;
	gboolean ret_poolupdate;
	g_autofree gchar *cache_fname = NULL;
	g_autoptr(GError) tmp_error = NULL;

	/* try to create cache directory, in case it doesn't exist */
	g_mkdir_with_parents (priv->sys_cache_path, 0755);
	if (!as_utils_is_writable (priv->sys_cache_path)) {
		g_set_error (error,
				AS_DATA_POOL_ERROR,
				AS_DATA_POOL_ERROR_TARGET_NOT_WRITABLE,
				_("Cache location '%s' is not writable."), priv->sys_cache_path);
		return FALSE;
	}

	/* collect metadata */
#ifdef APT_SUPPORT
	/* currently, we only do something here if we are running with explicit APT support compiled in */
	as_data_pool_scan_apt (dpool, force, &tmp_error);
	if (tmp_error != NULL) {
		/* the exact error is not forwarded here, since we might be able to partially update the cache */
		g_warning ("Error while collecting metadata: %s", tmp_error->message);
		g_error_free (tmp_error);
		tmp_error = NULL;
	}
#endif

	/* create the filename of our cache */
	cache_fname = g_strdup_printf ("%s/%s.pb", priv->sys_cache_path, priv->locale);

	/* check if we need to refresh the cache
	 * (which is only necessary if the AppStream data has changed) */
	if (!as_data_pool_metadata_changed (dpool)) {
		g_debug ("Data did not change, no cache refresh needed.");
		if (force) {
			g_debug ("Forcing refresh anyway.");
		} else {
			return FALSE;
		}
	}
	g_debug ("Refreshing AppStream cache");

	/* find them wherever they are */
	ret_poolupdate = as_data_pool_load (dpool, NULL, &tmp_error);
	if (tmp_error != NULL) {
		/* the exact error is not forwarded here, since we might be able to partially update the cache */
		g_warning ("Error while updating the in-memory data pool: %s", tmp_error->message);
		g_error_free (tmp_error);
		tmp_error = NULL;
	}

	/* save the cache object */
	as_data_pool_save_cache_file (dpool, cache_fname, &tmp_error);
	if (tmp_error != NULL) {
		/* the exact error is not forwarded here, since we might be able to partially update the cache */
		g_warning ("Error while updating the cache: %s", tmp_error->message);
		g_error_free (tmp_error);
		tmp_error = NULL;
		ret = FALSE;
	} else {
		ret = TRUE;
	}

	if (ret) {
		if (!ret_poolupdate) {
			g_set_error (error,
				AS_DATA_POOL_ERROR,
				AS_DATA_POOL_ERROR_INCOMPLETE,
				_("AppStream data pool was loaded, but some metadata was ignored due to errors."));
		}
		/* update the cache mtime, to not needlessly rebuild it again */
		as_touch_location (cache_fname);
		as_data_pool_check_cache_ctime (dpool);
	} else {
		g_set_error (error,
				AS_DATA_POOL_ERROR,
				AS_DATA_POOL_ERROR_FAILED,
				_("AppStream cache update failed."));
	}

	return TRUE;
}

/**
 * as_data_pool_set_locale:
 * @dpool: An instance of #AsDataPool.
 * @locale: the locale.
 *
 * Sets the current locale which should be used when parsing metadata.
 **/
void
as_data_pool_set_locale (AsDataPool *dpool, const gchar *locale)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_free (priv->locale);
	priv->locale = g_strdup (locale);
}

/**
 * as_data_pool_get_locale:
 * @dpool: An instance of #AsDataPool.
 *
 * Gets the currently used locale.
 *
 * Returns: Locale used for metadata parsing.
 **/
const gchar *
as_data_pool_get_locale (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	return priv->locale;
}

/**
 * as_data_pool_get_metadata_locations:
 * @dpool: An instance of #AsDataPool.
 *
 * Return a list of all locations which are searched for metadata.
 *
 * Returns: (transfer none) (element-type utf8): A string-list of watched (absolute) filepaths
 **/
GPtrArray*
as_data_pool_get_metadata_locations (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	return priv->mdata_dirs;
}

/**
 * as_data_pool_set_metadata_locations:
 * @dpool: An instance of #AsDataPool.
 * @dirs: (array zero-terminated=1): a zero-terminated array of data input directories.
 *
 * Set locations for the data pool to read it's data from.
 * This is mainly used for testing purposes. Each location should have an
 * "xmls" and/or "yaml" subdirectory with the actual data as (compressed)
 * AppStream XML or DEP-11 YAML in it.
 */
void
as_data_pool_set_metadata_locations (AsDataPool *dpool, gchar **dirs)
{
	guint i;
	GPtrArray *icondirs;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	/* clear array */
	g_ptr_array_set_size (priv->mdata_dirs, 0);

	icondirs = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; priv->icon_paths[i] != NULL; i++) {
		g_ptr_array_add (icondirs, g_strdup (priv->icon_paths[i]));
	}

	for (i = 0; dirs[i] != NULL; i++) {
		g_autofree gchar *path = NULL;

		g_ptr_array_add (priv->mdata_dirs, g_strdup (dirs[i]));

		path = g_build_filename (dirs[i], "icons", NULL);
		if (g_file_test (path, G_FILE_TEST_EXISTS))
			g_ptr_array_add (icondirs, g_strdup (path));
	}

	/* add new icon search locations */
	g_strfreev (priv->icon_paths);
	priv->icon_paths = as_ptr_array_to_strv (icondirs);
}

/**
 * as_data_pool_get_cache_flags:
 * @dpool: An instance of #AsDataPool.
 *
 * Get the #AsCacheFlags for this data pool.
 */
AsCacheFlags
as_data_pool_get_cache_flags (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	return priv->cflags;
}

/**
 * as_data_pool_set_cache_flags:
 * @dpool: An instance of #AsDataPool.
 * @flags: The new #AsCacheFlags.
 *
 * Set the #AsCacheFlags for this data pool.
 */
void
as_data_pool_set_cache_flags (AsDataPool *dpool, AsCacheFlags flags)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	priv->cflags = flags;
}

/**
 * as_data_pool_get_cache_age:
 * @dpool: An instance of #AsDataPool.
 *
 * Get the age of our internal cache.
 */
time_t
as_data_pool_get_cache_age (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	return priv->cache_ctime;
}

/**
 * as_data_pool_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_data_pool_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsDataPool");
	return quark;
}

/**
 * as_data_pool_new:
 *
 * Creates a new #AsDataPool.
 *
 * Returns: (transfer full): a #AsDataPool
 *
 **/
AsDataPool*
as_data_pool_new (void)
{
	AsDataPool *dpool;
	dpool = g_object_new (AS_TYPE_DATA_POOL, NULL);
	return AS_DATA_POOL (dpool);
}

/* DEPRECATED */

/**
 * as_data_pool_update:
 */
gboolean
as_data_pool_update (AsDataPool *dpool, GError **error)
{
	return as_data_pool_load (dpool, NULL, error);
}
