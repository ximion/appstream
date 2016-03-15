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

#include "as-cache-builder.h"

#include <config.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "xapian/database-cwrap.hpp"
#include "as-utils.h"
#include "as-utils-private.h"
#include "as-data-pool.h"
#include "as-settings-private.h"

typedef struct
{
	struct XADatabaseWrite* db_w;
	gchar* db_path;
	gchar* cache_path;
	time_t cache_ctime;
	AsDataPool *dpool;
} AsCacheBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsCacheBuilder, as_cache_builder, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_cache_builder_get_instance_private (o))

/**
 * as_cache_builder_error_quark:
 *
 * Return value: An error quark.
 **/
G_DEFINE_QUARK (as-cache-builder-error-quark, as_cache_builder_error)

/**
 * as_cache_builder_init:
 **/
static void
as_cache_builder_init (AsCacheBuilder *builder)
{
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

	priv->db_w = xa_database_write_new ();
	priv->dpool = as_data_pool_new ();
	priv->cache_path = NULL;
	priv->cache_ctime = 0;
}

/**
 * as_cache_builder_finalize:
 */
static void
as_cache_builder_finalize (GObject *object)
{
	AsCacheBuilder *builder = AS_CACHE_BUILDER (object);
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

	xa_database_write_free (priv->db_w);
	g_object_unref (priv->dpool);
	g_free (priv->cache_path);
	g_free (priv->db_path);

	G_OBJECT_CLASS (as_cache_builder_parent_class)->finalize (object);
}

/**
 * as_cache_builder_class_init:
 **/
static void
as_cache_builder_class_init (AsCacheBuilderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_cache_builder_finalize;
}

/**
 * as_cache_builder_check_cache_ctime:
 * @builder: An instance of #AsCacheBuilder
 *
 * Update the cached cache-ctime. We need to cache it prior to potentially
 * creating a new database, so we will always rebuild the database in case
 * none existed previously.
 */
static void
as_cache_builder_check_cache_ctime (AsCacheBuilder *builder)
{
	struct stat cache_sbuf;
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

	if (stat (priv->db_path, &cache_sbuf) < 0)
		priv->cache_ctime = 0;
	else
		priv->cache_ctime = cache_sbuf.st_ctime;
}

/**
 * as_cache_builder_setup:
 * @builder: An instance of #AsCacheBuilder.
 * @dbpath: (nullable) (default NULL): Path to the database directory, or %NULL to use default.
 *
 * Initialize the cache builder.
 */
gboolean
as_cache_builder_setup (AsCacheBuilder *builder, const gchar *dbpath, GError **error)
{
	gboolean ret;
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

	/* update database path if necessary */
	g_free (priv->cache_path);
	if (as_str_empty (dbpath)) {
		priv->cache_path = g_strdup (AS_APPSTREAM_CACHE_PATH);
	} else {
		priv->cache_path = g_strdup (dbpath);
	}
	priv->db_path = g_build_filename (priv->cache_path, "xapian", "default", NULL);

	/* users umask shouldn't interfere with us creating new files */
	as_reset_umask ();

	/* check the ctime of the cache directory, if it exists at all */
	as_cache_builder_check_cache_ctime (builder);

	/* try to create db directory, in case it doesn't exist */
	g_mkdir_with_parents (priv->db_path, 0755);

	if (!as_utils_is_writable (priv->db_path)) {
		g_set_error (error,
				AS_CACHE_BUILDER_ERROR,
				AS_CACHE_BUILDER_ERROR_TARGET_NOT_WRITABLE,
				_("Cache location '%s' is not writable."), priv->db_path);
		return FALSE;
	}

	ret = xa_database_write_initialize (priv->db_w, priv->db_path);
	return ret;
}

/**
 * as_cache_builder_ctime_newer:
 *
 * Returns: %TRUE if ctime of file is newer than the cached time.
 */
static gboolean
as_cache_builder_ctime_newer (AsCacheBuilder *builder, const gchar *dir)
{
	struct stat sb;
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

	if (stat (dir, &sb) < 0)
		return FALSE;

	if (sb.st_ctime > priv->cache_ctime)
		return TRUE;

	return FALSE;
}

/**
 * as_cache_builder_appstream_data_changed:
 */
static gboolean
as_cache_builder_appstream_data_changed (AsCacheBuilder *builder)
{
	guint i;
	GPtrArray *locations;
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

	locations = as_data_pool_get_metadata_locations (priv->dpool);
	for (i = 0; i < locations->len; i++) {
		g_autofree gchar *xml_dir = NULL;
		g_autofree gchar *yaml_dir = NULL;
		const gchar *dir_root = (const gchar*) g_ptr_array_index (locations, i);

		if (as_cache_builder_ctime_newer (builder, dir_root))
			return TRUE;

		xml_dir = g_build_filename (dir_root, "xmls", NULL);
		if (as_cache_builder_ctime_newer (builder, xml_dir))
			return TRUE;

		yaml_dir = g_build_filename (dir_root, "yaml", NULL);
		if (as_cache_builder_ctime_newer (builder, yaml_dir))
			return TRUE;
	}

	return FALSE;
}

#ifdef APT_SUPPORT

#define YAML_SEPARATOR "---"
/* Compilers will optimise this to a constant */
#define YAML_SEPARATOR_LEN strlen(YAML_SEPARATOR)

/**
 * as_cache_builder_get_yml_data_origin:
 */
gchar*
as_cache_builder_get_yml_data_origin (const gchar *fname)
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
		break;
	}

	return origin;
}

/**
 * as_cache_builder_extract_icons:
 */
