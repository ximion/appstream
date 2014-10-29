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

#include "ubuntu-appinstall.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>

#include "../as-menu-parser.h"
#include "../as-utils.h"
#include "../as-utils-private.h"

struct _AsProviderUbuntuAppinstallPrivate
{

};

static gpointer as_provider_ubuntu_appinstall_parent_class = NULL;

GType as_provider_ubuntu_appinstall_get_type (void) G_GNUC_CONST;
#define AS_PROVIDER_UBUNTU_APPINSTALL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_PROVIDER_TYPE_UBUNTU_APPINSTALL, AsProviderUbuntuAppinstallPrivate))

static gchar* as_provider_ubuntu_appinstall_desktop_file_get_str (AsProviderUbuntuAppinstall* self, GKeyFile* key_file, const gchar* key);
static void as_provider_ubuntu_appinstall_process_desktop_file (AsProviderUbuntuAppinstall* self, const gchar* fname);
static gboolean as_provider_ubuntu_appinstall_real_execute (AsDataProvider* base);
static void as_provider_ubuntu_appinstall_finalize (GObject* obj);

AsProviderUbuntuAppinstall*
as_provider_ubuntu_appinstall_construct (GType object_type)
{
	AsProviderUbuntuAppinstall * self = NULL;
	self = (AsProviderUbuntuAppinstall*) as_data_provider_construct (object_type);

	return self;
}


AsProviderUbuntuAppinstall*
as_provider_ubuntu_appinstall_new (void)
{
	return as_provider_ubuntu_appinstall_construct (AS_PROVIDER_TYPE_UBUNTU_APPINSTALL);
}


static gchar*
as_provider_ubuntu_appinstall_desktop_file_get_str (AsProviderUbuntuAppinstall* self, GKeyFile* key_file, const gchar* key)
{
	gchar* str;
	gchar* s;
	GError *error = NULL;
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (key_file != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	str = g_strdup ("");

	s = g_key_file_get_string (key_file, "Desktop Entry", key, &error);
	if (error != NULL)
		goto out;
	g_free (str);
	str = s;

out:
	if (error != NULL)
		g_error_free(error);
	return str;
}


static void
as_provider_ubuntu_appinstall_process_desktop_file (AsProviderUbuntuAppinstall* self, const gchar* fname)
{
	GError *error = NULL;
	GKeyFile *dfile;
	AsComponent *cpt;
	gchar **lines;
	gchar **strv;
	gchar *str;
	gchar *str2;

	gchar *desktop_file_name;
	gchar **cats;


	dfile = g_key_file_new ();
	g_key_file_load_from_file (dfile, fname, G_KEY_FILE_NONE, &error);
	if (error != NULL) {
		str = g_strdup_printf ("Error while loading file %s: %s", fname, error->message);
		as_data_provider_log_error ((AsDataProvider*) self, str);
		g_free (str);
		goto out;
	}

	/* a fresh component */
	cpt = as_component_new ();

	/* this data provider can only handle desktop apps, so every component is type:desktop-app */
	as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);

	/* get the base filename from Ubuntu AppInstall data */
	lines = g_strsplit (fname, ":", 2);
	desktop_file_name = g_strdup (lines[1]);
	g_strfreev (lines);
	if (as_str_empty (desktop_file_name)) {
		g_free (desktop_file_name);
		desktop_file_name = g_path_get_basename (fname);
	}
	as_component_set_id (cpt, desktop_file_name);
	g_free (desktop_file_name);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "X-AppInstall-Ignore");
	if (g_strcmp0 (str, "true") == 0) {
		g_free (str);
		goto out;
	}
	g_free (str);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "NoDisplay");
	if (g_strcmp0 (str, "true") == 0) {
		g_free (str);
		goto out;
	}
	g_free (str);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "X-AppInstall-Package");
	strv = g_new0 (gchar*, 2);
	strv[0] = str;
	as_component_set_pkgnames (cpt, strv);
	g_strfreev (strv);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "Name");
	as_component_set_name (cpt, str);
	g_free (str);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "Name");
	as_component_set_name_original (cpt, str);
	g_free (str);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "Comment");
	as_component_set_summary (cpt, str);
	g_free (str);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "Icon");
	as_component_set_icon (cpt, str);
	g_free (str);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "Categories");
	cats = g_strsplit (str, ";", 0);
	g_free (str);
	as_component_set_categories (cpt, cats);
	g_strfreev (cats);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "MimeType");
	if (!as_str_empty (str)) {
		guint i;
		gchar **mimes;
		mimes = g_strsplit (str, ";", 0);
		for (i = 0; mimes[i] != NULL; i++) {
			as_component_add_provided_item (cpt, AS_PROVIDES_KIND_MIMETYPE, mimes[i], "");
		}
		g_strfreev (mimes);
	}
	g_free (str);

	str = as_provider_ubuntu_appinstall_desktop_file_get_str (self, dfile, "OnlyShowIn");
	if (!as_str_empty (str)) {
		/* we cheat here and assume that if a .desktop file states that it should only be shown in desktop X, it
		 * is compulsory for that desktop (admittedly, that might be a lie in many cases...).
		 * Another way would be to simply ignore data which states being desktop-specific, but first try how this works out.
		 * (AppStream doesn't know a concept for desktop-specific apps, and we are still evaluating if that's a useful thing)
		 */
		gchar **compulsory;
		compulsory = g_strsplit (str, ";", 0);
		as_component_set_compulsory_for_desktops (cpt, compulsory);
		g_strfreev (compulsory);
	}
	g_free (str);


	if (as_component_is_valid (cpt)) {
		/* everything is fine with this component, we can emit it */
		as_data_provider_emit_component ((AsDataProvider*) self, cpt);
	} else {
		str = as_component_to_string (cpt);
		str2 = g_strdup_printf ("Invalid application found: %s\n", str);
		as_data_provider_log_warning ((AsDataProvider*) self, str2);
		g_free (str);
		g_free (str2);
	}
	g_object_unref (cpt);

