/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_RELATION_H
#define __AS_RELATION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_RELATION (as_relation_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsRelation, as_relation, AS, RELATION, GObject)

struct _AsRelationClass
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
 * AsRelationKind:
 * @AS_RELATION_KIND_UNKNOWN:		Unknown kind
 * @AS_RELATION_KIND_REQUIRES:		The referenced item is required by the component
 * @AS_RELATION_KIND_RECOMMENDS:	The referenced item is recommended
 * @AS_RELATION_KIND_SUPPORTS:		The referenced item is supported
 *
 * Type of a component's relation to other items.
 **/
typedef enum  {
	AS_RELATION_KIND_UNKNOWN,
	AS_RELATION_KIND_REQUIRES,
	AS_RELATION_KIND_RECOMMENDS,
	AS_RELATION_KIND_SUPPORTS,
	/*< private >*/
	AS_RELATION_KIND_LAST
} AsRelationKind;

/**
 * AsRelationItemKind:
 * @AS_RELATION_ITEM_KIND_UNKNOWN:		Unknown kind
 * @AS_RELATION_ITEM_KIND_ID:			A component ID
 * @AS_RELATION_ITEM_KIND_MODALIAS:		A hardware modalias
 * @AS_RELATION_ITEM_KIND_KERNEL:		An operating system kernel (like Linux)
 * @AS_RELATION_ITEM_KIND_MEMORY:		A system RAM requirement
 * @AS_RELATION_ITEM_KIND_FIRMWARE:		A device firmware requirement (used by fwupd)
 * @AS_RELATION_ITEM_KIND_CONTROL:		An input method for users to control software
 * @AS_RELATION_ITEM_KIND_DISPLAY_LENGTH:	Display edge length
 * @AS_RELATION_ITEM_KIND_HARDWARE:		A Computer Hardware ID (CHID) to depend on system hardware
 * @AS_RELATION_ITEM_KIND_INTERNET:		Internet connectivity (Since: 0.15.5)
 *
 * Type of the item an #AsRelation is for.
 **/
typedef enum  {
	AS_RELATION_ITEM_KIND_UNKNOWN,
	AS_RELATION_ITEM_KIND_ID,
	AS_RELATION_ITEM_KIND_MODALIAS,
	AS_RELATION_ITEM_KIND_KERNEL,
	AS_RELATION_ITEM_KIND_MEMORY,
	AS_RELATION_ITEM_KIND_FIRMWARE,
	AS_RELATION_ITEM_KIND_CONTROL,
	AS_RELATION_ITEM_KIND_DISPLAY_LENGTH,
	AS_RELATION_ITEM_KIND_HARDWARE,
	AS_RELATION_ITEM_KIND_INTERNET,
	/*< private >*/
	AS_RELATION_ITEM_KIND_LAST
} AsRelationItemKind;

/**
 * AsRelationCompare:
 * @AS_RELATION_COMPARE_UNKNOWN:	Comparison predicate invalid or not known
 * @AS_RELATION_COMPARE_EQ:		Equal to
 * @AS_RELATION_COMPARE_NE:		Not equal to
 * @AS_RELATION_COMPARE_LT:		Less than
 * @AS_RELATION_COMPARE_GT:		Greater than
 * @AS_RELATION_COMPARE_LE:		Less than or equal to
 * @AS_RELATION_COMPARE_GE:		Greater than or equal to
 *
 * The relational comparison type.
 **/
typedef enum {
	AS_RELATION_COMPARE_UNKNOWN,
	AS_RELATION_COMPARE_EQ,
	AS_RELATION_COMPARE_NE,
	AS_RELATION_COMPARE_LT,
	AS_RELATION_COMPARE_GT,
	AS_RELATION_COMPARE_LE,
	AS_RELATION_COMPARE_GE,
	/*< private >*/
	AS_RELATION_COMPARE_LAST
} AsRelationCompare;

