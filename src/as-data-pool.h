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

#ifndef __AS_DATAPOOL_H
#define __AS_DATAPOOL_H

#include <glib-object.h>
#include <gio/gio.h>
#include "as-component.h"

G_BEGIN_DECLS

#define AS_TYPE_DATA_POOL (as_data_pool_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsDataPool, as_data_pool, AS, DATA_POOL, GObject)

struct _AsDataPoolClass
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
 * AsDataPoolError:
 * @AS_DATA_POOL_ERROR_FAILED:			Generic failure
 * @AS_DATA_POOL_ERROR_TARGET_NOT_WRITABLE:	We do not have write-access to the cache target location.
 * @AS_DATA_POOL_ERROR_INCOMPLETE:		The pool was loaded, but we had to ignore some metadata.
 *
 * A metadata pool error.
 **/
typedef enum {
	AS_DATA_POOL_ERROR_FAILED,
	AS_DATA_POOL_ERROR_TARGET_NOT_WRITABLE,
	AS_DATA_POOL_ERROR_INCOMPLETE,
	AS_DATA_POOL_ERROR_TERM_INVALID,
	/*< private >*/
	AS_DATA_POOL_ERROR_LAST
} AsDataPoolError;

#define AS_DATA_POOL_ERROR	as_data_pool_error_quark ()
GQuark			as_data_pool_error_quark (void);

AsDataPool		*as_data_pool_new (void);

const gchar 		*as_data_pool_get_locale (AsDataPool *dpool);
void			as_data_pool_set_locale (AsDataPool *dpool,
							const gchar *locale);

gboolean		as_data_pool_load (AsDataPool *dpool,
					   GCancellable *cancellable,
					   GError **error);
gboolean		as_data_pool_load_metadata (AsDataPool *dpool);

gboolean		as_data_pool_load_cache_file (AsDataPool *dpool,
						      const gchar *fname,
						      GError **error);
gboolean		as_data_pool_save_cache_file (AsDataPool *dpool,
						      const gchar *fname,
						      GError **error);

void			as_data_pool_clear (AsDataPool *dpool);
gboolean		as_data_pool_add_component (AsDataPool *dpool,
							AsComponent *cpt,
							GError **error);

GPtrArray		*as_data_pool_get_components (AsDataPool *dpool);
AsComponent		*as_data_pool_get_component_by_id (AsDataPool *dpool,
								const gchar *id);
GPtrArray		*as_data_pool_get_components_by_provided_item (AsDataPool *dpool,
								 AsProvidedKind kind,
								 const gchar *item,
								 GError **error);
GPtrArray		*as_data_pool_get_components_by_kind (AsDataPool *dpool,
								AsComponentKind kind,
								GError **error);
GPtrArray		*as_data_pool_search (AsDataPool *dpool,
					      const gchar *term);

GPtrArray		*as_data_pool_get_metadata_locations (AsDataPool *dpool);
void			as_data_pool_set_metadata_locations (AsDataPool *dpool,
								gchar **dirs);

AsCacheFlags		as_data_pool_get_cache_flags (AsDataPool *dpool);
void			as_data_pool_set_cache_flags (AsDataPool *dpool,
						      AsCacheFlags flags);

gboolean		as_data_pool_refresh_cache (AsDataPool *dpool,
						    gboolean force,
						    GError **error);

G_GNUC_INTERNAL
time_t			as_data_pool_get_cache_age (AsDataPool *dpool);

G_GNUC_DEPRECATED
gboolean		as_data_pool_update (AsDataPool *dpool, GError **error);

G_END_DECLS

#endif /* __AS_DATAPOOL_H */
