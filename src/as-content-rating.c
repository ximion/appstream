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

/**
 * SECTION:as-content-rating
 * @short_description: Object representing a content rating
 * @include: appstream.h
 * @stability: Unstable
 *
 * Content ratings are age-specific guidelines for applications.
 *
 * See also: #AsComponent
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include "as-content-rating.h"
#include "as-content-rating-private.h"
#include "as-utils-private.h"

typedef struct {
	gchar *id;
	AsContentRatingValue	 value;
} AsContentRatingKey;

typedef struct
{
	gchar *kind;
	GPtrArray *keys; /* of AsContentRatingKey */
} AsContentRatingPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsContentRating, as_content_rating, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_content_rating_get_instance_private (o))

typedef enum
{
	OARS_1_0,
	OARS_1_1,
} OarsVersion;

static gboolean is_oars_key (const gchar *id, OarsVersion version);

static void
as_content_rating_finalize (GObject *object)
{
	AsContentRating *content_rating = AS_CONTENT_RATING (object);
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);

	if (priv->kind != NULL)
		g_free (priv->kind);
	g_ptr_array_unref (priv->keys);

	G_OBJECT_CLASS (as_content_rating_parent_class)->finalize (object);
}

static void
as_content_rating_key_free (AsContentRatingKey *key)
{
	as_ref_string_release (key->id);
	g_slice_free (AsContentRatingKey, key);
}

static void
as_content_rating_init (AsContentRating *content_rating)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	priv->keys = g_ptr_array_new_with_free_func ((GDestroyNotify) as_content_rating_key_free);
}

static void
as_content_rating_class_init (AsContentRatingClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_content_rating_finalize;
}

static gint
ids_sort_cb (gconstpointer id_ptr_a, gconstpointer id_ptr_b)
{
	const gchar *id_a = *((const gchar **) id_ptr_a);
	const gchar *id_b = *((const gchar **) id_ptr_b);

	return g_strcmp0 (id_a, id_b);
}

/**
 * as_content_rating_get_rating_ids:
 * @content_rating: a #AsContentRating
 *
 * Gets the set of ratings IDs which are present in this @content_rating. An
 * example of a ratings ID is `violence-bloodshed`.
 *
 * The IDs are returned in lexicographical order.
 *
 * Returns: (array zero-terminated=1) (transfer container): %NULL-terminated
 *    array of ratings IDs; each ratings ID is owned by the #AsContentRating and
 *    must not be freed, but the container must be freed with g_free()
 *
 * Since: 0.12.10
 **/
const gchar **
as_content_rating_get_rating_ids (AsContentRating *content_rating)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	GPtrArray *ids = g_ptr_array_new_with_free_func (NULL);
	guint i;

	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), NULL);

	for (i = 0; i < priv->keys->len; i++) {
		AsContentRatingKey *key = g_ptr_array_index (priv->keys, i);
		g_ptr_array_add (ids, key->id);
	}

	g_ptr_array_sort (ids, ids_sort_cb);
	g_ptr_array_add (ids, NULL);  /* NULL terminator */

	return (const gchar **) g_ptr_array_free (g_steal_pointer (&ids), FALSE);
}

/**
 * as_content_rating_set_value:
 * @content_rating: a #AsContentRating
 * @id: A ratings ID, e.g. `violence-bloodshed`.
 * @value: A #AsContentRatingValue, e.g. %AS_CONTENT_RATING_VALUE_INTENSE
 *
 * Sets the value of a content rating key.
 *
 * Since: 0.11.0
 **/
void
as_content_rating_set_value (AsContentRating *content_rating, const gchar *id, AsContentRatingValue value)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	AsContentRatingKey *key;

	g_return_if_fail (id != NULL);
	g_return_if_fail (value != AS_CONTENT_RATING_VALUE_UNKNOWN);

	key = g_slice_new0 (AsContentRatingKey);
	key->id = g_ref_string_new_intern (id);
	key->value = value;
	g_ptr_array_add (priv->keys, key);
}

/**
 * as_content_rating_get_value:
 * @content_rating: a #AsContentRating
 * @id: A ratings ID, e.g. `violence-bloodshed`.
 *
 * Gets the value of a content rating key.
 *
 * Returns: the #AsContentRatingValue, or %AS_CONTENT_RATING_VALUE_UNKNOWN
 *
 * Since: 0.11.0
 **/
AsContentRatingValue
as_content_rating_get_value (AsContentRating *content_rating, const gchar *id)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), AS_CONTENT_RATING_VALUE_UNKNOWN);

	for (guint i = 0; i < priv->keys->len; i++) {
		AsContentRatingKey *key = g_ptr_array_index (priv->keys, i);
		if (g_strcmp0 (key->id, id) == 0)
			return key->value;
	}

	/* According to the
	 * [OARS specification](https://github.com/hughsie/oars/blob/master/specification/oars-1.1.md),
	 * return %AS_CONTENT_RATING_VALUE_NONE if the #AsContentRating exists
	 * overall. Only return %AS_CONTENT_RATING_VALUE_UNKNOWN if the
	 * #AsContentRating doesn’t exist at all (or for other types of content
	 * rating). */
	if ((g_strcmp0 (priv->kind, "oars-1.0") == 0 && is_oars_key (id, OARS_1_0)) ||
	    (g_strcmp0 (priv->kind, "oars-1.1") == 0 && is_oars_key (id, OARS_1_1)))
		return AS_CONTENT_RATING_VALUE_NONE;
	else
		return AS_CONTENT_RATING_VALUE_UNKNOWN;
}

/**
 * as_content_rating_value_to_string:
 * @value: the #AsContentRatingValue.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @value
 *
 * Since: 0.11.0
 **/
const gchar *
as_content_rating_value_to_string (AsContentRatingValue value)
{
	if (value == AS_CONTENT_RATING_VALUE_NONE)
		return "none";
	if (value == AS_CONTENT_RATING_VALUE_MILD)
		return "mild";
	if (value == AS_CONTENT_RATING_VALUE_MODERATE)
		return "moderate";
	if (value == AS_CONTENT_RATING_VALUE_INTENSE)
		return "intense";
	return "unknown";
}

/**
 * as_content_rating_value_from_string:
 * @value: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsContentRatingValue or %AS_CONTENT_RATING_VALUE_UNKNOWN for unknown
 *
 * Since: 0.11.0
 **/
AsContentRatingValue
as_content_rating_value_from_string (const gchar *value)
{
	if (g_strcmp0 (value, "none") == 0)
		return AS_CONTENT_RATING_VALUE_NONE;
	if (g_strcmp0 (value, "mild") == 0)
		return AS_CONTENT_RATING_VALUE_MILD;
	if (g_strcmp0 (value, "moderate") == 0)
		return AS_CONTENT_RATING_VALUE_MODERATE;
	if (g_strcmp0 (value, "intense") == 0)
		return AS_CONTENT_RATING_VALUE_INTENSE;
	return AS_CONTENT_RATING_VALUE_UNKNOWN;
}

