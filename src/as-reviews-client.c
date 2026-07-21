/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2026 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2016-2018 Kalev Lember <klember@redhat.com>
 * Copyright (C) 2021 Endless OS Foundation LLC
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
 * AsReviewsClient:
 *
 * Fetch user reviews for software components.
 *
 * This class is a client for the Open Desktop Ratings Service (ODRS) or
 * a compatible service, and can retrieve user reviews and ratings for
 * software components.
 */

#include "config.h"
#include "as-reviews-client.h"
#include "as-reviews-client-private.h"

#include <glib/gi18n-lib.h>
#include <libfyaml.h>
#include <math.h>
#include <float.h>

#include "as-curl.h"
#include "as-macros-private.h"
#include "as-review.h"
#include "as-release-list.h"
#include "as-system-info.h"
#include "as-utils-private.h"
#include "as-yaml.h"

#define AS_REVIEWS_SERVER_URL_DEFAULT "https://odrs.gnome.org/1.0/reviews/api"

/* maximum amount of reviews to fetch by default */
#define AS_REVIEWS_FETCH_LIMIT_DEFAULT 50

typedef struct {
	gchar *server_url;
	gchar *locale;

	AsCurl *acurl;

	gchar *distro_name;
	gchar *client_id;
	gchar *user_agent;
	gchar *user_hash;
} AsReviewsClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsReviewsClient, as_reviews_client, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_reviews_client_get_instance_private (o))

G_DEFINE_QUARK (AsReviewsClientError, as_reviews_client_error)

static void
as_reviews_client_init (AsReviewsClient *rrc)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);

	priv->server_url = g_strdup (AS_REVIEWS_SERVER_URL_DEFAULT);
	priv->locale = as_get_current_locale_posix ();

	priv->client_id = g_strdup ("appstream");
	priv->user_agent = g_strdup ("appstream/" PACKAGE_VERSION);
}

static void
as_reviews_client_finalize (GObject *object)
{
	AsReviewsClient *rrc = AS_REVIEWS_CLIENT (object);
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);

	g_free (priv->server_url);
	g_free (priv->locale);

	g_free (priv->client_id);
	g_free (priv->user_agent);
	g_free (priv->user_hash);

	g_free (priv->distro_name);
	g_clear_object (&priv->acurl);

	G_OBJECT_CLASS (as_reviews_client_parent_class)->finalize (object);
}

static void
as_reviews_client_class_init (AsReviewsClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_reviews_client_finalize;
}

/**
 * as_reviews_client_get_curl:
 *
 * Get the #AsCurl instance owned by this reviews client,
 * creating it if necessary.
 */
static AsCurl *
as_reviews_client_get_curl (AsReviewsClient *rrc, GError **error)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);

	if (priv->acurl == NULL) {
		priv->acurl = as_curl_new (error);
		if (priv->acurl != NULL)
			as_curl_set_user_agent (priv->acurl, priv->user_agent);
	}
	return priv->acurl;
}

/**
 * as_reviews_client_get_server_url:
 * @rrc: an #AsReviewsClient instance.
 *
 * Get the URL of the reviews server that we are communicating with.
 *
 * Returns: the reviews server URL.
 **/
const gchar *
as_reviews_client_get_server_url (AsReviewsClient *rrc)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	return priv->server_url;
}

/**
 * as_reviews_client_set_server_url:
 * @rrc: an #AsReviewsClient instance.
 * @url: (nullable): the new reviews server URL.
 *
 * Set the URL of the ODRS-compatible reviews server to communicate with.
 * Set it to %NULL to restore the default server.
 **/
void
as_reviews_client_set_server_url (AsReviewsClient *rrc, const gchar *url)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);

	g_free (priv->server_url);
	if (url == NULL) {
		priv->server_url = g_strdup (AS_REVIEWS_SERVER_URL_DEFAULT);
		return;
	}

	priv->server_url = g_strdup (url);
	/* drop any trailing slashes, we add them when building request URLs */
	while (g_str_has_suffix (priv->server_url, "/"))
		priv->server_url[strlen (priv->server_url) - 1] = '\0';
}

/**
 * as_reviews_client_get_client_id:
 * @rrc: an #AsReviewsClient instance.
 *
 * Get the ID of the client application which is fetching reviews.
 *
 * Returns: the client-ID.
 **/
const gchar *
as_reviews_client_get_client_id (AsReviewsClient *rrc)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	return priv->client_id;
}

