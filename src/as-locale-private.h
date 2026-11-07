/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2026 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_LOCALE_PRIVATE_H
#define __AS_LOCALE_PRIVATE_H

#include <glib.h>
#include "as-macros-private.h"
#include "as-locale.h"

AS_BEGIN_PRIVATE_DECLS

gchar *as_locale_get_current_posix (void);
gchar *as_locale_get_current_bcp47 (void);

AS_INTERNAL_VISIBLE
gboolean as_locale_is_posix (const gchar *locale);
AS_INTERNAL_VISIBLE
gboolean as_locale_is_bcp47 (const gchar *locale);

gboolean as_locale_is_cruft (const gchar *locale);

AS_INTERNAL_VISIBLE
gchar *as_locale_strip_encoding (const gchar *locale);

gchar *as_locale_to_language (const gchar *locale);

/**
 * AS_LOCALE_BUF_SIZE:
 * Locale buffer size for allocation-free paths.
 * Large enough for the canonical form of any locale we will ever see.
 */
#define AS_LOCALE_BUF_SIZE 64

AS_INTERNAL_VISIBLE
gboolean as_locale_canonicalize_buf (const gchar *locale,
				     gchar	 *dest,
				     gsize	  dest_size);
AS_INTERNAL_VISIBLE
gchar *as_locale_canonicalize (const gchar *locale);
void   as_locale_canonicalize_inplace (gchar *locale);

static inline gboolean
as_locale_is_section_end_char (gchar c)
{
	return c == '\0' || c == '-' || c == '_' || c == '.' || c == '@';
}

/**
 * as_locale_same_language:
 * @locale1: a POSIX or BCP47 locale, not necessarily canonical
 * @locale2: a POSIX or BCP47 locale, not necessarily canonical
 *
 * This is called for nearly every translation we read, so it is a cheap
 * check to ensure that locales might be compatible, before doing any
 * further tests on them.
 *
 * Returns: %TRUE if both locales have the same language subtag.
 */
static inline gboolean
as_locale_same_language (const gchar *locale1,
			 const gchar *locale2)
{
	for (;; locale1++, locale2++) {
		gboolean end1 = as_locale_is_section_end_char (*locale1);
		gboolean end2 = as_locale_is_section_end_char (*locale2);

		if (end1 || end2)
			return end1 && end2;
		if (g_ascii_tolower (*locale1) != g_ascii_tolower (*locale2))
			return FALSE;
	}
}

#define AS_LOCALE_CHAIN_MAX_ENTRIES 8
#define AS_LOCALE_CHAIN_BUF_SIZE    192

/**
 * AsLocaleChain:
 * @n_entries: Number of locales in the chain.
 * @n_exact: Number of locales at the start of the chain that are just other ways
 *	     to write the first one.
 *
 * The locales whose translations can be shown for a locale, best match first,
 * all in canonical BCP47 form. Use as_locale_chain_get() to access them.
 */
typedef struct {
	guint8 n_entries;
	guint8 n_exact;
	guint8 buf_used;
	guint8 offsets[AS_LOCALE_CHAIN_MAX_ENTRIES];
	gchar  buf[AS_LOCALE_CHAIN_BUF_SIZE];
} AsLocaleChain;

#define as_locale_chain_get(chain, idx) ((const gchar *) ((chain)->buf + (chain)->offsets[idx]))

AS_INTERNAL_VISIBLE
void as_locale_chain_init (AsLocaleChain *chain,
			   const gchar	 *canonical_locale);
AS_INTERNAL_VISIBLE
gboolean as_locale_chain_contains (const AsLocaleChain *chain,
				   const gchar	       *canonical_locale);

AS_END_PRIVATE_DECLS

#endif /* __AS_LOCALE_PRIVATE_H */
