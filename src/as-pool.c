/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-file-monitor.h"
#include "as-profile.h"

#include "as-metadata.h"

typedef struct
{
	gchar		*screenshot_service_url;
	gchar		*locale;
	gchar		*current_arch;
	AsProfile	*profile;

	GHashTable	*std_data_locations;
	GHashTable	*extra_data_locations;

	AsCache		*cache;
	guint		 pending_id; /* source ID for pending auto-reload */

	gchar		**term_greylist;
	AsPoolFlags	flags;

	GRWLock		rw_lock;
} AsPoolPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsPool, as_pool, G_TYPE_OBJECT)

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

#define GET_PRIVATE(o) (as_pool_get_instance_private (o))

/* TRANSLATORS: List of "grey-listed" words sperated with ";"
 * Do not translate this list directly. Instead,
 * provide a list of words in your language that people are likely
 * to include in a search but that should normally be ignored in
 * the search.
 */
#define AS_SEARCH_GREYLIST_STR _("app;application;package;program;programme;suite;tool")

/* Prefixes of locations where system-wide AppStream catalog metadata can be found.
 * TODO: We should really parse $XDG_DATA_DIRS for the /usr location in a safe way,
 * instead of hardcoding one canonical path here. */
static const gchar *SYSTEM_CATALOG_METADATA_PREFIXES[] = { "/usr/share",
							   "/var/lib",
							   "/var/cache",
							   NULL};

/* where .desktop files are installed to by packages to be registered with the system */
static gchar *APPLICATIONS_DIR = "/usr/share/applications";

/* where metainfo files can be found */
static gchar *METAINFO_DIR = "/usr/share/metainfo";

/* cache key used for local metainfo / desktop-entry data */
static gchar *LOCAL_METAINFO_CACHE_KEY = "local-metainfo";

/* cache key used for AppStream collection metadata provided by the OS */
static gchar *OS_COLLECTION_CACHE_KEY = "os-catalog";

typedef struct {
	AsFormatKind		format_kind;
	GRefString		*location;
	gboolean		compressed_only; /* load only compressed data, workaround for Flatpak */
} AsLocationEntry;

typedef struct {
	AsPool			*owner;
	AsComponentScope	scope;
	AsFormatStyle		format_style;
	gboolean		is_os_data;
	GPtrArray		*locations;
	GPtrArray		*icon_dirs;
	GRefString		*cache_key;
	AsFileMonitor		*monitor;
} AsLocationGroup;

static AsLocationEntry*
as_location_entry_new (AsFormatKind format_kind,
		       const gchar *location)
{
	AsLocationEntry *entry;
	entry = g_new0 (AsLocationEntry, 1);

	entry->format_kind = format_kind;
	as_ref_string_assign_safe (&entry->location, location);

	return entry;
}

static void
as_location_entry_free (AsLocationEntry *entry)
{
	as_ref_string_release (entry->location);
	g_free (entry);
}

static void as_pool_cache_refine_component_cb (AsComponent *cpt, gboolean is_serialization, gpointer user_data);
static void as_pool_location_group_monitor_changed_cb (AsFileMonitor *monitor, const gchar *filename, AsLocationGroup *lgroup);

static AsLocationGroup*
as_location_group_new (AsPool *owner,
		       AsComponentScope scope,
		       AsFormatStyle format_style,
		       gboolean is_os_data,
		       const gchar *cache_key)
{
	AsLocationGroup *lgroup;
	lgroup = g_new0 (AsLocationGroup, 1);

	lgroup->owner = owner;
	lgroup->scope = scope;
	lgroup->format_style = format_style;
	lgroup->is_os_data = is_os_data;

	lgroup->locations = g_ptr_array_new_with_free_func ((GDestroyNotify) as_location_entry_free);
	lgroup->icon_dirs = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_release);
	as_ref_string_assign_safe (&lgroup->cache_key, cache_key);

	lgroup->monitor = as_file_monitor_new ();
	g_signal_connect (lgroup->monitor, "changed",
			  G_CALLBACK (as_pool_location_group_monitor_changed_cb),
			  lgroup);
	g_signal_connect (lgroup->monitor, "added",
			  G_CALLBACK (as_pool_location_group_monitor_changed_cb),
			  lgroup);
	g_signal_connect (lgroup->monitor, "removed",
			  G_CALLBACK (as_pool_location_group_monitor_changed_cb),
			  lgroup);

	return lgroup;
}

static void
as_location_group_free (AsLocationGroup *lgroup)
{
	g_object_unref (lgroup->monitor);
	g_ptr_array_unref (lgroup->locations);
	g_ptr_array_unref (lgroup->icon_dirs);
	as_ref_string_release (lgroup->cache_key);

	g_free (lgroup);
}

static AsLocationEntry*
as_location_group_add_dir (AsLocationGroup *lgroup,
			   const gchar *dir,
			   const gchar *icon_dir,
			   AsFormatKind format_kind)
{
	AsPoolPrivate *priv = GET_PRIVATE (lgroup->owner);
	AsLocationEntry *entry;
	g_return_val_if_fail (dir != NULL, NULL);

	entry = as_location_entry_new (format_kind, dir);
	g_ptr_array_add (lgroup->locations, entry);
	if (icon_dir != NULL)
		g_ptr_array_add (lgroup->icon_dirs,
				 g_ref_string_new_intern (icon_dir));

	/* monitor directory for changes if needed */
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_MONITOR)) {
		g_autoptr(GError) error = NULL;
		if (!as_file_monitor_add_directory (lgroup->monitor, dir, NULL, &error))
			g_warning ("Unable to register directory '%s' for monitoring: %s",
				   dir, error->message);
	}

	return entry;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsLocationGroup, as_location_group_free)

/**
 * AsComponentRegistry:
 *
 * A helper object to refine partitions of
 * AppStream metadata properly.
 */
typedef struct {
	GHashTable *data_id_map;
	GHashTable *id_map;
} AsComponentRegistry;

static AsComponentRegistry*
as_component_registry_new (void)
{
	AsComponentRegistry *registry;
	registry = g_new0 (AsComponentRegistry, 1);

	registry->data_id_map = g_hash_table_new_full ((GHashFunc) as_utils_data_id_hash,
						       (GEqualFunc) as_utils_data_id_equal,
							NULL,
							g_object_unref);
	registry->id_map = g_hash_table_new_full (g_str_hash, g_str_equal,
						  NULL, (GDestroyNotify) g_ptr_array_unref);

	return registry;
}

static void
as_component_registry_free (AsComponentRegistry *registry)
{
	g_hash_table_unref (registry->data_id_map);
	g_hash_table_unref (registry->id_map);

	g_free (registry);
}

static void
as_component_registry_add (AsComponentRegistry *registry, AsComponent *cpt)
{
	GPtrArray *list;

	g_hash_table_insert (registry->data_id_map,
				(gchar*) as_component_get_data_id (cpt),
				g_object_ref (cpt));

	list = g_hash_table_lookup (registry->id_map, as_component_get_id (cpt));
	if (list == NULL) {
		list = g_ptr_array_new_with_free_func (g_object_unref);
		g_hash_table_insert (registry->id_map,
				     (gchar*) as_component_get_id (cpt),
				     list);
	}
	g_ptr_array_add (list,
			 g_object_ref (cpt));
}

static AsComponent*
as_component_registry_lookup (AsComponentRegistry *registry, const gchar *data_id)
{
	return g_hash_table_lookup (registry->data_id_map, data_id);
}

static void
as_component_registry_remove (AsComponentRegistry *registry, const gchar *data_id)
{
	g_hash_table_remove (registry->data_id_map, data_id);
}

static gboolean
as_component_registry_has_cid (AsComponentRegistry *registry, const gchar *cid)
{
	return g_hash_table_contains (registry->id_map, cid);
}

static GPtrArray*
as_component_registry_get_components_by_id (AsComponentRegistry *registry, const gchar *cid)
{
	return g_hash_table_lookup (registry->id_map, cid);
}

