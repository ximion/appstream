/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-utils
 * @short_description: Common utility functions for AppStream-Compose.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-utils.h"
#include "as-utils-private.h"

#ifdef HAVE_BLAKE3
#include <blake3.h>
#endif

/**
 * asc_build_component_global_id:
 * @component_id: an AppStream component ID.
 * @checksum: a checksum as string generated from the component's combined metadata.
 *
 * Builds a global component ID from a component-id
 * and a checksum (usually Blake3) generated from the component data.
 *
 * The global-id is used as a global, unique identifier for a component.
 * (while the component-ID is local, e.g. for one source).
 * Its primary usecase is to identify a media directory on the filesystem which is
 * associated with this component.
 *
 * Returns: The new global ID. Free with %g_free.
 **/
gchar *
asc_build_component_global_id (const gchar *component_id, const gchar *checksum)
{
	gboolean rdns_split;
	g_auto(GStrv) parts = NULL;

	if (as_is_empty (component_id))
		return NULL;
	if (as_is_empty (checksum))
		checksum = "last";

	g_return_val_if_fail (strlen (component_id) > 2, NULL);

	/* check whether we can build the gcid by using the reverse domain name,
	 * or whether we should use the simple standard splitter. */
	rdns_split = FALSE;
	parts = g_strsplit (component_id, ".", 3);

	if (g_strv_length (parts) == 3) {
		/* check if we have a valid TLD. If so, use the reverse-domain-name splitting. */
		if (as_utils_is_tld (parts[0]))
			rdns_split = TRUE;
	}

	if (rdns_split) {
		g_autofree gchar *tld_part = NULL;
		g_autofree gchar *domain_part = NULL;
		tld_part = g_utf8_strdown (parts[0], -1);
		domain_part = g_utf8_strdown (parts[1], -1);

		return g_strdup_printf ("%s/%s/%s/%s", tld_part, domain_part, parts[2], checksum);
	} else {
		g_autofree gchar *cid_low = NULL;
		g_autofree gchar *pdiv_part = NULL;
		g_autofree gchar *sdiv_part = NULL;
		cid_low = g_utf8_strdown (component_id, -1);
		pdiv_part = g_utf8_substring (cid_low, 0, 1);
		sdiv_part = g_utf8_substring (cid_low, 0, 2);

		return g_strdup_printf ("%s/%s/%s/%s", pdiv_part, sdiv_part, cid_low, checksum);
	}
}

/**
 * asc_compute_content_checksum_for_data:
 * @data: The data to hash.
 * @length: Length of @data.
 *
 * Compute a checksum for the given content.
 *
 * The output of this function is intended to be used with %asc_build_component_global_id
 * to form a unique global ID. The generated checksum is intended to be used as content-ID.
 * Do not assume it is cryptographically secure or has a certain length!
 *
 * Returns: The hash as hexadecimal string. Free with %g_free
 */
gchar *
asc_compute_content_checksum_for_data (const gchar *data, gsize length)
{
#ifdef HAVE_BLAKE3
	blake3_hasher hasher;
	uint8_t out[BLAKE3_OUT_LEN];

	blake3_hasher_init (&hasher);
	blake3_hasher_update (&hasher, data, length);

	blake3_hasher_finalize (&hasher, out, BLAKE3_OUT_LEN);

	/* we just take the first half of the hash (128 bits), which should be sufficiently strong
	 * for our content-ID (especially combined with the rest of the GCID) while also being as
	 * short as an MD5 hash */
	/* clang-format off */
	return g_strdup_printf (
		"%02x%02x%02x%02x%02x%02x%02x%02x"
		"%02x%02x%02x%02x%02x%02x%02x%02x",
		out[0], out[1], out[2], out[3],
		out[4], out[5], out[6], out[7],
		out[8], out[9], out[10], out[11],
		out[12], out[13], out[14], out[15]
	);
	/* clang-format on */
#else
	g_return_val_if_fail (data != NULL || length == 0, NULL);

	/* just fall back to MD5 if we were built without Blake3 */
	return g_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) data, length);
#endif
}

/**
 * asc_filename_from_url:
 * @url: The URL to extract a filename from.
 *
 * Generate a filename from a web-URL that can be used to store the
 * file on disk after download.
 */
gchar *
asc_filename_from_url (const gchar *url)
{
	gchar *tmp;
	g_autofree gchar *unescaped = NULL;

	if (url == NULL)
		return NULL;
	unescaped = g_uri_unescape_string (url, NULL);
	tmp = g_strstr_len (unescaped, -1, "?");
	if (tmp != NULL)
		tmp[0] = '\0';
	tmp = g_strstr_len (unescaped, -1, "#");
	if (tmp != NULL)
		tmp[0] = '\0';

	/* we couldn't extract a suitable name, so just give it a random string as result */
	if (unescaped[0] == '\0')
		return as_random_alnum_string (4);
	return g_path_get_basename (unescaped);
}
