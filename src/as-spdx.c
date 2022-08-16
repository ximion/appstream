/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
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

#include "as-spdx.h"

#include <config.h>
#include <string.h>
#include <gio/gio.h>

#include "as-resources.h"
#include "as-utils-private.h"

/**
 * SECTION:as-spdx
 * @short_description: Helper functions to work with SPDX license descriptions.
 * @include: appstream.h
 *
 */

typedef struct {
	gboolean	 last_token_literal;
	GPtrArray	*array;
	GString		*collect;
} AsSpdxHelper;

static gpointer
_g_ptr_array_last (GPtrArray *array)
{
	return g_ptr_array_index (array, array->len - 1);
}

/**
 * as_spdx_license_tokenize_drop:
 *
 * Helper function for as_spdx_license_tokenize().
 */
static void
as_spdx_license_tokenize_drop (AsSpdxHelper *helper)
{
	const gchar *tmp = helper->collect->str;
	guint i;
	g_autofree gchar *last_literal = NULL;
	struct {
		const gchar	*old;
		const gchar	*new;
	} licenses[] =  {
		{ "CC0",	"CC0-1.0" },
		{ "CC-BY",	"CC-BY-3.0" },
		{ "CC-BY-SA",	"CC-BY-SA-3.0" },
		{ "GFDL",	"GFDL-1.3" },
		{ "GPL-2",	"GPL-2.0" },
		{ "GPL-3",	"GPL-3.0" },
		{ "proprietary", "LicenseRef-proprietary" },
		{ NULL, NULL } };

	/* nothing from last time */
	if (helper->collect->len == 0)
		return;

	/* is license or exception enum */
	if (as_is_spdx_license_id (tmp) || as_is_spdx_license_exception_id (tmp)) {
		g_ptr_array_add (helper->array, g_strdup_printf ("@%s", tmp));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is license enum with "+" */
	if (g_str_has_suffix (tmp, "+")) {
		g_autofree gchar *license_id = g_strndup (tmp, strlen (tmp) - 1);
		if (as_is_spdx_license_id (license_id)) {
			g_ptr_array_add (helper->array, g_strdup_printf ("@%s", license_id));
			g_ptr_array_add (helper->array, g_strdup ("+"));
			helper->last_token_literal = FALSE;
			g_string_truncate (helper->collect, 0);
			return;
		}
	}

	/* is old license enum */
	for (i = 0; licenses[i].old != NULL; i++) {
		if (g_strcmp0 (tmp, licenses[i].old) != 0)
			continue;
		g_ptr_array_add (helper->array,
				 g_strdup_printf ("@%s", licenses[i].new));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is conjunctive */
	if (g_strcmp0 (tmp, "and") == 0 || g_strcmp0 (tmp, "AND") == 0) {
		g_ptr_array_add (helper->array, g_strdup ("&"));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is disjunctive */
	if (g_strcmp0 (tmp, "or") == 0 || g_strcmp0 (tmp, "OR") == 0) {
		g_ptr_array_add (helper->array, g_strdup ("|"));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is extension to the license */
	if (g_strcmp0 (tmp, "with") == 0 || g_strcmp0 (tmp, "WITH") == 0) {
		g_ptr_array_add (helper->array, g_strdup ("^"));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is literal */
	if (helper->last_token_literal) {
		last_literal = g_strdup (_g_ptr_array_last (helper->array));
		g_ptr_array_remove_index (helper->array, helper->array->len - 1);
		g_ptr_array_add (helper->array,
				 g_strdup_printf ("%s %s", last_literal, tmp));
	} else {
		g_ptr_array_add (helper->array, g_strdup (tmp));
		helper->last_token_literal = TRUE;
	}
	g_string_truncate (helper->collect, 0);
}

/**
 * as_is_spdx_license_id:
 * @license_id: a single SPDX license ID, e.g. "GPL-3.0"
 *
 * Searches the known list of SPDX license IDs.
 *
 * Returns: %TRUE if the string is a valid SPDX license ID
 *
 * Since: 0.9.8
 **/
gboolean
as_is_spdx_license_id (const gchar *license_id)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;

	/* handle invalid */
	if (license_id == NULL || license_id[0] == '\0')
		return FALSE;

	/* this is used to map non-SPDX licence-ids to legitimate values */
	if (g_str_has_prefix (license_id, "LicenseRef-"))
		return TRUE;

	/* load the readonly data section and look for the license ID */
	data = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream/spdx-license-ids.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", license_id);

	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_is_spdx_license_exception_id:
 * @exception_id: a single SPDX license exception ID, e.g. "GCC-exception-3.1"
 *
 * Searches the known list of SPDX license exception IDs.
 *
 * Returns: %TRUE if the string is a valid SPDX license exception ID
 *
 * Since: 0.12.10
 **/
gboolean
as_is_spdx_license_exception_id (const gchar *exception_id)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;

	/* handle invalid */
	if (exception_id == NULL || exception_id[0] == '\0')
		return FALSE;

	/* load the readonly data section and look for the license exception ID */
	data = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream/spdx-license-exception-ids.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", exception_id);

	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_is_spdx_license_expression:
 * @license: a SPDX license string, e.g. "CC-BY-3.0 and GFDL-1.3"
 *
 * Checks the licence string to check it being a valid licence.
 * NOTE: SPDX licenses can't typically contain brackets.
 *
 * Returns: %TRUE if the icon is a valid "SPDX license"
 *
 * Since: 0.9.8
 **/
gboolean
as_is_spdx_license_expression (const gchar *license)
{
	guint i;
	g_auto(GStrv) tokens = NULL;
	gboolean expect_exception = FALSE;

	/* handle nothing set */
	if (as_is_empty (license))
		return FALSE;

	/* no license information whatsoever */
	if (g_strcmp0 (license, "NONE") == 0)
		return TRUE;

	/* creator has intentionally provided no information */
	if (g_strcmp0 (license, "NOASSERTION") == 0)
		return TRUE;

	tokens = as_spdx_license_tokenize (license);
	if (tokens == NULL)
		return FALSE;

	for (i = 0; tokens[i] != NULL; i++) {
		if (tokens[i][0] == '@') {
			if (expect_exception) {
				expect_exception = FALSE;
				if (as_is_spdx_license_exception_id (tokens[i] + 1))
					continue;
			} else {
				if (as_is_spdx_license_id (tokens[i] + 1))
					continue;
			}
		}
		if (as_is_spdx_license_id (tokens[i]))
			continue;
		if (g_strcmp0 (tokens[i], "&") == 0)
			continue;
		if (g_strcmp0 (tokens[i], "|") == 0)
			continue;
		if (g_strcmp0 (tokens[i], "+") == 0)
			continue;
		if (g_strcmp0 (tokens[i], "^") == 0) {
			expect_exception = TRUE;
			continue;
		}
		return FALSE;
	}

	return TRUE;
}

/**
 * as_utils_spdx_license_3to2:
 *
 * SPDX decided to rename some of the really common license IDs in v3
 * which broke a lot of tools that we cannot really fix now.
 * So we will just convert licenses back to the previous notation where
 * necessary.
 */
static GString*
as_utils_spdx_license_3to2 (const gchar *license3)
{
	GString *license2 = g_string_new (license3);
	as_gstring_replace2 (license2, "-only", "", 1);
	as_gstring_replace2 (license2, "-or-later", "+", 1);
	return license2;
}

/**
 * as_utils_spdx_license_2to3:
 *
 * SPDX decided to rename some of the really common license IDs in v3
 * which broke a lot of tools that we cannot really fix now.
 * So we will convert between notations where necessary.
 */
static GString*
as_utils_spdx_license_2to3 (const gchar *license2)
{
	GString *license3 = g_string_new (license2);
	as_gstring_replace2 (license3, ".0+", ".0-or-later", 1);
	as_gstring_replace2 (license3, ".1+", ".1-or-later", 1);
	return license3;
}

/**
 * as_spdx_license_tokenize:
 * @license: a license string, e.g. "LGPLv2+ and (QPL or GPLv2) and MIT"
 *
 * Tokenizes the SPDX license string (or any simarly formatted string)
 * into parts. Any license parts of the string e.g. "LGPL-2.0+" are prefexed
 * with "@", the conjunctive replaced with "&", the disjunctive replaced
 * with "|" and the WITH operator for license exceptions replaced with "^".
 * Brackets are added as indervidual tokens and other strings are
 * appended into single tokens where possible.
 *
 * Returns: (transfer full) (nullable): array of strings, or %NULL for invalid
 *
 * Since: 0.9.8
 **/
gchar**
as_spdx_license_tokenize (const gchar *license)
{
	AsSpdxHelper helper;
	g_autoptr(GString) license2 = NULL;

	/* handle invalid */
	if (license == NULL)
		return NULL;

	/* SPDX broke the world with v3 */
	license2 = as_utils_spdx_license_3to2 (license);

	helper.last_token_literal = FALSE;
	helper.collect = g_string_new ("");
	helper.array = g_ptr_array_new_with_free_func (g_free);
	for (guint i = 0; i < license2->len; i++) {

		/* handle brackets */
		const gchar tmp = license2->str[i];
		if (tmp == '(' || tmp == ')') {
			as_spdx_license_tokenize_drop (&helper);
			g_ptr_array_add (helper.array, g_strdup_printf ("%c", tmp));
			helper.last_token_literal = FALSE;
			continue;
		}

		/* space, so dump queue */
		if (tmp == ' ') {
			as_spdx_license_tokenize_drop (&helper);
			continue;
		}
		g_string_append_c (helper.collect, tmp);
	}

	/* dump anything remaining */
	as_spdx_license_tokenize_drop (&helper);

	/* return GStrv */
	g_ptr_array_add (helper.array, NULL);
	g_string_free (helper.collect, TRUE);

	return (gchar **) g_ptr_array_free (helper.array, FALSE);
}

/**
 * as_spdx_license_detokenize:
 * @license_tokens: license tokens, typically from as_spdx_license_tokenize()
 *
 * De-tokenizes the SPDX licenses into a string.
 *
 * Returns: (transfer full) (nullable): string, or %NULL for invalid
 *
 * Since: 0.9.8
 **/
gchar*
as_spdx_license_detokenize (gchar **license_tokens)
{
	GString *tmp;
	guint i;

	/* handle invalid */
	if (license_tokens == NULL)
		return NULL;

	tmp = g_string_new ("");
	for (i = 0; license_tokens[i] != NULL; i++) {
		if (g_strcmp0 (license_tokens[i], "&") == 0) {
			g_string_append (tmp, " AND ");
			continue;
		}
		if (g_strcmp0 (license_tokens[i], "|") == 0) {
			g_string_append (tmp, " OR ");
			continue;
		}
		if (g_strcmp0 (license_tokens[i], "^") == 0) {
			g_string_append (tmp, " WITH ");
			continue;
		}
		if (g_strcmp0 (license_tokens[i], "+") == 0) {
			g_string_append (tmp, "+");
			continue;
		}
		if (license_tokens[i][0] != '@') {
			g_string_append (tmp, license_tokens[i]);
			continue;
		}
		g_string_append (tmp, license_tokens[i] + 1);
	}

	return g_string_free (tmp, FALSE);
}

/**
 * as_license_to_spdx_id:
 * @license: a not-quite SPDX license string, e.g. "GPLv3+"
 *
 * Converts a non-SPDX license into an SPDX format string where possible.
 *
 * Returns: the best-effort SPDX license string
 *
 * Since: 0.9.8
 **/
gchar*
as_license_to_spdx_id (const gchar *license)
{
	GString *str;
	guint i;
	guint j;
	guint license_len;
	struct {
		const gchar	*old;
		const gchar	*new;
	} convert[] =  {
		{ " with exceptions",		NULL },
		{ " with advertising",		NULL },
		{ " and ",			" AND " },
		{ " or ",			" OR " },
		{ "AGPLv3+",			"AGPL-3.0" },
		{ "AGPLv3",			"AGPL-3.0" },
		{ "Artistic 2.0",		"Artistic-2.0" },
		{ "Artistic clarified",		"Artistic-2.0" },
		{ "Artistic",			"Artistic-1.0" },
		{ "ASL 1.1",			"Apache-1.1" },
		{ "ASL 2.0",			"Apache-2.0" },
		{ "Boost",			"BSL-1.0" },
		{ "BSD",			"BSD-3-Clause" },
		{ "CC0",			"CC0-1.0" },
		{ "CC-BY-SA",			"CC-BY-SA-3.0" },
		{ "CC-BY",			"CC-BY-3.0" },
		{ "CDDL",			"CDDL-1.0" },
		{ "CeCILL-C",			"CECILL-C" },
		{ "CeCILL",			"CECILL-2.0" },
		{ "CPAL",			"CPAL-1.0" },
		{ "CPL",			"CPL-1.0" },
		{ "EPL",			"EPL-1.0" },
		{ "Free Art",			"ClArtistic" },
		{ "GFDL",			"GFDL-1.3" },
		{ "GPL+",			"GPL-1.0+" },
		{ "GPLv2+",			"GPL-2.0+" },
		{ "GPLv2",			"GPL-2.0" },
		{ "GPLv3+",			"GPL-3.0+" },
		{ "GPLv3",			"GPL-3.0" },
		{ "IBM",			"IPL-1.0" },
		{ "LGPL+",			"LGPL-2.1+" },
		{ "LGPLv2.1",			"LGPL-2.1" },
		{ "LGPLv2+",			"LGPL-2.1+" },
		{ "LGPLv2",			"LGPL-2.1" },
		{ "LGPLv3+",			"LGPL-3.0+" },
		{ "LGPLv3",			"LGPL-3.0" },
		{ "LPPL",			"LPPL-1.3c" },
		{ "MPLv1.0",			"MPL-1.0" },
		{ "MPLv1.1",			"MPL-1.1" },
		{ "MPLv2.0",			"MPL-2.0" },
		{ "Netscape",			"NPL-1.1" },
		{ "OFL",			"OFL-1.1" },
		{ "Python",			"Python-2.0" },
		{ "QPL",			"QPL-1.0" },
		{ "SPL",			"SPL-1.0" },
		{ "UPL",			"UPL-1.0" },
		{ "zlib",			"Zlib" },
		{ "ZPLv2.0",			"ZPL-2.0" },
		{ "Unlicense",			"CC0-1.0" },
		{ "Public Domain",		"LicenseRef-public-domain" },
		{ "SUSE-Public-Domain",		"LicenseRef-public-domain" },
		{ "Copyright only",		"LicenseRef-public-domain" },
		{ "Proprietary",		"LicenseRef-proprietary" },
		{ "Commercial",			"LicenseRef-proprietary" },
		{ NULL, NULL } };

	/* nothing set */
	if (license == NULL)
		return NULL;

	/* already in SPDX format */
	if (as_is_spdx_license_id (license))
		return g_strdup (license);

	/* go through the string looking for case-insensitive matches */
	str = g_string_new ("");
	license_len = strlen (license);
	for (i = 0; i < license_len; i++) {
		gboolean found = FALSE;
		for (j = 0; convert[j].old != NULL; j++) {
			guint old_len = strlen (convert[j].old);
			if (g_ascii_strncasecmp (license + i,
						 convert[j].old,
						 old_len) != 0)
				continue;
			if (convert[j].new != NULL)
				g_string_append (str, convert[j].new);
			i += old_len - 1;
			found = TRUE;
		}
		if (!found)
			g_string_append_c (str, license[i]);
	}

	return g_string_free (str, FALSE);
}

/**
 * as_license_is_metadata_license_id:
 * @license_id: a single SPDX license ID, e.g. "FSFAP"
 *
 * Tests license ID against the vetted list of licenses that
 * can be used for metainfo metadata.
 * This function will not work for license expressions, if you need
 * to test an SPDX license expression for compliance, please
 * use %as_license_is_metadata_license insread.
 *
 * Returns: %TRUE if the string is a valid metadata license ID.
 */
gboolean
as_license_is_metadata_license_id (const gchar *license_id)
{
	if (g_strcmp0 (license_id, "@FSFAP") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@MIT") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@0BSD") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC0-1.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-3.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-4.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-SA-3.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-SA-4.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@GFDL-1.1") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@GFDL-1.2") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@GFDL-1.3") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@BSL-1.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@FTL") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@FSFUL") == 0)
		return TRUE;

	/* any operators are fine */
	if (g_strcmp0 (license_id, "&") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "|") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "+") == 0)
		return TRUE;

	/* if there is any license exception involved, we don't have a content license */
	if (g_strcmp0 (license_id, "^") == 0)
		return FALSE;
	return FALSE;
}

/**
 * as_license_is_metadata_license:
 * @license: The SPDX license string to test.
 *
 * Check if the metadata license is suitable for mixing with other
 * metadata and redistributing the bundled result (this means we
 * prefer permissive licenses here, to not require people shipping
 * catalog metadata to perform a full license review).
 *
 * This method checks against a hardcoded list of permissive licenses
 * commonly used to license metadata under.
 *
 * Returns: %TRUE if the license contains only permissive licenses suitable
 * as metadata license.
 */
gboolean
as_license_is_metadata_license (const gchar *license)
{
	gboolean requires_all_tokens = TRUE;
	guint license_bad_cnt = 0;
	guint license_good_cnt = 0;
	g_auto(GStrv) tokens = NULL;

	tokens = as_spdx_license_tokenize (license);
	/* not a valid SPDX expression */
	if (tokens == NULL)
		return FALSE;

	/* this is too complicated to process */
	for (guint i = 0; tokens[i] != NULL; i++) {
		if (g_strcmp0 (tokens[i], "(") == 0 ||
		    g_strcmp0 (tokens[i], ")") == 0) {
			return FALSE;
		}
	}

	/* this is a simple expression parser and can be easily tricked */
	for (guint i = 0; tokens[i] != NULL; i++) {
		if (g_strcmp0 (tokens[i], "+") == 0)
			continue;
		if (g_strcmp0 (tokens[i], "|") == 0) {
			requires_all_tokens = FALSE;
			continue;
		}
		if (g_strcmp0 (tokens[i], "&") == 0) {
			requires_all_tokens = TRUE;
			continue;
		}
		if (as_license_is_metadata_license_id (tokens[i])) {
			license_good_cnt++;
		} else {
			license_bad_cnt++;
		}
	}

	/* any valid token makes this valid */
	if (!requires_all_tokens && license_good_cnt > 0)
		return TRUE;

	/* all tokens are required to be valid */
	if (requires_all_tokens && license_bad_cnt == 0)
		return TRUE;

	/* looks like the license was bad */
	return FALSE;
}

/**
 * as_get_license_url:
 * @license: The SPDX license ID.
 *
 * Get a web URL to the license text and more license information for an SPDX
 * license identifier.
 *
 * Returns: (transfer full): The license URL.
 *
 * Since: 0.12.7
 */
gchar*
as_get_license_url (const gchar *license)
{
	g_autoptr(GString) license_id = NULL;
	g_autofree gchar *tmp_spdx = NULL;
	g_autofree gchar *license_lower = NULL;

	if (license == NULL)
		return NULL;

	license_id = as_utils_spdx_license_2to3 (license);
	if (g_str_has_prefix (license_id->str, "@"))
		g_string_erase (license_id, 0, 1);
	tmp_spdx = as_license_to_spdx_id (license_id->str);
	g_string_truncate (license_id, 0);
	g_string_append (license_id, tmp_spdx);

	if (g_str_has_prefix (license_id->str, "LicenseRef")) {
		gchar *l;
		/* a license ref may include an URL on its own */
		l = g_strstr_len (license_id->str, -1, "=");
		if (l == NULL)
			return NULL;
		l = l + 1;
		if (l[0] == '\0')
			return NULL;
		return g_strdup (l);
	}
	if (!as_is_spdx_license_id (license_id->str) && !as_is_spdx_license_exception_id (license_id->str))
		return NULL;

	license_lower = g_utf8_strdown (license_id->str, -1);

	/* in the long run, AppStream itself should probably set up a user-focused license information repository,
	 * but in the short term we can link to something pretty close to that, at least for certain popular
	 * open-source licenses
	 * ChooseALicense.com is owned by GitHub, but the information there is easy to read, accurate, and overall
	 * nicer for users to understand than the raw license text on the SPDX website. */
	if (g_str_has_prefix (license_lower, "gpl-3.0"))
		return g_strdup ("https://choosealicense.com/licenses/gpl-3.0/");
	if (g_str_has_prefix (license_lower, "gpl-2.0"))
		return g_strdup ("https://choosealicense.com/licenses/gpl-2.0/");
	if (g_str_has_prefix (license_lower, "lgpl-3.0"))
		return g_strdup ("https://choosealicense.com/licenses/lgpl-3.0/");
	if (g_str_has_prefix (license_lower, "lgpl-2.1"))
		return g_strdup ("https://choosealicense.com/licenses/lgpl-2.1/");
	if (g_str_has_prefix (license_lower, "agpl-3.0"))
		return g_strdup ("https://choosealicense.com/licenses/agpl-3.0/");
	if (g_strcmp0 (license_lower, "mpl-2.0") == 0 || g_strcmp0 (license_lower, "mit") == 0 ||
	    g_strcmp0 (license_lower, "0bsd") == 0 || g_strcmp0 (license_lower, "bsd-2-clause") == 0 ||
	    g_strcmp0 (license_lower, "bsd-3-clause") == 0 || g_strcmp0 (license_lower, "apache-2.0") == 0 ||
	    g_strcmp0 (license_lower, "bsl-1.0") == 0)
		return g_strdup_printf ("https://choosealicense.com/licenses/%s/", license_lower);

	return g_strdup_printf ("https://spdx.org/licenses/%s.html#page", license_id->str);
}

/**
 * as_license_is_free_license:
 * @license: The SPDX license string to test.
 *
 * Check if the given license is for free-as-in-freedom software.
 * A free software license is either approved by the Free Software Foundation
 * or the Open Source Initiative.
 *
 * This function does *not* yet handle complex license expressions with AND and OR.
 * If the expression contains any of these, it will still simply check if all mentioned
 * licenses are Free licenses.
 * Currently, any license exception recognized by SPDX is assumed to not impact the free-ness
 * status of a software component.
 *
 * Please note that this function does not give any legal advice. Please read the license texts
 * to learn more about the individual licenses and their conditions.
 *
 * Returns: %TRUE if the license string contains only free-as-in-freedom licenses.
 *
 * Since: 0.12.10
 */
gboolean
as_license_is_free_license (const gchar *license)
{
	g_auto(GStrv) tokens = NULL;
	g_autoptr(GBytes) rdata = NULL;
	gboolean is_free;

	/* no license at all is "non-free" */
	if (as_is_empty (license))
		return FALSE;
	if (g_strcmp0 (license, "NONE") == 0)
		return FALSE;

	/* load the readonly data section of (free) license IDs */
	rdata = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream/spdx-free-license-ids.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	g_return_val_if_fail (rdata != NULL, FALSE);

	/* assume we have a free software license, unless proven otherwise */
	is_free = TRUE;
	tokens = as_spdx_license_tokenize (license);
	for (guint i = 0; tokens[i] != NULL; i++) {
		g_autofree gchar *lkey = NULL;

		if (g_strcmp0 (tokens[i], "&") == 0 ||
		    g_strcmp0 (tokens[i], "+") == 0 ||
		    g_strcmp0 (tokens[i], "|") == 0 ||
		    g_strcmp0 (tokens[i], "^") == 0 ||
		    g_strcmp0 (tokens[i], "(") == 0 ||
		    g_strcmp0 (tokens[i], ")") == 0)
			continue;

		if (g_str_has_prefix (tokens[i], "@LicenseRef")) {
			/* we only consider license ref's to be free if they explicitly state so */
			if (!g_str_has_prefix (tokens[i], "@LicenseRef-free")) {
				is_free = FALSE;
				break;
			}
		} else if (g_str_has_prefix (tokens[i], "@NOASSERTION")
			|| g_str_has_prefix (tokens[i], "@NONE")) {
			/* no license info is fishy as well */
			is_free = FALSE;
			break;
		}

		if (tokens[i][0] != '@') {
			/* if token has no license-id prefix, consider the license to be non-free */
			is_free = FALSE;
			break;
		}

		if (as_is_spdx_license_exception_id (tokens[i] + 1)) {
			/* for now, we assume any SPDX license exception is still fine and doesn't change the
			 * "free-ness" status of a software component */
			continue;
		}

		lkey = g_strdup_printf ("\n%s\n", tokens[i] + 1);
		if (g_strstr_len (g_bytes_get_data (rdata, NULL), -1, lkey) == NULL) {
			/* the license was not in our "free" list, so we consider it non-free */
			is_free = FALSE;
			break;
		}
	}

	return is_free;
}
