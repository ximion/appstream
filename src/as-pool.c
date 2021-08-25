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

/**
 * SECTION:as-pool
 * @short_description: Access the AppStream metadata pool.
 *
 * This class loads AppStream metadata from various sources and refines it with existing
 * knowledge about the system (e.g. by setting absolute paths for cached icons).
 * An #AsPool will use an on-disk cache to store metadata is has read and refined to
 * speed up the loading time when the same data is requested a second time.
 *
 * You can find AppStream metadata matching various user-defined criteria, and also add new
 * metadata to the pool.
 * The caching behavior can be controlled by the application using #AsCacheFlags.
 *
 * An AppStream cache object can also be created and read using the appstreamcli(1) utility.
 *
 * This class is threadsafe.
 *
 * See also: #AsComponent
 */

#include "config.h"
#include "as-pool.h"
#include "as-pool-private.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-component-private.h"
#include "as-distro-details.h"
#include "as-settings-private.h"
#include "as-distro-extras.h"
#include "as-stemmer.h"
#include "as-cache.h"
#include "as-profile.h"

#include "as-metadata.h"

typedef struct
{
	gchar *screenshot_service_url;
	gchar *locale;
	gchar *current_arch;
	AsProfile *profile;

	GPtrArray *xml_dirs;
	GPtrArray *yaml_dirs;
	GPtrArray *icon_dirs;

	AsCache *system_cache;
	AsCache *cache;
	gchar *cache_fname;
	gchar *sys_cache_dir_system;
	gchar *sys_cache_dir_user;

	gchar **term_greylist;

	AsPoolFlags flags;
	AsCacheFlags cache_flags;
	gboolean prefer_local_metainfo;

	GMutex mutex;
} AsPoolPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsPool, as_pool, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_pool_get_instance_private (o))

/**
 * AS_SYSTEM_COLLECTION_METADATA_PATHS:
 *
 * Locations where system-wide AppStream collection metadata may be stored.
 */
