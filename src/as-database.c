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
#include "as-utils-private.h"
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
 * is explicitly defined via %as_database_set_location().
 *
 * A new cache can be created using the appstreamcli(1) utility.
 *
 * See also: #AsComponent, #AsDataPool
 */

typedef struct
{
	struct XADatabaseRead *xdb;
	gboolean opened;
	gchar *database_path;
	gchar **term_greylist;
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
	as_database_set_location (db, AS_APPSTREAM_CACHE_PATH);

	priv->term_greylist = g_strsplit (AS_SEARCH_GREYLIST_STR, ";", -1);
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
	g_strfreev (priv->term_greylist);

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
	gboolean ret = FALSE;
	g_autofree gchar *path = NULL;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	path = g_build_filename (priv->database_path, "xapian", "default", NULL);
	ret = xa_database_read_open (priv->xdb, path);
	priv->opened = ret;

	if (!ret)
		g_set_error_literal (error,
					AS_DATABASE_ERROR,
					AS_DATABASE_ERROR_FAILED,
					_("Unable to open the AppStream software component cache."));

	return ret;
}

/**
 * as_database_test_opened:
 * @db: An instance of #AsDatabase.
 * @error: A #GError or %NULL.
 */
static gboolean
as_database_test_opened (AsDatabase *db, GError **error)
{
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (!priv->opened) {
		g_set_error_literal (error,
					AS_DATABASE_ERROR,
					AS_DATABASE_ERROR_CLOSED,
					_("Tried to perform query on closed database."));
		return FALSE;
	}
	return TRUE;
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
	GPtrArray *cpts = NULL;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (!as_database_test_opened (db, error))
		return NULL;

	cpts = xa_database_read_get_all_components (priv->xdb);
	return cpts;
}

/**
 * as_database_sanitize_search_term:
 *
 * Improve the search term slightly, by stripping whitespaces and
 * removing greylist words.
 */
static gchar*
as_database_sanitize_search_term (AsDatabase *db, const gchar *term)
{
	gchar *res_term;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (term == NULL)
		return NULL;
	res_term = g_strdup (term);

	/* check if there is a ":" in the search, if so, it means the user
	 * could be using a xapian prefix like "pkg:" or "mime:" and in this case
	 * we do not want to alter the search term (as application is in the
	 * greylist but a common mime-type prefix)
	 */
	if (strstr (term, ":") == NULL) {
		guint i;

		/* filter query by greylist (to avoid overly generic search terms) */
		for (i = 0; priv->term_greylist[i] != NULL; i++) {
			gchar *str;
			str = as_str_replace (res_term, priv->term_greylist[i], "");
			g_free (res_term);
			res_term = str;
		}

		/* restore query if it was just greylist words */
		if (g_strcmp0 (res_term, "") == 0) {
			g_debug ("grey-list replaced all terms, restoring");
			g_free (res_term);
			res_term = g_strdup (term);
		}
	}

	/* we have to strip the leading and trailing whitespaces to avoid having
	 * different results for e.g. 'font ' and 'font' (LP: #506419)
	 */
	g_strstrip (res_term);

	return res_term;
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
	GPtrArray *cpts;
	g_autofree gchar *sterm = NULL;
	g_auto(GStrv) cats = NULL;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (!as_database_test_opened (db, error))
		return NULL;

	/* return everything if term and categories are both NULL or empty */
	if ((as_str_empty (term)) && (as_str_empty (cats_str)))
		return as_database_get_all_components (db, error);

	/* sanitize our search term */
	sterm = as_database_sanitize_search_term (db, term);

	if (cats_str != NULL)
		cats = g_strsplit (cats_str, ";", -1);

	cpts = xa_database_read_find_components (priv->xdb, sterm, cats);
	return cpts;
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

	if (!as_database_test_opened (db, error))
		return NULL;
	if (cid == NULL) {
		g_set_error_literal (error,
					AS_DATABASE_ERROR,
					AS_DATABASE_ERROR_TERM_INVALID,
					"Search term must not be NULL.");
		return NULL;
	}

	return xa_database_read_get_component_by_id (priv->xdb, cid);
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
	GPtrArray* cpt_array;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (!as_database_test_opened (db, error))
		return NULL;
	if (item == NULL) {
		g_set_error_literal (error,
					AS_DATABASE_ERROR,
					AS_DATABASE_ERROR_TERM_INVALID,
					"Search term must not be NULL.");
		return NULL;
	}

	cpt_array = xa_database_read_get_components_by_provides (priv->xdb, kind, item);
	return cpt_array;
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
	GPtrArray* cpt_array;
	AsDatabasePrivate *priv = GET_PRIVATE (db);

	if (!as_database_test_opened (db, error))
		return NULL;
	if (kind >= AS_COMPONENT_KIND_LAST) {
		g_set_error_literal (error,
					AS_DATABASE_ERROR,
					AS_DATABASE_ERROR_TERM_INVALID,
					"Can not search for unknown component type.");
		return NULL;
	}

	cpt_array = xa_database_read_get_components_by_kind (priv->xdb, kind);
	return cpt_array;
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
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	return priv->database_path;
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
	AsDatabasePrivate *priv = GET_PRIVATE (db);
	g_free (priv->database_path);
	priv->database_path = g_strdup (dir);
	g_debug ("AppSteam cache location altered to: %s", dir);
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
