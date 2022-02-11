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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_POOL_H
#define __AS_POOL_H

#include <glib-object.h>
#include <gio/gio.h>
#include "as-component.h"

G_BEGIN_DECLS

#define AS_TYPE_POOL (as_pool_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsPool, as_pool, AS, POOL, GObject)

struct _AsPoolClass
{
	GObjectClass	parent_class;
	void		(*changed)	(AsPool *pool);

	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
};

/**
 * AsPoolFlags:
 * @AS_POOL_FLAG_NONE:			No flags.
 * @AS_POOL_FLAG_LOAD_OS_COLLECTION:	Load AppStream collection metadata from OS locations.
 * @AS_POOL_FLAG_LOAD_OS_METAINFO:	Load MetaInfo data from OS locations.
 * @AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES:	Load components from desktop-entry files in OS locations.
 * @AS_POOL_FLAG_LOAD_FLATPAK:		Load AppStream collection metadata from Flatpak.
 * @AS_POOL_FLAG_IGNORE_CACHE_AGE:	Load fresh data even if an up-o-date cache is available.
 * @AS_POOL_FLAG_RESOLVE_ADDONS:	Always resolve addons for returned components.
 * @AS_POOL_FLAG_PREFER_OS_METAINFO:	Prefer local metainfo data over the system-provided collection data. Useful for debugging.
 * @AS_POOL_FLAG_MONITOR:		Monitor registered directories for changes, and auto-reload metadata if necessary.
 *
 * Flags controlling the metadata pool behavior.
 **/
typedef enum {
	AS_POOL_FLAG_NONE = 0,
	AS_POOL_FLAG_LOAD_OS_COLLECTION    = 1 << 0,
	AS_POOL_FLAG_LOAD_OS_METAINFO      = 1 << 1,
	AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES = 1 << 2,
	AS_POOL_FLAG_LOAD_FLATPAK          = 1 << 3,
	AS_POOL_FLAG_IGNORE_CACHE_AGE      = 1 << 4,
	AS_POOL_FLAG_RESOLVE_ADDONS        = 1 << 5,
	AS_POOL_FLAG_PREFER_OS_METAINFO    = 1 << 6,
	AS_POOL_FLAG_MONITOR		   = 1 << 7,
} AsPoolFlags;

#define AS_POOL_FLAG_READ_COLLECTION AS_POOL_FLAG_LOAD_OS_COLLECTION
#define AS_POOL_FLAG_READ_METAINFO AS_POOL_FLAG_LOAD_OS_METAINFO
#define AS_POOL_FLAG_READ_DESKTOP_FILES AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES

/**
 * AsCacheFlags:
 * @AS_CACHE_FLAG_NONE:			No flags.
 * @AS_CACHE_FLAG_USE_USER:		Create an user-specific metadata cache.
 * @AS_CACHE_FLAG_USE_SYSTEM:		Use and - if possible - update the system metadata cache.
 * @AS_CACHE_FLAG_NO_CLEAR:		Don't clear the cache when opening it.
 * @AS_CACHE_FLAG_REFRESH_SYSTEM:	Refresh the system cache that is shared between applications.
 *
 * Flags on how caching should be used.
 **/
typedef enum {
	AS_CACHE_FLAG_NONE = 0,
	AS_CACHE_FLAG_USE_USER		= 1 << 0,
	AS_CACHE_FLAG_USE_SYSTEM	= 1 << 1,
	AS_CACHE_FLAG_NO_CLEAR		= 1 << 2,
	AS_CACHE_FLAG_REFRESH_SYSTEM	= 1 << 3,
} AsCacheFlags;

/**
 * AsPoolError:
 * @AS_POOL_ERROR_FAILED:		Generic failure
 * @AS_POOL_ERROR_TARGET_NOT_WRITABLE:	We do not have write-access to the cache target location.
 * @AS_POOL_ERROR_INCOMPLETE:		The pool was loaded, but we had to ignore some metadata.
 * @AS_POOL_ERROR_COLLISION:		An AppStream-ID collision occured (a component with that ID already existed in the pool)
 * @AS_POOL_ERROR_OLD_CACHE:		Some issue with an old on-disk cache occured.
 *
 * A metadata pool error.
 **/