/**
 * as_reviews_client_set_client_id:
 * @rrc: an #AsReviewsClient instance.
 * @client_id: (nullable): the new client-ID.
 *
 * Set an ID for the client application which is fetching reviews.
 * The ID is used when generating the user hash, so the same user
 * gets a different identity for each client application they are
 * submitting reviews from.
 *
 * You should set this value early, before any user hash was generated.
 * Set it to %NULL to restore the default client-ID.
 **/
void
as_reviews_client_set_client_id (AsReviewsClient *rrc, const gchar *client_id)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);

	g_free (priv->client_id);
	priv->client_id = g_strdup (client_id == NULL ? "appstream" : client_id);
}

/**
 * as_reviews_client_get_user_agent:
 * @rrc: an #AsReviewsClient instance.
 *
 * Get the user agent used for communication with the reviews server.
 *
 * Returns: (nullable): the user agent string.
 **/
const gchar *
as_reviews_client_get_user_agent (AsReviewsClient *rrc)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	return priv->user_agent;
}

/**
 * as_reviews_client_set_user_agent:
 * @rrc: an #AsReviewsClient instance.
 * @user_agent: (nullable): the new user agent string.
 *
 * Set the user agent to use when communicating with the reviews server.
 * Set it to %NULL to restore the default user agent.
 **/
void
as_reviews_client_set_user_agent (AsReviewsClient *rrc, const gchar *user_agent)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);

	g_free (priv->user_agent);
	priv->user_agent = g_strdup (user_agent == NULL ? "appstream/" PACKAGE_VERSION
							: user_agent);
	if (priv->acurl != NULL)
		as_curl_set_user_agent (priv->acurl, priv->user_agent);
}

/**
 * as_reviews_client_get_user_hash:
 * @rrc: an #AsReviewsClient instance.
 *
 * Get the (salted) hash value used to identify the current user
 * to the reviews service, generating it if necessary.
 * The hash is used so the user can only vote once on each application,
 * and so their own reviews can be identified. It can not easily be traced
 * back to an individual user.
 *
 * Returns: the user hash, or %NULL if none could be generated.
 **/
const gchar *
as_reviews_client_get_user_hash (AsReviewsClient *rrc)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	g_autofree gchar *machine_id = NULL;
	g_autofree gchar *salted = NULL;

	if (priv->user_hash != NULL)
		return priv->user_hash;

	if (!g_file_get_contents ("/etc/machine-id", &machine_id, NULL, NULL))
		g_file_get_contents ("/var/lib/dbus/machine-id", &machine_id, NULL, NULL);
	if (machine_id == NULL)
		machine_id = g_strdup (g_get_host_name ());
	g_strstrip (machine_id);
	if (as_is_empty (machine_id))
		return NULL;

	salted = g_strdup_printf ("%s[%s:%s]", priv->client_id, g_get_user_name (), machine_id);
	priv->user_hash = g_compute_checksum_for_string (G_CHECKSUM_SHA1, salted, -1);

	return priv->user_hash;
}

/**
 * as_reviews_client_set_user_hash:
 * @rrc: an #AsReviewsClient instance.
 * @user_hash: (nullable): the new user hash string.
 *
 * Explicitly set the opaque hash value used to identify the current
 * user to the reviews service.
 * Set it to %NULL to have a suitable hash generated automatically,
 * which is the default behavior.
 **/
void
as_reviews_client_set_user_hash (AsReviewsClient *rrc, const gchar *user_hash)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	as_assign_string_safe (priv->user_hash, user_hash);
}

/**
 * as_reviews_client_get_locale:
 * @rrc: an #AsReviewsClient instance.
 *
 * Get the locale used for filtering reviews.
 *
 * Returns: the current locale, in POSIX format.
 **/
const gchar *
as_reviews_client_get_locale (AsReviewsClient *rrc)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	return priv->locale;
}

/**
 * as_reviews_client_set_locale:
 * @rrc: an #AsReviewsClient instance.
 * @locale: (nullable): the new locale, in POSIX format.
 *
 * Set the locale used for filtering reviews, so reviews in the
 * given (or a compatible) language are preferred.
 * Set it to %NULL to restore the default of using the current
 * system locale.
 **/
void
as_reviews_client_set_locale (AsReviewsClient *rrc, const gchar *locale)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);

	g_free (priv->locale);
	if (locale == NULL)
		priv->locale = as_get_current_locale_posix ();
	else
		priv->locale = as_locale_strip_encoding (locale);
}

/**
 * as_reviews_client_get_distro_name:
 *
 * Get the name of the current operating system, used to tell the
 * reviews server about the platform the reviews are fetched for.
 */
