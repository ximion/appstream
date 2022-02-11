/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Canonical Ltd.
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:as-review
 * @title: AsReview
 * @include: appstream.h
 * @short_description: A user review
 *
 * This object represents a user-submitted software review.
 *
 * Since: 0.14.0
 **/

#include "config.h"

#include "as-review-private.h"
#include "as-utils-private.h"

typedef struct
{
	AsReviewFlags		 flags;
	GRefString		*id;
	GRefString		*summary;
	GRefString		*description;
	GRefString		*locale;
	gint			 priority;
	gint			 rating;
	GRefString		*version;
	GRefString		*reviewer_id;
	GRefString		*reviewer_name;
	GDateTime		*date;
	GHashTable		*metadata; /* GRefString : GRefString */
} AsReviewPrivate;

enum {
	PROP_0,
	PROP_ID,
	PROP_SUMMARY,
	PROP_DESCRIPTION,
	PROP_LOCALE,
	PROP_RATING,
	PROP_VERSION,
	PROP_REVIEWER_ID,
	PROP_REVIEWER_NAME,
	PROP_DATE,
	PROP_FLAGS,
	PROP_LAST
};

G_DEFINE_TYPE_WITH_PRIVATE (AsReview, as_review, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_review_get_instance_private (o))

static void
as_review_finalize (GObject *object)
{
	AsReview *review = AS_REVIEW (object);
	AsReviewPrivate *priv = GET_PRIVATE (review);

	as_ref_string_release (priv->id);
	as_ref_string_release (priv->summary);
	as_ref_string_release (priv->description);
	as_ref_string_release (priv->locale);
	as_ref_string_release (priv->version);
	as_ref_string_release (priv->reviewer_id);
	as_ref_string_release (priv->reviewer_name);

	g_hash_table_unref (priv->metadata);
	if (priv->date != NULL)
		g_date_time_unref (priv->date);

	G_OBJECT_CLASS (as_review_parent_class)->finalize (object);
}

static void
as_review_init (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	priv->metadata = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) g_ref_string_release,
						(GDestroyNotify) g_ref_string_release);
}

