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

#include "as-relation-private.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <glib.h>

#include "as-utils.h"
#include "as-vercmp.h"

#include "as-pool.h"
#include "as-system-info.h"

/**
 * SECTION:as-relation
 * @short_description: Description of relations a software component has with other things
 * @include: appstream.h
 *
 * A component can have recommends- or requires relations on other components, system properties,
 * hardware and other interfaces.
 * This class contains a representation of those relations.
 *
 * See also: #AsComponent
 */

typedef struct
{
	AsRelationKind kind;
	AsRelationItemKind item_kind;
	AsRelationCompare compare;

	GVariant *value;
	gchar *version;

	/* specific to "display_length" relations */
	AsDisplaySideKind display_side_kind;
	AsDisplayLengthKind display_length_kind;

	/* specific to "internet" relations */
	guint bandwidth_mbitps;
} AsRelationPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsRelation, as_relation, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_relation_get_instance_private (o))

/**
 * as_relation_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.16.0
 */
G_DEFINE_QUARK (as-relation-error-quark, as_relation_error)

/**
 * as_relation_kind_to_string:
 * @kind: the #AsRelationKind.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.12.0
 **/
const gchar*
as_relation_kind_to_string (AsRelationKind kind)
{
	if (kind == AS_RELATION_KIND_REQUIRES)
		return "requires";
	if (kind == AS_RELATION_KIND_RECOMMENDS)
		return "recommends";
	if (kind == AS_RELATION_KIND_SUPPORTS)
		return "supports";
	return "unknown";
}

/**
 * as_relation_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsRelationKind or %AS_RELATION_KIND_UNKNOWN for unknown
 *
 * Since: 0.12.0
 **/
AsRelationKind
as_relation_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "requires") == 0)
		return AS_RELATION_KIND_REQUIRES;
	if (g_strcmp0 (kind_str, "recommends") == 0)
		return AS_RELATION_KIND_RECOMMENDS;
	if (g_strcmp0 (kind_str, "supports") == 0)
		return AS_RELATION_KIND_SUPPORTS;
	return AS_RELATION_KIND_UNKNOWN;
}

/**
 * as_relation_item_kind_to_string:
 * @kind: the #AsRelationKind.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.12.0
 **/
const gchar*
as_relation_item_kind_to_string (AsRelationItemKind kind)
{
	if (kind == AS_RELATION_ITEM_KIND_ID)
		return "id";
	if (kind == AS_RELATION_ITEM_KIND_MODALIAS)
		return "modalias";
	if (kind == AS_RELATION_ITEM_KIND_KERNEL)
		return "kernel";
	if (kind == AS_RELATION_ITEM_KIND_MEMORY)
		return "memory";
	if (kind == AS_RELATION_ITEM_KIND_FIRMWARE)
		return "firmware";
	if (kind == AS_RELATION_ITEM_KIND_CONTROL)
		return "control";
	if (kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH)
		return "display_length";
	if (kind == AS_RELATION_ITEM_KIND_HARDWARE)
		return "hardware";
	if (kind == AS_RELATION_ITEM_KIND_INTERNET)
		return "internet";
	return "unknown";
}

/**
 * as_relation_item_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsRelationItemKind or %AS_RELATION_ITEM_KIND_UNKNOWN for unknown
 *
 * Since: 0.12.0
 **/
AsRelationItemKind
as_relation_item_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "id") == 0)
		return AS_RELATION_ITEM_KIND_ID;
	if (g_strcmp0 (kind_str, "modalias") == 0)
		return AS_RELATION_ITEM_KIND_MODALIAS;
	if (g_strcmp0 (kind_str, "kernel") == 0)
		return AS_RELATION_ITEM_KIND_KERNEL;
	if (g_strcmp0 (kind_str, "memory") == 0)
		return AS_RELATION_ITEM_KIND_MEMORY;
	if (g_strcmp0 (kind_str, "firmware") == 0)
		return AS_RELATION_ITEM_KIND_FIRMWARE;
	if (g_strcmp0 (kind_str, "control") == 0)
		return AS_RELATION_ITEM_KIND_CONTROL;
	if (g_strcmp0 (kind_str, "display_length") == 0)
		return AS_RELATION_ITEM_KIND_DISPLAY_LENGTH;
	if (g_strcmp0 (kind_str, "hardware") == 0)
		return AS_RELATION_ITEM_KIND_HARDWARE;
	if (g_strcmp0 (kind_str, "internet") == 0)
		return AS_RELATION_ITEM_KIND_INTERNET;
	return AS_RELATION_ITEM_KIND_UNKNOWN;
}

/**
 * as_relation_compare_from_string:
 * @compare_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsRelationCompare, or %AS_RELATION_COMPARE_UNKNOWN for unknown.
 *
 * Since: 0.12.0
 **/
AsRelationCompare
as_relation_compare_from_string (const gchar *compare_str)
{
	if (g_strcmp0 (compare_str, "eq") == 0)
		return AS_RELATION_COMPARE_EQ;
	if (g_strcmp0 (compare_str, "ne") == 0)
		return AS_RELATION_COMPARE_NE;
	if (g_strcmp0 (compare_str, "gt") == 0)
		return AS_RELATION_COMPARE_GT;
	if (g_strcmp0 (compare_str, "lt") == 0)
		return AS_RELATION_COMPARE_LT;
	if (g_strcmp0 (compare_str, "ge") == 0)
		return AS_RELATION_COMPARE_GE;
	if (g_strcmp0 (compare_str, "le") == 0)
		return AS_RELATION_COMPARE_LE;

	/* YAML */
	if (g_strcmp0 (compare_str, "==") == 0)
		return AS_RELATION_COMPARE_EQ;
	if (g_strcmp0 (compare_str, "!=") == 0)
		return AS_RELATION_COMPARE_NE;
	if (g_strcmp0 (compare_str, ">>") == 0)
		return AS_RELATION_COMPARE_GT;
	if (g_strcmp0 (compare_str, "<<") == 0)
		return AS_RELATION_COMPARE_LT;
	if (g_strcmp0 (compare_str, ">=") == 0)
		return AS_RELATION_COMPARE_GE;
	if (g_strcmp0 (compare_str, "<=") == 0)
		return AS_RELATION_COMPARE_LE;

	/* default value */
	if (compare_str == NULL)
		return AS_RELATION_COMPARE_GE;

	return AS_RELATION_COMPARE_UNKNOWN;
}

/**
 * as_relation_compare_to_string:
 * @compare: the #AsRelationCompare.
 *
 * Converts the enumerated value to an text representation.
 * The enum is converted into a two-letter identifier ("eq", "ge", etc.)
 * for use in the XML representation.
 *
 * Returns: string version of @compare
 *
 * Since: 0.12.0
 **/
const gchar*
as_relation_compare_to_string (AsRelationCompare compare)
{
	if (compare == AS_RELATION_COMPARE_EQ)
		return "eq";
	if (compare == AS_RELATION_COMPARE_NE)
		return "ne";
	if (compare == AS_RELATION_COMPARE_GT)
		return "gt";
	if (compare == AS_RELATION_COMPARE_LT)
		return "lt";
	if (compare == AS_RELATION_COMPARE_GE)
		return "ge";
	if (compare == AS_RELATION_COMPARE_LE)
		return "le";
	return NULL;
}

/**
 * as_relation_compare_to_symbols_string:
 * @compare: the #AsRelationCompare.
 *
 * Converts the enumerated value to an text representation.
 * The enum is converted into an identifier consisting of two
 * mathematical comparison operators ("==", ">=", etc.)
 * for use in the YAML representation and user interfaces.
 *
 * Returns: string version of @compare
 *
 * Since: 0.12.0
 **/
const gchar*
as_relation_compare_to_symbols_string (AsRelationCompare compare)
{
	if (compare == AS_RELATION_COMPARE_EQ)
		return "==";
	if (compare == AS_RELATION_COMPARE_NE)
		return "!=";
	if (compare == AS_RELATION_COMPARE_GT)
		return ">>";
	if (compare == AS_RELATION_COMPARE_LT)
		return "<<";
	if (compare == AS_RELATION_COMPARE_GE)
		return ">=";
	if (compare == AS_RELATION_COMPARE_LE)
		return "<=";
	return NULL;
}

/**
 * as_control_kind_to_string:
 * @kind: the #AsControlKind.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.12.11
 **/
