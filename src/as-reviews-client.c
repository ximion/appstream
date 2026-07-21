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
 *
 * All operations do blocking network I/O and this class is not thread-safe:
 * When calling it from worker threads, use one instance per thread or
 * serialize access to a shared instance with your own locking.
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

	gchar *distro_id;
	gchar *distro_version;
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

	g_free (priv->distro_id);
	g_free (priv->distro_version);
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
 * as_reviews_client_ensure_distro_info:
 *
 * Read the name, ID and version of the current operating system,
 * used to tell the reviews server about the platform the reviews
 * are fetched from and submitted for.
 */
static void
as_reviews_client_ensure_distro_info (AsReviewsClient *rrc)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	g_autoptr(AsSystemInfo) sysinfo = NULL;

	if (priv->distro_name != NULL)
		return;

	sysinfo = as_system_info_new ();
	priv->distro_name = g_strdup (as_system_info_get_os_name (sysinfo));
	if (priv->distro_name == NULL)
		priv->distro_name = g_strdup ("unknown");
	priv->distro_id = g_strdup (as_system_info_get_os_id (sysinfo));
	priv->distro_version = g_strdup (as_system_info_get_os_version (sysinfo));
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
 * as_json_document_new:
 *
 * Create a new document with a mapping as root, for building
 * a JSON request payload.
 */
static struct fy_document *
as_json_document_new (struct fy_node **root)
{
	struct fy_document *fyd = fy_document_create (NULL);
	*root = fy_node_create_mapping (fyd);
	fy_document_set_root (fyd, *root);
	return fyd;
}

/**
 * as_json_document_to_bytes:
 *
 * Emit the given document as JSON data and destroy it.
 */
static GBytes *
as_json_document_to_bytes (struct fy_document *fyd)
{
	gchar *json_data;

	json_data = fy_emit_document_to_string (fyd,
						FYECF_MODE_JSON_ONELINE | FYECF_WIDTH_INF |
						    FYECF_NO_ENDING_NEWLINE);
	fy_document_destroy (fyd);

	return g_bytes_new_take (json_data, strlen (json_data));
}

/**
 * as_json_new_str_node:
 *
 * Create a scalar node for a string value, ensuring it is never
 * emitted as a JSON number or boolean.
 */
static struct fy_node *
as_json_new_str_node (struct fy_document *fyd, const gchar *value)
{
	struct fy_node *value_node = fy_node_create_scalar_copy (fyd, value, FY_NT);
#if AS_FYAML_CHECK_VERSION(0, 9, 0)
	fy_node_set_style (value_node, FYNS_DOUBLE_QUOTED);
#endif
	return value_node;
}

/**
 * as_json_mapping_add_str:
 *
 * Add a string entry to a JSON mapping node.
 * Does nothing if @value is %NULL.
 */
static void
as_json_mapping_add_str (struct fy_document *fyd,
			 struct fy_node *map,
			 const gchar *key,
			 const gchar *value)
{
	if (value == NULL)
		return;
	fy_node_mapping_append (map,
				fy_node_create_scalar_copy (fyd, key, FY_NT),
				as_json_new_str_node (fyd, value));
}

/**
 * as_json_mapping_add_int:
 *
 * Add an integer entry to a JSON mapping node,
 * emitted as an unquoted number.
 */
