/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2024 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined(__APPSTREAM_H) && !defined(AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_PROMOTIONAL_H
#define __AS_PROMOTIONAL_H

#include <glib-object.h>

#include "as-context.h"
#include "as-image.h"
#include "as-video.h"

G_BEGIN_DECLS

#define AS_TYPE_PROMOTIONAL (as_promotional_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsPromotional, as_promotional, AS, PROMOTIONAL, GObject)

struct _AsPromotionalClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

/**
 * AsPromotionalMediaKind:
 * @AS_PROMOTIONAL_MEDIA_KIND_UNKNOWN:	Media kind is unknown
 * @AS_PROMOTIONAL_MEDIA_KIND_IMAGE:	The promotional contains images
 * @AS_PROMOTIONAL_MEDIA_KIND_VIDEO:	The promotional contains videos
 *
 * The media kind contained in this promotional.
 **/
typedef enum {
	AS_PROMOTIONAL_MEDIA_KIND_UNKNOWN,
	AS_PROMOTIONAL_MEDIA_KIND_IMAGE,
	AS_PROMOTIONAL_MEDIA_KIND_VIDEO,
	/*< private >*/
	AS_PROMOTIONAL_MEDIA_KIND_LAST
} AsPromotionalMediaKind;

/**
 * AsPromotionalContainsText:
 * @AS_PROMOTIONAL_CONTAINS_TEXT_UNKNOWN:	Whether this promotional contains text is unset
 * @AS_PROMOTIONAL_CONTAINS_TEXT_YES:		The promotional image/video contains text
 * @AS_PROMOTIONAL_CONTAINS_TEXT_NO:		The promotional image/video does not contain text
 *
 * Whether this promotional item contains text that is integral to understanding it.
 * This attribute must be set explicitly; its absence triggers a validator warning.
 **/
typedef enum {
	AS_PROMOTIONAL_CONTAINS_TEXT_UNKNOWN,
	AS_PROMOTIONAL_CONTAINS_TEXT_YES,
	AS_PROMOTIONAL_CONTAINS_TEXT_NO,
	/*< private >*/
	AS_PROMOTIONAL_CONTAINS_TEXT_LAST
} AsPromotionalContainsText;

gboolean		     as_promotional_is_valid (AsPromotional *promotional);

AsPromotional		    *as_promotional_new (void);

AsPromotionalMediaKind	     as_promotional_get_media_kind (AsPromotional *promotional);

AsPromotionalContainsText    as_promotional_get_contains_text (AsPromotional *promotional);
void			     as_promotional_set_contains_text (AsPromotional		  *promotional,
							       AsPromotionalContainsText   contains_text);

const gchar		    *as_promotional_get_environment (AsPromotional *promotional);
void			     as_promotional_set_environment (AsPromotional *promotional,
							     const gchar   *env_id);

AsContext		    *as_promotional_get_context (AsPromotional *promotional);
void			     as_promotional_set_context (AsPromotional *promotional,
							 AsContext      *context);

const gchar		    *as_promotional_get_caption (AsPromotional *promotional);
void			     as_promotional_set_caption (AsPromotional *promotional,
							 const gchar   *caption,
							 const gchar   *locale);

GPtrArray		    *as_promotional_get_images_all (AsPromotional *promotional);
GPtrArray		    *as_promotional_get_images (AsPromotional *promotional);
AsImage			    *as_promotional_get_image (AsPromotional *promotional,
						       guint		width,
						       guint		height,
						       guint		scale);
void			     as_promotional_add_image (AsPromotional *promotional, AsImage *image);
void			     as_promotional_clear_images (AsPromotional *promotional);

GPtrArray		    *as_promotional_get_videos_all (AsPromotional *promotional);
GPtrArray		    *as_promotional_get_videos (AsPromotional *promotional);
void			     as_promotional_add_video (AsPromotional *promotional, AsVideo *video);

G_END_DECLS

#endif /* __AS_PROMOTIONAL_H */