const gchar *AS_SYSTEM_COLLECTION_METADATA_PATHS[4] = { "/usr/share/app-info",
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

/* where .desktop files are installed to by packages to be registered with the system */
static gchar *APPLICATIONS_DIR = "/usr/share/applications";

/* where metainfo files can be found */
static gchar *METAINFO_DIR = "/usr/share/metainfo";

static void as_pool_add_metadata_location_internal (AsPool *pool, const gchar *directory, gboolean add_root);
static void as_pool_cache_refine_component_cb (gpointer data, gpointer user_data);
static void as_pool_cleanup_cache_dir (AsPool *pool, const gchar *cache_dir);

/**
 * as_pool_init:
 **/
static void
as_pool_init (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autofree gchar *cache_root = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autoptr(AsDistroDetails) distro = NULL;

	g_mutex_init (&priv->mutex);

	priv->profile = as_profile_new ();

	/* set active locale */
	priv->locale = as_get_current_locale ();

	priv->xml_dirs = g_ptr_array_new_with_free_func (g_free);
	priv->yaml_dirs = g_ptr_array_new_with_free_func (g_free);
	priv->icon_dirs = g_ptr_array_new_with_free_func (g_free);

	/* set the current architecture */
	priv->current_arch = as_get_current_arch ();

	/* set up our localized search-term greylist */
	priv->term_greylist = g_strsplit (AS_SEARCH_GREYLIST_STR, ";", -1);

	/* system-wide system data cache locations */
	priv->sys_cache_dir_system = g_strdup (AS_APPSTREAM_CACHE_PATH);

	/* per-user system data cache locations */
	cache_root = as_get_user_cache_dir ();
	priv->sys_cache_dir_user = g_build_filename (cache_root, "system", NULL);

	if (as_utils_is_root ()) {
		/* users umask shouldn't interfere with us creating new files when we are root */
		as_reset_umask ();

		/* ensure we never start gvfsd as root: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=852696 */
		g_setenv ("GIO_USE_VFS", "local", TRUE);
	}

	/* create caches */
	priv->system_cache = as_cache_new ();
	priv->cache = as_cache_new ();

	/* system cache is always read-only */
	as_cache_set_readonly (priv->system_cache, TRUE);

	/* set callback to refine components after deserialization */
	as_cache_set_refine_func (priv->cache, as_pool_cache_refine_component_cb, pool);
	as_cache_set_refine_func (priv->system_cache, as_pool_cache_refine_component_cb, pool);

	/* open our session cache in temporary mode by default */
	priv->cache_fname = g_strdup (":temporary");
	if (!as_cache_open (priv->cache, priv->cache_fname, priv->locale, &tmp_error)) {
		g_critical ("Unable to open temporary cache: %s", tmp_error->message);
		g_clear_pointer (&tmp_error, g_error_free);
	}

	distro = as_distro_details_new ();
	priv->screenshot_service_url = as_distro_details_get_str (distro, "ScreenshotUrl");

	/* check whether we might want to prefer local metainfo files over remote data */
	priv->prefer_local_metainfo = as_distro_details_get_bool (distro, "PreferLocalMetainfoData", FALSE);

	/* set watched default directories for AppStream metadata */
	for (guint i = 0; AS_SYSTEM_COLLECTION_METADATA_PATHS[i] != NULL; i++)
		as_pool_add_metadata_location_internal (pool, AS_SYSTEM_COLLECTION_METADATA_PATHS[i], FALSE);

	/* set default pool flags */
	priv->flags = AS_POOL_FLAG_READ_COLLECTION | AS_POOL_FLAG_READ_METAINFO;

	/* set default cache flags */
	priv->cache_flags = AS_CACHE_FLAG_USE_SYSTEM | AS_CACHE_FLAG_USE_USER | AS_CACHE_FLAG_REFRESH_SYSTEM;
}

/**
 * as_pool_finalize:
 **/
static void
as_pool_finalize (GObject *object)
{
	AsPool *pool = AS_POOL (object);
	AsPoolPrivate *priv = GET_PRIVATE (pool);

	g_mutex_lock (&priv->mutex);
	g_free (priv->screenshot_service_url);

	g_ptr_array_unref (priv->xml_dirs);
	g_ptr_array_unref (priv->yaml_dirs);
	g_ptr_array_unref (priv->icon_dirs);

	g_object_unref (priv->cache);
	g_object_unref (priv->system_cache);
	g_free (priv->cache_fname);
	g_free (priv->sys_cache_dir_user);
	g_free (priv->sys_cache_dir_system);

	g_free (priv->locale);
	g_free (priv->current_arch);
	g_strfreev (priv->term_greylist);

	g_object_unref (priv->profile);

	g_mutex_unlock (&priv->mutex);
	g_mutex_clear (&priv->mutex);

	G_OBJECT_CLASS (as_pool_parent_class)->finalize (object);
}

/**
 * as_pool_class_init:
 **/
static void
as_pool_class_init (AsPoolClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_pool_finalize;
}

/**
 * as_pool_can_query_system_cache:
 *
 * Check whether the system cache can be used for reading data.
 */
static inline gboolean
as_pool_can_query_system_cache (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	if (as_flags_contains (priv->cache_flags, AS_CACHE_FLAG_USE_SYSTEM))
		return as_cache_is_open (priv->system_cache);
	return FALSE;
}

/**
 * as_pool_get_component_by_data_id:
 */
static AsComponent*
as_pool_get_component_by_data_id (AsPool *pool, const gchar *cdid, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	GError *tmp_error = NULL;
	AsComponent *cpt;

	cpt = as_cache_get_component_by_data_id (priv->cache, cdid, &tmp_error);
	if (cpt != NULL)
		return cpt;
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	/* check system cache last */
	if (as_pool_can_query_system_cache (pool))
		return as_cache_get_component_by_data_id (priv->system_cache, cdid, &tmp_error);
	else
		return NULL;
}

/**
 * as_pool_remove_by_data_id:
 */
static gboolean
as_pool_remove_by_data_id (AsPool *pool, const gchar *cdid, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	GError *tmp_error = NULL;

	if (as_pool_can_query_system_cache (pool)) {
		as_cache_remove_by_data_id (priv->system_cache, cdid, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}
	}
	return as_cache_remove_by_data_id (priv->cache, cdid, error);
}

/**
 * as_pool_insert:
 */
static gboolean
as_pool_insert (AsPool *pool, AsComponent *cpt, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	GError *tmp_error = NULL;

	/* if we have a system cache, ensure the component is "removed" (masked) there,
	 * and re-added then to the current session cache */
	if (as_pool_can_query_system_cache (pool)) {
		as_cache_remove_by_data_id (priv->system_cache,
					    as_component_get_data_id (cpt),
					    &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}
	}
	return as_cache_insert (priv->cache, cpt, error);
}

/**
 * as_pool_has_component_id:
 */
static gboolean
as_pool_has_component_id (AsPool *pool, const gchar *cid, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	GError *tmp_error = NULL;
	gboolean ret;

	ret = as_cache_has_component_id (priv->cache, cid, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	if (ret)
		return ret;

	/* check system cache last */
	if (as_pool_can_query_system_cache (pool))
		return as_cache_has_component_id (priv->system_cache, cid, &tmp_error);
	else
		return FALSE;
}

/**
 * as_pool_add_component_internal:
 * @pool: An instance of #AsPool
 * @cpt: The #AsComponent to add to the pool.
 * @pedantic_noadd: If %TRUE, always emit an error if component couldn't be added.
 * @error: A #GError or %NULL
 *
 * Internal.
 */
static gboolean
as_pool_add_component_internal (AsPool *pool, AsComponent *cpt, gboolean pedantic_noadd, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	const gchar *cdid = NULL;
	g_autoptr(AsComponent) existing_cpt = NULL;
	gint pool_priority;
	AsOriginKind new_cpt_orig_kind;
	AsOriginKind existing_cpt_orig_kind;
	AsMergeKind new_cpt_merge_kind;
	GError *tmp_error = NULL;

	cdid = as_component_get_data_id (cpt);
	if (as_component_is_ignored (cpt)) {
		if (pedantic_noadd)
			g_set_error (error,
					AS_POOL_ERROR,
					AS_POOL_ERROR_FAILED,
					"Skipping '%s' from inclusion into the pool: Component is ignored.", cdid);
		return FALSE;
	}

	existing_cpt = as_pool_get_component_by_data_id (pool, cdid, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	if (as_component_get_origin_kind (cpt) == AS_ORIGIN_KIND_DESKTOP_ENTRY) {
		g_autofree gchar *tmp_cdid = NULL;

		/* .desktop entries might map to existing metadata data with or without .desktop suffix, we need to check for that.
		 * (the .desktop suffix is optional for desktop-application metainfo files, and the desktop-entry parser will automatically
		 * omit it if the desktop-entry-id is following the reverse DNS scheme)
		 */
		if (existing_cpt == NULL) {
			tmp_cdid = g_strdup_printf ("%s.desktop", cdid);

			existing_cpt = as_pool_get_component_by_data_id (pool, tmp_cdid, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return FALSE;
			}
		}

		if (existing_cpt != NULL) {
			if (as_component_get_origin_kind (existing_cpt) != AS_ORIGIN_KIND_DESKTOP_ENTRY) {
				/* discard this component if we have better data already in the pool,
				 * which is basically anything *but* data from a .desktop file */
				g_debug ("Ignored .desktop metadata for '%s': We already have better data.", cdid);
				return FALSE;
			}
		}
	}

	/* perform metadata merges if necessary */
	new_cpt_merge_kind = as_component_get_merge_kind (cpt);
	if (new_cpt_merge_kind != AS_MERGE_KIND_NONE) {
		g_autoptr(GPtrArray) matches = NULL;
		guint i;

		/* we merge the data into all components with matching IDs at time */
		matches = as_pool_get_components_by_id (pool,
							as_component_get_id (cpt));
		for (i = 0; i < matches->len; i++) {
			AsComponent *match = AS_COMPONENT (g_ptr_array_index (matches, i));
			if (new_cpt_merge_kind == AS_MERGE_KIND_REMOVE_COMPONENT) {
				/* remove matching component from pool if its priority is lower */
				if (as_component_get_priority (match) < as_component_get_priority (cpt)) {
					const gchar *match_cdid = as_component_get_data_id (match);
					as_pool_remove_by_data_id (pool, match_cdid, &tmp_error);
					if (tmp_error != NULL) {
						g_propagate_error (error, tmp_error);
						return FALSE;
					}
					g_debug ("Removed via merge component: %s", match_cdid);
				}
			} else {
				as_component_merge (match, cpt);
			}
		}

		return TRUE;
	}

	if (existing_cpt == NULL) {
		if (!as_pool_insert (pool, cpt, error))
			return FALSE;
		return TRUE;
	}

	/* safety check so we don't ignore a good component because we added a bad one first */
	if (!as_component_is_valid (existing_cpt)) {
		g_debug ("Replacing invalid component '%s' with new one.", cdid);
		if (!as_pool_insert (pool, cpt, error))
			return FALSE;
		return TRUE;
	}

	new_cpt_orig_kind = as_component_get_origin_kind (cpt);
	existing_cpt_orig_kind = as_component_get_origin_kind (existing_cpt);

	/* always replace data from .desktop entries */
	if (existing_cpt_orig_kind == AS_ORIGIN_KIND_DESKTOP_ENTRY) {
		if (new_cpt_orig_kind == AS_ORIGIN_KIND_METAINFO) {
			/* do an append-merge to ensure the data from an existing metainfo file has an icon */
			as_component_merge_with_mode (cpt,
							existing_cpt,
							AS_MERGE_KIND_APPEND);

			if (!as_pool_insert (pool, cpt, error))
				return FALSE;
			g_debug ("Replaced '%s' with data from metainfo and desktop-entry file.", cdid);
			return TRUE;
		} else {
			as_component_set_priority (existing_cpt, -G_MAXINT);
		}
	}

	/* merge desktop-entry data in, if we already have existing data from a metainfo file */
	if (new_cpt_orig_kind == AS_ORIGIN_KIND_DESKTOP_ENTRY) {
		if (existing_cpt_orig_kind == AS_ORIGIN_KIND_METAINFO) {
			/* do an append-merge to ensure the metainfo file has an icon */
			as_component_merge_with_mode (existing_cpt,
						      cpt,
						      AS_MERGE_KIND_APPEND);
			g_debug ("Merged desktop-entry data into metainfo data for '%s'.", cdid);
			return TRUE;
		}
		if (existing_cpt_orig_kind == AS_ORIGIN_KIND_COLLECTION) {
			g_debug ("Ignored desktop-entry component '%s': We already have better data.", cdid);
			return FALSE;
		}
	}

	/* check whether we should prefer data from metainfo files over preexisting data */
	if ((priv->prefer_local_metainfo) &&
	    (new_cpt_orig_kind == AS_ORIGIN_KIND_METAINFO)) {
		/* update package info, metainfo files do never have this data.
		 * (we hope that collection data was loaded first here, so the existing_cpt already contains
		 *  the information we want - if that's not the case, no harm is done here) */
		as_component_set_pkgnames (cpt, as_component_get_pkgnames (existing_cpt));
		as_component_set_bundles_array (cpt, as_component_get_bundles (existing_cpt));

		if (!as_pool_insert (pool, cpt, error))
			return FALSE;
		g_debug ("Replaced '%s' with data from metainfo file.", cdid);
		return TRUE;
	}

	/* if we are here, we might have duplicates and no merges, so check if we should replace a component
	 * with data of higher priority, or if we have an actual error in the metadata */
	pool_priority = as_component_get_priority (existing_cpt);
	if (pool_priority < as_component_get_priority (cpt)) {
		if (!as_pool_insert (pool, cpt, error))
			return FALSE;
		g_debug ("Replaced '%s' with data of higher priority.", cdid);
	} else {
		/* bundles are treated specially here */
		if ((!as_component_has_bundle (existing_cpt)) && (as_component_has_bundle (cpt))) {
			GPtrArray *bundles;
			/* propagate bundle information to existing component */
			bundles = as_component_get_bundles (cpt);
			as_component_set_bundles_array (existing_cpt, bundles);
			return TRUE;
		}

		/* experimental multiarch support */
		if (as_component_get_architecture (cpt) != NULL) {
			if (as_arch_compatible (as_component_get_architecture (cpt), priv->current_arch)) {
				const gchar *earch;
				/* this component is compatible with our current architecture */

				earch = as_component_get_architecture (existing_cpt);
				if (earch != NULL) {
					if (as_arch_compatible (earch, priv->current_arch)) {
						if (!as_pool_insert (pool, cpt, error))
							return FALSE;
						g_debug ("Preferred component for native architecture for %s (was %s)", cdid, earch);
						return TRUE;
					} else {
						g_debug ("Ignored additional entry for '%s' on architecture %s.", cdid, earch);
						return FALSE;
					}
				}
			}
		}

		if (pool_priority == as_component_get_priority (cpt)) {
			g_set_error (error,
					AS_POOL_ERROR,
					AS_POOL_ERROR_COLLISION,
					"Detected colliding IDs: %s was already added with the same priority.", cdid);
			return FALSE;
		} else {
			if (pedantic_noadd)
				g_set_error (error,
						AS_POOL_ERROR,
						AS_POOL_ERROR_COLLISION,
						"Detected colliding IDs: %s was already added with a higher priority.", cdid);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * as_pool_add_component:
 * @pool: An instance of #AsPool
 * @cpt: The #AsComponent to add to the pool.
 * @error: A #GError or %NULL
 *
 * Register a new component in the AppStream metadata pool.
 *
 * Returns: %TRUE if the new component was successfully added to the pool.
 */
gboolean
as_pool_add_component (AsPool *pool, AsComponent *cpt, GError **error)
{
	return as_pool_add_component_internal (pool, cpt, TRUE, error);
}

/**
 * as_pool_clear2:
 * @pool: An #AsPool.
 *
 * Remove all metadata from the pool and clear caches.
 */
gboolean
as_pool_clear2 (AsPool *pool, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	/* close system cache so it won't be used anymore
	 * (will be loaded explicitly again later, when needed) */
	as_cache_close (priv->system_cache);

	/* if we were just created, we may be able to reuse the current temporary cache,
	 * instead of creating a new one (which is a bit wasteful).
	 * Reuse requires a temporary cache with no elements. */
	if ((g_strcmp0 (priv->cache_fname, ":temporary") == 0) &&
	    (as_cache_count_components (priv->cache, NULL) == 0)) {
		/* we can reuse the user cache */

		g_debug ("Not clearing user cache: The cache was already empty.");
		return TRUE;
	} else {
		/* it looks like we can not reuse the old cache, so now we need to clear
		 * the cache for real by deleting the old one and creating a new one */

		g_debug ("Clearing user cache.");
		as_cache_close (priv->cache);
		if (g_file_test (priv->cache_fname, G_FILE_TEST_EXISTS)) {
			if (g_remove (priv->cache_fname) != 0) {
				g_set_error_literal (error,
							AS_POOL_ERROR,
							AS_POOL_ERROR_OLD_CACHE,
							_("Unable to remove old cache."));
				return FALSE;
			}
		}

		/* reopen the session cache as a new, pristine one */
		return as_cache_open (priv->cache, priv->cache_fname, priv->locale, error);
	}
}

/**
 * as_pool_clear:
 * @pool: An #AsPool.
 *
 * Remove all metadat from the pool.
 */
void
as_pool_clear (AsPool *pool)
{
	GError *tmp_error = NULL;

	/* reopen the new cache */
	if (!as_pool_clear2 (pool, &tmp_error)) {
		g_critical ("Unable to reopen cache: %s", tmp_error->message);
		g_error_free (tmp_error);
		tmp_error = NULL;
	}
}

/**
 * as_pool_ctime_newer:
 *
 * Returns: %TRUE if ctime of file is newer than the cached time.
 */
static gboolean
as_pool_ctime_newer (AsPool *pool, const gchar *dir, AsCache *cache)
{
	struct stat sb;
	time_t cache_ctime;

	if (stat (dir, &sb) < 0)
		return FALSE;

	cache_ctime = as_cache_get_ctime (cache);
	if (sb.st_ctime > cache_ctime)
		return TRUE;

	return FALSE;
}

/**
 * as_path_is_system_metadata_location:
 */
static gboolean
as_path_is_system_metadata_location (const gchar *dir)
{
	/* we can't just do a "/home/" prefix check here, as e.g. Flatpak data may also be
	 * in system directories, and not every instance of an AppStream-using app will have
	 * these included, which would mess up cross-app cache sharing.
	 * In addition, some cliants may have multiple AsPool instance, further complicating
	 * this issue. */
	for (gint i = 0; AS_SYSTEM_COLLECTION_METADATA_PATHS[i] != NULL; i++)
		if (g_str_has_prefix (dir, AS_SYSTEM_COLLECTION_METADATA_PATHS[i]))
			return TRUE;
	return FALSE;
}

/**
 * as_pool_has_system_metadata_paths:
 */
static gboolean
as_pool_has_system_metadata_paths (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	guint i;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	for (i = 0; i < priv->xml_dirs->len; i++) {
		const gchar *dir = (const gchar*) g_ptr_array_index (priv->xml_dirs, i);
		if (as_path_is_system_metadata_location (dir))
			return TRUE;
	}
	for (i = 0; i < priv->yaml_dirs->len; i++) {
		const gchar *dir = (const gchar*) g_ptr_array_index (priv->yaml_dirs, i);
		if (as_path_is_system_metadata_location (dir))
			return TRUE;
	}

	return FALSE;
}

/**
 * as_pool_appstream_data_changed:
 */
static gboolean
as_pool_metadata_changed (AsPool *pool, AsCache *cache, gboolean system_only)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	/* if the cache does not exist, we always need to recreate it */
	if (!g_file_test (as_cache_get_location (cache), G_FILE_TEST_EXISTS))
		return TRUE;

	/* compare file times */
	for (guint i = 0; i < priv->xml_dirs->len; i++) {
		const gchar *dir = (const gchar*) g_ptr_array_index (priv->xml_dirs, i);
		if (system_only && !as_path_is_system_metadata_location (dir))
			continue;
		if (as_pool_ctime_newer (pool, dir, cache))
			return TRUE;
	}
	for (guint i = 0; i < priv->yaml_dirs->len; i++) {
		const gchar *dir = (const gchar*) g_ptr_array_index (priv->yaml_dirs, i);
		if (system_only && !as_path_is_system_metadata_location (dir))
			continue;
		if (as_pool_ctime_newer (pool, dir, cache))
			return TRUE;
	}

	return FALSE;
}

/**
 * as_pool_load_collection_data:
 *
 * Load fresh metadata from AppStream collection data directories.
 */
static gboolean
as_pool_load_collection_data (AsPool *pool, gboolean refresh, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	GPtrArray *cpts;
	g_autoptr(GPtrArray) merge_cpts = NULL;
	gboolean ret;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GPtrArray) mdata_files = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autoptr(GMutexLocker) locker = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;
	gboolean system_cache_used = FALSE;

	/* see if we can use the system caches */
	if (!refresh && as_pool_has_system_metadata_paths (pool)) {
		g_autofree gchar *cache_fname = NULL;
		gboolean use_user_cache = FALSE;
		gboolean cache_stale = TRUE;

		g_mutex_lock (&priv->mutex);
		cache_fname = g_strdup_printf ("%s/%s.cache", priv->sys_cache_dir_system, priv->locale);
		as_cache_set_location (priv->system_cache, cache_fname);
		g_mutex_unlock (&priv->mutex);
		if (!as_pool_metadata_changed (pool, priv->system_cache, TRUE)) {
			g_debug ("Shared metadata cache of system data seems up to date.");
			cache_stale = FALSE;
		} else {
			g_mutex_lock (&priv->mutex);
			g_free (cache_fname);
			cache_fname = g_strdup_printf ("%s/%s.cache", priv->sys_cache_dir_user, priv->locale);
			as_cache_set_location (priv->system_cache, cache_fname);
			g_mutex_unlock (&priv->mutex);

			use_user_cache = TRUE;
			cache_stale = as_pool_metadata_changed (pool, priv->system_cache, TRUE);
			if (cache_stale)
				g_debug ("User metadata cache of system data is stale, may try to recreate it.");
			else
				g_debug ("User metadata cache of system data seems up to date.");
		}

		g_mutex_lock (&priv->mutex);
		if (as_flags_contains (priv->cache_flags, AS_CACHE_FLAG_USE_SYSTEM)) {
			g_mutex_unlock (&priv->mutex);

			as_cache_close (priv->system_cache);
			if (cache_stale) {
				if (as_flags_contains (priv->cache_flags, AS_CACHE_FLAG_REFRESH_SYSTEM)) {
					g_autoptr(AsPool) refresh_pool = as_pool_new ();
					g_debug ("System-wide metadata cache is stale, will refresh it now.");
					ret = as_pool_refresh_system_cache (refresh_pool,
									    TRUE, /* user */
									    FALSE, /* force */
									    &tmp_error);
					if (tmp_error != NULL) {
						if (ret)
							g_warning ("System cache issue: %s", tmp_error->message);
						else
							g_warning ("Unable to refresh system cache: %s", tmp_error->message);
						g_clear_pointer (&tmp_error, g_error_free);
					}
					if (ret) {
						/* the cache should exist now, ready to be loaded */
						g_mutex_lock (&priv->mutex);
						if (as_cache_open2 (priv->system_cache, priv->locale, &tmp_error)) {
							system_cache_used = TRUE;
						} else {
							g_warning ("Unable to load newly generated system cache: %s", tmp_error->message);
							g_clear_pointer (&tmp_error, g_error_free);
							system_cache_used = FALSE;
						}
						g_mutex_unlock (&priv->mutex);
					}
				} else {
					g_debug ("System-wide metadata cache is stale, but refresh was prohibited.");
					system_cache_used = FALSE;
				}
			} else {
				g_debug ("Using system cache data %s.", use_user_cache? "from user cache" : "from shared cache");

				g_mutex_lock (&priv->mutex);
				if (as_cache_open2 (priv->system_cache, priv->locale, &tmp_error)) {
					system_cache_used = TRUE;
				} else {
					/* if we can't open the system cache for whatever reason, we complain but
					 * silently fall back to reading all data again */
					g_warning ("Unable to load system cache: %s", tmp_error->message);
					g_clear_pointer (&tmp_error, g_error_free);
					system_cache_used = FALSE;
				}
				g_mutex_unlock (&priv->mutex);

				/* try to clean up old caches for the user, in case the system cache is up to date
				 * and we are using it instead */
				if (!use_user_cache)
					as_pool_cleanup_cache_dir (pool, priv->sys_cache_dir_user);
			}

		} else {
			g_mutex_unlock (&priv->mutex);
			g_debug ("Not using system cache.");
			as_cache_close (priv->system_cache);
			system_cache_used = FALSE;
		}

	} else {
		if (!refresh)
			g_debug ("No system collection metadata paths selected, can not use system cache.");
	}

	ptask = as_profile_start_literal (priv->profile, "AsPool:load_collection_data");

	/* prepare metadata parser */
	metad = as_metadata_new ();
	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_COLLECTION);
	g_mutex_lock (&priv->mutex);
	as_metadata_set_locale (metad, priv->locale);
	g_mutex_unlock (&priv->mutex);

	/* find AppStream metadata */
	ret = TRUE;
	mdata_files = g_ptr_array_new_with_free_func (g_free);

	/* protect access to directory lists */
	locker = g_mutex_locker_new (&priv->mutex);

	/* find XML data */
	for (guint i = 0; i < priv->xml_dirs->len; i++) {
		const gchar *xml_path = (const gchar *) g_ptr_array_index (priv->xml_dirs, i);
		if (system_cache_used && as_path_is_system_metadata_location (xml_path)) {
			g_debug ("Skipped XML path '%s' for session cache: Already considered for system cache.", xml_path);
			continue;
		}

		if (g_file_test (xml_path, G_FILE_TEST_IS_DIR)) {
			g_autoptr(GPtrArray) xmls = NULL;

			g_debug ("Searching for data in: %s", xml_path);
			xmls = as_utils_find_files_matching (xml_path, "*.xml*", FALSE, NULL);
			if (xmls != NULL) {
				for (guint j = 0; j < xmls->len; j++) {
					const gchar *val;
					val = (const gchar *) g_ptr_array_index (xmls, j);
					g_ptr_array_add (mdata_files,
								g_strdup (val));
				}
			}
		}
	}

	/* find YAML metadata */
	for (guint i = 0; i < priv->yaml_dirs->len; i++) {
		const gchar *yaml_path = (const gchar *) g_ptr_array_index (priv->yaml_dirs, i);
		if (system_cache_used && as_path_is_system_metadata_location (yaml_path)) {
			g_debug ("Skipped YAML path '%s' for session cache: Already considered for system cache.", yaml_path);
			continue;
		}

		if (g_file_test (yaml_path, G_FILE_TEST_IS_DIR)) {
			g_autoptr(GPtrArray) yamls = NULL;

			g_debug ("Searching for data in: %s", yaml_path);
			yamls = as_utils_find_files_matching (yaml_path, "*.yml*", FALSE, NULL);
			if (yamls != NULL) {
				for (guint j = 0; j < yamls->len; j++) {
					const gchar *val;
					val = (const gchar *) g_ptr_array_index (yamls, j);
					g_ptr_array_add (mdata_files,
								g_strdup (val));
				}
			}
		}
	}

	/* no need for further locking after this point, AsCache is threadsafe */
	g_clear_pointer (&locker, g_mutex_locker_free);

	/* parse the found data */
	for (guint i = 0; i < mdata_files->len; i++) {
		g_autoptr(GFile) infile = NULL;
		const gchar *fname;

		fname = (const gchar*) g_ptr_array_index (mdata_files, i);
		g_debug ("Reading: %s", fname);

		infile = g_file_new_for_path (fname);
		if (!g_file_query_exists (infile, NULL)) {
			g_warning ("Metadata file '%s' does not exist.", fname);
			continue;
		}

		as_metadata_parse_file (metad,
					infile,
					AS_FORMAT_KIND_UNKNOWN,
					&tmp_error);
		if (tmp_error != NULL) {
			g_debug ("WARNING: %s", tmp_error->message);
			g_clear_pointer (&tmp_error, g_error_free);
			ret = FALSE;

			if (error != NULL) {
				if (*error == NULL)
					g_set_error_literal (error,
							     AS_POOL_ERROR,
							     AS_POOL_ERROR_FAILED,
							     fname);
				else
					g_prefix_error (error, "%s, ", fname);
			}
		}
	}

	/* finalize error message, if we had errors */
	if ((error != NULL) && (*error != NULL))
		g_prefix_error (error, "%s ", _("Metadata files have errors:"));

	/* add found components to the metadata pool */
	cpts = as_metadata_get_components (metad);
	merge_cpts = g_ptr_array_new ();
	for (guint i = 0; i < cpts->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));

		/* TODO: We support only system components at time */
		as_component_set_scope (cpt, AS_COMPONENT_SCOPE_SYSTEM);

		/* deal with merge-components later */
		if (as_component_get_merge_kind (cpt) != AS_MERGE_KIND_NONE) {
			g_ptr_array_add (merge_cpts, cpt);
			continue;
		}

		as_pool_add_component_internal (pool, cpt, TRUE, &tmp_error);
		if (tmp_error != NULL) {
			g_debug ("Metadata ignored: %s", tmp_error->message);
			g_clear_pointer (&tmp_error, g_error_free);
		}
	}

	/* we need to merge the merge-components into the pool last, so the merge process can fetch
	 * all components with matching IDs from the pool */
	for (guint i = 0; i < merge_cpts->len; i++) {
		AsComponent *mcpt = AS_COMPONENT (g_ptr_array_index (merge_cpts, i));

		as_pool_add_component_internal (pool, mcpt, TRUE, &tmp_error);
		if (tmp_error != NULL) {
			g_debug ("Merge component ignored: %s", tmp_error->message);
			g_clear_pointer (&tmp_error, g_error_free);
		}
	}

	return ret;
}

/**
 * as_pool_get_desktop_entries_table:
 *
 * Load fresh metadata from .desktop files.
 *
 * Returns: (transfer full): a hash map of #AsComponent instances.
 */
static GHashTable*
as_pool_get_desktop_entries_table (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GPtrArray) de_files = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;
	GHashTable *de_cpt_table = NULL;
	GError *error = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:get_desktop_entries_table");

	/* prepare metadata parser */
	metad = as_metadata_new ();
	g_mutex_lock (&priv->mutex);
	as_metadata_set_locale (metad, priv->locale);
	g_mutex_unlock (&priv->mutex);

	de_cpt_table = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      g_free,
					      (GDestroyNotify) g_object_unref);

	/* find .desktop files */
	g_debug ("Searching for data in: %s", APPLICATIONS_DIR);
	de_files = as_utils_find_files_matching (APPLICATIONS_DIR, "*.desktop", FALSE, NULL);
	if (de_files == NULL) {
		g_debug ("Unable find .desktop files.");
		return de_cpt_table;
	}

	/* parse the found data */
	for (guint i = 0; i < de_files->len; i++) {
		g_autoptr(GFile) infile = NULL;
		AsComponent *cpt;
		const gchar *fname = (const gchar*) g_ptr_array_index (de_files, i);

		g_debug ("Reading: %s", fname);
		infile = g_file_new_for_path (fname);
		if (!g_file_query_exists (infile, NULL)) {
			g_warning ("Metadata file '%s' does not exist.", fname);
			continue;
		}

		as_metadata_clear_components (metad);
		as_metadata_parse_file (metad,
					infile,
					AS_FORMAT_KIND_DESKTOP_ENTRY,
					&error);
		if (error != NULL) {
			g_debug ("Error reading .desktop file '%s': %s", fname, error->message);
			g_error_free (error);
			error = NULL;
			continue;
		}

		cpt = as_metadata_get_component (metad);
		if (cpt != NULL) {
			/* we only read metainfo files from system directories */
			as_component_set_scope (cpt, AS_COMPONENT_SCOPE_SYSTEM);

			g_hash_table_insert (de_cpt_table,
					     g_path_get_basename (fname),
					     g_object_ref (cpt));
		}
	}

	return de_cpt_table;
}

