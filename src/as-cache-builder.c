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

#include "as-cache-builder.h"

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <sys/stat.h>

#include "xapian/database-cwrap.hpp"
#include "as-utils.h"
#include "as-utils-private.h"
#include "as-data-pool.h"
#include "as-settings-private.h"

struct _AsBuilderPrivate
{
	struct XADatabaseWrite* db_w;
	gchar* db_path;
	gchar* cache_path;
	AsDataPool *dpool;
};

static gpointer as_builder_parent_class = NULL;

#define AS_BUILDER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_BUILDER, AsBuilderPrivate))

static void as_builder_finalize (GObject* obj);

/**
 * as_builder_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_builder_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsBuilderError");
	return quark;
}

static AsBuilder*
as_builder_construct (GType object_type, const gchar *dbpath)
{
	AsBuilder *self = NULL;
	AsBuilderPrivate *priv;

	self = (AsBuilder*) g_object_new (object_type, NULL);
	priv = self->priv;
	priv->db_w = xa_database_write_new ();
	priv->dpool = as_data_pool_new ();

	/* update database path if necessary */
	if (as_str_empty (dbpath)) {
		priv->cache_path = g_strdup (AS_APPSTREAM_CACHE_PATH);
	} else {
		priv->cache_path = g_strdup (dbpath);
	}
	priv->db_path = g_build_filename (priv->cache_path, "xapian", "C", NULL);

	/* ensure db directory exists */
	as_utils_touch_dir (priv->db_path);

	return self;
}

/**
 * as_builder_new:
 *
 * Creates a new #AsBuilder.
 *
 * Returns: (transfer full): an #AsBuilder
 **/
AsBuilder*
as_builder_new (void)
{
	return as_builder_construct (AS_TYPE_BUILDER, NULL);
}

/**
 * as_builder_new_path:
 *
 * Creates a new #AsBuilder with custom database path.
 *
 * @path path to the new Xapian database
 *
 * Returns: (transfer full): an #AsBuilder
 **/
AsBuilder*
as_builder_new_path (const gchar* dbpath)
{
	return as_builder_construct (AS_TYPE_BUILDER, dbpath);
}

/**
 * as_builder_initialize:
 */
gboolean
as_builder_initialize (AsBuilder* self)
{
	gboolean ret;
	AsBuilderPrivate *priv = self->priv;

	as_data_pool_initialize (self->priv->dpool);

	as_utils_touch_dir (self->priv->db_path);

	ret = xa_database_write_initialize (priv->db_w, priv->db_path);
	return ret;
}

static gboolean
as_builder_appstream_data_changed (AsBuilder* self)
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
	g_return_val_if_fail (self != NULL, FALSE);

	fname = g_build_filename (self->priv->cache_path, "cache.watch", NULL, NULL);
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
	files = as_data_pool_get_watched_locations (self->priv->dpool);
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

/**
 * as_builder_refresh_cache:
 *
 * Update the AppStream Xapian cache
 */
gboolean
as_builder_refresh_cache (AsBuilder* self, gboolean force, GError **error)
{
	gboolean ret = FALSE;
	gboolean ret_poolupdate;
	GList *cpt_list;
	g_return_val_if_fail (self != NULL, FALSE);

	/* check if we need to refresh the cache */
	/* (which is only necessary if the AppStream data has changed) */
	if (!as_builder_appstream_data_changed (self)) {
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
	ret_poolupdate = as_data_pool_update (self->priv->dpool);

	/* populate the cache */
	cpt_list = as_data_pool_get_components (self->priv->dpool);
	ret = xa_database_write_rebuild (self->priv->db_w, cpt_list);
	g_list_free (cpt_list);

	if (ret) {
		if (!ret_poolupdate)
			g_set_error (error,
				AS_BUILDER_ERROR,
				AS_BUILDER_ERROR_PARTIALLY_FAILED,
				_("AppStream cache update completed, but some metadata was ignored due to errors."));
	} else {
		g_set_error (error,
				AS_BUILDER_ERROR,
				AS_BUILDER_ERROR_FAILED,
				_("AppStream cache update failed."));
	}

	return ret;
}

/**
 * as_builder_set_data_source_directories:
 * @self: a valid #AsBuilder instance
 * @dirs: (array zero-terminated=1): a zero-terminated array of data input directories.
 *
 * Set locations for the database builder to pull it's data from.
 * This is mainly used for testing purposes. Each location should have an
 * "xmls" and/or "yaml" subdirectory with the actual data as (compressed)
 * AppStream XML or DEP-11 YAML in it.
 */
void
as_builder_set_data_source_directories (AsBuilder *self, gchar **dirs)
{
	as_data_pool_set_data_source_directories (self->priv->dpool, dirs);
}

static void
as_builder_class_init (AsBuilderClass * klass)
{
	as_builder_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsBuilderPrivate));
	G_OBJECT_CLASS (klass)->finalize = as_builder_finalize;
}


static void
as_builder_instance_init (AsBuilder * self)
{
	self->priv = AS_BUILDER_GET_PRIVATE (self);
}


static void
as_builder_finalize (GObject* obj)
{
	AsBuilder * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_BUILDER, AsBuilder);

	xa_database_write_free (self->priv->db_w);
	g_object_unref (self->priv->dpool);
	g_free (self->priv->cache_path);
	g_free (self->priv->db_path);

	G_OBJECT_CLASS (as_builder_parent_class)->finalize (obj);
}


GType as_builder_get_type (void) {
	static volatile gsize as_builder_type_id__volatile = 0;
	if (g_once_init_enter (&as_builder_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = { sizeof (AsBuilderClass),
									(GBaseInitFunc) NULL,
									(GBaseFinalizeFunc) NULL,
									(GClassInitFunc) as_builder_class_init,
									(GClassFinalizeFunc) NULL,
									NULL,
									sizeof (AsBuilder),
									0,
									(GInstanceInitFunc) as_builder_instance_init,
									NULL
		};
		GType as_builder_type_id;
		as_builder_type_id = g_type_register_static (G_TYPE_OBJECT, "AsBuilder", &g_define_type_info, 0);
		g_once_init_leave (&as_builder_type_id__volatile, as_builder_type_id);
	}
	return as_builder_type_id__volatile;
}
