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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_COMPONENT_H
#define __AS_COMPONENT_H

#include <glib-object.h>
#include "as-screenshot.h"
#include "as-provides.h"

#define AS_TYPE_COMPONENT_KIND (as_component_kind_get_type ())

#define AS_TYPE_COMPONENT (as_component_get_type ())
#define AS_COMPONENT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_COMPONENT, AsComponent))
#define AS_COMPONENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_COMPONENT, AsComponentClass))
#define AS_IS_COMPONENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_COMPONENT))
#define AS_IS_COMPONENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_COMPONENT))
#define AS_COMPONENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_COMPONENT, AsComponentClass))

G_BEGIN_DECLS

typedef struct _AsComponent AsComponent;
typedef struct _AsComponentClass AsComponentClass;
typedef struct _AsComponentPrivate AsComponentPrivate;

struct _AsComponent {
	GObject parent_instance;
	AsComponentPrivate * priv;
};

struct _AsComponentClass {
	GObjectClass parent_class;
};

typedef enum  {
	AS_COMPONENT_KIND_UNKNOWN,
	AS_COMPONENT_KIND_GENERIC,
	AS_COMPONENT_KIND_DESKTOP_APP,
	AS_COMPONENT_KIND_FONT,
	AS_COMPONENT_KIND_CODEC,
	AS_COMPONENT_KIND_INPUTMETHOD,
	AS_COMPONENT_KIND_LAST
} AsComponentKind;

GType				as_component_kind_get_type (void) G_GNUC_CONST;
const gchar*		as_component_kind_to_string (AsComponentKind kind);
AsComponentKind		as_component_kind_from_string (const gchar *kind_str);

GType				as_component_get_type (void) G_GNUC_CONST;
AsComponent*		as_component_new (void);
AsComponent*		as_component_construct (GType object_type);
gboolean			as_component_is_valid (AsComponent* self);
gchar* 				as_component_to_string (AsComponent* self);

gboolean			as_component_provides_item (AsComponent *self, AsProvidesKind kind, const gchar *value);

AsComponentKind		as_component_get_kind (AsComponent* self);
const gchar*		as_component_get_pkgname (AsComponent* self);
const gchar*		as_component_get_idname (AsComponent* self);
const gchar*		as_component_get_name (AsComponent* self);
const gchar* 		as_component_get_name_original (AsComponent* self);
const gchar* 		as_component_get_project_license (AsComponent* self);
const gchar* 		as_component_get_project_group (AsComponent* self);
gchar**				as_component_get_compulsory_for_desktops (AsComponent* self);
const gchar*		as_component_get_summary (AsComponent* self);
gchar**				as_component_get_categories (AsComponent* self);
GPtrArray*			as_component_get_screenshots (AsComponent* self);
const gchar*		as_component_get_description (AsComponent* self);
gchar**				as_component_get_keywords (AsComponent* self);
const gchar*		as_component_get_icon (AsComponent* self);
const gchar*		as_component_get_icon_url (AsComponent* self);
const gchar*		as_component_get_homepage (AsComponent* self);
gchar**				as_component_get_mimetypes (AsComponent* self);
GPtrArray*			as_component_get_provides (AsComponent* self);

void				as_component_add_screenshot (AsComponent* self, AsScreenshot* sshot);
void				as_component_set_categories_from_str (AsComponent* self, const gchar* categories_str);
void				as_component_set_kind (AsComponent* self, AsComponentKind value);
void				as_component_set_name (AsComponent* self, const gchar* value);
void				as_component_set_keywords (AsComponent* self, gchar** value);
void				as_component_set_mimetypes (AsComponent* self, gchar** value);
void				as_component_set_compulsory_for_desktops (AsComponent* self, gchar** value);
void				as_component_set_pkgname (AsComponent* self, const gchar* value);
void				as_component_set_idname (AsComponent* self, const gchar* value);
void				as_component_set_name_original (AsComponent* self, const gchar* value);
void				as_component_set_summary (AsComponent* self, const gchar* value);
void				as_component_set_description (AsComponent* self, const gchar* value);
void				as_component_set_homepage (AsComponent* self, const gchar* value);
void				as_component_set_icon (AsComponent* self, const gchar* value);
void				as_component_set_icon_url (AsComponent* self, const gchar* value);
void				as_component_set_project_license (AsComponent* self, const gchar* value);
void				as_component_set_project_group (AsComponent* self, const gchar* value);
void				as_component_set_categories (AsComponent* self, gchar** value);

G_END_DECLS

#endif /* __AS_COMPONENT_H */
