/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2019 Kalev Lember <klember@redhat.com>
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
 * SECTION:asc-utils-l10n
 * @short_description: Helper functions for extracting localization status information
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-utils-l10n.h"

#include <fnmatch.h>

#include "as-utils-private.h"
#include "asc-globals.h"

typedef struct {
	gchar		*locale;
	guint		 nstrings;
	guint		 percentage;
} AscLocaleEntry;

typedef struct {
	guint		 max_nstrings;
	GList		*data;
	GPtrArray	*translations;	/* no ref */
} AscLocaleContext;

static AscLocaleEntry*
asc_locale_entry_new (void)
{
	AscLocaleEntry *entry;
	entry = g_slice_new0 (AscLocaleEntry);
	return entry;
}

static void
asc_locale_entry_free (AscLocaleEntry *entry)
{
	g_free (entry->locale);
	g_slice_free (AscLocaleEntry, entry);
}

static AscLocaleContext*
asc_locale_ctx_new (void)
{
	AscLocaleContext *ctx;
	ctx = g_new0 (AscLocaleContext, 1);
	ctx->translations = NULL;
	return ctx;
}

static void
asc_locale_ctx_free (AscLocaleContext *ctx)
{
	g_list_free_full (ctx->data, (GDestroyNotify) asc_locale_entry_free);
	g_free (ctx);
}

static void
asc_locale_ctx_add_entry (AscLocaleContext *ctx, AscLocaleEntry *entry)
{
	if (entry->nstrings > ctx->max_nstrings)
		ctx->max_nstrings = entry->nstrings;
	ctx->data = g_list_prepend (ctx->data, entry);
}

static gint
asc_locale_entry_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (((AscLocaleEntry *) a)->locale,
			  ((AscLocaleEntry *) b)->locale);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AscLocaleContext, asc_locale_ctx_free)

typedef struct {
	guint32		 magic;
	guint32		 revision;
	guint32		 nstrings;
	guint32		 orig_tab_offset;
	guint32		 trans_tab_offset;
	guint32		 hash_tab_size;
	guint32		 hash_tab_offset;
	guint32		 n_sysdep_segments;
	guint32		 sysdep_segments_offset;
	guint32		 n_sysdep_strings;
	guint32		 orig_sysdep_tab_offset;
	guint32		 trans_sysdep_tab_offset;
} AscLocaleGettextHeader;

static gboolean
asc_l10n_parse_file_gettext (AscLocaleContext *ctx,
				AscUnit *unit,
				const gchar *locale,
				const gchar *filename,
				GError **error)
{
	AscLocaleEntry *entry;
	AscLocaleGettextHeader h;
	g_autoptr(GBytes) bytes = NULL;
	const gchar *data = NULL;
	gboolean swapped;

	/* read data */
	bytes = asc_unit_read_data (unit, filename, error);
	if (bytes == NULL)
		return FALSE;
	data = g_bytes_get_data (bytes, NULL);

	/* we only strictly need the header */
	memcpy (&h, data, sizeof (AscLocaleGettextHeader));
	if (h.magic == 0x950412de)
		swapped = FALSE;
	else if (h.magic == 0xde120495)
		swapped = TRUE;
	else {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     "Gettext file is invalid");
		return FALSE;
	}
	entry = asc_locale_entry_new ();
	entry->locale = g_strdup (locale);
	if (swapped)
		entry->nstrings = GUINT32_SWAP_LE_BE (h.nstrings);
	else
		entry->nstrings = h.nstrings;
	asc_locale_ctx_add_entry (ctx, entry);
	return TRUE;
}