static void
as_review_get_property (GObject *object, guint prop_id,
			GValue *value, GParamSpec *pspec)
{
	AsReview *review = AS_REVIEW (object);
	AsReviewPrivate *priv = GET_PRIVATE (review);

	switch (prop_id) {
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_SUMMARY:
		g_value_set_string (value, priv->summary);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_LOCALE:
		g_value_set_string (value, priv->locale);
		break;
	case PROP_RATING:
		g_value_set_int (value, priv->rating);
		break;
	case PROP_FLAGS:
		g_value_set_uint64 (value, priv->flags);
		break;
	case PROP_VERSION:
		g_value_set_string (value, priv->version);
		break;
	case PROP_REVIEWER_ID:
		g_value_set_string (value, priv->reviewer_id);
		break;
	case PROP_REVIEWER_NAME:
		g_value_set_string (value, priv->reviewer_name);
		break;
	case PROP_DATE:
		g_value_set_object (value, priv->date);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
as_review_set_property (GObject *object, guint prop_id,
			const GValue *value, GParamSpec *pspec)
{
	AsReview *review = AS_REVIEW (object);

	switch (prop_id) {
	case PROP_ID:
		as_review_set_id (review, g_value_get_string (value));
		break;
	case PROP_SUMMARY:
		as_review_set_summary (review, g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		as_review_set_description (review, g_value_get_string (value));
		break;
	case PROP_LOCALE:
		as_review_set_locale (review, g_value_get_string (value));
		break;
	case PROP_RATING:
		as_review_set_rating (review, g_value_get_int (value));
		break;
	case PROP_FLAGS:
		as_review_set_flags (review, g_value_get_uint64 (value));
		break;
	case PROP_VERSION:
		as_review_set_version (review, g_value_get_string (value));
		break;
	case PROP_REVIEWER_ID:
		as_review_set_reviewer_id (review, g_value_get_string (value));
		break;
	case PROP_REVIEWER_NAME:
		as_review_set_reviewer_name (review, g_value_get_string (value));
		break;
	case PROP_DATE:
		as_review_set_date (review, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
as_review_class_init (AsReviewClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_review_finalize;
	object_class->get_property = as_review_get_property;
	object_class->set_property = as_review_set_property;

	/**
	 * AsReview:id:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	/**
	 * AsReview:summary:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_string ("summary", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_SUMMARY, pspec);

	/**
	 * AsReview:description:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * AsReview:locale:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_string ("locale", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_LOCALE, pspec);

	/**
	 * AsReview:rating:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_int ("rating", NULL, NULL,
				  -1, 100, 0,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_RATING, pspec);

	/**
	 * AsReview:flags:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_uint64 ("flags", NULL, NULL,
				     AS_REVIEW_FLAG_NONE,
				     AS_REVIEW_FLAG_LAST,
				     AS_REVIEW_FLAG_NONE,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_FLAGS, pspec);

	/**
	 * AsReview:version:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_string ("version", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_VERSION, pspec);

	/**
	 * AsReview:reviewer-id:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_string ("reviewer-id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_REVIEWER_ID, pspec);

	/**
	 * AsReview:reviewer-name:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_string ("reviewer-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_REVIEWER_NAME, pspec);

	/**
	 * AsReview:date:
	 *
	 * Since: 0.14.0
	 **/
	pspec = g_param_spec_object ("date", NULL, NULL,
				     AS_TYPE_REVIEW,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_DATE, pspec);
}

/**
 * as_review_get_priority:
 * @review: a #AsReview
 *
 * This allows the UI to sort reviews into the correct order.
 * Higher numbers indicate a more important or relevant review.
 *
 * Returns: the review priority, or 0 for unset.
 *
 * Since: 0.14.0
 **/
gint
as_review_get_priority (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), 0);
	return priv->priority;
}

/**
 * as_review_set_priority:
 * @review: a #AsReview
 * @priority: a priority value
 *
 * Sets the priority for the review, where positive numbers indicate
 * a better review for the specific user.
 *
 * Since: 0.14.0
 **/
void
as_review_set_priority (AsReview *review, gint priority)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	priv->priority = priority;
}

/**
 * as_review_get_id:
 * @review: a #AsReview
 *
 * Gets the review id.
 *
 * Returns: the review identifier, e.g. "deadbeef"
 *
 * Since: 0.14.0
 **/
const gchar *
as_review_get_id (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	return priv->id;
}

/**
 * as_review_get_summary:
 * @review: a #AsReview
 *
 * Gets the review summary.
 *
 * Returns: the one-line summary, e.g. "Awesome application"
 *
 * Since: 0.14.0
 **/
const gchar *
as_review_get_summary (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	return priv->summary;
}

/**
 * as_review_set_id:
 * @review: a #AsReview
 * @id: review identifier, e.g. "deadbeef"
 *
 * Sets the review identifier that is unique to each review.
 *
 * Since: 0.14.0
 **/
void
as_review_set_id (AsReview *review, const gchar *id)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	as_ref_string_assign_safe (&priv->id, id);
}

/**
 * as_review_set_summary:
 * @review: a #AsReview
 * @summary: a one-line summary, e.g. "Awesome application"
 *
 * Sets the one-line summary that may be displayed in bold.
 *
 * Since: 0.14.0
 **/
void
as_review_set_summary (AsReview *review, const gchar *summary)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	as_ref_string_assign_safe (&priv->summary, summary);
}

/**
 * as_review_get_description:
 * @review: a #AsReview
 *
 * Gets the multi-line review text that forms the body of the review.
 *
 * Returns: the string, or %NULL
 *
 * Since: 0.14.0
 **/
const gchar *
as_review_get_description (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	return priv->description;
}

/**
 * as_review_set_description:
 * @review: a #AsReview
 * @description: multi-line description
 *
 * Sets the multi-line review text that forms the body of the review.
 *
 * Since: 0.14.0
 **/
void
as_review_set_description (AsReview *review, const gchar *description)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	as_ref_string_assign_safe (&priv->description, description);
}

/**
 * as_review_get_locale:
 * @review: a #AsReview
 *
 * Gets the locale for the review.
 *
 * Returns: the string, or %NULL
 *
 * Since: 0.14.0
 **/
const gchar *
as_review_get_locale (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	return priv->locale;
}

