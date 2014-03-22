/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#include "debian-dep11.h"
#include <glib.h>
#include <glib-object.h>


static gpointer as_provider_dep11_parent_class = NULL;
static gboolean as_provider_dep11_real_execute (AsDataProvider* base);

GType as_data_provider_get_type (void) G_GNUC_CONST;
GType as_provider_dep11_get_type (void) G_GNUC_CONST;

AsProviderDEP11* as_provider_dep11_construct (GType object_type) {
	AsProviderDEP11 * self = NULL;
	self = (AsProviderDEP11*) as_data_provider_construct (object_type);
	return self;
}


AsProviderDEP11* as_provider_dep11_new (void) {
	return as_provider_dep11_construct (AS_PROVIDER_TYPE_DEP11);
}


static gboolean as_provider_dep11_real_execute (AsDataProvider* base) {
	AsProviderDEP11 * self;
	gboolean result = FALSE;
	self = (AsProviderDEP11*) base;
	result = FALSE;
	return result;
}


static void as_provider_dep11_class_init (AsProviderDEP11Class * klass) {
	as_provider_dep11_parent_class = g_type_class_peek_parent (klass);
	AS_DATA_PROVIDER_CLASS (klass)->execute = as_provider_dep11_real_execute;
}


static void as_provider_dep11_instance_init (AsProviderDEP11 * self) {
}


GType as_provider_dep11_get_type (void) {
	static volatile gsize as_provider_dep11_type_id__volatile = 0;
	if (g_once_init_enter (&as_provider_dep11_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
					sizeof (AsProviderDEP11Class),
					(GBaseInitFunc) NULL,
					(GBaseFinalizeFunc) NULL,
					(GClassInitFunc) as_provider_dep11_class_init,
					(GClassFinalizeFunc) NULL,
					NULL,
					sizeof (AsProviderDEP11),
					0,
					(GInstanceInitFunc) as_provider_dep11_instance_init,
					NULL
		};
		GType as_provider_dep11_type_id;
		as_provider_dep11_type_id = g_type_register_static (AS_TYPE_DATA_PROVIDER, "AsProviderDEP11", &g_define_type_info, 0);
		g_once_init_leave (&as_provider_dep11_type_id__volatile, as_provider_dep11_type_id);
	}
	return as_provider_dep11_type_id__volatile;
}