static const gchar *rating_system_names[] = {
	[AS_CONTENT_RATING_SYSTEM_UNKNOWN] = NULL,
	[AS_CONTENT_RATING_SYSTEM_INCAA] = "INCAA",
	[AS_CONTENT_RATING_SYSTEM_ACB] = "ACB",
	[AS_CONTENT_RATING_SYSTEM_DJCTQ] = "DJCTQ",
	[AS_CONTENT_RATING_SYSTEM_GSRR] = "GSRR",
	[AS_CONTENT_RATING_SYSTEM_PEGI] = "PEGI",
	[AS_CONTENT_RATING_SYSTEM_KAVI] = "KAVI",
	[AS_CONTENT_RATING_SYSTEM_USK] = "USK",
	[AS_CONTENT_RATING_SYSTEM_ESRA] = "ESRA",
	[AS_CONTENT_RATING_SYSTEM_CERO] = "CERO",
	[AS_CONTENT_RATING_SYSTEM_OFLCNZ] = "OFLCNZ",
	[AS_CONTENT_RATING_SYSTEM_RUSSIA] = "RUSSIA",
	[AS_CONTENT_RATING_SYSTEM_MDA] = "MDA",
	[AS_CONTENT_RATING_SYSTEM_GRAC] = "GRAC",
	[AS_CONTENT_RATING_SYSTEM_ESRB] = "ESRB",
	[AS_CONTENT_RATING_SYSTEM_IARC] = "IARC",
};
G_STATIC_ASSERT (G_N_ELEMENTS (rating_system_names) == AS_CONTENT_RATING_SYSTEM_LAST);

/**
 * as_content_rating_system_to_string:
 * @system: an #AsContentRatingSystem
 *
 * Get a human-readable string to identify @system. %NULL will be returned for
 * %AS_CONTENT_RATING_SYSTEM_UNKNOWN.
 *
 * Returns: (nullable): a human-readable string for @system, or %NULL if unknown
 * Since: 0.12.12
 */
const gchar *
as_content_rating_system_to_string (AsContentRatingSystem system)
{
	if ((gint) system < AS_CONTENT_RATING_SYSTEM_UNKNOWN ||
	    (gint) system >= AS_CONTENT_RATING_SYSTEM_LAST)
		return NULL;

	return rating_system_names[system];
}

static char *
get_esrb_string (const gchar *source, const gchar *translate)
{
	if (g_strcmp0 (source, translate) == 0)
		return g_strdup (source);
	/* TRANSLATORS: This is the formatting of English and localized name
	 * of the rating e.g. "Adults Only (solo adultos)" */
	return g_strdup_printf (_("%s (%s)"), source, translate);
}

/**
 * as_content_rating_system_format_age:
 * @system: an #AsContentRatingSystem
 * @age: a CSM age to format
 *
 * Format @age as a human-readable string in the given rating @system. This is
 * the way to present system-specific strings in a UI.
 *
 * Returns: (transfer full) (nullable): a newly allocated formatted version of
 *    @age, or %NULL if the given @system has no representation for @age
 * Since: 0.12.12
 */
/* data obtained from https://en.wikipedia.org/wiki/Video_game_rating_system */
gchar *
as_content_rating_system_format_age (AsContentRatingSystem system, guint age)
{
	if (system == AS_CONTENT_RATING_SYSTEM_INCAA) {
		if (age >= 18)
			return g_strdup ("+18");
		if (age >= 13)
			return g_strdup ("+13");
		return g_strdup ("ATP");
	}
	if (system == AS_CONTENT_RATING_SYSTEM_ACB) {
		if (age >= 18)
			return g_strdup ("R18+");
		if (age >= 15)
			return g_strdup ("MA15+");
		return g_strdup ("PG");
	}
	if (system == AS_CONTENT_RATING_SYSTEM_DJCTQ) {
		if (age >= 18)
			return g_strdup ("18");
		if (age >= 16)
			return g_strdup ("16");
		if (age >= 14)
			return g_strdup ("14");
		if (age >= 12)
			return g_strdup ("12");
		if (age >= 10)
			return g_strdup ("10");
		return g_strdup ("L");
	}
	if (system == AS_CONTENT_RATING_SYSTEM_GSRR) {
		if (age >= 18)
			return g_strdup ("限制");
		if (age >= 15)
			return g_strdup ("輔15");
		if (age >= 12)
			return g_strdup ("輔12");
		if (age >= 6)
			return g_strdup ("保護");
		return g_strdup ("普通");
	}
	if (system == AS_CONTENT_RATING_SYSTEM_PEGI) {
		if (age >= 18)
			return g_strdup ("18");
		if (age >= 16)
			return g_strdup ("16");
		if (age >= 12)
			return g_strdup ("12");
		if (age >= 7)
			return g_strdup ("7");
		if (age >= 3)
			return g_strdup ("3");
		return NULL;
	}
	if (system == AS_CONTENT_RATING_SYSTEM_KAVI) {
		if (age >= 18)
			return g_strdup ("18+");
		if (age >= 16)
			return g_strdup ("16+");
		if (age >= 12)
			return g_strdup ("12+");
		if (age >= 7)
			return g_strdup ("7+");
		if (age >= 3)
			return g_strdup ("3+");
		return NULL;
	}
	if (system == AS_CONTENT_RATING_SYSTEM_USK) {
		if (age >= 18)
			return g_strdup ("18");
		if (age >= 16)
			return g_strdup ("16");
		if (age >= 12)
			return g_strdup ("12");
		if (age >= 6)
			return g_strdup ("6");
		return g_strdup ("0");
	}
	/* Reference: http://www.esra.org.ir/ */
	if (system == AS_CONTENT_RATING_SYSTEM_ESRA) {
		if (age >= 18)
			return g_strdup ("+18");
		if (age >= 15)
			return g_strdup ("+15");
		if (age >= 12)
			return g_strdup ("+12");
		if (age >= 7)
			return g_strdup ("+7");
		if (age >= 3)
			return g_strdup ("+3");
		return NULL;
	}
	if (system == AS_CONTENT_RATING_SYSTEM_CERO) {
		if (age >= 18)
			return g_strdup ("Z");
		if (age >= 17)
			return g_strdup ("D");
		if (age >= 15)
			return g_strdup ("C");
		if (age >= 12)
			return g_strdup ("B");
		return g_strdup ("A");
	}
	if (system == AS_CONTENT_RATING_SYSTEM_OFLCNZ) {
		if (age >= 18)
			return g_strdup ("R18");
		if (age >= 16)
			return g_strdup ("R16");
		if (age >= 15)
			return g_strdup ("R15");
		if (age >= 13)
			return g_strdup ("R13");
		return g_strdup ("G");
	}
	if (system == AS_CONTENT_RATING_SYSTEM_RUSSIA) {
		if (age >= 18)
			return g_strdup ("18+");
		if (age >= 16)
			return g_strdup ("16+");
		if (age >= 12)
			return g_strdup ("12+");
		if (age >= 6)
			return g_strdup ("6+");
		return g_strdup ("0+");
	}
	if (system == AS_CONTENT_RATING_SYSTEM_MDA) {
		if (age >= 18)
			return g_strdup ("M18");
		if (age >= 16)
			return g_strdup ("ADV");
		return get_esrb_string ("General", _("General"));
	}
	if (system == AS_CONTENT_RATING_SYSTEM_GRAC) {
		if (age >= 18)
			return g_strdup ("18");
		if (age >= 15)
			return g_strdup ("15");
		if (age >= 12)
			return g_strdup ("12");
		return get_esrb_string ("ALL", _("ALL"));
	}
	if (system == AS_CONTENT_RATING_SYSTEM_ESRB) {
		if (age >= 18)
			return get_esrb_string ("Adults Only", _("Adults Only"));
		if (age >= 17)
			return get_esrb_string ("Mature", _("Mature"));
		if (age >= 13)
			return get_esrb_string ("Teen", _("Teen"));
		if (age >= 10)
			return get_esrb_string ("Everyone 10+", _("Everyone 10+"));
		if (age >= 6)
			return get_esrb_string ("Everyone", _("Everyone"));

		return get_esrb_string ("Early Childhood", _("Early Childhood"));
	}
	/* IARC = everything else */
	if (age >= 18)
		return g_strdup ("18+");
	if (age >= 16)
		return g_strdup ("16+");
	if (age >= 12)
		return g_strdup ("12+");
	if (age >= 7)
		return g_strdup ("7+");
	if (age >= 3)
		return g_strdup ("3+");
	return NULL;
}