/**
 * AsControlKind:
 * @AS_CONTROL_KIND_UNKNOWN:	Unknown kind
 * @AS_CONTROL_KIND_POINTING:	Mouse/cursors/other pointing device
 * @AS_CONTROL_KIND_KEYBOARD:	Keyboard input
 * @AS_CONTROL_KIND_CONSOLE:	Console / command-line interface
 * @AS_CONTROL_KIND_TOUCH:	Touch input
 * @AS_CONTROL_KIND_GAMEPAD:	Gamepad input (any game controller with wheels/buttons/joysticks)
 * @AS_CONTROL_KIND_VOICE:	Control via voice recognition/activation
 * @AS_CONTROL_KIND_VISION:	Computer vision / visual object and sign detection
 * @AS_CONTROL_KIND_TV_REMOTE:	Input via a television remote
 * @AS_CONTROL_KIND_TABLET:	Graphics tablet input
 *
 * Kind of an input method for users to control software
 **/
typedef enum {
	AS_CONTROL_KIND_UNKNOWN,
	AS_CONTROL_KIND_POINTING,
	AS_CONTROL_KIND_KEYBOARD,
	AS_CONTROL_KIND_CONSOLE,
	AS_CONTROL_KIND_TOUCH,
	AS_CONTROL_KIND_GAMEPAD,
	AS_CONTROL_KIND_VOICE,
	AS_CONTROL_KIND_VISION,
	AS_CONTROL_KIND_TV_REMOTE,
	AS_CONTROL_KIND_TABLET,
	/*< private >*/
	AS_CONTROL_KIND_LAST
} AsControlKind;

/**
 * AsDisplaySideKind:
 * @AS_DISPLAY_SIDE_KIND_UNKNOWN:	Unknown
 * @AS_DISPLAY_SIDE_KIND_SHORTEST:	Shortest side of the display rectangle.
 * @AS_DISPLAY_SIDE_KIND_LONGEST:	Longest side of the display rectangle.
 *
 * Side a display_length requirement is for.
 **/
typedef enum  {
	AS_DISPLAY_SIDE_KIND_UNKNOWN,
	AS_DISPLAY_SIDE_KIND_SHORTEST,
	AS_DISPLAY_SIDE_KIND_LONGEST,
	/*< private >*/
	AS_DISPLAY_SIDE_KIND_LAST
} AsDisplaySideKind;

/**
 * AsDisplayLengthKind:
 * @AS_DISPLAY_LENGTH_KIND_UNKNOWN:	Unknown
 * @AS_DISPLAY_LENGTH_KIND_XSMALL:	Very small display
 * @AS_DISPLAY_LENGTH_KIND_SMALL:	Small display
 * @AS_DISPLAY_LENGTH_KIND_MEDIUM:	Medium display
 * @AS_DISPLAY_LENGTH_KIND_LARGE:	Large display
 * @AS_DISPLAY_LENGTH_KIND_XLARGE:	Very large display
 *
 * A rough estimate of how large a given display length is.
 **/
typedef enum  {
	AS_DISPLAY_LENGTH_KIND_UNKNOWN,
	AS_DISPLAY_LENGTH_KIND_XSMALL,
	AS_DISPLAY_LENGTH_KIND_SMALL,
	AS_DISPLAY_LENGTH_KIND_MEDIUM,
	AS_DISPLAY_LENGTH_KIND_LARGE,
	AS_DISPLAY_LENGTH_KIND_XLARGE,
	/*< private >*/
	AS_DISPLAY_LENGTH_KIND_LAST
} AsDisplayLengthKind;

/**
 * AsInternetKind:
 * @AS_INTERNET_KIND_UNKNOWN:		Unknown
 * @AS_INTERNET_KIND_ALWAYS:		Always requires/recommends internet
 * @AS_INTERNET_KIND_OFFLINE_ONLY:	Application is offline-only
 * @AS_INTERNET_KIND_FIRST_RUN:	Requires/Recommends internet on first run only
 *
 * Different internet connectivity requirements or recommendations for an
 * application.
 *
 * Since: 0.15.5
 **/