static const gchar *
as_reviews_client_get_distro_name (AsReviewsClient *rrc)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);

	if (priv->distro_name == NULL) {
		g_autoptr(AsSystemInfo) sysinfo = as_system_info_new ();
		priv->distro_name = g_strdup (as_system_info_get_os_name (sysinfo));
		if (priv->distro_name == NULL)
			priv->distro_name = g_strdup ("unknown");
	}

	return priv->distro_name;
}

/**
 * as_reviews_client_compute_wilson_score:
 *
 * Compute a review priority using the lower bound of the Wilson score
 * confidence interval for the given up/down vote counts.
 * See https://www.evanmiller.org/how-not-to-sort-by-average-rating.html
 */
static gint
as_reviews_client_compute_wilson_score (gdouble karma_up, gdouble karma_down)
{
	gdouble wilson = 0.0;

	if (karma_up <= 0 && karma_down <= 0)
		return 0;

	wilson = ((karma_up + 1.9208) / (karma_up + karma_down) -
		  1.96 * sqrt ((karma_up * karma_down) / (karma_up + karma_down) + 0.9604) /
		      (karma_up + karma_down)) /
		 (1 + 3.8416 / (karma_up + karma_down));
	wilson *= 100.0;

	return (gint) wilson;
}

/**
 * as_reviews_client_parse_review_node:
 *
 * Parse a single review JSON object into a new #AsReview,
 * or return %NULL if the node contained no review data
 * (e.g. it was an ODRS rating-summary object).
 */
static AsReview *
as_reviews_client_parse_review_node (AsReviewsClient *rrc, struct fy_node *rnode)
{
	g_autoptr(AsReview) review = NULL;
	const gchar *review_id = NULL;
	const gchar *reviewer_hash = NULL;
	gint64 karma_up = -1;
	gint64 karma_down = -1;
	gboolean have_score = FALSE;

	if (!fy_node_is_mapping (rnode))
		return NULL;

	review = as_review_new ();
	AS_YAML_MAPPING_FOREACH (pair, rnode) {
		const gchar *key = as_yaml_node_get_key0 (pair);
		const gchar *value = as_yaml_node_get_value0 (pair);

		if (key == NULL || value == NULL)
			continue;

		if (g_str_equal (key, "review_id")) {
			review_id = value;
			as_review_set_id (review, value);

		} else if (g_str_equal (key, "rating")) {
			as_review_set_rating (review, g_ascii_strtoll (value, NULL, 10));

		} else if (g_str_equal (key, "date_created")) {
			g_autoptr(GDateTime) dt = NULL;
			/* the ODRS sends timestamps as floating-point numbers for some reason */
			dt = g_date_time_new_from_unix_utc ((gint64) g_ascii_strtod (value, NULL));
			if (dt != NULL)
				as_review_set_date (review, dt);

		} else if (g_str_equal (key, "score")) {
			as_review_set_priority (review, g_ascii_strtoll (value, NULL, 10));
			have_score = TRUE;

		} else if (g_str_equal (key, "karma_up")) {
			karma_up = g_ascii_strtoll (value, NULL, 10);

		} else if (g_str_equal (key, "karma_down")) {
			karma_down = g_ascii_strtoll (value, NULL, 10);

		} else if (g_str_equal (key, "user_hash")) {
			reviewer_hash = value;
			as_review_set_reviewer_id (review, value);

		} else if (g_str_equal (key, "user_display")) {
			g_autofree gchar *name = g_strdup (value);
			as_review_set_reviewer_name (review, g_strstrip (name));

		} else if (g_str_equal (key, "summary")) {
			g_autofree gchar *summary = g_strdup (value);
			as_review_set_summary (review, g_strstrip (summary));

		} else if (g_str_equal (key, "description")) {
			g_autofree gchar *description = g_strdup (value);
			as_review_set_description (review, g_strstrip (description));

		} else if (g_str_equal (key, "version")) {
			as_review_set_version (review, value);

		} else if (g_str_equal (key, "locale")) {
			g_autofree gchar *locale = as_locale_strip_encoding (value);
			as_review_set_locale (review, locale);

		} else if (g_str_equal (key, "user_skey")) {
			/* extra metadata needed to submit votes for this review later */
			as_review_add_metadata (review, "ODRS::user_skey", value);

		} else if (g_str_equal (key, "app_id")) {
			as_review_add_metadata (review, "ODRS::app_id", value);

		} else if (g_str_equal (key, "vote_id")) {
			/* don't allow multiple votes */
			as_review_add_flags (review, AS_REVIEW_FLAG_VOTED);
		}
	}

	/* Ignore any non-review objects: if the ODRS has no reviews for an application,
	 * it sends a fake item without a review-ID, so the client can still obtain a
	 * user_skey. Reviews from an unknown (deleted) user have no user hash. */
	if (review_id == NULL || reviewer_hash == NULL)
		return NULL;

	/* if we have no explicit score, compute a priority from the karma votes */
	if (!have_score && (karma_up >= 0 || karma_down >= 0))
		as_review_set_priority (
		    review,
		    as_reviews_client_compute_wilson_score (MAX (karma_up, 0),
							    MAX (karma_down, 0)));

	/* the user hash matches ours, so this is our own review */
	if (g_strcmp0 (reviewer_hash, as_reviews_client_get_user_hash (rrc)) == 0)
		as_review_add_flags (review, AS_REVIEW_FLAG_SELF);

	return g_steal_pointer (&review);
}

