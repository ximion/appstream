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

#include "data-providers/appstream-xml.h"
#ifdef DEBIAN_DEP11
#include "data-providers/debian-dep11.h"
#endif
#ifdef UBUNTU_APPINSTALL
#include "data-providers/ubuntu-appinstall.h"
#endif

const gchar* AS_APPSTREAM_XML_PATHS[4] = {AS_APPSTREAM_BASE_PATH "/xmls",
										"/var/cache/app-info/xmls",
										"/var/lib/app-info/xmls",
										NULL};
#define AS_PROVIDER_UBUNTU_APPINSTALL_DIR "/usr/share/app-install"

typedef struct _AsDataPoolPrivate	AsDataPoolPrivate;
struct _AsDataPoolPrivate
{
	GHashTable* cpt_table;
	GPtrArray* providers;
	gchar *scr_base_url;
	gboolean initialized;

	gchar **asxml_paths;
	gchar **dep11_paths;
	gchar **appinstall_paths;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsDataPool, as_data_pool, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_data_pool_get_instance_private (o))

/**
 * as_data_pool_finalize:
 **/
static void
as_data_pool_finalize (GObject *object)
{
	AsDataPool *dpool = AS_DATA_POOL (object);
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	g_ptr_array_unref (priv->providers);
	g_free (priv->scr_base_url);
	g_hash_table_unref (priv->cpt_table);

	g_strfreev (priv->appinstall_paths);
	g_strfreev (priv->asxml_paths);

	G_OBJECT_CLASS (as_data_pool_parent_class)->finalize (object);
}

/**
 * as_data_pool_init:
 **/
static void
as_data_pool_init (AsDataPool *dpool)
{
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

static void
as_data_pool_new_component_cb (AsDataProvider *sender, AsComponent* cpt, AsDataPool *dpool)
{
	const gchar *cpt_id;
	AsComponent *existing_cpt;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_return_if_fail (cpt != NULL);

	cpt_id = as_component_get_id (cpt);
	existing_cpt = g_hash_table_lookup (priv->cpt_table, cpt_id);

	/* add additional data to the component, e.g. external screenshots */
	as_component_complete (cpt, priv->scr_base_url);

	if (existing_cpt) {
		int priority;
		priority = as_component_get_priority (existing_cpt);
		if (priority < as_component_get_priority (cpt)) {
			g_hash_table_replace (priv->cpt_table,
								  g_strdup (cpt_id),
								  g_object_ref (cpt));
		} else {
			g_debug ("Detected colliding ids: %s was already added.", cpt_id);
		}
	} else {
		g_hash_table_insert (priv->cpt_table,
							g_strdup (cpt_id),
							g_object_ref (cpt));
	}
}

/**
 * as_data_pool_initialize:
 *
 * Initialize the pool with the predefined metadata locations.
 **/
void
as_data_pool_initialize (AsDataPool *dpool)
{
	AsDataProvider *dprov;
	guint i;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	/* regenerate data providers, in case someone is calling init twice */
	g_ptr_array_unref (priv->providers);
	priv->providers = g_ptr_array_new_with_free_func (g_object_unref);

	/* added by priority: Appstream XML has the highest, Ubuntu AppInstall the lowest priority */
	dprov = (AsDataProvider*) as_provider_appstream_xml_new ();
	as_data_provider_set_watch_files (dprov, priv->asxml_paths);
	g_ptr_array_add (priv->providers, dprov);
#ifdef DEBIAN_DEP11
	dprov = (AsDataProvider*) as_provider_dep11_new ();
	g_ptr_array_add (priv->providers, dprov);
#endif
#ifdef UBUNTU_APPINSTALL
	dprov = (AsDataProvider*) as_provider_ubuntu_appinstall_new ();
	as_data_provider_set_watch_files (dprov, priv->appinstall_paths);
	g_ptr_array_add (priv->providers, dprov);
#endif
	dprov = NULL;

	/* connect all data provider signals */
	for (i = 0; i < priv->providers->len; i++) {
		dprov = (AsDataProvider*) g_ptr_array_index (priv->providers, i);
		g_signal_connect_object (dprov, "component", (GCallback) as_data_pool_new_component_cb, dpool, 0);

		/* FIXME: For some reason, we need to increase refcount of the provider objects to not raise an error
		 * when calling unref() later.
		 * This doesn't make sense and needs further investigation.
		 */
		g_object_ref (dprov);
	}

	priv->initialized = TRUE;
}

/**
 * as_data_pool_get_watched_locations:
 * @dpool: a valid #AsDataPool instance
 *
 * Return a list of all locations which are searched for metadata.
 *
 * Returns: (transfer full): A string-list of watched (absolute) filepaths
 **/
gchar**
as_data_pool_get_watched_locations (AsDataPool *dpool)
{
	AsDataProvider *dprov;
	gchar **wfiles;
	guint i;
	GPtrArray *res_array;
	gchar **res;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_return_val_if_fail (dpool != NULL, NULL);

	res_array = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; i < priv->providers->len; i++) {
		guint j;
		dprov = (AsDataProvider*) g_ptr_array_index (priv->providers, i);
		wfiles = as_data_provider_get_watch_files (dprov);
		/* if there is nothing to watch for, we can just continue here */
		if (wfiles == NULL)
			continue;
		for (j = 0; wfiles[j] != NULL; j++) {
			g_ptr_array_add (res_array, wfiles[j]);
		}
	}

	res = as_ptr_array_to_strv (res_array);
	g_ptr_array_unref (res_array);
	return res;
}