const gchar*
as_control_kind_to_string (AsControlKind kind)
{
	if (kind == AS_CONTROL_KIND_POINTING)
		return "pointing";
	if (kind == AS_CONTROL_KIND_KEYBOARD)
		return "keyboard";
	if (kind == AS_CONTROL_KIND_CONSOLE)
		return "console";
	if (kind == AS_CONTROL_KIND_TOUCH)
		return "touch";
	if (kind == AS_CONTROL_KIND_GAMEPAD)
		return "gamepad";
	if (kind == AS_CONTROL_KIND_VOICE)
		return "voice";
	if (kind == AS_CONTROL_KIND_VISION)
		return "vision";
	if (kind == AS_CONTROL_KIND_TV_REMOTE)
		return "tv-remote";
	if (kind == AS_CONTROL_KIND_TABLET)
		return "tablet";
	return "unknown";
}

/**
 * as_control_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsControlKind or %AS_CONTROL_KIND_UNKNOWN for unknown
 *
 * Since: 0.12.11
 **/
AsControlKind
as_control_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "pointing") == 0)
		return AS_CONTROL_KIND_POINTING;
	if (g_strcmp0 (kind_str, "keyboard") == 0)
		return AS_CONTROL_KIND_KEYBOARD;
	if (g_strcmp0 (kind_str, "console") == 0)
		return AS_CONTROL_KIND_CONSOLE;
	if (g_strcmp0 (kind_str, "touch") == 0)
		return AS_CONTROL_KIND_TOUCH;
	if (g_strcmp0 (kind_str, "gamepad") == 0)
		return AS_CONTROL_KIND_GAMEPAD;
	if (g_strcmp0 (kind_str, "voice") == 0)
		return AS_CONTROL_KIND_VOICE;
	if (g_strcmp0 (kind_str, "vision") == 0)
		return AS_CONTROL_KIND_VISION;
	if (g_strcmp0 (kind_str, "tv-remote") == 0)
		return AS_CONTROL_KIND_TV_REMOTE;
	if (g_strcmp0 (kind_str, "tablet") == 0)
		return AS_CONTROL_KIND_TABLET;
	return AS_CONTROL_KIND_UNKNOWN;
}

/**
 * as_display_side_kind_to_string:
 * @kind: the #AsDisplaySideKind.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.12.12
 **/
const gchar*
as_display_side_kind_to_string (AsDisplaySideKind kind)
{
	if (kind == AS_DISPLAY_SIDE_KIND_SHORTEST)
		return "shortest";
	if (kind == AS_DISPLAY_SIDE_KIND_LONGEST)
		return "longest";
	return "unknown";
}

/**
 * as_display_side_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsDisplaySideKind or %AS_DISPLAY_SIDE_KIND_UNKNOWN for unknown
 *
 * Since: 0.12.12
 **/
AsDisplaySideKind
as_display_side_kind_from_string (const gchar *kind_str)
{
	if (kind_str == NULL)
		return AS_DISPLAY_SIDE_KIND_SHORTEST;
	if (g_strcmp0 (kind_str, "shortest") == 0)
		return AS_DISPLAY_SIDE_KIND_SHORTEST;
	if (g_strcmp0 (kind_str, "longest") == 0)
		return AS_DISPLAY_SIDE_KIND_LONGEST;
	return AS_DISPLAY_SIDE_KIND_UNKNOWN;
}

/**
 * as_display_length_kind_to_px:
 * @kind: the #AsDisplaySideKind.
 *
 * Converts the rough display length value to an absolute
 * logical pixel measurement, roughly matching the shortest
 * display size of the respective screen size.
 *
 * Returns: amount of logical pixels for shortest display edge, or -1 on invalid input.
 *
 * Since: 0.12.12
 **/
gint
as_display_length_kind_to_px (AsDisplayLengthKind kind)
{
	if (kind == AS_DISPLAY_LENGTH_KIND_XSMALL)
		return 360;
	if (kind == AS_DISPLAY_LENGTH_KIND_SMALL)
		return 420;
	if (kind == AS_DISPLAY_LENGTH_KIND_MEDIUM)
		return 760;
	if (kind == AS_DISPLAY_LENGTH_KIND_LARGE)
		return 900;
	if (kind == AS_DISPLAY_LENGTH_KIND_XLARGE)
		return 1200;
	return -1;
}

/**
 * as_display_length_kind_from_px:
 * @kind: the #AsDisplaySideKind.
 *
 * Classify a logical pixel amount into a display size.
 *
 * Returns: display size enum.
 *
 * Since: 0.12.12
 **/
AsDisplayLengthKind
as_display_length_kind_from_px (gint px)
{
	if (px >= 1200 )
		return AS_DISPLAY_LENGTH_KIND_XLARGE;
	if (px >= 900 )
		return AS_DISPLAY_LENGTH_KIND_LARGE;
	if (px >= 760 )
		return AS_DISPLAY_LENGTH_KIND_MEDIUM;
	if (px >= 360 )
		return AS_DISPLAY_LENGTH_KIND_SMALL;
	if (px < 360 )
		return AS_DISPLAY_LENGTH_KIND_XSMALL;
	return AS_DISPLAY_LENGTH_KIND_UNKNOWN;
}

/**
 * as_display_length_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsDisplayLengthKind or %AS_DISPLAY_LENGTH_KIND_UNKNOWN for unknown
 *
 * Since: 0.12.12
 **/
AsDisplayLengthKind
as_display_length_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "xsmall") == 0)
		return AS_DISPLAY_LENGTH_KIND_XSMALL;
	if (g_strcmp0 (kind_str, "small") == 0)
		return AS_DISPLAY_LENGTH_KIND_SMALL;
	if (g_strcmp0 (kind_str, "medium") == 0)
		return AS_DISPLAY_LENGTH_KIND_MEDIUM;
	if (g_strcmp0 (kind_str, "large") == 0)
		return AS_DISPLAY_LENGTH_KIND_LARGE;
	if (g_strcmp0 (kind_str, "xlarge") == 0)
		return AS_DISPLAY_LENGTH_KIND_XLARGE;
	return AS_DISPLAY_LENGTH_KIND_UNKNOWN;
}

/**
 * as_display_length_kind_to_string:
 * @kind: the #AsDisplayLengthKind.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.12.12
 **/
const gchar*
as_display_length_kind_to_string (AsDisplayLengthKind kind)
{
	if (kind == AS_DISPLAY_LENGTH_KIND_XSMALL)
		return "xsmall";
	if (kind == AS_DISPLAY_LENGTH_KIND_SMALL)
		return "small";
	if (kind == AS_DISPLAY_LENGTH_KIND_MEDIUM)
		return "medium";
	if (kind == AS_DISPLAY_LENGTH_KIND_LARGE)
		return "large";
	if (kind == AS_DISPLAY_LENGTH_KIND_XLARGE)
		return "xlarge";
	return "unknown";
}

/**
 * as_internet_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsInternetKind or %AS_INTERNET_KIND_UNKNOWN for unknown
 *
 * Since: 0.15.5
 **/
AsInternetKind
as_internet_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "always") == 0)
		return AS_INTERNET_KIND_ALWAYS;
	if (g_strcmp0 (kind_str, "offline-only") == 0)
		return AS_INTERNET_KIND_OFFLINE_ONLY;
	if (g_strcmp0 (kind_str, "first-run") == 0)
		return AS_INTERNET_KIND_FIRST_RUN;
	return AS_INTERNET_KIND_UNKNOWN;
}

/**
 * as_internet_kind_to_string:
 * @kind: the #AsInternetKind.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.15.5
 **/
const gchar *
as_internet_kind_to_string (AsInternetKind kind)
{
	if (kind == AS_INTERNET_KIND_ALWAYS)
		return "always";
	if (kind == AS_INTERNET_KIND_OFFLINE_ONLY)
		return "offline-only";
	if (kind == AS_INTERNET_KIND_FIRST_RUN)
		return "first-run";
	return "unknown";
}

/**
 * as_relation_finalize:
 **/
static void
as_relation_finalize (GObject *object)
{
	AsRelation *relation = AS_RELATION (object);
	AsRelationPrivate *priv = GET_PRIVATE (relation);

	g_free (priv->version);
	if (priv->value != NULL)
		g_variant_unref (priv->value);

	G_OBJECT_CLASS (as_relation_parent_class)->finalize (object);
}

/**
 * as_relation_init:
 **/
static void
as_relation_init (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);

	priv->compare = AS_RELATION_COMPARE_GE; /* greater-or-equal is the default comparison method */
}

/**
 * as_relation_class_init:
 **/
