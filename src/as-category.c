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

struct _AsCategoryPrivate {
	gchar* name;
	gchar* summary;
	gchar* icon;
	gchar* directory;
	GList* included;
	GList* excluded;
	gint level;
	GList* subcats;
};

static gpointer as_category_parent_class = NULL;

#define AS_CATEGORY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_CATEGORY, AsCategoryPrivate))
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

static void as_category_set_summary (AsCategory* self, const gchar* value);

static void as_category_finalize (GObject* obj);
static void as_category_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void as_category_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);

AsCategory*
as_category_construct (GType object_type)
{
	AsCategory * self = NULL;
	self = (AsCategory*) g_object_new (object_type, NULL);
	self->priv->included = NULL;
	self->priv->excluded = NULL;
	self->priv->subcats = NULL;
	return self;
}


AsCategory*
as_category_new (void)
{
	return as_category_construct (AS_TYPE_CATEGORY);
}

void
as_category_complete (AsCategory* self)
{
	GKeyFile* kfile = NULL;
	GError *error = NULL;
	gchar *path;
	gchar *str = NULL;
	g_return_if_fail (self != NULL);

	if (self->priv->directory == NULL) {
		g_debug ("No directory set for category %s", self->priv->name);
		return;
	}
	as_category_set_summary (self, "");
	as_category_set_icon (self, "applications-other");

	kfile = g_key_file_new ();
	path = g_strdup_printf ("/usr/share/desktop-directories/%s", self->priv->directory);
	g_key_file_load_from_file (kfile, path, 0, &error);
	g_free (path);
	if (error != NULL)
		goto out;

	str = g_key_file_get_string (kfile, "Desktop Entry", "Name", &error);
	if (error != NULL)
		goto out;
	as_category_set_name (self, str);
	g_free (str);

	if (g_key_file_has_key (kfile, "Desktop Entry", "Comment", NULL)) {
		str = g_key_file_get_string (kfile, "Desktop Entry", "Comment", &error);
		if (error != NULL)
			goto out;
		as_category_set_summary (self, str);
		g_free (str);
	}

	str = g_key_file_get_string (kfile, "Desktop Entry", "Icon", &error);
	if (error != NULL)
		goto out;
	if (str == NULL)
		as_category_set_icon (self, "");
	else
		as_category_set_icon (self, str);
	g_free (str);

	if (self->priv->summary == NULL)
		as_category_set_summary (self, "");

out:
	if (str != NULL)
		g_free (str);
	g_key_file_unref (kfile);
	if (error != NULL) {
		g_debug ("Error retrieving data for %s: %s\n", self->priv->directory, error->message);
		g_error_free (error);
	}
}

void
as_category_add_subcategory (AsCategory* self, AsCategory* cat)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (cat != NULL);

	self->priv->subcats = g_list_append (self->priv->subcats, g_object_ref (cat));
}


void
as_category_remove_subcategory (AsCategory* self, AsCategory* cat)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (cat != NULL);
	self->priv->subcats = g_list_remove (self->priv->subcats, cat);
}


/**
 * @return TRUE if this category has any subcategory
 */
gboolean
as_category_has_subcategory (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, FALSE);

	return g_list_length (self->priv->subcats) > 0;
}


const gchar*
as_category_get_name (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->name;
}


void
as_category_set_name (AsCategory* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->name);
	self->priv->name = g_strdup (value);
	g_object_notify ((GObject *) self, "name");
}


const gchar*
as_category_get_summary (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->summary;
}


static void
as_category_set_summary (AsCategory* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->summary);
	self->priv->summary = g_strdup (value);
	g_object_notify ((GObject *) self, "summary");
}


const gchar*
as_category_get_icon (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->icon;
}


void
as_category_set_icon (AsCategory* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->icon);
	self->priv->icon = g_strdup (value);
	g_object_notify ((GObject *) self, "icon");
}


const gchar*
as_category_get_directory (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->directory;
}


void
as_category_set_directory (AsCategory* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->directory);
	self->priv->directory = g_strdup (value);
	g_object_notify ((GObject *) self, "directory");
}


GList*
as_category_get_included (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->included;
}


GList*
as_category_get_excluded (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->excluded;
}


gint
as_category_get_level (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, 0);
	return self->priv->level;
}


void
as_category_set_level (AsCategory* self, gint value)
{
	g_return_if_fail (self != NULL);

	self->priv->level = value;
	g_object_notify ((GObject *) self, "level");
}


