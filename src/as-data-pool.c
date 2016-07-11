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
 * @short_description: Collect and temporarily store metadata from different sources
 *
 * This class contains a temporary pool of metadata which has been collected from different
 * sources on the system.
 * It can directly be used, but usually it is accessed through a #AsDatabase instance.
 * This class is used by internally by the cache builder, but might be useful for others.
 *
 * See also: #AsDatabase
 */

#include "config.h"
#include "as-data-pool.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "pb/as-cache-internal.h"
#include "as-utils.h"
#include "as-utils-private.h"
#include "as-component-private.h"
#include "as-distro-details.h"
#include "as-settings-private.h"

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
 *
 * Register a new component in the global AppStream metadata pool.
 */
static void
as_data_pool_add_component (AsDataPool *dpool, AsComponent *cpt)
{
	const gchar *cpt_id;
	AsComponent *existing_cpt;
	int priority;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_return_if_fail (cpt != NULL);

	cpt_id = as_component_get_id (cpt);
	existing_cpt = g_hash_table_lookup (priv->cpt_table, cpt_id);

	/* add additional data to the component, e.g. external screenshots. Also refines
	 * the component's icon paths */
	as_component_complete (cpt, priv->screenshot_service_url, priv->icon_paths);

	if (existing_cpt == NULL) {
		g_hash_table_insert (priv->cpt_table,
					g_strdup (cpt_id),
					g_object_ref (cpt));
		return;
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
			return;
		}

		ecpt_has_name = as_component_get_name (existing_cpt) != NULL;
		ncpt_has_name = as_component_get_name (cpt) != NULL;
		if ((ecpt_has_name) && (!ncpt_has_name)) {
			/* existing component is updated with data from the new one */
			as_data_pool_merge_components (dpool, cpt, existing_cpt);
			g_debug ("Merged stub data for component %s (<-, based on name)", cpt_id);
			return;
		}

		if ((!ecpt_has_name) && (ncpt_has_name)) {
			/* new component is updated with data from the existing component,
			 * then it replaces the prior metadata */
			as_data_pool_merge_components (dpool, existing_cpt, cpt);
			g_hash_table_replace (priv->cpt_table,
					g_strdup (cpt_id),
					g_object_ref (cpt));
			g_debug ("Merged stub data for component %s (->, based on name)", cpt_id);
			return;
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
						return;
					} else {
						g_debug ("Ignored additional entry for '%s' on architecture %s.", cpt_id, earch);
						return;
					}
				}
			}
		}

		if (priority == as_component_get_priority (cpt)) {
			g_debug ("Detected colliding ids: %s was already added with the same priority.", cpt_id);
			return;
		} else {
			g_debug ("Detected colliding ids: %s was already added with a higher priority.", cpt_id);
			return;
		}
	}
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
			g_debug ("No metadata-specific subdirectories found in '%s'", root_path);
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
					    AS_COMPONENT (g_ptr_array_index (cpts, i)));
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
		as_data_pool_add_component (dpool, AS_COMPONENT (g_ptr_array_index (cpts, i)));
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
 * Returns: (element-type AsComponent) (transfer none): an array of #AsComponent instances.
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
	if (id == NULL)
		return NULL;

	return g_object_ref (AS_COMPONENT (g_hash_table_lookup (priv->cpt_table, id)));
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
	gchar *res_term;
	guint i;

	if (term == NULL)
		return NULL;
	res_term = g_strdup (term);

	/* filter query by greylist (to avoid overly generic search terms) */
	for (i = 0; priv->term_greylist[i] != NULL; i++) {
		gchar *str;
		str = as_str_replace (res_term, priv->term_greylist[i], "");
		g_free (res_term);
		res_term = str;
	}

	/* restore query if it was just greylist words */
	if (g_strcmp0 (res_term, "") == 0) {
		g_debug ("grey-list replaced all terms, restoring");
		g_free (res_term);
		res_term = g_strdup (term);
	}

	/* we have to strip the leading and trailing whitespaces to avoid having
	 * different results for e.g. 'font ' and 'font' (LP: #506419)
	 */
	g_strstrip (res_term);

	return res_term;
}