static gboolean
asc_l10n_search_translations_gettext (AscLocaleContext *ctx,
					AscUnit *unit,
					const gchar *prefix,
					GError **error)
{
	GPtrArray *contents = NULL;
	const gsize prefix_len = strlen (prefix) + 1;

	contents = asc_unit_get_contents (unit);
	for (guint i = 0; i < ctx->translations->len; i++) {
		AsTranslation *t = g_ptr_array_index (ctx->translations, i);
		g_autofree gchar *fn = NULL;
		g_autofree gchar *match_path = NULL;
		if (as_translation_get_kind (t) != AS_TRANSLATION_KIND_GETTEXT &&
			as_translation_get_kind (t) != AS_TRANSLATION_KIND_UNKNOWN)
			continue;

		/* construct expected translation file name pattern */
		fn = g_strdup_printf ("%s.mo", as_translation_get_id (t));
		match_path = g_build_filename (prefix, "share", "locale*", "LC_MESSAGES", fn, NULL);

		/* try to find locale data */
		for (guint j = 0; j < contents->len; j++) {
			const gchar *fname = g_ptr_array_index (contents, j);
			g_auto(GStrv) segments = NULL;
			if (fnmatch (match_path, fname, FNM_NOESCAPE) != 0)
				continue;

			/* fetch locale name from path */
			segments = g_strsplit (fname + prefix_len, G_DIR_SEPARATOR_S, 4);
			if (!asc_l10n_parse_file_gettext (ctx,
							  unit,
							  segments[2],
							  fname,
							  error))
				return FALSE;
		}
	}

	return TRUE;
}

typedef enum {
	ASC_LOCALE_QM_TAG_END		= 0x1,
	/* SourceText16 */
	ASC_LOCALE_QM_TAG_TRANSLATION	= 0x3,
	/* Context16 */
	ASC_LOCALE_QM_TAG_OBSOLETE1	= 0x5,
	ASC_LOCALE_QM_TAG_SOURCE_TEXT	= 0x6,
	ASC_LOCALE_QM_TAG_CONTEXT	= 0x7,
	ASC_LOCALE_QM_TAG_COMMENT	= 0x8
} AscLocaleQmTag;

typedef enum {
	ASC_LOCALE_QM_SECTION_CONTEXTS	= 0x2f,
	ASC_LOCALE_QM_SECTION_HASHES	= 0x42,
	ASC_LOCALE_QM_SECTION_MESSAGES	= 0x69,
	ASC_LOCALE_QM_SECTION_NUMERUS	= 0x88,
	ASC_LOCALE_QM_SECTION_DEPS	= 0x96
} AscLocaleQmSection;

static guint8
_read_uint8 (const guint8 *data, guint32 *offset)
{
	guint8 tmp;
	tmp = data[*offset];
	(*offset) += 1;
	return tmp;
}

static guint32
_read_uint32 (const guint8 *data, guint32 *offset)
{
	guint32 tmp = 0;
	memcpy (&tmp, data + *offset, 4);
	(*offset) += 4;
	return GUINT32_FROM_BE (tmp);
}

static void
asc_l10n_parse_data_qt (AscLocaleContext *ctx,
			const gchar *locale,
			const guint8 *data,
			guint32 len)
{
	AscLocaleEntry *entry;
	guint32 m = 0;
	guint nstrings = 0;

	/* read data */
	while (m < len) {
		guint32 tag_len;
		AscLocaleQmTag tag = _read_uint8(data, &m);
		switch (tag) {
		case ASC_LOCALE_QM_TAG_END:
			break;
		case ASC_LOCALE_QM_TAG_OBSOLETE1:
			m += 4;
			break;
		case ASC_LOCALE_QM_TAG_TRANSLATION:
		case ASC_LOCALE_QM_TAG_SOURCE_TEXT:
		case ASC_LOCALE_QM_TAG_CONTEXT:
		case ASC_LOCALE_QM_TAG_COMMENT:
			tag_len = _read_uint32 (data, &m);
			if (tag_len < 0xffffffff)
				m += tag_len;
			if (tag == ASC_LOCALE_QM_TAG_TRANSLATION)
				nstrings++;
			break;
		default:
			m = G_MAXUINT32;
			break;
		}
	}

	/* add new entry */
	entry = asc_locale_entry_new ();
	entry->locale = g_strdup (locale);
	entry->nstrings = nstrings;
	asc_locale_ctx_add_entry (ctx, entry);
}

