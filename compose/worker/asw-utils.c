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

#include "config.h"
#include "asw-utils.h"

#include <gio/gio.h>
#include <string.h>
#include <unistd.h>

/**
 * SECTION:asw-utils
 * @short_description: Helper functions for the Compose MediaWorker
 */

/**
 * asw_read_fd_head:
 * @fd: file descriptor to read from.
 * @buf: buffer to read into.
 * @buf_len: size of @buf.
 *
 * Read up to @buf_len leading bytes of @fd, without disturbing its file offset.
 *
 * Returns: the amount of bytes read, zero if the descriptor could not be read.
 */
gsize
asw_read_fd_head (gint fd, guchar *buf, gsize buf_len)
{
	gssize len;

	if (fd < 0)
		return 0;

	len = pread (fd, buf, buf_len, 0);
	if (len < 0)
		return 0;

	return (gsize) len;
}

/**
 * asw_data_is_html:
 *
 * Check whether @data looks like the beginning of an HTML document.
 */
static gboolean
asw_data_is_html (const guchar *data, gsize len)
{
	const gchar *html_tags[] = {
		"<!doctype html", "<html", "<head", "<body", "<script", "<meta"
	};
	gsize offset = 0;

	/* skip an UTF-8 BOM and any leading whitespace */
	if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
		offset = 3;
	while (offset < len && g_ascii_isspace (data[offset]))
		offset++;

	for (gsize i = 0; i < G_N_ELEMENTS (html_tags); i++) {
		gsize tag_len = strlen (html_tags[i]);
		if (len - offset < tag_len)
			continue;
		if (g_ascii_strncasecmp ((const gchar *) data + offset, html_tags[i], tag_len) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * asw_data_is_matroska:
 * @data: the data to inspect.
 * @len: length of @data.
 *
 * Check whether @data starts with an EBML header.
 */
gboolean
asw_data_is_matroska (const guchar *data, gsize len)
{
	const guchar ebml_magic[] = { 0x1A, 0x45, 0xDF, 0xA3 };

	return len >= sizeof (ebml_magic) && memcmp (data, ebml_magic, sizeof (ebml_magic)) == 0;
}

/**
 * asw_describe_data:
 * @data: the data to inspect.
 * @len: length of @data.
 * @content_type: (out) (optional) (nullable): the content type we found, or %NULL
 *   if we could not determine it.
 *
 * Determine what @data actually is by looking at its content.
 *
 * Returns: %TRUE if we could determine the content type.
 */
gboolean
asw_describe_data (const guchar *data, gsize len, gchar **content_type)
{
	g_autofree gchar *guessed_type = NULL;
	gboolean uncertain = TRUE;

	if (content_type != NULL)
		*content_type = NULL;

	if (data == NULL || len == 0) {
		if (content_type != NULL)
			*content_type = g_strdup ("application/x-zerosize");
		return TRUE;
	}

	/* We handle HTML detection ourselves in case shared-mime-info is missing.
	 * Due to the prevalence of AI-defenses, this is the one thing we need to
	 * detect reliably, to give better error messages. */
	if (asw_data_is_html (data, len)) {
		if (content_type != NULL)
			*content_type = g_strdup ("text/html");
		return TRUE;
	}

	guessed_type = g_content_type_guess (NULL, data, len, &uncertain);
	if (guessed_type == NULL || uncertain)
		return FALSE;

	/* this is what we get told when there is nothing to tell */
	if (g_strcmp0 (guessed_type, "application/octet-stream") == 0)
		return FALSE;

	if (content_type != NULL)
		*content_type = g_steal_pointer (&guessed_type);

	return TRUE;
}

/**
 * asw_describe_wrong_media:
 * @content_type: (nullable): the content type we actually have, as determined by
 *   %asw_describe_data, or %NULL if we could not determine it.
 * @expected_prefix: (nullable): content type prefix the data should have had,
 *   e.g. `image/`, or %NULL if we can not say.
 *
 * Explain what we ended up with instead of usable media.
 *
 * Returns: (transfer full): an explanation to add to an error message.
 */
gchar *
asw_describe_wrong_media (const gchar *content_type, const gchar *expected_prefix)
{
	if (content_type == NULL)
		return g_strdup ("the data could not be identified");

	if (g_str_equal (content_type, "text/html"))
		return g_strdup ("the data is an HTML document - a download may have hit an error "
				 "page or bot protection");

	if (g_str_equal (content_type, "application/x-zerosize"))
		return g_strdup ("the file was empty");

	if (expected_prefix != NULL && g_str_has_prefix (content_type, expected_prefix))
		return g_strdup_printf ("the %s data could not be read, it may be damaged "
					"or truncated",
					content_type);

	return g_strdup_printf ("the data is %s", content_type);
}
