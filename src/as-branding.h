/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_BRANDING_H
#define __AS_BRANDING_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_BRANDING (as_branding_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsBranding, as_branding, AS, BRANDING, GObject)

struct _AsBrandingClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

typedef struct
{
  /*< private >*/
  gpointer	dummy1;
  guint		dummy2;
  gpointer	dummy3;
  gpointer	dummy4;
  gpointer	dummy5;
  gpointer	dummy6;
} AsBrandingColorIter;

/**
 * AsColorKind:
 * @AS_COLOR_KIND_UNKNOWN:	Color type invalid or not known
 * @AS_COLOR_KIND_PRIMARY:	Primary accent color
 *
 * A branding color type.
 **/
typedef enum {
	AS_COLOR_KIND_UNKNOWN,
	AS_COLOR_KIND_PRIMARY,
	/*< private >*/
	AS_COLOR_KIND_LAST
} AsColorKind;

/**
 * AsColorSchemeKind:
 * @AS_COLOR_SCHEME_KIND_UNKNOWN:	Color scheme invalid or not known
 * @AS_COLOR_SCHEME_KIND_LIGHT:		A light color scheme
 * @AS_COLOR_SCHEME_KIND_DARK:		A dark color scheme
 *
 * A color scheme type.
 **/
typedef enum {
	AS_COLOR_SCHEME_KIND_UNKNOWN,
	AS_COLOR_SCHEME_KIND_LIGHT,
	AS_COLOR_SCHEME_KIND_DARK,
	/*< private >*/
	AS_COLOR_SCHEME_KIND_LAST
} AsColorSchemeKind;

const gchar		*as_color_kind_to_string (AsColorKind kind);
AsColorKind		as_color_kind_from_string (const gchar *str);

const gchar		*as_color_scheme_kind_to_string (AsColorSchemeKind kind);
AsColorSchemeKind	as_color_scheme_kind_from_string (const gchar *str);

AsBranding		*as_branding_new (void);

void		 	as_branding_set_color (AsBranding *branding,
					       AsColorKind kind,
					       AsColorSchemeKind scheme_preference,
					       const gchar *colorcode);
void		 	as_branding_remove_color (AsBranding *branding,
						  AsColorKind kind,
						  AsColorSchemeKind scheme_preference);

void			as_branding_color_iter_init (AsBrandingColorIter *iter,
						     AsBranding *branding);
gboolean		as_branding_color_iter_next (AsBrandingColorIter *iter,
							AsColorKind *kind,
							AsColorSchemeKind *scheme_preference,
							const gchar **value);

const gchar		*as_branding_get_color (AsBranding *branding,
						AsColorKind kind,
						AsColorSchemeKind scheme_kind);

G_END_DECLS

#endif /* __AS_BRANDING_H */