static void
as_relation_class_init (AsRelationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_relation_finalize;
}

/**
 * as_relation_get_kind:
 * @relation: a #AsRelation instance.
 *
 * The type (and thereby strength) of this #AsRelation.
 *
 * Returns: an enum of type #AsRelationKind
 *
 * Since: 0.12.0
 */
AsRelationKind
as_relation_get_kind (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	return priv->kind;
}

/**
 * as_relation_set_kind:
 * @relation: a #AsRelation instance.
 * @kind: the new #AsRelationKind
 *
 * Set the kind of this #AsRelation.
 *
 * Since: 0.12.0
 */
void
as_relation_set_kind (AsRelation *relation, AsRelationKind kind)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	priv->kind = kind;
}

/**
 * as_relation_get_item_kind:
 * @relation: a #AsRelation instance.
 *
 * The kind of the item of this #AsRelation.
 *
 * Returns: an enum of type #AsRelationItemKind
 *
 * Since: 0.12.0
 */
AsRelationItemKind
as_relation_get_item_kind (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	return priv->item_kind;
}

/**
 * as_relation_set_item_kind:
 * @relation: a #AsRelation instance.
 * @kind: the new #AsRelationItemKind
 *
 * Set the kind of the item this #AsRelation is about.
 *
 * Since: 0.12.0
 */
void
as_relation_set_item_kind (AsRelation *relation, AsRelationItemKind kind)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	priv->item_kind = kind;
}

/**
 * as_relation_get_compare:
 * @relation: a #AsRelation instance.
 *
 * The version comparison type.
 *
 * Returns: an enum of type #AsRelationCompare
 *
 * Since: 0.12.0
 */
AsRelationCompare
as_relation_get_compare (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	return priv->compare;
}

/**
 * as_relation_set_compare:
 * @relation: an #AsRelation instance.
 * @compare: the new #AsRelationCompare
 *
 * Set the version comparison type of this #AsRelation.
 *
 * Since: 0.12.0
 */
void
as_relation_set_compare (AsRelation *relation, AsRelationCompare compare)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	priv->compare = compare;
}

/**
 * as_relation_get_version:
 * @relation: an #AsRelation instance.
 *
 * Returns: The version of the item this #AsRelation is about.
 *
 * Since: 0.12.0
 **/
const gchar*
as_relation_get_version (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	return priv->version;
}

/**
 * as_relation_set_version:
 * @relation: an #AsRelation instance.
 * @version: the new version.
 *
 * Sets the item version.
 *
 * Since: 0.12.0
 **/
void
as_relation_set_version (AsRelation *relation, const gchar *version)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	g_free (priv->version);
	priv->version = g_strdup (version);
}

/**
 * as_relation_get_value_var:
 * @relation: an #AsRelation instance.
 *
 * Returns: (transfer none): The value of the item this #AsRelation is about, or %NULL if none is set.
 *
 * Since: 0.12.12
 **/
GVariant*
as_relation_get_value_var (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	return priv->value;
}

/**
 * as_relation_set_value_var:
 * @relation: an #AsRelation instance.
 * @value: the new value.
 *
 * Sets the item value.
 * This function will call %g_variant_ref_sink on the passed variant.
 *
 * Since: 0.12.12
 **/
void
as_relation_set_value_var (AsRelation *relation, GVariant *value)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if (priv->value != NULL)
		g_variant_unref (priv->value);
	priv->value = g_variant_ref_sink (value);
}

/**
 * as_relation_get_value_str:
 * @relation: an #AsRelation instance.
 *
 * Returns: The value of the item this #AsRelation is about, as a string.
 *
 * Since: 0.12.12
 **/
const gchar*
as_relation_get_value_str (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if (priv->value == NULL)
		return NULL;
	return g_variant_get_string (priv->value, NULL);
}

/**
 * as_relation_set_value_str:
 * @relation: an #AsRelation instance.
 * @value: the new value.
 *
 * Sets the item value as a string, if the given item type
 * of this #AsRelation permits string values.
 *
 * Since: 0.12.12
 **/
void
as_relation_set_value_str (AsRelation *relation, const gchar *value)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if ((priv->item_kind == AS_RELATION_ITEM_KIND_MEMORY) ||
	    (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH))
		return;
	as_relation_set_value_var (relation, g_variant_new_string (value));
}

/**
 * as_relation_get_value_int:
 * @relation: an #AsRelation instance.
 *
 * Returns: The value of this #AsRelation item as an integer. Returns 0 if the value was no integer.
 *
 * Since: 0.12.0
 **/
gint
as_relation_get_value_int (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if (priv->value == NULL)
		return 0;
	if ((priv->item_kind != AS_RELATION_ITEM_KIND_MEMORY) &&
	    (priv->item_kind != AS_RELATION_ITEM_KIND_DISPLAY_LENGTH))
		return 0;
	return g_variant_get_int32 (priv->value);
}

/**
 * as_relation_set_value_int:
 * @relation: an #AsRelation instance.
 * @value: the new value.
 *
 * Sets the item value as an integer, if the given item type
 * of this #AsRelation permits integer values.
 *
 * Since: 0.12.12
 **/
void
as_relation_set_value_int (AsRelation *relation, gint value)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if ((priv->item_kind != AS_RELATION_ITEM_KIND_MEMORY) &&
	    (priv->item_kind != AS_RELATION_ITEM_KIND_DISPLAY_LENGTH))
		return;
	priv->display_length_kind = AS_DISPLAY_LENGTH_KIND_UNKNOWN;
	as_relation_set_value_var (relation, g_variant_new_int32 (value));
}

/**
 * as_relation_get_value_control_kind:
 * @relation: an #AsRelation instance.
 *
 * Get the value of this #AsRelation item as #AsControlKind if the
 * type of this relation is %AS_RELATION_ITEM_KIND_CONTROL.
 * Otherwise return %AS_CONTROL_KIND_UNKNOWN
 *
 * Returns: a #AsControlKind or %AS_CONTROL_KIND_UNKNOWN in case the item is not of the right kind.
 *
 * Since: 0.12.11
 **/
AsControlKind
as_relation_get_value_control_kind (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if (priv->item_kind != AS_RELATION_ITEM_KIND_CONTROL)
		return AS_CONTROL_KIND_UNKNOWN;
	if (priv->value == NULL)
		return AS_CONTROL_KIND_UNKNOWN;
	return (AsControlKind) g_variant_get_int32 (priv->value);
}

/**
 * as_relation_set_value_control_kind:
 * @relation: an #AsRelation instance.
 * @kind: an #AsControlKind
 *
 * Set relation item value from an #AsControlKind.
 *
 * Since: 0.12.12
 **/
void
as_relation_set_value_control_kind (AsRelation *relation, AsControlKind kind)
{
	as_relation_set_value_var (relation, g_variant_new_int32 (kind));
}

/**
 * as_relation_get_value_internet_kind:
 * @relation: an #AsRelation instance.
 *
 * Get the value of this #AsRelation item as #AsInternetKind if the
 * type of this relation is %AS_RELATION_ITEM_KIND_INTERNET.
 * Otherwise return %AS_INTERNET_KIND_UNKNOWN
 *
 * Returns: a #AsInternetKind or %AS_INTERNET_KIND_UNKNOWN in case the item is not of the right kind.
 *
 * Since: 0.15.5
 **/
AsInternetKind
as_relation_get_value_internet_kind (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if (priv->item_kind != AS_RELATION_ITEM_KIND_INTERNET)
		return AS_INTERNET_KIND_UNKNOWN;
	if (priv->value == NULL)
		return AS_INTERNET_KIND_UNKNOWN;
	return (AsInternetKind) g_variant_get_int32 (priv->value);
}

/**
 * as_relation_set_value_internet_kind:
 * @relation: an #AsRelation instance.
 * @kind: an #AsInternetKind
 *
 * Set relation item value from an #AsInternetKind.
 *
 * Since: 0.15.5
 **/
void
as_relation_set_value_internet_kind (AsRelation *relation, AsInternetKind kind)
{
	as_relation_set_value_var (relation, g_variant_new_int32 (kind));
}

/**
 * as_relation_get_value_internet_bandwidth:
 * @relation: an #AsRelation instance.
 *
 * If this #AsRelation is of kind %AS_RELATION_ITEM_KIND_INTERNET, return the
 * minimum bandwidth requirement of the component, if set.
 *
 * If the relation is of a different kind, or the requirement isn’t set, this
 * returns `0`.
 *
 * Returns: The minimum bandwidth requirement, in Mbit/s.
 * Since: 0.15.5
 */