/**
 * as_review_set_locale:
 * @review: a #AsReview
 * @locale: locale, e.g. "en_GB"
 *
 * Sets the locale for the review.
 *
 * Since: 0.14.0
 **/
void
as_review_set_locale (AsReview *review, const gchar *locale)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	as_ref_string_assign_safe (&priv->locale, locale);
}

/**
 * as_review_get_rating:
 * @review: a #AsReview
 *
 * Gets the star rating of the review, where 100 is 5 stars.
 *
 * Returns: integer as a percentage, or 0 for unset
 *
 * Since: 0.14.0
 **/
gint
as_review_get_rating (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), 0);
	return priv->rating;
}

/**
 * as_review_set_rating:
 * @review: a #AsReview
 * @rating: a integer as a percentage, or 0 for unset
 *
 * Sets the star rating of the review, where 100 is 5 stars..
 *
 * Since: 0.14.0
 **/
void
as_review_set_rating (AsReview *review, gint rating)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	priv->rating = rating;
}

/**
 * as_review_get_flags:
 * @review: a #AsReview
 *
 * Gets any flags set on the review, for example if the user has already
 * voted on the review or if the user wrote the review themselves.
 *
 * Returns: a #AsReviewFlags, e.g. %AS_REVIEW_FLAG_SELF
 *
 * Since: 0.14.0
 **/
AsReviewFlags
as_review_get_flags (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), AS_REVIEW_FLAG_NONE);
	return priv->flags;
}

/**
 * as_review_set_flags:
 * @review: a #AsReview
 * @flags: a #AsReviewFlags, e.g. %AS_REVIEW_FLAG_SELF
 *
 * Gets any flags set on the review, for example if the user has already
 * voted on the review or if the user wrote the review themselves.
 *
 * Since: 0.14.0
 **/
void
as_review_set_flags (AsReview *review, AsReviewFlags flags)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	priv->flags = flags;
}

/**
 * as_review_add_flags:
 * @review: a #AsReview
 * @flags: a #AsReviewFlags, e.g. %AS_REVIEW_FLAG_SELF
 *
 * Adds flags to an existing review without replacing the other flags.
 *
 * Since: 0.14.0
 **/
void
as_review_add_flags (AsReview *review, AsReviewFlags flags)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	priv->flags |= flags;
}

/**
 * as_review_get_reviewer_id:
 * @review: a #AsReview
 *
 * Gets the name of the reviewer.
 *
 * Returns: the reviewer ID, e.g. "deadbeef", or %NULL
 *
 * Since: 0.14.0
 **/
const gchar *
as_review_get_reviewer_id (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	return priv->reviewer_id;
}

/**
 * as_review_get_reviewer_name:
 * @review: a #AsReview
 *
 * Gets the name of the reviewer.
 *
 * Returns: the reviewer name, e.g. "David Smith", or %NULL
 *
 * Since: 0.14.0
 **/
const gchar *
as_review_get_reviewer_name (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	return priv->reviewer_name;
}

/**
 * as_review_set_reviewer_id:
 * @review: a #AsReview
 * @reviewer_id: the reviewer ID, e.g. "deadbeef"
 *
 * Sets the name of the reviewer, which can be left unset.
 *
 * Since: 0.14.0
 **/
void
as_review_set_reviewer_id (AsReview *review, const gchar *reviewer_id)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	as_ref_string_assign_safe (&priv->reviewer_id, reviewer_id);
}

/**
 * as_review_set_reviewer_name:
 * @review: a #AsReview
 * @reviewer_name: the reviewer name, e.g. "David Smith"
 *
 * Sets the name of the reviewer, which can be left unset.
 *
 * Since: 0.14.0
 **/
void
as_review_set_reviewer_name (AsReview *review, const gchar *reviewer_name)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	as_ref_string_assign_safe (&priv->reviewer_name, reviewer_name);
}

/**
 * as_review_set_version:
 * @review: a #AsReview
 * @version: a version string, e.g. "0.1.2"
 *
 * Sets the version string for the application being reviewed.
 *
 * Since: 0.14.0
 **/
void
as_review_set_version (AsReview *review, const gchar *version)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	as_ref_string_assign_safe (&priv->version, version);
}