/**
 * as_reviews_client_parse_reviews:
 * @rrc: an #AsReviewsClient instance.
 * @json_data: JSON data received from an ODRS-compatible server.
 * @data_len: length of @json_data, or -1 if NULL-terminated.
 * @error: a #GError.
 *
 * Parse a reviews JSON document into a list of #AsReview.
 *
 * Returns: (transfer container) (element-type AsReview): the parsed reviews, or %NULL on error.
 */
GPtrArray *
as_reviews_client_parse_reviews (AsReviewsClient *rrc,
				 const gchar *json_data,
				 gssize data_len,
				 GError **error)
{
	struct fy_document *ydoc = NULL;
	struct fy_node *root = NULL;
	g_autoptr(GPtrArray) reviews = NULL;
	g_autoptr(GHashTable) reviewer_ids = NULL;
	struct fy_parse_cfg ycfg = { .search_path = "", .flags = FYPCF_JSON_FORCE };

	if (json_data == NULL) {
		g_set_error_literal (error,
				     AS_REVIEWS_CLIENT_ERROR,
				     AS_REVIEWS_CLIENT_ERROR_PARSE,
				     "Can not parse reviews: No data was received.");
		return NULL;
	}
	if (data_len < 0)
		data_len = strlen (json_data);

	ycfg.diag = as_yaml_error_diag_create ();
	ydoc = fy_document_build_from_string (&ycfg, json_data, (size_t) data_len);
	if (ydoc == NULL) {
		g_autofree gchar *issue_msg = as_yaml_make_error_message (ycfg.diag);
		g_set_error (error,
			     AS_REVIEWS_CLIENT_ERROR,
			     AS_REVIEWS_CLIENT_ERROR_PARSE,
			     "Failed to parse reviews data: %s",
			     issue_msg);
		fy_diag_destroy (ycfg.diag);
		return NULL;
	}

	root = fy_document_root (ydoc);
	if (root == NULL || !fy_node_is_sequence (root)) {
		g_set_error_literal (error,
				     AS_REVIEWS_CLIENT_ERROR,
				     AS_REVIEWS_CLIENT_ERROR_PARSE,
				     "Failed to parse reviews data: No sequence of reviews found.");
		fy_document_destroy (ydoc);
		fy_diag_destroy (ycfg.diag);
		return NULL;
	}

	reviews = g_ptr_array_new_with_free_func (g_object_unref);
	reviewer_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	AS_YAML_SEQUENCE_FOREACH (rnode, root) {
		g_autoptr(AsReview) review = NULL;
		const gchar *reviewer_id;

		review = as_reviews_client_parse_review_node (rrc, rnode);
		if (review == NULL)
			continue;

		/* dedupe each review on its user hash */
		reviewer_id = as_review_get_reviewer_id (review);
		if (g_hash_table_contains (reviewer_ids, reviewer_id)) {
			g_debug ("Duplicate review from user %s, skipping it.", reviewer_id);
			continue;
		}
		g_hash_table_add (reviewer_ids, g_strdup (reviewer_id));

		g_ptr_array_add (reviews, g_steal_pointer (&review));
	}

	fy_document_destroy (ydoc);
	fy_diag_destroy (ycfg.diag);

	return g_steal_pointer (&reviews);
}

/**
 * as_pnormaldist:
 *
 * Inverse of the standard normal cumulative distribution function.
 */
