/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2021 Matthias Klumpp <matthias@tenstral.net>
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
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

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
 * AsPoolFlags:
 * @AS_POOL_FLAG_NONE:			No flags.
 * @AS_POOL_FLAG_READ_COLLECTION:	Add AppStream collection metadata to the pool.
 * @AS_POOL_FLAG_READ_METAINFO:		Add data from AppStream metainfo files to the pool.
 * @AS_POOL_FLAG_READ_DESKTOP_FILES:	Add metadata from .desktop files to the pool.
 *
 * Flags on how caching should be used.
 **/
typedef enum {
	AS_POOL_FLAG_NONE = 0,
	AS_POOL_FLAG_READ_COLLECTION    = 1 << 0,
	AS_POOL_FLAG_READ_METAINFO      = 1 << 1,
	AS_POOL_FLAG_READ_DESKTOP_FILES = 1 << 2,
} AsPoolFlags;

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

gboolean		as_pool_clear2 (AsPool *pool,
					GError **error);
gboolean		as_pool_add_component (AsPool *pool,
						AsComponent *cpt,
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
GPtrArray		*as_pool_search (AsPool *pool,
					 const gchar *search);
gchar			**as_pool_build_search_tokens (AsPool *pool,
						       const gchar *search);

void			as_pool_clear_metadata_locations (AsPool *pool);
void			as_pool_add_metadata_location (AsPool *pool,
						       const gchar *directory);

AsCacheFlags		as_pool_get_cache_flags (AsPool *pool);
void			as_pool_set_cache_flags (AsPool *pool,
						      AsCacheFlags flags);

AsPoolFlags		as_pool_get_flags (AsPool *pool);
void			as_pool_set_flags (AsPool *pool,
						AsPoolFlags flags);

const gchar 		*as_pool_get_cache_location (AsPool *pool);
void			as_pool_set_cache_location (AsPool *pool,
						const gchar *fname);

/* DEPRECATED */

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
void			as_pool_clear (AsPool *pool);

G_END_DECLS

#endif /* __AS_POOL_H */