/**
 * as_pool_load_metainfo_data:
 *
 * Load fresh metadata from metainfo files.
 */
static void
as_pool_load_metainfo_data (AsPool *pool, GHashTable *desktop_entry_cpts)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GPtrArray) mi_files = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;
	GError *error = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:load_metainfo_data");

	/* prepare metadata parser */
	metad = as_metadata_new ();
	g_mutex_lock (&priv->mutex);
	as_metadata_set_locale (metad, priv->locale);
	g_mutex_unlock (&priv->mutex);

	/* find metainfo files */
	g_debug ("Searching for data in: %s", METAINFO_DIR);
	mi_files = as_utils_find_files_matching (METAINFO_DIR, "*.xml", FALSE, NULL);
	if (mi_files == NULL) {
		g_debug ("Unable find metainfo files.");
		return;
	}

	/* parse the found data */
	for (guint i = 0; i < mi_files->len; i++) {
		AsComponent *cpt;
		AsLaunchable *launchable;
		g_autoptr(GFile) infile = NULL;
		g_autofree gchar *desktop_id = NULL;
		const gchar *fname = (const gchar*) g_ptr_array_index (mi_files, i);

		if (!priv->prefer_local_metainfo) {
			g_autofree gchar *mi_cid = NULL;

			mi_cid = g_path_get_basename (fname);
			if (g_str_has_suffix (mi_cid, ".metainfo.xml"))
				mi_cid[strlen (mi_cid) - 13] = '\0';
			if (g_str_has_suffix (mi_cid, ".appdata.xml")) {
				g_autofree gchar *mi_cid_desktop = NULL;
				mi_cid[strlen (mi_cid) - 12] = '\0';

				mi_cid_desktop = g_strdup_printf ("%s.desktop", mi_cid);
				/* check with .desktop suffix too */
				if (as_pool_has_component_id (pool, mi_cid_desktop, NULL)) {
					g_debug ("Skipped: %s (already known)", fname);
					continue;
				}
			}

			/* quickly check if we know the component already */
			if (as_pool_has_component_id (pool, mi_cid, NULL)) {
				g_debug ("Skipped: %s (already known)", fname);
				continue;
			}
		}

		g_debug ("Reading: %s", fname);
		infile = g_file_new_for_path (fname);
		if (!g_file_query_exists (infile, NULL)) {
			g_warning ("Metadata file '%s' does not exist.", fname);
			continue;
		}

		as_metadata_clear_components (metad);
		as_metadata_parse_file (metad,
					infile,
					AS_FORMAT_KIND_UNKNOWN,
					&error);
		if (error != NULL) {
			g_debug ("Errors in '%s': %s", fname, error->message);
			g_error_free (error);
			error = NULL;
		}

		cpt = as_metadata_get_component (metad);
		if (cpt == NULL)
			continue;

		/* we only read metainfo files from system directories */
		as_component_set_scope (cpt, AS_COMPONENT_SCOPE_SYSTEM);

		launchable = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
		if ((launchable != NULL) && (as_launchable_get_entries (launchable)->len > 0)) {
			/* find matching .desktop component to merge with via launchable */
			desktop_id = g_strdup (g_ptr_array_index (as_launchable_get_entries (launchable), 0));
		} else {
			/* try to guess the matching .desktop ID from the component-id */
			if (g_str_has_suffix (as_component_get_id (cpt), ".desktop"))
				desktop_id = g_strdup (as_component_get_id (cpt));
			else
				desktop_id = g_strdup_printf ("%s.desktop", as_component_get_id (cpt));
		}

		/* merge .desktop data into component if possible */
		if (desktop_id != NULL) {
			AsComponent *de_cpt;
			de_cpt = g_hash_table_lookup (desktop_entry_cpts, desktop_id);
			if (de_cpt != NULL) {
				as_component_merge_with_mode (cpt,
								de_cpt,
								AS_MERGE_KIND_APPEND);
				g_hash_table_remove (desktop_entry_cpts, desktop_id);
			}
		}

		as_pool_add_component_internal (pool, cpt, FALSE, &error);
		if (error != NULL) {
			g_debug ("Component '%s' ignored: %s", as_component_get_data_id (cpt), error->message);
			g_error_free (error);
			error = NULL;
		}
	}
}

