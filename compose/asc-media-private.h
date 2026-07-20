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

#pragma once

#include "as-macros-private.h"
#include "asc-media.h"

AS_BEGIN_PRIVATE_DECLS

/**
 * AscImageTarget:
 * @name: Filename (without directory components) the rendition should be stored as.
 * @scale_mode: How the image should be scaled for this rendition.
 * @width: Target width (used depending on @scale_mode).
 * @height: Target height (used depending on @scale_mode).
 * @save_flags: Flags to use when storing this rendition.
 * @only_downscale: Skip this rendition if it would upscale the source image.
 * @skip_if_src_width_gt: Skip this rendition if the source image is wider than this value (0 to disable).
 * @skipped: Result: Whether this rendition was skipped due to one of the skip conditions.
 * @result_width: Result: Actual width of the stored rendition.
 * @result_height: Result: Actual height of the stored rendition.
 * @error_msg: Result: Error message if (and only if) creating this particular rendition failed.
 *
 * Describes one requested output rendition of a media operation, and
 * carries its result data after the operation was run.
 */
typedef struct {
	gchar		 *name;
	AscImageScaleMode scale_mode;
	gint		  width;
	gint		  height;
	AscImageSaveFlags save_flags;
	gboolean	  only_downscale;
	gint		  skip_if_src_width_gt;

	/* result fields, set by the media operation */
	gboolean	  skipped;
	gint		  result_width;
	gint		  result_height;
	gchar		 *error_msg;
} AscImageTarget;

#define ASC_TYPE_IMAGE_TARGET (asc_image_target_get_type ())
AS_INTERNAL_VISIBLE
GType asc_image_target_get_type (void);
AS_INTERNAL_VISIBLE
AscImageTarget *asc_image_target_new (const gchar      *name,
				      AscImageScaleMode scale_mode,
				      gint		width,
				      gint		height);
AS_INTERNAL_VISIBLE
void asc_image_target_free (AscImageTarget *target);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (AscImageTarget, asc_image_target_free)

/**
 * AscFontInfo:
 *
 * Metadata of a font file, as extracted by the media worker.
 * String fields are %NULL if the respective information was not available.
 */
typedef struct {
	gchar  *family;
	gchar  *style;
	gchar  *fullname;
	gchar  *id;
	gchar  *description;
	gchar  *homepage;
	gchar **languages;
	gchar  *preferred_language;
	gchar  *sample_text;
	gchar  *sample_icon_text;
} AscFontInfo;

#define ASC_TYPE_FONT_INFO (asc_font_info_get_type ())
AS_INTERNAL_VISIBLE
GType asc_font_info_get_type (void);
AS_INTERNAL_VISIBLE
void asc_font_info_free (AscFontInfo *info);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (AscFontInfo, asc_font_info_free)

AS_INTERNAL_VISIBLE
void asc_media_set_max_input_size (AscMedia *media, guint64 max_size);
AS_INTERNAL_VISIBLE
void asc_media_set_memory_limit (AscMedia *media, guint32 limit_mib);

AS_INTERNAL_VISIBLE
gboolean asc_media_process_image (AscMedia	   *media,
				  GBytes	   *img_data,
				  AscImageFormat    format_hint,
				  gint		    load_width,
				  gint		    load_height,
				  AscImageLoadFlags load_flags,
				  const gchar	   *out_dir,
				  GPtrArray	   *targets,
				  gint		   *src_width,
				  gint		   *src_height,
				  GError	  **error);

AS_INTERNAL_VISIBLE
AscFontInfo *asc_media_read_font_info (AscMedia		  *media,
				       GBytes		  *font_data,
				       const gchar	  *basename,
				       const gchar	  *preferred_language,
				       const gchar *const *extra_languages,
				       const gchar	  *custom_sample_text,
				       const gchar	  *custom_icon_text,
				       GError		 **error);

AS_INTERNAL_VISIBLE
gboolean asc_media_render_font_card (AscMedia		*media,
				     GBytes		*font_data,
				     const gchar	*basename,
				     const gchar	*preferred_language,
				     const gchar *const *extra_languages,
				     const gchar	*custom_sample_text,
				     const gchar	*custom_icon_text,
				     const gchar	*info_label,
				     const gchar	*out_dir,
				     GPtrArray		*targets,
				     GError	       **error);

AS_INTERNAL_VISIBLE
gboolean asc_media_render_font_icon (AscMedia		*media,
				     GBytes		*font_data,
				     const gchar	*basename,
				     const gchar	*preferred_language,
				     const gchar *const *extra_languages,
				     const gchar	*custom_sample_text,
				     const gchar	*custom_icon_text,
				     const gchar	*out_dir,
				     GPtrArray		*targets,
				     GError	       **error);

AS_INTERNAL_VISIBLE
gboolean asc_media_probe_video (AscMedia    *media,
				GBytes	    *video_data,
				const gchar *basename,
				gchar	   **codec_name,
				gchar	   **audio_codec_name,
				gchar	   **format_name,
				gint	    *width,
				gint	    *height,
				GError	   **error);

AS_END_PRIVATE_DECLS
