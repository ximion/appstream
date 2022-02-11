/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2022 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_CACHE_H
#define __AS_CACHE_H

#include <glib-object.h>
#include "as-component.h"

G_BEGIN_DECLS

#define AS_TYPE_CACHE (as_cache_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsCache, as_cache, AS, CACHE, GObject)

struct _AsCacheClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * AsCacheScope:
 * @AS_CACHE_SCOPE_UNKNOWN: Unknown scope, or scope is not relevant.
 * @AS_CACHE_SCOPE_SYSTEM: System-wide cache, always considered unwritable.
 * @AS_CACHE_SCOPE_WRITABLE: User-specific cache, always writable.
 *
 * Scope of the cache.
 **/
typedef enum {
	AS_CACHE_SCOPE_UNKNOWN,
	AS_CACHE_SCOPE_SYSTEM,
	AS_CACHE_SCOPE_WRITABLE
} AsCacheScope;

/**
 * AsCacheDataRefineFn:
 * @cpt: (not nullable): The component that should be refined.
 * @is_serialization: %TRUE on serialization to the cache.
 * @user_data: Additional data.
 *
 * Helper function called by #AsCache to refine components and add additional
 * data to them, transform data to be suitable for storing or unpack it once
 * the component is loaded from cache again.
 */
typedef void (*AsCacheDataRefineFn)(AsComponent *cpt,
				    gboolean is_serialization,
				    gpointer user_data);

/**
 * AsCacheError:
 * @AS_CACHE_ERROR_FAILED:		Generic failure
 * @AS_CACHE_ERROR_PERMISSIONS:		Some permissions are missing, e.g. a file may not be writable
 * @AS_CACHE_ERROR_BAD_VALUE:		Some value, possibly user-defined, was invalid.
 *
 * A metadata cache error.
 **/
typedef enum {
	AS_CACHE_ERROR_FAILED,
	AS_CACHE_ERROR_PERMISSIONS,
	AS_CACHE_ERROR_BAD_VALUE,
	/*< private >*/
	AS_CACHE_ERROR_LAST
} AsCacheError;

#define AS_CACHE_ERROR	as_cache_error_quark ()
GQuark			as_cache_error_quark (void);

AsCache			*as_cache_new (void);

const gchar		*as_cache_get_locale (AsCache *cache);
void			as_cache_set_locale (AsCache *cache,
					      const gchar *locale);

void			as_cache_set_locations (AsCache *cache,
						const gchar *system_cache_dir,
						const gchar *user_cache_dir);

gboolean		as_cache_get_prefer_os_metainfo (AsCache *cache);
void			as_cache_set_prefer_os_metainfo (AsCache *cache,
							 gboolean prefer_os_metainfo);

void			as_cache_set_resolve_addons (AsCache *cache,
							 gboolean resolve_addons);

void			as_cache_prune_data (AsCache *cache);

void			as_cache_clear (AsCache *cache);

gboolean		as_cache_set_contents_for_section (AsCache *cache,
							   AsComponentScope scope,
							   AsFormatStyle source_format_style,
							   gboolean is_os_data,
							   GPtrArray *cpts,
							   const gchar *cache_key,
							   gpointer refinefn_user_data,
							   GError **error);
gboolean		as_cache_set_contents_for_path (AsCache *cache,
							GPtrArray *cpts,
							const gchar *filename,
							gpointer refinefn_user_data,
							GError **error);

time_t			as_cache_get_ctime (AsCache *cache,
					    AsComponentScope scope,
					    const gchar *cache_key,
					    AsCacheScope *cache_scope);

void			as_cache_load_section_for_key (AsCache *cache,
							AsComponentScope scope,
							AsFormatStyle source_format_style,
							gboolean is_os_data,
							const gchar *cache_key,
							gboolean *is_outdated,
						       gpointer refinefn_user_data);
void			as_cache_load_section_for_path (AsCache *cache,
							const gchar *filename,
							gboolean *is_outdated,
							gpointer refinefn_user_data);

void			as_cache_mask_by_data_id (AsCache *cache,
						  const gchar *cdid);
gboolean		as_cache_add_masking_components (AsCache *cache,
							 GPtrArray *cpts,
							 GError **error);

void			as_cache_set_refine_func (AsCache *cache,
						  AsCacheDataRefineFn func);

GPtrArray		*as_cache_get_components_all (AsCache *cache,
							GError **error);

GPtrArray		*as_cache_get_components_by_id (AsCache *cache,
							const gchar *id,
							GError **error);

GPtrArray		*as_cache_get_components_by_extends (AsCache *cache,
							     const gchar *extends_id,
							     GError **error);

GPtrArray		*as_cache_get_components_by_kind (AsCache *cache,
							  AsComponentKind kind,
							  GError **error);

GPtrArray		*as_cache_get_components_by_provided_item (AsCache *cache,
								   AsProvidedKind kind,
								   const gchar *item,
								   GError **error);

GPtrArray		*as_cache_get_components_by_categories (AsCache *cache,
								gchar **categories,
								GError **error);

GPtrArray		*as_cache_get_components_by_launchable (AsCache *cache,
								AsLaunchableKind kind,
								const gchar *id,
								GError **error);

GPtrArray		*as_cache_search (AsCache *cache,
					  const gchar * const *terms,
					  gboolean sort,
					  GError **error);

G_END_DECLS

#endif /* __AS_CACHE_H */