static const gchar *content_rating_strings[AS_CONTENT_RATING_SYSTEM_LAST][7] = {
	/* AS_CONTENT_RATING_SYSTEM_UNKNOWN is handled in code */
	[AS_CONTENT_RATING_SYSTEM_INCAA] = { "ATP", "+13", "+18", NULL },
	[AS_CONTENT_RATING_SYSTEM_ACB] = { "PG", "MA15+", "R18+", NULL },
	[AS_CONTENT_RATING_SYSTEM_DJCTQ] = { "L", "10", "12", "14", "16", "18", NULL },
	[AS_CONTENT_RATING_SYSTEM_GSRR] = { "普通", "保護", "輔12", "輔15", "限制", NULL },
	[AS_CONTENT_RATING_SYSTEM_PEGI] = { "3", "7", "12", "16", "18", NULL },
	[AS_CONTENT_RATING_SYSTEM_KAVI] = { "3+", "7+", "12+", "16+", "18+", NULL },
	[AS_CONTENT_RATING_SYSTEM_USK] = { "0", "6", "12", "16", "18", NULL },
	[AS_CONTENT_RATING_SYSTEM_ESRA] = { "+3", "+7", "+12", "+15", "+18", NULL },
	[AS_CONTENT_RATING_SYSTEM_CERO] = { "A", "B", "C", "D", "Z", NULL },
	[AS_CONTENT_RATING_SYSTEM_OFLCNZ] = { "G", "R13", "R15", "R16", "R18", NULL },
	[AS_CONTENT_RATING_SYSTEM_RUSSIA] = { "0+", "6+", "12+", "16+", "18+", NULL },
	[AS_CONTENT_RATING_SYSTEM_MDA] = { "General", "ADV", "M18", NULL },
	[AS_CONTENT_RATING_SYSTEM_GRAC] = { "ALL", "12", "15", "18", NULL },
	/* Note: ESRB has locale-specific suffixes, so needs special further
	 * handling in code. These strings are just the locale-independent parts. */
	[AS_CONTENT_RATING_SYSTEM_ESRB] = { "Early Childhood", "Everyone", "Everyone 10+", "Teen", "Mature", "Adults Only", NULL },
	[AS_CONTENT_RATING_SYSTEM_IARC] = { "3+", "7+", "12+", "16+", "18+", NULL },
};

/**
 * as_content_rating_add_attribute:
 * @content_rating: a #AsContentRating instance.
 * @id: a content rating ID, e.g. `money-gambling`.
 * @value: a #AsContentRatingValue, e.g. %AS_CONTENT_RATING_VALUE_MODERATE.
 *
 * Adds an attribute value to the content rating.
 *
 * Since: 0.14.0
 **/
void
as_content_rating_add_attribute (AsContentRating *content_rating,
				 const gchar *id,
				 AsContentRatingValue value)
{
	AsContentRatingKey *key = g_slice_new0 (AsContentRatingKey);
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);

	g_return_if_fail (AS_IS_CONTENT_RATING (content_rating));
	g_return_if_fail (id != NULL);
	g_return_if_fail (value != AS_CONTENT_RATING_VALUE_UNKNOWN);

	key->id = g_ref_string_new_intern (id);
	key->value = value;
	g_ptr_array_add (priv->keys, key);
}

/**
 * as_content_rating_system_get_formatted_ages:
 * @system: an #AsContentRatingSystem
 *
 * Get an array of all the possible return values of
 * as_content_rating_system_format_age() for the given @system. The array is
 * sorted with youngest CSM age first.
 *
 * Returns: (transfer full): %NULL-terminated array of human-readable age strings
 * Since: 0.12.12
 */
gchar **
as_content_rating_system_get_formatted_ages (AsContentRatingSystem system)
{
	g_return_val_if_fail ((int) system < AS_CONTENT_RATING_SYSTEM_LAST, NULL);

	/* IARC is the fallback for everything */
	if (system == AS_CONTENT_RATING_SYSTEM_UNKNOWN)
		system = AS_CONTENT_RATING_SYSTEM_IARC;

	/* ESRB is special as it requires localised suffixes */
	if (system == AS_CONTENT_RATING_SYSTEM_ESRB) {
		g_auto(GStrv) esrb_ages = g_new0 (gchar *, 7);

		esrb_ages[0] = get_esrb_string (content_rating_strings[system][0], _("Early Childhood"));
		esrb_ages[1] = get_esrb_string (content_rating_strings[system][1], _("Everyone"));
		esrb_ages[2] = get_esrb_string (content_rating_strings[system][2], _("Everyone 10+"));
		esrb_ages[3] = get_esrb_string (content_rating_strings[system][3], _("Teen"));
		esrb_ages[4] = get_esrb_string (content_rating_strings[system][4], _("Mature"));
		esrb_ages[5] = get_esrb_string (content_rating_strings[system][5], _("Adults Only"));
		esrb_ages[6] = NULL;

		return g_steal_pointer (&esrb_ages);
	}

	return g_strdupv ((gchar **) content_rating_strings[system]);
}

