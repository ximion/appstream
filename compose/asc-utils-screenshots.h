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

#pragma once

#include <glib-object.h>
#include <appstream.h>
#include "as-settings-private.h"
#include "as-curl.h"

#include "asc-result.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

/**
 * AscVideoInfo: (skip):
 *
 * Contains some basic information about the video
 * we downloaded from an upstream site.
 */
typedef struct {
	gchar *codec_name;
	gchar *audio_codec_name;
	gint width;
	gint height;
	gchar *format_name;
	AsVideoContainerKind container_kind;
	AsVideoCodecKind codec_kind;
	gboolean is_acceptable;
} AscVideoInfo;

AS_INTERNAL_VISIBLE
AscVideoInfo	*asc_extract_video_info (AscResult *cres,
					 AsComponent *cpt,
				         const gchar *vid_fname);
AS_INTERNAL_VISIBLE
void		asc_video_info_free (AscVideoInfo *vinfo);

void		asc_process_screenshots (AscResult *cres,
					 AsComponent *cpt,
					 AsCurl *acurl,
					 const gchar *media_export_root,
					 const gssize max_size_bytes,
					 gboolean process_videos,
					 gboolean store_screenshots);

#pragma GCC visibility pop
G_END_DECLS
