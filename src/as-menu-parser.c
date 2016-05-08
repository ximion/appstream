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

#include "as-menu-parser.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <glib/gi18n-lib.h>

#include "as-category.h"

/**
 * SECTION:as-menu-parser
 * @short_description: Parser for XDG menu files designed for software-centers
 * @include: appstream.h
 *
 * This object parses an XDG menu file and returns a set of #AsCategory objects which
 * can be used by software-centers to group the applications they show.
 * By default, it loads a common set of categories from an internal menu file.
 * A custom menu file may be specified using the alternative class constructor.
 *
 * See also: #AsCategory
 */

typedef struct
{
	gchar* menu_file;
	gboolean update_category_data;
} AsMenuParserPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsMenuParser, as_menu_parser, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_menu_parser_get_instance_private (o))

enum  {
	AS_MENU_PARSER_DUMMY_PROPERTY,
	AS_MENU_PARSER_UPDATE_CATEGORY_DATA
};

static AsCategory *as_menu_parser_parse_menu_entry (AsMenuParser *mp, xmlNode *nd);

/**
 * as_menu_parser_finalize:
 **/
static void
as_menu_parser_finalize (GObject *object)
{
	AsMenuParser *mp = AS_MENU_PARSER (object);
	AsMenuParserPrivate *priv = GET_PRIVATE (mp);

	g_free (priv->menu_file);

	G_OBJECT_CLASS (as_menu_parser_parent_class)->finalize (object);
}

/**
 * as_menu_parser_init:
 **/
static void
as_menu_parser_init (AsMenuParser *mp)
{
	AsMenuParserPrivate *priv = GET_PRIVATE (mp);

	as_menu_parser_set_update_category_data (mp, TRUE);
	priv->menu_file = g_strdup ("/usr/share/app-info/categories.xml");
}

/**
 * as_menu_parser_complete_categories_cb:
 *
 * Helper function.
 */
static void
as_menu_parser_complete_categories_cb (AsCategory *cat, gpointer user_data)
{
	g_return_if_fail (cat != NULL);
	as_category_complete (cat);
}

/**
 * as_menu_parser_parse:
 * @mp: An instance of #AsMenuParser.
 *
 * Parse the menu file
 *
 * Returns: (element-type AsCategory) (transfer full): #GList of #AsCategory objects found in the menu, or NULL if there was an error
 */
GList*
as_menu_parser_parse (AsMenuParser *mp)
{
	GList *category_list = NULL;
	xmlDoc *xdoc;
	xmlNode *root;
	xmlNode *iter;
	category_list = NULL;
	AsMenuParserPrivate *priv = GET_PRIVATE (mp);

	/* parse the document from path */
	xdoc = xmlParseFile (priv->menu_file);
	if (xdoc == NULL) {
		g_debug (_ ("File %s not found or permission denied!"), priv->menu_file);
		return NULL;
	}

	/* get the root node */
	root = xmlDocGetRootElement (xdoc);
	if ((root == NULL) || (g_strcmp0 ((gchar*) root->name, "Menu") != 0)) {
		g_warning (_ ("XDG Menu XML file '%s' is damaged."), priv->menu_file);
		xmlFreeDoc (xdoc);
		return NULL;
	}

	for (iter = root->children; iter != NULL; iter = iter->next) {
		/* spaces between tags are also nodes, discard them */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (g_strcmp0 ((gchar*) iter->name, "Menu") == 0) {
			/* parse menu entry */
			category_list = g_list_append (category_list, as_menu_parser_parse_menu_entry (mp, iter));
		}
	}

	if (priv->update_category_data) {
		/* complete the missing information from desktop-directories folder */
		g_list_foreach (category_list,
				(GFunc) as_menu_parser_complete_categories_cb,
				NULL);
	}

	return category_list;
}

/**
 * as_menu_parser_extend_category_name_list:
 */
static void
as_menu_parser_extend_category_name_list (AsMenuParser *mp, xmlNode *nd, GList *list)
{
	xmlNode *iter;

	for (iter = nd->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (g_strcmp0 ((gchar*) iter->name, "Category") == 0) {
			list = g_list_append (list, (gchar*) xmlNodeGetContent (iter));
		}
	}
}

/**
 * as_menu_parser_parse_category_entry:
 */
static void
as_menu_parser_parse_category_entry (AsMenuParser *mp, xmlNode *nd, AsCategory *cat)
{
	xmlNode *iter;

	for (iter = nd->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (g_strcmp0 ((gchar*) iter->name, "And") == 0) {
			xmlNode *not_iter;
			as_menu_parser_extend_category_name_list (mp,
								  iter,
								  as_category_get_included (cat));
			/* check for "Not" elements */
			for (not_iter = iter->children; not_iter != NULL; not_iter = not_iter->next) {
				if (g_strcmp0 ((gchar*) not_iter->name, "Not") == 0) {
					as_menu_parser_extend_category_name_list (mp,
										  not_iter,
										  as_category_get_excluded (cat));
				}
			}
		} else if (g_strcmp0 ((gchar*) iter->name, "Or") == 0) {
			as_menu_parser_extend_category_name_list (mp,
								  iter,
								  as_category_get_included (cat));
		}
	}
}

