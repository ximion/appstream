/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#include "as-database.h"

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <database-cwrap.hpp>
#include <glib/gstdio.h>

#include "as-utils.h"
#include "as-settings-private.h"

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
 * is explicitly defined via %as_database_set_database_path().
 *
 * A new cache can be created using the appstreamcli(1) utility.
 *
 * See also: #AsComponent
 */

typedef struct
{
	struct XADatabaseRead *xdb;
	gboolean opened;
	gchar* database_path;
} AsDatabasePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsDatabase, as_database, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_database_get_instance_private (o))

/* TRANSLATORS: List of "grey-listed" words sperated with ";"
 * Do not translate this list directly. Instead,
 * provide a list of words in your language that people are likely
 * to include in a search but that should normally be ignored in
 * the search.
 */
#define AS_SEARCH_GREYLIST_STR _("app;application;package;program;programme;suite;tool")

/**
 * as_database_init:
 **/
static void
as_database_init (AsDatabase *db)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	priv->xdb = xa_database_read_new ();
	priv->opened = FALSE;
	as_database_set_database_path (db, AS_APPSTREAM_CACHE_PATH);
}

/**
 * as_database_finalize:
 **/
static void
as_database_finalize (GObject *object)
{
	AsDatabase *db = AS_DATABASE (object);
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	xa_database_read_free (priv->xdb);
	g_free (priv->database_path);

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
 *
 * Open the current AppStream metadata cache for reading.
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
as_database_open (AsDatabase *db)
{
	gboolean ret = FALSE;
	g_autofree gchar *path = NULL;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	path = g_build_filename (priv->database_path, "xapian", "default", NULL);
	ret = xa_database_read_open (priv->xdb, path);
	priv->opened = ret;

	return ret;
}

/**
 * as_database_get_all_components:
 * @db: An instance of #AsDatabase.
 *
 * Dump a list of all software components found in the database.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects.
 */
GPtrArray*
as_database_get_all_components (AsDatabase *db)
{
	GPtrArray *cpts = NULL;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (!priv->opened)
		return NULL;

	cpts = xa_database_read_get_all_components (priv->xdb);
	return cpts;
}

/**
 * as_database_find_components:
 * @db: An instance of #AsDatabase.
 * @term: (nullable): a search-term to look for.
 * @cats_str: (nullable): A set of categories to be searched in.
 *
 * Find components in the Appstream database, which match a given term
 * in a set of categories.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found
 */
GPtrArray*
as_database_find_components (AsDatabase *db, const gchar *term, const gchar *cats_str)
{
	GPtrArray *cpts;
	AsSearchQuery* query;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	/* return everything if term and categories are both NULL or empty */
	if ((as_str_empty (term)) && (as_str_empty (cats_str)))
		return as_database_get_all_components (db);

	if (!priv->opened)
		return NULL;

	query = as_search_query_new (term);
	if (cats_str == NULL) {
		as_search_query_set_search_all_categories (query);
	} else {
		as_search_query_set_categories_from_string (query, cats_str);
	}

	cpts = xa_database_read_find_components (priv->xdb, query);
	g_object_unref (query);
	return cpts;
}

/**
 * as_database_get_component_by_id:
 * @db: An instance of #AsDatabase.
 * @idname: the ID of the component
 *
 * Get a component by it's ID
 *
 * Returns: (transfer full): an #AsComponent or NULL if none was found
 **/
AsComponent*
as_database_get_component_by_id (AsDatabase *db, const gchar *idname)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	g_return_val_if_fail (idname != NULL, NULL);

	return xa_database_read_get_component_by_id (priv->xdb, idname);
}

/**
 * as_database_get_components_by_provides:
 * @db: An instance of #AsDatabase.
 * @kind: an #AsProvidesKind
 * @item: the name of the provided item.
 *
 * Find components in the Appstream database.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found, NULL on error
 */
GPtrArray*
as_database_get_components_by_provides (AsDatabase *db, AsProvidedKind kind, const gchar *item)
{
	GPtrArray* cpt_array;
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	g_return_val_if_fail (item != NULL, NULL);

	if (!priv->opened)
		return NULL;

	cpt_array = xa_database_read_get_components_by_provides (priv->xdb, kind, item);

	return cpt_array;
}

/**
 * as_database_get_components_by_kind:
 * @db: An instance of #AsDatabase.
 * @kinds: an #AsComponentKind bitfield
 *
 * Find components of a given kind.
 *
 * Returns: (element-type AsComponent) (transfer full): an array of #AsComponent objects which have been found, NULL on error
 */
GPtrArray*
as_database_get_components_by_kind (AsDatabase *db, AsComponentKind kinds)
{
	GPtrArray* cpt_array;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (!priv->opened)
		return NULL;

	cpt_array = xa_database_read_get_components_by_kind (priv->xdb, kinds);

	return cpt_array;
}

/**
 * as_database_get_database_path:
 * @db: An instance of #AsDatabase.
 *
 * Get the current path of the AppStream database we use.
 */
const gchar*
as_database_get_database_path (AsDatabase *db)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	return priv->database_path;
}

/**
 * as_database_set_database_path:
 * @db: An instance of #AsDatabase.
 * @dir: The directory of the Xapian database.
 *
 * Set the location of the AppStream database we use.
 */
void
as_database_set_database_path (AsDatabase *db, const gchar *dir)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	g_free (priv->database_path);
	priv->database_path = g_strdup (dir);
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