typedef enum  {
	AS_INTERNET_KIND_UNKNOWN,
	AS_INTERNET_KIND_ALWAYS,
	AS_INTERNET_KIND_OFFLINE_ONLY,
	AS_INTERNET_KIND_FIRST_RUN,
	/*< private >*/
	AS_INTERNET_KIND_LAST
} AsInternetKind;

const gchar		*as_relation_kind_to_string (AsRelationKind kind);
AsRelationKind		as_relation_kind_from_string (const gchar *kind_str);

const gchar		*as_relation_item_kind_to_string (AsRelationItemKind kind);
AsRelationItemKind	as_relation_item_kind_from_string (const gchar *kind_str);

AsRelationCompare	as_relation_compare_from_string (const gchar *compare_str);
const gchar		*as_relation_compare_to_string (AsRelationCompare compare);
const gchar		*as_relation_compare_to_symbols_string (AsRelationCompare compare);

const gchar		*as_control_kind_to_string (AsControlKind kind);
AsControlKind		as_control_kind_from_string (const gchar *kind_str);

const gchar		*as_display_side_kind_to_string (AsDisplaySideKind kind);
AsDisplaySideKind	as_display_side_kind_from_string (const gchar *kind_str);

const gchar		*as_display_length_kind_to_string (AsDisplayLengthKind kind);
AsDisplayLengthKind	as_display_length_kind_from_string (const gchar *kind_str);

const gchar		*as_internet_kind_to_string (AsInternetKind kind);
AsInternetKind		as_internet_kind_from_string (const gchar *kind_str);

AsRelation		*as_relation_new (void);

AsRelationKind		as_relation_get_kind (AsRelation *relation);
void			as_relation_set_kind (AsRelation *relation,
						AsRelationKind kind);

AsRelationItemKind	as_relation_get_item_kind (AsRelation *relation);
void			as_relation_set_item_kind (AsRelation *relation,
						   AsRelationItemKind kind);

AsRelationCompare	as_relation_get_compare (AsRelation *relation);
void			as_relation_set_compare (AsRelation *relation,
						 AsRelationCompare compare);

const gchar		*as_relation_get_version (AsRelation *relation);
void			as_relation_set_version (AsRelation *relation,
						  const gchar *version);

const gchar		*as_relation_get_value_str (AsRelation *relation);
void			as_relation_set_value_str (AsRelation *relation,
						   const gchar *value);

gint			as_relation_get_value_int (AsRelation *relation);
void			as_relation_set_value_int (AsRelation *relation,
						   gint value);

AsControlKind		as_relation_get_value_control_kind (AsRelation *relation);
void			as_relation_set_value_control_kind (AsRelation *relation,
							    AsControlKind kind);

AsDisplaySideKind	as_relation_get_display_side_kind (AsRelation *relation);
void			as_relation_set_display_side_kind (AsRelation *relation,
							   AsDisplaySideKind kind);

gint			as_relation_get_value_px (AsRelation *relation);
void			as_relation_set_value_px (AsRelation *relation,
						  gint logical_px);
AsDisplayLengthKind	as_relation_get_value_display_length_kind (AsRelation *relation);
void			as_relation_set_value_display_length_kind (AsRelation *relation,
								   AsDisplayLengthKind kind);

AsInternetKind		as_relation_get_value_internet_kind (AsRelation *relation);
void			as_relation_set_value_internet_kind (AsRelation *relation,
							     AsInternetKind kind);
guint			as_relation_get_value_internet_bandwidth (AsRelation *relation);
void			as_relation_set_value_internet_bandwidth (AsRelation *relation,
								  guint bandwidth_mbitps);

gboolean		as_relation_version_compare (AsRelation *relation,
						     const gchar *version,
						     GError **error);

/* DEPRECATED */

G_DEPRECATED
const gchar		*as_relation_get_value (AsRelation *relation);
G_DEPRECATED
void			as_relation_set_value (AsRelation *relation,
					        const gchar *value);

G_END_DECLS

#endif /* __AS_RELATION_H */
