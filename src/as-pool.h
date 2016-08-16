/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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
 * @AS_CACHE_FLAG_NONE:		Type invalid or not known
 * @AS_CACHE_FLAG_USE_USER:	Create an user-specific metadata cache.
 * @AS_CACHE_FLAG_USE_SYSTEM:	Use and - if possible - update the global metadata cache.
 *
 * Flags on how caching should be used.
 **/
typedef enum {
	AS_CACHE_FLAG_NONE = 0,
	AS_CACHE_FLAG_USE_USER   = 1 << 0,
	AS_CACHE_FLAG_USE_SYSTEM = 1 << 1,
} AsCacheFlags;

/**
 * AsPoolError:
 * @AS_POOL_ERROR_FAILED:		Generic failure
 * @AS_POOL_ERROR_TARGET_NOT_WRITABLE:	We do not have write-access to the cache target location.
 * @AS_POOL_ERROR_INCOMPLETE:		The pool was loaded, but we had to ignore some metadata.
 * @AS_POOL_ERROR_COLLISION:		An AppStream-ID collision occured (a component with that ID already existed in the pool)
 * @AS_POOL_ERROR_TERM_INVALID:		A search or selection term was invalid.
 *
 * A metadata pool error.
 **/
typedef enum {
	AS_POOL_ERROR_FAILED,
	AS_POOL_ERROR_TARGET_NOT_WRITABLE,
	AS_POOL_ERROR_INCOMPLETE,
	AS_POOL_ERROR_COLLISION,
	AS_POOL_ERROR_TERM_INVALID,
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
gboolean		as_pool_load_metadata (AsPool *pool);

gboolean		as_pool_load_cache_file (AsPool *pool,
						 const gchar *fname,
						 GError **error);
gboolean		as_pool_save_cache_file (AsPool *pool,
						 const gchar *fname,
						 GError **error);

void			as_pool_clear (AsPool *pool);
gboolean		as_pool_add_component (AsPool *pool,
						AsComponent *cpt,
						GError **error);

GPtrArray		*as_pool_get_components (AsPool *pool);
AsComponent		*as_pool_get_component_by_id (AsPool *pool,
								const gchar *id);
GPtrArray		*as_pool_get_components_by_provided_item (AsPool *pool,
								  AsProvidedKind kind,
								  const gchar *item,
								  GError **error);
GPtrArray		*as_pool_get_components_by_kind (AsPool *pool,
							 AsComponentKind kind,
							 GError **error);
GPtrArray		*as_pool_get_components_by_categories (AsPool *pool,
								const gchar *categories);
GPtrArray		*as_pool_search (AsPool *pool,
					 const gchar *search);

GPtrArray		*as_pool_get_metadata_locations (AsPool *pool);
void			as_pool_set_metadata_locations (AsPool *pool,
							gchar **dirs);

AsCacheFlags		as_pool_get_cache_flags (AsPool *pool);
void			as_pool_set_cache_flags (AsPool *pool,
						      AsCacheFlags flags);

gboolean		as_pool_refresh_cache (AsPool *pool,
						gboolean force,
						GError **error);

time_t			as_pool_get_cache_age (AsPool *pool);

G_END_DECLS

#endif /* __AS_POOL_H */