/**
 * as_data_pool_search:
 * @dpool: An instance of #AsDataPool
 * @term: A search term/string
 *
 * Search for a list of components matching the search terms.
 * The list will be unordered.
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

#ifdef APT_SUPPORT

#define YAML_SEPARATOR "---"
/* Compilers will optimise this to a constant */
#define YAML_SEPARATOR_LEN strlen(YAML_SEPARATOR)

/**
 * as_data_pool_get_yml_data_origin:
 *
 * Extract the data origin from the AppStream YAML file.
 * We don't use the #AsYAMLData loader, because it is much
 * slower than just loading the initial parts of the file and
 * extracting the origin manually.
 */
gchar*
as_data_pool_get_yml_data_origin (const gchar *fname)
{
	const gchar *data;
	GZlibDecompressor *zdecomp;
	g_autoptr(GFileInputStream) fistream = NULL;
	g_autoptr(GMemoryOutputStream) mem_os = NULL;
	g_autoptr(GInputStream) conv_stream = NULL;
	g_autoptr(GFile) file = NULL;
	g_autofree gchar *str = NULL;
	g_auto(GStrv) strv = NULL;
	guint i;
	gchar *start, *end;
	gchar *origin = NULL;

	file = g_file_new_for_path (fname);
	fistream = g_file_read (file, NULL, NULL);
	mem_os = (GMemoryOutputStream*) g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
	zdecomp = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
	conv_stream = g_converter_input_stream_new (G_INPUT_STREAM (fistream), G_CONVERTER (zdecomp));
	g_object_unref (zdecomp);

	g_output_stream_splice (G_OUTPUT_STREAM (mem_os), conv_stream, 0, NULL, NULL);
	data = (const gchar*) g_memory_output_stream_get_data (mem_os);

	/* faster than a regular expression?
	 * Get the first YAML document, then extract the origin string.
	 */
	if (data == NULL)
		return NULL;
	/* start points to the start of the document, i.e. "File:" normally */
	start = g_strstr_len (data, 400, YAML_SEPARATOR) + YAML_SEPARATOR_LEN;
	if (start == NULL)
		return NULL;
	/* Find the end of the first document - can be NULL if there is only one,
	 * for example if we're given YAML for an empty archive */
	end = g_strstr_len (start, -1, YAML_SEPARATOR);
	str = g_strndup (start, strlen(start) - (end ? strlen(end) : 0));

	strv = g_strsplit (str, "\n", -1);
	for (i = 0; strv[i] != NULL; i++) {
		g_auto(GStrv) strv2 = NULL;
		if (!g_str_has_prefix (strv[i], "Origin:"))
			continue;

		strv2 = g_strsplit (strv[i], ":", 2);
		g_strstrip (strv2[1]);
		origin = g_strdup (strv2[1]);

		/* remove quotes, in case the string is quoted */
		if ((g_str_has_prefix (origin, "\"")) && (g_str_has_suffix (origin, "\""))) {
			g_autofree gchar *tmp = NULL;

			tmp = origin;
			origin = g_strndup (tmp + 1, strlen (tmp) - 2);
		}

		break;
	}

	return origin;
}

/**
 * as_data_pool_extract_icons:
 */
