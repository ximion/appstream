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

#ifndef __AS_UTILS_PRIVATE_H
#define __AS_UTILS_PRIVATE_H

#include <glib-object.h>
#include "as-macros-private.h"
#include "as-utils.h"

AS_BEGIN_PRIVATE_DECLS

/* component data-ID constants */
#define AS_DATA_ID_WILDCARD    "*"
#define AS_DATA_ID_PARTS_COUNT 5

gchar *as_get_current_locale_posix (void);
gchar *as_get_current_locale_bcp47 (void);

AS_INTERNAL_VISIBLE
gboolean as_locale_is_posix (const gchar *locale);
AS_INTERNAL_VISIBLE
gboolean as_locale_is_bcp47 (const gchar *locale);

AS_INTERNAL_VISIBLE
gboolean   as_is_empty (const gchar *str);
GDateTime *as_iso8601_to_datetime (const gchar *iso_date);

AS_INTERNAL_VISIBLE
gboolean as_str_verify_integer (const gchar *str,
				gint64	     min_value,
				gint64	     max_value);

AS_INTERNAL_VISIBLE
gboolean as_path_segment_verify (const gchar *str);
AS_INTERNAL_VISIBLE
gchar *as_path_segment_sanitize (const gchar *str);

AS_INTERNAL_VISIBLE
gboolean as_utils_delete_dir_recursive (const gchar *dirname);

AS_INTERNAL_VISIBLE
gchar *as_xml_file_get_root_name (const gchar *fname,
				  GError     **error);

AS_INTERNAL_VISIBLE
GPtrArray *as_utils_find_files_matching (const gchar *dir,
					 const gchar *pattern,
					 gboolean     recursive,
					 GError	    **error);
AS_INTERNAL_VISIBLE
GPtrArray *as_utils_find_files (const gchar *dir,
				gboolean     recursive,
				GError	   **error);

AS_INTERNAL_VISIBLE
gboolean as_utils_is_root (void);

AS_INTERNAL_VISIBLE
gboolean as_utils_is_writable (const gchar *path);

AS_INTERNAL_VISIBLE
gchar	    *as_str_replace (const gchar *str,
			     const gchar *old_str,
			     const gchar *new_str,
			     guint	  limit);

gchar	   **as_ptr_array_to_strv (GPtrArray *array);
GPtrArray   *as_strv_to_ptr_array (gchar  **strv,
				   gboolean ignore_empty,
				   gboolean copy);
const gchar *as_ptr_array_find_string (GPtrArray   *array,
				       const gchar *value);
void	     as_hash_table_string_keys_to_array (GHashTable *table,
						 GPtrArray  *array);

gboolean     as_touch_location (const gchar *fname);

AS_INTERNAL_VISIBLE
void as_reset_umask (void);

AS_INTERNAL_VISIBLE
gboolean as_copy_file (const gchar *source,
		       const gchar *destination,
		       GError	  **error);

gboolean as_is_cruft_locale (const gchar *locale);

AS_INTERNAL_VISIBLE
gchar *as_locale_strip_encoding (const gchar *locale);

gchar *as_utils_locale_to_language (const gchar *locale);

/**
 * AS_LOCALE_BUF_SIZE:
 * Locale buffer size for allocation-free paths.
 * Large enough for the canonical form of any locale we will ever see.
 */
#define AS_LOCALE_BUF_SIZE 64

AS_INTERNAL_VISIBLE
gboolean as_utils_locale_canonicalize_buf (const gchar *locale,
					   gchar       *dest,
					   gsize	dest_size);
AS_INTERNAL_VISIBLE
gchar *as_utils_locale_canonicalize (const gchar *locale);
void   as_utils_locale_canonicalize_inplace (gchar *locale);

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
 further tests on them.
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
gboolean     as_locale_chain_contains (const AsLocaleChain *chain,
				       const gchar	   *canonical_locale);

gchar	    *as_get_current_arch (void);
gboolean     as_arch_compatible (const gchar *arch1,
				 const gchar *arch2);

gboolean     as_utils_search_token_valid (const gchar *token);

AsBundleKind as_utils_get_component_bundle_kind (AsComponent *cpt);
gchar	    *as_utils_build_data_id_for_cpt (AsComponent *cpt);

AS_INTERNAL_VISIBLE
gchar *as_utils_dns_to_rdns (const gchar *url,
			     const gchar *suffix);

void   as_sort_components_by_score (GPtrArray *cpts);

void   as_object_ptr_array_absorb (GPtrArray *dest,
				   GPtrArray *src);

AS_INTERNAL_VISIBLE
gchar *as_ptr_array_to_str (GPtrArray	*array,
			    const gchar *separator);

AS_INTERNAL_VISIBLE
gchar *as_filebasename_from_uri (const gchar *uri);

AS_INTERNAL_VISIBLE
gchar *as_strstripnl (gchar *string);

AS_INTERNAL_VISIBLE
void as_ref_string_release (GRefString *rstr);
AS_INTERNAL_VISIBLE
void as_ref_string_assign_safe (GRefString **rstr_ptr,
				const gchar *str);

void as_ref_string_assign_transfer (GRefString **rstr_ptr,
				    GRefString	*new_rstr);

AS_INTERNAL_VISIBLE
gboolean as_utils_extract_tarball (const gchar *filename,
				   const gchar *target_dir,
				   GError     **error);

gboolean as_utils_category_name_is_bad (const gchar *category_name);

gboolean as_utils_is_platform_triplet_arch (const gchar *arch);
gboolean as_utils_is_platform_triplet_oskernel (const gchar *os);
gboolean as_utils_is_platform_triplet_osenv (const gchar *env);

gboolean as_utils_is_reference_registry (const gchar *regname);

gchar	*as_get_user_cache_dir (GError **error);

gboolean as_unichar_accepted (gunichar c);

AS_INTERNAL_VISIBLE
gchar *as_sanitize_text_spaces (const gchar *text);

AS_INTERNAL_VISIBLE
gchar *as_random_alnum_string (gssize len);

gchar *as_utils_find_stock_icon_filename_full (const gchar *root_dir,
					       const gchar *icon_name,
					       guint	    icon_size,
					       guint	    icon_scale,
					       GError	  **error);

gchar *as_make_usertag_key (const gchar *ns,
			    const gchar *tag);

AS_END_PRIVATE_DECLS

#endif /* __AS_UTILS_PRIVATE_H */