/**
 * as_menu_parser_parse_menu_entry:
 */
static AsCategory*
as_menu_parser_parse_menu_entry (AsMenuParser *mp, xmlNode *nd)
{
	AsCategory* cat = NULL;
	xmlNode *iter;

	cat = as_category_new ();
	for (iter = nd->children; iter != NULL; iter = iter->next) {
		const gchar* node_name;
		/* spaces between tags are also nodes, discard them */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (const gchar*) iter->name;
		if (g_strcmp0 (node_name, "Name") == 0) {
			/* we don't want a localized name (indicated through a language property) */
			if (iter->properties == NULL) {
				gchar *content;
				content = (gchar*) xmlNodeGetContent (iter);
				as_category_set_name (cat, content);
				g_free (content);
			}
		} else if (g_strcmp0 (node_name, "Directory") == 0) {
			gchar *content;
			content = (gchar*) xmlNodeGetContent (iter);
			as_category_set_directory (cat, content);
			g_free (content);
		} else if (g_strcmp0 (node_name, "Icon") == 0) {
			gchar *content;
			content = (gchar*) xmlNodeGetContent (iter);
			as_category_set_icon (cat, content);
			g_free (content);
		} else if (g_strcmp0 (node_name, "Categories") == 0) {
			as_menu_parser_parse_category_entry (mp, iter, cat);
		} else if (g_strcmp0 (node_name, "Menu") == 0) {
			/* we have a submenu! */
			AsCategory* subcat;
			subcat = as_menu_parser_parse_menu_entry (mp, iter);
			as_category_add_subcategory (cat, subcat);
			g_object_unref (subcat);
		}
	}

	return cat;
}

/**
 * as_menu_parser_get_update_category_data:
 * @mp: An instance of #AsMenuParser.
 */
gboolean
as_menu_parser_get_update_category_data (AsMenuParser *mp)
{
	AsMenuParserPrivate *priv = GET_PRIVATE (mp);
	return priv->update_category_data;
}

/**
 * as_menu_parser_set_update_category_data:
 * @mp: An instance of #AsMenuParser.
 */
void
as_menu_parser_set_update_category_data (AsMenuParser *mp, gboolean value)
{
	AsMenuParserPrivate *priv = GET_PRIVATE (mp);
	priv->update_category_data = value;
	g_object_notify ((GObject *) mp, "update-category-data");
}

/**
 * as_menu_parser_get_property:
 */
static void
as_menu_parser_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AsMenuParser  *mp;
	mp = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_MENU_PARSER, AsMenuParser);
	switch (property_id) {
		case AS_MENU_PARSER_UPDATE_CATEGORY_DATA:
			g_value_set_boolean (value, as_menu_parser_get_update_category_data (mp));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/**
 * as_menu_parser_set_property:
 */
static void
as_menu_parser_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AsMenuParser  *mp;
	mp = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_MENU_PARSER, AsMenuParser);
	switch (property_id) {
		case AS_MENU_PARSER_UPDATE_CATEGORY_DATA:
			as_menu_parser_set_update_category_data (mp, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/**
 * as_menu_parser_class_init:
 **/
static void
as_menu_parser_class_init (AsMenuParserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_menu_parser_finalize;
	object_class->get_property = as_menu_parser_get_property;
	object_class->set_property = as_menu_parser_set_property;

	g_object_class_install_property (object_class,
					AS_MENU_PARSER_UPDATE_CATEGORY_DATA,
					g_param_spec_boolean ("update-category-data", "update-category-data", "update-category-data", FALSE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
}

/**
 * as_menu_parser_new:
 *
 * Creates a new #AsMenuParser.
 *
 * Returns: (transfer full): an #AsMenuParser.
 *
 **/
AsMenuParser*
as_menu_parser_new (void)
{
	AsMenuParser *mp;
	mp = g_object_new (AS_TYPE_MENU_PARSER, NULL);
	return AS_MENU_PARSER (mp);
}

/**
 * as_menu_parser_new_from_file:
 * @menu_file: The menu-file to parse.
 *
 * Creates a new #AsMenuParser using a custom XDG menu XML file.
 *
 * Returns: (transfer full): an #AsMenuParser.
 *
 **/
AsMenuParser*
as_menu_parser_new_from_file (const gchar *menu_file)
{
	AsMenuParser *mp;
	AsMenuParserPrivate *priv;

	mp = as_menu_parser_new ();
	priv = GET_PRIVATE (mp);

	g_free (priv->menu_file);
	priv->menu_file = g_strdup (menu_file);

	return mp;
}

/**
 * as_get_system_categories:
 *
 * Get a GList of the default AppStream categories
 *
 * Returns: (element-type AsCategory) (transfer full): #GList of #AsCategory objects
 */
GList*
as_get_system_categories (void)
{
	AsMenuParser *parser;
	GList* system_cats = NULL;

	parser = as_menu_parser_new ();
	system_cats = as_menu_parser_parse (parser);
	g_object_unref (parser);

	return system_cats;
}
