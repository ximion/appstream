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

struct _AsMenuParserPrivate {
	gchar* menu_file;
	gboolean update_category_data;
};


static gpointer as_menu_parser_parent_class = NULL;

#define AS_MENU_PARSER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_MENU_PARSER, AsMenuParserPrivate))
enum  {
	AS_MENU_PARSER_DUMMY_PROPERTY,
	AS_MENU_PARSER_UPDATE_CATEGORY_DATA
};

static AsCategory* as_menu_parser_parse_menu_entry (AsMenuParser* self, xmlNode* nd);

static void as_menu_parser_extend_category_name_list (AsMenuParser* self, xmlNode* nd, GList* list);
static void as_menu_parser_parse_category_entry (AsMenuParser* self, xmlNode* nd, AsCategory* cat);
static void as_menu_parser_finalize (GObject* obj);
static void as_menu_parser_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void as_menu_parser_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);

/**
 * as_menu_parser_construct:
 *
 * Create a new MenuParser for the generic AppStream categories list
 *
 * Returns: (transfer full): an #AsMenuParser
 */
AsMenuParser*
as_menu_parser_construct (GType object_type)
{
	AsMenuParser * self = NULL;
	self = (AsMenuParser*) as_menu_parser_construct_from_file (object_type, DATADIR "/app-info/categories.xml");
	return self;
}

/**
 * as_menu_parser_new:
 *
 * Creates a new #AsMenuParser.
 *
 * Returns: (transfer full): an #AsMenuParser
 **/
AsMenuParser*
as_menu_parser_new (void)
{
	return as_menu_parser_construct (AS_TYPE_MENU_PARSER);
}


/**
 * as_menu_parser_construct_from_file:
 *
 * Create a new MenuParser for an arbitrary menu file
 *
 * Returns: (transfer full): an #AsMenuParser
 */
AsMenuParser*
as_menu_parser_construct_from_file (GType object_type, const gchar* menu_file)
{
	AsMenuParser * self = NULL;
	g_return_val_if_fail (menu_file != NULL, NULL);

	self = (AsMenuParser*) g_object_new (object_type, NULL);
	as_menu_parser_set_update_category_data (self, TRUE);

	g_free (self->priv->menu_file);
	self->priv->menu_file = g_strdup (menu_file);
	return self;
}

/**
 * as_menu_parser_new_from_file:
 *
 * Creates a new #AsMenuParser with a custom XDG menu file.
 *
 * @menu_file a custom XDG menu XML file
 *
 * Returns: (transfer full): an #AsMenuParser
 **/
AsMenuParser*
as_menu_parser_new_from_file (const gchar* menu_file)
{
	return as_menu_parser_construct_from_file (AS_TYPE_MENU_PARSER, menu_file);
}

static void
_as_menu_parser_complete_categories (AsCategory* cat, gpointer user_data)
{
	g_return_if_fail (cat != NULL);
	as_category_complete (cat);
}

/**
 * as_menu_parser_parse:
 *
 * Parse the menu file
 *
 * Returns: (element-type AsCategory) (transfer full): #GList of #AsCategory objects found in the menu, or NULL if there was an error
 */
GList*
as_menu_parser_parse (AsMenuParser* self)
{
	GList *category_list = NULL;
	xmlDoc *xdoc;
	xmlNode *root;
	xmlNode *iter;
	g_return_val_if_fail (self != NULL, NULL);
	category_list = NULL;

	/* parse the document from path */
	xdoc = xmlParseFile (self->priv->menu_file);
	if (xdoc == NULL) {
		g_warning (_ ("File %s not found or permission denied!"), self->priv->menu_file);
		return NULL;
	}

	/* get the root node */
	root = xmlDocGetRootElement (xdoc);
	if ((root == NULL) || (g_strcmp0 ((gchar*) root->name, "Menu") != 0)) {
		g_warning (_ ("XDG Menu XML file '%s' is damaged."), self->priv->menu_file);
		xmlFreeDoc (xdoc);
		return NULL;
	}

	for (iter = root->children; iter != NULL; iter = iter->next) {
		/* spaces between tags are also nodes, discard them */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (g_strcmp0 ((gchar*) iter->name, "Menu") == 0) {
			/* parse menu entry */
			category_list = g_list_append (category_list, as_menu_parser_parse_menu_entry (self, iter));
		}
	}

	if (self->priv->update_category_data) {
		/* complete the missing information from desktop-directories folder */
		g_list_foreach (category_list, (GFunc) _as_menu_parser_complete_categories, NULL);
	}

	return category_list;
}

static void
as_menu_parser_extend_category_name_list (AsMenuParser* self, xmlNode* nd, GList* list)
{
	xmlNode *iter;
	g_return_if_fail (self != NULL);

	for (iter = nd->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (g_strcmp0 ((gchar*) iter->name, "Category") == 0) {
			list = g_list_append (list, (gchar*) xmlNodeGetContent (iter));
		}
	}
}