/**
 * as_data_pool_update:
 *
 * Builds an index of all found components in the watched locations.
 **/
void
as_data_pool_update (AsDataPool *dpool)
{
	guint i;
	AsDataProvider *dprov;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	if (!priv->initialized) {
		g_error ("DataPool has never been initialized and can not find metadata.");
		return;
	}

	/* just in case, clear the components table */
	g_hash_table_unref (priv->cpt_table);
	priv->cpt_table = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						(GDestroyNotify) g_object_unref);

	/* call all AppStream data providers to return components they find */
	for (i = 0; i < priv->providers->len; i++) {
		dprov = (AsDataProvider*) g_ptr_array_index (priv->providers, i);
		as_data_provider_execute (dprov);
	}
}

/**
 * as_data_pool_get_components:
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
 * as_data_pool_set_xml_paths:
 * @values: (allow-none) (array zero-terminated=1): The new absolute paths to AppStream XML data
 */
void
as_data_pool_set_xml_paths (AsDataPool *dpool, gchar** values)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_strfreev (priv->asxml_paths);
	priv->asxml_paths = g_strdupv (values);
}

/**
 * as_data_pool_set_dep11_paths:
 * @values: (allow-none) (array zero-terminated=1): The new absolute paths to AppStream DEP-11 YAML data
 */
void
as_data_pool_set_dep11_paths (AsDataPool *dpool, gchar** values)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_strfreev (priv->dep11_paths);
	priv->dep11_paths = g_strdupv (values);
}

/**
 * as_data_pool_set_appinstall_paths:
 * @values: (allow-none) (array zero-terminated=1): The new absolute paths to AppInstall data
 */
void
as_data_pool_set_appinstall_paths (AsDataPool *dpool, gchar** values)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_strfreev (priv->appinstall_paths);
	priv->appinstall_paths = g_strdupv (values);
}

/**
 * as_data_pool_new:
 *
 * Creates a new #AsDataPool.
 *
 * Returns: (transfer full): a #AsDataPool
 *
 **/
AsDataPool *
as_data_pool_new (void)
{
	AsDataPool *dpool;
	AsDataPoolPrivate *priv;
	AsDistroDetails *distro;
	guint len;
	guint i;

	dpool = g_object_new (AS_TYPE_DATA_POOL, NULL);
	priv = GET_PRIVATE (dpool);

	priv->cpt_table = g_hash_table_new_full (g_str_hash,
								g_str_equal,
								g_free,
								(GDestroyNotify) g_object_unref);
	priv->providers = g_ptr_array_new_with_free_func (g_object_unref);

	distro = as_distro_details_new ();
	priv->scr_base_url = as_distro_details_config_distro_get_str (distro, "ScreenshotUrl");
	if (priv->scr_base_url == NULL) {
		g_debug ("Unable to determine screenshot service for distribution '%s'. Using the Debian services.", as_distro_details_get_distro_name (distro));
		priv->scr_base_url = g_strdup ("http://screenshots.debian.net");
	}
	g_object_unref (distro);

	/* set watched default directories for AppStream XML */
	len = G_N_ELEMENTS (AS_APPSTREAM_XML_PATHS);
	priv->asxml_paths = g_new0 (gchar *, len + 1);
	for (i = 0; i < len+1; i++) {
		if (i < len)
			priv->asxml_paths[i] = g_strdup (AS_APPSTREAM_XML_PATHS[i]);
		else
			priv->asxml_paths[i] = NULL;
	}

	/* set default directories for Ubuntu AppInstall */
	priv->appinstall_paths = g_new0 (gchar*, 1 + 1);
	priv->appinstall_paths[0] = g_strdup (AS_PROVIDER_UBUNTU_APPINSTALL_DIR);

	priv->initialized = FALSE;

	return AS_DATA_POOL (dpool);
}
