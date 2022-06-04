/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-settings-private.h"
#include "as-utils.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

/* component data-ID constants */
#define AS_DATA_ID_WILDCARD	"*"
#define	AS_DATA_ID_PARTS_COUNT	5


/**
 * as_assign_string_safe:
 * @target: target variable variable to assign string to
 * @new_val: the value to set the target variable to
 *
 * Assigns @new_val to @target, freeing the previous content of
 * @target, unless both variables have been identical.
 *
 * This is useful in setter functions for class members, to ensure
 * we do not accidentally free a memory region that is still in use.
 */
#define as_assign_string_safe(target, new_val) \
  G_STMT_START { \
    if (G_LIKELY ((target) != (new_val))) { \
	g_free (target); \
	target = g_strdup (new_val); \
      } \
  } G_STMT_END

/**
 * as_assign_ptr_array_safe:
 * @target: target variable variable to assign #GPtrArray to
 * @new_ptrarray: the value to set the target variable to
 *
 * Assigns @new_ptrarray to @target, decreasing the reference count of
 * @target, unless both variables are already identical.
 *
 * This is useful in setter functions for class members, to ensure
 * we do not accidentally free a memory region that is still in use.
 */
#define as_assign_ptr_array_safe(target, new_ptrarray) \
  G_STMT_START { \
    if (G_LIKELY ((target) != (new_ptrarray))) { \
	g_ptr_array_unref (target); \
	target = g_ptr_array_ref (new_ptrarray); \
      } \
  } G_STMT_END

/**
 * AsMarkupKind:
 * @AS_MARKUP_KIND_UNKNOWN:	Unknown markup.
 * @AS_MARKUP_KIND_XML:		XML markup.
 * @AS_MARKUP_KIND_YAML:	YAML markup.
 * @AS_MARKUP_KIND_TEXT:	Simple text.
 * @AS_MARKUP_KIND_MARKDOWN:	Markdown
 *
 * Markup types.
 **/
typedef enum {
	AS_MARKUP_KIND_UNKNOWN,
	AS_MARKUP_KIND_XML,
	AS_MARKUP_KIND_YAML,
	AS_MARKUP_KIND_TEXT,
	AS_MARKUP_KIND_MARKDOWN,
	/*< private >*/
	AS_MARKUP_KIND_LAST
} AsMarkupKind;

gchar			*as_description_markup_convert (const gchar *markup,
							AsMarkupKind to_kind,
							GError **error);

gchar			*as_get_current_locale (void);

AS_INTERNAL_VISIBLE
gboolean		as_is_empty (const gchar *str);
GDateTime		*as_iso8601_to_datetime (const gchar *iso_date);

AS_INTERNAL_VISIBLE
gboolean		as_str_verify_integer (const gchar *str,
						gint64 min_value,
						gint64 max_value);

AS_INTERNAL_VISIBLE
gboolean		as_utils_delete_dir_recursive (const gchar* dirname);

AS_INTERNAL_VISIBLE
GPtrArray		*as_utils_find_files_matching (const gchar *dir,
							const gchar *pattern,
							gboolean recursive,
							GError **error);
AS_INTERNAL_VISIBLE
GPtrArray		*as_utils_find_files (const gchar *dir,
						gboolean recursive,
						GError **error);

AS_INTERNAL_VISIBLE
gboolean		as_utils_is_root (void);

AS_INTERNAL_VISIBLE
gboolean		as_utils_is_writable (const gchar *path);

AS_INTERNAL_VISIBLE
gchar			*as_str_replace (const gchar *str,
					 const gchar *old_str,
					 const gchar *new_str,
					 guint limit);

gchar			**as_ptr_array_to_strv (GPtrArray *array);
const gchar		*as_ptr_array_find_string (GPtrArray *array,
						   const gchar *value);
void			as_hash_table_string_keys_to_array (GHashTable *table,
							    GPtrArray *array);

gboolean		as_touch_location (const gchar *fname);

AS_INTERNAL_VISIBLE
void			as_reset_umask (void);

AS_INTERNAL_VISIBLE
gboolean		as_copy_file (const gchar *source, const gchar *destination, GError **error);

gboolean		as_is_cruft_locale (const gchar *locale);

AS_INTERNAL_VISIBLE
gchar			*as_locale_strip_encoding (const gchar *locale);

gchar			*as_utils_locale_to_language (const gchar *locale);

gchar			*as_get_current_arch (void);
gboolean		as_arch_compatible (const gchar *arch1,
					    const gchar *arch2);

gboolean		as_utils_search_token_valid (const gchar *token);

AsBundleKind		as_utils_get_component_bundle_kind (AsComponent *cpt);
gchar			*as_utils_build_data_id_for_cpt (AsComponent *cpt);

AS_INTERNAL_VISIBLE
gchar			*as_utils_dns_to_rdns (const gchar *url,
					       const gchar *suffix);

void			as_sort_components_by_score (GPtrArray *cpts);

void			as_object_ptr_array_absorb (GPtrArray *dest,
						    GPtrArray *src);

AS_INTERNAL_VISIBLE
gchar			*as_ptr_array_to_str (GPtrArray *array,
						const gchar *separator);

AS_INTERNAL_VISIBLE
gchar			*as_filebasename_from_uri (const gchar *uri);

AS_INTERNAL_VISIBLE
gchar			*as_strstripnl (gchar *string);

AS_INTERNAL_VISIBLE
void			as_ref_string_release (GRefString *rstr);
AS_INTERNAL_VISIBLE
void			as_ref_string_assign_safe (GRefString **rstr_ptr,
						   const gchar *str);

void			as_ref_string_assign_transfer (GRefString **rstr_ptr,
						       GRefString *new_rstr);

gboolean		as_utils_extract_tarball (const gchar *filename,
						  const gchar *target_dir,
						  GError **error);

gboolean		as_utils_is_platform_triplet_arch (const gchar *arch);
gboolean		as_utils_is_platform_triplet_oskernel (const gchar *os);
gboolean		as_utils_is_platform_triplet_osenv (const gchar *env);

gchar			*as_get_user_cache_dir (GError **error);

gboolean		as_unichar_accepted (gunichar c);

AS_INTERNAL_VISIBLE
gchar			*as_sanitize_text_spaces (const gchar *text);

AS_INTERNAL_VISIBLE
gchar			*as_random_alnum_string (gssize len);

gchar			*as_utils_find_stock_icon_filename_full (const gchar *root_dir,
								 const gchar *icon_name,
								 guint icon_size,
								 guint icon_scale,
								 GError **error);
AS_INTERNAL_VISIBLE
void			as_utils_ensure_resources ();

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_UTILS_PRIVATE_H */
