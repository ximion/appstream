/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined(__APPSTREAM_COMPOSE_H) && !defined(ASW_COMPILATION)
#error "Only <appstream-compose.h> can be included directly."
#endif
#pragma once

#include <glib-object.h>
#include <appstream.h>

G_BEGIN_DECLS

#define ASW_TYPE_IMAGE (asw_image_get_type ())
G_DECLARE_FINAL_TYPE (AswImage, asw_image, ASW, IMAGE, GObject)

/**
 * AswImageSaveFlags:
 * @ASW_IMAGE_SAVE_FLAG_NONE:		No special flags set
 * @ASW_IMAGE_SAVE_FLAG_OPTIMIZE:	Optimize generated PNG for size
 * @ASW_IMAGE_SAVE_FLAG_PAD_16_9:	Pad with alpha to 16:9 aspect
 * @ASW_IMAGE_SAVE_FLAG_SHARPEN:	Sharpen the image to clarify detail
 * @ASW_IMAGE_SAVE_FLAG_BLUR:		Blur the image to clear detail
 *
 * The flags used for saving images.
 **/
typedef enum {
	ASW_IMAGE_SAVE_FLAG_NONE     = 0,
	ASW_IMAGE_SAVE_FLAG_OPTIMIZE = 1 << 0,
	ASW_IMAGE_SAVE_FLAG_PAD_16_9 = 1 << 1,
	ASW_IMAGE_SAVE_FLAG_SHARPEN  = 1 << 2,
	ASW_IMAGE_SAVE_FLAG_BLUR     = 1 << 3,
	/*< private >*/
	ASW_IMAGE_SAVE_FLAG_LAST
} AswImageSaveFlags;

/**
 * AswImageLoadFlags:
 * @ASW_IMAGE_LOAD_FLAG_NONE:			No special flags set
 * @ASW_IMAGE_LOAD_FLAG_SHARPEN:		Sharpen the resulting image
 * @ASW_IMAGE_LOAD_FLAG_ALLOW_UNSUPPORTED:	Allow loading of unsupported image types.
 * @ASW_IMAGE_LOAD_FLAG_ALWAYS_RESIZE:		Always resize the source image to the perfect size
 *
 * The flags used for loading images.
 **/
typedef enum {
	ASW_IMAGE_LOAD_FLAG_NONE	      = 0,
	ASW_IMAGE_LOAD_FLAG_SHARPEN	      = 1 << 0,
	ASW_IMAGE_LOAD_FLAG_ALLOW_UNSUPPORTED = 1 << 1,
	ASW_IMAGE_LOAD_FLAG_ALWAYS_RESIZE     = 1 << 2,
	/*< private >*/
	ASW_IMAGE_LOAD_FLAG_LAST
} AswImageLoadFlags;

/**
 * AswImageFormat:
 * @ASW_IMAGE_FORMAT_UNKNOWN:	Unknown image format.
 * @ASW_IMAGE_FORMAT_PNG:	PNG format
 * @ASW_IMAGE_FORMAT_JXL:	JPEG-XL format
 * @ASW_IMAGE_FORMAT_AVIF:	AVIF format
 * @ASW_IMAGE_FORMAT_WEBP:	WebP format
 * @ASW_IMAGE_FORMAT_SVG:	SVG format
 * @ASW_IMAGE_FORMAT_SVGZ:	Compressed SVG format
 * @ASW_IMAGE_FORMAT_JPEG:	JPEG format
 * @ASW_IMAGE_FORMAT_GIF:	GIF format
 * @ASW_IMAGE_FORMAT_XPM:	XPM format
 *
 * File format of an image.
 **/
typedef enum {
	ASW_IMAGE_FORMAT_UNKNOWN,
	ASW_IMAGE_FORMAT_PNG,
	ASW_IMAGE_FORMAT_JXL,
	ASW_IMAGE_FORMAT_AVIF,
	ASW_IMAGE_FORMAT_WEBP,
	ASW_IMAGE_FORMAT_SVG,
	ASW_IMAGE_FORMAT_SVGZ,
	ASW_IMAGE_FORMAT_JPEG,
	ASW_IMAGE_FORMAT_GIF,
	ASW_IMAGE_FORMAT_XPM,
	/*< private >*/
	ASW_IMAGE_FORMAT_LAST
} AswImageFormat;

/**
 * AswImageError:
 * @ASW_IMAGE_ERROR_FAILED:	 Generic failure.
 * @ASW_IMAGE_ERROR_UNSUPPORTED: The graphic type is not supported.
 *
 * An image processing error.
 **/
typedef enum {
	ASW_IMAGE_ERROR_FAILED,
	ASW_IMAGE_ERROR_UNSUPPORTED,
	/*< private >*/
	ASW_IMAGE_ERROR_LAST
} AswImageError;

#define ASW_IMAGE_ERROR asw_image_error_quark ()
GQuark	       asw_image_error_quark (void);

const gchar   *asw_image_format_to_string (AswImageFormat format);
AswImageFormat asw_image_format_from_string (const gchar *str);
AswImageFormat asw_image_format_from_filename (const gchar *fname);

GHashTable    *asw_image_supported_format_names (void);

AswImage      *asw_image_new (void);
AswImage      *asw_image_new_from_file (const gchar	 *fname,
					gint		  dest_width,
					gint		  dest_height,
					AswImageLoadFlags flags,
					GError		**error);
AswImage      *asw_image_new_from_data (const void	 *data,
					gssize		  len,
					gint		  dest_width,
					gint		  dest_height,
					AswImageLoadFlags flags,
					AswImageFormat	  format_hint,
					GError		**error);

gboolean       asw_image_load_filename (AswImage	 *image,
					const gchar	 *filename,
					gint		  dest_width,
					gint		  dest_height,
					gint		  src_size_min,
					AswImageLoadFlags flags,
					GError		**error);

gboolean       asw_image_save_filename (AswImage	 *image,
					const gchar	 *filename,
					gint		  width,
					gint		  height,
					AswImageSaveFlags flags,
					GError		**error);

gint	       asw_image_get_width (AswImage *image);
gint	       asw_image_get_height (AswImage *image);

void	       asw_image_scale (AswImage *image, gint new_width, gint new_height);

void	       asw_image_scale_to_width (AswImage *image, gint new_width);
void	       asw_image_scale_to_height (AswImage *image, gint new_height);
void	       asw_image_scale_to_fit (AswImage *image, gint size);

gboolean       asw_render_svg_to_file (GInputStream  *stream,
				       gint	      width,
				       gint	      height,
				       AswImageFormat format,
				       const gchar   *filename,
				       GError	    **error);

G_END_DECLS