/**
 * as_pool_load_metainfo_desktop_data:
 *
 * Load metadata from metainfo files and .desktop files that
 * where made available by locally installed applications.
 */
static void
as_pool_load_metainfo_desktop_data (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GHashTable) de_cpts = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:load_metainfo_desktop_data");

	/* check if we actually need to load anything */
	g_mutex_lock (&priv->mutex);
	if (!as_flags_contains (priv->flags, AS_POOL_FLAG_READ_DESKTOP_FILES) && !as_flags_contains (priv->flags, AS_POOL_FLAG_READ_METAINFO)) {
		g_mutex_unlock (&priv->mutex);
		return;
	}
	g_mutex_unlock (&priv->mutex);

	/* get a hashmap of desktop-entry components */
	de_cpts = as_pool_get_desktop_entries_table (pool);

	g_mutex_lock (&priv->mutex);
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_READ_METAINFO)) {
		g_mutex_unlock (&priv->mutex);

		/* load metainfo components, absorb desktop-entry components into them */
		as_pool_load_metainfo_data (pool, de_cpts);
	} else {
		g_mutex_unlock (&priv->mutex);
	}

	/* read all remaining .desktop file components, if needed */
	g_mutex_lock (&priv->mutex);
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_READ_DESKTOP_FILES)) {
		GHashTableIter iter;
		gpointer value;
		GError *error = NULL;

		g_mutex_unlock (&priv->mutex);
		g_debug ("Including components from .desktop files in the pool.");
		g_hash_table_iter_init (&iter, de_cpts);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			AsComponent *cpt = AS_COMPONENT (value);

			as_pool_add_component_internal (pool, cpt, FALSE, &error);
			if (error != NULL) {
				g_debug ("Component '%s' ignored: %s", as_component_get_data_id (cpt), error->message);
				g_error_free (error);
				error = NULL;
			}
		}
	} else {
		g_mutex_unlock (&priv->mutex);
	}
}