static guint content_rating_csm_ages[AS_CONTENT_RATING_SYSTEM_LAST][7] = {
	/* AS_CONTENT_RATING_SYSTEM_UNKNOWN is handled in code */
	[AS_CONTENT_RATING_SYSTEM_INCAA] = { 0, 13, 18 },
	[AS_CONTENT_RATING_SYSTEM_ACB] = { 0, 15, 18 },
	[AS_CONTENT_RATING_SYSTEM_DJCTQ] = { 0, 10, 12, 14, 16, 18 },
	[AS_CONTENT_RATING_SYSTEM_GSRR] = { 0, 6, 12, 15, 18 },
	[AS_CONTENT_RATING_SYSTEM_PEGI] = { 3, 7, 12, 16, 18 },
	[AS_CONTENT_RATING_SYSTEM_KAVI] = { 3, 7, 12, 16, 18 },
	[AS_CONTENT_RATING_SYSTEM_USK] = { 0, 6, 12, 16, 18 },
	[AS_CONTENT_RATING_SYSTEM_ESRA] = { 3, 7, 12, 15, 18 },
	[AS_CONTENT_RATING_SYSTEM_CERO] = { 0, 12, 15, 17, 18 },
	[AS_CONTENT_RATING_SYSTEM_OFLCNZ] = { 0, 13, 15, 16, 18 },
	[AS_CONTENT_RATING_SYSTEM_RUSSIA] = { 0, 6, 12, 16, 18 },
	[AS_CONTENT_RATING_SYSTEM_MDA] = { 0, 16, 18 },
	[AS_CONTENT_RATING_SYSTEM_GRAC] = { 0, 12, 15, 18 },
	[AS_CONTENT_RATING_SYSTEM_ESRB] = { 0, 6, 10, 13, 17, 18 },
	[AS_CONTENT_RATING_SYSTEM_IARC] = { 3, 7, 12, 16, 18 },
};

/**
 * as_content_rating_system_get_csm_ages:
 * @system: an #AsContentRatingSystem
 * @length_out: (out) (not nullable): return location for the length of the
 *    returned array
 *
 * Get the CSM ages corresponding to the entries returned by
 * as_content_rating_system_get_formatted_ages() for this @system.
 *
 * Returns: (transfer container) (array length=length_out): an array of CSM ages
 * Since: 0.12.12
 */
const guint *
as_content_rating_system_get_csm_ages (AsContentRatingSystem system, gsize *length_out)
{
	g_return_val_if_fail ((int) system < AS_CONTENT_RATING_SYSTEM_LAST, NULL);
	g_return_val_if_fail (length_out != NULL, NULL);

	/* IARC is the fallback for everything */
	if (system == AS_CONTENT_RATING_SYSTEM_UNKNOWN)
		system = AS_CONTENT_RATING_SYSTEM_IARC;

	*length_out = g_strv_length ((gchar **) content_rating_strings[system]);
	return content_rating_csm_ages[system];
}

/*
 * parse_locale:
 * @locale: (transfer full): a locale to parse
 * @language_out: (out) (optional) (nullable): return location for the parsed
 *    language, or %NULL to ignore
 * @territory_out: (out) (optional) (nullable): return location for the parsed
 *    territory, or %NULL to ignore
 * @codeset_out: (out) (optional) (nullable): return location for the parsed
 *    codeset, or %NULL to ignore
 * @modifier_out: (out) (optional) (nullable): return location for the parsed
 *    modifier, or %NULL to ignore
 *
 * Parse @locale as a locale string of the form
 * `language[_territory][.codeset][@modifier]` — see `man 3 setlocale` for
 * details.
 *
 * On success, %TRUE will be returned, and the components of the locale will be
 * returned in the given addresses, with each component not including any
 * separators. Otherwise, %FALSE will be returned and the components will be set
 * to %NULL.
 *
 * @locale is modified, and any returned non-%NULL pointers will point inside
 * it.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
static gboolean
parse_locale (gchar *locale  /* (transfer full) */,
	      const gchar **language_out,
	      const gchar **territory_out,
	      const gchar **codeset_out,
	      const gchar **modifier_out)
{
	gchar *separator;
	const gchar *language = NULL, *territory = NULL, *codeset = NULL, *modifier = NULL;

	separator = strrchr (locale, '@');
	if (separator != NULL) {
		modifier = separator + 1;
		*separator = '\0';
	}

	separator = strrchr (locale, '.');
	if (separator != NULL) {
		codeset = separator + 1;
		*separator = '\0';
	}

	separator = strrchr (locale, '_');
	if (separator != NULL) {
		territory = separator + 1;
		*separator = '\0';
	}

	language = locale;

	/* Parse failure? */
	if (*language == '\0') {
		language = NULL;
		territory = NULL;
		codeset = NULL;
		modifier = NULL;
	}

	if (language_out != NULL)
		*language_out = language;
	if (territory_out != NULL)
		*territory_out = territory;
	if (codeset_out != NULL)
		*codeset_out = codeset;
	if (modifier_out != NULL)
		*modifier_out = modifier;

	return (language != NULL);
}

/**
 * as_content_rating_system_from_locale:
 * @locale: a locale, in the format described in `man 3 setlocale`
 *
 * Determine the most appropriate #AsContentRatingSystem for the given @locale.
 * Content rating systems are selected by territory. If no content rating system
 * seems suitable, %AS_CONTENT_RATING_SYSTEM_IARC is returned.
 *
 * Returns: the most relevant #AsContentRatingSystem
 * Since: 0.12.12
 */