out:
	g_key_file_unref (dfile);
	if (error != NULL)
		g_error_free (error);
}


static gboolean
as_provider_ubuntu_appinstall_real_execute (AsDataProvider *base)
{
	AsProviderUbuntuAppinstall * self;
	GPtrArray* desktop_files;
	guint i;
	gchar **paths;
	self = AS_PROVIDER_UBUNTU_APPINSTALL (base);

	paths = as_data_provider_get_watch_files (base);
	if ((paths == NULL) || (paths[0] == NULL))
		return TRUE;

	for (i = 0; paths[i] != NULL; i++) {
		gchar *fname;
		guint j;
		fname = g_build_filename (paths[i], "desktop", NULL);
		if (!g_file_test (fname, G_FILE_TEST_EXISTS)) {
			g_free (fname);
			continue;
		}

		desktop_files = as_utils_find_files_matching (fname, "*.desktop", FALSE);
		if (desktop_files == NULL) {
			g_free (fname);
			return FALSE;
		}

		for (j = 0; j < desktop_files->len; j++) {
				const gchar *path;
				path = (const gchar *) g_ptr_array_index (desktop_files, j);
				as_provider_ubuntu_appinstall_process_desktop_file (self, path);
		}
		g_ptr_array_unref (desktop_files);
		g_free (fname);
	}

	return TRUE;
}


static void
as_provider_ubuntu_appinstall_class_init (AsProviderUbuntuAppinstallClass * klass)
{
	as_provider_ubuntu_appinstall_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsProviderUbuntuAppinstallPrivate));
	AS_DATA_PROVIDER_CLASS (klass)->execute = as_provider_ubuntu_appinstall_real_execute;
	G_OBJECT_CLASS (klass)->finalize = as_provider_ubuntu_appinstall_finalize;
}


static void
as_provider_ubuntu_appinstall_instance_init (AsProviderUbuntuAppinstall * self)
{
	self->priv = AS_PROVIDER_UBUNTU_APPINSTALL_GET_PRIVATE (self);
}


static void
as_provider_ubuntu_appinstall_finalize (GObject* obj)
{
#if 0
	AsProviderUbuntuAppinstall * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_PROVIDER_TYPE_UBUNTU_APPINSTALL, AsProviderUbuntuAppinstall);
#endif

	G_OBJECT_CLASS (as_provider_ubuntu_appinstall_parent_class)->finalize (obj);
}


GType
as_provider_ubuntu_appinstall_get_type (void)
{
	static volatile gsize as_provider_ubuntu_appinstall_type_id__volatile = 0;
	if (g_once_init_enter (&as_provider_ubuntu_appinstall_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
							sizeof (AsProviderUbuntuAppinstallClass),
							(GBaseInitFunc) NULL,
							(GBaseFinalizeFunc) NULL,
							(GClassInitFunc) as_provider_ubuntu_appinstall_class_init,
							(GClassFinalizeFunc) NULL,
							NULL,
							sizeof (AsProviderUbuntuAppinstall),
							0,
							(GInstanceInitFunc) as_provider_ubuntu_appinstall_instance_init,
							NULL
		};
		GType as_provider_ubuntu_appinstall_type_id;
		as_provider_ubuntu_appinstall_type_id = g_type_register_static (AS_TYPE_DATA_PROVIDER, "AsProviderUbuntuAppinstall", &g_define_type_info, 0);
		g_once_init_leave (&as_provider_ubuntu_appinstall_type_id__volatile, as_provider_ubuntu_appinstall_type_id);
	}
	return as_provider_ubuntu_appinstall_type_id__volatile;
}