static gdouble
as_pnormaldist (gdouble qn)
{
	static gdouble b[11] = { 1.570796288,	   0.03706987906,   -0.8364353589e-3,
				 -0.2250947176e-3, 0.6841218299e-5, 0.5824238515e-5,
				 -0.104527497e-5,  0.8360937017e-7, -0.3231081277e-8,
				 0.3657763036e-10, 0.6936233982e-12 };
	gdouble w1, w3;
	guint i;

	if (qn < 0 || qn > 1)
		return 0; // This is an error case
	if (G_APPROX_VALUE (qn, 0.5, DBL_EPSILON))
		return 0;

	w1 = qn;
	if (qn > 0.5)
		w1 = 1.0 - w1;
	w3 = -log (4.0 * w1 * (1.0 - w1));
	w1 = b[0];
	for (i = 1; i < 11; i++)
		w1 = w1 + (b[i] * pow (w3, i));

	if (qn > 0.5)
		return sqrt (w1 * w3);
	else
		return -sqrt (w1 * w3);
}

/**
 * as_wilson_score:
 *
 * Lower bound of the Wilson score confidence interval for a Bernoulli parameter.
 */
static gdouble
as_wilson_score (gdouble value, gdouble n, gdouble power)
{
	gdouble z, phat;
	if (G_APPROX_VALUE (value, 0.0, DBL_EPSILON))
		return 0;

	z = as_pnormaldist (1 - power / 2);
	phat = value / n;
	return (phat + z * z / (2 * n) - z * sqrt ((phat * (1 - phat) + z * z / (4 * n)) / n)) /
	       (1 + z * z / n);
}

/**
 * as_reviews_client_parse_rating:
 * @rrc: an #AsReviewsClient instance.
 * @json_data: rating summary JSON data received from an ODRS-compatible server.
 * @data_len: length of @json_data, or -1 if NULL-terminated.
 * @error: a #GError.
 *
 * Parse a star-rating summary JSON document and compute an overall
 * rating percentage from it, using the lower bound of the Wilson score
 * confidence interval so a small number of ratings does not produce an
 * overly high score.
 *
 * Returns: the overall rating percentage, or -1 if no rating was found.
 */
gint
as_reviews_client_parse_rating (AsReviewsClient *rrc,
				const gchar *json_data,
				gssize data_len,
				GError **error)
{
	struct fy_document *ydoc = NULL;
	struct fy_node *root = NULL;
	guint64 star_counts[6] = { 0, 0, 0, 0, 0, 0 };
	guint64 star_sum = 0;
	gdouble val;
	struct fy_parse_cfg ycfg = { .search_path = "", .flags = FYPCF_JSON_FORCE };

	if (json_data == NULL) {
		g_set_error_literal (error,
				     AS_REVIEWS_CLIENT_ERROR,
				     AS_REVIEWS_CLIENT_ERROR_PARSE,
				     "Can not parse rating: No data was received.");
		return -1;
	}
	if (data_len < 0)
		data_len = (gssize) strlen (json_data);

	ycfg.diag = as_yaml_error_diag_create ();
	ydoc = fy_document_build_from_string (&ycfg, json_data, (size_t) data_len);
	if (ydoc == NULL) {
		g_autofree gchar *issue_msg = as_yaml_make_error_message (ycfg.diag);
		g_set_error (error,
			     AS_REVIEWS_CLIENT_ERROR,
			     AS_REVIEWS_CLIENT_ERROR_PARSE,
			     "Failed to parse rating data: %s",
			     issue_msg);
		fy_diag_destroy (ycfg.diag);
		return -1;
	}

	root = fy_document_root (ydoc);

	/* the ODRS sends an empty array if it knows nothing about a component */
	if (root != NULL && fy_node_is_sequence (root)) {
		fy_document_destroy (ydoc);
		fy_diag_destroy (ycfg.diag);
		return -1;
	}

	if (root == NULL || !fy_node_is_mapping (root)) {
		g_set_error_literal (error,
				     AS_REVIEWS_CLIENT_ERROR,
				     AS_REVIEWS_CLIENT_ERROR_PARSE,
				     "Failed to parse rating data: No rating summary found.");
		fy_document_destroy (ydoc);
		fy_diag_destroy (ycfg.diag);
		return -1;
	}

	AS_YAML_MAPPING_FOREACH (pair, root) {
		const gchar *key = as_yaml_node_get_key0 (pair);
		const gchar *value = as_yaml_node_get_value0 (pair);

		if (key == NULL || value == NULL)
			continue;
		if (g_str_has_prefix (key, "star") && strlen (key) == 5) {
			gint star_idx = key[4] - '0';
			if (star_idx >= 0 && star_idx <= 5)
				star_counts[star_idx] = (guint64) g_ascii_strtoull (value,
										    NULL,
										    10);
		}
	}

	fy_document_destroy (ydoc);
	fy_diag_destroy (ycfg.diag);

	for (guint i = 1; i <= 5; i++)
		star_sum += star_counts[i];
	if (star_sum == 0)
		return -1;

	/* compute the score, then normalize it from -2..+2 to 0..5 and convert to a percentage */
	val = (as_wilson_score ((gdouble) star_counts[1], (gdouble) star_sum, 0.2) * -2);
	val += (as_wilson_score ((gdouble) star_counts[2], (gdouble) star_sum, 0.2) * -1);
	val += (as_wilson_score ((gdouble) star_counts[4], (gdouble) star_sum, 0.2) * 1);
	val += (as_wilson_score ((gdouble) star_counts[5], (gdouble) star_sum, 0.2) * 2);
	val += 3;
	val *= 20;

	return (gint) ceil (val);
}