/* data obtained from https://en.wikipedia.org/wiki/Video_game_rating_system */
AsContentRatingSystem
as_content_rating_system_from_locale (const gchar *locale)
{
	g_autofree gchar *locale_copy = g_strdup (locale);
	const gchar *territory;

	/* Default to IARC for locales which can’t be parsed. */
	if (!parse_locale (locale_copy, NULL, &territory, NULL, NULL))
		return AS_CONTENT_RATING_SYSTEM_IARC;

	/* Argentina */
	if (g_strcmp0 (territory, "AR") == 0)
		return AS_CONTENT_RATING_SYSTEM_INCAA;

	/* Australia */
	if (g_strcmp0 (territory, "AU") == 0)
		return AS_CONTENT_RATING_SYSTEM_ACB;

	/* Brazil */
	if (g_strcmp0 (territory, "BR") == 0)
		return AS_CONTENT_RATING_SYSTEM_DJCTQ;

	/* Taiwan */
	if (g_strcmp0 (territory, "TW") == 0)
		return AS_CONTENT_RATING_SYSTEM_GSRR;

	/* Europe (but not Finland or Germany), India, Israel,
	 * Pakistan, Quebec, South Africa */
	if ((g_strcmp0 (territory, "GB") == 0) ||
	    g_strcmp0 (territory, "AL") == 0 ||
	    g_strcmp0 (territory, "AD") == 0 ||
	    g_strcmp0 (territory, "AM") == 0 ||
	    g_strcmp0 (territory, "AT") == 0 ||
	    g_strcmp0 (territory, "AZ") == 0 ||
	    g_strcmp0 (territory, "BY") == 0 ||
	    g_strcmp0 (territory, "BE") == 0 ||
	    g_strcmp0 (territory, "BA") == 0 ||
	    g_strcmp0 (territory, "BG") == 0 ||
	    g_strcmp0 (territory, "HR") == 0 ||
	    g_strcmp0 (territory, "CY") == 0 ||
	    g_strcmp0 (territory, "CZ") == 0 ||
	    g_strcmp0 (territory, "DK") == 0 ||
	    g_strcmp0 (territory, "EE") == 0 ||
	    g_strcmp0 (territory, "FR") == 0 ||
	    g_strcmp0 (territory, "GE") == 0 ||
	    g_strcmp0 (territory, "GR") == 0 ||
	    g_strcmp0 (territory, "HU") == 0 ||
	    g_strcmp0 (territory, "IS") == 0 ||
	    g_strcmp0 (territory, "IT") == 0 ||
	    g_strcmp0 (territory, "LZ") == 0 ||
	    g_strcmp0 (territory, "XK") == 0 ||
	    g_strcmp0 (territory, "LV") == 0 ||
	    g_strcmp0 (territory, "FL") == 0 ||
	    g_strcmp0 (territory, "LU") == 0 ||
	    g_strcmp0 (territory, "LT") == 0 ||
	    g_strcmp0 (territory, "MK") == 0 ||
	    g_strcmp0 (territory, "MT") == 0 ||
	    g_strcmp0 (territory, "MD") == 0 ||
	    g_strcmp0 (territory, "MC") == 0 ||
	    g_strcmp0 (territory, "ME") == 0 ||
	    g_strcmp0 (territory, "NL") == 0 ||
	    g_strcmp0 (territory, "NO") == 0 ||
	    g_strcmp0 (territory, "PL") == 0 ||
	    g_strcmp0 (territory, "PT") == 0 ||
	    g_strcmp0 (territory, "RO") == 0 ||
	    g_strcmp0 (territory, "SM") == 0 ||
	    g_strcmp0 (territory, "RS") == 0 ||
	    g_strcmp0 (territory, "SK") == 0 ||
	    g_strcmp0 (territory, "SI") == 0 ||
	    g_strcmp0 (territory, "ES") == 0 ||
	    g_strcmp0 (territory, "SE") == 0 ||
	    g_strcmp0 (territory, "CH") == 0 ||
	    g_strcmp0 (territory, "TR") == 0 ||
	    g_strcmp0 (territory, "UA") == 0 ||
	    g_strcmp0 (territory, "VA") == 0 ||
	    g_strcmp0 (territory, "IN") == 0 ||
	    g_strcmp0 (territory, "IL") == 0 ||
	    g_strcmp0 (territory, "PK") == 0 ||
	    g_strcmp0 (territory, "ZA") == 0)
		return AS_CONTENT_RATING_SYSTEM_PEGI;

	/* Finland */
	if (g_strcmp0 (territory, "FI") == 0)
		return AS_CONTENT_RATING_SYSTEM_KAVI;

	/* Germany */
	if (g_strcmp0 (territory, "DE") == 0)
		return AS_CONTENT_RATING_SYSTEM_USK;

	/* Iran */
	if (g_strcmp0 (territory, "IR") == 0)
		return AS_CONTENT_RATING_SYSTEM_ESRA;

	/* Japan */
	if (g_strcmp0 (territory, "JP") == 0)
		return AS_CONTENT_RATING_SYSTEM_CERO;

	/* New Zealand */
	if (g_strcmp0 (territory, "NZ") == 0)
		return AS_CONTENT_RATING_SYSTEM_OFLCNZ;

	/* Russia: Content rating law */
	if (g_strcmp0 (territory, "RU") == 0)
		return AS_CONTENT_RATING_SYSTEM_RUSSIA;

	/* Singapore */
	if (g_strcmp0 (territory, "SQ") == 0)
		return AS_CONTENT_RATING_SYSTEM_MDA;

	/* South Korea */
	if (g_strcmp0 (territory, "KR") == 0)
		return AS_CONTENT_RATING_SYSTEM_GRAC;

	/* USA, Canada, Mexico */
	if ((g_strcmp0 (territory, "US") == 0) ||
	    g_strcmp0 (territory, "CA") == 0 ||
	    g_strcmp0 (territory, "MX") == 0)
		return AS_CONTENT_RATING_SYSTEM_ESRB;

	/* everything else is IARC */
	return AS_CONTENT_RATING_SYSTEM_IARC;
}

/* Table of the human-readable descriptions for each #AsContentRatingValue for
 * each content rating category. @desc_none must be non-%NULL, but the other
 * values may be %NULL if no description is appropriate. In that case, the next
 * non-%NULL description for a lower #AsContentRatingValue will be used. */
