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

#define AS_TYPE_COMPONENT			(as_component_get_type())
#define AS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_COMPONENT, AsComponent))
#define AS_COMPONENT_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_COMPONENT, AsComponentClass))
#define AS_IS_COMPONENT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_COMPONENT))
#define AS_IS_COMPONENT_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_COMPONENT))
#define AS_COMPONENT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_COMPONENT, AsComponentClass))

G_BEGIN_DECLS

typedef struct _AsComponent		AsComponent;
typedef struct _AsComponentClass	AsComponentClass;

struct _AsComponent
{
	GObject			parent;
};

struct _AsComponentClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * AsComponentKind:
 * @AS_COMPONENT_KIND_UNKNOWN:		Type invalid or not known
 * @AS_COMPONENT_KIND_GENERIC:		A generic (= without specialized type) component
 * @AS_COMPONENT_KIND_DESKTOP_APP:	An application with a .desktop-file
 * @AS_COMPONENT_KIND_FONT:			A font
 * @AS_COMPONENT_KIND_CODEC:		A multimedia codec
 * @AS_COMPONENT_KIND_INPUTMETHOD:	An input-method provider
 * @AS_COMPONENT_KIND_ADDON:		An extension of existing software, which does not run standalone
 *
 * The URL type.
 **/
typedef enum  {
	AS_COMPONENT_KIND_UNKNOWN = 0,
	AS_COMPONENT_KIND_GENERIC = 1 << 0,
	AS_COMPONENT_KIND_DESKTOP_APP = 1 << 1,
	AS_COMPONENT_KIND_FONT = 1 << 2,
	AS_COMPONENT_KIND_CODEC = 1 << 3,
	AS_COMPONENT_KIND_INPUTMETHOD = 1 << 4,
	AS_COMPONENT_KIND_ADDON = 1 << 5,
	AS_COMPONENT_KIND_LAST = 7
} AsComponentKind;

GType				as_component_kind_get_type (void) G_GNUC_CONST;
const gchar			*as_component_kind_to_string (AsComponentKind kind);
AsComponentKind		as_component_kind_from_string (const gchar *kind_str);

GType				as_component_get_type (void) G_GNUC_CONST;
AsComponent			*as_component_new (void);

gboolean			as_component_is_valid (AsComponent *cpt);
gchar				*as_component_to_string (AsComponent *cpt);

gchar				*as_component_get_active_locale (AsComponent *cpt);
void				as_component_set_active_locale (AsComponent *cpt,
												const gchar *locale);

AsComponentKind		as_component_get_kind (AsComponent *cpt);
void				as_component_set_kind (AsComponent *cpt,
										   AsComponentKind value);

const gchar			*as_component_get_id (AsComponent *cpt);
void				as_component_set_id (AsComponent *cpt,
											const gchar* value);

const gchar			*as_component_get_origin (AsComponent *cpt);
void				as_component_set_origin (AsComponent *cpt,
											const gchar* origin);

gchar				**as_component_get_pkgnames (AsComponent *cpt);
void				as_component_set_pkgnames (AsComponent *cpt,
											gchar **value);

const gchar			*as_component_get_name (AsComponent *cpt);
void				as_component_set_name (AsComponent *cpt,
											const gchar *value,
											const gchar *locale);

const gchar			*as_component_get_summary (AsComponent *cpt);
void				as_component_set_summary (AsComponent *cpt,
											const gchar *value,
											const gchar *locale);

const gchar			*as_component_get_description (AsComponent *cpt);
void				as_component_set_description (AsComponent *cpt,
											const gchar* value,
											const gchar *locale);

const gchar			*as_component_get_project_license (AsComponent *cpt);
void				as_component_set_project_license (AsComponent *cpt,
													const gchar* value);

const gchar			*as_component_get_project_group (AsComponent *cpt);
void				as_component_set_project_group (AsComponent *cpt,
													const gchar *value);

const gchar			*as_component_get_developer_name (AsComponent *cpt);
void				as_component_set_developer_name (AsComponent *cpt,
											const gchar *value,
											const gchar *locale);

gchar				**as_component_get_compulsory_for_desktops (AsComponent *cpt);
void				as_component_set_compulsory_for_desktops (AsComponent *cpt,
																gchar **value);
gboolean			as_component_is_compulsory_for_desktop (AsComponent *cpt,
																const gchar* desktop);

gchar				**as_component_get_categories (AsComponent *cpt);
void				as_component_set_categories (AsComponent *cpt,
												 gchar **value);
void				as_component_set_categories_from_str (AsComponent *cpt,
												const gchar* categories_str);
gboolean			as_component_has_category (AsComponent *cpt,
												const gchar *category);

GPtrArray			*as_component_get_screenshots (AsComponent *cpt);
void				as_component_add_screenshot (AsComponent *cpt,
												AsScreenshot* sshot);

gchar				**as_component_get_keywords (AsComponent *cpt);
void				as_component_set_keywords (AsComponent *cpt,
												gchar **value,
												const gchar *locale);

const gchar			*as_component_get_icon (AsComponent *cpt);
void				as_component_set_icon (AsComponent *cpt,
											const gchar* value);
const gchar			*as_component_get_icon_url (AsComponent *cpt,
											int width,
											int height);
void				as_component_add_icon_url (AsComponent *cpt,
											int width,
											int height,
											const gchar* value);
GHashTable			*as_component_get_icon_urls (AsComponent *cpt);

GPtrArray			*as_component_get_provided_items (AsComponent *cpt);
void				as_component_add_provided_item (AsComponent *cpt,
										AsProvidesKind kind,
										const gchar *value,
										const gchar *data);
gboolean			as_component_provides_item (AsComponent *cpt,
										AsProvidesKind kind,
										const gchar *value);

GHashTable			*as_component_get_urls (AsComponent *cpt);
const gchar			*as_component_get_url (AsComponent *cpt,
										  AsUrlKind url_kind);
void				as_component_add_url (AsComponent *cpt,
										AsUrlKind url_kind,
										const gchar *url);

GPtrArray			*as_component_get_releases (AsComponent *cpt);
void				as_component_add_release (AsComponent *cpt,
												AsRelease* release);

GPtrArray			*as_component_get_extends (AsComponent *cpt);
void				as_component_add_extends (AsComponent *cpt,
												const gchar *cpt_id);

void				as_component_add_language (AsComponent *cpt,
												const gchar *locale,
												gint percentage);
gint				as_component_get_language (AsComponent *cpt,
											   const gchar *locale);
GList*				as_component_get_languages (AsComponent *cpt);

GHashTable			*as_component_get_bundle_ids (AsComponent *cpt);
const gchar			*as_component_get_bundle_id (AsComponent *cpt,
										  AsBundleKind bundle_kind);
void				as_component_add_bundle_id (AsComponent *cpt,
										AsBundleKind bundle_kind,
										const gchar *id);

G_END_DECLS

#endif /* __AS_COMPONENT_H */