static void
as_menu_parser_parse_category_entry (AsMenuParser* self, xmlNode* nd, AsCategory* cat)
{
	xmlNode *iter;
	g_return_if_fail (self != NULL);
	g_return_if_fail (cat != NULL);

	for (iter = nd->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (g_strcmp0 ((gchar*) iter->name, "And") == 0) {
			xmlNode *not_iter;
			as_menu_parser_extend_category_name_list (self,
													iter,
													as_category_get_included (cat));
			/* check for "Not" elements */
			for (not_iter = iter->children; not_iter != NULL; not_iter = not_iter->next) {
				if (g_strcmp0 ((gchar*) not_iter->name, "Not") == 0) {
					as_menu_parser_extend_category_name_list (self,
													not_iter,
													as_category_get_excluded (cat));
				}
			}
		} else if (g_strcmp0 ((gchar*) iter->name, "Or") == 0) {
			as_menu_parser_extend_category_name_list (self,
													iter,
													as_category_get_included (cat));
		}
	}
}

static AsCategory*
as_menu_parser_parse_menu_entry (AsMenuParser* self, xmlNode* nd)
{
	AsCategory* cat = NULL;
	xmlNode *iter;
	g_return_val_if_fail (self != NULL, NULL);

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
			as_menu_parser_parse_category_entry (self, iter, cat);
		} else if (g_strcmp0 (node_name, "Menu") == 0) {
			/* we have a submenu! */
			AsCategory* subcat;
			subcat = as_menu_parser_parse_menu_entry (self, iter);
			as_category_add_subcategory (cat, subcat);
			g_object_unref (subcat);
		}
	}

	return cat;
}


gboolean
as_menu_parser_get_update_category_data (AsMenuParser* self)
{
	g_return_val_if_fail (self != NULL, FALSE);
	return self->priv->update_category_data;
}


void
as_menu_parser_set_update_category_data (AsMenuParser* self, gboolean value)
{
	g_return_if_fail (self != NULL);
	self->priv->update_category_data = value;
	g_object_notify ((GObject *) self, "update-category-data");
}


static void
as_menu_parser_class_init (AsMenuParserClass * klass)
{
	as_menu_parser_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsMenuParserPrivate));
	G_OBJECT_CLASS (klass)->get_property = as_menu_parser_get_property;
	G_OBJECT_CLASS (klass)->set_property = as_menu_parser_set_property;
	G_OBJECT_CLASS (klass)->finalize = as_menu_parser_finalize;
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_MENU_PARSER_UPDATE_CATEGORY_DATA, g_param_spec_boolean ("update-category-data", "update-category-data", "update-category-data", FALSE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
}


static void
as_menu_parser_instance_init (AsMenuParser * self)
{
	self->priv = AS_MENU_PARSER_GET_PRIVATE (self);
}


static void
as_menu_parser_finalize (GObject* obj)
{
	AsMenuParser * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_MENU_PARSER, AsMenuParser);
	g_free (self->priv->menu_file);
	G_OBJECT_CLASS (as_menu_parser_parent_class)->finalize (obj);
}


/**
 * as_menu_parser_get_type:
 *
 * Parser for XDG Menu files
 */
GType
as_menu_parser_get_type (void)
{
	static volatile gsize as_menu_parser_type_id__volatile = 0;
	if (g_once_init_enter (&as_menu_parser_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
					sizeof (AsMenuParserClass),
					(GBaseInitFunc) NULL,
					(GBaseFinalizeFunc) NULL,
					(GClassInitFunc) as_menu_parser_class_init,
					(GClassFinalizeFunc) NULL,
					NULL,
					sizeof (AsMenuParser),
					0,
					(GInstanceInitFunc) as_menu_parser_instance_init,
					NULL
		};
		GType as_menu_parser_type_id;
		as_menu_parser_type_id = g_type_register_static (G_TYPE_OBJECT, "AsMenuParser", &g_define_type_info, 0);
		g_once_init_leave (&as_menu_parser_type_id__volatile, as_menu_parser_type_id);
	}
	return as_menu_parser_type_id__volatile;
}


static void
as_menu_parser_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	AsMenuParser * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_MENU_PARSER, AsMenuParser);
	switch (property_id) {
		case AS_MENU_PARSER_UPDATE_CATEGORY_DATA:
			g_value_set_boolean (value, as_menu_parser_get_update_category_data (self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void
as_menu_parser_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	AsMenuParser * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_MENU_PARSER, AsMenuParser);
	switch (property_id) {
		case AS_MENU_PARSER_UPDATE_CATEGORY_DATA:
			as_menu_parser_set_update_category_data (self, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
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
	AsMenuParser* parser;
	GList* system_cats = NULL;

	parser = as_menu_parser_new ();
	system_cats = as_menu_parser_parse (parser);
	g_object_unref (parser);

	return system_cats;
}