static const struct {
	const gchar *id;  /* (not nullable) */
	const gchar *desc_none;  /* (not nullable) */
	const gchar *desc_mild;  /* (nullable) */
	const gchar *desc_moderate;  /* (nullable) */
	const gchar *desc_intense;  /* (nullable) */
} oars_descriptions[] = {
	{
		"violence-cartoon",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No cartoon violence"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Cartoon characters in unsafe situations"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Cartoon characters in aggressive conflict"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic violence involving cartoon characters"),
	},
	{
		"violence-fantasy",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No fantasy violence"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Characters in unsafe situations easily distinguishable from reality"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Characters in aggressive conflict easily distinguishable from reality"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic violence easily distinguishable from reality"),
	},
	{
		"violence-realistic",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No realistic violence"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Mildly realistic characters in unsafe situations"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Depictions of realistic characters in aggressive conflict"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic violence involving realistic characters"),
	},
	{
		"violence-bloodshed",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No bloodshed"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Unrealistic bloodshed"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Realistic bloodshed"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Depictions of bloodshed and the mutilation of body parts"),
	},
	{
		"violence-sexual",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No sexual violence"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Rape or other violent sexual behavior"),
		NULL,
		NULL,
	},
	{
		"drugs-alcohol",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to alcohol"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("References to alcoholic beverages"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Use of alcoholic beverages"),
		NULL,
	},
	{
		"drugs-narcotics",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to illicit drugs"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("References to illicit drugs"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Use of illicit drugs"),
		NULL,
	},
	{
		"drugs-tobacco",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to tobacco products"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("References to tobacco products"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Use of tobacco products"),
		NULL,
	},
	{
		"sex-nudity",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No nudity of any sort"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Brief artistic nudity"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Prolonged nudity"),
		NULL,
	},
	{
		"sex-themes",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to or depictions of sexual nature"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Provocative references or depictions"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Sexual references or depictions"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic sexual behavior"),
	},
	{
		"language-profanity",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No profanity of any kind"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Mild or infrequent use of profanity"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Moderate use of profanity"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Strong or frequent use of profanity"),
	},
	{
		"language-humor",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No inappropriate humor"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Slapstick humor"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Vulgar or bathroom humor"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Mature or sexual humor"),
	},
	{
		"language-discrimination",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No discriminatory language of any kind"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Negativity towards a specific group of people"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Discrimination designed to cause emotional harm"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Explicit discrimination based on gender, sexuality, race or religion"),
	},
	{
		"money-advertising",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No advertising of any kind"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Product placement"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Explicit references to specific brands or trademarked products"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Users are encouraged to purchase specific real-world items"),
	},
	{
		"money-gambling",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No gambling of any kind"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Gambling on random events using tokens or credits"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Gambling using “play” money"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Gambling using real money"),
	},
	{
		"money-purchasing",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No ability to spend money"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Users are encouraged to donate real money"),
		NULL,
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Ability to spend real money in-app"),
	},
	{
		"social-chat",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No way to chat with other users"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("User-to-user interactions without chat functionality"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Moderated chat functionality between users"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Uncontrolled chat functionality between users"),
	},
	{
		"social-audio",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No way to talk with other users"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Uncontrolled audio or video chat functionality between users"),
		NULL,
		NULL,
	},
	{
		"social-contacts",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No sharing of social network usernames or email addresses"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Sharing social network usernames or email addresses"),
		NULL,
		NULL,
	},
	{
		"social-info",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No sharing of user information with third parties"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Checking for the latest application version"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Sharing diagnostic data that does not let others identify the user"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Sharing information that lets others identify the user"),
	},
	{
		"social-location",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No sharing of physical location with other users"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Sharing physical location with other users"),
		NULL,
		NULL,
	},

	/* v1.1 */
	{
		/* Why is there an OARS category which discriminates based on sexual orientation?
		 * It’s because there are, very unfortunately, still countries in the world in
		 * which homosexuality, or software which refers to it, is illegal. In order to be
		 * able to ship FOSS in those countries, there needs to be a mechanism for apps to
		 * describe whether they refer to anything illegal, and for ratings mechanisms in
		 * those countries to filter out any apps which describe themselves as such.
		 *
		 * As a counterpoint, it’s illegal in many more countries to discriminate on the
		 * basis of sexual orientation, so this category is treated exactly the same as
		 * sex-themes (once the intensities of the ratings levels for both categories are
		 * normalised) in those countries.
		 *
		 * The differences between countries are handled through handling #AsContentRatingSystem
		 * values differently. */
		"sex-homosexuality",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to homosexuality"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Indirect references to homosexuality"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Kissing between people of the same gender"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic sexual behavior between people of the same gender"),
	},
	{
		"sex-prostitution",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to prostitution"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Indirect references to prostitution"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Direct references to prostitution"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic depictions of the act of prostitution"),
	},
	{
		"sex-adultery",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to adultery"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Indirect references to adultery"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Direct references to adultery"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic depictions of the act of adultery"),
	},
	{
		"sex-appearance",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No sexualized characters"),
		NULL,
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Scantily clad human characters"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Overtly sexualized human characters"),
	},
	{
		"violence-worship",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to desecration"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Depictions of or references to historical desecration"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Depictions of modern-day human desecration"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic depictions of modern-day desecration"),
	},
	{
		"violence-desecration",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No visible dead human remains"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Visible dead human remains"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Dead human remains that are exposed to the elements"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic depictions of desecration of human bodies"),
	},
	{
		"violence-slavery",
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("No references to slavery"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Depictions of or references to historical slavery"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Depictions of modern-day slavery"),
		/* TRANSLATORS: content rating description, see https://hughsie.github.io/oars/ */
		N_("Graphic depictions of modern-day slavery"),
	},
};

/**
 * as_content_rating_attribute_get_description:
 * @id: the subsection ID e.g. `violence-cartoon`
 * @value: the #AsContentRatingValue, e.g. %AS_CONTENT_RATING_VALUE_INTENSE
 *
 * Get a human-readable description of what content would be expected to
 * require the content rating attribute given by @id and @value.
 *
 * Returns: a human-readable description of @id and @value
 * Since: 0.12.12
 */
const gchar *
as_content_rating_attribute_get_description (const gchar *id, AsContentRatingValue value)
{
	gsize i;

	if ((gint) value < AS_CONTENT_RATING_VALUE_NONE ||
	    (gint) value > AS_CONTENT_RATING_VALUE_INTENSE)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS (oars_descriptions); i++) {
		if (!g_str_equal (oars_descriptions[i].id, id))
			continue;

		/* Return the most-intense non-NULL string. */
		if (oars_descriptions[i].desc_intense != NULL && value >= AS_CONTENT_RATING_VALUE_INTENSE)
			return _(oars_descriptions[i].desc_intense);
		if (oars_descriptions[i].desc_moderate != NULL && value >= AS_CONTENT_RATING_VALUE_MODERATE)
			return _(oars_descriptions[i].desc_moderate);
		if (oars_descriptions[i].desc_mild != NULL && value >= AS_CONTENT_RATING_VALUE_MILD)
			return _(oars_descriptions[i].desc_mild);
		if (oars_descriptions[i].desc_none != NULL && value >= AS_CONTENT_RATING_VALUE_NONE)
			return _(oars_descriptions[i].desc_none);
		g_assert_not_reached ();
	}

	/* This means the requested @id is missing from @oars_descriptions, so
	 * presumably the OARS spec has been updated but appstream-glib hasn’t. */
	g_warn_if_reached ();

	return NULL;
}

/* The struct definition below assumes we don’t grow more
 * #AsContentRating values. */
G_STATIC_ASSERT (AS_CONTENT_RATING_VALUE_LAST == AS_CONTENT_RATING_VALUE_INTENSE + 1);