static GPtrArray*
as_component_registry_get_contents (AsComponentRegistry *registry)
{
	GPtrArray *cpt_array;
	GHashTableIter iter;
	gpointer value;

	cpt_array = g_ptr_array_new_with_free_func (g_object_unref);
	g_hash_table_iter_init (&iter, registry->data_id_map);
	while (g_hash_table_iter_next (&iter, NULL, &value))
		g_ptr_array_add (cpt_array, g_object_ref (value));

	return cpt_array;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsComponentRegistry, as_component_registry_free)

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

	g_rw_lock_init (&priv->rw_lock);

	priv->profile = as_profile_new ();

	/* set active locale */
	priv->locale = as_get_current_locale ();

	/* well-known default metadata directories */
	priv->std_data_locations = g_hash_table_new_full (g_str_hash, g_str_equal,
							  g_free, (GDestroyNotify) as_location_group_free);

	/* user-defined collection metadata locations */
	priv->extra_data_locations = g_hash_table_new_full (g_str_hash, g_str_equal,
							    g_free, (GDestroyNotify) as_location_group_free);

	/* set the current architecture */
	priv->current_arch = as_get_current_arch ();

	/* set up our localized search-term greylist */
	priv->term_greylist = g_strsplit (AS_SEARCH_GREYLIST_STR, ";", -1);

	/* create caches */
	priv->cache = as_cache_new ();

	/* set callback to refine components after deserialization */
	as_cache_set_refine_func (priv->cache, as_pool_cache_refine_component_cb);

	distro = as_distro_details_new ();
	priv->screenshot_service_url = as_distro_details_get_str (distro, "ScreenshotUrl");

	/* set default pool flags */
	priv->flags = AS_POOL_FLAG_LOAD_OS_COLLECTION |
		      AS_POOL_FLAG_LOAD_OS_METAINFO |
		      AS_POOL_FLAG_LOAD_FLATPAK;

	/* check whether we might want to prefer local metainfo files over remote data */
	if (as_distro_details_get_bool (distro, "PreferLocalMetainfoData", FALSE))
		as_pool_add_flags (pool, AS_POOL_FLAG_PREFER_OS_METAINFO);
}

/**
 * as_pool_finalize:
 **/
static void
as_pool_finalize (GObject *object)
{
	AsPool *pool = AS_POOL (object);
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_rw_lock_writer_lock (&priv->rw_lock);

	if (priv->pending_id)
		g_source_remove (priv->pending_id);

	g_free (priv->screenshot_service_url);

	g_hash_table_unref (priv->std_data_locations);
	g_hash_table_unref (priv->extra_data_locations);

	g_object_unref (priv->cache);

	g_free (priv->locale);
	g_free (priv->current_arch);
	g_strfreev (priv->term_greylist);

	g_object_unref (priv->profile);

	g_rw_lock_writer_unlock (&priv->rw_lock);
	g_rw_lock_clear (&priv->rw_lock);

	G_OBJECT_CLASS (as_pool_parent_class)->finalize (object);
}

/**
 * as_pool_class_init:
 **/
static void
as_pool_class_init (AsPoolClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * AsPool::changed:
	 * @pool: the #AsPool instance that emitted the signal
	 *
	 * The ::changed signal is emitted when components have been added
	 * or removed from the metadata pool.
	 *
	 * Since: 0.15.0
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsPoolClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	object_class->finalize = as_pool_finalize;
}

/**
 * as_pool_add_catalog_metadata_dir_internal:
 * @pool: An instance of #AsPool.
 * @lgroup: The location grouping to add to.
 * @directory: An existing filesystem location.
 * @add_root: Whether to add the root directory if necessary.
 * @with_legacy_support: Whether some legacy support should be enabled.
 *
 * See %as_pool_add_metadata_location()
 */
static void
as_pool_add_catalog_metadata_dir_internal (AsPool *pool,
					   AsLocationGroup *lgroup,
					   const gchar *directory,
					   gboolean add_root,
					   gboolean with_legacy_support)
{
	gboolean dir_added = FALSE;
	g_autofree gchar *icon_dir = NULL;
	gchar *path;

	if (!g_file_test (directory, G_FILE_TEST_IS_DIR)) {
		g_debug ("Not adding metadata catalog location '%s': Not a directory, or does not exist.",
			 directory);
		return;
	}

	/* icons */
	icon_dir = g_build_filename (directory, "icons", NULL);
	if (!g_file_test (icon_dir, G_FILE_TEST_IS_DIR))
		g_free (g_steal_pointer (&icon_dir));

	/* metadata locations */
	path = g_build_filename (directory, "xml", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		as_location_group_add_dir (lgroup,
					   path,
					   icon_dir,
					   AS_FORMAT_KIND_XML);
		dir_added = TRUE;
	}
	g_free (path);

	if (with_legacy_support) {
		path = g_build_filename (directory, "xmls", NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			as_location_group_add_dir (lgroup,
						path,
						icon_dir,
						AS_FORMAT_KIND_XML);
			dir_added = TRUE;
		}
		g_free (path);
	}

	path = g_build_filename (directory, "yaml", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		as_location_group_add_dir (lgroup,
					   path,
					   icon_dir,
					   AS_FORMAT_KIND_YAML);
		dir_added = TRUE;
	}
	g_free (path);

	if ((add_root) && (!dir_added)) {
		/* we didn't find metadata-specific directories, so let's watch to root path for both YAML and XML */
		as_location_group_add_dir (lgroup,
					   directory,
					   icon_dir,
					   AS_FORMAT_KIND_XML);
		as_location_group_add_dir (lgroup,
					   directory,
					   icon_dir,
					   AS_FORMAT_KIND_YAML);
		g_debug ("Added %s to YAML and XML metadata watch locations.", directory);
	}
}

/**
 * as_pool_register_flatpak_dir:
 *
 * Find Flatpak metadata in the given directory and add it to the
 * standard metadata set.
 */
static void
as_pool_register_flatpak_dir (AsPool *pool, const gchar *flatpak_root_dir, AsComponentScope scope)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GFileEnumerator) fpr_direnum = NULL;
	g_autoptr(GFile) fpr_dir = NULL;
	g_autoptr(GError) error = NULL;

	if (!g_file_test (flatpak_root_dir, G_FILE_TEST_EXISTS))
		return;

	fpr_dir =  g_file_new_for_path (flatpak_root_dir);
	fpr_direnum = g_file_enumerate_children (fpr_dir,
					     G_FILE_ATTRIBUTE_STANDARD_NAME,
					     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					     NULL, &error);
	if (error != NULL) {
		g_warning ("Unable to read Flatpak directory '%s': %s",
			   flatpak_root_dir, error->message);
		return;
	}

	while (TRUE) {
		GFileInfo *repo_finfo = NULL;
		const gchar *repo_name;
		g_autofree gchar *repo_fname = NULL;

		if (!g_file_enumerator_iterate (fpr_direnum, &repo_finfo, NULL, NULL, &error)) {
			g_warning ("Unable to iterate Flatpak directory '%s': %s",
			   flatpak_root_dir, error->message);
			return;
		}
		if (repo_finfo == NULL)
			break;

		/* generate full repo filename */
		repo_name = g_file_info_get_name (repo_finfo);
		repo_fname = g_build_filename (flatpak_root_dir, repo_name, NULL);

		/* jump one directory deeper */
		if (g_file_info_get_file_type (repo_finfo) == G_FILE_TYPE_DIRECTORY) {
			g_autoptr(GFileEnumerator) repo_direnum = NULL;
			g_autoptr(GFile) repo_dir = NULL;

			repo_dir =  g_file_new_for_path (repo_fname);
			repo_direnum = g_file_enumerate_children (repo_dir,
								  G_FILE_ATTRIBUTE_STANDARD_NAME,
								  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
								  NULL, &error);
			if (repo_direnum == NULL) {
				g_warning ("Unable to scan Flatpak repository directory '%s': %s",
					   repo_fname, error->message);
				g_error_free (g_steal_pointer (&error));
				continue;
			}

			while (TRUE) {
				GFileInfo *repo_arch_finfo = NULL;
				g_autofree gchar *fp_appstream_dir = NULL;
				g_autofree gchar *fp_appstream_icons_dir = NULL;
				g_autofree gchar *cache_key = NULL;
				g_autoptr(AsLocationGroup) lgroup = NULL;
				AsLocationEntry *lentry;
				const gchar *arch_name;

				if (!g_file_enumerator_iterate (repo_direnum, &repo_arch_finfo, NULL, NULL, &error)) {
					g_warning ("Unable to iterate Flatpak repository directory '%s': %s",
						   repo_fname, error->message);
					g_error_free (g_steal_pointer (&error));
					break;
				}
				if (repo_arch_finfo == NULL)
					break;
				if (g_file_info_get_file_type (repo_arch_finfo) != G_FILE_TYPE_DIRECTORY)
					continue;

				arch_name = g_file_info_get_name (repo_arch_finfo);

				/* add the Flatpak AppStream metadata dir */
				fp_appstream_dir = g_build_filename (repo_fname, arch_name, "active", NULL);
				fp_appstream_icons_dir = g_build_filename (fp_appstream_dir, "icons", NULL);
				cache_key = g_strconcat ("flatpak-", repo_name, "-", arch_name, NULL);
				lgroup = as_location_group_new (pool,
								scope,
								AS_FORMAT_STYLE_COLLECTION,
								FALSE, /* no OS data, external from Flatpak */
								cache_key);
				lentry = as_location_group_add_dir (lgroup,
								    fp_appstream_dir,
								    fp_appstream_icons_dir,
								    AS_FORMAT_KIND_XML);
				if (lentry == NULL) {
					g_critical ("Unable to watch possibly invalid Flatpak metadata directory %s", fp_appstream_dir);
					continue;
				}

				/* Flatpak keeps an uncompressed copy of the same data in the same directory - AppStream does not expect
				 * that, so we work around this issue to not load unnecessary data. Loading the compressed data was faster
				 * in every tested scenario, so that's the way to go. */
				lentry->compressed_only = TRUE;
				g_hash_table_insert (priv->std_data_locations,
						     g_strdup (cache_key),
						     g_steal_pointer (&lgroup));
			}
		} else {
			g_warning ("Flatpak metadata repository at '%s' is not a directory.",
				   repo_fname);
			continue;
		}
	}
}

