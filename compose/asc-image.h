/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include <appstream.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define ASC_TYPE_IMAGE (asc_image_get_type ())
G_DECLARE_FINAL_TYPE (AscImage, asc_image, ASC, IMAGE, GObject)

/**
 * AscImageSaveFlags:
 * @ASC_IMAGE_SAVE_FLAG_NONE:		No special flags set
 * @ASC_IMAGE_SAVE_FLAG_OPTIMIZE:	Optimize generated PNG for size
 * @ASC_IMAGE_SAVE_FLAG_PAD_16_9:	Pad with alpha to 16:9 aspect
 * @ASC_IMAGE_SAVE_FLAG_SHARPEN:	Sharpen the image to clarify detail
 * @ASC_IMAGE_SAVE_FLAG_BLUR:		Blur the image to clear detail
 *
 * The flags used for saving images.
 **/
typedef enum {
	ASC_IMAGE_SAVE_FLAG_NONE	= 0,
	ASC_IMAGE_SAVE_FLAG_OPTIMIZE	= 1 << 0,
	ASC_IMAGE_SAVE_FLAG_PAD_16_9	= 1 << 1,
	ASC_IMAGE_SAVE_FLAG_SHARPEN	= 1 << 2,
	ASC_IMAGE_SAVE_FLAG_BLUR	= 1 << 3,
	/*< private >*/
	ASC_IMAGE_SAVE_FLAG_LAST
} AscImageSaveFlags;

/**
 * AscImageLoadFlags:
 * @ASC_IMAGE_LOAD_FLAG_NONE:			No special flags set
 * @ASC_IMAGE_LOAD_FLAG_SHARPEN:		Sharpen the resulting image
 * @ASC_IMAGE_LOAD_FLAG_ALLOW_UNSUPPORTED:	Allow loading of unsupported image types.
 * @ASC_IMAGE_LOAD_FLAG_ALWAYS_RESIZE:		Always resize the source image to the perfect size
 *
 * The flags used for loading images.
 **/
typedef enum {
	ASC_IMAGE_LOAD_FLAG_NONE		= 0,
	ASC_IMAGE_LOAD_FLAG_SHARPEN		= 1 << 0,
	ASC_IMAGE_LOAD_FLAG_ALLOW_UNSUPPORTED	= 1 << 1,
	ASC_IMAGE_LOAD_FLAG_ALWAYS_RESIZE	= 1 << 2,
	/*< private >*/
	ASC_IMAGE_LOAD_FLAG_LAST
} AscImageLoadFlags;

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

/**
 * AscImageError:
 * @ASC_IMAGE_ERROR_FAILED:	Generic failure.
 *
 * An image processing error.
 **/
typedef enum {
	ASC_IMAGE_ERROR_FAILED,
	/*< private >*/
	ASC_IMAGE_ERROR_LAST
} AscImageError;

#define	ASC_IMAGE_ERROR	asc_image_error_quark ()
GQuark			asc_image_error_quark (void);

const gchar*	asc_image_format_to_string (AscImageFormat format);
AscImageFormat	asc_image_format_from_string (const gchar *str);
AscImageFormat	asc_image_format_from_filename (const gchar *fname);

gboolean	 asc_optimize_png (const gchar *fname,
				   GError **error);
GHashTable	*asc_image_supported_format_names (void);

AscImage	*asc_image_new (void);
AscImage	*asc_image_new_from_file (const gchar* fname,
					  guint dest_size,
					  AscImageLoadFlags flags,
					  GError **error);
AscImage	*asc_image_new_from_data (const void *data,
					  gssize len,
					  guint dest_size,
					  gboolean compressed,
					  AscImageLoadFlags flags,
					  GError **error);

gboolean	 asc_image_load_filename (AscImage *image,
					  const gchar *filename,
					  guint dest_size,
					  guint src_size_min,
					  AscImageLoadFlags flags,
					  GError **error);

GdkPixbuf	*asc_image_save_pixbuf (AscImage *image,
				        guint width,
					guint height,
					AscImageSaveFlags flags);
gboolean	 asc_image_save_filename (AscImage *image,
					  const gchar *filename,
					  guint width,
					  guint height,
					  AscImageSaveFlags flags,
					  GError **error);

GdkPixbuf 	*asc_image_get_pixbuf (AscImage *image);
void		asc_image_set_pixbuf (AscImage *image,
				      GdkPixbuf *pixbuf);

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

void		asc_pixbuf_blur (GdkPixbuf *src,
				 gint radius,
				 gint iterations);
void		asc_pixbuf_sharpen (GdkPixbuf *src,
				    gint radius,
				    gdouble amount);

G_END_DECLS
