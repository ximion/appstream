/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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
 * AsDataPoolError:
 * @AS_DATA_POOL_ERROR_FAILED:		Generic failure
 *
 * A metadata pool error.
 **/
typedef enum {
	AS_DATA_POOL_ERROR_FAILED,
	/*< private >*/
	AS_DATA_POOL_ERROR_LAST
} AsDataPoolError;

#define AS_DATA_POOL_ERROR	as_data_pool_error_quark ()
GQuark			as_data_pool_error_quark (void);

AsDataPool		*as_data_pool_new (void);

const gchar 		*as_data_pool_get_locale (AsDataPool *dpool);
void			as_data_pool_set_locale (AsDataPool *dpool,
							const gchar *locale);

gchar			**as_data_pool_get_watched_locations (AsDataPool *dpool);

gboolean		as_data_pool_update (AsDataPool *dpool, GError **error);

GList			*as_data_pool_get_components (AsDataPool *dpool);
AsComponent		*as_data_pool_get_component_by_id (AsDataPool *dpool,
								const gchar *id);

void			as_data_pool_set_data_source_directories (AsDataPool *dpool,
									gchar **dirs);

G_END_DECLS

#endif /* __AS_DATAPOOL_H */