/**
 * as_pool_detect_std_metadata_dirs:
 *
 * Find common AppStream metadata directories.
 */
static void
as_pool_detect_std_metadata_dirs (AsPool *pool, gboolean include_user_data)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	AsLocationGroup *lgroup_catalog;
	AsLocationGroup *lgroup_metainfo;

	/* NOTE: Data must be write-locked by the calling function. */

	/* clear existing entries */
	g_hash_table_remove_all (priv->std_data_locations);

	/* create location groups and register them */
	lgroup_catalog = as_location_group_new (pool,
					     AS_COMPONENT_SCOPE_SYSTEM,
					     AS_FORMAT_STYLE_COLLECTION,
					     TRUE, /* is OS data */
					     OS_COLLECTION_CACHE_KEY);
	g_hash_table_insert (priv->std_data_locations,
			     g_strdup (lgroup_catalog->cache_key),
			     lgroup_catalog);
	lgroup_metainfo = as_location_group_new (pool,
						 AS_COMPONENT_SCOPE_SYSTEM,
						 AS_FORMAT_STYLE_METAINFO,
						 TRUE, /* is OS data */
						 LOCAL_METAINFO_CACHE_KEY);
	g_hash_table_insert (priv->std_data_locations,
			     g_strdup (lgroup_metainfo->cache_key),
			     lgroup_metainfo);

	/* we always need to look at both if either Metainfo loading or desktop-entry loading is enabled.
	 * the fine sorting can happen later. */
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES) ||
	    as_flags_contains (priv->flags, AS_POOL_FLAG_LOAD_OS_METAINFO)) {
		/* desktop-entry */
		if (g_file_test (APPLICATIONS_DIR, G_FILE_TEST_IS_DIR)) {
			as_location_group_add_dir (lgroup_metainfo,
						APPLICATIONS_DIR,
						NULL, /* no icon dir */
						AS_FORMAT_KIND_DESKTOP_ENTRY);
		} else {
			g_debug ("System applications desktop-entry directory was not found!");
		}

		/* metainfo files */
		if (g_file_test (METAINFO_DIR, G_FILE_TEST_IS_DIR)) {
			as_location_group_add_dir (lgroup_metainfo,
						METAINFO_DIR,
						NULL, /* no icon dir */
						AS_FORMAT_KIND_XML);
		} else {
			g_debug ("System installed MetaInfo directory was not found!");
		}
	}

	/* add collection XML directories for the OS */
	/* check if we are permitted to load this */
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_LOAD_OS_COLLECTION)) {
		for (guint i = 0; SYSTEM_CATALOG_METADATA_PREFIXES[i] != NULL; i++) {
			g_autofree gchar *catalog_path = NULL;
			g_autofree gchar *catalog_legacy_path = NULL;
			gboolean ignore_legacy_path = FALSE;

			catalog_path = g_build_filename (SYSTEM_CATALOG_METADATA_PREFIXES[i], "swcatalog", NULL);
			catalog_legacy_path = g_build_filename (SYSTEM_CATALOG_METADATA_PREFIXES[i], "app-info", NULL);

			/* ignore compatibility symlink if one exists */
			if (g_file_test (catalog_legacy_path, G_FILE_TEST_IS_SYMLINK)) {
				g_autofree gchar *link_target = g_file_read_link (catalog_legacy_path, NULL);
				if (link_target != NULL) {
					if (g_strcmp0 (link_target, catalog_path) == 0) {
						ignore_legacy_path = TRUE;
						g_debug ("Ignoring legacy catalog location '%s'.", catalog_legacy_path);
					}
				}
			}

			as_pool_add_catalog_metadata_dir_internal (pool,
								   lgroup_catalog,
								   catalog_path,
								   FALSE, /* add root */
								   FALSE /* no legacy support */);

			if (!ignore_legacy_path)
				as_pool_add_catalog_metadata_dir_internal (pool,
									   lgroup_catalog,
									   catalog_legacy_path,
									   FALSE, /* add root */
									   TRUE /* enable legacy support */);
		}
	}

	/* Add directories belonging to the Flatpak bundling system */
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_LOAD_FLATPAK)) {
		as_pool_register_flatpak_dir (pool,
						"/var/lib/flatpak/appstream/",
						AS_COMPONENT_SCOPE_SYSTEM);
		if (include_user_data) {
			g_autofree gchar *flatpak_user_dir = g_build_filename (g_get_user_data_dir (),
									       "flatpak",
										"appstream",
										NULL);
			as_pool_register_flatpak_dir (pool,
						      flatpak_user_dir,
						      AS_COMPONENT_SCOPE_USER);
		}
	}
}

/**
 * as_pool_add_component_internal:
 * @pool: An instance of #AsPool
 * @registry: Hash table for the results / known components
 * @cpt: The #AsComponent to add to the pool.
 * @pedantic_noadd: If %TRUE, always emit an error if component couldn't be added.
 * @error: A #GError or %NULL
 *
 * Internal.
 */
