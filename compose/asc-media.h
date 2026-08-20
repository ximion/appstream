/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined(__APPSTREAM_COMPOSE_H) && !defined(ASC_COMPILATION)
#error "Only <appstream-compose.h> can be included directly."
#endif
#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * AscMediaError:
 * @ASC_MEDIA_ERROR_FAILED:		Generic failure.
 * @ASC_MEDIA_ERROR_DEAD_WORKER:	The media worker process crashed or could not be spawned.
 * @ASC_MEDIA_ERROR_TIMEOUT:		A media operation took too long and was aborted.
 * @ASC_MEDIA_ERROR_PROTOCOL:		Communication with the media worker was corrupted.
 * @ASC_MEDIA_ERROR_NOT_FOUND:		A required component or data was not found.
 * @ASC_MEDIA_ERROR_UNSUPPORTED:	The requested operation or image type was not supported.
 *
 * A media processing error.
 **/
typedef enum {
	ASC_MEDIA_ERROR_FAILED,
	ASC_MEDIA_ERROR_DEAD_WORKER,
	ASC_MEDIA_ERROR_TIMEOUT,
	ASC_MEDIA_ERROR_PROTOCOL,
	ASC_MEDIA_ERROR_NOT_FOUND,
	ASC_MEDIA_ERROR_UNSUPPORTED,
	/*< private >*/
	ASC_MEDIA_ERROR_LAST
} AscMediaError;

#define ASC_MEDIA_ERROR asc_media_error_quark ()
GQuark asc_media_error_quark (void);

/**
 * AscImageScaleMode:
 * @ASC_IMAGE_SCALE_MODE_NONE:		Keep the original (loaded) image size.
 * @ASC_IMAGE_SCALE_MODE_EXACT:		Scale to the exact target dimensions.
 * @ASC_IMAGE_SCALE_MODE_FIT_WIDTH:	Scale proportionally to match the target width.
 * @ASC_IMAGE_SCALE_MODE_FIT_HEIGHT:	Scale proportionally to match the target height.
 *
 * How an image rendition should be scaled.
 **/
typedef enum {
	ASC_IMAGE_SCALE_MODE_NONE,
	ASC_IMAGE_SCALE_MODE_EXACT,
	ASC_IMAGE_SCALE_MODE_FIT_WIDTH,
	ASC_IMAGE_SCALE_MODE_FIT_HEIGHT,
	/*< private >*/
	ASC_IMAGE_SCALE_MODE_LAST
} AscImageScaleMode;

/**
 * AscImageSaveFlags:
 * @ASC_IMAGE_SAVE_FLAG_NONE:		No special flags set
 * @ASC_IMAGE_SAVE_FLAG_LOSSLESS:	Encode the image losslessly
 * @ASC_IMAGE_SAVE_FLAG_OPTIMIZE:	Optimize generated PNG for size
 * @ASC_IMAGE_SAVE_FLAG_PAD_16_9:	Pad with alpha to 16:9 aspect
 * @ASC_IMAGE_SAVE_FLAG_SHARPEN:	Sharpen the image to clarify detail
 * @ASC_IMAGE_SAVE_FLAG_BLUR:		Blur the image to clear detail
 *
 * The flags used for saving images.
 **/
typedef enum {
	ASC_IMAGE_SAVE_FLAG_NONE     = 0,
	ASC_IMAGE_SAVE_FLAG_LOSSLESS = 1 << 0,
	ASC_IMAGE_SAVE_FLAG_OPTIMIZE = 1 << 1,
	ASC_IMAGE_SAVE_FLAG_PAD_16_9 = 1 << 2,
	ASC_IMAGE_SAVE_FLAG_SHARPEN  = 1 << 3,
	ASC_IMAGE_SAVE_FLAG_BLUR     = 1 << 4,
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
	ASC_IMAGE_LOAD_FLAG_NONE	      = 0,
	ASC_IMAGE_LOAD_FLAG_SHARPEN	      = 1 << 0,
	ASC_IMAGE_LOAD_FLAG_ALLOW_UNSUPPORTED = 1 << 1,
	ASC_IMAGE_LOAD_FLAG_ALWAYS_RESIZE     = 1 << 2,
	/*< private >*/
	ASC_IMAGE_LOAD_FLAG_LAST
} AscImageLoadFlags;

