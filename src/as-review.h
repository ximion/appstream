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

#pragma once

#if !defined (__APPSTREAM_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_REVIEW (as_review_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsReview, as_review, AS, REVIEW, GObject)

struct _AsReviewClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

/**
 * AsReviewFlags:
 * @AS_REVIEW_FLAG_NONE:	No special flags set
 * @AS_REVIEW_FLAG_SELF:	The user wrote the review themselves
 * @AS_REVIEW_FLAG_VOTED:	The user voted on the review
 *
 * The flags for the review.
 *
 * Since: 0.14.0
 **/
typedef enum {
	AS_REVIEW_FLAG_NONE	= 0,
	AS_REVIEW_FLAG_SELF	= 1 << 0,
	AS_REVIEW_FLAG_VOTED	= 1 << 1,
	/*< private >*/
	AS_REVIEW_FLAG_LAST
} AsReviewFlags;

AsReview	*as_review_new			(void);

gint		 as_review_get_priority (AsReview *review);
void		 as_review_set_priority (AsReview *review,
					 gint priority);

const gchar	*as_review_get_id (AsReview *review);
void		 as_review_set_id (AsReview *review,
				   const gchar *id);

const gchar	*as_review_get_summary (AsReview *review);
void		 as_review_set_summary (AsReview *review,
					const gchar *summary);

const gchar	*as_review_get_description (AsReview *review);
void		 as_review_set_description (AsReview *review,
					    const gchar *description);

const gchar	*as_review_get_locale (AsReview	*review);
void		 as_review_set_locale (AsReview	*review,
					const gchar *locale);

gint		 as_review_get_rating (AsReview *review);
void		 as_review_set_rating (AsReview *review,
					gint rating);

const gchar	*as_review_get_version (AsReview *review);
void		 as_review_set_version (AsReview *review,
					const gchar *version);

const gchar	*as_review_get_reviewer_id (AsReview *review);
void		 as_review_set_reviewer_id (AsReview *review,
					    const gchar *reviewer_id);

const gchar	*as_review_get_reviewer_name (AsReview *review);
void		 as_review_set_reviewer_name (AsReview *review,
					      const gchar *reviewer_name);

GDateTime	*as_review_get_date (AsReview *review);
void		 as_review_set_date (AsReview *review,
				     GDateTime *date);

AsReviewFlags	 as_review_get_flags (AsReview *review);
void		 as_review_set_flags (AsReview *review,
				      AsReviewFlags flags);
void		 as_review_add_flags (AsReview *review,
				      AsReviewFlags flags);

const gchar	*as_review_get_metadata_item (AsReview *review,
					      const gchar *key);
void		 as_review_add_metadata (AsReview *review,
					 const gchar *key,
					 const gchar *value);

gboolean	 as_review_equal (AsReview *review1,
				  AsReview *review2);

G_END_DECLS
