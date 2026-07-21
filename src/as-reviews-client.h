/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#if !defined(__APPSTREAM_H_INSIDE__) && !defined(AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#include <glib-object.h>
#include "as-component.h"

G_BEGIN_DECLS

#define AS_TYPE_REVIEWS_CLIENT (as_reviews_client_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsReviewsClient, as_reviews_client, AS, REVIEWS_CLIENT, GObject)

struct _AsReviewsClientClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
	void (*_as_reserved7) (void);
	void (*_as_reserved8) (void);
};

/**
 * AsReviewsClientError:
 * @AS_REVIEWS_CLIENT_ERROR_FAILED:	Generic failure.
 * @AS_REVIEWS_CLIENT_ERROR_NETWORK:	Could not communicate with the ratings server.
 * @AS_REVIEWS_CLIENT_ERROR_PARSE:	Data received from the server could not be parsed.
 *
 * A ratings client error.
 *
 * Since: 1.1.4
 **/
typedef enum {
	AS_REVIEWS_CLIENT_ERROR_FAILED,
	AS_REVIEWS_CLIENT_ERROR_NETWORK,
	AS_REVIEWS_CLIENT_ERROR_PARSE,
	/*< private >*/
	AS_REVIEWS_CLIENT_ERROR_LAST
} AsReviewsClientError;

#define AS_REVIEWS_CLIENT_ERROR as_reviews_client_error_quark ()
GQuark as_reviews_client_error_quark (void);

/**
 * AsReviewVoteKind:
 * @AS_REVIEW_VOTE_KIND_UNKNOWN:	Unknown vote action.
 * @AS_REVIEW_VOTE_KIND_UP:		Vote the review up, it was helpful.
 * @AS_REVIEW_VOTE_KIND_DOWN:		Vote the review down, it was unhelpful.
 * @AS_REVIEW_VOTE_KIND_REPORT:		Report the review as abusive.
 *
 * A vote cast on a user review.
 *
 * Since: 1.1.4
 **/
typedef enum {
	AS_REVIEW_VOTE_KIND_UNKNOWN,
	AS_REVIEW_VOTE_KIND_UP,
	AS_REVIEW_VOTE_KIND_DOWN,
	AS_REVIEW_VOTE_KIND_REPORT,
	/*< private >*/
	AS_REVIEW_VOTE_KIND_LAST
} AsReviewVoteKind;

AsReviewsClient *as_reviews_client_new (void);

const gchar	*as_reviews_client_get_server_url (AsReviewsClient *rrc);
void		 as_reviews_client_set_server_url (AsReviewsClient *rrc, const gchar *url);

const gchar	*as_reviews_client_get_client_id (AsReviewsClient *rrc);
void		 as_reviews_client_set_client_id (AsReviewsClient *rrc, const gchar *client_id);

const gchar	*as_reviews_client_get_user_agent (AsReviewsClient *rrc);
void		 as_reviews_client_set_user_agent (AsReviewsClient *rrc, const gchar *user_agent);

const gchar	*as_reviews_client_get_user_hash (AsReviewsClient *rrc);
void		 as_reviews_client_set_user_hash (AsReviewsClient *rrc, const gchar *user_hash);

const gchar	*as_reviews_client_get_locale (AsReviewsClient *rrc);
void		 as_reviews_client_set_locale (AsReviewsClient *rrc, const gchar *locale);

GPtrArray	*as_reviews_client_fetch_reviews (AsReviewsClient *rrc,
						  AsComponent	  *cpt,
						  guint		   start,
						  guint		   limit,
						  GError	 **error);
GPtrArray	*as_reviews_client_fetch_reviews_for_id (AsReviewsClient *rrc,
							 const gchar	 *component_id,
							 const gchar	 *version,
							 guint		  start,
							 guint		  limit,
							 GError		**error);

gint		 as_reviews_client_fetch_rating_for_id (AsReviewsClient *rrc,
							const gchar	*component_id,
							GError	       **error);

gboolean	 as_reviews_client_submit_review (AsReviewsClient *rrc,
						  const gchar	  *component_id,
						  AsReview	  *review,
						  GError	 **error);
gboolean	 as_reviews_client_vote_review (AsReviewsClient *rrc,
						AsReview	*review,
						AsReviewVoteKind vote,
						GError	       **error);
gboolean as_reviews_client_remove_review (AsReviewsClient *rrc, AsReview *review, GError **error);

G_END_DECLS