/**
 * as_pool_cache_refine_component_cb:
 *
 * Callback function run on components before they are
 * (de)serialized.
 */
static void
as_pool_cache_refine_component_cb (gpointer data, gpointer user_data)
{
	AsPool *pool = AS_POOL (user_data);
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	AsComponent *cpt = AS_COMPONENT (data);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	/* add additional data to the component, e.g. external screenshots. Also refines
	 * the component's icon paths */
	as_component_complete (cpt,
				priv->screenshot_service_url,
				priv->icon_dirs);
}

/**
 * as_pool_load:
 * @pool: An instance of #AsPool.
 * @cancellable: a #GCancellable.
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
as_pool_load (AsPool *pool, GCancellable *cancellable, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsProfileTask) ptask = NULL;
	gboolean ret = TRUE;
	guint invalid_cpts_n;
	gssize all_cpts_n;
	gdouble valid_percentage;
	GError *tmp_error = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:load");

	g_mutex_lock (&priv->mutex);
	if (as_flags_contains (priv->cache_flags, AS_CACHE_FLAG_NO_CLEAR)) {
		g_autoptr(GMutexLocker) inner_locker = NULL;
		g_mutex_unlock (&priv->mutex);
		inner_locker = g_mutex_locker_new (&priv->mutex);

		/* we are supposed not to clear the cache before laoding its data */
		if (!as_cache_open (priv->cache, priv->cache_fname, priv->locale, error))
			return FALSE;
	} else {
		g_mutex_unlock (&priv->mutex);

		/* load (here) means to reload, so we clear potential old data */
		if (!as_pool_clear2 (pool, error))
			return FALSE;
	}

	as_cache_make_floating (priv->cache);

	/* read all AppStream metadata that we can find */
	g_mutex_lock (&priv->mutex);
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_READ_COLLECTION)) {
		g_mutex_unlock (&priv->mutex);
		ret = as_pool_load_collection_data (pool, FALSE, error);
	} else {
		g_mutex_unlock (&priv->mutex);
	}

	/* read all metainfo and desktop files and add them to the pool */
	as_pool_load_metainfo_desktop_data (pool);

	/* automatically refine the metadata we have in the pool */
	invalid_cpts_n = as_cache_unfloat (priv->cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	all_cpts_n = as_cache_count_components (priv->cache, &tmp_error);
	if (all_cpts_n < 0) {
		if (tmp_error != NULL) {
			g_warning ("Unable to retrieve component count from cache: %s", tmp_error->message);
			g_error_free (tmp_error);
			tmp_error = NULL;
		}
		all_cpts_n = 0;
	}

	valid_percentage = (all_cpts_n == 0)? 100 : (100 / (gdouble) all_cpts_n) * (gdouble) (all_cpts_n - invalid_cpts_n);
	g_debug ("Percentage of valid components: %0.3f", valid_percentage);

	/* we only return a non-TRUE value if a significant amount (10%) of components has been declared invalid. */
	if ((invalid_cpts_n != 0) && (valid_percentage <= 90))
		ret = FALSE;

	/* report errors if refining has failed */
	if (!ret && (error != NULL)) {
		if (*error == NULL) {
			g_set_error_literal (error,
					     AS_POOL_ERROR,
					     AS_POOL_ERROR_INCOMPLETE,
					     _("Many components have been recognized as invalid. See debug output for details."));
		} else {
			g_prefix_error (error, "Some components have been ignored: ");
		}
	}

	return ret;
}

static void
_pool_load_thread (GTask *task,
		   gpointer source_object,
		   gpointer task_data,
		   GCancellable *cancellable)
{
	AsPool *pool = AS_POOL (source_object);
	GError *error = NULL;
	gboolean success;

	success = as_pool_load (pool, cancellable, &error);
	if (error != NULL)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, success);
}

/**
 * as_pool_load_async:
 * @pool: An instance of #AsPool.
 * @cancellable: a #GCancellable.
 * @callback: A #GAsyncReadyCallback
 * @user_data: Data to pass to @callback
 *
 * Asynchronously loads data from all registered locations.
 * Equivalent to as_pool_load() (but asynchronous)
 *
 * Since: 0.12.10
 **/
void
as_pool_load_async (AsPool *pool,
		    GCancellable *cancellable,
		    GAsyncReadyCallback callback,
		    gpointer user_data)
{
	GTask *task = g_task_new (pool, cancellable, callback, user_data);
	g_task_run_in_thread (task, _pool_load_thread);
	g_object_unref (task);
}

/**
 * as_pool_load_finish:
 * @pool: An instance of #AsPool.
 * @result: A #GAsyncResult
 * @error: A #GError or %NULL.
 *
 * Retrieve the result of as_pool_load_async().
 *
 * Returns: %TRUE for success
 *
 * Since: 0.12.10
 **/
gboolean
as_pool_load_finish (AsPool *pool,
		     GAsyncResult *result,
		     GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, pool), FALSE);
	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * as_pool_load_cache_file:
 * @pool: An instance of #AsPool.
 * @fname: Filename of the cache file to load into the pool.
 * @error: A #GError or %NULL.
 *
 * Load AppStream metadata from a cache file.
 */