static gboolean
as_pool_add_component_internal (AsPool *pool,
				AsComponentRegistry *registry,
				AsComponent *cpt,
				gboolean pedantic_noadd,
				GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	const gchar *cdid = NULL;
	AsComponent *existing_cpt;
	gint pool_priority;
	AsOriginKind new_cpt_orig_kind;
	AsOriginKind existing_cpt_orig_kind;
	AsMergeKind new_cpt_merge_kind;

	cdid = as_component_get_data_id (cpt);
	if (as_component_is_ignored (cpt)) {
		if (pedantic_noadd)
			g_set_error (error,
					AS_POOL_ERROR,
					AS_POOL_ERROR_FAILED,
					"Skipping '%s' from inclusion into the pool: Component is ignored.", cdid);
		return FALSE;
	}

	existing_cpt = as_component_registry_lookup (registry, cdid);
	if (as_component_get_origin_kind (cpt) == AS_ORIGIN_KIND_DESKTOP_ENTRY) {
		/* .desktop entries might map to existing metadata data with or without .desktop suffix, we need to check for that.
		 * (the .desktop suffix is optional for desktop-application metainfo files, and the desktop-entry parser will automatically
		 * omit it if the desktop-entry-id is following the reverse DNS scheme)
		 */
		if (existing_cpt == NULL) {
			g_autofree gchar *tmp_cdid = NULL;
			tmp_cdid = g_strdup_printf ("%s.desktop", cdid);
			existing_cpt = as_component_registry_lookup (registry, tmp_cdid);
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
		GPtrArray *matches;

		/* we merge the data into all components with matching IDs at time */
		matches = as_component_registry_get_components_by_id (registry,
								      as_component_get_id (cpt));
		if (matches == NULL)
			return TRUE;

		for (guint i = 0; i < matches->len; i++) {
			AsComponent *match = AS_COMPONENT (g_ptr_array_index (matches, i));
			if (new_cpt_merge_kind == AS_MERGE_KIND_REMOVE_COMPONENT) {
				/* remove matching component from pool if its priority is lower */
				if (as_component_get_priority (match) < as_component_get_priority (cpt)) {
					const gchar *match_cdid = as_component_get_data_id (match);
					as_component_registry_remove (registry, match_cdid);
					g_debug ("Removed via merge component: %s", match_cdid);
				}
			} else {
				as_component_merge (match, cpt);
			}
		}

		return TRUE;
	}

	if (existing_cpt == NULL) {
		as_component_registry_add (registry, cpt);
		return TRUE;
	}

	/* safety check so we don't ignore a good component because we added a bad one first */
	if (!as_component_is_valid (existing_cpt)) {
		g_debug ("Replacing invalid component '%s' with new one.", cdid);
		as_component_registry_add (registry, cpt);
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

			as_component_registry_add (registry, cpt);
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
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_PREFER_OS_METAINFO) &&
	    (new_cpt_orig_kind == AS_ORIGIN_KIND_METAINFO)) {
		/* update package info, metainfo files do never have this data.
		 * (we hope that collection data was loaded first here, so the existing_cpt already contains
		 *  the information we want - if that's not the case, no harm is done here) */
		as_component_set_pkgnames (cpt, as_component_get_pkgnames (existing_cpt));
		as_component_set_bundles_array (cpt, as_component_get_bundles (existing_cpt));

		as_component_registry_add (registry, cpt);
		g_debug ("Replaced '%s' with data from metainfo file.", cdid);
		return TRUE;
	}

	/* if we are here, we might have duplicates and no merges, so check if we should replace a component
	 * with data of higher priority, or if we have an actual error in the metadata */
	pool_priority = as_component_get_priority (existing_cpt);
	if (pool_priority < as_component_get_priority (cpt)) {
		as_component_registry_add (registry, cpt);
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
						as_component_registry_add (registry, cpt);
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
 * as_pool_add_components:
 * @pool: An instance of #AsPool
 * @cpts: (element-type AsComponent): Array of components to add to the pool.
 * @error: A #GError or %NULL
 *
 * Register a set of components with the pool temporarily.
 * Data from components added like this will not be cached.
 *
 * Returns: %TRUE if the new components were successfully added to the pool.
 *
 * Since: 0.15.0
 */
gboolean
as_pool_add_components (AsPool *pool, GPtrArray *cpts, GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	return as_cache_add_masking_components (priv->cache,
						cpts,
						error);
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
 *
 * Deprecated: 0.15.0: This function is very inefficient. Collect all the components you need
 *                     to add, and then register them with %as_pool_add_components in one go.
 */
gboolean
as_pool_add_component (AsPool *pool, AsComponent *cpt, GError **error)
{
	g_autoptr(GPtrArray) array = g_ptr_array_new ();
	g_ptr_array_add (array, cpt);
	return as_pool_add_components (pool, array, error);
}

/**
 * as_pool_clear:
 * @pool: An #AsPool.
 *
 * Remove all metadata from the pool, data will be reloaded
 * once %as_pool_load is called again.
 */
void
as_pool_clear (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	as_cache_clear (priv->cache);
	as_cache_set_locale (priv->cache, priv->locale);
}

/**
 * as_pool_clear2:
 * @pool: An #AsPool.
 *
 * Remove all metadata from the pool.
 */
gboolean
as_pool_clear2 (AsPool *pool, GError **error)
{
	as_pool_clear (pool);
	return TRUE;
}

/**
 * as_pool_load_collection_data:
 *
 * Load metadata from AppStream collection data directories,
 * which are usually provided by some kind of software repository.
 */
static gboolean
as_pool_load_collection_data (AsPool *pool,
			      AsComponentRegistry *registry,
			      AsLocationGroup *lgroup,
			      GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	GPtrArray *cpts;
	g_autoptr(GPtrArray) merge_cpts = NULL;
	gboolean ret;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GPtrArray) mdata_files = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* NOTE: Write-lock is held by the caller. */

	/* do nothing if the group has the wrong format */
	if (lgroup->format_style != AS_FORMAT_STYLE_COLLECTION)
		return TRUE;

	ptask = as_profile_start_literal (priv->profile, "AsPool:load_collection_data");

	/* prepare metadata parser */
	metad = as_metadata_new ();
	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_COLLECTION);
	as_metadata_set_locale (metad, priv->locale);

	/* find AppStream metadata */
	ret = TRUE;
	mdata_files = g_ptr_array_new_with_free_func (g_free);

	for (guint i = 0; i < lgroup->locations->len; i++) {
		AsLocationEntry *lentry = g_ptr_array_index (lgroup->locations, i);

		/* skip things we can't handle */
		if (lentry->format_kind == AS_FORMAT_KIND_DESKTOP_ENTRY)
			continue;

		/* find XML data */
		if (lentry->format_kind == AS_FORMAT_KIND_XML) {
			g_autoptr(GPtrArray) xmls = NULL;
			g_debug ("Searching for XML data in: %s", lentry->location);
			if (lentry->compressed_only)
				xmls = as_utils_find_files_matching (lentry->location, "*.xml.gz", FALSE, NULL);
			else
				xmls = as_utils_find_files_matching (lentry->location, "*.xml*", FALSE, NULL);
			if (xmls != NULL) {
				for (guint j = 0; j < xmls->len; j++) {
					const gchar *val;
					val = (const gchar *) g_ptr_array_index (xmls, j);
					g_ptr_array_add (mdata_files,
								g_strdup (val));
				}
			}
		}

		/* find YAML metadata */
		if (lentry->format_kind == AS_FORMAT_KIND_YAML) {
			g_autoptr(GPtrArray) yamls = NULL;

			g_debug ("Searching for YAML data in: %s", lentry->location);
			if (lentry->compressed_only)
				yamls = as_utils_find_files_matching (lentry->location, "*.yml.gz", FALSE, NULL);
			else
				yamls = as_utils_find_files_matching (lentry->location, "*.yml*", FALSE, NULL);
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

		as_component_set_scope (cpt, lgroup->scope);

		/* deal with merge-components later */
		if (as_component_get_merge_kind (cpt) != AS_MERGE_KIND_NONE) {
			g_ptr_array_add (merge_cpts, cpt);
			continue;
		}

		as_pool_add_component_internal (pool, registry, cpt, TRUE, &tmp_error);
		if (tmp_error != NULL) {
			g_debug ("Metadata ignored: %s", tmp_error->message);
			g_clear_pointer (&tmp_error, g_error_free);
		}
	}

	/* we need to merge the merge-components into the pool last, so the merge process can fetch
	 * all components with matching IDs from the pool */
	for (guint i = 0; i < merge_cpts->len; i++) {
		AsComponent *mcpt = AS_COMPONENT (g_ptr_array_index (merge_cpts, i));

		as_pool_add_component_internal (pool, registry, mcpt, TRUE, &tmp_error);
		if (tmp_error != NULL) {
			g_debug ("Merge component ignored: %s", tmp_error->message);
			g_clear_pointer (&tmp_error, g_error_free);
		}
	}

	return ret;
}

/**
 * as_pool_cache_refine_component_cb:
 *
 * Callback function run on components before they are
 * (de)serialized.
 */
static void
as_pool_cache_refine_component_cb (AsComponent *cpt, gboolean is_serialization, gpointer user_data)
{
	AsLocationGroup *lgroup = user_data;
	AsPool *pool = lgroup->owner;
	AsPoolPrivate *priv = GET_PRIVATE (pool);

	/* NOTE: Write-lock on AsPool structures is held by the caller (as the direct caller is
	 * on the same thread and itself triggered by a write-locked AsPool). */

	if (is_serialization) {
		/* we refine everything except for the icon paths, as that is expensive on many components,
		 * and needs to be done partially again on deserialization anyway */
		/* FIXME: We *do* resolve icon paths here, as not doing so currently causes issues for some apps.
		 * There's a test case for this, so we can address the issue later. */
		as_component_complete (cpt,
					priv->screenshot_service_url,
					lgroup->icon_dirs);
	} else {
		/* add additional data to the component, e.g. external screenshots. Also refines
		 * the component's icon paths */
		as_component_complete (cpt,
					priv->screenshot_service_url,
					lgroup->icon_dirs);
	}
}

/**
 * as_pool_update_desktop_entries_table:
 *
 * Load metadata from desktop-entry files.
 */
static void
as_pool_update_desktop_entries_table (AsPool *pool, GHashTable *de_cpt_table, const gchar *apps_dir)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GPtrArray) de_files = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;
	GError *error = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:get_desktop_entries_table");

	/* prepare metadata parser */
	metad = as_metadata_new ();
	as_metadata_set_locale (metad, priv->locale);

	/* find .desktop files */
	g_debug ("Searching for data in: %s", apps_dir);
	de_files = as_utils_find_files_matching (apps_dir, "*.desktop", FALSE, NULL);
	if (de_files == NULL) {
		g_debug ("Unable find desktop-entry files in '%s'.", apps_dir);
		return;
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
}

/**
 * as_pool_load_metainfo_data:
 *
 * Load metadata from locally installed metainfo files.
 */
static void
as_pool_load_metainfo_data (AsPool *pool,
			    AsComponentRegistry *registry,
			    GHashTable *desktop_entry_cpts,
			    AsComponentScope scope,
			    const gchar *metainfo_dir,
			    const gchar *cache_key)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GPtrArray) mi_files = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;
	GError *error = NULL;

	/* NOTE: Write-lock is held by the caller. */

	ptask = as_profile_start (priv->profile, "AsPool:load_metainfo_data:%s", cache_key);

	/* prepare metadata parser */
	metad = as_metadata_new ();
	as_metadata_set_locale (metad, priv->locale);

	/* find metainfo files */
	g_debug ("Searching for data in: %s", metainfo_dir);
	mi_files = as_utils_find_files_matching (metainfo_dir, "*.xml", FALSE, NULL);
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

		if (!as_flags_contains (priv->flags, AS_POOL_FLAG_PREFER_OS_METAINFO)) {
			g_autofree gchar *mi_cid = NULL;

			mi_cid = g_path_get_basename (fname);
			if (g_str_has_suffix (mi_cid, ".metainfo.xml"))
				mi_cid[strlen (mi_cid) - 13] = '\0';
			if (g_str_has_suffix (mi_cid, ".appdata.xml")) {
				g_autofree gchar *mi_cid_desktop = NULL;
				mi_cid[strlen (mi_cid) - 12] = '\0';

				mi_cid_desktop = g_strdup_printf ("%s.desktop", mi_cid);
				/* check with .desktop suffix too */
				if (as_component_registry_has_cid (registry, mi_cid_desktop)) {
					g_debug ("Skipped: %s (already known)", fname);
					continue;
				}
			}

			/* quickly check if we know the component already */
			if (as_component_registry_has_cid (registry, mi_cid)) {
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

		/* set scope of these metainfo files */
		as_component_set_scope (cpt, scope);

		/* if we are at system scope, we just assume an "os" origin for metainfo files */
		if (scope == AS_COMPONENT_SCOPE_SYSTEM)
			as_component_set_origin (cpt, "os");

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

		as_pool_add_component_internal (pool, registry, cpt, FALSE, &error);
		if (error != NULL) {
			g_debug ("Component '%s' ignored: %s", as_component_get_data_id (cpt), error->message);
			g_error_free (error);
			error = NULL;
		}
	}
}

