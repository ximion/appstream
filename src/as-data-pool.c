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
	gchar *scr_base_url;
	gchar *locale;
	gchar *current_arch;

	GPtrArray *mdata_dirs;

	gchar **icon_paths;
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
	priv->scr_base_url = as_distro_details_get_str (distro, "ScreenshotUrl");
	if (priv->scr_base_url == NULL) {
		g_debug ("Unable to determine screenshot service for distribution '%s'. Using the Debian services.", as_distro_details_get_name (distro));
		priv->scr_base_url = g_strdup ("http://screenshots.debian.net");
	}

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
}

/**
 * as_data_pool_finalize:
 **/
static void
as_data_pool_finalize (GObject *object)
{
	AsDataPool *dpool = AS_DATA_POOL (object);
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	g_free (priv->scr_base_url);
	g_hash_table_unref (priv->cpt_table);

	g_ptr_array_unref (priv->mdata_dirs);
	g_strfreev (priv->icon_paths);

	g_free (priv->current_arch);

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
 * as_data_pool_add_new_component:
 *
 * Register a new component in the global AppStream metadata pool.
 */
static void
as_data_pool_add_new_component (AsDataPool *dpool, AsComponent *cpt)
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
	as_component_complete (cpt, priv->scr_base_url, priv->icon_paths);

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
 * as_data_pool_get_metadata_locations:
 * @dpool: An instance of #AsDataPool.
 *
 * Return a list of all locations which are searched for metadata.
 *
 * Returns: (transfer none): A string-list of watched (absolute) filepaths
 **/
GPtrArray*
as_data_pool_get_metadata_locations (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	return priv->mdata_dirs;
}

/**
 * as_data_pool_load_metadata:
 */
static gboolean
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
		as_data_pool_add_new_component (dpool,
						AS_COMPONENT (g_ptr_array_index (cpts, i)));
	}

	return ret;
}

/**
 * as_data_pool_update:
 * @dpool: An instance of #AsDataPool.
 * @error: A #GError or %NULL.
 *
 * Builds an index of all found components in the watched locations.
 * The function will try to get as much data into the pool as possible, so even if
 * the updates completes with %FALSE, it might still add components to the pool.
 *
 * Returns: %TRUE if update completed without error.
 **/
gboolean
as_data_pool_update (AsDataPool *dpool, GError **error)
{
	gboolean ret;
	gboolean ret2;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	/* just in case, clear the components table */
	if (g_hash_table_size (priv->cpt_table) > 0) {
		g_hash_table_unref (priv->cpt_table);
		priv->cpt_table = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 g_free,
							 (GDestroyNotify) g_object_unref);
	}

	/* read all AppStream metadata that we can find */
	ret = as_data_pool_load_metadata (dpool);

	/* automatically refine the metadata we have in the pool */
	ret2 = as_data_pool_refine_data (dpool);

	return ret && ret2;
}

/**
 * as_data_pool_get_components:
 * @dpool: An instance of #AsDataPool.
 *
 * Get a list of found components.
 *
 * Returns: (element-type AsComponent) (transfer container): a list of #AsComponent instances, free with g_list_free()
 */
GList*
as_data_pool_get_components (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	return g_hash_table_get_values (priv->cpt_table);
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