static const struct {
	const gchar	*id;
	OarsVersion	 oars_version;  /* when the key was first added */
	guint		 csm_age_none;  /* for %AS_CONTENT_RATING_VALUE_NONE */
	guint		 csm_age_mild;  /* for %AS_CONTENT_RATING_VALUE_MILD */
	guint		 csm_age_moderate;  /* for %AS_CONTENT_RATING_VALUE_MODERATE */
	guint		 csm_age_intense;  /* for %AS_CONTENT_RATING_VALUE_INTENSE */
} oars_to_csm_mappings[] =  {
	/* Each @id must only appear once. The set of @csm_age_* values for a
	 * given @id must be complete and non-decreasing. */
	/* v1.0 */
	{ "violence-cartoon",	OARS_1_0, 0, 3, 4, 6 },
	{ "violence-fantasy",	OARS_1_0, 0, 3, 7, 8 },
	{ "violence-realistic",	OARS_1_0, 0, 4, 9, 14 },
	{ "violence-bloodshed",	OARS_1_0, 0, 9, 11, 18 },
	{ "violence-sexual",	OARS_1_0, 0, 18, 18, 18 },
	{ "drugs-alcohol",	OARS_1_0, 0, 11, 13, 16 },
	{ "drugs-narcotics",	OARS_1_0, 0, 12, 14, 17 },
	{ "drugs-tobacco",	OARS_1_0, 0, 10, 13, 13 },
	{ "sex-nudity",		OARS_1_0, 0, 12, 14, 14 },
	{ "sex-themes",		OARS_1_0, 0, 13, 14, 15 },
	{ "language-profanity",	OARS_1_0, 0, 8, 11, 14 },
	{ "language-humor",	OARS_1_0, 0, 3, 8, 14 },
	{ "language-discrimination", OARS_1_0, 0, 9, 10, 11 },
	{ "money-advertising",	OARS_1_0, 0, 7, 8, 10 },
	{ "money-gambling",	OARS_1_0, 0, 7, 10, 18 },
	{ "money-purchasing",	OARS_1_0, 0, 12, 14, 15 },
	{ "social-chat",	OARS_1_0, 0, 4, 10, 13 },
	{ "social-audio",	OARS_1_0, 0, 15, 15, 15 },
	{ "social-contacts",	OARS_1_0, 0, 12, 12, 12 },
	{ "social-info",	OARS_1_0, 0, 0, 13, 13 },
	{ "social-location",	OARS_1_0, 0, 13, 13, 13 },
	/* v1.1 additions */
	{ "sex-homosexuality",	OARS_1_1, 0, 13, 14, 15 },
	{ "sex-prostitution",	OARS_1_1, 0, 12, 14, 18 },
	{ "sex-adultery",	OARS_1_1, 0, 8, 10, 18 },
	{ "sex-appearance",	OARS_1_1, 0, 10, 10, 15 },
	{ "violence-worship",	OARS_1_1, 0, 13, 15, 18 },
	{ "violence-desecration", OARS_1_1, 0, 13, 15, 18 },
	{ "violence-slavery",	OARS_1_1, 0, 13, 15, 18 },
};

static gboolean
is_oars_key (const gchar *id, OarsVersion version)
{
	for (gsize i = 0; i < G_N_ELEMENTS (oars_to_csm_mappings); i++) {
		if (g_str_equal (id, oars_to_csm_mappings[i].id))
			return (oars_to_csm_mappings[i].oars_version <= version);
	}
	return FALSE;
}

/**
 * as_content_rating_attribute_to_csm_age:
 * @id: the subsection ID e.g. `violence-cartoon`
 * @value: the #AsContentRatingValue, e.g. %AS_CONTENT_RATING_VALUE_INTENSE
 *
 * Gets the Common Sense Media approved age for a specific rating level.
 *
 * Returns: The age in years, or 0 for no details.
 *
 * Since: 0.12.10
 **/
guint
as_content_rating_attribute_to_csm_age (const gchar *id, AsContentRatingValue value)
{
	if (value == AS_CONTENT_RATING_VALUE_UNKNOWN ||
	    value == AS_CONTENT_RATING_VALUE_LAST)
		return 0;

	for (gsize i = 0; i < G_N_ELEMENTS (oars_to_csm_mappings); i++) {
		if (g_str_equal (id, oars_to_csm_mappings[i].id)) {
			switch (value) {
			case AS_CONTENT_RATING_VALUE_NONE:
				return oars_to_csm_mappings[i].csm_age_none;
			case AS_CONTENT_RATING_VALUE_MILD:
				return oars_to_csm_mappings[i].csm_age_mild;
			case AS_CONTENT_RATING_VALUE_MODERATE:
				return oars_to_csm_mappings[i].csm_age_moderate;
			case AS_CONTENT_RATING_VALUE_INTENSE:
				return oars_to_csm_mappings[i].csm_age_intense;
			default:
				/* Handled above. */
				g_assert_not_reached ();
				return 0;
			}
		}
	}

	/* @id not found. */
	return 0;
}

/**
 * as_content_rating_attribute_from_csm_age:
 * @id: the subsection ID e.g. `violence-cartoon`
 * @age: the CSM age
 *
 * Gets the highest #AsContentRatingValue which is allowed to be seen by the
 * given Common Sense Media @age for the given subsection @id.
 *
 * For example, if the CSM age mappings for `violence-bloodshed` are:
 *  * age ≥ 0 for %AS_CONTENT_RATING_VALUE_NONE
 *  * age ≥ 9 for %AS_CONTENT_RATING_VALUE_MILD
 *  * age ≥ 11 for %AS_CONTENT_RATING_VALUE_MODERATE
 *  * age ≥ 18 for %AS_CONTENT_RATING_VALUE_INTENSE
 * then calling this function with `violence-bloodshed` and @age set to 17 would
 * return %AS_CONTENT_RATING_VALUE_MODERATE. Calling it with age 18 would
 * return %AS_CONTENT_RATING_VALUE_INTENSE.
 *
 * Returns: the #AsContentRatingValue, or %AS_CONTENT_RATING_VALUE_UNKNOWN if
 *    unknown
 * Since: 0.12.12
 */
AsContentRatingValue
as_content_rating_attribute_from_csm_age (const gchar *id, guint age)
{
	for (gsize i = 0; G_N_ELEMENTS (oars_to_csm_mappings); i++) {
		if (g_strcmp0 (id, oars_to_csm_mappings[i].id) == 0) {
			if (age >= oars_to_csm_mappings[i].csm_age_intense)
				return AS_CONTENT_RATING_VALUE_INTENSE;
			else if (age >= oars_to_csm_mappings[i].csm_age_moderate)
				return AS_CONTENT_RATING_VALUE_MODERATE;
			else if (age >= oars_to_csm_mappings[i].csm_age_mild)
				return AS_CONTENT_RATING_VALUE_MILD;
			else if (age >= oars_to_csm_mappings[i].csm_age_none)
				return AS_CONTENT_RATING_VALUE_NONE;
			else
				return AS_CONTENT_RATING_VALUE_UNKNOWN;
		}
	}

	return AS_CONTENT_RATING_VALUE_UNKNOWN;
}

/**
 * as_content_rating_get_all_rating_ids:
 *
 * Returns a list of all the valid OARS content rating attribute IDs as could
 * be passed to as_content_rating_add_attribute() or
 * as_content_rating_attribute_to_csm_age().
 *
 * Returns: (array zero-terminated=1) (transfer container): a %NULL-terminated
 *    array of IDs, to be freed with g_free() (the element values are owned by
 *    libappstream-glib and must not be freed)
 * Since: 0.12.10
 */
