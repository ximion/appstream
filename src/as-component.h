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
#include "as-release.h"
#include "as-enums.h"

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

struct _AsComponent
{
	GObject parent_instance;
	AsComponentPrivate * priv;
};

struct _AsComponentClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

/**
 * AsComponentKind:
 * @AS_COMPONENT_KIND_UNKNOWN:		Type invalid or not known
 * @AS_COMPONENT_KIND_GENERIC:		A generic (= without specialized type) component
 * @AS_COMPONENT_KIND_DESKTOP_APP:	An application with a .desktop-file
 * @AS_COMPONENT_KIND_FONT:			A font
 * @AS_COMPONENT_KIND_CODEC:		A multimedia codec
 * @AS_COMPONENT_KIND_INPUTMETHOD:	An input-method provider
 *
 * The URL type.
 **/
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

AsComponentKind		as_component_get_kind (AsComponent* self);
void				as_component_set_kind (AsComponent* self,
										   AsComponentKind value);

const gchar*		as_component_get_id (AsComponent* self);
void				as_component_set_id (AsComponent* self,
											 const gchar* value);

const gchar*		as_component_get_pkgname (AsComponent* self);
void				as_component_set_pkgname (AsComponent* self,
											  const gchar* value);

const gchar*		as_component_get_name (AsComponent* self);
void				as_component_set_name (AsComponent* self,
										   const gchar* value);
const gchar* 		as_component_get_name_original (AsComponent* self);
void				as_component_set_name_original (AsComponent* self,
													const gchar* value);

const gchar*		as_component_get_summary (AsComponent* self);
void				as_component_set_summary (AsComponent* self,
											  const gchar* value);

const gchar*		as_component_get_description (AsComponent* self);
void				as_component_set_description (AsComponent* self,
												  const gchar* value);

const gchar* 		as_component_get_project_license (AsComponent* self);
void				as_component_set_project_license (AsComponent* self,
													  const gchar* value);

const gchar* 		as_component_get_project_group (AsComponent* self);
void				as_component_set_project_group (AsComponent* self,
													const gchar* value);

gchar**				as_component_get_compulsory_for_desktops (AsComponent* self);
void				as_component_set_compulsory_for_desktops (AsComponent* self,
															  gchar** value);
gboolean			as_component_is_compulsory_for_desktop (AsComponent* self,
															  const gchar* desktop);

gchar**				as_component_get_categories (AsComponent* self);
void				as_component_set_categories (AsComponent* self,
												 gchar** value);
void				as_component_set_categories_from_str (AsComponent* self,
												const gchar* categories_str);
gboolean			as_component_has_category (AsComponent *self,
												const gchar *category);

GPtrArray*			as_component_get_screenshots (AsComponent* self);
void				as_component_add_screenshot (AsComponent* self,
												 AsScreenshot* sshot);

gchar**				as_component_get_keywords (AsComponent* self);
void				as_component_set_keywords (AsComponent* self,
											   gchar** value);

const gchar*		as_component_get_icon (AsComponent* self);
void				as_component_set_icon (AsComponent* self,
										   const gchar* value);
const gchar*		as_component_get_icon_url (AsComponent* self);
void				as_component_set_icon_url (AsComponent* self,
											   const gchar* value);

GPtrArray*			as_component_get_provided_items (AsComponent* self);
void				as_component_add_provided_item (AsComponent *self,
										AsProvidesKind kind,
										const gchar *value,
										const gchar *data);
gboolean			as_component_provides_item (AsComponent *self,
										AsProvidesKind kind,
										const gchar *value);

GHashTable*			as_component_get_urls (AsComponent *self);
const gchar*		as_component_get_url (AsComponent *self,
										  AsUrlKind url_kind);
void				as_component_add_url (AsComponent *self,
										AsUrlKind url_kind,
										const gchar *url);

GPtrArray*			as_component_get_releases (AsComponent* self);
void				as_component_add_release (AsComponent* self,
												 AsRelease* release);

/* deprecated */
G_GNUC_DEPRECATED const gchar*		as_component_get_homepage (AsComponent* self);
G_GNUC_DEPRECATED void				as_component_set_homepage (AsComponent* self,
											   const gchar* value);

G_GNUC_DEPRECATED const gchar*		as_component_get_idname (AsComponent* self);
G_GNUC_DEPRECATED void				as_component_set_idname (AsComponent* self,
											 const gchar* value);

G_END_DECLS

#endif /* __AS_COMPONENT_H */