static void
as_data_pool_extract_icons (const gchar *asicons_target,
			    const gchar *origin,
			    const gchar *apt_basename,
			    const gchar *apt_lists_dir,
			    const gchar *icons_size)
{
	g_autofree gchar *icons_tarball = NULL;
	g_autofree gchar *target_dir = NULL;
	g_autofree gchar *cmd = NULL;
	g_autofree gchar *stderr_txt = NULL;
	gint res;
	g_autoptr(GError) tmp_error = NULL;

	icons_tarball = g_strdup_printf ("%s/%sicons-%s.tar.gz", apt_lists_dir, apt_basename, icons_size);
	if (!g_file_test (icons_tarball, G_FILE_TEST_EXISTS)) {
		/* no icons found, stop here */
		return;
	}

	target_dir = g_build_filename (asicons_target, origin, icons_size, NULL);
	if (g_mkdir_with_parents (target_dir, 0755) > 0) {
		g_debug ("Unable to create '%s': %s", target_dir, g_strerror (errno));
		return;
	}

	if (!as_utils_is_writable (target_dir)) {
		g_debug ("Unable to write to '%s': Can't add AppStream icon-cache from APT to the pool.", target_dir);
		return;
	}

	cmd = g_strdup_printf ("/bin/tar -xzf '%s' -C '%s'", icons_tarball, target_dir);
	g_spawn_command_line_sync (cmd, NULL, &stderr_txt, &res, &tmp_error);
	if (tmp_error != NULL) {
		g_debug ("Failed to run tar: %s", tmp_error->message);
	}
	if (res != 0) {
		g_debug ("Running tar failed with exit-code %i: %s", res, stderr_txt);
	}
}

/**
 * as_data_pool_scan_apt:
 *
 * Scan for additional metadata in 3rd-party directories and move it to the right place.
 */
