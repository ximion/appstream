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

#include "as-category.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <glib/gi18n-lib.h>
#include <gobject/gvaluecollector.h>

/**
 * SECTION:as-category
 * @short_description: Representation of a XDG category
 * @include: appstream.h
 *
 * This object represents an XDG category, as defined at:
 * http://standards.freedesktop.org/menu-spec/menu-spec-1.0.html#category-registry
 *
 * The #AsCategory object does not support all aspects of a menu. It's main purpose
 * is to be used in software-centers to show information about application-groups,
 * which are use to thematically group applications.
 *
 * You can use #AsMenuParser to get a set of supported default categories.
 *
 * See also: #AsMenuParser
 */

typedef struct
{
	gchar *name;
	gchar *summary;
	gchar *icon;
	gchar *directory;
	GList *included;
	GList *excluded;
	gint level;
	GList *subcats;
} AsCategoryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsCategory, as_category, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_category_get_instance_private (o))

enum  {
	AS_CATEGORY_DUMMY_PROPERTY,
	AS_CATEGORY_NAME,
	AS_CATEGORY_SUMMARY,
	AS_CATEGORY_ICON,
	AS_CATEGORY_DIRECTORY,
	AS_CATEGORY_INCLUDED,
	AS_CATEGORY_EXCLUDED,
	AS_CATEGORY_LEVEL,
	AS_CATEGORY_SUBCATEGORIES
};

/**
 * as_category_init:
 **/
static void
as_category_init (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	priv->included = NULL;
	priv->excluded = NULL;
	priv->subcats = NULL;
}

/**
 * as_category_finalize:
 */
static void
as_category_finalize (GObject *object)
{
	AsCategory *cat = AS_CATEGORY (object);
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	g_free (priv->name);
	g_free (priv->summary);
	g_free (priv->icon);
	g_free (priv->directory);
	g_list_free_full (priv->included, g_free);
	g_list_free_full (priv->excluded, g_free);
	g_list_free_full (priv->subcats, g_object_unref);

	G_OBJECT_CLASS (as_category_parent_class)->finalize (object);
}

/**
 * as_category_complete:
 *
 * Update incomplete category data with information from
 * "/usr/share/desktop-directories".
 */
void
as_category_complete (AsCategory *cat)
{
	GKeyFile* kfile = NULL;
	GError *error = NULL;
	gchar *path;
	gchar *str = NULL;
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	if (priv->directory == NULL) {
		g_debug ("No directory set for category %s", priv->name);
		return;
	}
	as_category_set_summary (cat, "");
	as_category_set_icon (cat, "applications-other");

	kfile = g_key_file_new ();
	path = g_strdup_printf ("/usr/share/desktop-directories/%s", priv->directory);
	g_key_file_load_from_file (kfile, path, 0, &error);
	g_free (path);
	if (error != NULL)
		goto out;

	str = g_key_file_get_string (kfile, "Desktop Entry", "Name", &error);
	if (error != NULL)
		goto out;
	as_category_set_name (cat, str);
	g_free (str);
	str = NULL;

	if (g_key_file_has_key (kfile, "Desktop Entry", "Comment", NULL)) {
		str = g_key_file_get_string (kfile, "Desktop Entry", "Comment", &error);
		if (error != NULL)
			goto out;
		as_category_set_summary (cat, str);
		g_free (str);
		str = NULL;
	}

	str = g_key_file_get_string (kfile, "Desktop Entry", "Icon", &error);
	if (error != NULL)
		goto out;
	if (str == NULL)
		as_category_set_icon (cat, "");
	else
		as_category_set_icon (cat, str);

	if (priv->summary == NULL)
		as_category_set_summary (cat, "");

out:
	if (str != NULL)
		g_free (str);
	g_key_file_unref (kfile);
	if (error != NULL) {
		g_debug ("Error retrieving data for %s: %s\n", priv->directory, error->message);
		g_error_free (error);
	}
}

/**
 * as_category_add_subcategory:
 * @cat: An instance of #AsCategory.
 * @subcat: A subcategory to add.
 *
 * Add a subcategory to this category.
 */
void
as_category_add_subcategory (AsCategory *cat, AsCategory *subcat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	priv->subcats = g_list_append (priv->subcats, g_object_ref (subcat));
}


/**
 * as_category_remove_subcategory:
 * @cat: An instance of #AsCategory.
 * @subcat: A subcategory to remove.
 *
 * Drop a subcategory from this #AsCategory.
 */
void
as_category_remove_subcategory (AsCategory *cat, AsCategory *subcat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	priv->subcats = g_list_remove (priv->subcats, subcat);
}

/**
 * as_category_has_subcategory:
 * @cat: An instance of #AsCategory.
 *
 * Test for sub-categories.
 *
 * Returns: %TRUE if this category has any subcategory
 */
gboolean
as_category_has_subcategory (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return g_list_length (priv->subcats) > 0;
}

/**
 * as_category_get_name:
 * @cat: An instance of #AsCategory.
 *
 * Get the name of this category.
 */
const gchar*
as_category_get_name (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return priv->name;
}

/**
 * as_category_set_name:
 * @cat: An instance of #AsCategory.
 *
 * Set the name of this category.
 */
void
as_category_set_name (AsCategory *cat, const gchar *value)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	g_free (priv->name);
	priv->name = g_strdup (value);
	g_object_notify ((GObject*) cat, "name");
}

/**
 * as_category_get_summary:
 * @cat: An instance of #AsCategory.
 *
 * Get the summary (short description) of this category.
 */
const gchar*
as_category_get_summary (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return priv->summary;
}

/**
 * as_category_set_summary:
 * @cat: An instance of #AsCategory.
 * @value: A new short summary of this category.
 *
 * Get the summary (short description) of this category.
 */
