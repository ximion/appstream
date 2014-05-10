/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include "as-database-write.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "xapian/database-cwrap.hpp"
#include "as-settings-private.h"
#include "as-utils.h"

struct _AsDatabaseWritePrivate {
	struct XADatabaseWrite* db_w;
};

static gpointer as_database_write_parent_class = NULL;


#define AS_DATABASE_WRITE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_DATABASE_WRITE, AsDatabaseWritePrivate))

static gboolean as_database_write_real_open (AsDatabase* base);
static void as_database_write_finalize (GObject* obj);

AsDatabaseWrite*
as_database_write_construct (GType object_type)
{
	AsDatabaseWrite * self = NULL;
	self = (AsDatabaseWrite*) as_database_construct (object_type);
	self->priv->db_w = xa_database_write_new ();
	/* ensure db directory exists */
	as_utils_touch_dir (AS_APPSTREAM_DATABASE_PATH);
	return self;
}


AsDatabaseWrite*
as_database_write_new (void)
{
	return as_database_write_construct (AS_TYPE_DATABASE_WRITE);
}


static gboolean
as_database_write_real_open (AsDatabase* base)
{
	AsDatabaseWrite * self;
	gboolean ret = FALSE;
	const gchar *dbpath;
	self = (AsDatabaseWrite*) base;
	dbpath = as_database_get_database_path ((AsDatabase*) self);
	ret = xa_database_write_initialize (self->priv->db_w, dbpath);
	if (!ret)
		return FALSE;
	ret = AS_DATABASE_CLASS (as_database_write_parent_class)->open (G_TYPE_CHECK_INSTANCE_CAST (self, AS_TYPE_DATABASE, AsDatabase));

	return ret;
}


gboolean
as_database_write_rebuild (AsDatabaseWrite* self, GList* cpt_list)
{
	gboolean ret = FALSE;
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (cpt_list != NULL, FALSE);

	ret = xa_database_write_rebuild (self->priv->db_w, cpt_list);
	return ret;
}


static void
as_database_write_class_init (AsDatabaseWriteClass * klass)
{
	as_database_write_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsDatabaseWritePrivate));
	AS_DATABASE_CLASS (klass)->open = as_database_write_real_open;
	G_OBJECT_CLASS (klass)->finalize = as_database_write_finalize;
}


static void
as_database_write_instance_init (AsDatabaseWrite * self)
{
	self->priv = AS_DATABASE_WRITE_GET_PRIVATE (self);
}


static void
as_database_write_finalize (GObject* obj)
{
	AsDatabaseWrite * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_DATABASE_WRITE, AsDatabaseWrite);
	xa_database_write_free (self->priv->db_w);
	G_OBJECT_CLASS (as_database_write_parent_class)->finalize (obj);
}


/**
 * as_database_write_get_type:
 *
 * Internal class to allow helper applications
 * to modify the AppStream application database
 */
GType
as_database_write_get_type (void)
{
	static volatile gsize as_database_write_type_id__volatile = 0;
	if (g_once_init_enter (&as_database_write_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
				sizeof (AsDatabaseWriteClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) as_database_write_class_init,
				(GClassFinalizeFunc) NULL,
				NULL,
				sizeof (AsDatabaseWrite),
				0,
				(GInstanceInitFunc) as_database_write_instance_init,
				NULL
		};
		GType as_database_write_type_id;
		as_database_write_type_id = g_type_register_static (AS_TYPE_DATABASE, "AsDatabaseWrite", &g_define_type_info, 0);
		g_once_init_leave (&as_database_write_type_id__volatile, as_database_write_type_id);
	}
	return as_database_write_type_id__volatile;
}