static gboolean
asc_l10n_parse_file_qt (AscLocaleContext *ctx,
			AscUnit *unit,
			const gchar *locale,
			const gchar *filename,
			GError **error)
{
	gsize len;
	guint32 m = 0;
	g_autoptr(GBytes) bytes = NULL;
	const guint8 *data = NULL;
	const guint8 qm_magic[] = {
		0x3c, 0xb8, 0x64, 0x18, 0xca, 0xef, 0x9c, 0x95,
		0xcd, 0x21, 0x1c, 0xbf, 0x60, 0xa1, 0xbd, 0xdd
	};

	/* read data */
	bytes = asc_unit_read_data (unit, filename, error);
	if (bytes == NULL)
		return FALSE;
	data = g_bytes_get_data (bytes, &len);

	/* check header */
	if (len < sizeof(qm_magic) ||
	    memcmp (data, qm_magic, sizeof(qm_magic)) != 0) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     "QM translation file is invalid");
		return FALSE;
	}
	m += (guint32) sizeof(qm_magic);

	/* parse each section */
	while (m < len) {
		AscLocaleQmSection section = _read_uint8(data, &m);
		guint32 section_len = _read_uint32 (data, &m);
		if (section_len > len - m) {
			g_set_error_literal (error,
					     ASC_COMPOSE_ERROR,
					     ASC_COMPOSE_ERROR_FAILED,
					     "QM file is invalid, section too large");
			return FALSE;
		}
		if (section == ASC_LOCALE_QM_SECTION_MESSAGES) {
			asc_l10n_parse_data_qt (ctx,
						      locale,
						      data + m,
						      section_len);
		}
		m += section_len;
	}

	return TRUE;
}

static gboolean
asc_l10n_search_translations_qt (AscLocaleContext *ctx,
				 AscUnit *unit,
				 const gchar *prefix,
				 GError **error)
{
	GPtrArray *contents = asc_unit_get_contents (unit);
	const gsize prefix_len = strlen (prefix) + 1;

	/* search for each translation ID */
	for (guint i = 0; i < ctx->translations->len; i++) {
		const gchar *location_hint;
		AsTranslation *t = AS_TRANSLATION (g_ptr_array_index (ctx->translations, i));

		if (as_translation_get_kind (t) != AS_TRANSLATION_KIND_QT &&
		    as_translation_get_kind (t) != AS_TRANSLATION_KIND_UNKNOWN)
			continue;
		if (as_translation_get_id (t) == NULL)
			continue;

		location_hint = as_translation_get_id (t);
		if (g_strstr_len (location_hint, -1, "/") == NULL) {
			/* look in ${prefix}/share/locale/${locale}/LC_MESSAGES/${hint}.mo */
			g_autofree gchar *fn = NULL;
			g_autofree gchar *match_path = NULL;

			fn = g_strdup_printf ("%s.qm", location_hint);
			match_path = g_build_filename (prefix, "share", "locale*", "LC_MESSAGES", fn, NULL);
			for (guint j = 0; j < contents->len; j++) {
				g_auto(GStrv) segments = NULL;
				const gchar *fname = g_ptr_array_index (contents, j);
				if (!g_str_has_suffix (fname, ".qm"))
					continue;
				if (fnmatch (match_path, fname, FNM_NOESCAPE) != 0)
					continue;

				segments = g_strsplit (fname + prefix_len, G_DIR_SEPARATOR_S, 4);
				if (!asc_l10n_parse_file_qt (ctx, unit, segments[2], fname, error))
					return FALSE;
			}
		} else {
			/* look in ${prefix}/share/${hint}_${locale}.qm
			 * and     ${prefix}/share/${hint}/${locale}.qm */

			g_autofree gchar *qm_root = NULL;
			qm_root = g_build_filename (prefix,
							"share",
							location_hint,
							NULL);
			for (guint j = 0; j < contents->len; j++) {
				g_autofree gchar *locale = NULL;
				gchar *tmp;
				const gchar *fname = g_ptr_array_index (contents, j);
				if (!g_str_has_prefix (fname, qm_root))
					continue;
				if (!g_str_has_suffix (fname, ".qm"))
					continue;
				locale = g_strdup (fname + strlen (qm_root) + 1);
				g_strdelimit (locale, ".", '\0');
				tmp = g_strstr_len (locale, -1, "/");
				if (tmp != NULL) {
					/* we have the ${hint}/${locale}.qm form */
					locale = tmp + 1;
				}
				if (!asc_l10n_parse_file_qt (ctx, unit, locale, fname, error))
					return FALSE;
			}
		}

	}

	return TRUE;
}