GList*
as_category_get_subcategories (AsCategory* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->subcats;
}


static void
as_category_class_init (AsCategoryClass * klass)
{
	as_category_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsCategoryPrivate));
	G_OBJECT_CLASS (klass)->get_property = as_category_get_property;
	G_OBJECT_CLASS (klass)->set_property = as_category_set_property;
	G_OBJECT_CLASS (klass)->finalize = as_category_finalize;
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CATEGORY_NAME, g_param_spec_string ("name", "name", "name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CATEGORY_SUMMARY, g_param_spec_string ("summary", "summary", "summary", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CATEGORY_ICON, g_param_spec_string ("icon", "icon", "icon", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CATEGORY_DIRECTORY, g_param_spec_string ("directory", "directory", "directory", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CATEGORY_INCLUDED, g_param_spec_pointer ("included", "included", "included", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CATEGORY_EXCLUDED, g_param_spec_pointer ("excluded", "excluded", "excluded", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CATEGORY_LEVEL, g_param_spec_int ("level", "level", "level", G_MININT, G_MAXINT, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CATEGORY_SUBCATEGORIES, g_param_spec_pointer ("subcategories", "subcategories", "subcategories", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}


static void
as_category_instance_init (AsCategory * self)
{
	self->priv = AS_CATEGORY_GET_PRIVATE (self);
}


static void
as_category_finalize (GObject* obj)
{
	AsCategory * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_CATEGORY, AsCategory);
	g_free (self->priv->name);
	g_free (self->priv->summary);
	g_free (self->priv->icon);
	g_free (self->priv->directory);
	g_list_free_full (self->priv->included, g_free);
	g_list_free_full (self->priv->excluded, g_free);
	g_list_free_full (self->priv->subcats, g_object_unref);
	G_OBJECT_CLASS (as_category_parent_class)->finalize (obj);
}


/**
 * Description of an XDG Menu category
 */
GType
as_category_get_type (void)
{
	static volatile gsize as_category_type_id__volatile = 0;
	if (g_once_init_enter (&as_category_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
						sizeof (AsCategoryClass),
						(GBaseInitFunc) NULL,
						(GBaseFinalizeFunc) NULL,
						(GClassInitFunc) as_category_class_init,
						(GClassFinalizeFunc) NULL,
						NULL,
						sizeof (AsCategory),
						0,
						(GInstanceInitFunc) as_category_instance_init,
						NULL
		};
		GType as_category_type_id;
		as_category_type_id = g_type_register_static (G_TYPE_OBJECT, "AsCategory", &g_define_type_info, 0);
		g_once_init_leave (&as_category_type_id__volatile, as_category_type_id);
	}
	return as_category_type_id__volatile;
}


static void
as_category_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	AsCategory * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_CATEGORY, AsCategory);
	switch (property_id) {
		case AS_CATEGORY_NAME:
			g_value_set_string (value, as_category_get_name (self));
			break;
		case AS_CATEGORY_SUMMARY:
			g_value_set_string (value, as_category_get_summary (self));
			break;
		case AS_CATEGORY_ICON:
			g_value_set_string (value, as_category_get_icon (self));
			break;
		case AS_CATEGORY_DIRECTORY:
			g_value_set_string (value, as_category_get_directory (self));
			break;
		case AS_CATEGORY_INCLUDED:
			g_value_set_pointer (value, as_category_get_included (self));
			break;
		case AS_CATEGORY_EXCLUDED:
			g_value_set_pointer (value, as_category_get_excluded (self));
			break;
		case AS_CATEGORY_LEVEL:
			g_value_set_int (value, as_category_get_level (self));
			break;
		case AS_CATEGORY_SUBCATEGORIES:
			g_value_set_pointer (value, as_category_get_subcategories (self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void
as_category_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	AsCategory * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_CATEGORY, AsCategory);
	switch (property_id) {
		case AS_CATEGORY_NAME:
			as_category_set_name (self, g_value_get_string (value));
			break;
		case AS_CATEGORY_SUMMARY:
			as_category_set_summary (self, g_value_get_string (value));
			break;
		case AS_CATEGORY_ICON:
			as_category_set_icon (self, g_value_get_string (value));
			break;
		case AS_CATEGORY_DIRECTORY:
			as_category_set_directory (self, g_value_get_string (value));
			break;
		case AS_CATEGORY_LEVEL:
			as_category_set_level (self, g_value_get_int (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}