const gchar **
as_content_rating_get_all_rating_ids (void)
{
	g_autofree const gchar **ids = NULL;

	ids = g_new0 (const gchar *, G_N_ELEMENTS (oars_to_csm_mappings) + 1 /* NULL terminator */);
	for (gsize i = 0; i < G_N_ELEMENTS (oars_to_csm_mappings); i++)
		ids[i] = oars_to_csm_mappings[i].id;

	return g_steal_pointer (&ids);
}

/**
 * as_content_rating_get_minimum_age:
 * @content_rating: a #AsContentRating
 *
 * Gets the lowest Common Sense Media approved age for the content_rating block.
 * NOTE: these numbers are based on the data and descriptions available from
 * https://www.commonsensemedia.org/about-us/our-mission/about-our-ratings and
 * you may disagree with them.
 *
 * You're free to disagree with these, and of course you should use your own
 * brain to work our if your child is able to cope with the concepts enumerated
 * here. Some 13 year olds may be fine with the concept of mutilation of body
 * parts; others may get nightmares.
 *
 * Returns: The age in years, 0 for no rating, or G_MAXUINT for no details.
 *
 * Since: 0.11.0
 **/
guint
as_content_rating_get_minimum_age (AsContentRating *content_rating)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	guint csm_age = 0;

	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), 0);

	/* check kind */
	if (g_strcmp0 (priv->kind, "oars-1.0") != 0 &&
	    g_strcmp0 (priv->kind, "oars-1.1") != 0)
		return G_MAXUINT;

	for (guint i = 0; i < priv->keys->len; i++) {
		AsContentRatingKey *key;
		guint csm_tmp;
		key = g_ptr_array_index (priv->keys, i);
		csm_tmp = as_content_rating_attribute_to_csm_age (key->id, key->value);
		if (csm_tmp > 0 && csm_tmp > csm_age)
			csm_age = csm_tmp;
	}
	return csm_age;
}

/**
 * as_content_rating_get_kind:
 * @content_rating: a #AsContentRating instance.
 *
 * Gets the content_rating kind.
 *
 * Returns: a string, e.g. "oars-1.0", or NULL
 *
 * Since: 0.11.0
 **/
const gchar *
as_content_rating_get_kind (AsContentRating *content_rating)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), NULL);
	return priv->kind;
}

/**
 * as_content_rating_set_kind:
 * @content_rating: a #AsContentRating instance.
 * @kind: the rating kind, e.g. "oars-1.0"
 *
 * Sets the content rating kind.
 *
 * Since: 0.11.0
 **/
void
as_content_rating_set_kind (AsContentRating *content_rating, const gchar *kind)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	g_return_if_fail (AS_IS_CONTENT_RATING (content_rating));
	g_free (priv->kind);
	priv->kind = g_strdup (kind);
}

/**
 * as_content_rating_load_from_xml:
 * @content_rating: a #AsContentRating
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads data from an XML node.
 **/
gboolean
as_content_rating_load_from_xml (AsContentRating *content_rating, AsContext *ctx, xmlNode *node, GError **error)
{
	xmlNode *iter;
	g_autofree gchar *type_str = NULL;

	/* set selected content-rating type (usually oars-1.0) */
	type_str = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	as_content_rating_set_kind (content_rating, (gchar*) type_str);

	/* read attributes */
	for (iter = node->children; iter != NULL; iter = iter->next) {
		g_autofree gchar *attr_id = NULL;
		AsContentRatingValue attr_value;
		g_autofree gchar *str_value = NULL;

		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (g_strcmp0 ((gchar*) iter->name, "content_attribute") != 0)
			continue;

		attr_id = (gchar*) xmlGetProp (iter, (xmlChar*) "id");
		str_value = as_xml_get_node_value (iter);
		attr_value = as_content_rating_value_from_string (str_value);
		if ((attr_value == AS_CONTENT_RATING_VALUE_UNKNOWN) || (attr_id == NULL))
			continue; /* this rating attribute is invalid */

		as_content_rating_set_value (content_rating, attr_id, attr_value);
	}

	return TRUE;
}

/**
 * as_content_rating_to_xml_node:
 * @content_rating: a #AsContentRating
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_content_rating_to_xml_node (AsContentRating *content_rating, AsContext *ctx, xmlNode *root)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	guint i;
	xmlNode *rnode;

	rnode = xmlNewChild (root, NULL, (xmlChar*) "content_rating", NULL);
	xmlNewProp (rnode,
		    (xmlChar*) "type",
		    (xmlChar*) priv->kind);

	for (i = 0; i < priv->keys->len; i++) {
		xmlNode *anode;
		AsContentRatingKey *key = (AsContentRatingKey*) g_ptr_array_index (priv->keys, i);

		anode = xmlNewTextChild (rnode,
					 NULL,
					 (xmlChar*) "content_attribute",
					 (xmlChar*) as_content_rating_value_to_string (key->value));
		xmlNewProp (anode,
			    (xmlChar*) "id",
			    (xmlChar*) key->id);
	}
}

/**
 * as_content_rating_load_from_yaml:
 * @content_rating: a #AsContentRating
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_content_rating_load_from_yaml (AsContentRating *content_rating, AsContext *ctx, GNode *node, GError **error)
{
	GNode *n;

	as_content_rating_set_kind (content_rating,
				    as_yaml_node_get_key (node));
	for (n = node->children; n != NULL; n = n->next) {
		AsContentRatingValue attr_value;

		attr_value = as_content_rating_value_from_string (as_yaml_node_get_value (n));
		if (attr_value == AS_CONTENT_RATING_VALUE_UNKNOWN)
			continue;

		as_content_rating_set_value (content_rating,
					     as_yaml_node_get_key (n),
					     attr_value);
	}

	return TRUE;
}

/**
 * as_content_rating_emit_yaml:
 * @content_rating: a #AsContentRating
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_content_rating_emit_yaml (AsContentRating *content_rating, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	guint j;

	if (priv->kind == NULL)
		return; /* we need to check for null to not mess up the YAML sequence */
	as_yaml_emit_scalar (emitter, priv->kind);

	as_yaml_mapping_start (emitter);
	for (j = 0; j < priv->keys->len; j++) {
		AsContentRatingKey *key = (AsContentRatingKey*) g_ptr_array_index (priv->keys, j);

		as_yaml_emit_entry (emitter,
				    key->id,
				    as_content_rating_value_to_string (key->value));
	}
	as_yaml_mapping_end (emitter);
}

/**
 * as_content_rating_new:
 *
 * Creates a new #AsContentRating.
 *
 * Returns: (transfer full): a #AsContentRating
 *
 * Since: 0.11.0
 **/
AsContentRating*
as_content_rating_new (void)
{
	AsContentRating *content_rating;
	content_rating = g_object_new (AS_TYPE_CONTENT_RATING, NULL);
	return AS_CONTENT_RATING (content_rating);
}