/**
 * as_reviews_client_build_fetch_request:
 *
 * Create the JSON payload for a reviews fetch request.
 */
static GBytes *
as_reviews_client_build_fetch_request (AsReviewsClient *rrc,
				       const gchar *cpt_id,
				       GPtrArray *compat_ids,
				       const gchar *version,
				       guint start,
				       guint limit)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	struct fy_document *fyd = NULL;
	struct fy_node *root = NULL;
	g_autofree gchar *limit_str = NULL;
	gchar *json_data;

	struct {
		const gchar *key;
		const gchar *value;
	} str_entries[] = {
		{ "user_hash", as_reviews_client_get_user_hash (rrc)   },
		{ "app_id",    cpt_id				   },
		{ "locale",    priv->locale				 },
		{ "distro",    as_reviews_client_get_distro_name (rrc) },
		{ "version",   version == NULL ? "unknown" : version   },
		{ NULL,	NULL				    },
	};

	fyd = fy_document_create (NULL);
	root = fy_node_create_mapping (fyd);
	fy_document_set_root (fyd, root);

	for (guint i = 0; str_entries[i].key != NULL; i++) {
		struct fy_node *value_node;
		if (str_entries[i].value == NULL)
			continue;
		value_node = fy_node_create_scalar_copy (fyd, str_entries[i].value, FY_NT);
		fy_node_set_style (value_node, FYNS_DOUBLE_QUOTED);
		fy_node_mapping_append (root,
					fy_node_create_scalar_copy (fyd, str_entries[i].key, FY_NT),
					value_node);
	}

	/* limit of reviews to fetch, this one is a number and must not be quoted */
	limit_str = g_strdup_printf ("%u", limit);
	fy_node_mapping_append (root,
				fy_node_create_scalar_copy (fyd, "limit", FY_NT),
				fy_node_create_scalar_copy (fyd, limit_str, FY_NT));

	/* index of the first result to return, for pagination (also a number) */
	if (start > 0) {
		g_autofree gchar *start_str = g_strdup_printf ("%u", start);
		fy_node_mapping_append (root,
					fy_node_create_scalar_copy (fyd, "start", FY_NT),
					fy_node_create_scalar_copy (fyd, start_str, FY_NT));
	}

	if (compat_ids != NULL && compat_ids->len > 0) {
		struct fy_node *seq = fy_node_create_sequence (fyd);
		for (guint i = 0; i < compat_ids->len; i++) {
			const gchar *compat_id = g_ptr_array_index (compat_ids, i);
			struct fy_node *id_node;

			if (g_strcmp0 (compat_id, cpt_id) == 0)
				continue;
			id_node = fy_node_create_scalar_copy (fyd, compat_id, FY_NT);
			fy_node_set_style (id_node, FYNS_DOUBLE_QUOTED);
			fy_node_sequence_append (seq, id_node);
		}
		fy_node_mapping_append (root,
					fy_node_create_scalar_copy (fyd, "compat_ids", FY_NT),
					seq);
	}

	json_data = fy_emit_document_to_string (fyd,
						FYECF_MODE_JSON_ONELINE | FYECF_WIDTH_INF |
						    FYECF_NO_ENDING_NEWLINE);
	fy_document_destroy (fyd);

	return g_bytes_new_take (json_data, strlen (json_data));
}

/**
 * as_reviews_client_get_server_error_msg:
 *
 * Try to extract a human-readable error message from a JSON error
 * reply that an ODRS-compatible server sent with an HTTP error status,
 * e.g. `{"msg": "invalid data, expected user_hash", "success": false}`.
 *
 * Returns: (nullable): the server-provided message, or %NULL if none was found.
 */
