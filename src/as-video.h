/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_VIDEO_H
#define __AS_VIDEO_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_VIDEO (as_video_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsVideo, as_video, AS, VIDEO, GObject)

struct _AsVideoClass
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
 * AsVideoCodec:
 * @AS_VIDEO_CODEC_UNKNOWN:	Unknown video codec
 * @AS_VIDEO_CODEC_VP9:		The VP9 video codec
 * @AS_VIDEO_CODEC_AV1:		The AV1 video codec
 *
 * Supported video codecs.
 **/
typedef enum {
	AS_VIDEO_CODEC_UNKNOWN,
	AS_VIDEO_CODEC_VP9,
	AS_VIDEO_CODEC_AV1,
	/*< private >*/
	AS_VIDEO_CODEC_LAST
} AsVideoCodec;

/**
 * AsVideoContainer:
 * @AS_VIDEO_CONTAINER_UNKNOWN:		Unknown video container
 * @AS_VIDEO_CONTAINER_MKV:		The Matroska video (MKV) container
 * @AS_VIDEO_CONTAINER_WEBM:		The WebM video container
 *
 * Supported video codecs.
 **/
typedef enum {
	AS_VIDEO_CONTAINER_UNKNOWN,
	AS_VIDEO_CONTAINER_MKV,
	AS_VIDEO_CONTAINER_WEBM,
	/*< private >*/
	AS_VIDEO_CONTAINER_LAST
} AsVideoContainer;

AsVideoCodec		as_video_codec_from_string (const gchar *codec);
const gchar		*as_video_codec_to_string (AsVideoCodec codec);

AsVideoContainer	as_video_container_from_string (const gchar *container);
const gchar		*as_video_container_to_string (AsVideoContainer container);

AsVideo			*as_video_new (void);

AsVideoCodec		as_video_get_codec (AsVideo *video);
void			as_video_set_codec (AsVideo *video,
					    AsVideoCodec codec);

AsVideoContainer	as_video_get_container (AsVideo *video);
void			as_video_set_container (AsVideo *video,
						AsVideoContainer container);

const gchar		*as_video_get_url (AsVideo *video);
void			 as_video_set_url (AsVideo *video,
						const gchar *url);

guint			 as_video_get_width (AsVideo *video);
void			 as_video_set_width (AsVideo *video,
						guint width);

guint			 as_video_get_height (AsVideo *video);
void			 as_video_set_height (AsVideo *video,
						guint height);

const gchar		*as_video_get_locale (AsVideo *video);
void			 as_video_set_locale (AsVideo *video,
						const gchar *locale);

G_END_DECLS

#endif /* __AS_VIDEO_H */
