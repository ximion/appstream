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

/**
 * SECTION:asw-video
 * @short_description: Video probing (via ffprobe) for the AppStream media worker.
 */

#include "config.h"
#include "asw-video.h"

#include "asc-globals.h"
#include "asc-media.h"

/**
 * asw_video_probe_free:
 *
 * Free an #AswVideoProbe.
 */
void
asw_video_probe_free (AswVideoProbe *probe)
{
	if (probe == NULL)
		return;
	g_free (probe->codec_name);
	g_free (probe->audio_codec_name);
	g_free (probe->format_name);
	g_free (probe);
}

/**
 * asw_probe_video:
 * @vid_fname: Path to the video file to inspect.
 * @error: A #GError or %NULL
 *
 * Run ffprobe on the given video file and extract raw stream information.
 *
 * Returns: (transfer full): Video information, or %NULL on error.
 */
AswVideoProbe *
asw_probe_video (const gchar *vid_fname, GError **error)
{
	g_autoptr(AswVideoProbe) probe = NULL;
	gboolean ret = FALSE;
	g_autofree gchar *ff_stdout = NULL;
	g_autofree gchar *ff_stderr = NULL;
	gint exit_status = 0;
	g_auto(GStrv) lines = NULL;
	g_autofree gchar *prev_codec_name = NULL;
	GError *tmp_error = NULL;
	const gchar *ffprobe_argv[] = { asc_globals_get_ffprobe_binary (),
					"-v",
					"quiet",
					"-show_entries",
					"stream=width,height,codec_name,codec_type",
					"-show_entries",
					"format=format_name",
					"-of",
					"default=noprint_wrappers=1",
					vid_fname,
					NULL };

	if (asc_globals_get_ffprobe_binary () == NULL) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "Unable to probe video file: No ffprobe binary was found.");
		return NULL;
	}

	ret = g_spawn_sync (NULL, /* working directory */
			    (gchar **) ffprobe_argv,
			    NULL, /* envp */
			    G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
			    NULL, /* child setup */
			    NULL, /* user data */
			    &ff_stdout,
			    &ff_stderr,
			    &exit_status,
			    &tmp_error);
	if (!ret) {
		g_propagate_prefixed_error (error, tmp_error, "Failed to spawn ffprobe: ");
		return NULL;
	}

	if (exit_status != 0) {
		if (ff_stderr == NULL) {
			ff_stderr = g_strdup (ff_stdout);
		} else {
			g_autofree gchar *dummy = ff_stderr;
			ff_stderr = g_strconcat (ff_stderr, "\n", ff_stdout, NULL);
		}
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Code %i, %s",
			     exit_status,
			     ff_stderr);
		return NULL;
	}

	probe = g_new0 (AswVideoProbe, 1);

	/* NOTE: We are currently extracting information from ffprobe's simple output, but it also has a JSON
	 * mode. Parsing JSON is a bit slower, but if it is more reliable we should switch to that. */
	lines = g_strsplit (ff_stdout, "\n", -1);
	for (guint i = 0; lines[i] != NULL; i++) {
		gchar *tmp;
		gchar *value;
		gchar *key;
		tmp = g_strstr_len (lines[i], -1, "=");
		if (tmp == NULL)
			continue;
		value = tmp + 1;
		key = lines[i];
		tmp[0] = '\0';

		if (g_strcmp0 (key, "codec_name") == 0) {
			g_free (prev_codec_name);
			prev_codec_name = g_strdup (value);
			continue;
		}
		if (g_strcmp0 (key, "codec_type") == 0) {
			if (g_strcmp0 (value, "video") == 0) {
				if (probe->codec_name == NULL)
					probe->codec_name = g_strdup (prev_codec_name);
			} else if (g_strcmp0 (value, "audio") == 0) {
				if (probe->audio_codec_name == NULL)
					probe->audio_codec_name = g_strdup (prev_codec_name);
			}
			continue;
		}
		if (g_strcmp0 (key, "format_name") == 0) {
			if (probe->format_name == NULL)
				probe->format_name = g_strdup (value);
			continue;
		}
		if (g_strcmp0 (key, "width") == 0) {
			if (g_strcmp0 (value, "N/A") != 0)
				probe->width = g_ascii_strtoll (value, NULL, 10);
			continue;
		}
		if (g_strcmp0 (key, "height") == 0) {
			if (g_strcmp0 (value, "N/A") != 0)
				probe->height = g_ascii_strtoll (value, NULL, 10);
			continue;
		}
	}

	return g_steal_pointer (&probe);
}