/**
 * as_pool_process_metainfo_desktop_data:
 *
 * Load metadata from metainfo files and .desktop files that
 * were made available by locally installed applications.
 */
static void
as_pool_process_metainfo_desktop_data (AsPool *pool,
				       AsComponentRegistry *registry,
				       AsLocationGroup *lgroup,
				       const gchar *cache_key)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GHashTable) de_cpts = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* NOTE: Write-lock is held by the caller. */

	/* check if we actually need to load anything */
	if (!as_flags_contains (priv->flags, AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES) && !as_flags_contains (priv->flags, AS_POOL_FLAG_LOAD_OS_METAINFO))
		return;

	/* check if the group has the right format */
	if (lgroup->format_style != AS_FORMAT_STYLE_METAINFO)
		return;

	ptask = as_profile_start (priv->profile, "AsPool:load_metainfo_desktop_data:%s", cache_key);

	/* create a hashmap of all desktop-entry components we know of */
	de_cpts = g_hash_table_new_full (g_str_hash,
					 g_str_equal,
					 g_free,
					 (GDestroyNotify) g_object_unref);
	for (guint i = 0; i < lgroup->locations->len; i++) {
		AsLocationEntry *lentry = (AsLocationEntry*) g_ptr_array_index (lgroup->locations, i);
		if (lentry->format_kind != AS_FORMAT_KIND_DESKTOP_ENTRY)
			continue;
		as_pool_update_desktop_entries_table (pool, de_cpts, lentry->location);
	}

	if (as_flags_contains (priv->flags, AS_POOL_FLAG_LOAD_OS_METAINFO)) {
		for (guint i = 0; i < lgroup->locations->len; i++) {
			AsLocationEntry *lentry = (AsLocationEntry*) g_ptr_array_index (lgroup->locations, i);
			if (lentry->format_kind != AS_FORMAT_KIND_XML)
				continue;

			/* load metainfo components, absorb desktop-entry components into them */
			as_pool_load_metainfo_data (pool,
						    registry,
						    de_cpts,
						    lgroup->scope,
						    lentry->location,
						    cache_key);
		}
	}

	/* read all remaining .desktop file components, if needed */
	if (as_flags_contains (priv->flags, AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES)) {
		GHashTableIter iter;
		gpointer value;
		GError *error = NULL;

		g_debug ("Adding components from desktop-entry files to the metadata pool.");
		g_hash_table_iter_init (&iter, de_cpts);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			AsComponent *cpt = AS_COMPONENT (value);

			as_pool_add_component_internal (pool, registry, cpt, FALSE, &error);
			if (error != NULL) {
				g_debug ("Ignored component '%s': %s",
					 as_component_get_data_id (cpt), error->message);
				g_error_free (error);
				error = NULL;
			}
		}
	}
}

/**
 * as_pool_get_ctime:
 *
 * Returns: The changed time of a location.
 */
static time_t
as_get_location_ctime (const gchar *location)
{
	struct stat sb;
	if (stat (location, &sb) < 0)
		return 0;
	return sb.st_ctime;
}

/**
 * as_pool_loader_process_group:
 *
 * Process a location group and load all data from the source files
 * or a suitable cache section.
 */