static void
as_json_mapping_add_int (struct fy_document *fyd,
			 struct fy_node *map,
			 const gchar *key,
			 gint64 value)
{
	gchar buf[32];

	g_snprintf (buf, sizeof (buf), "%" G_GINT64_FORMAT, value);
	fy_node_mapping_append (map,
				fy_node_create_scalar_copy (fyd, key, FY_NT),
				fy_node_create_scalar_copy (fyd, buf, FY_NT));
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

	as_reviews_client_ensure_distro_info (rrc);

	fyd = as_json_document_new (&root);
	as_json_mapping_add_str (fyd, root, "user_hash", as_reviews_client_get_user_hash (rrc));
	as_json_mapping_add_str (fyd, root, "app_id", cpt_id);
	as_json_mapping_add_str (fyd, root, "locale", priv->locale);
	as_json_mapping_add_str (fyd, root, "distro", priv->distro_name);
	as_json_mapping_add_str (fyd, root, "version", version == NULL ? "unknown" : version);

	/* limit of reviews to fetch */
	as_json_mapping_add_int (fyd, root, "limit", limit);

	/* index of the first result to return, for pagination */
	if (start > 0)
		as_json_mapping_add_int (fyd, root, "start", start);

	if (compat_ids != NULL && compat_ids->len > 0) {
		struct fy_node *seq = fy_node_create_sequence (fyd);
		for (guint i = 0; i < compat_ids->len; i++) {
			const gchar *compat_id = g_ptr_array_index (compat_ids, i);
			if (g_strcmp0 (compat_id, cpt_id) == 0)
				continue;
			fy_node_sequence_append (seq, as_json_new_str_node (fyd, compat_id));
		}
		fy_node_mapping_append (root,
					fy_node_create_scalar_copy (fyd, "compat_ids", FY_NT),
					seq);
	}

	return as_json_document_to_bytes (fyd);
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
 * as_reviews_client_post_json:
 *
 * Send a JSON request to the given endpoint of the reviews server and
 * return the reply data, translating any transport failure or JSON error
 * description sent by the server into a useful #GError.
 */
static GBytes *
as_reviews_client_post_json (AsReviewsClient *rrc,
			     const gchar *endpoint,
			     GBytes *request,
			     GError **error)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	AsCurl *acurl;
	g_autofree gchar *url = NULL;
	g_autoptr(GBytes) reply = NULL;
	g_autoptr(GBytes) error_reply = NULL;
	g_autoptr(GError) tmp_error = NULL;

	acurl = as_reviews_client_get_curl (rrc, &tmp_error);
	if (acurl == NULL) {
		g_propagate_prefixed_error (error,
					    g_steal_pointer (&tmp_error),
					    "Unable to contact reviews server: ");
		return NULL;
	}

	url = g_strconcat (priv->server_url, endpoint, NULL);
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

		if (server_msg != NULL) {
			/* the server received & rejected our request, this is not a network issue */
			g_set_error (
			    error,
			    AS_REVIEWS_CLIENT_ERROR,
			    AS_REVIEWS_CLIENT_ERROR_FAILED,
			    /* TRANSLATORS: The ODRS review server rejected our request, %s is the error message it sent */
			    _("The reviews server rejected the request: %s"), server_msg);
		} else {
			g_set_error (
			    error,
			    AS_REVIEWS_CLIENT_ERROR,
			    AS_REVIEWS_CLIENT_ERROR_NETWORK,
			    /* TRANSLATORS: We failed to communicate with the ODRS review server, %s is an error message */
			    _("Unable to communicate with the reviews server: %s"),
			      tmp_error->message);
		}
		return NULL;
	}

	return g_steal_pointer (&reply);
}

/**
 * as_reviews_client_check_success_reply:
 * @reply: reply data received from an ODRS-compatible server.
 * @review_id: (out) (optional) (nullable): return location for a
 *   server-assigned review-ID, if one was sent.
 * @error: a #GError.
 *
 * Parse a status reply from an ODRS-compatible server, e.g.
 * `{"success": true, "review_id": 42}`, and set an error with the
 * server-provided message if the request was not successful.
 *
 * Returns: %TRUE if the server reported success.
 */
