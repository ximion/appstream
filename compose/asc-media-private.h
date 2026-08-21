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

AS_INTERNAL_VISIBLE
const gchar *asc_media_get_worker_path (AscMedia *media);
AS_INTERNAL_VISIBLE
void asc_media_set_worker_path (AscMedia *media, const gchar *path);

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
				const gchar *video_fname,
				gchar	   **codec_name,
				gchar	   **audio_codec_name,
				gchar	   **format_name,
				gint	    *width,
				gint	    *height,
				GError	   **error);

AS_END_PRIVATE_DECLS