/**
 * AscImageFormat:
 * @ASC_IMAGE_FORMAT_UNKNOWN:	Unknown image format.
 * @ASC_IMAGE_FORMAT_PNG:	PNG format
 * @ASC_IMAGE_FORMAT_JXL:	JPEG-XL format
 * @ASC_IMAGE_FORMAT_AVIF:	AVIF format
 * @ASC_IMAGE_FORMAT_WEBP:	WebP format
 * @ASC_IMAGE_FORMAT_SVG:	SVG format
 * @ASC_IMAGE_FORMAT_SVGZ:	Compressed SVG format
 * @ASC_IMAGE_FORMAT_JPEG:	JPEG format
 * @ASC_IMAGE_FORMAT_GIF:	GIF format
 *
 * File format of an image.
 **/
typedef enum {
	ASC_IMAGE_FORMAT_UNKNOWN,
	ASC_IMAGE_FORMAT_PNG,
	ASC_IMAGE_FORMAT_JXL,
	ASC_IMAGE_FORMAT_AVIF,
	ASC_IMAGE_FORMAT_WEBP,
	ASC_IMAGE_FORMAT_SVG,
	ASC_IMAGE_FORMAT_SVGZ,
	ASC_IMAGE_FORMAT_JPEG,
	ASC_IMAGE_FORMAT_GIF,
	/*< private >*/
	ASC_IMAGE_FORMAT_LAST
} AscImageFormat;

const gchar		      *asc_image_format_to_string (AscImageFormat format);
AscImageFormat		       asc_image_format_from_string (const gchar *str);
AscImageFormat		       asc_image_format_from_filename (const gchar *fname);

typedef struct _AscImageTarget AscImageTarget;

#define ASC_TYPE_IMAGE_TARGET (asc_image_target_get_type ())
GType		asc_image_target_get_type (void);

AscImageTarget *asc_image_target_new (const gchar      *name,
				      AscImageScaleMode scale_mode,
				      gint		width,
				      gint		height);
AscImageTarget *asc_image_target_copy (AscImageTarget *target);
void		asc_image_target_free (AscImageTarget *target);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (AscImageTarget, asc_image_target_free)

const gchar *asc_image_target_get_name (AscImageTarget *target);

void	     asc_image_target_set_save_flags (AscImageTarget *target, AscImageSaveFlags flags);
void	     asc_image_target_set_only_downscale (AscImageTarget *target, gboolean only_downscale);
void	     asc_image_target_set_source_size_range (AscImageTarget *target,
						     gint	     min_width,
						     gint	     min_height,
						     gint	     max_width,
						     gint	     max_height);

gboolean     asc_image_target_get_skipped (AscImageTarget *target);
gint	     asc_image_target_get_result_width (AscImageTarget *target);
gint	     asc_image_target_get_result_height (AscImageTarget *target);
const gchar *asc_image_target_get_error_message (AscImageTarget *target);

#define ASC_TYPE_MEDIA (asc_media_get_type ())
G_DECLARE_FINAL_TYPE (AscMedia, asc_media, ASC, MEDIA, GObject)

AscMedia    *asc_media_new (void);

guint	     asc_media_get_request_timeout (AscMedia *media);
void	     asc_media_set_request_timeout (AscMedia *media, guint seconds);

guint32	     asc_media_get_memory_limit (AscMedia *media);
void	     asc_media_set_memory_limit (AscMedia *media, guint32 limit_mib);

const gchar *asc_media_get_worker_path (AscMedia *media);
void	     asc_media_set_worker_path (AscMedia *media, const gchar *path);

gboolean     asc_media_ensure_worker (AscMedia *media, GError **error);
void	     asc_media_stop (AscMedia *media);

gboolean     asc_media_error_is_worker_failure (const GError *error);

gboolean     asc_media_process_image (AscMedia	       *media,
				      GBytes	       *img_data,
				      gint		load_width,
				      gint		load_height,
				      AscImageLoadFlags load_flags,
				      const gchar      *out_dir,
				      GPtrArray	       *targets,
				      gint	       *src_width,
				      gint	       *src_height,
				      GError	      **error);

G_END_DECLS