/**
 * as_review_get_version:
 * @review: a #AsReview
 *
 * Gets the version string for the application being reviewed..
 *
 * Returns: the version string, e.g. "0.1.2", or %NULL for unset
 *
 * Since: 0.14.0
 **/
const gchar *
as_review_get_version (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	return priv->version;
}

/**
 * as_review_get_date:
 * @review: a #AsReview
 *
 * Gets the date the review was originally submitted.
 *
 * Returns: (transfer none): the #GDateTime, or %NULL for unset
 *
 * Since: 0.14.0
 **/
GDateTime *
as_review_get_date (AsReview *review)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	return priv->date;
}

/**
 * as_review_set_date:
 * @review: a #AsReview
 * @date: a #GDateTime, or %NULL
 *
 * Sets the date the review was originally submitted.
 *
 * Since: 0.14.0
 **/
void
as_review_set_date (AsReview *review, GDateTime *date)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	g_clear_pointer (&priv->date, g_date_time_unref);
	if (date != NULL)
		priv->date = g_date_time_ref (date);
}

/**
 * as_review_get_metadata_item:
 * @review: a #AsReview
 * @key: a string
 *
 * Gets some metadata from a review object.
 * It is left for the the plugin to use this method as required, but a
 * typical use would be to retrieve some secure authentication token.
 *
 * Returns: A string value, or %NULL for not found
 *
 * Since: 0.14.0
 **/
const gchar *
as_review_get_metadata_item (AsReview *review, const gchar *key)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_val_if_fail (AS_IS_REVIEW (review), NULL);
	g_return_val_if_fail (key != NULL, NULL);
	return g_hash_table_lookup (priv->metadata, key);
}

/**
 * as_review_add_metadata:
 * @review: a #AsReview
 * @key: a string
 * @value: a string
 *
 * Adds metadata to the review object.
 * It is left for the the plugin to use this method as required, but a
 * typical use would be to store some secure authentication token.
 *
 * Since: 0.14.0
 **/
void
as_review_add_metadata (AsReview *review, const gchar *key, const gchar *value)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	g_return_if_fail (AS_IS_REVIEW (review));
	g_hash_table_insert (priv->metadata,
			     g_ref_string_new_intern (key),
			     g_ref_string_new_intern (value));
}

/**
 * as_review_equal:
 * @review1: a #AsReview instance.
 * @review2: a #AsReview instance.
 *
 * Checks if two reviews are the same.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.14.0
 **/
gboolean
as_review_equal (AsReview *review1, AsReview *review2)
{
	AsReviewPrivate *priv1 = GET_PRIVATE (review1);
	AsReviewPrivate *priv2 = GET_PRIVATE (review2);

	/* trivial */
	if (review1 == review2)
		return TRUE;

	/* check for equality */
	if (!g_date_time_equal (priv1->date, priv2->date))
		return FALSE;
	if (priv1->priority != priv2->priority)
		return FALSE;
	if (priv1->rating != priv2->rating)
		return FALSE;
	if (g_strcmp0 (priv1->id, priv2->id) != 0)
		return FALSE;
	if (g_strcmp0 (priv1->summary, priv2->summary) != 0)
		return FALSE;
	if (g_strcmp0 (priv1->description, priv2->description) != 0)
		return FALSE;
	if (g_strcmp0 (priv1->locale, priv2->locale) != 0)
		return FALSE;
	if (g_strcmp0 (priv1->version, priv2->version) != 0)
		return FALSE;

	/* success */
	return TRUE;
}

/**
 * as_review_load_from_xml:
 * @review: a #AsReview instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads review data from an XML node.
 **/
