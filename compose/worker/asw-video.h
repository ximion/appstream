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

#include <glib.h>

G_BEGIN_DECLS

/**
 * AswVideoProbe:
 *
 * Raw information about a video file, as determined by ffprobe.
 * Any interpretation and validation of these values is up to the client.
 */
typedef struct {
	gchar *codec_name;
	gchar *audio_codec_name;
	gchar *format_name;
	gint   width;
	gint   height;
} AswVideoProbe;

AswVideoProbe *asw_probe_video (const gchar *vid_fname, GError **error);
void	       asw_video_probe_free (AswVideoProbe *probe);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AswVideoProbe, asw_video_probe_free)

G_END_DECLS