static void
as_cache_builder_extract_icons (const gchar *asicons_target, const gchar *origin, const gchar *apt_basename, const gchar *apt_lists_dir, const gchar *icons_size)
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
 * as_cache_builder_scan:
 *
 * Scan for additional metadata in 3rd-party directories and move it to the right place.
 */
static void
as_cache_builder_scan_apt (AsCacheBuilder *builder, gboolean force, GError **error)
{
	const gchar *apt_lists_dir = "/var/lib/apt/lists/";
	const gchar *appstream_yml_target = "/var/lib/app-info/yaml";
	const gchar *appstream_icons_target = "/var/lib/app-info/icons";
	g_autoptr(GPtrArray) yml_files = NULL;
	g_autoptr(GError) tmp_error = NULL;
	gboolean data_changed = FALSE;
	gboolean metadata_target_empty = FALSE;
	guint i;
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

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
	} else {
		/* we would actually need to check whether there is metadata in that directory at all,
		 * and if the new metadata from APT is already included. We don't do that for
		 * performance reasons.
		 * The reason why we do this check at all is APT putting files with the *server* ctime/mtime
		 * into it's lists directory, and that time might be lower than the time the metadata cache
		 * was last updated, which results in no cache update at all.
		 */
		metadata_target_empty = TRUE;
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
	} else {
		if (metadata_target_empty)
			data_changed = TRUE;
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
		origin = as_cache_builder_get_yml_data_origin (dest_fname);
		if (origin == NULL) {
			g_warning ("No origin found for file %s", fbasename);
			continue;
		}

		/* get base prefix for this file in the APT download cache */
		file_baseprefix = g_strndup (fbasename, strlen (fbasename) - strlen (g_strrstr (fbasename, "_") + 1));

		/* extract icons to their destination (if they exist at all */
		as_cache_builder_extract_icons (appstream_icons_target,
						origin,
						file_baseprefix,
						apt_lists_dir,
						"64x64");
		as_cache_builder_extract_icons (appstream_icons_target,
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
 * as_cache_builder_refresh:
 * @builder: An instance of #AsCacheBuilder.
 * @force: Enforce refresh, even if source data has not changed.
 *
 * Update the AppStream Xapian cache.
 *
 * Returns: %TRUE if the cache was updated, %FALSE on error or if the cache update was not necessary and has been skipped.
 */
gboolean
as_cache_builder_refresh (AsCacheBuilder *builder, gboolean force, GError **error)
{
	gboolean ret = FALSE;
	gboolean ret_poolupdate;
	GList *cpt_list;
	g_autoptr(GError) tmp_error = NULL;
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

	/* collect metadata */
#ifdef APT_SUPPORT
	/* currently, we only do something here if we are running with explicit APT support compiled in */
	as_cache_builder_scan_apt (builder, force, &tmp_error);
	if (tmp_error != NULL) {
		/* the exact error is not forwarded here, since we might be able to partially update the cache */
		g_warning ("Error while collecting metadata: %s", tmp_error->message);
		g_error_free (tmp_error);
		tmp_error = NULL;
	}
#endif

	/* check if we need to refresh the cache
	 * (which is only necessary if the AppStream data has changed) */
	if (!as_cache_builder_appstream_data_changed (builder)) {
		g_debug ("Data did not change, no cache refresh needed.");
		if (force) {
			g_debug ("Forcing refresh anyway.");
		} else {
			return FALSE;
		}
	}
	g_debug ("Refreshing AppStream cache");

	/* find them wherever they are */
	ret_poolupdate = as_data_pool_update (priv->dpool, &tmp_error);
	if (tmp_error != NULL) {
		/* the exact error is not forwarded here, since we might be able to partially update the cache */
		g_warning ("Error while updating the in-memory data pool: %s", tmp_error->message);
		g_error_free (tmp_error);
		tmp_error = NULL;
	}

	/* populate the cache */
	cpt_list = as_data_pool_get_components (priv->dpool);
	ret = xa_database_write_rebuild (priv->db_w, cpt_list);
	g_list_free (cpt_list);

	if (ret) {
		if (!ret_poolupdate) {
			g_set_error (error,
				AS_CACHE_BUILDER_ERROR,
				AS_CACHE_BUILDER_ERROR_CACHE_INCOMPLETE,
				_("AppStream cache update completed, but some metadata was ignored due to errors."));
		}
		/* update the cache mtime, to not needlessly rebuild it again */
		as_touch_location (priv->db_path);
		as_cache_builder_check_cache_ctime (builder);
	} else {
		g_set_error (error,
				AS_CACHE_BUILDER_ERROR,
				AS_CACHE_BUILDER_ERROR_FAILED,
				_("AppStream cache update failed."));
	}

	return TRUE;
}

/**
 * as_cache_builder_set_data_source_directories:
 * @builder: a valid #AsCacheBuilder instance
 * @dirs: (array zero-terminated=1): a zero-terminated array of data input directories.
 *
 * Set locations for the database builder to pull it's data from.
 * This is mainly used for testing purposes. Each location should have an
 * "xmls" and/or "yaml" subdirectory with the actual data as (compressed)
 * AppStream XML or DEP-11 YAML in it.
 */
void
as_cache_builder_set_data_source_directories (AsCacheBuilder *builder, gchar **dirs)
{
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);
	as_data_pool_set_metadata_locations (priv->dpool, dirs);
}

/**
 * as_cache_builder_new:
 *
 * Creates a new #AsCacheBuilder.
 *
 * Returns: (transfer full): a new #AsCacheBuilder
 **/
AsCacheBuilder*
as_cache_builder_new (void)
{
	AsCacheBuilder *builder;
	builder = g_object_new (AS_TYPE_CACHE_BUILDER, NULL);
	return AS_CACHE_BUILDER (builder);
}