guint
as_relation_get_value_internet_bandwidth (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);

	if (priv->item_kind != AS_RELATION_ITEM_KIND_INTERNET)
		return 0;

	return priv->bandwidth_mbitps;
}

/**
 * as-relation_set_value_internet_bandwidth:
 * @relation: an #AsRelation instance.
 * @bandwidth_mbitps: Minimum bandwidth requirement, in Mbit/s, or `0` for no
 *     requirement.
 *
 * Sets the minimum bandwidth requirement of the component.
 *
 * This requires the relation to be of item kind
 * %AS_RELATION_ITEM_KIND_INTERNET.
 *
 * Since: 0.15.5
 */
void
as_relation_set_value_internet_bandwidth (AsRelation *relation,
                                          guint       bandwidth_mbitps)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);

	if (priv->item_kind != AS_RELATION_ITEM_KIND_INTERNET)
		return;

	priv->bandwidth_mbitps = bandwidth_mbitps;
}

/**
 * as_relation_get_value_px:
 * @relation: an #AsRelation instance.
 *
 * In case this #AsRelation is of kind %AS_RELATION_ITEM_KIND_DISPLAY_LENGTH,
 * return the set logical pixel amount.
 *
 * Returns: The logical pixel amount for this display length, value <= 0 on error.
 *
 * Since: 0.12.12
 **/
gint
as_relation_get_value_px (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	gint value;
	if (priv->item_kind != AS_RELATION_ITEM_KIND_DISPLAY_LENGTH)
		return -1;
	if (priv->value == NULL)
		return as_display_length_kind_to_px (priv->display_length_kind);
	value = g_variant_get_int32 (priv->value);
	if (value == 0)
		return as_display_length_kind_to_px (priv->display_length_kind);
	return value;
}

/**
 * as_relation_set_value_px:
 * @relation: an #AsRelation instance.
 * @logical_px: logical pixel count.
 *
 * Sets the item value as logical pixel count. This requires the relation
 * to be of item kind %AS_RELATION_ITEM_KIND_DISPLAY_LENGTH.
 *
 * Since: 0.12.12
 **/
void
as_relation_set_value_px (AsRelation *relation, gint logical_px)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if (priv->item_kind != AS_RELATION_ITEM_KIND_DISPLAY_LENGTH)
		return;
	priv->display_length_kind = AS_DISPLAY_LENGTH_KIND_UNKNOWN;
	as_relation_set_value_var (relation, g_variant_new_int32 (logical_px));
}

/**
 * as_relation_get_value_display_length_kind:
 * @relation: an #AsRelation instance.
 *
 * In case this #AsRelation is of kind %AS_RELATION_ITEM_KIND_DISPLAY_LENGTH,
 * return the #AsDisplayLengthKind.
 *
 * Returns: The #AsDisplayLengthKind classification of the current pixel value, or %AS_DISPLAY_LENGTH_KIND_UNKNOWN on error.
 *
 * Since: 0.12.12
 **/
AsDisplayLengthKind
as_relation_get_value_display_length_kind (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if (priv->display_length_kind != AS_DISPLAY_LENGTH_KIND_UNKNOWN)
		return priv->display_length_kind;
	return as_display_length_kind_from_px (as_relation_get_value_px (relation));
}

/**
 * as_relation_set_value_display_length_kind:
 * @relation: an #AsRelation instance.
 * @kind: the #AsDisplayLengthKind
 *
 * Sets the item value as display length placeholder value. This requires the relation
 * to be of item kind %AS_RELATION_ITEM_KIND_DISPLAY_LENGTH.
 *
 * Since: 0.12.12
 **/
void
as_relation_set_value_display_length_kind (AsRelation *relation, AsDisplayLengthKind kind)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	if (priv->item_kind != AS_RELATION_ITEM_KIND_DISPLAY_LENGTH)
		return;
	priv->display_length_kind = kind;
	as_relation_set_value_var (relation, g_variant_new_int32 (0));
}

/**
 * as_relation_get_display_side_kind:
 * @relation: an #AsRelation instance.
 *
 * Gets the display side kind, in case this item is of
 * kind %AS_RELATION_ITEM_KIND_DISPLAY_LENGTH
 *
 * Returns: a #AsDisplaySideKind or %AS_DISPLAY_SIDE_KIND_UNKNOWN
 *
 * Since: 0.12.12
 **/
AsDisplaySideKind
as_relation_get_display_side_kind (AsRelation *relation)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	return priv->display_side_kind;
}

/**
 * as_relation_set_display_side_kind:
 * @relation: an #AsRelation instance.
 * @kind: the new #AsDisplaySideKind.
 *
 * Sets the display side kind, in case this item is of
 * kind %AS_RELATION_ITEM_KIND_DISPLAY_LENGTH
 *
 * Since: 0.12.12
 **/
void
as_relation_set_display_side_kind (AsRelation *relation, AsDisplaySideKind kind)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	priv->display_side_kind = kind;
}

/**
 * as_relation_get_value:
 * @relation: an #AsRelation instance.
 *
 * Deprecated method. Use %as_relation_get_value_str instead.
 *
 * Since: 0.12.0
 **/
const gchar*
as_relation_get_value (AsRelation *relation)
{
	return as_relation_get_value_str (relation);
}

/**
 * as_relation_set_value:
 * @relation: an #AsRelation instance.
 * @value: the new value.
 *
 * Deprecated method. Use %as_relation_set_value_str instead.
 *
 * Since: 0.12.0
 **/
void
as_relation_set_value (AsRelation *relation, const gchar *value)
{
	as_relation_set_value_str (relation, value);
}

/**
 * as_relation_version_compare:
 * @relation: an #AsRelation instance.
 * @version: a version number, e.g. `1.2.0`
 * @error: A #GError or %NULL
 *
 * Tests whether the version number of this #AsRelation is fulfilled by
 * @version. Whether the given version is sufficient to fulfill the version
 * requirement of this #AsRelation is determined by its comparison resraint.
 *
 * Returns: %TRUE if the version from the parameter is sufficient.
 *
 * Since: 0.12.0
 **/
gboolean
as_relation_version_compare (AsRelation *relation, const gchar *version, GError **error)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	gint rc;

	/* if we have no version set, any version checked against is satisfactory */
	if (priv->version == NULL)
		return TRUE;

	switch (priv->compare) {
	case AS_RELATION_COMPARE_EQ:
		rc = as_vercmp_simple (priv->version, version);
		return rc == 0;
	case AS_RELATION_COMPARE_NE:
		rc = as_vercmp_simple (priv->version, version);
		return rc != 0;
	case AS_RELATION_COMPARE_LT:
		rc = as_vercmp_simple (priv->version, version);
		return rc > 0;
	case AS_RELATION_COMPARE_GT:
		rc = as_vercmp_simple (priv->version, version);
		return rc < 0;
	case AS_RELATION_COMPARE_LE:
		rc = as_vercmp_simple (priv->version, version);
		return rc >= 0;
	case AS_RELATION_COMPARE_GE:
		rc = as_vercmp_simple (priv->version, version);
		return rc <= 0;
	default:
		return FALSE;
	}
}

/**
 * as_relation_load_from_xml:
 * @relation: a #AsRelation instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads #AsRelation data from an XML node.
 **/