static gchar *
as_reviews_client_get_server_error_msg (GBytes *error_reply)
{
	struct fy_document *ydoc = NULL;
	struct fy_node *root = NULL;
	gchar *msg = NULL;
	gconstpointer data;
	gsize data_len;
	struct fy_parse_cfg ycfg = { .search_path = "", .flags = FYPCF_JSON_FORCE };

	data = g_bytes_get_data (error_reply, &data_len);
	if (data == NULL || data_len == 0)
		return NULL;

	/* the reply may be anything, e.g. an HTML page from a proxy,
	 * so we parse it very defensively and quietly */
	ycfg.diag = as_yaml_error_diag_create ();
	ydoc = fy_document_build_from_string (&ycfg, data, data_len);
	if (ydoc == NULL) {
		fy_diag_destroy (ycfg.diag);
		return NULL;
	}

	root = fy_document_root (ydoc);
	if (root != NULL && fy_node_is_mapping (root)) {
		AS_YAML_MAPPING_FOREACH (pair, root) {
			const gchar *key = as_yaml_node_get_key0 (pair);
			const gchar *value = as_yaml_node_get_value0 (pair);

			if (key == NULL || value == NULL)
				continue;
			if (g_str_equal (key, "msg")) {
				msg = g_strdup (value);
				break;
			}
		}
	}

	fy_document_destroy (ydoc);
	fy_diag_destroy (ycfg.diag);

	return msg;
}

/**
 * as_reviews_client_fetch_reviews_internal:
 *
 * Fetch reviews from the reviews server, given a component-ID and
 * optionally a list of compatible alternative IDs and a version number.
 */
static GPtrArray *
as_reviews_client_fetch_reviews_internal (AsReviewsClient *rrc,
					  const gchar *cpt_id,
					  GPtrArray *compat_ids,
					  const gchar *version,
					  guint start,
					  guint limit,
					  GError **error)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	AsCurl *acurl;
	g_autofree gchar *url = NULL;
	g_autoptr(GBytes) request = NULL;
	g_autoptr(GBytes) reply = NULL;
	g_autoptr(GBytes) error_reply = NULL;
	g_autoptr(GError) tmp_error = NULL;

	acurl = as_reviews_client_get_curl (rrc, &tmp_error);
	if (acurl == NULL) {
		g_propagate_prefixed_error (error,
					    g_steal_pointer (&tmp_error),
					    "Unable to fetch reviews: ");
		return NULL;
	}

	if (limit == 0)
		limit = AS_REVIEWS_FETCH_LIMIT_DEFAULT;
	request = as_reviews_client_build_fetch_request (rrc,
							 cpt_id,
							 compat_ids,
							 version,
							 start,
							 limit);

	url = g_strconcat (priv->server_url, "/fetch", NULL);
	reply = as_curl_post_bytes (acurl,
				    url,
				    "application/json; charset=utf-8",
				    request,
				    &error_reply,
				    &tmp_error);
	if (reply == NULL) {
		g_autofree gchar *server_msg = NULL;

		/* the server may have sent a JSON error description with the failure status */
		if (error_reply != NULL)
			server_msg = as_reviews_client_get_server_error_msg (error_reply);

		g_set_error (
		    error,
		    AS_REVIEWS_CLIENT_ERROR,
		    AS_REVIEWS_CLIENT_ERROR_NETWORK,
		    /* TRANSLATORS: We failed to fetch user reviews from the ODRS review server */
		    _("Unable to fetch software reviews: %s"),
		      server_msg != NULL ? server_msg : tmp_error->message);
		return NULL;
	}

	return as_reviews_client_parse_reviews (rrc,
						g_bytes_get_data (reply, NULL),
						(gssize) g_bytes_get_size (reply),
						error);
}

/**
 * as_reviews_client_fetch_reviews:
 * @rrc: an #AsReviewsClient instance.
 * @cpt: the component to fetch reviews for.
 * @start: index of the first review to fetch, for pagination.
 * @limit: maximum amount of reviews to fetch, or 0 for the default limit.
 * @error: a #GError.
 *
 * Fetch user reviews for the given software component from the reviews server.
 * The reviews are sorted by their score (most helpful first), so subsequent
 * pages can be requested by increasing @start in steps of @limit.
 * This call does blocking network I/O.
 *
 * Returns: (transfer container) (element-type AsReview): the fetched reviews, or %NULL on error.
 **/