static void
as_data_pool_scan_apt (AsDataPool *dpool, gboolean force, GError **error)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	const gchar *apt_lists_dir = "/var/lib/apt/lists/";
	const gchar *appstream_yml_target = "/var/lib/app-info/yaml";
	const gchar *appstream_icons_target = "/var/lib/app-info/icons";
	g_autoptr(GPtrArray) yml_files = NULL;
	g_autoptr(GError) tmp_error = NULL;
	gboolean data_changed = FALSE;
	guint i;

	/* skip this step if the APT lists directory doesn't exist */
	if (!g_file_test (apt_lists_dir, G_FILE_TEST_IS_DIR)) {
		g_debug ("APT lists directory (%s) not found!", apt_lists_dir);
		return;
	}

	if (g_file_test (appstream_yml_target, G_FILE_TEST_IS_DIR)) {
		g_autoptr(GPtrArray) ytfiles = NULL;

		/* we can't modify the files here if we don't have write access */
		if (!as_utils_is_writable (appstream_yml_target)) {
			g_debug ("Unable to write to '%s': Can't add AppStream data from APT to the pool.", appstream_yml_target);
			return;
		}

		ytfiles = as_utils_find_files_matching (appstream_yml_target, "*", FALSE, &tmp_error);
		if (tmp_error != NULL) {
			g_warning ("Could not scan for broken symlinks in DEP-11 target: %s", tmp_error->message);
			return;
		}
		for (i = 0; i < ytfiles->len; i++) {
			const gchar *fname = (const gchar*) g_ptr_array_index (ytfiles, i);
			if (!g_file_test (fname, G_FILE_TEST_EXISTS)) {
				g_remove (fname);
				data_changed = TRUE;
			}
		}
	}

	yml_files = as_utils_find_files_matching (apt_lists_dir, "*Components-*.yml.gz", FALSE, &tmp_error);
	if (tmp_error != NULL) {
		g_warning ("Could not scan for APT-downloaded DEP-11 files: %s", tmp_error->message);
		return;
	}

	/* no data found? skip scan step */
	if (yml_files->len <= 0) {
		g_debug ("Couldn't find DEP-11 data in APT directories.");
		return;
	}

	/* We have to check if our metadata is in the target directory at all, and - if not - trigger a cache refresh.
	 * This is needed because APT is putting files with the *server* ctime/mtime into it's lists directory,
	 * and that time might be lower than the time the metadata cache was last updated, which may result
	 * in no cache update being triggered at all.
	 */
	for (i = 0; i < yml_files->len; i++) {
		g_autofree gchar *fbasename = NULL;
		g_autofree gchar *dest_fname = NULL;
		const gchar *fname = (const gchar*) g_ptr_array_index (yml_files, i);

		fbasename = g_path_get_basename (fname);
		dest_fname = g_build_filename (appstream_yml_target, fbasename, NULL);
		if (!g_file_test (dest_fname, G_FILE_TEST_EXISTS)) {
			data_changed = TRUE;
			g_debug ("File '%s' missing, cache update is needed.", dest_fname);
			break;
		}
	}

	/* get the last time we touched the database */
	if (!data_changed) {
		for (i = 0; i < yml_files->len; i++) {
			struct stat sb;
			const gchar *fname = (const gchar*) g_ptr_array_index (yml_files, i);
			if (stat (fname, &sb) < 0)
				continue;
			if (sb.st_ctime > priv->cache_ctime) {
				/* we need to update the cache */
				data_changed = TRUE;
				break;
			}
		}
	}

	/* no changes means nothing to do here */
	if ((!data_changed) && (!force))
		return;

	/* this is not really great, but we simply can't detect if we should remove an icons folder or not,
	 * or which specific icons we should drop from a folder.
	 * So, we hereby simply "own" the icons directory and all it's contents, anything put in there by 3rd-parties will
	 * be deleted.
	 * (And there should actually be no cases 3rd-parties put icons there on a Debian machine, since metadata in packages
	 * will land in /usr/share/app-info anyway)
	 */
	as_utils_delete_dir_recursive (appstream_icons_target);
	if (g_mkdir_with_parents (appstream_yml_target, 0755) > 0) {
		g_debug ("Unable to create '%s': %s", appstream_yml_target, g_strerror (errno));
		return;
	}

	for (i = 0; i < yml_files->len; i++) {
		g_autofree gchar *fbasename = NULL;
		g_autofree gchar *dest_fname = NULL;
		g_autofree gchar *origin = NULL;
		g_autofree gchar *file_baseprefix = NULL;
		const gchar *fname = (const gchar*) g_ptr_array_index (yml_files, i);

		fbasename = g_path_get_basename (fname);
		dest_fname = g_build_filename (appstream_yml_target, fbasename, NULL);

		if (!g_file_test (dest_fname, G_FILE_TEST_EXISTS)) {
			/* file not found, let's symlink */
			if (symlink (fname, dest_fname) != 0) {
				g_debug ("Unable to set symlink (%s -> %s): %s",
							fname,
							dest_fname,
							g_strerror (errno));
				continue;
			}
		} else if (!g_file_test (dest_fname, G_FILE_TEST_IS_SYMLINK)) {
			/* file found, but it isn't a symlink, try to rescue */
			g_debug ("Regular file '%s' found, which doesn't belong there. Removing it.", dest_fname);
			g_remove (dest_fname);
			continue;
		}

		/* get DEP-11 data origin */
		origin = as_data_pool_get_yml_data_origin (dest_fname);
		if (origin == NULL) {
			g_warning ("No origin found for file %s", fbasename);
			continue;
		}

		/* get base prefix for this file in the APT download cache */
		file_baseprefix = g_strndup (fbasename, strlen (fbasename) - strlen (g_strrstr (fbasename, "_") + 1));

		/* extract icons to their destination (if they exist at all */
		as_data_pool_extract_icons (appstream_icons_target,
						origin,
						file_baseprefix,
						apt_lists_dir,
						"64x64");
		as_data_pool_extract_icons (appstream_icons_target,
						origin,
						file_baseprefix,
						apt_lists_dir,
						"128x128");
	}

	/* ensure the cache-rebuild process notices these changes */
	as_touch_location (appstream_yml_target);
}
#endif

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
				AS_DATA_POOL_ERROR_CACHE_INCOMPLETE,
				_("AppStream cache update completed, but some metadata was ignored due to errors."));
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
