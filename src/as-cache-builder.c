/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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
	AsDataPool *dpool;
} AsCacheBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsCacheBuilder, as_cache_builder, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_cache_builder_get_instance_private (o))

/**
 * as_cache_builder_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_cache_builder_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsCacheBuilderError");
	return quark;
}

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
 * as_cache_builder_setup:
 * @builder: An instance of #AsCacheBuilder.
 * @dbpath: (nullable) (default NULL): Path to the database directory, or %NULL to use defaulr.
 *
 * Initialize the cache builder.
 */
gboolean
as_cache_builder_setup (AsCacheBuilder *builder, const gchar *dbpath)
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

	/* try to create db directory, in case it doesn't exist */
	g_mkdir_with_parents (priv->db_path, 0755);

	ret = xa_database_write_initialize (priv->db_w, priv->db_path);
	return ret;
}

/**
 * as_cache_builder_appstream_data_changed:
 */
static gboolean
as_cache_builder_appstream_data_changed (AsCacheBuilder *builder)
{
	gchar *fname;
	GFile *file;
	GPtrArray *watchfile_old;
	gchar *watchfile_new = NULL;
	gchar **files;
	guint i;
	gboolean ret = FALSE;
	GDataOutputStream *dos = NULL;
	GFileOutputStream *fos;
	GError *error = NULL;
	AsCacheBuilderPrivate *priv = GET_PRIVATE (builder);

	fname = g_build_filename (priv->cache_path, "cache.watch", NULL, NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);

	watchfile_old = g_ptr_array_new_with_free_func (g_free);
	if (g_file_query_exists (file, NULL)) {
		GDataInputStream* dis;
		GFileInputStream* fis;
		GError *e = NULL;
		gchar* line;

		fis = g_file_read (file, NULL, &e);
		dis = g_data_input_stream_new ((GInputStream*) fis);
		g_object_unref (fis);

		if (e != NULL) {
			ret = TRUE;
			g_error_free (e);
			goto out;
		}

		while ((line = g_data_input_stream_read_line (dis, NULL, NULL, NULL)) != NULL) {
			g_ptr_array_add (watchfile_old, line);
		}
		g_object_unref (dis);
	} else {
		/* no watch-file: data might have changed, so we rebuild the cache */
		ret = TRUE;
	}

	watchfile_new = g_strdup ("");
	files = as_data_pool_get_watched_locations (priv->dpool);
	for (i = 0; files[i] != NULL; i++) {
		struct stat sbuf;
		gchar *ctime_str;
		gchar *tmp;
		guint j;
		gchar *wentry;

		fname = files[i];
		if (stat (fname, &sbuf) == -1)
			continue;

		ctime_str = g_strdup_printf ("%ld", (glong) sbuf.st_ctime);
		tmp = g_strdup_printf ("%s%s %s\n", watchfile_new, fname, ctime_str);
		g_free (watchfile_new);
		watchfile_new = tmp;

		/* no need to perform test for a up-to-date data from the old watch file if it is empty */
		if (watchfile_old->len == 0)
			continue;

		for (j = 0; j < watchfile_old->len; j++) {
			gchar **wparts;
			wentry = (gchar*) g_ptr_array_index (watchfile_old, j);

			if (g_str_has_prefix (wentry, fname)) {
				wparts = g_strsplit (wentry, " ", 2);
				if (g_strcmp0 (wparts[1], ctime_str) != 0)
					ret = TRUE;
				g_strfreev (wparts);
				break;
			}
		}
		g_free (ctime_str);
	}
	g_strfreev (files);

	/* write our (new) watchfile */
	if (g_file_query_exists (file, NULL))
		g_file_delete (file, NULL, &error);
	if (error != NULL) {
		ret = TRUE;
		g_error_free (error);
		goto out;
	}

	fos = g_file_create (file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error);
	dos = g_data_output_stream_new ((GOutputStream*) fos);
	g_object_unref (fos);
	if (error != NULL) {
		ret = TRUE;
		g_error_free (error);
		goto out;
	}

	g_data_output_stream_put_string (dos, watchfile_new, NULL, &error);
	if (error != NULL) {
		ret = TRUE;
		g_error_free (error);
		goto out;
	}


out:
	g_ptr_array_unref (watchfile_old);
	g_object_unref (file);
	if (watchfile_new != NULL)
		g_free (watchfile_new);
	if (dos != NULL)
		g_object_unref (dos);

	return ret;
}