gboolean
as_pool_load_cache_file (AsPool *pool, const gchar *fname, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	as_cache_close (priv->system_cache);
	if (!as_cache_open (priv->cache, fname, priv->locale, error))
		return FALSE;

	return TRUE;
}

/**
 * as_pool_save_cache_file:
 * @pool: An instance of #AsPool.
 * @fname: Filename of the cache file the pool contents should be dumped to.
 * @error: A #GError or %NULL.
 *
 * Serialize AppStream metadata to a cache file.
 */
gboolean
as_pool_save_cache_file (AsPool *pool, const gchar *fname, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GPtrArray) cpts = NULL;
	g_autoptr(AsCache) cache = NULL;
	g_autoptr(GMutexLocker) locker = NULL;

	cpts = as_pool_get_components (pool);
	cache = as_cache_new ();
	locker = g_mutex_locker_new (&priv->mutex);
	if (!as_cache_open (cache, fname, priv->locale, error))
		return FALSE;
	g_clear_pointer (&locker, g_mutex_locker_free);

	for (guint i = 0; i < cpts->len; i++) {
		if (!as_cache_insert (cache, AS_COMPONENT (g_ptr_array_index (cpts, i)), error))
			return FALSE;
	}

	return TRUE;
}

/**
 * as_pool_get_components:
 * @pool: An instance of #AsPool.
 *
 * Get a list of found components.
 *
 * Returns: (transfer container) (element-type AsComponent): an array of #AsComponent instances.
 */
GPtrArray*
as_pool_get_components (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsProfileTask) ptask = NULL;
	g_autoptr(GError) tmp_error = NULL;
	GPtrArray *result = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:get_components");

	result = as_cache_get_components_all (priv->cache, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable to retrieve all components from session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

	if (as_pool_can_query_system_cache (pool)) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		tmp_res = as_cache_get_components_all (priv->system_cache, &tmp_error);
		if (tmp_res == NULL) {
			g_warning ("Unable to retrieve all components from system cache: %s", tmp_error->message);
			return result;
		}
		as_object_ptr_array_absorb (result, tmp_res);
	}

	return result;
}

/**
 * as_pool_get_components_by_id:
 * @pool: An instance of #AsPool.
 * @cid: The AppStream-ID to look for.
 *
 * Get a specific component by its ID.
 * This function may contain multiple results if we have
 * data describing this component from multiple scopes/origin types.
 *
 * Returns: (transfer container) (element-type AsComponent): An #AsComponent
 */
GPtrArray*
as_pool_get_components_by_id (AsPool *pool, const gchar *cid)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsProfileTask) ptask = NULL;
	g_autoptr(GError) tmp_error = NULL;
	GPtrArray *result = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:get_components_by_id");

	result = as_cache_get_components_by_id (priv->cache, cid, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find components by ID in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

	if (as_pool_can_query_system_cache (pool)) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		tmp_res = as_cache_get_components_by_id (priv->system_cache, cid, &tmp_error);
		if (tmp_res == NULL) {
			g_warning ("Unable find components by ID in system cache: %s", tmp_error->message);
			return result;
		}
		as_object_ptr_array_absorb (result, tmp_res);
	}

	return result;
}

/**
 * as_pool_get_components_by_provided_item:
 * @pool: An instance of #AsPool.
 * @kind: An #AsProvidesKind
 * @item: The value of the provided item.
 *
 * Find components in the AppStream data pool which provide a certain item.
 *
 * Returns: (transfer container) (element-type AsComponent): an array of #AsComponent objects which have been found.
 */
GPtrArray*
as_pool_get_components_by_provided_item (AsPool *pool,
					      AsProvidedKind kind,
					      const gchar *item)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GError) tmp_error = NULL;
	GPtrArray *result = NULL;

	result = as_cache_get_components_by_provided_item (priv->cache, kind, item, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find components by provided item in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

	if (as_pool_can_query_system_cache (pool)) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		tmp_res = as_cache_get_components_by_provided_item (priv->system_cache, kind, item, &tmp_error);
		if (tmp_res == NULL) {
			g_warning ("Unable find components by provided item in system cache: %s", tmp_error->message);
			return result;
		}
		as_object_ptr_array_absorb (result, tmp_res);
	}

	return result;
}

/**
 * as_pool_get_components_by_kind:
 * @pool: An instance of #AsDatabase.
 * @kind: An #AsComponentKind.
 *
 * Return a list of all components in the pool which are of a certain kind.
 *
 * Returns: (transfer container) (element-type AsComponent): an array of #AsComponent objects which have been found.
 */
GPtrArray*
as_pool_get_components_by_kind (AsPool *pool, AsComponentKind kind)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GError) tmp_error = NULL;
	GPtrArray *result = NULL;

	result = as_cache_get_components_by_kind (priv->cache, kind, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find components by kind in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

	if (as_pool_can_query_system_cache (pool)) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		tmp_res = as_cache_get_components_by_kind (priv->system_cache, kind, &tmp_error);
		if (tmp_res == NULL) {
			g_warning ("Unable find components by kind in system cache: %s", tmp_error->message);
			return result;
		}
		as_object_ptr_array_absorb (result, tmp_res);
	}

	return result;
}

/**
 * as_pool_get_components_by_categories:
 * @pool: An instance of #AsDatabase.
 * @categories: An array of XDG categories to include.
 *
 * Return a list of components which are in one of the categories.
 *
 * Returns: (transfer container) (element-type AsComponent): an array of #AsComponent objects which have been found.
 */
GPtrArray*
as_pool_get_components_by_categories (AsPool *pool, gchar **categories)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GError) tmp_error = NULL;
	GPtrArray *result = NULL;

	/* sanity check */
	for (guint i = 0; categories[i] != NULL; i++) {
		if (!as_utils_is_category_name (categories[i])) {
			g_warning ("'%s' is not a valid XDG category name, search results might be invalid or empty.", categories[i]);
		}
	}

	result = as_cache_get_components_by_categories (priv->cache, categories, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find components by categories in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

	if (as_pool_can_query_system_cache (pool)) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		tmp_res = as_cache_get_components_by_categories (priv->system_cache, categories, &tmp_error);
		if (tmp_res == NULL) {
			g_warning ("Unable find components by categories in system cache: %s", tmp_error->message);
			return result;
		}
		as_object_ptr_array_absorb (result, tmp_res);
	}

	return result;
}

/**
 * as_pool_get_components_by_launchable:
 * @pool: An instance of #AsPool.
 * @kind: An #AsLaunchableKind
 * @id: The ID of the launchable.
 *
 * Find components in the AppStream data pool which provide a specific launchable.
 * See #AsLaunchable for details on launchables, or refer to the AppStream specification.
 *
 * Returns: (transfer container) (element-type AsComponent): an array of #AsComponent objects which have been found.
 *
 * Since: 0.11.4
 */
GPtrArray*
as_pool_get_components_by_launchable (AsPool *pool,
					AsLaunchableKind kind,
					const gchar *id)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GError) tmp_error = NULL;
	GPtrArray *result = NULL;

	result = as_cache_get_components_by_launchable (priv->cache, kind, id, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find components by launchable in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

	if (as_pool_can_query_system_cache (pool)) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		tmp_res = as_cache_get_components_by_launchable (priv->system_cache, kind, id, &tmp_error);
		if (tmp_res == NULL) {
			g_warning ("Unable find components by launchable in system cache: %s", tmp_error->message);
			return result;
		}
		as_object_ptr_array_absorb (result, tmp_res);
	}

	return result;
}

/**
 * as_user_search_term_valid:
 *
 * Test for search term validity (filter out any markup).
 *
 * Returns: %TRUE is the search term was valid.
 **/
static gboolean
as_user_search_term_valid (const gchar *term)
{
	guint i;
	for (i = 0; term[i] != '\0'; i++) {
		if (term[i] == '<' ||
		    term[i] == '>' ||
		    term[i] == '(' ||
		    term[i] == ')')
			return FALSE;
	}
	if (i == 1)
		return FALSE;
	return TRUE;
}

/**
 * as_pool_build_search_tokens:
 * @pool: An instance of #AsPool.
 * @search: the (user-provided) search phrase.
 *
 * Splits up a string into an array of tokens that are suitable for searching.
 * This includes stripping whitespaces, casefolding the terms and removing greylist words.
 *
 * This function is usually called automatically when needed, you will only need to
 * run it explicitly when you need to check which search tokens the pool will actually
 * use internally for a given phrase.
 *
 * Returns: (transfer full): Valid tokens to search for, or %NULL for error
 */
