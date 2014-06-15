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

#include "as-distro-details.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>
#include <gio/gio.h>

#include "as-settings-private.h"

/**
 * SECTION:as-distro-details
 * @short_description: Object providing information about the current distribution
 * @include: appstream.h
 *
 * This object abstracts various distribution-specific settings and provides information
 * about the (Linux) distribution which is currently in use.
 * It is used internalls to get information about the icon-store or the 3rd-party screenshot
 * service distributors may want to provide.
 *
 * See also: #AsDatabase
 */

struct _AsDistroDetailsPrivate {
	gchar* distro_id;
	gchar* distro_name;
	gchar* distro_version;
	GKeyFile* keyf;
};

/**
 * AS_ICON_PATHS:
 *
 * The path where software icons (of not-installed software) are located.
 */
const gchar* AS_ICON_PATHS[3] = {AS_APPSTREAM_BASE_PATH "/icons", "/var/cache/app-info/icons", NULL};

static gpointer as_distro_details_parent_class = NULL;
#define AS_DISTRO_DETAILS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_DISTRO_DETAILS, AsDistroDetailsPrivate))

enum  {
	AS_DISTRO_DETAILS_DUMMY_PROPERTY,
	AS_DISTRO_DETAILS_DISTRO_ID,
	AS_DISTRO_DETAILS_DISTRO_NAME,
	AS_DISTRO_DETAILS_DISTRO_VERSION
};

static void as_distro_details_set_distro_id (AsDistroDetails* self, const gchar* value);
static void as_distro_details_set_distro_name (AsDistroDetails* self, const gchar* value);
static void as_distro_details_set_distro_version (AsDistroDetails* self, const gchar* value);

static void as_distro_details_finalize (GObject* obj);
static void as_distro_details_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void as_distro_details_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);

/**
 * as_distro_details_construct:
 *
 * Construct a new #AsDistroDetails instance.
 *
 * Returns: (transfer full): a new #AsDistroDetails instance.
 **/
AsDistroDetails*
as_distro_details_construct (GType object_type)
{
	AsDistroDetails * self = NULL;
	GFile* f = NULL;
	gchar *line;
	GError *error = NULL;

	self = (AsDistroDetails*) g_object_new (object_type, NULL);

	as_distro_details_set_distro_id (self, "unknown");
	as_distro_details_set_distro_name (self, "");
	as_distro_details_set_distro_version (self, "");

	/* load configuration */
	self->priv->keyf = g_key_file_new ();
	g_key_file_load_from_file (self->priv->keyf, AS_CONFIG_NAME, G_KEY_FILE_NONE, NULL);

	/* get details about the distribution we are running on */
	f = g_file_new_for_path ("/etc/os-release");
	if (g_file_query_exists (f, NULL)) {
		GDataInputStream *dis;
		GFileInputStream* fis;

		fis = g_file_read (f, NULL, &error);
		if (error != NULL)
			goto out;
		dis = g_data_input_stream_new ((GInputStream*) fis);
		g_object_unref (fis);

		while ((line = g_data_input_stream_read_line (dis, NULL, NULL, &error)) != NULL) {
			gchar **data;
			gchar *dvalue;
			if (error != NULL) {
				g_object_unref (dis);
				goto out;
			}

			data = g_strsplit (line, "=", 2);
			if (g_strv_length (data) != 2) {
				g_strfreev (data);
				g_free (line);
				continue;
			}

			dvalue = data[1];
			if (g_str_has_prefix (dvalue, "\"")) {
				gchar *tmpstr;
				tmpstr = g_strndup (dvalue + 1, strlen(dvalue) - 2);
				g_free (dvalue);
				dvalue = tmpstr;
			}

			if (g_strcmp0 (data[0], "ID") == 0)
				as_distro_details_set_distro_id (self, dvalue);
			else if (g_strcmp0 (data[0], "NAME") == 0)
				as_distro_details_set_distro_name (self, dvalue);
			else if (g_strcmp0 (data[0], "VERSION_ID") == 0)
				as_distro_details_set_distro_version (self, dvalue);

			g_free (line);
		}
	}

out:
	if (error != NULL)
		g_error_free (error);
	g_object_unref (f);

	return self;
}

/**
 * as_distro_details_new:
 *
 * Creates a new #AsDistroDetails instance.
 *
 * Returns: (transfer full): a new #AsDistroDetails instance.
 **/
AsDistroDetails*
as_distro_details_new (void) {
	return as_distro_details_construct (AS_TYPE_DISTRO_DETAILS);
}


/**
 * as_distro_details_get_icon_repository_paths:
 *
 * Returns list of icon-paths for software-center applications to use.
 * Icons of software (even if it is not installed) are stored in these
 * locations.
 *
 * Returns: (transfer full): A NULL-terminated array of paths.
 */
gchar**
as_distro_details_get_icon_repository_paths ()
{
	gchar **paths;
	guint len;
	guint i;

	len = G_N_ELEMENTS (AS_ICON_PATHS);
	paths = g_new0 (gchar *, len + 1);
	for (i = 0; i < len+1; i++) {
		if (i < len)
			paths[i] = g_strdup (AS_ICON_PATHS[i]);
		else
			paths[i] = NULL;
	}

	return paths;
}