static gboolean
as_pool_loader_process_group (AsPool *pool,
			      AsLocationGroup *lgroup,
			      gboolean force_cache_refresh,
			      gboolean *caches_updated,
			      GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsComponentRegistry) registry = NULL;
	g_autoptr(GPtrArray) final_results = NULL;
	time_t cache_time;
	gboolean cache_outdated = FALSE;
	gboolean ret;

	/* NOTE: Write-lock is held by the caller. */

	if (lgroup->locations->len == 0)
		return TRUE;

	/* first check if we can load cache data */
	if (!force_cache_refresh && !as_flags_contains (priv->flags, AS_POOL_FLAG_IGNORE_CACHE_AGE)) {
		cache_time = as_cache_get_ctime (priv->cache,
						lgroup->scope,
						lgroup->cache_key,
						NULL);
		for (guint i = 0; i < lgroup->locations->len; i++) {
			AsLocationEntry *lentry = (AsLocationEntry*) g_ptr_array_index (lgroup->locations, i);
			if (as_get_location_ctime (lentry->location) > cache_time) {
				cache_outdated = TRUE;
				break;
			}
		}

		if (!cache_outdated) {
			/* cache is not out of data, let's use it! */
			g_debug ("Using cached metadata: %s", lgroup->cache_key);
			as_cache_load_section_for_key (priv->cache,
							lgroup->scope,
							lgroup->format_style,
							lgroup->is_os_data,
							lgroup->cache_key,
							&cache_outdated,
							lgroup);
			if (!cache_outdated) {
				/* cache was fine and is now loaded, we are done here */
				return TRUE;
			}
			/* if we are here, the cache either went out of date (e.g. by being removed)
			* or loading failed, in which case we will just regenerate it */
			g_debug ("Failed to load cache metadata for '%s' or cache suddenly went out of data. Regenerating cache.",
					lgroup->cache_key);
		}
	}

	/* container for the generated components */
	registry = as_component_registry_new ();

	/* process any MetaInfo and desktop-entry files */
	as_pool_process_metainfo_desktop_data (pool, registry,
					lgroup,
					lgroup->cache_key);

	/* process collection data - we intentionally ignore errors here, and just skip any broken metadata*/
	as_pool_load_collection_data (pool,
					registry,
					lgroup,
					NULL);

	/* save cache section */
	final_results = as_component_registry_get_contents (registry);
	ret = as_cache_set_contents_for_section (priv->cache,
						lgroup->scope,
						lgroup->format_style,
						lgroup->is_os_data,
						final_results,
						lgroup->cache_key,
						lgroup,
						error);
	if (!ret)
		return FALSE;

	/* we updated caches */
	if (caches_updated != NULL)
		*caches_updated = TRUE;

	return TRUE;
}

/**
 * as_pool_load_internal:
 *
 * Load metadata, either by reading the source files or loading
 * an up-to-date cache of them.
 */
static gboolean
as_pool_load_internal (AsPool *pool,
		       gboolean include_user_data,
		       gboolean force_cache_refresh,
		       gboolean *caches_updated,
		       GCancellable *cancellable,
		       GError **error)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(AsProfileTask) ptask = NULL;
	GHashTableIter loc_iter;
	gpointer loc_value;
	gboolean ret = TRUE;
	g_autoptr(GRWLockWriterLocker) locker = NULL;

	ptask = as_profile_start_literal (priv->profile, "AsPool:load");

	/* load as AsPool also means to reload, so we clear any potential old data */
	as_pool_clear (pool);

	/* lock for writing */
	locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	/* apply settings */
	as_cache_set_prefer_os_metainfo (priv->cache,
					 as_flags_contains (priv->flags, AS_POOL_FLAG_PREFER_OS_METAINFO));

	/* prune any ancient data from the cache that has not been used for a long time */
	as_cache_prune_data (priv->cache);

	/* find common locations that have metadata */
	as_pool_detect_std_metadata_dirs (pool, include_user_data);

	if (caches_updated != NULL)
		*caches_updated = FALSE;

	/* process data from all the individual metadata silos in known locations */
	g_hash_table_iter_init (&loc_iter, priv->std_data_locations);
	while (g_hash_table_iter_next (&loc_iter, NULL, &loc_value)) {
		ret = as_pool_loader_process_group (pool,
						    loc_value,
						    force_cache_refresh,
						    caches_updated,
						    error);
		/* cache writing errors or other fatal stuff will cause us to stop loading anything */
		if (!ret)
			return FALSE;

	}

	/* process data in user-defined locations */
	g_hash_table_iter_init (&loc_iter, priv->extra_data_locations);
	while (g_hash_table_iter_next (&loc_iter, NULL, &loc_value)) {
		ret = as_pool_loader_process_group (pool,
						    loc_value,
						    force_cache_refresh,
						    caches_updated,
						    error);
		if (!ret)
			return FALSE;

	}

	return ret;
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
	return as_pool_load_internal (pool,
				      TRUE, /* also load user-specific data */
				      FALSE, /* do not force cache refresh */
				      NULL, /* we don't care whether caches were used or not */
				      cancellable,
				      error);
}

/**
 * as_pool_load_thread:
 */
static void
as_pool_load_thread (GTask *task,
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
	g_autoptr(GTask) task = g_task_new (pool,
					    cancellable,
					    callback,
					    user_data);
	g_task_run_in_thread (task, as_pool_load_thread);
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
 * as_pool_section_reload_thread:
 */
static void
as_pool_section_reload_thread (GTask *task,
		   gpointer source_object,
		   gpointer task_data,
		   GCancellable *cancellable)
{
	AsPool *pool = AS_POOL (source_object);
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	AsLocationGroup *lgroup = task_data;
	g_autoptr(GError) error = NULL;
	gboolean ret;
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	ret = as_pool_loader_process_group (pool,
					    lgroup,
					    TRUE, /* always refresh cache, don't bother verifying timestamps */
					    NULL,
					    &error);
	if (!ret)
		g_warning ("Failed to auto-reload cache section %s: %s",
			   lgroup->cache_key, error->message);

	g_debug ("Emitting Pool::changed() [%s]", ret? "success" : "failure");
	g_signal_emit (pool, signals[SIGNAL_CHANGED], 0);

	g_task_return_boolean (task, TRUE);
}

/**
 * as_pool_process_pending_reload_cb:
 *
 * Process a background reload for a specific location group.
 */
static gboolean
as_pool_process_pending_reload_cb (gpointer user_data)
{
	AsLocationGroup *lgroup = user_data;
	AsPoolPrivate *priv = GET_PRIVATE (AS_POOL (lgroup->owner));
	g_autoptr(GTask) task = NULL;

	priv->pending_id = 0;
	g_debug ("Auto-reload of cache for %s due to source data changes.", lgroup->cache_key);

	task = g_task_new (lgroup->owner, NULL, NULL, NULL);
	g_task_set_task_data (task, lgroup, NULL);
	g_task_run_in_thread (task, as_pool_section_reload_thread);

	return FALSE;
}

/**
 * as_pool_trigger_reload_pending:
 *
 * Schedule a cache section reload.
 */
static void
as_pool_trigger_reload_pending (AsPool *pool, AsLocationGroup *lgroup, guint timeout_ms)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	if (priv->pending_id)
		g_source_remove (priv->pending_id);
	else
		g_debug ("Reload for %s pending in ~%i ms", lgroup->cache_key, timeout_ms);
	priv->pending_id = g_timeout_add (timeout_ms,
					  as_pool_process_pending_reload_cb,
					  lgroup);
}

/**
 * as_pool_location_group_monitor_changed_cb:
 *
 * Called when any interesting file in the watched group is added, removed
 * or changed.
 */
