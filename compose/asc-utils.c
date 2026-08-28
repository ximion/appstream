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
#include "asc-utils-private.h"
#include "as-utils-private.h"
#include "asc-globals.h"

#ifdef HAVE_BLAKE3
#include <blake3.h>
#endif

/**
 * asc_gcid_verify_part:
 *
 * Verify one piece of the path we are about to build, and explain which piece it was
 * if it does not hold up. Every segment is checked individually: the component-ID is
 * split apart to form the path, so a piece of it may be unusable as a directory name
 * even when the ID reads fine as a whole.
 */
static gboolean
asc_gcid_verify_part (const gchar *part, const gchar *kind, GError **error)
{
	g_autofree gchar *part_printable = NULL;

	if (G_LIKELY (as_path_segment_verify (part)))
		return TRUE;

	/* one of the things we reject here is invalid UTF-8, so we must not put the
	 * offending piece into the error message verbatim - the message ends up in issue
	 * hints and in the generated reports, which have to be valid UTF-8 */
	part_printable = g_utf8_make_valid (part, -1);
	g_set_error (error,
		     ASC_COMPOSE_ERROR,
		     ASC_COMPOSE_ERROR_FAILED,
		     "The %s '%s' is invalid.",
		     kind,
		     part_printable);
	return FALSE;
}

/**
 * asc_build_component_global_id:
 * @component_id: an AppStream component ID.
 * @checksum: a checksum as string generated from the component's combined metadata.
 * @error: a #GError or %NULL
 *
 * Builds a global component ID from a component-id
 * and a checksum (usually Blake3) generated from the component data.
 *
 * The global-id is used as a global, unique identifier for a component.
 * (while the component-ID is local, e.g. for one source).
 * Its primary usecase is to identify a media directory on the filesystem which is
 * associated with this component.
 *
 * Since the result is used to construct filesystem paths, this function will refuse
 * to build a global ID unless every segment it is made of - the pieces of the
 * component-ID as well as @checksum - is safe to use as a directory name. The error
 * that is set in that case names the offending piece, and is meant to be shown to
 * whoever wrote the metadata.
 *
 * Returns: (transfer full) (nullable): The new global ID, or %NULL on error.
 *
 * Since: 0.13.0
 **/
gchar *
asc_build_component_global_id (const gchar *component_id, const gchar *checksum, GError **error)
{
	gboolean rdns_split;
	g_auto(GStrv) parts = NULL;

	if (as_is_empty (component_id)) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     "The component-ID is empty.");
		return NULL;
	}
	if (as_is_empty (checksum))
		checksum = "last";

	if (strlen (component_id) <= 2) {
		g_autofree gchar *cid_printable = g_utf8_make_valid (component_id, -1);
		g_set_error (error,
			     ASC_COMPOSE_ERROR,
			     ASC_COMPOSE_ERROR_FAILED,
			     "The component-ID '%s' is shorter than three characters.",
			     cid_printable);
		return NULL;
	}

	/* the checksum becomes the last segment of the path we build, so it has to hold up
	 * as a directory name just like the pieces of the ID do. We check it here, before
	 * anything is allocated, so a bad one costs us nothing but this scan. */
	if (!asc_gcid_verify_part (checksum, "checksum", error))
		return NULL;

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

		if (!asc_gcid_verify_part (tld_part, "top-level domain", error) ||
		    !asc_gcid_verify_part (domain_part, "domain part", error) ||
		    !asc_gcid_verify_part (parts[2], "last part of the component-ID", error))
			return NULL;

		return g_strdup_printf ("%s/%s/%s/%s", tld_part, domain_part, parts[2], checksum);
	} else {
		g_autofree gchar *cid_low = NULL;
		g_autofree gchar *pdiv_part = NULL;
		g_autofree gchar *sdiv_part = NULL;
		cid_low = g_utf8_strdown (component_id, -1);
		pdiv_part = g_utf8_substring (cid_low, 0, 1);
		sdiv_part = g_utf8_substring (cid_low, 0, 2);

		if (!asc_gcid_verify_part (pdiv_part,
					   "first character of the component-ID",
					   error) ||
		    !asc_gcid_verify_part (sdiv_part,
					   "first two characters of the component-ID",
					   error) ||
		    !asc_gcid_verify_part (cid_low, "component-ID", error))
			return NULL;

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
 * Returns: (transfer full): The hash as hexadecimal string. Free with %g_free
 *
 * Since: 1.1.3
 */
gchar *
asc_compute_content_checksum_for_data (const gchar *data, gsize length)
{
#ifdef HAVE_BLAKE3
	blake3_hasher hasher;
	uint8_t out[BLAKE3_OUT_LEN];

	g_return_val_if_fail (data != NULL || length == 0, NULL);

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
 *
 * Returns: (transfer full) (nullable): The generated filename, or %NULL if @url was %NULL.
 *
 * Since: 0.14.6
 */
gchar *
asc_filename_from_url (const gchar *url)
{
	gchar *tmp;
	g_autofree gchar *unescaped = NULL;

	if (url == NULL)
		return NULL;

	/* URLs come straight from (untrusted) metainfo data, so they may well contain
	 * invalid escape sequences and fail to unescape - in that case we just use the
	 * raw string to derive a name from */
	unescaped = g_uri_unescape_string (url, NULL);
	if (unescaped == NULL)
		unescaped = g_strdup (url);

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

/**
 * asc_component_get_source_icon:
 * @cpt: an #AsComponent instance.
 *
 * Find the unprocessed icon that a component was created with, that is the
 * icon reference that came straight from its metainfo or desktop-entry file.
 * A stock icon is preferred, but a local one is accepted as well to
 * accommodate for applications that used the "local" icon type wrong.
 *
 * This is the icon that appstream-compose renders its icon set from. If
 * %ASC_COMPOSE_FLAG_IGNORE_ICONS is set, it is left on the component
 * untouched, so that the caller can run its own icon processing on it.
 *
 * Returns: (transfer none) (nullable): The source #AsIcon, or %NULL if the
 *    component has none.
 *
 * Since: 1.2.0
 */
AsIcon *
asc_component_get_source_icon (AsComponent *cpt)
{
	GPtrArray *icons;
	AsIcon *local_icon = NULL;

	g_return_val_if_fail (AS_IS_COMPONENT (cpt), NULL);

	icons = as_component_get_icons (cpt);
	if (icons == NULL)
		return NULL;

	for (guint i = 0; i < icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK)
			return icon;

		/* we cheat here to accomodate for apps which used the "local" icon type wrong */
		if (as_icon_get_kind (icon) == AS_ICON_KIND_LOCAL)
			local_icon = icon;
	}

	return local_icon;
}
