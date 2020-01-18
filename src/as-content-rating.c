/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-content-rating.h"
#include "as-content-rating-private.h"

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
	if (key->id != NULL)
		g_free (key->id);
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
	key->id = g_strdup (id);
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
	{ "sex-homosexuality",	OARS_1_1, 0, 10, 13, 18 },
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
	content_rating = g_object_new (AS_TYPE_CONTENT, NULL);
	return AS_CONTENT_RATING (content_rating);
}