GPtrArray *
as_reviews_client_fetch_reviews (AsReviewsClient *rrc,
				 AsComponent *cpt,
				 guint start,
				 guint limit,
				 GError **error)
{
	AsReleaseList *releases;
	const gchar *cpt_id;
	const gchar *version = NULL;

	g_return_val_if_fail (AS_IS_REVIEWS_CLIENT (rrc), NULL);
	g_return_val_if_fail (AS_IS_COMPONENT (cpt), NULL);

	cpt_id = as_component_get_id (cpt);
	if (as_is_empty (cpt_id)) {
		g_set_error_literal (error,
				     AS_REVIEWS_CLIENT_ERROR,
				     AS_REVIEWS_CLIENT_ERROR_FAILED,
				     "Unable to fetch reviews for a component without ID.");
		return NULL;
	}

	/* use the latest known release version, if any is available */
	releases = as_component_get_releases_plain (cpt);
	if (releases != NULL && as_release_list_get_size (releases) > 0)
		version = as_release_get_version (as_release_list_index (releases, 0));

	return as_reviews_client_fetch_reviews_internal (rrc,
							 cpt_id,
							 as_component_get_replaces (cpt),
							 version,
							 start,
							 limit,
							 error);
}

/**
 * as_reviews_client_fetch_reviews_for_id:
 * @rrc: an #AsReviewsClient instance.
 * @component_id: the ID of the software component to fetch reviews for.
 * @version: (nullable): the version of the component, or %NULL if unknown.
 * @start: index of the first review to fetch, for pagination.
 * @limit: maximum amount of reviews to fetch, or 0 for the default limit.
 * @error: a #GError.
 *
 * Fetch user reviews for the given software component ID from the reviews server.
 * The reviews are sorted by their score (most helpful first), so subsequent
 * pages can be requested by increasing @start in steps of @limit.
 * This call does blocking network I/O.
 *
 * Returns: (transfer container) (element-type AsReview): the fetched reviews, or %NULL on error.
 **/
GPtrArray *
as_reviews_client_fetch_reviews_for_id (AsReviewsClient *rrc,
					const gchar *component_id,
					const gchar *version,
					guint start,
					guint limit,
					GError **error)
{
	g_return_val_if_fail (AS_IS_REVIEWS_CLIENT (rrc), NULL);
	g_return_val_if_fail (component_id != NULL, NULL);

	return as_reviews_client_fetch_reviews_internal (rrc,
							 component_id,
							 NULL,
							 version,
							 start,
							 limit,
							 error);
}

/**
 * as_reviews_client_fetch_rating_for_id:
 * @rrc: an #AsReviewsClient instance.
 * @component_id: the ID of the software component to fetch the rating for.
 * @error: a #GError.
 *
 * Fetch the overall user rating for the given software component ID
 * from the reviews server, as a percentage value (where 100% means
 * a perfect five-star rating).
 * This call does blocking network I/O.
 *
 * Returns: the overall rating percentage, or -1 if no rating was available.
 **/
gint
as_reviews_client_fetch_rating_for_id (AsReviewsClient *rrc,
				       const gchar *component_id,
				       GError **error)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	AsCurl *acurl;
	g_autofree gchar *url = NULL;
	g_autoptr(GBytes) reply = NULL;
	g_autoptr(GError) tmp_error = NULL;

	g_return_val_if_fail (AS_IS_REVIEWS_CLIENT (rrc), -1);
	g_return_val_if_fail (component_id != NULL, -1);

	acurl = as_reviews_client_get_curl (rrc, &tmp_error);
	if (acurl == NULL) {
		g_propagate_prefixed_error (error,
					    g_steal_pointer (&tmp_error),
					    "Unable to fetch rating: ");
		return -1;
	}

	url = g_strconcat (priv->server_url, "/ratings/", component_id, NULL);
	reply = as_curl_download_bytes (acurl, url, &tmp_error);
	if (reply == NULL) {
		g_set_error (
		    error,
		    AS_REVIEWS_CLIENT_ERROR,
		    AS_REVIEWS_CLIENT_ERROR_NETWORK,
		    /* TRANSLATORS: We failed to fetch a software rating from the ODRS review server */
		    _("Unable to fetch software rating: %s"), tmp_error->message);
		return -1;
	}

	return as_reviews_client_parse_rating (rrc,
					       g_bytes_get_data (reply, NULL),
					       (gssize) g_bytes_get_size (reply),
					       error);
}

/**
 * as_reviews_client_new:
 *
 * Creates a new #AsReviewsClient.
 *
 * Returns: (transfer full): an #AsReviewsClient
 **/
AsReviewsClient *
as_reviews_client_new (void)
{
	AsReviewsClient *rrc = g_object_new (AS_TYPE_REVIEWS_CLIENT, NULL);
	return AS_REVIEWS_CLIENT (rrc);
}
