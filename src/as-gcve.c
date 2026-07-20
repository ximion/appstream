/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-gcve.h"

/**
 * SECTION:as-gcve                                                                                                                                                                                                                                                                                                      * SECTION:as-gcve
 * @short_description: Helpers to work with GCVE vulnerability identifiers.
 * @include: appstream.h
 *
 * Helper functions to work with identifiers of the decentralized
 * Global CVE Allocation System (GCVE), and to translate between legacy
 * CVE identifiers and their GCVE representation.
 */

#define AS_GCVE_DB_WEB_URL  "https://db.gcve.eu/vuln/"
#define AS_GCVE_DB_JSON_URL "https://db.gcve.eu/api/vulnerability/"

#define AS_GCVE_ID_PATTERN "GCVE-[0-9]+-[0-9]{4}-[0-9]+"

static const GRegex *
as_gcve_id_regex (void)
{
	static GRegex *re = NULL;
	static gsize initialized = 0;
	if (g_once_init_enter (&initialized)) {
		re = g_regex_new ("^" AS_GCVE_ID_PATTERN "$", G_REGEX_OPTIMIZE, 0, NULL);
		g_once_init_leave (&initialized, 1);
	}
	return re;
}

/**
 * as_gcve_id_word_regex:
 *
 * Get a regular expression to find GCVE identifiers in free-form text.
 *
 * Returns: (transfer none): the compiled #GRegex
 */
const GRegex *
as_gcve_id_word_regex (void)
{
	static GRegex *re = NULL;
	static gsize initialized = 0;
	if (g_once_init_enter (&initialized)) {
		re = g_regex_new ("\\b" AS_GCVE_ID_PATTERN "\\b", G_REGEX_OPTIMIZE, 0, NULL);
		g_once_init_leave (&initialized, 1);
	}
	return re;
}

static const GRegex *
as_gcve_cve_id_regex (void)
{
	static GRegex *re = NULL;
	static gsize initialized = 0;
	if (g_once_init_enter (&initialized)) {
		re = g_regex_new ("^CVE-[0-9]{4}-[0-9]+$", G_REGEX_OPTIMIZE, 0, NULL);
		g_once_init_leave (&initialized, 1);
	}
	return re;
}

/**
 * as_is_gcve_id:
 * @gcve_id: a potential GCVE identifier, e.g. `GCVE-1-2025-0001`
 *
 * Check whether the given string is a valid identifier of the
 * Global CVE Allocation System (GCVE), following the
 * `GCVE-<GNA-ID>-<YEAR>-<UNIQUE-ID>` scheme.
 *
 * Returns: %TRUE if the string is a valid GCVE ID.
 */
gboolean
as_is_gcve_id (const gchar *gcve_id)
{
	if (gcve_id == NULL)
		return FALSE;
	return g_regex_match (as_gcve_id_regex (), gcve_id, 0, NULL);
}

/**
 * as_gcve_id_from_cve:
 * @cve_id: a CVE identifier, e.g. `CVE-2023-40224`
 *
 * Convert a legacy CVE identifier into its GCVE representation
 * (GNA ID `0` is reserved for existing CVE identifiers), so
 * `CVE-2023-40224` becomes `GCVE-0-2023-40224`.
 *
 * Returns: (transfer full) (nullable): The GCVE ID, or %NULL if the input was no valid CVE ID.
 */
gchar *
as_gcve_id_from_cve (const gchar *cve_id)
{
	if (cve_id == NULL)
		return NULL;
	if (!g_regex_match (as_gcve_cve_id_regex (), cve_id, 0, NULL))
		return NULL;

	return g_strdup_printf ("GCVE-0-%s", cve_id + 4);
}

/**
 * as_gcve_id_to_cve:
 * @gcve_id: a GCVE identifier, e.g. `GCVE-0-2023-40224`
 *
 * Convert a GCVE identifier from the reserved GNA `0` namespace for
 * legacy CVE identifiers back into its CVE representation, so
 * `GCVE-0-2023-40224` becomes `CVE-2023-40224`.
 * Only GCVE IDs with GNA ID `0` have a CVE equivalent, %NULL is
 * returned for IDs assigned by any other GCVE Numbering Authority.
 *
 * Returns: (transfer full) (nullable): The CVE ID, or %NULL if no CVE representation exists.
 */
gchar *
as_gcve_id_to_cve (const gchar *gcve_id)
{
	if (gcve_id == NULL)
		return NULL;
	if (!g_str_has_prefix (gcve_id, "GCVE-0-") || !as_is_gcve_id (gcve_id))
		return NULL;

	return g_strdup_printf ("CVE-%s", gcve_id + 7);
}

/**
 * as_get_gcve_url:
 * @id: a GCVE or CVE identifier, e.g. `GCVE-1-2025-0001` or `CVE-2023-40224`
 *
 * Get a web URL to a page with human-readable information about the
 * given vulnerability in the Global CVE Allocation System (GCVE)
 * database.
 * Legacy CVE identifiers are translated into their GCVE representation
 * for this purpose.
 *
 * Returns: (transfer full) (nullable): The web URL, or %NULL if the ID was not valid.
 */
gchar *
as_get_gcve_url (const gchar *id)
{
	g_autofree gchar *gcve_id = NULL;

	if (id == NULL)
		return NULL;

	/* prefer the canonical GCVE form of the ID for the website link */
	gcve_id = as_gcve_id_from_cve (id);
	if (gcve_id != NULL)
		return g_strconcat (AS_GCVE_DB_WEB_URL, gcve_id, NULL);
	if (as_is_gcve_id (id))
		return g_strconcat (AS_GCVE_DB_WEB_URL, id, NULL);

	return NULL;
}

/**
 * as_get_gcve_json_url:
 * @id: a GCVE or CVE identifier, e.g. `GCVE-1-2025-0001` or `CVE-2023-40224`
 *
 * Get a URL to fetch machine-readable JSON data about the given
 * vulnerability from the Global CVE Allocation System (GCVE) database.
 *
 * Returns: (transfer full) (nullable): The JSON data URL, or %NULL if the ID was not valid.
 */
gchar *
as_get_gcve_json_url (const gchar *id)
{
	g_autofree gchar *cve_id = NULL;

	if (id == NULL)
		return NULL;

	/* the JSON API only resolves canonical identifiers, so GCVE IDs from the
	 * reserved GNA 0 namespace must be translated back into their CVE form */
	cve_id = as_gcve_id_to_cve (id);
	if (cve_id != NULL)
		return g_strconcat (AS_GCVE_DB_JSON_URL, cve_id, NULL);
	if (as_is_gcve_id (id) || g_regex_match (as_gcve_cve_id_regex (), id, 0, NULL))
		return g_strconcat (AS_GCVE_DB_JSON_URL, id, NULL);

	return NULL;
}
