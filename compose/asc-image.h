/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (__APPSTREAM_COMPOSE_H) && !defined (ASC_COMPILATION)
#error "Only <appstream-compose.h> can be included directly."
#endif
#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define ASC_TYPE_IMAGE (asc_image_get_type ())
G_DECLARE_FINAL_TYPE (AscImage, asc_image, ASC, IMAGE, GObject)

/**
 * AscImageFormat:
 * @ASC_IMAGE_FORMAT_UNKNOWN:	Unknown image format.
 * @ASC_IMAGE_FORMAT_PNG:	PNG format
 * @ASC_IMAGE_FORMAT_JPEG:	JPEG format
 * @ASC_IMAGE_FORMAT_GIF:	GIF format
 * @ASC_IMAGE_FORMAT_SVG:	SVG format
 * @ASC_IMAGE_FORMAT_SVGZ:	Compressed SVG format
 * @ASC_IMAGE_FORMAT_WEBP:	WebP format
 * @ASC_IMAGE_FORMAT_AVIF:	AVIF format
 * @ASC_IMAGE_FORMAT_XPM:	XPM format
 *
 * File format of an image.
 **/
typedef enum {
	ASC_IMAGE_FORMAT_UNKNOWN,
	ASC_IMAGE_FORMAT_PNG,
	ASC_IMAGE_FORMAT_JPEG,
	ASC_IMAGE_FORMAT_GIF,
	ASC_IMAGE_FORMAT_SVG,
	ASC_IMAGE_FORMAT_SVGZ,
	ASC_IMAGE_FORMAT_WEBP,
	ASC_IMAGE_FORMAT_AVIF,
	ASC_IMAGE_FORMAT_XPM,
	/*< private >*/
	ASC_IMAGE_FORMAT_LAST
} AscImageFormat;

const gchar*	asc_image_format_to_string (AscImageFormat format);
AscImageFormat	asc_image_format_from_string (const gchar *str);
AscImageFormat	asc_image_format_from_filename (const gchar *fname);

gboolean	asc_optimize_png (const gchar *fname, GError **error);
GHashTable	*asc_image_supported_format_names (void);

AscImage	*asc_image_new_from_file (const gchar* fname,
					  GError **error);
AscImage	*asc_image_new_from_data (const void *data,
					  gssize len,
					  GError **error);

guint		asc_image_get_width (AscImage *image);
guint		asc_image_get_height (AscImage *image);

void		asc_image_scale (AscImage *image,
				 guint new_width,
				 guint new_height);

void		asc_image_scale_to_width (AscImage *image,
					  guint new_width);
void		asc_image_scale_to_height (AscImage *image,
					   guint new_height);
void		asc_image_scale_to_fit (AscImage *image,
					guint size);

gboolean	asc_image_save_png (AscImage *image,
				    const gchar *fname,
				    GError **error);

G_END_DECLS