gboolean
as_relation_load_from_xml (AsRelation *relation, AsContext *ctx, xmlNode *node, GError **error)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	g_autofree gchar *content = NULL;

	content = as_xml_get_node_value (node);
	if (content == NULL)
		return FALSE;

	priv->item_kind = as_relation_item_kind_from_string ((const gchar*) node->name);

	if (priv->item_kind == AS_RELATION_ITEM_KIND_MEMORY) {
		as_relation_set_value_var (relation, g_variant_new_int32 (g_ascii_strtoll (content, NULL, 10)));
	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
		gint value = g_ascii_strtoll (content, NULL, 10);
		priv->display_length_kind = AS_DISPLAY_LENGTH_KIND_UNKNOWN;
		if (value == 0)
			priv->display_length_kind = as_display_length_kind_from_string (content);
		as_relation_set_value_var (relation, g_variant_new_int32 (value));

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
		as_relation_set_value_var (relation, g_variant_new_int32 (as_control_kind_from_string (content)));
	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_INTERNET) {
		as_relation_set_value_var (relation, g_variant_new_int32 (as_internet_kind_from_string (content)));
	} else {
		as_relation_set_value_str (relation, content);
	}

	if (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
		g_autofree gchar *side_str = as_xml_get_prop_value (node, "side");
		priv->display_side_kind = as_display_side_kind_from_string (side_str);

		g_free (g_steal_pointer (&priv->version));
	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_INTERNET) {
		g_autofree gchar *bandwidth_str = as_xml_get_prop_value (node, "bandwidth_mbitps");

		if (bandwidth_str != NULL)
			priv->bandwidth_mbitps = g_ascii_strtoll (bandwidth_str, NULL, 10);
		else
			priv->bandwidth_mbitps = 0;

		g_free (g_steal_pointer (&priv->version));
	} else if (priv->item_kind != AS_RELATION_ITEM_KIND_CONTROL) {
		g_free (priv->version);
		priv->version = as_xml_get_prop_value (node, "version");
	}

	if ((priv->version != NULL) || (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH)) {
		g_autofree gchar *compare_str = as_xml_get_prop_value (node, "compare");
		priv->compare = as_relation_compare_from_string (compare_str);
	}

	return TRUE;
}

/**
 * as_relation_to_xml_node:
 * @relation: an #AsRelation
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new node to.
 *
 * Serializes the data to a XML node.
 * @root should be a <requires/> or <recommends/> root node.
 **/
void
as_relation_to_xml_node (AsRelation *relation, AsContext *ctx, xmlNode *root)
{

	AsRelationPrivate *priv = GET_PRIVATE (relation);
	xmlNode *n;

	if (priv->item_kind == AS_RELATION_ITEM_KIND_UNKNOWN)
		return;

	if (priv->item_kind == AS_RELATION_ITEM_KIND_MEMORY) {
		g_autofree gchar *value_str = g_strdup_printf("%i", as_relation_get_value_int (relation));
		n = as_xml_add_text_node (root,
					  as_relation_item_kind_to_string (priv->item_kind),
					  value_str);

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
		if (priv->display_length_kind != AS_DISPLAY_LENGTH_KIND_UNKNOWN) {
			n = as_xml_add_text_node (root,
						  as_relation_item_kind_to_string (priv->item_kind),
						  as_display_length_kind_to_string (priv->display_length_kind));
		} else {
			g_autofree gchar *value_str = g_strdup_printf("%i", as_relation_get_value_int (relation));
			n = as_xml_add_text_node (root,
						  as_relation_item_kind_to_string (priv->item_kind),
						  value_str);
		}

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
		n = as_xml_add_text_node (root,
					  as_relation_item_kind_to_string (priv->item_kind),
					  as_control_kind_to_string (as_relation_get_value_control_kind (relation)));

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_INTERNET) {
		n = as_xml_add_text_node (root,
					  as_relation_item_kind_to_string (priv->item_kind),
					  as_internet_kind_to_string (as_relation_get_value_internet_kind (relation)));

	} else {
		n = as_xml_add_text_node (root,
					  as_relation_item_kind_to_string (priv->item_kind),
					  as_relation_get_value_str (relation));
	}

	if (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
		if ((priv->display_side_kind != AS_DISPLAY_SIDE_KIND_SHORTEST) && (priv->display_side_kind != AS_DISPLAY_SIDE_KIND_UNKNOWN))
			as_xml_add_text_prop (n, "side",
					      as_display_side_kind_to_string (priv->display_side_kind));
		if (priv->compare != AS_RELATION_COMPARE_GE)
			as_xml_add_text_prop (n, "compare",
					      as_relation_compare_to_string (priv->compare));

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_INTERNET) {
		if (priv->bandwidth_mbitps > 0) {
			g_autofree gchar *bandwidth_str = g_strdup_printf ("%u", priv->bandwidth_mbitps);
			as_xml_add_text_prop (n, "bandwidth_mbitps",
					      bandwidth_str);
		}

	} else if ((priv->item_kind == AS_RELATION_ITEM_KIND_CONTROL) || (priv->item_kind == AS_RELATION_ITEM_KIND_MEMORY)) {
	} else if (priv->version != NULL) {
		as_xml_add_text_prop (n,
				      "version",
				      priv->version);
		as_xml_add_text_prop (n,
				      "compare",
				      as_relation_compare_to_string (priv->compare));
	}
}

/**
 * as_relation_load_from_yaml:
 * @relation: an #AsRelation
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_relation_load_from_yaml (AsRelation *relation, AsContext *ctx, GNode *node, GError **error)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	GNode *n;

	if (node->children == NULL)
		return FALSE;

	for (n = node->children; n != NULL; n = n->next) {
		const gchar *entry = as_yaml_node_get_key (n);
		if (entry == NULL)
			continue;

		if (g_strcmp0 (entry, "version") == 0) {
			g_autofree gchar *compare_str = NULL;
			const gchar *ver_str = as_yaml_node_get_value (n);
			if (strlen (ver_str) <= 2)
				continue; /* this string is too short to contain any valid version */
			compare_str = g_strndup (ver_str, 2);
			priv->compare = as_relation_compare_from_string (compare_str);
			g_free (priv->version);
			priv->version = g_strdup (ver_str + 2);
			g_strstrip (priv->version);
		} else if (g_strcmp0 (entry, "side") == 0) {
			priv->display_side_kind = as_display_side_kind_from_string (as_yaml_node_get_value (n));
		} else if (g_strcmp0 (entry, "bandwidth_mbitps") == 0) {
			priv->bandwidth_mbitps = g_ascii_strtoll (as_yaml_node_get_value (n), NULL, 10);
		} else {
			AsRelationItemKind kind = as_relation_item_kind_from_string (entry);
			if (kind == AS_RELATION_ITEM_KIND_UNKNOWN) {
				g_debug ("Unknown Requires/Recommends YAML field: %s", entry);
				continue;
			}

			priv->item_kind = kind;
			if (kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
				g_autofree gchar *value_str = NULL;
				gint value_px;
				const gchar *len_str = as_yaml_node_get_value (n);
				if (strlen (len_str) <= 2) {
					/* this string is too short to contain a comparison operator */
					value_str = g_strdup (len_str);
				} else {
					g_autofree gchar *compare_str = NULL;
					compare_str = g_strndup (len_str, 2);
					priv->compare = as_relation_compare_from_string (compare_str);

					if (priv->compare == AS_RELATION_COMPARE_UNKNOWN) {
						value_str = g_strdup (len_str);
						priv->compare = AS_RELATION_COMPARE_GE;
					} else {
						value_str = g_strdup (len_str + 2);
						g_strstrip (value_str);
					}
				}

				value_px = g_ascii_strtoll (value_str, NULL, 10);
				priv->display_length_kind = AS_DISPLAY_LENGTH_KIND_UNKNOWN;
				if (value_px == 0)
					priv->display_length_kind = as_display_length_kind_from_string (value_str);
				as_relation_set_value_var (relation, g_variant_new_int32 (value_px));

			} else if (kind == AS_RELATION_ITEM_KIND_MEMORY) {
				gint value_i = g_ascii_strtoll (as_yaml_node_get_value (n), NULL, 10);
				as_relation_set_value_var (relation, g_variant_new_int32 (value_i));

			} else if (kind == AS_RELATION_ITEM_KIND_CONTROL) {
				as_relation_set_value_var (relation,
							   g_variant_new_int32 (as_control_kind_from_string (as_yaml_node_get_value (n))));

			} else if (kind == AS_RELATION_ITEM_KIND_INTERNET) {
				as_relation_set_value_var (relation,
							   g_variant_new_int32 (as_internet_kind_from_string (as_yaml_node_get_value (n))));

			} else {
				as_relation_set_value_str (relation, as_yaml_node_get_value (n));
			}
		}
	}

	return TRUE;
}

