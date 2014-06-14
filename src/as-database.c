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

#include "as-database.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <database-cwrap.hpp>
#include <glib/gstdio.h>

#include "as-settings-private.h"

/**
 * SECTION:as-database
 * @short_description: Read-only access to the Appstream component database
 * @include: appstream.h
 *
 * This object provides access to the Appstream Xapian database of available software components.
 * You can search for components using various criteria, as well as getting some information
 * about the data provided by this Appstream database.
 *
 * See also: #AsComponent, #AsSearchQuery
 */

struct _AsDatabasePrivate {
	struct XADatabaseRead* db;
	gboolean opened;
	gchar* database_path;
};

static gpointer as_database_parent_class = NULL;


#define AS_DATABASE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_DATABASE, AsDatabasePrivate))
enum  {
	AS_DATABASE_DUMMY_PROPERTY,
	AS_DATABASE_DATABASE_PATH
};

static gboolean as_database_real_open (AsDatabase* self);
static void as_database_finalize (GObject* obj);

/**
 * as_database_construct:
 *
 * Construct a new #AsDatabase.
 *
 * Returns: (transfer full): a new #AsDatabase
 **/
AsDatabase*
as_database_construct (GType object_type)
{
	AsDatabase * self = NULL;
	self = (AsDatabase*) g_object_new (object_type, NULL);

	self->priv->db = xa_database_read_new ();
	self->priv->opened = FALSE;
	as_database_set_database_path (self, AS_APPSTREAM_DATABASE_PATH);

	return self;
}

/**
 * as_database_new:
 *
 * Creates a new #AsDatabase.
 *
 * Returns: (transfer full): a new #AsDatabase
 **/
AsDatabase*
as_database_new (void)
{
	return as_database_construct (AS_TYPE_DATABASE);
}


static gboolean
as_database_real_open (AsDatabase* self)
{
	gboolean ret = FALSE;

	ret = xa_database_read_open (self->priv->db, self->priv->database_path);
	self->priv->opened = ret;

	return ret;
}


gboolean
as_database_open (AsDatabase* self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	return AS_DATABASE_GET_CLASS (self)->open (self);
}


/**
 * as_database_db_exists:
 * @self: a valid #AsDatabase instance
 *
 * Returns: TRUE if the application database exists
 */
gboolean
as_database_db_exists (AsDatabase* self)
{
	g_return_val_if_fail (self != NULL, FALSE);

	return g_file_test (self->priv->database_path, G_FILE_TEST_IS_DIR);
}

/**
 * as_database_get_all_components:
 * @self: a valid #AsDatabase instance
 *
 * Dump a list of all components found in the database.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects
 */
GPtrArray*
as_database_get_all_components (AsDatabase* self)
{
	GPtrArray* cpt_array = NULL;
	g_return_val_if_fail (self != NULL, NULL);
	if (!self->priv->opened)
		return NULL;

	cpt_array = xa_database_read_get_all_components (self->priv->db);
	return cpt_array;
}

/**
 * as_database_find_components:
 * @self: a valid #AsDatabase instance
 * @query: a #AsSearchQuery
 *
 * Find components in the Appstream database.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found
 */
GPtrArray*
as_database_find_components (AsDatabase* self, AsSearchQuery* query)
{
	GPtrArray* cpt_array;
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (query != NULL, NULL);
	if (!self->priv->opened)
		return NULL;

	cpt_array = xa_database_read_find_components (self->priv->db, query);

	return cpt_array;
}

/**
 * as_database_find_components_by_str:
 * @self: a valid #AsDatabase instance
 * @search_str: the string to search for
 * @categories_str: (allow-none) (default NULL): a comma-separated list of category names, or NULL to search in all categories
 *
 * Find components in the Appstream database by searching for a simple string.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found
 */
GPtrArray*
as_database_find_components_by_str (AsDatabase* self, const gchar* search_str, const gchar* categories_str)
{
	GPtrArray* cpt_array;
	AsSearchQuery* query;
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (search_str != NULL, NULL);

	query = as_search_query_new (search_str);
	if (categories_str == NULL) {
		as_search_query_set_search_all_categories (query);
	} else {
		as_search_query_set_categories_from_string (query, categories_str);
	}
	cpt_array = as_database_find_components (self, query);
	g_object_unref (query);
	return cpt_array;
}

