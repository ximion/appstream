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

#include "as-database.h"

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-settings-private.h"
#include "as-data-pool.h"

/**
 * SECTION:as-database
 * @short_description: Read-only access to the AppStream component database
 * @include: appstream.h
 *
 * This object provides access to the Appstream Xapian database of available software components.
 * You can search for components using various criteria, as well as getting some information
 * about the data provided by this AppStream database.
 *
 * By default, the global software component cache is used as datasource, unless a different database
 * is explicitly defined via %as_database_set_location().
 *
 * A new cache can be created using the appstreamcli(1) utility.
 *
 * See also: #AsComponent, #AsDataPool
 */

typedef struct
{
	AsDataPool *dpool;
} AsDatabasePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsDatabase, as_database, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_database_get_instance_private (o))

/**
 * as_database_init:
 **/
static void
as_database_init (AsDatabase *db)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	priv->dpool = as_data_pool_new ();
}

/**
 * as_database_finalize:
 **/
static void
as_database_finalize (GObject *object)
{
	AsDatabase *db = AS_DATABASE (object);
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	g_object_unref (priv->dpool);

	G_OBJECT_CLASS (as_database_parent_class)->finalize (object);
}

/**
 * as_database_class_init:
 **/
static void
as_database_class_init (AsDatabaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_database_finalize;
}

/**
 * as_database_open:
 * @db: An instance of #AsDatabase.
 * @error: A #GError or %NULL.
 *
 * Open the current AppStream metadata cache for reading.
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
as_database_open (AsDatabase *db, GError **error)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	return as_data_pool_load (priv->dpool, NULL, error);
}

/**
 * as_database_get_all_components:
 * @db: An instance of #AsDatabase.
 * @error: A #GError or %NULL.
 *
 * Dump a list of all software components found in the database.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects.
 */
GPtrArray*
as_database_get_all_components (AsDatabase *db, GError **error)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	return as_data_pool_get_components (priv->dpool);
}

/**
 * as_database_find_components:
 * @db: An instance of #AsDatabase.
 * @term: (nullable): a search-term to look for.
 * @cats_str: (nullable): A semicolon-delimited list of lower-cased category names, e.g. "science;development".
 * @error: A #GError or %NULL.
 *
 * Find components in the AppStream database, which match a given term.
 * You can limit the search to a specific set of categories by setting the categories string to
 * a semicolon-separated list of lower-cased category names.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found
 */
GPtrArray*
as_database_find_components (AsDatabase *db, const gchar *term, const gchar *cats_str, GError **error)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (term == NULL)
		return as_data_pool_get_components_by_categories (priv->dpool, cats_str);

	return as_data_pool_search (priv->dpool, term);
}

/**
 * as_database_get_component_by_id:
 * @db: An instance of #AsDatabase.
 * @cid: the ID of the component, e.g. "org.kde.gwenview.desktop"
 * @error: A #GError or %NULL.
 *
 * Get a component by its AppStream-ID.
 *
 * Returns: (transfer full): an #AsComponent or %NULL if none was found.
 **/
AsComponent*
as_database_get_component_by_id (AsDatabase *db, const gchar *cid, GError **error)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	return as_data_pool_get_component_by_id (priv->dpool, cid);
}

/**
 * as_database_get_components_by_provided_item:
 * @db: An instance of #AsDatabase.
 * @kind: an #AsProvidesKind
 * @item: the name of the provided item.
 * @error: A #GError or %NULL.
 *
 * Find components in the Appstream database.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found, NULL on error
 */
GPtrArray*
as_database_get_components_by_provided_item (AsDatabase *db, AsProvidedKind kind, const gchar *item, GError **error)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	return as_data_pool_get_components_by_provided_item (priv->dpool, kind, item, error);
}

/**
 * as_database_get_components_by_kind:
 * @db: An instance of #AsDatabase.
 * @kind: an #AsComponentKind.
 * @error: A #GError or %NULL.
 *
 * Return a list of all components in the database which match a certain kind.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found, %NULL on error
 */
GPtrArray*
as_database_get_components_by_kind (AsDatabase *db, AsComponentKind kind, GError **error)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	return as_data_pool_get_components_by_kind (priv->dpool, kind, error);
}

/**
 * as_database_get_location:
 * @db: An instance of #AsDatabase.
 *
 * Get the current path of the AppStream database we use.
 */
const gchar*
as_database_get_location (AsDatabase *db)
{
	return NULL;
}

/**
 * as_database_set_location:
 * @db: An instance of #AsDatabase.
 * @dir: The directory of the Xapian database.
 *
 * Set the location of the AppStream database we use.
 */
void
as_database_set_location (AsDatabase *db, const gchar *dir)
{
}

/**
 * as_database_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_database_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsDatabaseError");
	return quark;
}

/**
 * as_database_new:
 *
 * Creates a new #AsDatabase.
 *
 * Returns: (transfer full): a #AsDatabase
 **/
AsDatabase*
as_database_new (void)
{
	AsDatabase *db;
	db = g_object_new (AS_TYPE_DATABASE, NULL);
	return AS_DATABASE (db);
}