/**
 * as_relation_emit_yaml:
 * @relation: an #AsRelation
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_relation_emit_yaml (AsRelation *relation, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);

	if ((priv->item_kind <= AS_RELATION_ITEM_KIND_UNKNOWN) || (priv->item_kind >= AS_RELATION_ITEM_KIND_LAST))
		return;

	as_yaml_mapping_start (emitter);

	if (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
		if ((priv->compare != AS_RELATION_COMPARE_UNKNOWN) && (priv->compare != AS_RELATION_COMPARE_GE)) {
			g_autofree gchar *value = NULL;
			g_autofree gchar *len_str = NULL;

			if (priv->display_length_kind != AS_DISPLAY_LENGTH_KIND_UNKNOWN)
				value = g_strdup (as_display_length_kind_to_string (priv->display_length_kind));
			else
				value = g_strdup_printf("%i", as_relation_get_value_int (relation));

			len_str = g_strdup_printf ("%s %s",
						   as_relation_compare_to_symbols_string (priv->compare),
						   value);
			as_yaml_emit_entry (emitter,
					    as_relation_item_kind_to_string (priv->item_kind),
					    len_str);
		} else {
			if (priv->display_length_kind != AS_DISPLAY_LENGTH_KIND_UNKNOWN)
				as_yaml_emit_entry (emitter,
						    as_relation_item_kind_to_string (priv->item_kind),
						    as_display_length_kind_to_string (priv->display_length_kind));
			else
				as_yaml_emit_entry_uint64 (emitter,
							   as_relation_item_kind_to_string (priv->item_kind),
							   as_relation_get_value_int (relation));
		}

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
		as_yaml_emit_entry (emitter,
				    as_relation_item_kind_to_string (priv->item_kind),
				    as_control_kind_to_string (as_relation_get_value_control_kind (relation)));

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_MEMORY) {
		as_yaml_emit_entry_uint64 (emitter,
					   as_relation_item_kind_to_string (priv->item_kind),
					   as_relation_get_value_int (relation));

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_INTERNET) {
		as_yaml_emit_entry (emitter,
				    as_relation_item_kind_to_string (priv->item_kind),
				    as_internet_kind_to_string (as_relation_get_value_internet_kind (relation)));
		if (priv->bandwidth_mbitps > 0)
			as_yaml_emit_entry_uint64 (emitter,
						   "bandwidth_mbitps",
						   priv->bandwidth_mbitps);

	} else {
		as_yaml_emit_entry (emitter,
				    as_relation_item_kind_to_string (priv->item_kind),
				    as_relation_get_value_str (relation));
	}

	if (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
		if ((priv->display_side_kind != AS_DISPLAY_SIDE_KIND_SHORTEST) && (priv->display_side_kind != AS_DISPLAY_SIDE_KIND_UNKNOWN))
			as_yaml_emit_entry (emitter, "side", as_display_side_kind_to_string (priv->display_side_kind));

	} else if (priv->item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
	} else if (priv->version != NULL) {
		g_autofree gchar *ver_str = g_strdup_printf ("%s %s",
							     as_relation_compare_to_symbols_string (priv->compare),
							     priv->version);
		as_yaml_emit_entry (emitter, "version", ver_str);
	}

	as_yaml_mapping_end (emitter);
}

/**
 * as_compare_int_match:
 *
 * Compare two integers and check if the comparison operator matches.
 */
static gboolean
as_compare_int_match (glong first, AsRelationCompare compare, glong second)
{
	g_return_val_if_fail (compare != AS_RELATION_COMPARE_UNKNOWN, FALSE);

	switch (compare) {
	case AS_RELATION_COMPARE_EQ:
		return first == second;
	case AS_RELATION_COMPARE_NE:
		return first != 0;
	case AS_RELATION_COMPARE_LT:
		return first < second;
	case AS_RELATION_COMPARE_GT:
		return first > second;
	case AS_RELATION_COMPARE_LE:
		return first <= second;
	case AS_RELATION_COMPARE_GE:
		return first >= second;
	default:
		return FALSE;
	}
}

/**
 * _as_set_satify_message:
 */
static void
_as_set_satify_message (gchar **message, gchar *text)
{
	if (message == NULL) {
		g_free (text);
		return;
	}
	*message = text;
}

/**
 * _as_get_control_missing_message:
 */
static gchar*
_as_get_control_missing_message (AsControlKind c_kind, AsRelationKind r_kind)
{
	const gchar *ctl_name = NULL;

	if (c_kind == AS_CONTROL_KIND_POINTING)
		ctl_name = _("pointing device (e.g. a mouse)");
	if (c_kind == AS_CONTROL_KIND_KEYBOARD)
		ctl_name = _("keyboard");
	if (c_kind == AS_CONTROL_KIND_GAMEPAD)
		ctl_name = _("gamepad");
	if (c_kind == AS_CONTROL_KIND_TV_REMOTE)
		ctl_name = _("tv remote");
	if (c_kind == AS_CONTROL_KIND_TABLET)
		ctl_name = _("graphics tablet");

	if (r_kind == AS_RELATION_KIND_REQUIRES) {
		if (ctl_name != NULL) {
			/* TRANSLATORS: The placeholder is replaced with an input device name, e.g. "gamepad" */
			return g_strdup_printf (_("This software requires a %s for input."),
						  ctl_name);
		}

		if (c_kind == AS_CONTROL_KIND_TOUCH)
			return g_strdup (_("This software requires a touch input device."));
		if (c_kind == AS_CONTROL_KIND_VOICE)
			return g_strdup (_("This software requires a microphone to be controlled via voice input."));
		if (c_kind == AS_CONTROL_KIND_VISION)
			return g_strdup (_("This software requires a camera for input control."));
		if (c_kind == AS_CONTROL_KIND_CONSOLE)
			return g_strdup (_("This software requires a method for console input."));

	} else if (r_kind == AS_RELATION_KIND_RECOMMENDS) {
		if (ctl_name != NULL) {
			/* TRANSLATORS: The placeholder is replaced with an input device name, e.g. "gamepad" */
			return g_strdup_printf (_("This software recommends a %s for input."),
						  ctl_name);
		}

		if (c_kind == AS_CONTROL_KIND_TOUCH)
			return g_strdup (_("This software recommends a touch input device."));
		if (c_kind == AS_CONTROL_KIND_VOICE)
			return g_strdup (_("This software recommends a microphone to be controlled via voice input."));
		if (c_kind == AS_CONTROL_KIND_VISION)
			return g_strdup (_("This software recommends a camera for input control."));
		if (c_kind == AS_CONTROL_KIND_CONSOLE)
			return g_strdup (_("This software recommends a method for console input."));

	} else if (r_kind == AS_RELATION_KIND_SUPPORTS) {

		if (c_kind == AS_CONTROL_KIND_POINTING)
			ctl_name = _("pointing devices (e.g. mice)");
		if (c_kind == AS_CONTROL_KIND_KEYBOARD)
			ctl_name = _("keyboards");
		if (c_kind == AS_CONTROL_KIND_GAMEPAD)
			ctl_name = _("gamepads");
		if (c_kind == AS_CONTROL_KIND_TV_REMOTE)
			ctl_name = _("tv remotes");
		if (c_kind == AS_CONTROL_KIND_TABLET)
			ctl_name = _("graphics tablets");

		if (ctl_name != NULL) {
			/* TRANSLATORS: The placeholder is replaced with a plural input device name, e.g. "gamepads" */
			return g_strdup_printf (_("This software supports %s."),
						  ctl_name);
		}

		if (c_kind == AS_CONTROL_KIND_TOUCH)
			return g_strdup (_("This software supports touch input."));
		if (c_kind == AS_CONTROL_KIND_VOICE)
			return g_strdup (_("This software can be controlled via voice input."));
		if (c_kind == AS_CONTROL_KIND_VISION)
			return g_strdup (_("This software can be controlled via a camera."));
		if (c_kind == AS_CONTROL_KIND_CONSOLE)
			return g_strdup (_("This software supports operation via console commands."));
	}

	return NULL;
}

/**
 * _as_get_control_found_message:
 */
static gchar*
_as_get_control_found_message (AsControlKind c_kind)
{
	if (c_kind == AS_CONTROL_KIND_POINTING)
		return g_strdup (_("Pointing device (e.g. a mouse or touchpad) found."));
	if (c_kind == AS_CONTROL_KIND_KEYBOARD)
		return g_strdup (_("Physical keyboard found."));
	if (c_kind == AS_CONTROL_KIND_GAMEPAD)
		return g_strdup (_("Gamepad found."));
	if (c_kind == AS_CONTROL_KIND_TV_REMOTE)
		return g_strdup (_("TV remote found."));
	if (c_kind == AS_CONTROL_KIND_TABLET)
		return g_strdup (_("Graphics tablet found."));

	if (c_kind == AS_CONTROL_KIND_TOUCH)
		return g_strdup (_("Touch input device found."));
	if (c_kind == AS_CONTROL_KIND_VOICE)
		return g_strdup (_("Microphone for voice input control found."));
	if (c_kind == AS_CONTROL_KIND_VISION)
		return g_strdup (_("Camera for input control found."));
	if (c_kind == AS_CONTROL_KIND_CONSOLE)
		return g_strdup (_("Console interface available."));

	return NULL;
}