/**
 * as_database_get_component_by_id:
 * @self: a valid #AsDatabase instance
 * @idname: the ID of the component
 *
 * Get a component by it's ID
 *
 * Returns: (transfer full): an #AsComponent or NULL if none was found
 **/
AsComponent*
as_database_get_component_by_id (AsDatabase *self, const gchar *idname)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (idname != NULL, NULL);

	return xa_database_read_get_component_by_id (self->priv->db, idname);
}

/**
 * as_database_get_components_by_provides:
 * @self a valid #AsDatabase instance
 * @kind an #AsProvidesKind
 * @value a value of the selected provides kind
 * @data (allow-none) (default NULL): additional provides data
 *
 * Find components in the Appstream database.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found, NULL on error
 */
GPtrArray*
as_database_get_components_by_provides (AsDatabase* self, AsProvidesKind kind, const gchar *value, const gchar *data)
{
	GPtrArray* cpt_array;
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (value != NULL, NULL);
	if (!self->priv->opened)
		return NULL;

	cpt_array = xa_database_read_get_components_by_provides (self->priv->db, kind, value, data);

	return cpt_array;
}

/**
 * as_database_get_components_by_kind:
 * @self a valid #AsDatabase instance
 * @kind an #AsComponentKind
 *
 * Find components of a given kind.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found, NULL on error
 */
GPtrArray*
as_database_get_components_by_kind (AsDatabase* self, AsComponentKind kind)
{
	GPtrArray* cpt_array;
	g_return_val_if_fail (self != NULL, NULL);
	if (!self->priv->opened)
		return NULL;

	cpt_array = xa_database_read_get_components_by_kind (self->priv->db, kind);

	return cpt_array;
}

const gchar*
as_database_get_database_path (AsDatabase* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->database_path;
}


void
as_database_set_database_path (AsDatabase* self, const gchar* value)
{
	g_return_if_fail (self != NULL);
	g_free (self->priv->database_path);
	self->priv->database_path = g_strdup (value);
	g_object_notify ((GObject *) self, "database-path");
}

static void
as_database_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	AsDatabase * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_DATABASE, AsDatabase);
	switch (property_id) {
		case AS_DATABASE_DATABASE_PATH:
			g_value_set_string (value, as_database_get_database_path (self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void
as_database_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	AsDatabase * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_DATABASE, AsDatabase);
	switch (property_id) {
		case AS_DATABASE_DATABASE_PATH:
			as_database_set_database_path (self, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
as_database_class_init (AsDatabaseClass * klass)
{
	as_database_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsDatabasePrivate));
	AS_DATABASE_CLASS (klass)->open = as_database_real_open;
	G_OBJECT_CLASS (klass)->get_property = as_database_get_property;
	G_OBJECT_CLASS (klass)->set_property = as_database_set_property;
	G_OBJECT_CLASS (klass)->finalize = as_database_finalize;
	g_object_class_install_property (G_OBJECT_CLASS (klass),
									AS_DATABASE_DATABASE_PATH,
									g_param_spec_string ("database-path", "database-path", "database-path", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE)
	);
}


static void
as_database_instance_init (AsDatabase * self)
{
	self->priv = AS_DATABASE_GET_PRIVATE (self);
}


static void
as_database_finalize (GObject* obj)
{
	AsDatabase * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_DATABASE, AsDatabase);
	xa_database_read_free (self->priv->db);
	g_free (self->priv->database_path);
	G_OBJECT_CLASS (as_database_parent_class)->finalize (obj);
}


/**
 * as_database_get_type:
 *
 * Class to access the AppStream
 * application database
 */
GType
as_database_get_type (void)
{
	static volatile gsize as_database_type_id__volatile = 0;
	if (g_once_init_enter (&as_database_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
				sizeof (AsDatabaseClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) as_database_class_init,
				(GClassFinalizeFunc) NULL,
				NULL,
				sizeof (AsDatabase),
				0,
				(GInstanceInitFunc) as_database_instance_init,
				NULL
		};
		GType as_database_type_id;
		as_database_type_id = g_type_register_static (G_TYPE_OBJECT, "AsDatabase", &g_define_type_info, 0);
		g_once_init_leave (&as_database_type_id__volatile, as_database_type_id);
	}
	return as_database_type_id__volatile;
}