typedef enum {
	AS_POOL_ERROR_FAILED,
	AS_POOL_ERROR_TARGET_NOT_WRITABLE,
	AS_POOL_ERROR_INCOMPLETE,
	AS_POOL_ERROR_COLLISION,
	AS_POOL_ERROR_OLD_CACHE,
	/*< private >*/
	AS_POOL_ERROR_LAST
} AsPoolError;

#define AS_POOL_ERROR	as_pool_error_quark ()
GQuark			as_pool_error_quark (void);

AsPool			*as_pool_new (void);

const gchar 		*as_pool_get_locale (AsPool *pool);
void			as_pool_set_locale (AsPool *pool,
						const gchar *locale);

gboolean		as_pool_load (AsPool *pool,
					GCancellable *cancellable,
					GError **error);
void 			as_pool_load_async (AsPool *pool,
					    GCancellable *cancellable,
					    GAsyncReadyCallback callback,
					    gpointer user_data);
gboolean		as_pool_load_finish (AsPool *pool,
					     GAsyncResult *result,
					     GError **error);

void			as_pool_clear (AsPool *pool);
gboolean		as_pool_add_components (AsPool *pool,
						GPtrArray *cpts,
						GError **error);

GPtrArray		*as_pool_get_components (AsPool *pool);
GPtrArray		*as_pool_get_components_by_id (AsPool *pool,
							const gchar *cid);
GPtrArray		*as_pool_get_components_by_provided_item (AsPool *pool,
								  AsProvidedKind kind,
								  const gchar *item);
GPtrArray		*as_pool_get_components_by_kind (AsPool *pool,
							 AsComponentKind kind);
GPtrArray		*as_pool_get_components_by_categories (AsPool *pool,
								gchar **categories);
GPtrArray		*as_pool_get_components_by_launchable (AsPool *pool,
							       AsLaunchableKind kind,
							       const gchar *id);
GPtrArray		*as_pool_get_components_by_extends (AsPool *pool,
							       const gchar *extended_id);
GPtrArray		*as_pool_search (AsPool *pool,
					 const gchar *search);
gchar			**as_pool_build_search_tokens (AsPool *pool,
						       const gchar *search);

void			as_pool_reset_extra_data_locations (AsPool *pool);
void			as_pool_add_extra_data_location (AsPool *pool,
							 const gchar *directory,
							 AsFormatStyle format_style);

AsPoolFlags		as_pool_get_flags (AsPool *pool);
void			as_pool_set_flags (AsPool *pool,
					   AsPoolFlags flags);
void			as_pool_add_flags (AsPool *pool,
					   AsPoolFlags flags);
void			as_pool_remove_flags (AsPool *pool,
					      AsPoolFlags flags);

void			as_pool_set_load_std_data_locations (AsPool *pool,
							     gboolean enabled);

/* DEPRECATED */

gboolean		as_pool_add_component (AsPool *pool,
						AsComponent *cpt,
						GError **error);

G_DEPRECATED
gboolean		as_pool_load_cache_file (AsPool *pool,
						 const gchar *fname,
						 GError **error);
G_DEPRECATED
gboolean		as_pool_save_cache_file (AsPool *pool,
						 const gchar *fname,
						 GError **error);
G_DEPRECATED
gboolean		as_pool_refresh_cache (AsPool *pool,
						gboolean force,
						GError **error);
G_DEPRECATED
gboolean		as_pool_clear2 (AsPool *pool,
					GError **error);

G_DEPRECATED
const gchar 		*as_pool_get_cache_location (AsPool *pool);
G_DEPRECATED
void			as_pool_set_cache_location (AsPool *pool,
						const gchar *fname);

G_DEPRECATED
AsCacheFlags		as_pool_get_cache_flags (AsPool *pool);
G_DEPRECATED
void			as_pool_set_cache_flags (AsPool *pool,
						      AsCacheFlags flags);

G_DEPRECATED
void			as_pool_clear_metadata_locations (AsPool *pool);
G_DEPRECATED
void			as_pool_add_metadata_location (AsPool *pool,
						       const gchar *directory);

G_END_DECLS

#endif /* __AS_POOL_H */