/**
 * as_relation_is_satisfied:
 * @relation: a #AsRelation instance.
 * @system_info: (nullable): an #AsSystemInfo to use for system information.
 * @pool: (nullable): an #AsPool to find component dependencies in.
 * @message: (out) (optional): receive a localized status message.
 * @error: a #GError.
 *
 * Test if this relation is satisfied on the current system or with the
 * provided #AsPool. If no #AsSystemInfo is found, a temporary one will be
 * created. If no #AsPool is provided, any component relationships can not
 * be validated and an error will be thrown.
 *
 * Returns: %AS_CHECK_RESULT_TRUE if the system satisfies the relation, %AS_CHECK_RESULT_ERROR on error
 */
AsCheckResult
as_relation_is_satisfied (AsRelation *relation,
			  AsSystemInfo *system_info,
			  AsPool *pool,
			  gchar **message,
			  GError **error)
{
	AsRelationPrivate *priv = GET_PRIVATE (relation);
	g_autoptr(AsSystemInfo) sysinfo = NULL;

	sysinfo = (system_info == NULL)? as_system_info_new () : g_object_ref (system_info);

	/* Components */
	if (priv->item_kind == AS_RELATION_ITEM_KIND_ID) {
		g_autoptr(GPtrArray) cpts = NULL;
		const gchar *cid;
		if (pool == NULL) {
			g_set_error_literal (error,
						AS_RELATION_ERROR,
						AS_RELATION_ERROR_FAILED,
						"Unable to check ID relation status: No valid metadata pool was provided.");
			return AS_CHECK_RESULT_ERROR;
		}

		cid = as_relation_get_value_str (relation);
		if (cid == NULL) {
			g_set_error_literal (error,
						AS_RELATION_ERROR,
						AS_RELATION_ERROR_BAD_VALUE,
						"Unable to check ID relation status: This relation is invalid, it has no valid value.");
			return AS_CHECK_RESULT_ERROR;
		}
		cpts = as_pool_get_components_by_id (pool, cid);

		if (cpts->len > 0) {
			_as_set_satify_message (message,
						g_strdup_printf (_("Software '%s' was found"),
								 as_component_get_name (g_ptr_array_index (cpts, 0))));
			return AS_CHECK_RESULT_TRUE;
		} else {
			if (priv->kind == AS_RELATION_KIND_REQUIRES)
				_as_set_satify_message (message,
							g_strdup_printf (_("Required software component '%s' is missing."),
									cid));
			else if (priv->kind == AS_RELATION_KIND_RECOMMENDS)
				_as_set_satify_message (message,
							g_strdup_printf (_("Recommended software component '%s' is missing."),
									cid));
			else if (priv->kind == AS_RELATION_KIND_SUPPORTS)
				_as_set_satify_message (message,
							g_strdup_printf (_("Found supported software component '%s'."),
									cid));
			return AS_CHECK_RESULT_FALSE;
		}
	}

	/* Modaliases */
	if (priv->item_kind == AS_RELATION_ITEM_KIND_MODALIAS) {
		const gchar *modalias;
		g_autofree gchar *device_name = NULL;

		modalias = as_relation_get_value_str (relation);
		if (modalias == NULL) {
			g_set_error_literal (error,
						AS_RELATION_ERROR,
						AS_RELATION_ERROR_BAD_VALUE,
						"Unable to check modalias relation status: This relation is invalid, it has no valid value.");
			return AS_CHECK_RESULT_ERROR;
		}

		device_name = as_system_info_get_device_name_for_modalias (sysinfo,
									   modalias,
									   TRUE,
									   NULL);
		if (device_name == NULL)
			device_name = g_strdup (modalias);

		if (as_system_info_has_device_matching_modalias (sysinfo, modalias)) {
			_as_set_satify_message (message,
						g_strdup_printf (_("Found hardware that is supported by this software: '%s'"),
								 device_name));
			return AS_CHECK_RESULT_TRUE;
		} else {
			if (priv->kind == AS_RELATION_KIND_REQUIRES)
				_as_set_satify_message (message,
							g_strdup_printf (_("Required hardware for this software was not found on this system: '%s'"),
									device_name));
			else if (priv->kind == AS_RELATION_KIND_RECOMMENDS)
				_as_set_satify_message (message,
							g_strdup_printf (_("Recommended hardware for this software was not found on this system: '%s'"),
									device_name));
			else
				_as_set_satify_message (message,
							g_strdup_printf (_("This software supports hardware not present in this system: '%s'"),
									device_name));
			return AS_CHECK_RESULT_FALSE;
		}
	}

	/* Kernels */
	if (priv->item_kind == AS_RELATION_ITEM_KIND_KERNEL) {
		const gchar *current_kernel_name;
		const gchar *current_kernel_version;
		const gchar *req_kernel_name;
		const gchar *req_kernel_version;

		current_kernel_name = as_system_info_get_kernel_name (sysinfo);
		if (current_kernel_name == NULL) {
			g_set_error_literal (error,
						AS_SYSTEM_INFO_ERROR,
						AS_SYSTEM_INFO_ERROR_NOT_FOUND,
						"Unable to determine the current kernel name.");
			return AS_CHECK_RESULT_ERROR;
		}

		req_kernel_name = as_relation_get_value_str (relation);
		if (req_kernel_name == NULL) {
			g_set_error_literal (error,
						AS_RELATION_ERROR,
						AS_RELATION_ERROR_BAD_VALUE,
						"Unable to check kernel relation status: No valid value set for relation.");
			return AS_CHECK_RESULT_ERROR;
		}

		if (g_ascii_strcasecmp (current_kernel_name, req_kernel_name) != 0) {
			if (priv->kind == AS_RELATION_KIND_REQUIRES)
				_as_set_satify_message (message,
							g_strdup_printf (_("This software requires a %s kernel, but this system is running %s."),
									req_kernel_name, current_kernel_name));
			else if (priv->kind == AS_RELATION_KIND_RECOMMENDS)
				_as_set_satify_message (message,
							g_strdup_printf (_("This software recommends a %s kernel, but this system is running %s."),
									req_kernel_name, current_kernel_name));
			else
				_as_set_satify_message (message,
							g_strdup_printf (_("This software only supports a %s kernel, but may run on %s anyway."),
									req_kernel_name, current_kernel_name));
			return AS_CHECK_RESULT_FALSE;
		}

		current_kernel_version = as_system_info_get_kernel_version (sysinfo);
		req_kernel_version = as_relation_get_version (relation);

		/* if no version was specified, we just needed to test for a kernel name */
		if (req_kernel_version == NULL)
			return AS_CHECK_RESULT_TRUE;

		if (!as_vercmp_test_match (current_kernel_version,
					   as_relation_get_compare (relation),
					   req_kernel_version,
					   AS_VERCMP_FLAG_NONE)) {
			const gchar *compare_symbols = as_relation_compare_to_symbols_string (as_relation_get_compare (relation));
			if (priv->kind == AS_RELATION_KIND_REQUIRES)
				_as_set_satify_message (message,
							/* TRANSLATORS: We checked a kernel dependency, the first placeholder is the required kernel name,
							   second is comparison operator (e.g. >=), third is the expected version number fourth the current kernel name
							   and fifth is the version we are running. */
							g_strdup_printf (_("This software requires %s %s %s, but this system is running %s %s."),
									req_kernel_name, compare_symbols, req_kernel_version,
									current_kernel_name, current_kernel_version));
			else if (priv->kind == AS_RELATION_KIND_RECOMMENDS)
				_as_set_satify_message (message,
							/* TRANSLATORS: We checked a kernel dependency, the first placeholder is the required kernel name,
							   second is comparison operator (e.g. >=), third is the expected version number fourth the current kernel name
							   and fifth is the version we are running. */
							g_strdup_printf (_("The use of %s %s %s is recommended, but this system is running %s %s."),
									req_kernel_name, compare_symbols, req_kernel_version,
									current_kernel_name, current_kernel_version));
			else if (priv->kind == AS_RELATION_KIND_SUPPORTS) {
				_as_set_satify_message (message,
							/* TRANSLATORS: We checked a kernel dependency, the first placeholder is the kernel name,
							   second is comparison operator (e.g. >=), third is the expected version number. */
							g_strdup_printf (_("This software supports %s %s %s."),
									req_kernel_name, compare_symbols, req_kernel_version));
				/* this is not an error, supports is only a hint for kernels */
				return AS_CHECK_RESULT_TRUE;
			}
			return AS_CHECK_RESULT_FALSE;
		}

		_as_set_satify_message (message,
					/* TRANSLATORS: We checked a kernel dependency, with success, the first placeholder is the current kernel name, second is its version number. */
					g_strdup_printf (_("Kernel %s %s is supported."),
							 current_kernel_name, current_kernel_version));

		/* if we are here, we are running an acceptable kernel version */
		return AS_CHECK_RESULT_TRUE;
	}

	/* Physical Memory */
	if (priv->item_kind == AS_RELATION_ITEM_KIND_MEMORY) {
		gulong req_memory;
		gulong current_memory;

		req_memory = as_relation_get_value_int (relation);
		if (req_memory == 0) {
			g_set_error_literal (error,
						AS_RELATION_ERROR,
						AS_RELATION_ERROR_BAD_VALUE,
						"Unable to check memory relation: No valid value set in metadata.");
			return AS_CHECK_RESULT_ERROR;
		}

		current_memory = as_system_info_get_memory_total (sysinfo);

		if (!as_compare_int_match (current_memory, as_relation_get_compare (relation), req_memory)) {
			const gchar *compare_symbols = as_relation_compare_to_symbols_string (as_relation_get_compare (relation));
			if (priv->kind == AS_RELATION_KIND_REQUIRES)
				_as_set_satify_message (message,
							/* TRANSLATORS: We checked a memory dependency, the first placeholder is the comparison operator (e.g. >=),
							   second is the expected amount of memory and fourth is the amount of memory we have. */
							g_strdup_printf (_("This software requires %s %.2f GiB of memory, but this system has %.2f GiB."),
									compare_symbols, req_memory / 1024.0, current_memory / 1024.0));
			else if (priv->kind == AS_RELATION_KIND_RECOMMENDS)
				_as_set_satify_message (message,
							/* TRANSLATORS: We checked a memory dependency, the first placeholder is the comparison operator (e.g. >=),
							   second is the expected amount of memory and fourth is the amount of memory we have. */
							g_strdup_printf (_("This software recommends %s %.2f GiB of memory, but this system has %.2f GiB."),
									compare_symbols, req_memory / 1024.0, current_memory / 1024.0));
			else if (priv->kind == AS_RELATION_KIND_SUPPORTS) {
				_as_set_satify_message (message,
							/* TRANSLATORS: We checked a memory dependency, the first placeholder is the comparison operator (e.g. >=),
							   second is the expected amount of memory. */
							g_strdup_printf (_("This software supports %s %.2f GiB of memory."),
									compare_symbols, req_memory / 1024.0));
				/* this is not an error, supports is only a hint for memory */
				return AS_CHECK_RESULT_TRUE;
			}
			return AS_CHECK_RESULT_FALSE;
		}

		_as_set_satify_message (message,
					g_strdup_printf (_("This system has sufficient memory for this software.")));

		/* if we are here, we have sufficient memory */
		return AS_CHECK_RESULT_TRUE;
	}

	/* User Input Controls */
	if (priv->item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
		AsCheckResult res;
		AsControlKind control_kind;
		GError *tmp_error = NULL;

		control_kind = as_relation_get_value_control_kind (relation);
		res = as_system_info_has_input_control (sysinfo,
							control_kind,
							&tmp_error);
		if (res == AS_CHECK_RESULT_ERROR) {
			g_propagate_error (error, tmp_error);
			return AS_CHECK_RESULT_ERROR;
		}

		if (res == AS_CHECK_RESULT_FALSE || priv->kind == AS_RELATION_KIND_SUPPORTS)
			_as_set_satify_message (message,
						_as_get_control_missing_message (control_kind, priv->kind));
		if (res == AS_CHECK_RESULT_TRUE)
			_as_set_satify_message (message,
						_as_get_control_found_message (control_kind));

		return res;
	}

	/* Display Length */
	if (priv->item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
		gulong req_length;
		gulong current_length;
		AsDisplaySideKind side_kind;

		req_length = as_relation_get_value_px (relation);
		side_kind = as_relation_get_display_side_kind (relation);
		if (req_length == 0) {
			g_set_error_literal (error,
						AS_RELATION_ERROR,
						AS_RELATION_ERROR_BAD_VALUE,
						"Unable to check display size relation: No valid size value set in metadata.");
			return AS_CHECK_RESULT_ERROR;
		}
		if (side_kind == AS_DISPLAY_SIDE_KIND_UNKNOWN) {
			g_set_error_literal (error,
						AS_RELATION_ERROR,
						AS_RELATION_ERROR_BAD_VALUE,
						"Unable to check display size relation: No valid side type value set in metadata.");
			return AS_CHECK_RESULT_ERROR;
		}

		current_length = as_system_info_get_display_length (sysinfo, side_kind);
		if (current_length == 0) {
			g_set_error_literal (error,
						AS_SYSTEM_INFO_ERROR,
						AS_SYSTEM_INFO_ERROR_NOT_FOUND,
						"Unable to determine the display length of this device: This value needs to be provided "
						"by a GUI frontend for AppStream.");
			return AS_CHECK_RESULT_ERROR;
		}

		if (!as_compare_int_match (current_length, as_relation_get_compare (relation), req_length)) {
			const gchar *compare_symbols = as_relation_compare_to_symbols_string (as_relation_get_compare (relation));
			if (priv->kind == AS_RELATION_KIND_REQUIRES) {
				if (side_kind == AS_DISPLAY_SIDE_KIND_LONGEST)
					_as_set_satify_message (message,
								/* TRANSLATORS: We checked a display size dependency, the first placeholder is the comparison operator (e.g. >=),
								   second is the expected size and fourth is the size the current device has. */
								g_strdup_printf (_("This software requires a display with its longest edge being %s %lu px in size, "
										   "but the display of this device has %lu px."),
										compare_symbols, req_length, current_length));
				else
					_as_set_satify_message (message,
								/* TRANSLATORS: We checked a display size dependency, the first placeholder is the comparison operator (e.g. >=),
								   second is the expected size and fourth is the size the current device has. */
								g_strdup_printf (_("This software requires a display with its shortest edge being %s %lu px in size, "
										   "but the display of this device has %lu px."),
										compare_symbols, req_length, current_length));
			} else if (priv->kind == AS_RELATION_KIND_RECOMMENDS) {
				if (side_kind == AS_DISPLAY_SIDE_KIND_LONGEST)
					_as_set_satify_message (message,
								/* TRANSLATORS: We checked a display size dependency, the first placeholder is the comparison operator (e.g. >=),
								   second is the expected size and fourth is the size the current device has. */
								g_strdup_printf (_("This software recommends a display with its longest edge being %s %lu px in size, "
										   "but the display of this device has %lu px."),
										compare_symbols, req_length, current_length));
				else
					_as_set_satify_message (message,
								/* TRANSLATORS: We checked a display size dependency, the first placeholder is the comparison operator (e.g. >=),
								   second is the expected size and fourth is the size the current device has. */
								g_strdup_printf (_("This software recommends a display with its shortest edge being %s %lu px in size, "
										   "but the display of this device has %lu px."),
										compare_symbols, req_length, current_length));
			}
			return AS_CHECK_RESULT_FALSE;
		}

		_as_set_satify_message (message,
					g_strdup (_("Display size is sufficient for this software.")));

		/* if we are here, we have sufficient memory */
		return AS_CHECK_RESULT_TRUE;
	}

	/* TODO: Still needs implementation:
	 *   AS_RELATION_ITEM_KIND_FIRMWARE
	 *   AS_RELATION_ITEM_KIND_HARDWARE
	 *   AS_RELATION_ITEM_KIND_INTERNET
	 */

	g_set_error (error,
		     AS_RELATION_ERROR,
		     AS_RELATION_ERROR_NOT_IMPLEMENTED,
		     _("Relation satisfy check for items of type '%s' is not implemented yet."),
		     as_relation_item_kind_to_string (priv->item_kind));

	return AS_CHECK_RESULT_ERROR;
}

/**
 * as_relation_new:
 *
 * Creates a new #AsRelation.
 *
 * Returns: (transfer full): a #AsRelation
 *
 * Since: 0.11.0
 **/
AsRelation*
as_relation_new (void)
{
	AsRelation *relation;
	relation = g_object_new (AS_TYPE_RELATION, NULL);
	return AS_RELATION (relation);
}