static void
as_pool_location_group_monitor_changed_cb (AsFileMonitor *monitor,
					   const gchar *filename,
					   AsLocationGroup *lgroup)
{
	as_pool_trigger_reload_pending (lgroup->owner, lgroup, 800);
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
	g_set_error (error,
			AS_POOL_ERROR,
			AS_POOL_ERROR_FAILED,
			"Can not load cache file '%s': Direct cache injection is no longer possible.", fname);
	return FALSE;
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
	g_set_error (error,
			AS_POOL_ERROR,
			AS_POOL_ERROR_FAILED,
			"Can not write cache file '%s': Single-file cache export is no longer possible.", fname);
	return FALSE;
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
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

	ptask = as_profile_start_literal (priv->profile, "AsPool:get_components");

	result = as_cache_get_components_all (priv->cache, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable to retrieve all components from session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
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
	GPtrArray *result;
	g_autoptr(GError) tmp_error = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

	ptask = as_profile_start_literal (priv->profile, "AsPool:get_components_by_id");
	result = as_cache_get_components_by_id (priv->cache, cid, &tmp_error);
	if (result == NULL) {
		g_warning ("Error while trying to get components by ID: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
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
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

	result = as_cache_get_components_by_provided_item (priv->cache, kind, item, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find components by provided item in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
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
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

	result = as_cache_get_components_by_kind (priv->cache, kind, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find components by kind in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
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
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

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
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

	result = as_cache_get_components_by_launchable (priv->cache, kind, id, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find components by launchable in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

	return result;
}

/**
 * as_pool_get_components_by_extends:
 * @pool: An instance of #AsPool.
 * @extended_id: The ID of the component to search extensions for.
 *
 * Find components extending the component with the given ID. They can then be registered to the
 * #AsComponent they extend via %as_component_add_addon.
 * If the %AS_POOL_FLAG_RESOLVE_ADDONS pool flag is set, addons are automatically resolved and
 * this explicit function is not needed, but overall query time will be increased (so only use
 * this flag if you will be resolving addon information later anyway).
 *
 * Returns: (transfer container) (element-type AsComponent): an array of #AsComponent objects.
 *
 * Since: 0.15.0
 */
GPtrArray*
as_pool_get_components_by_extends (AsPool *pool, const gchar *extended_id)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GError) tmp_error = NULL;
	GPtrArray *result = NULL;
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

	result = as_cache_get_components_by_extends (priv->cache, extended_id, &tmp_error);
	if (result == NULL) {
		g_warning ("Unable find addon components in session cache: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
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
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

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

	result = as_cache_search (priv->cache,
				  (const gchar * const *) tokens,
				  TRUE, /* sort */
				  &tmp_error);
	if (result == NULL) {
		g_warning ("Search failed: %s", tmp_error->message);
		return g_ptr_array_new_with_free_func (g_object_unref);
	}

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
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
as_pool_refresh_cache (AsPool *pool, gboolean force, GError **error)
{
	return as_pool_refresh_system_cache (pool, force, NULL, error);
}

/**
 * as_pool_refresh_system_cache:
 * @pool: An instance of #AsPool.
 * @force: Enforce refresh, even if source data has not changed.
 * @caches_updated: Return whether caches were updated or not.
 *
 * Update the AppStream cache. There is normally no need to call this function manually, because cache updates are handled
 * transparently in the background.
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
as_pool_refresh_system_cache (AsPool *pool,
			      gboolean force,
			      gboolean *caches_updated,
			      GError **error)
{
	gboolean ret = FALSE;
	GError *tmp_error = NULL;

	/* Ensure the pool is empty and cache is initialized before refreshing data
	 * We need an initialized cache so the APT support can test for cache age correctly. */
	as_pool_clear (pool);

	/* collect metadata */
#ifdef HAVE_APT_SUPPORT
	/* currently, we only do something here if we are running with explicit APT support compiled in and are root */
	if (as_utils_is_root ()) {
		as_pool_scan_apt (pool, force, &tmp_error);
		if (tmp_error != NULL) {
			/* the exact error is not forwarded here, since we might be able to partially update the cache */
			g_warning ("Error while collecting metadata: %s", tmp_error->message);
			g_clear_error (&tmp_error);
		}
	}
#endif

	/* load AppStream system metadata only and refine it */
	ret = as_pool_load_internal (pool,
				     FALSE, /* no user data, only load system data */
				     force,
				     caches_updated,
				     NULL,
				     &tmp_error);
	if (!ret) {
		g_propagate_prefixed_error (error,
					    tmp_error,
					    "Failed to refresh the metadata cache:");
		return FALSE;
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
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	g_free (priv->locale);
	priv->locale = g_strdup (locale);
	as_cache_set_locale (priv->cache, locale);
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
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);
	return priv->locale;
}

/**
 * as_pool_add_extra_data_location:
 * @pool: An instance of #AsPool.
 * @directory: An existing filesystem location.
 * @format_style: The expected format style of the metadata, e.g. %AS_FORMAT_STYLE_COLLECTION
 *
 * Add an additional non-standard location to the metadata pool where metadata will be read from.
 * If @directory contains a "xml", "xmls", "yaml" or "icons" subdirectory (or all of them),
 * those paths will be added to the search paths instead.
 */
void
as_pool_add_extra_data_location (AsPool *pool, const gchar *directory, AsFormatStyle format_style)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	AsLocationGroup *extra_group;
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	extra_group = as_location_group_new (pool,
					     as_utils_guess_scope_from_path (directory),
					     format_style,
					     FALSE, /* is not OS data */
					     directory);
	g_hash_table_insert (priv->extra_data_locations,
			     g_strdup (extra_group->cache_key),
			     extra_group);
	as_pool_add_catalog_metadata_dir_internal (pool,
						   extra_group,
						   directory,
						   TRUE /* add root */,
						   FALSE /* no legacy support */);
}

/**
 * as_pool_reset_extra_data_locations:
 * @pool: An instance of #AsPool.
 *
 * Remove all explicitly added metadata locations.
 *
 * Since: 0.15.0
 */
void
as_pool_reset_extra_data_locations (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	/* clear arrays */
	g_hash_table_remove_all (priv->extra_data_locations);

	g_debug ("Cleared extra metadata search paths.");
}

/**
 * as_pool_print_location_group_info:
 */
static gboolean
as_pool_print_location_group_info (AsLocationGroup *lgroup)
{
	gboolean data_found = FALSE;
	g_print (" %s: %s\n", _("Group"), lgroup->cache_key);

	for (guint i = 0; i < lgroup->locations->len; i++) {
		AsLocationEntry *lentry = g_ptr_array_index (lgroup->locations, i);
		g_autoptr(GPtrArray) files = NULL;
		const gchar *format_kind_str;

		if (!g_file_test (lentry->location, G_FILE_TEST_IS_DIR))
			continue;

		g_print ("  %s\n", lentry->location);
		if (lentry->format_kind == AS_FORMAT_KIND_XML) {
			files = as_utils_find_files_matching (lentry->location, "*.xml*", FALSE, NULL);
			if (lgroup->format_style == AS_FORMAT_STYLE_METAINFO)
				format_kind_str = "MetaInfo XML";
			else
				format_kind_str = "Collection XML";
		} else if (lentry->format_kind == AS_FORMAT_KIND_YAML) {
			files = as_utils_find_files_matching (lentry->location, "*.yml*", FALSE, NULL);
			format_kind_str = "YAML";
		} else if (lentry->format_kind == AS_FORMAT_KIND_DESKTOP_ENTRY) {
			files = as_utils_find_files_matching (lentry->location, "*.desktop", FALSE, NULL);
			format_kind_str = "Desktop Entry";
		} else {
			g_warning ("Unknown data format type detected: %s", as_format_kind_to_string (lentry->format_kind));
			continue;
		}

		if (files != NULL) {
			g_print ("    • %s:  %i\n", format_kind_str, files->len);
			if (files->len > 0)
				data_found = TRUE;
		}
	}


	for (guint i = 0; i < lgroup->icon_dirs->len; i++) {
		const gchar *icons_path = g_ptr_array_index (lgroup->icon_dirs, i);
		g_autoptr(GPtrArray) icon_dirs = NULL;

		if (!g_file_test (icons_path, G_FILE_TEST_IS_DIR))
			continue;
		g_print ("  %s\n", icons_path);

		icon_dirs = as_utils_find_files_matching (icons_path, "*", FALSE, NULL);
		if (icon_dirs != NULL) {
			g_print ("    • %s:\n", _("Iconsets"));
			for (guint j = 0; j < icon_dirs->len; j++) {
				const gchar *ipath;
				g_autofree gchar *dname = NULL;
				ipath = (const gchar *) g_ptr_array_index (icon_dirs, j);

				dname = g_path_get_basename (ipath);
				g_print ("        %s\n", dname);
			}
		}
	}

	return data_found;
}

/**
 * as_pool_print_std_data_locations_info_private:
 *
 * Internal diagnostic function to print information about the
 * standard data directories to stdout.
 * Used in appstreamcli, but not exposed as public API.
 */
gboolean
as_pool_print_std_data_locations_info_private (AsPool *pool, gboolean print_os_data, gboolean print_extra_data)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	gboolean data_found = FALSE;
	GHashTableIter loc_iter;
	gpointer loc_value;
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	/* find common locations that have metadata */
	if (g_hash_table_size (priv->std_data_locations) == 0)
		as_pool_detect_std_metadata_dirs (pool, TRUE);


	g_hash_table_iter_init (&loc_iter, priv->std_data_locations);
	while (g_hash_table_iter_next (&loc_iter, NULL, &loc_value)) {
		AsLocationGroup *lgroup = loc_value;
		if (!print_os_data && lgroup->is_os_data)
			continue;
		if (!print_extra_data && !lgroup->is_os_data)
			continue;

		if (lgroup->is_os_data) {
			if (lgroup->format_style == AS_FORMAT_STYLE_METAINFO)
				/* TRANSLATORS: Info in appstreamcli status about OS data origin */
				g_print (" %s\n", _("Data from locally installed software"));
			else
				/* TRANSLATORS: Info in appstreamcli status about OS data origin */
				g_print (" %s\n", _("Software catalog data"));
		}

		if (as_pool_print_location_group_info (lgroup))
			data_found = TRUE;
		g_print ("\n");
	}

	return data_found;
}

/**
 * as_pool_add_metadata_location:
 * @pool: An instance of #AsPool.
 * @directory: An existing filesystem location.
 *
 * Add a location for the data pool to read data from.
 * If @directory contains a "xml", "xmls", "yaml" or "icons" subdirectory (or all of them),
 * those paths will be added to the search paths instead.
 *
 * Deprecated: 0.15.0: Use %as_pool_add_extra_data_location instead.
 */
void
as_pool_add_metadata_location (AsPool *pool, const gchar *directory)
{
	as_pool_add_extra_data_location (pool, directory, AS_FORMAT_STYLE_COLLECTION);
}

/**
 * as_pool_clear_metadata_locations:
 * @pool: An instance of #AsPool.
 *
 * Remove all metadata locations from the list of watched locations.
 *
 * Deprecated: 0.15.0: Use %as_pool_reset_extra_data_locations and control system data loading via flags.
 */
void
as_pool_clear_metadata_locations (AsPool *pool)
{
	/* don't load stuff from default locations to mimic previous behavior */
	as_pool_set_load_std_data_locations (pool, FALSE);

	/* clear arrays */
	as_pool_reset_extra_data_locations (pool);

	g_debug ("Cleared all metadata search paths.");
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
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);
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
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	priv->flags = flags;
	as_cache_set_resolve_addons (priv->cache,
				     as_flags_contains (priv->flags, AS_POOL_FLAG_RESOLVE_ADDONS));
}

/**
 * as_pool_add_flags:
 * @pool: An instance of #AsPool.
 * @flags: The #AsPoolFlags to add.
 *
 * Convenience function to add one or multiple #AsPoolFlags to
 * the flag set of this data pool.
 *
 * Since: 0.15.0
 */
void
as_pool_add_flags (AsPool *pool, AsPoolFlags flags)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	as_flags_add (priv->flags, flags);
	as_cache_set_resolve_addons (priv->cache,
				     as_flags_contains (priv->flags, AS_POOL_FLAG_RESOLVE_ADDONS));
}

/**
 * as_pool_remove_flags:
 * @pool: An instance of #AsPool.
 * @flags: The #AsPoolFlags to remove.
 *
 * Convenience function to remove one or multiple #AsPoolFlags from
 * the flag set of this data pool.
 *
 * Since: 0.15.0
 */
void
as_pool_remove_flags (AsPool *pool, AsPoolFlags flags)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	as_flags_remove (priv->flags, flags);
	as_cache_set_resolve_addons (priv->cache,
				     as_flags_contains (priv->flags, AS_POOL_FLAG_RESOLVE_ADDONS));
}

/**
 * as_pool_set_load_std_data_locations:
 * @pool: An instance of #AsPool.
 * @enabled: Whether loading of data from standard locations should be enabled.
 *
 * This is a convenience function that enables or disables loading of metadata
 * from well-known standard locations by configuring the #AsPoolFlags of this
 * #AsPool accordingly.
 * Data affected by this includes the OS data catalog, metainfo, desktop-entry
 * files and Flatpak data.
 * If you need more fine-grained control, set the #AsPoolFlags explicitly.
 *
 * Since: 0.15.0
 */
void
as_pool_set_load_std_data_locations (AsPool *pool, gboolean enabled)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	if (enabled) {
		as_flags_add (priv->flags, AS_POOL_FLAG_LOAD_OS_COLLECTION);
		as_flags_add (priv->flags, AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES);
		as_flags_add (priv->flags, AS_POOL_FLAG_LOAD_OS_METAINFO);
		as_flags_add (priv->flags, AS_POOL_FLAG_LOAD_FLATPAK);
	} else {
		as_flags_remove (priv->flags, AS_POOL_FLAG_LOAD_OS_COLLECTION);
		as_flags_remove (priv->flags, AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES);
		as_flags_remove (priv->flags, AS_POOL_FLAG_LOAD_OS_METAINFO);
		as_flags_remove (priv->flags, AS_POOL_FLAG_LOAD_FLATPAK);
	}
}