void
as_category_set_summary (AsCategory *cat, const gchar *value)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	g_free (priv->summary);
	priv->summary = g_strdup (value);
	g_object_notify ((GObject*) cat, "summary");
}

/**
 * as_category_get_icon:
 * @cat: An instance of #AsCategory.
 *
 * Get the stock icon name for this category.
 */
const gchar*
as_category_get_icon (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return priv->icon;
}

/**
 * as_category_set_icon:
 * @cat: An instance of #AsCategory.
 *
 * Set the stock icon name for this category.
 */
void
as_category_set_icon (AsCategory *cat, const gchar *value)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	g_free (priv->icon);
	priv->icon = g_strdup (value);
	g_object_notify ((GObject*) cat, "icon");
}

/**
 * as_category_get_directory:
 * @cat: An instance of #AsCategory.
 *
 * Get associated XDG directory name for this category,
 * in case one exists in "/usr/share/desktop-directories/".
 */
const gchar*
as_category_get_directory (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return priv->directory;
}

/**
 * as_category_set_directory:
 * @cat: An instance of #AsCategory.
 *
 * Set associated XDG directory name for this category.
 */
void
as_category_set_directory (AsCategory *cat, const gchar *value)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	g_free (priv->directory);
	priv->directory = g_strdup (value);
	g_object_notify ((GObject*) cat, "directory");
}

/**
 * as_category_get_included:
 * @cat: An instance of #AsCategory.
 *
 * Returns: (element-type utf8) (transfer none): A list of category names
 */
GList*
as_category_get_included (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return priv->included;
}

/**
 * as_category_get_excluded:
 * @cat: An instance of #AsCategory.
 *
 * Returns: (element-type utf8) (transfer none): A list of category names
 */
GList*
as_category_get_excluded (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return priv->excluded;
}

/**
 * as_category_get_level:
 */
gint
as_category_get_level (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return priv->level;
}

/**
 * as_category_set_level:
 */
void
as_category_set_level (AsCategory *cat, gint value)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);

	priv->level = value;
	g_object_notify ((GObject*) cat, "level");
}

/**
 * as_category_get_subcategories:
 * @cat: An instance of #AsCategory.
 *
 * Returns: (element-type AsCategory) (transfer none): A list of subcategories.
 */
GList*
as_category_get_subcategories (AsCategory *cat)
{
	AsCategoryPrivate *priv = GET_PRIVATE (cat);
	return priv->subcats;
}

/**
 * as_category_get_property:
 */
static void
as_category_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AsCategory  *cat;
	cat = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_CATEGORY, AsCategory);
	switch (property_id) {
		case AS_CATEGORY_NAME:
			g_value_set_string (value, as_category_get_name (cat));
			break;
		case AS_CATEGORY_SUMMARY:
			g_value_set_string (value, as_category_get_summary (cat));
			break;
		case AS_CATEGORY_ICON:
			g_value_set_string (value, as_category_get_icon (cat));
			break;
		case AS_CATEGORY_DIRECTORY:
			g_value_set_string (value, as_category_get_directory (cat));
			break;
		case AS_CATEGORY_INCLUDED:
			g_value_set_pointer (value, as_category_get_included (cat));
			break;
		case AS_CATEGORY_EXCLUDED:
			g_value_set_pointer (value, as_category_get_excluded (cat));
			break;
		case AS_CATEGORY_LEVEL:
			g_value_set_int (value, as_category_get_level (cat));
			break;
		case AS_CATEGORY_SUBCATEGORIES:
			g_value_set_pointer (value, as_category_get_subcategories (cat));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/**
 * as_category_set_property:
 */
static void
as_category_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	AsCategory  *cat;
	cat = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_CATEGORY, AsCategory);
	switch (property_id) {
		case AS_CATEGORY_NAME:
			as_category_set_name (cat, g_value_get_string (value));
			break;
		case AS_CATEGORY_SUMMARY:
			as_category_set_summary (cat, g_value_get_string (value));
			break;
		case AS_CATEGORY_ICON:
			as_category_set_icon (cat, g_value_get_string (value));
			break;
		case AS_CATEGORY_DIRECTORY:
			as_category_set_directory (cat, g_value_get_string (value));
			break;
		case AS_CATEGORY_LEVEL:
			as_category_set_level (cat, g_value_get_int (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/**
 * as_category_class_init:
 */
static void
as_category_class_init (AsCategoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = as_category_get_property;
	object_class->set_property = as_category_set_property;
	object_class->finalize = as_category_finalize;

	g_object_class_install_property (object_class,
					AS_CATEGORY_NAME,
					g_param_spec_string ("name", "name", "name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
					AS_CATEGORY_SUMMARY,
					g_param_spec_string ("summary", "summary", "summary", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					AS_CATEGORY_ICON,
					g_param_spec_string ("icon", "icon", "icon", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
					AS_CATEGORY_DIRECTORY,
					g_param_spec_string ("directory", "directory", "directory", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
					AS_CATEGORY_INCLUDED,
					g_param_spec_pointer ("included", "included", "included", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					AS_CATEGORY_EXCLUDED,
					g_param_spec_pointer ("excluded", "excluded", "excluded", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					AS_CATEGORY_LEVEL,
					g_param_spec_int ("level", "level", "level", G_MININT, G_MAXINT, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
					AS_CATEGORY_SUBCATEGORIES,
					g_param_spec_pointer ("subcategories", "subcategories", "subcategories", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}

/**
 * as_category_new:
 *
 * Creates a new #AsCategory.
 *
 * Returns: (transfer full): a new #AsCategory
 **/
AsCategory*
as_category_new (void)
{
	AsCategory *cat;
	cat = g_object_new (AS_TYPE_CATEGORY, NULL);
	return AS_CATEGORY (cat);
}