gboolean
as_reviews_client_check_success_reply (GBytes *reply, gchar **review_id, GError **error)
{
	struct fy_document *ydoc = NULL;
	struct fy_node *root = NULL;
	g_autofree gchar *success_str = NULL;
	g_autofree gchar *msg = NULL;
	gconstpointer data;
	gsize data_len;
	struct fy_parse_cfg ycfg = { .search_path = "", .flags = FYPCF_JSON_FORCE };

	if (review_id != NULL)
		*review_id = NULL;

	data = g_bytes_get_data (reply, &data_len);
	if (data == NULL || data_len == 0) {
		g_set_error_literal (error,
				     AS_REVIEWS_CLIENT_ERROR,
				     AS_REVIEWS_CLIENT_ERROR_PARSE,
				     "The reviews server sent an empty status reply.");
		return FALSE;
	}

	ycfg.diag = as_yaml_error_diag_create ();
	ydoc = fy_document_build_from_string (&ycfg, data, data_len);
	if (ydoc == NULL) {
		g_autofree gchar *issue_msg = as_yaml_make_error_message (ycfg.diag);
		g_set_error (error,
			     AS_REVIEWS_CLIENT_ERROR,
			     AS_REVIEWS_CLIENT_ERROR_PARSE,
			     "Failed to parse status reply: %s",
			     issue_msg);
		fy_diag_destroy (ycfg.diag);
		return FALSE;
	}

	root = fy_document_root (ydoc);
	if (root != NULL && fy_node_is_mapping (root)) {
		AS_YAML_MAPPING_FOREACH (pair, root) {
			const gchar *key = as_yaml_node_get_key0 (pair);
			const gchar *value = as_yaml_node_get_value0 (pair);

			if (key == NULL || value == NULL)
				continue;
			if (g_str_equal (key, "success"))
				success_str = g_strdup (value);
			else if (g_str_equal (key, "msg"))
				msg = g_strdup (value);
			else if (review_id != NULL && g_str_equal (key, "review_id"))
				*review_id = g_strdup (value);
		}
	}

	fy_document_destroy (ydoc);
	fy_diag_destroy (ycfg.diag);

	if (g_strcmp0 (success_str, "true") != 0) {
		if (review_id != NULL)
			g_clear_pointer (review_id, g_free);
		g_set_error (
		    error,
		    AS_REVIEWS_CLIENT_ERROR,
		    AS_REVIEWS_CLIENT_ERROR_FAILED,
		    /* TRANSLATORS: The ODRS review server did not accept our request, %s is a (possibly server-provided) reason */
		    _("The reviews server rejected the request: %s"),
		      msg != NULL ? msg : _("Unknown reason"));
		return FALSE;
	}

	return TRUE;
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
	g_autoptr(GBytes) request = NULL;
	g_autoptr(GBytes) reply = NULL;

	if (limit == 0)
		limit = AS_REVIEWS_FETCH_LIMIT_DEFAULT;
	request = as_reviews_client_build_fetch_request (rrc,
							 cpt_id,
							 compat_ids,
							 version,
							 start,
							 limit);

	reply = as_reviews_client_post_json (rrc, "/fetch", request, error);
	if (reply == NULL) {
		g_prefix_error_literal (error, "Unable to fetch software reviews: ");
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
 * as_reviews_client_build_submit_request:
 *
 * Create the JSON payload for submitting a new review.
 */
static GBytes *
as_reviews_client_build_submit_request (AsReviewsClient *rrc, const gchar *cpt_id, AsReview *review)
{
	AsReviewsClientPrivate *priv = GET_PRIVATE (rrc);
	struct fy_document *fyd = NULL;
	struct fy_node *root = NULL;
	const gchar *locale;
	const gchar *version;

	as_reviews_client_ensure_distro_info (rrc);
	locale = as_review_get_locale (review);
	version = as_review_get_version (review);

	fyd = as_json_document_new (&root);
	as_json_mapping_add_str (fyd, root, "user_hash", as_reviews_client_get_user_hash (rrc));
	as_json_mapping_add_str (fyd, root, "app_id", cpt_id);
	as_json_mapping_add_str (fyd, root, "locale", locale == NULL ? priv->locale : locale);
	as_json_mapping_add_str (fyd, root, "distro", priv->distro_name);
	as_json_mapping_add_str (fyd, root, "distro_id", priv->distro_id);
	as_json_mapping_add_str (fyd, root, "distro_version", priv->distro_version);
	as_json_mapping_add_str (fyd, root, "version", version == NULL ? "unknown" : version);
	as_json_mapping_add_str (fyd, root, "user_display", as_review_get_reviewer_name (review));
	as_json_mapping_add_str (fyd, root, "summary", as_review_get_summary (review));
	as_json_mapping_add_str (fyd, root, "description", as_review_get_description (review));

	/* the star rating percentage */
	as_json_mapping_add_int (fyd, root, "rating", as_review_get_rating (review));

	return as_json_document_to_bytes (fyd);
}

/**
 * as_reviews_client_submit_review:
 * @rrc: an #AsReviewsClient instance.
 * @component_id: the ID of the software component the review is for.
 * @review: the review to submit.
 *
 * Submit a new user review for a software component to the reviews server.
 * The @review must have a rating, summary and description set. If it has no
 * reviewer name set, a suitable one is chosen automatically based on the
 * name of the current user.
 *
 * On success, the review is updated with its server-assigned ID and is
 * marked as written by the current user.
 * This call does blocking network I/O.
 *
 * Returns: %TRUE on success.
 *
 * Since: 1.1.4
 **/
gboolean
as_reviews_client_submit_review (AsReviewsClient *rrc,
				 const gchar *component_id,
				 AsReview *review,
				 GError **error)
{
	g_autoptr(GBytes) request = NULL;
	g_autoptr(GBytes) reply = NULL;
	g_autofree gchar *review_id = NULL;
	gint rating;

	g_return_val_if_fail (AS_IS_REVIEWS_CLIENT (rrc), FALSE);
	g_return_val_if_fail (AS_IS_REVIEW (review), FALSE);
	g_return_val_if_fail (component_id != NULL, FALSE);

	rating = as_review_get_rating (review);
	if (rating < 1 || rating > 100) {
		g_set_error (error,
			     AS_REVIEWS_CLIENT_ERROR,
			     AS_REVIEWS_CLIENT_ERROR_FAILED,
			     "Unable to submit review: The review rating of %i is invalid "
			     "(expected a percentage between 1 and 100).",
			     rating);
		return FALSE;
	}

	/* pick a suitable display name of the review author, if none was set */
	if (as_review_get_reviewer_name (review) == NULL)
		as_review_set_reviewer_name (review, g_get_real_name ());

	request = as_reviews_client_build_submit_request (rrc, component_id, review);
	reply = as_reviews_client_post_json (rrc, "/submit", request, error);
	if (reply == NULL)
		return FALSE;
	if (!as_reviews_client_check_success_reply (reply, &review_id, error))
		return FALSE;

	/* update the local review with data we submitted, so it can be displayed
	 * right away. Note that voting on or removing the review is only possible
	 * after fetching it back from the server, as that provides the required
	 * user session key. */
	if (review_id != NULL)
		as_review_set_id (review, review_id);
	as_review_add_metadata (review, "ODRS::app_id", component_id);
	as_review_add_flags (review, AS_REVIEW_FLAG_SELF);

	return TRUE;
}

/**
 * as_reviews_client_review_action:
 *
 * Perform an action on an existing review, e.g. voting on it or removing it.
 * The @review must have been received via a previous fetch operation, so it
 * has the required server-provided metadata attached.
 */
static gboolean
as_reviews_client_review_action (AsReviewsClient *rrc,
				 AsReview *review,
				 const gchar *endpoint,
				 GError **error)
{
	struct fy_document *fyd = NULL;
	struct fy_node *root = NULL;
	g_autoptr(GBytes) request = NULL;
	g_autoptr(GBytes) reply = NULL;
	g_autoptr(GError) tmp_error = NULL;
	const gchar *review_id;
	const gchar *app_id;
	const gchar *user_skey;
	gint64 review_id_num;

	review_id = as_review_get_id (review);
	app_id = as_review_get_metadata_item (review, "ODRS::app_id");
	user_skey = as_review_get_metadata_item (review, "ODRS::user_skey");

	if (review_id == NULL || app_id == NULL || user_skey == NULL) {
		g_set_error_literal (
		    error,
		    AS_REVIEWS_CLIENT_ERROR,
		    AS_REVIEWS_CLIENT_ERROR_FAILED,
		    "Unable to perform review action: The review is missing its ID or server "
		    "metadata. Only reviews received from the server can be acted on.");
		return FALSE;
	}

	/* the server expects the review-ID as a number */
	if (!g_ascii_string_to_signed (review_id, 10, 1, G_MAXINT64, &review_id_num, &tmp_error)) {
		g_propagate_prefixed_error (error,
					    g_steal_pointer (&tmp_error),
					    "Unable to perform review action: ");
		return FALSE;
	}

	fyd = as_json_document_new (&root);
	as_json_mapping_add_str (fyd, root, "app_id", app_id);
	as_json_mapping_add_str (fyd, root, "user_hash", as_reviews_client_get_user_hash (rrc));
	as_json_mapping_add_str (fyd, root, "user_skey", user_skey);
	as_json_mapping_add_int (fyd, root, "review_id", review_id_num);
	request = as_json_document_to_bytes (fyd);

	reply = as_reviews_client_post_json (rrc, endpoint, request, error);
	if (reply == NULL)
		return FALSE;

	return as_reviews_client_check_success_reply (reply, NULL, error);
}

/**
 * as_reviews_client_vote_review:
 * @rrc: an #AsReviewsClient instance.
 * @review: the review to vote on.
 * @vote: the kind of vote to cast, e.g. %AS_REVIEW_VOTE_KIND_UP.
 *
 * Cast a vote on a review that was previously received from the reviews
 * server, to mark it as helpful or unhelpful, or to report it as abusive.
 * On success, the review is marked as voted on by the current user.
 * This call does blocking network I/O.
 *
 * Returns: %TRUE on success.
 *
 * Since: 1.1.4
 **/
gboolean
as_reviews_client_vote_review (AsReviewsClient *rrc,
			       AsReview *review,
			       AsReviewVoteKind vote,
			       GError **error)
{
	const gchar *endpoint = NULL;

	g_return_val_if_fail (AS_IS_REVIEWS_CLIENT (rrc), FALSE);
	g_return_val_if_fail (AS_IS_REVIEW (review), FALSE);

	switch (vote) {
	case AS_REVIEW_VOTE_KIND_UP:
		endpoint = "/upvote";
		break;
	case AS_REVIEW_VOTE_KIND_DOWN:
		endpoint = "/downvote";
		break;
	case AS_REVIEW_VOTE_KIND_REPORT:
		endpoint = "/report";
		break;
	default:
		g_set_error_literal (error,
				     AS_REVIEWS_CLIENT_ERROR,
				     AS_REVIEWS_CLIENT_ERROR_FAILED,
				     "Unable to vote on review: An invalid vote kind was given.");
		return FALSE;
	}

	if (!as_reviews_client_review_action (rrc, review, endpoint, error))
		return FALSE;

	/* don't allow the user to vote twice */
	as_review_add_flags (review, AS_REVIEW_FLAG_VOTED);

	return TRUE;
}

/**
 * as_reviews_client_remove_review:
 * @rrc: an #AsReviewsClient instance.
 * @review: the review to remove.
 *
 * Remove a review that the current user has written from the reviews server.
 * The review must have been received from the server via a previous fetch
 * operation, and the server will refuse to remove reviews that were not
 * written by the current user.
 * This call does blocking network I/O.
 *
 * Returns: %TRUE on success.
 *
 * Since: 1.1.4
 **/
gboolean
as_reviews_client_remove_review (AsReviewsClient *rrc, AsReview *review, GError **error)
{
	g_return_val_if_fail (AS_IS_REVIEWS_CLIENT (rrc), FALSE);
	g_return_val_if_fail (AS_IS_REVIEW (review), FALSE);

	return as_reviews_client_review_action (rrc, review, "/remove", error);
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