/**
 * asc_read_translation_status:
 * @cres: an #AscResult
 * @unit: an #AscUnit that the result belongs to
 * @prefix: a prefix to search, e.g. "/usr"
 * @min_percentage: minimum percentage to add language
 *
 * Searches a prefix for languages, and adds <language/>
 * tags to the specified component, if it has one or more <translation/> tags
 * defined to point to the right translation domains and types.
 *
 * @min_percentage sets the minimum percentage to add a language tag.
 * The usual value would be 25% and any language less complete than
 * this will not be added.
 *
 * The purpose of this functionality is to avoid blowing up the size
 * of the AppStream metadata with a lot of extra data detailing
 * languages with very few translated strings.
 **/
void
asc_read_translation_status (AscResult *cres,
			     AscUnit *unit,
			     const gchar *prefix,
			     guint min_percentage)
{
	g_autoptr(GPtrArray) cpts = NULL;

	cpts = asc_result_fetch_components (cres);
	for (guint i = 0; i < cpts->len; i++) {
		AscLocaleEntry *e;
		g_autoptr(AscLocaleContext) ctx = NULL;
		g_autoptr(GError) error = NULL;
		gboolean have_results = FALSE;
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));

		ctx = asc_locale_ctx_new ();
		ctx->translations = as_component_get_translations (cpt);

		/* skip if we have no translation hints */
		if (ctx->translations->len == 0)
			continue;

		/* search for Qt .qm files */
		if (!asc_l10n_search_translations_qt (ctx, unit, prefix, &error)) {
			asc_result_add_hint (cres,
					     cpt,
					     "translation-status-error",
					     "msg",
					     error->message,
					     NULL);
			continue;
		}

		/* search for gettext .mo files */
		if (!asc_l10n_search_translations_gettext (ctx, unit, prefix, &error)) {
			asc_result_add_hint (cres,
					     cpt,
					     "translation-status-error",
					     "msg",
					     error->message,
					     NULL);
			continue;
		}

		/* calculate percentages */
		for (GList *l = ctx->data; l != NULL; l = l->next) {
			e = l->data;
			e->percentage = MIN (e->nstrings * 100 / ctx->max_nstrings, 100);
		}

		/* sort */
		ctx->data = g_list_sort (ctx->data, asc_locale_entry_sort_cb);

		/* add results */
		for (GList *l = ctx->data; l != NULL; l = l->next) {
			e = l->data;
			have_results = TRUE;
			if (e->percentage < min_percentage)
				continue;
			as_component_add_language (cpt, e->locale, (gint) e->percentage);
		}

		if (!have_results)
			asc_result_add_hint_simple (cres, cpt, "translations-not-found");

		/* Add a fake entry for the source locale. Do so after checking
		 * !have_results since the source locale is always guaranteed
		 * to exist, so would break that check.
		 * as_component_add_language() will deduplicate in case that’s
		 * needed. */
		for (guint i = 0; i < ctx->translations->len; i++) {
			AsTranslation *t = g_ptr_array_index (ctx->translations, i);

			as_component_add_language (cpt,
						   as_translation_get_source_locale (t),
						   100);
		}

		/* remove translation elements, they should no longer be in the resulting component */
		g_ptr_array_set_size (ctx->translations, 0);
	}
}

