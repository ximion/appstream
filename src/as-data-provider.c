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

#include "as-data-provider.h"
#include <glib.h>
#include <glib-object.h>

struct _AsDataProviderPrivate {
	gchar** watch_files;
};

static gpointer as_data_provider_parent_class = NULL;
#define AS_DATA_PROVIDER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_DATA_PROVIDER, AsDataProviderPrivate))

enum  {
	AS_DATA_PROVIDER_DUMMY_PROPERTY,
	AS_DATA_PROVIDER_WATCH_FILES
};

static gboolean as_data_provider_real_execute (AsDataProvider* self);
static void as_data_provider_finalize (GObject* obj);

static void as_data_provider_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void as_data_provider_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);

AsDataProvider* as_data_provider_construct (GType object_type) {
	AsDataProvider * self = NULL;
	self = (AsDataProvider*) g_object_new (object_type, NULL);
	return self;
}


void as_data_provider_emit_application (AsDataProvider* self, AsComponent* cpt) {
	g_return_if_fail (self != NULL);
	g_return_if_fail (cpt != NULL);
	g_signal_emit_by_name (self, "application", cpt);
}


static gboolean as_data_provider_real_execute (AsDataProvider* self) {
	g_critical ("Type `%s' does not implement abstract method `as_data_provider_execute'", g_type_name (G_TYPE_FROM_INSTANCE (self)));
	return FALSE;
}


gboolean as_data_provider_execute (AsDataProvider* self) {
	g_return_val_if_fail (self != NULL, FALSE);
	return AS_DATA_PROVIDER_GET_CLASS (self)->execute (self);
}


void as_data_provider_log_error (AsDataProvider* self, const gchar* msg) {
	g_return_if_fail (self != NULL);
	g_return_if_fail (msg != NULL);
	g_debug ("%s", msg);
}


void as_data_provider_log_warning (AsDataProvider* self, const gchar* msg) {
	g_return_if_fail (self != NULL);
	g_return_if_fail (msg != NULL);
	g_warning ("%s", msg);
}


gchar** as_data_provider_get_watch_files (AsDataProvider* self) {
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->watch_files;
}

void as_data_provider_set_watch_files (AsDataProvider* self, gchar** value) {
	self->priv->watch_files = g_strdupv (value);
	g_object_notify ((GObject *) self, "watch-files");
}


static void as_data_provider_class_init (AsDataProviderClass * klass) {
	as_data_provider_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsDataProviderPrivate));
	AS_DATA_PROVIDER_CLASS (klass)->execute = as_data_provider_real_execute;
	G_OBJECT_CLASS (klass)->get_property = as_data_provider_get_property;
	G_OBJECT_CLASS (klass)->set_property = as_data_provider_set_property;
	G_OBJECT_CLASS (klass)->finalize = as_data_provider_finalize;
	g_object_class_install_property (G_OBJECT_CLASS (klass),
				AS_DATA_PROVIDER_WATCH_FILES,
				g_param_spec_boxed ("watch-files", "watch-files", "watch-files",
						    G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_signal_new ("application", AS_TYPE_DATA_PROVIDER, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, AS_TYPE_COMPONENT);
}


static void as_data_provider_instance_init (AsDataProvider * self) {
	self->priv = AS_DATA_PROVIDER_GET_PRIVATE (self);
}


static void as_data_provider_finalize (GObject* obj) {
	AsDataProvider * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_DATA_PROVIDER, AsDataProvider);
	g_strfreev (self->priv->watch_files);
	G_OBJECT_CLASS (as_data_provider_parent_class)->finalize (obj);
}


GType as_data_provider_get_type (void) {
	static volatile gsize as_data_provider_type_id__volatile = 0;
	if (g_once_init_enter (&as_data_provider_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = { sizeof (AsDataProviderClass),
						(GBaseInitFunc) NULL,
						(GBaseFinalizeFunc) NULL,
						(GClassInitFunc) as_data_provider_class_init,
						(GClassFinalizeFunc) NULL,
						NULL,
						sizeof (AsDataProvider),
						0,
						(GInstanceInitFunc) as_data_provider_instance_init,
						NULL
		};
		GType as_data_provider_type_id;
		as_data_provider_type_id = g_type_register_static (G_TYPE_OBJECT, "AsDataProvider", &g_define_type_info, G_TYPE_FLAG_ABSTRACT);
		g_once_init_leave (&as_data_provider_type_id__volatile, as_data_provider_type_id);
	}
	return as_data_provider_type_id__volatile;
}


static void as_data_provider_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	AsDataProvider * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_DATA_PROVIDER, AsDataProvider);
	switch (property_id) {
		case AS_DATA_PROVIDER_WATCH_FILES:
			g_value_set_boxed (value, as_data_provider_get_watch_files (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void as_data_provider_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	AsDataProvider * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_DATA_PROVIDER, AsDataProvider);
	switch (property_id) {
		case AS_DATA_PROVIDER_WATCH_FILES:
		{
			gpointer boxed;
			boxed = g_value_get_boxed (value);
			as_data_provider_set_watch_files (self, boxed);
		}
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}