/**
 * as_pool_get_os_metadata_cache_age:
 * @pool: An instance of #AsPool.
 *
 * Get the age of the system cache for OS collection data.
 */
time_t
as_pool_get_os_metadata_cache_age (AsPool *pool)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);
	return as_cache_get_ctime (priv->cache,
				   AS_COMPONENT_SCOPE_SYSTEM,
				   OS_COLLECTION_CACHE_KEY,
				   NULL);
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
 *
 * Deprecated: 0.15.0: Cache location can no longer be set explicitly.
 **/
void
as_pool_set_cache_location (AsPool *pool, const gchar *fname)
{
	g_warning ("Not changing AppStream cache location: No longer supported.");
}

/**
 * as_pool_get_cache_flags:
 * @pool: An instance of #AsPool.
 *
 * Get the #AsCacheFlags for this data pool.
 *
 * Deprecated: 0.15.0: Cache flags can no longer be changed.
 */
AsCacheFlags
as_pool_get_cache_flags (AsPool *pool)
{
	return AS_CACHE_FLAG_USE_SYSTEM | AS_CACHE_FLAG_USE_USER | AS_CACHE_FLAG_REFRESH_SYSTEM;
}

/**
 * as_pool_set_cache_flags:
 * @pool: An instance of #AsPool.
 * @flags: The new #AsCacheFlags.
 *
 * Set the #AsCacheFlags for this data pool.
 *
 * Deprecated: 0.15.0: Cache flags can no longer be modified.
 */
void
as_pool_set_cache_flags (AsPool *pool, AsCacheFlags flags)
{
	/* Legacy function that is just providing some compatibility glue */
	if (as_flags_contains (flags, AS_CACHE_FLAG_USE_USER))
		as_pool_remove_flags (pool, AS_POOL_FLAG_LOAD_OS_COLLECTION |
					    AS_POOL_FLAG_LOAD_OS_METAINFO);
}

/**
 * as_pool_get_cache_location:
 * @pool: An instance of #AsPool.
 *
 * Gets the location of the session cache.
 *
 * Returns: Location of the cache, or %NULL if unknown.
 *
 * Deprecated: 0.15.0: Cache location can no longer be set explicitly.
 **/
const gchar*
as_pool_get_cache_location (AsPool *pool)
{
	/* No-op */
	return NULL;
}

/**
 * as_pool_override_cache_location:
 * @pool: An instance of #AsPool.
 * @dir_sys: Directory to store system/non-writable cache
 * @dir_user: Directory to store writable/user cache.
 *
 * Override the automatic cache location placement.
 * This is useful primarily for debugging purposes and unit tests.
 **/
void
as_pool_override_cache_locations (AsPool *pool, const gchar *dir_sys, const gchar *dir_user)
{
	AsPoolPrivate *priv = GET_PRIVATE (pool);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);
	if (dir_sys == NULL)
		as_cache_set_locations (priv->cache, dir_user, dir_user);
	else if (dir_user == NULL)
		as_cache_set_locations (priv->cache, dir_sys, dir_sys);
	else
		as_cache_set_locations (priv->cache, dir_sys, dir_user);
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