gchar**
as_pool_build_search_tokens (AsPool *pool, const gchar *search)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsStemmer) stemmer = NULL;
	g_autofree gchar *search_norm = NULL;
	g_auto(GStrv) words = NULL;
	g_auto(GStrv) strv = NULL;
	gchar **terms;
	guint idx;

	if (search == NULL)
		return NULL;
	search_norm = g_utf8_casefold (search, -1);

	/* filter query by greylist (to avoid overly generic search terms) */
	words = g_strsplit (search_norm, " ", -1);
	for (guint i = 0; words[i] != NULL; i++) {
		as_strstripnl (words[i]);
		for (guint j = 0; priv->term_greylist[j] != NULL; j++) {
			if (g_strcmp0 (words[i], priv->term_greylist[j]) == 0) {
				g_free (words[i]);
				words[i] = g_strdup ("");
			}
		}
	}
	g_free (search_norm);
	search_norm = g_strjoinv (" ", words);

	/* restore query if it was just greylist words */
	g_strstrip (search_norm);
	if (g_strcmp0 (search_norm, "") == 0) {
		g_debug ("grey-list replaced all terms, restoring");
		g_free (search_norm);
		search_norm = g_utf8_casefold (search, -1);
	}

	strv = g_str_tokenize_and_fold (search_norm, priv->locale, NULL);
	/* we might still be able to extract tokens if g_str_tokenize_and_fold() can't do it or +/- were found */
	if (strv == NULL) {
		g_autofree gchar *delim = NULL;
		delim = g_utf8_strdown (search_norm, -1);
		g_strdelimit (delim, "/,.;:", ' ');
		strv = g_strsplit (delim, " ", -1);
	}

	terms = g_new0 (gchar *, g_strv_length (strv) + 1);
	idx = 0;
	stemmer = g_object_ref (as_stemmer_get ());
	for (guint i = 0; strv[i] != NULL; i++) {
		gchar *token;
		if (!as_user_search_term_valid (strv[i]))
			continue;
		/* stem the string and add it to terms */
		token = as_stemmer_stem (stemmer, strv[i]);
		if (token != NULL)
			terms[idx++] = token;
	}
	/* if we have no valid terms, return NULL */
	if (idx == 0) {
		g_strfreev (terms);
		return NULL;
	}

	return terms;
}

/**
 * as_pool_search:
 * @pool: An instance of #AsPool
 * @search: A search string
 *
 * Search for a list of components matching the search term.
 * The list will be ordered by match score.
 *
 * Returns: (transfer container) (element-type AsComponent): an array of the found #AsComponent objects.
 *
 * Since: 0.9.7
 */
GPtrArray*
as_pool_search (AsPool *pool, const gchar *search)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsProfileTask) ptask = NULL;
	g_autoptr(GError) tmp_error = NULL;
	GPtrArray *result = NULL;
	g_auto(GStrv) tokens = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:search");

	/* sanitize user's search term */
	tokens = as_pool_build_search_tokens (pool, search);

	if (tokens == NULL) {
		/* the query was invalid */
		g_autofree gchar *tmp = g_strdup (search);
		as_strstripnl (tmp);
		if (strlen (tmp) <= 1) {
			/* we have a one-letter search query - we cheat here and just return everything */
			g_debug ("Search query too broad. Matching everything.");
			return as_pool_get_components (pool);
		} else {
			g_debug ("No valid search tokens. Can not find any results.");
			return g_ptr_array_new_with_free_func (g_object_unref);
		}
	} else {
		g_autofree gchar *tmp_str = NULL;
		tmp_str = g_strjoinv (" ", tokens);
		g_debug ("Searching for: %s", tmp_str);
	}

	result = as_cache_search (priv->cache, tokens, FALSE, &tmp_error);
	if (result == NULL) {
		g_mutex_lock (&priv->mutex);
		g_warning ("Search in session cache failed: %s", tmp_error->message);
		g_mutex_unlock (&priv->mutex);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

	if (as_pool_can_query_system_cache (pool)) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		tmp_res = as_cache_search (priv->system_cache, tokens, FALSE, &tmp_error);
		if (tmp_res == NULL) {
			g_warning ("Search in system cache failed: %s", tmp_error->message);
			return result;
		}
		as_object_ptr_array_absorb (result, tmp_res);
	}

	/* sort the results by their priority (this was explicitly disabled for the caches before,
	 * so we could sort the combines result list) */
	as_sort_components_by_score (result);

	return result;
}

/**
 * as_pool_refresh_cache:
 * @pool: An instance of #AsPool.
 * @force: Enforce refresh, even if source data has not changed.
 *
 * Update the AppStream cache. There is normally no need to call this function manually, because cache updates are handled
 * transparently in the background.
 *
 * Returns: %TRUE if the cache was updated, %FALSE on error or if the cache update was not necessary and has been skipped.
 */
gboolean
as_pool_refresh_cache (AsPool *pool, gboolean force, GError **error)
{
	return as_pool_refresh_system_cache (pool, FALSE, force, error);
}

/**
 * as_pool_cleanup_cache_dir:
 *
 * Delete all files in a cache directory.
 */
static void
as_pool_cleanup_cache_dir (AsPool *pool, const gchar *cache_dir)
{
	g_autoptr(GFileEnumerator) direnum = NULL;
	g_autoptr(GFile) cdir = NULL;
	g_autoptr(GError) error = NULL;

	cdir =  g_file_new_for_path (cache_dir);
	direnum = g_file_enumerate_children (cdir,
					     G_FILE_ATTRIBUTE_STANDARD_NAME,
					     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					     NULL,
					     &error);
	if (error != NULL) {
		g_debug ("Unable to clean cache directories '%s': %s", cache_dir, error->message);
		return;
	}

	while (TRUE) {
		GFileInfo *finfo = NULL;
		g_autofree gchar *fname_full = NULL;
		const gchar *fname;
		if (!g_file_enumerator_iterate (direnum, &finfo, NULL, NULL, NULL))
			return;
		if (finfo == NULL)
			break;
		if (g_file_info_get_file_type (finfo) != G_FILE_TYPE_REGULAR)
			continue;
		fname = g_file_info_get_name (finfo);
		if (!g_str_has_suffix (fname, ".cache") &&
		    !g_str_has_suffix (fname, ".tmp") &&
		    !g_str_has_suffix (fname, ".mdb"))
			continue;

		fname_full = g_build_filename (cache_dir, fname, NULL);
		g_debug ("Deleting cache file: %s", fname);
		g_unlink (fname_full);
	}
}

/**
 * as_pool_refresh_system_cache:
 * @pool: An instance of #AsPool.
 * @user: Build cache for the current user, instead of system-wide.
 * @force: Enforce refresh, even if source data has not changed.
 *
 * Update the AppStream cache. There is normally no need to call this function manually, because cache updates are handled
 * transparently in the background.
 *
 * Returns: %TRUE if the cache was updated, %FALSE on error or if the cache update was not necessary and has been skipped.
 */