#ifdef APT_SUPPORT

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
	gchar *tmp;
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
	tmp = g_strstr_len (data, 400, "---");
	if (tmp == NULL)
		return NULL;
	str = g_strndup (tmp+3, strlen(tmp) - strlen (g_strstr_len (tmp+3, -1, "---")) - 3);

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
as_cache_builder_extract_icons (const gchar *origin, const gchar *apt_basename, const gchar *apt_lists_dir, const gchar *icons_size)
{
	const gchar *appstream_icons_target = "/var/lib/app-info/icons";
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

	target_dir = g_build_filename (appstream_icons_target, origin, icons_size, NULL);
	g_mkdir_with_parents (target_dir, 0755);

	cmd = g_strdup_printf ("/bin/tar -xzf '%s' -C '%s'", icons_tarball, target_dir);
	g_spawn_command_line_sync (cmd, NULL, &stderr_txt, &res, &tmp_error);
	if (tmp_error != NULL) {
		g_debug ("Failed to run tar: %s", tmp_error->message);
	}
	if (res != 0) {
		g_debug ("Running tar failed with exist code %i: %s", res, stderr_txt);
	}
}

/**
 * as_cache_builder_scan:
 *
 * Scan for additional metadata in 3rd-party directories and move it to the right place.
 */
static void
as_cache_builder_scan (AsCacheBuilder *builder, GError **error)
{
	const gchar *apt_lists_dir = "/var/lib/apt/lists/";
	const gchar *appstream_yml_target = "/var/lib/app-info/yaml";
	g_autoptr(GPtrArray) yml_files = NULL;
	g_autoptr(GError) tmp_error = NULL;
	guint i;

	/* we can't do anything here if we're not root */
	if (!as_utils_is_root ())
		return;

	if (!g_file_test (apt_lists_dir, G_FILE_TEST_IS_DIR)) {
		/* directory not found, nothing to do here */
		return;
	}

	yml_files = as_utils_find_files_matching (appstream_yml_target, "*", FALSE, &tmp_error);
	if (tmp_error != NULL) {
		g_warning ("Could not scan for broken symlinks in DEP-11 target: %s", tmp_error->message);
		return;
	}
	for (i = 0; i < yml_files->len; i++) {
		const gchar *fname = (const gchar*) g_ptr_array_index (yml_files, i);
		if (!g_file_test (fname, G_FILE_TEST_EXISTS)) {
			g_remove (fname);
		}
	}

	g_ptr_array_unref (yml_files);
	yml_files = as_utils_find_files_matching (apt_lists_dir, "*Components-*.yml.gz", FALSE, &tmp_error);
	if (tmp_error != NULL) {
		g_warning ("Could not scan for APT-downloaded DEP-11 files: %s", tmp_error->message);
		return;
	}

	if (yml_files->len > 0)
		g_mkdir_with_parents (appstream_yml_target, 0755);

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
		as_cache_builder_extract_icons (origin, file_baseprefix, apt_lists_dir, "64x64");
		as_cache_builder_extract_icons (origin, file_baseprefix, apt_lists_dir, "128x128");
	}
}
#endif

/**
 * as_cache_builder_refresh:
 * @builder: An instance of #AsCacheBuilder.
 * @force: Enforce refresh, even if source data has not changed.
 *
 * Update the AppStream Xapian cache
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
	if (force) {
		/* at time, we can't detect if the data has changed and rebuild the cache on-demand, so we simply only scan for new data if the force
		 * parameter is also given, and we will rebuild anyway. */
		as_cache_builder_scan (builder, &tmp_error);
		if (tmp_error != NULL) {
			/* the exact error is not forwarded here, since we might be able to partially update the cache */
			g_warning ("Error while collecting metadata: %s", tmp_error->message);
			g_error_free (tmp_error);
			tmp_error = NULL;
		}
	}
#endif

	/* check if we need to refresh the cache
	 * (which is only necessary if the AppStream data has changed) */
	if (!as_cache_builder_appstream_data_changed (builder)) {
		g_debug ("Data did not change, no cache refresh needed.");
		if (force) {
			g_debug ("Forcing refresh anyway.");
		} else {
			ret = TRUE;
			return ret;
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
		if (!ret_poolupdate)
			g_set_error (error,
				AS_CACHE_BUILDER_ERROR,
				AS_CACHE_BUILDER_ERROR_PARTIALLY_FAILED,
				_("AppStream cache update completed, but some metadata was ignored due to errors."));
	} else {
		g_set_error (error,
				AS_CACHE_BUILDER_ERROR,
				AS_CACHE_BUILDER_ERROR_FAILED,
				_("AppStream cache update failed."));
	}

	return ret;
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
	as_data_pool_set_data_source_directories (priv->dpool, dirs);
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
