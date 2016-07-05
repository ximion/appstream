/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-enums.h"
#include "as-provided.h"
#include "as-icon.h"
#include "as-screenshot.h"
#include "as-release.h"
#include "as-translation.h"
#include "as-suggested.h"

G_BEGIN_DECLS

#define AS_TYPE_COMPONENT (as_component_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsComponent, as_component, AS, COMPONENT, GObject)

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
 * @AS_COMPONENT_KIND_FONT:		A font
 * @AS_COMPONENT_KIND_CODEC:		A multimedia codec
 * @AS_COMPONENT_KIND_INPUTMETHOD:	An input-method provider
 * @AS_COMPONENT_KIND_ADDON:		An extension of existing software, which does not run standalone
 * @AS_COMPONENT_KIND_FIRMWARE:		Firmware
 *
 * The type of an #AsComponent.
 **/
typedef enum  {
	AS_COMPONENT_KIND_UNKNOWN,
	AS_COMPONENT_KIND_GENERIC,
	AS_COMPONENT_KIND_DESKTOP_APP,
	AS_COMPONENT_KIND_FONT,
	AS_COMPONENT_KIND_CODEC,
	AS_COMPONENT_KIND_INPUTMETHOD,
	AS_COMPONENT_KIND_ADDON,
	AS_COMPONENT_KIND_FIRMWARE,
	/*< private >*/
	AS_COMPONENT_KIND_LAST
} AsComponentKind;

#define AS_TYPE_COMPONENT_KIND (as_component_kind_get_type ())

GType			as_component_kind_get_type (void) G_GNUC_CONST;
const gchar		*as_component_kind_to_string (AsComponentKind kind);
AsComponentKind		as_component_kind_from_string (const gchar *kind_str);

AsComponent		*as_component_new (void);

gboolean		as_component_is_valid (AsComponent *cpt);
gchar			*as_component_to_string (AsComponent *cpt);

gchar			*as_component_get_active_locale (AsComponent *cpt);
void			as_component_set_active_locale (AsComponent *cpt,
							const gchar *locale);

AsComponentKind		as_component_get_kind (AsComponent *cpt);
void			as_component_set_kind (AsComponent *cpt,
						AsComponentKind value);

const gchar		*as_component_get_id (AsComponent *cpt);
void			as_component_set_id (AsComponent *cpt,
						const gchar *value);

const gchar		*as_component_get_origin (AsComponent *cpt);
void			as_component_set_origin (AsComponent *cpt,
							const gchar *origin);

gchar			**as_component_get_pkgnames (AsComponent *cpt);
void			as_component_set_pkgnames (AsComponent *cpt,
							gchar **value);

const gchar		*as_component_get_source_pkgname (AsComponent *cpt);
void			as_component_set_source_pkgname (AsComponent *cpt,
								const gchar* spkgname);

const gchar		*as_component_get_name (AsComponent *cpt);
void			as_component_set_name (AsComponent *cpt,
						const gchar *value,
						const gchar *locale);

const gchar		*as_component_get_summary (AsComponent *cpt);
void			as_component_set_summary (AsComponent *cpt,
							const gchar *value,
							const gchar *locale);

const gchar		*as_component_get_description (AsComponent *cpt);
void			as_component_set_description (AsComponent *cpt,
							const gchar *value,
							const gchar *locale);

const gchar		*as_component_get_project_license (AsComponent *cpt);
void			as_component_set_project_license (AsComponent *cpt,
								const gchar *value);

const gchar		*as_component_get_project_group (AsComponent *cpt);
void			as_component_set_project_group (AsComponent *cpt,
								const gchar *value);

const gchar		*as_component_get_developer_name (AsComponent *cpt);
void			as_component_set_developer_name (AsComponent *cpt,
								const gchar *value,
								const gchar *locale);

gchar			**as_component_get_compulsory_for_desktops (AsComponent *cpt);
void			as_component_set_compulsory_for_desktops (AsComponent *cpt,
									gchar **value);
gboolean		as_component_is_compulsory_for_desktop (AsComponent *cpt,
									const gchar *desktop);

gchar			**as_component_get_categories (AsComponent *cpt);
void			as_component_set_categories (AsComponent *cpt,
							gchar **value);
gboolean		as_component_has_category (AsComponent *cpt,
							const gchar *category);

GPtrArray		*as_component_get_screenshots (AsComponent *cpt);
void			as_component_add_screenshot (AsComponent *cpt,
							AsScreenshot *sshot);

GPtrArray		*as_component_get_suggestions (AsComponent *cpt);
void			as_component_add_suggestion (AsComponent *cpt,
							AsSuggested *suggested);

gchar			**as_component_get_keywords (AsComponent *cpt);
void			as_component_set_keywords (AsComponent *cpt,
							gchar **value,
							const gchar *locale);

GPtrArray		*as_component_get_icons (AsComponent *cpt);
AsIcon			*as_component_get_icon_by_size (AsComponent *cpt,
							guint width,
							guint height);
void			as_component_add_icon (AsComponent *cpt,
						AsIcon *icon);

void			as_component_add_provided (AsComponent *cpt,
							AsProvided *prov);
AsProvided		*as_component_get_provided_for_kind (AsComponent *cpt,
							AsProvidedKind kind);
GList			*as_component_get_provided (AsComponent *cpt);

const gchar		*as_component_get_url (AsComponent *cpt,
						AsUrlKind url_kind);
void			as_component_add_url (AsComponent *cpt,
						AsUrlKind url_kind,
						const gchar *url);

GPtrArray		*as_component_get_releases (AsComponent *cpt);
void			as_component_add_release (AsComponent *cpt,
							AsRelease* release);

GPtrArray		*as_component_get_extends (AsComponent *cpt);
void			as_component_add_extends (AsComponent *cpt,
							const gchar *cpt_id);

GPtrArray		*as_component_get_extensions (AsComponent *cpt);
void			as_component_add_extension (AsComponent *cpt,
							const gchar *cpt_id);

GList			*as_component_get_languages (AsComponent *cpt);
gint			as_component_get_language (AsComponent *cpt,
							const gchar *locale);
void			as_component_add_language (AsComponent *cpt,
							const gchar *locale,
							gint percentage);

GPtrArray		*as_component_get_translations (AsComponent *cpt);
void			as_component_add_translation (AsComponent *cpt,
							AsTranslation *tr);

gboolean		as_component_has_bundle (AsComponent *cpt);
const gchar		*as_component_get_bundle_id (AsComponent *cpt,
							AsBundleKind bundle_kind);
void			as_component_add_bundle_id (AsComponent *cpt,
							AsBundleKind bundle_kind,
							const gchar *id);

GPtrArray		*as_component_get_search_tokens (AsComponent *cpt);
guint			 as_component_search_matches (AsComponent *cpt,
						      const gchar *search_term);

G_END_DECLS

#endif /* __AS_COMPONENT_H */