gchar*
as_distro_details_config_distro_get_str (AsDistroDetails* self, const gchar* key)
{
	gchar *value;

	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	value = g_key_file_get_string (self->priv->keyf, self->priv->distro_id, key, NULL);

	return value;
}

gboolean
as_distro_details_config_distro_get_bool (AsDistroDetails* self, const gchar* key)
{
	gboolean value;

	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	value = g_key_file_get_boolean (self->priv->keyf, self->priv->distro_id, key, NULL);

	return value;
}


const gchar*
as_distro_details_get_distro_id (AsDistroDetails* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->distro_id;
}


static void
as_distro_details_set_distro_id (AsDistroDetails* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->distro_id);
	self->priv->distro_id = g_strdup (value);
	g_object_notify ((GObject *) self, "distro-id");
}


const gchar*
as_distro_details_get_distro_name (AsDistroDetails* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->distro_name;
}


static void
as_distro_details_set_distro_name (AsDistroDetails* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->distro_name);
	self->priv->distro_name = g_strdup (value);
	g_object_notify ((GObject *) self, "distro-name");
}


const gchar*
as_distro_details_get_distro_version (AsDistroDetails* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->distro_version;
}


static void
as_distro_details_set_distro_version (AsDistroDetails* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->distro_version);
	self->priv->distro_version = g_strdup (value);
	g_object_notify ((GObject *) self, "distro-version");
}


static void
as_distro_details_class_init (AsDistroDetailsClass * klass)
{
	as_distro_details_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsDistroDetailsPrivate));
	G_OBJECT_CLASS (klass)->get_property = as_distro_details_get_property;
	G_OBJECT_CLASS (klass)->set_property = as_distro_details_set_property;
	G_OBJECT_CLASS (klass)->finalize = as_distro_details_finalize;
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_DISTRO_DETAILS_DISTRO_ID, g_param_spec_string ("distro-id", "distro-id", "distro-id", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_DISTRO_DETAILS_DISTRO_NAME, g_param_spec_string ("distro-name", "distro-name", "distro-name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_DISTRO_DETAILS_DISTRO_VERSION, g_param_spec_string ("distro-version", "distro-version", "distro-version", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}


static void
as_distro_details_instance_init (AsDistroDetails * self)
{
	self->priv = AS_DISTRO_DETAILS_GET_PRIVATE (self);
}


static void
as_distro_details_finalize (GObject* obj)
{
	AsDistroDetails * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_DISTRO_DETAILS, AsDistroDetails);
	g_free (self->priv->distro_id);
	g_free (self->priv->distro_name);
	g_free (self->priv->distro_version);
	g_key_file_unref (self->priv->keyf);
	G_OBJECT_CLASS (as_distro_details_parent_class)->finalize (obj);
}


/**
 * as_distro_details_get_type:
 *
 * Get details about the AppStream settings for the
 * current distribution
 */
GType
as_distro_details_get_type (void)
{
	static volatile gsize as_distro_details_type_id__volatile = 0;
	if (g_once_init_enter (&as_distro_details_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
					sizeof (AsDistroDetailsClass),
					(GBaseInitFunc) NULL,
					(GBaseFinalizeFunc) NULL,
					(GClassInitFunc) as_distro_details_class_init,
					(GClassFinalizeFunc) NULL,
					NULL,
					sizeof (AsDistroDetails),
					0,
					(GInstanceInitFunc) as_distro_details_instance_init,
					NULL
		};
		GType as_distro_details_type_id;
		as_distro_details_type_id = g_type_register_static (G_TYPE_OBJECT, "AsDistroDetails", &g_define_type_info, 0);
		g_once_init_leave (&as_distro_details_type_id__volatile, as_distro_details_type_id);
	}
	return as_distro_details_type_id__volatile;
}


static void
as_distro_details_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	AsDistroDetails * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_DISTRO_DETAILS, AsDistroDetails);
	switch (property_id) {
		case AS_DISTRO_DETAILS_DISTRO_ID:
			g_value_set_string (value, as_distro_details_get_distro_id (self));
			break;
		case AS_DISTRO_DETAILS_DISTRO_NAME:
			g_value_set_string (value, as_distro_details_get_distro_name (self));
			break;
		case AS_DISTRO_DETAILS_DISTRO_VERSION:
			g_value_set_string (value, as_distro_details_get_distro_version (self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void
as_distro_details_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	AsDistroDetails * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_DISTRO_DETAILS, AsDistroDetails);
	switch (property_id) {
		case AS_DISTRO_DETAILS_DISTRO_ID:
			as_distro_details_set_distro_id (self, g_value_get_string (value));
			break;
		case AS_DISTRO_DETAILS_DISTRO_NAME:
			as_distro_details_set_distro_name (self, g_value_get_string (value));
			break;
		case AS_DISTRO_DETAILS_DISTRO_VERSION:
			as_distro_details_set_distro_version (self, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}
