/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2016-2020 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_CONTENT_RATING_H
#define __AS_CONTENT_RATING_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_CONTENT_RATING (as_content_rating_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsContentRating, as_content_rating, AS, CONTENT_RATING, GObject)

struct _AsContentRatingClass
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
 * AsContentRatingSystem:
 * @AS_CONTENT_RATING_SYSTEM_UNKNOWN: Unknown ratings system
 * @AS_CONTENT_RATING_SYSTEM_INCAA: INCAA
 * @AS_CONTENT_RATING_SYSTEM_ACB: ACB
 * @AS_CONTENT_RATING_SYSTEM_DJCTQ: DJCTQ
 * @AS_CONTENT_RATING_SYSTEM_GSRR: GSRR
 * @AS_CONTENT_RATING_SYSTEM_PEGI: PEGI
 * @AS_CONTENT_RATING_SYSTEM_KAVI: KAVI
 * @AS_CONTENT_RATING_SYSTEM_USK: USK
 * @AS_CONTENT_RATING_SYSTEM_ESRA: ESRA
 * @AS_CONTENT_RATING_SYSTEM_CERO: CERO
 * @AS_CONTENT_RATING_SYSTEM_OFLCNZ: OFLCNZ
 * @AS_CONTENT_RATING_SYSTEM_RUSSIA: Russia
 * @AS_CONTENT_RATING_SYSTEM_MDA: MDA
 * @AS_CONTENT_RATING_SYSTEM_GRAC: GRAC
 * @AS_CONTENT_RATING_SYSTEM_ESRB: ESRB
 * @AS_CONTENT_RATING_SYSTEM_IARC: IARC
 *
 * A content rating system for a particular territory.
 *
 * Since: 0.12.12
 */
typedef enum {
	AS_CONTENT_RATING_SYSTEM_UNKNOWN,
	AS_CONTENT_RATING_SYSTEM_INCAA,
	AS_CONTENT_RATING_SYSTEM_ACB,
	AS_CONTENT_RATING_SYSTEM_DJCTQ,
	AS_CONTENT_RATING_SYSTEM_GSRR,
	AS_CONTENT_RATING_SYSTEM_PEGI,
	AS_CONTENT_RATING_SYSTEM_KAVI,
	AS_CONTENT_RATING_SYSTEM_USK,
	AS_CONTENT_RATING_SYSTEM_ESRA,
	AS_CONTENT_RATING_SYSTEM_CERO,
	AS_CONTENT_RATING_SYSTEM_OFLCNZ,
	AS_CONTENT_RATING_SYSTEM_RUSSIA,
	AS_CONTENT_RATING_SYSTEM_MDA,
	AS_CONTENT_RATING_SYSTEM_GRAC,
	AS_CONTENT_RATING_SYSTEM_ESRB,
	AS_CONTENT_RATING_SYSTEM_IARC,
	/*< private >*/
	AS_CONTENT_RATING_SYSTEM_LAST
} AsContentRatingSystem;

/**
 * AsContentRatingValue:
 * @AS_CONTENT_RATING_VALUE_UNKNOWN:		Unknown value
 * @AS_CONTENT_RATING_VALUE_NONE:		None
 * @AS_CONTENT_RATING_VALUE_MILD:		A small amount
 * @AS_CONTENT_RATING_VALUE_MODERATE:		A moderate amount
 * @AS_CONTENT_RATING_VALUE_INTENSE:		An intense amount
 *
 * The specified level of an content_rating rating ID.
 **/
typedef enum {
	AS_CONTENT_RATING_VALUE_UNKNOWN,
	AS_CONTENT_RATING_VALUE_NONE,
	AS_CONTENT_RATING_VALUE_MILD,
	AS_CONTENT_RATING_VALUE_MODERATE,
	AS_CONTENT_RATING_VALUE_INTENSE,
	/*< private >*/
	AS_CONTENT_RATING_VALUE_LAST
} AsContentRatingValue;

const gchar		*as_content_rating_value_to_string (AsContentRatingValue value);
AsContentRatingValue	 as_content_rating_value_from_string (const gchar *value);

guint			as_content_rating_attribute_to_csm_age (const gchar *id,
								AsContentRatingValue value);

const gchar		**as_content_rating_get_all_rating_ids (void);

const gchar		*as_content_rating_system_to_string (AsContentRatingSystem system);
gchar			*as_content_rating_system_format_age (AsContentRatingSystem system,
								guint age);

AsContentRatingSystem	as_content_rating_system_from_locale (const gchar *locale);

gchar			**as_content_rating_system_get_formatted_ages (AsContentRatingSystem system);
const guint	 	*as_content_rating_system_get_csm_ages (AsContentRatingSystem system,
								gsize *length_out);

AsContentRatingValue	as_content_rating_attribute_from_csm_age (const gchar *id,
								  guint age);
const gchar		*as_content_rating_attribute_get_description (const gchar *id,
								      AsContentRatingValue value);

AsContentRating		*as_content_rating_new (void);

const gchar		*as_content_rating_get_kind (AsContentRating *content_rating);
void			as_content_rating_set_kind (AsContentRating *content_rating,
						    const gchar *kind);

guint			as_content_rating_get_minimum_age (AsContentRating *content_rating);

AsContentRatingValue	as_content_rating_get_value (AsContentRating *content_rating,
						     const gchar *id);
void			as_content_rating_set_value (AsContentRating *content_rating,
						     const gchar *id,
						     AsContentRatingValue value);

const gchar		**as_content_rating_get_rating_ids (AsContentRating *content_rating);

void			as_content_rating_add_attribute (AsContentRating *content_rating,
							const gchar *id,
							AsContentRatingValue value);

G_END_DECLS

#endif /* __AS_CONTENT_RATING_H */