gboolean
as_review_load_from_xml (AsReview *review, AsContext *ctx, xmlNode *node, GError **error)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	gchar *str;
	gint itmp;

	as_ref_string_assign_transfer (&priv->id, as_xml_get_prop_value_refstr (node, "id"));

	str = as_xml_get_prop_value (node, "date");
	if (str != NULL) {
		g_autoptr(GDateTime) dt = as_iso8601_to_datetime (str);
		if (dt != NULL)
			as_review_set_date (review, dt);
		g_free (str);
	}

	itmp = as_xml_get_prop_value_as_int (node, "rating");
	if (itmp != G_MAXINT)
		as_review_set_rating (review, itmp);

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "priority") == 0) {
			gint64 prio;
			str = as_xml_get_node_value (iter);
			prio = g_ascii_strtoll (str, NULL, 10);
			as_review_set_priority (review, (gint) prio);
			g_free (str);

		} else if (g_strcmp0 ((gchar*) iter->name, "summary") == 0) {
			as_ref_string_assign_transfer (&priv->summary, as_xml_get_node_value_refstr (iter));

		} else if (g_strcmp0 ((gchar*) iter->name, "description") == 0) {
			g_autofree gchar *desc = as_xml_dump_node_children (iter);
			as_ref_string_assign_transfer (&priv->description, g_ref_string_new_intern (desc));

		} else if (g_strcmp0 ((gchar*) iter->name, "version") == 0) {
			as_ref_string_assign_transfer (&priv->version, as_xml_get_node_value_refstr (iter));

		} else if (g_strcmp0 ((gchar*) iter->name, "reviewer_id") == 0) {
			as_ref_string_assign_transfer (&priv->reviewer_id, as_xml_get_node_value_refstr (iter));

		} else if (g_strcmp0 ((gchar*) iter->name, "reviewer_name") == 0) {
			as_ref_string_assign_transfer (&priv->reviewer_name, as_xml_get_node_value_refstr (iter));

		} else if (g_strcmp0 ((gchar*) iter->name, "lang") == 0) {
			as_ref_string_assign_transfer (&priv->locale, as_xml_get_node_value_refstr (iter));

		} else if (g_strcmp0 ((gchar*) iter->name, "metadata") == 0) {
			as_xml_parse_custom_node (iter, priv->metadata);
		}
	}

	return TRUE;
}

/**
 * as_review_to_xml_node:
 * @review: a #AsReview instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_review_to_xml_node (AsReview *review, AsContext *ctx, xmlNode *root)
{
	AsReviewPrivate *priv = GET_PRIVATE (review);
	xmlNode *n_review;

	n_review = xmlNewChild (root, NULL, (xmlChar*) "review", (xmlChar*) "");
	if (priv->id != NULL) {
		as_xml_add_text_prop (n_review, "id", priv->id);
	}

	if (priv->date != NULL) {
		g_autofree gchar *str = g_date_time_format (priv->date, "%F");
		as_xml_add_text_prop (n_review, "date", str);
	}

	if (priv->rating != 0) {
		g_autofree gchar *str = g_strdup_printf ("%i", priv->rating);
		as_xml_add_text_prop (n_review, "rating", str);
	}

	if (priv->priority != 0) {
		g_autofree gchar *str = g_strdup_printf ("%i", priv->priority);
		as_xml_add_text_node (n_review, "priority", str);
	}

	as_xml_add_text_node (n_review, "summary", priv->summary);
	as_xml_add_description_node_raw (n_review, priv->description);

	as_xml_add_text_node (n_review, "version", priv->version);

	as_xml_add_text_node (n_review, "reviewer_id", priv->reviewer_id);
	as_xml_add_text_node (n_review, "reviewer_name", priv->reviewer_name);

	as_xml_add_text_node (n_review, "lang", priv->locale);

	/* <metadata> */
	as_xml_add_custom_node (n_review, "metadata", priv->metadata);

	xmlAddChild (root, n_review);
}

/**
 * as_review_load_from_yaml:
 * @review: a #AsReview instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_review_load_from_yaml (AsReview *review, AsContext *ctx, GNode *node, GError **error)
{
	/* TODO */
	return TRUE;
}

/**
 * as_review_emit_yaml:
 * @review: a #AsReview instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_review_emit_yaml (AsReview *review, AsContext *ctx, yaml_emitter_t *emitter)
{
	/* TODO */
	return;
}

/**
 * as_review_new:
 *
 * Creates a new #AsReview.
 *
 * Returns: (transfer full): a #AsReview
 *
 * Since: 0.14.0
 **/
AsReview *
as_review_new (void)
{
	AsReview *review;
	review = g_object_new (AS_TYPE_REVIEW, NULL);
	return AS_REVIEW (review);
}