gboolean
as_pool_refresh_system_cache (AsPool *pool, gboolean user, gboolean force, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	gboolean ret = FALSE;
	gboolean cache_updated = FALSE;
	g_autofree gchar *cache_fname = NULL;
	g_autofree gchar *cache_fname_tmp = NULL;
	g_autofree gchar *random_str = NULL;
	g_autoptr(GError) data_load_error = NULL;
	GError *tmp_error = NULL;
	AsCacheFlags prev_cache_flags;
	guint invalid_cpts_n;
	const gchar *sys_cache_dir;

	if (user)
		sys_cache_dir = priv->sys_cache_dir_user;
	else
		sys_cache_dir = priv->sys_cache_dir_system;

	/* try to create cache directory, in case it doesn't exist */
	g_mkdir_with_parents (sys_cache_dir, 0755);
	if (!as_utils_is_writable (sys_cache_dir)) {
		g_set_error (error,
				AS_POOL_ERROR,
				AS_POOL_ERROR_TARGET_NOT_WRITABLE,
				_("Cache location '%s' is not writable."), sys_cache_dir);
		return FALSE;
	}

	/* create the filename of our cache and set the location of the system cache.
	 * This has to happen before we check for new metadata, so the system cache can
	 * determine its age (so we know whether a refresh is needed at all). */
	g_mutex_lock (&priv->mutex);
	cache_fname = g_strdup_printf ("%s/%s.cache", sys_cache_dir, priv->locale);
	as_cache_set_location (priv->system_cache, cache_fname);
	g_mutex_unlock (&priv->mutex);

	/* collect metadata */
#ifdef HAVE_APT_SUPPORT
	/* currently, we only do something here if we are running with explicit APT support compiled in and are root */
	if (!user || as_utils_is_root ()) {
		as_pool_scan_apt (pool, force, &tmp_error);
		if (tmp_error != NULL) {
			/* the exact error is not forwarded here, since we might be able to partially update the cache */
			g_warning ("Error while collecting metadata: %s", tmp_error->message);
			g_clear_error (&tmp_error);
		}
	}
#endif

	/* check if we need to refresh the cache
	 * (which is only necessary if the AppStream data has changed) */
	if (!as_pool_metadata_changed (pool, priv->system_cache, TRUE)) {
		g_debug ("Data did not change, no cache refresh needed.");
		if (force) {
			g_debug ("Forcing refresh anyway.");
		} else {
			return FALSE;
		}
	}
	g_debug ("Refreshing AppStream system data cache");

	/* ensure we start with an empty pool */
	as_cache_close (priv->system_cache);
	as_cache_close (priv->cache);

	/* don't call sync explicitly for a dramatic improvement in speed */
	as_cache_set_nosync (priv->cache, TRUE);

	/* open new system cache as user cache temporarily, so we can modify it */
	g_mutex_lock (&priv->mutex);

	random_str = as_random_alnum_string (8);
	cache_fname_tmp = g_strconcat (cache_fname, random_str, ".tmp", NULL);

	/* remove old files for other languages in per-user mode */
	if (user)
		as_pool_cleanup_cache_dir (pool, sys_cache_dir);

	if (!as_cache_open (priv->cache, cache_fname_tmp, priv->locale, error)) {
		g_mutex_unlock (&priv->mutex);
		g_remove (cache_fname_tmp);
		return FALSE;
	}
	g_mutex_unlock (&priv->mutex);

	/* NOTE: we will only cache AppStream metadata, no .desktop file metadata etc. */

	/* since the session cache is the system cache now (in order to update it), temporarily modify
	 * the cache flags */
	g_mutex_lock (&priv->mutex);
	prev_cache_flags = priv->cache_flags;
	priv->cache_flags = AS_CACHE_FLAG_USE_USER;
	g_mutex_unlock (&priv->mutex);

	/* set cache to floating mode to increase performance by holding all data
	 * in memory in unserialized form */
	as_cache_make_floating (priv->cache);

	/* load AppStream collection metadata only and refine it */
	ret = as_pool_load_collection_data (pool, TRUE, &data_load_error);
	if (data_load_error != NULL)
		g_debug ("Error while updating the in-memory data pool: %s", data_load_error->message);

	/* un-float the cache, persisting all data */
	invalid_cpts_n = as_cache_unfloat (priv->cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_remove (cache_fname_tmp);
		return FALSE;
	}

	/* save the cache object (this will sync it to disk explicitly too) */
	as_cache_close (priv->cache);

	/* atomically replace any old cache */
	g_chmod (cache_fname_tmp, 0644);
	if (g_rename (cache_fname_tmp, cache_fname) < 0)
		g_warning ("Unable to replace old cache '%s': %s", cache_fname, g_strerror (errno));
	else
		cache_updated = TRUE;
	g_clear_pointer (&cache_fname_tmp, g_free);

	/* restore cache flags */
	g_mutex_lock (&priv->mutex);
	priv->cache_flags = prev_cache_flags;
	g_mutex_unlock (&priv->mutex);

	/* switch back to default sync mode */
	as_cache_set_nosync (priv->cache, FALSE);

	/* reset (so the proper session cache is opened again) */
	as_pool_clear2 (pool, NULL);

	if (ret) {
		if (invalid_cpts_n != 0) {
			g_autofree gchar *error_message = NULL;
			if (data_load_error == NULL)
				error_message = g_strdup (_("The AppStream system cache was updated, but some components were ignored. Refer to the verbose log for more information."));
			else
				error_message = g_strdup_printf (_("The AppStream system cache was updated, but problems were found which resulted in metadata being ignored: %s"), data_load_error->message);

			g_set_error_literal (error,
					     AS_POOL_ERROR,
					     AS_POOL_ERROR_INCOMPLETE,
					     error_message);
		}
		/* update the cache mtime, to not needlessly rebuild it again */
		if (cache_updated)
			as_touch_location (cache_fname);
	} else {
		g_set_error (error,
			     AS_POOL_ERROR,
			     AS_POOL_ERROR_FAILED,
			     _("AppStream system cache refresh failed. Turn on verbose mode to get detailed issue information."));
	}

	return TRUE;
}

/**
 * as_pool_set_locale:
 * @pool: An instance of #AsPool.
 * @locale: the locale.
 *
 * Sets the current locale which should be used when parsing metadata.
 **/
void
as_pool_set_locale (AsPool *pool, const gchar *locale)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	g_free (priv->locale);
	priv->locale = g_strdup (locale);
}

/**
 * as_pool_get_locale:
 * @pool: An instance of #AsPool.
 *
 * Gets the currently used locale.
 *
 * Returns: Locale used for metadata parsing.
 **/
const gchar *
as_pool_get_locale (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return priv->locale;
}

/**
 * as_pool_add_metadata_location_internal:
 * @pool: An instance of #AsPool.
 * @directory: An existing filesystem location.
 * @add_root: Whether to add the root directory if necessary.
 *
 * See %as_pool_add_metadata_location()
 */
static void
as_pool_add_metadata_location_internal (AsPool *pool, const gchar *directory, gboolean add_root)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	gboolean dir_added = FALSE;
	gchar *path;
	g_autoptr(GMutexLocker) locker = NULL;

	if (!g_file_test (directory, G_FILE_TEST_IS_DIR)) {
		g_debug ("Not adding metadata location '%s': Is no directory", directory);
		return;
	}

	/* protect access to directory arrays */
	locker = g_mutex_locker_new (&priv->mutex);

	/* metadata locations */
	path = g_build_filename (directory, "xml", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		g_ptr_array_add (priv->xml_dirs, path);
		dir_added = TRUE;
		g_debug ("Added %s to XML metadata search path.", path);
	} else {
		g_free (path);
	}

	path = g_build_filename (directory, "xmls", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		g_ptr_array_add (priv->xml_dirs, path);
		dir_added = TRUE;
		g_debug ("Added %s to XML metadata search path.", path);
	} else {
		g_free (path);
	}

	path = g_build_filename (directory, "yaml", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		g_ptr_array_add (priv->yaml_dirs, path);
		dir_added = TRUE;
		g_debug ("Added %s to YAML metadata search path.", path);
	} else {
		g_free (path);
	}

	if ((add_root) && (!dir_added)) {
		/* we didn't find metadata-specific directories, so let's watch to root path for both YAML and XML */
		g_ptr_array_add (priv->xml_dirs, g_strdup (directory));
		g_ptr_array_add (priv->yaml_dirs, g_strdup (directory));
		g_debug ("Added %s to all metadata search paths.", directory);
	}

	/* icons */
	path = g_build_filename (directory, "icons", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR))
		g_ptr_array_add (priv->icon_dirs, path);
	else
		g_free (path);

}

/**
 * as_pool_add_metadata_location:
 * @pool: An instance of #AsPool.
 * @directory: An existing filesystem location.
 *
 * Add a location for the data pool to read data from.
 * If @directory contains a "xml", "xmls", "yaml" or "icons" subdirectory (or all of them),
 * those paths will be added to the search paths instead.
 */
void
as_pool_add_metadata_location (AsPool *pool, const gchar *directory)
{
	as_pool_add_metadata_location_internal (pool, directory, TRUE);
}

/**
 * as_pool_clear_metadata_locations:
 * @pool: An instance of #AsPool.
 *
 * Remove all metadata locations from the list of watched locations.
 */
void
as_pool_clear_metadata_locations (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	/* clear arrays */
	g_ptr_array_set_size (priv->xml_dirs, 0);
	g_ptr_array_set_size (priv->yaml_dirs, 0);
	g_ptr_array_set_size (priv->icon_dirs, 0);

	g_debug ("Cleared all metadata search paths.");
}

/**
 * as_pool_get_cache_flags:
 * @pool: An instance of #AsPool.
 *
 * Get the #AsCacheFlags for this data pool.
 */
AsCacheFlags
as_pool_get_cache_flags (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return priv->cache_flags;
}

/**
 * as_pool_set_cache_flags:
 * @pool: An instance of #AsPool.
 * @flags: The new #AsCacheFlags.
 *
 * Set the #AsCacheFlags for this data pool.
 */
void
as_pool_set_cache_flags (AsPool *pool, AsCacheFlags flags)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	priv->cache_flags = flags;
}

/**
 * as_pool_get_flags:
 * @pool: An instance of #AsPool.
 *
 * Get the #AsPoolFlags for this data pool.
 */
AsPoolFlags
as_pool_get_flags (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return priv->flags;
}

/**
 * as_pool_set_flags:
 * @pool: An instance of #AsPool.
 * @flags: The new #AsPoolFlags.
 *
 * Set the #AsPoolFlags for this data pool.
 */
void
as_pool_set_flags (AsPool *pool, AsPoolFlags flags)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	priv->flags = flags;
}

/**
 * as_pool_get_system_cache_age:
 * @pool: An instance of #AsPool.
 *
 * Get the age of the system cache.
 */
time_t
as_pool_get_system_cache_age (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	return as_cache_get_ctime (priv->system_cache);
}

/**
 * as_pool_set_cache_location:
 * @pool: An instance of #AsPool.
 * @fname: Filename of the cache file, or special identifier.
 *
 * Sets the name of the cache file. If @fname is ":memory", the cache will be
 * kept in memory, if it is set to ":temporary", the cache will be stored in
 * a temporary directory. In any other case, the given filename is used.
 *
 * Since: 0.12.7
 **/
void
as_pool_set_cache_location (AsPool *pool, const gchar *fname)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	g_free (priv->cache_fname);
	priv->cache_fname = g_strdup (fname);
}

/**
 * as_pool_get_cache_location:
 * @pool: An instance of #AsPool.
 *
 * Gets the location of the session cache.
 *
 * Returns: Location of the cache.
 **/
const gchar*
as_pool_get_cache_location (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return priv->cache_fname;
}

/**
 * as_pool_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_pool_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsPool");
	return quark;
}

/**
 * as_pool_new:
 *
 * Creates a new #AsPool.
 *
 * Returns: (transfer full): a #AsPool
 *
 **/
AsPool*
as_pool_new (void)
{
	AsPool *pool;
	pool = g_object_new (AS_TYPE_POOL, NULL);
	return AS_POOL (pool);
}
