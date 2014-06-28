/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#define AS_TYPE_DATA_POOL		(as_data_pool_get_type())
#define AS_DATA_POOL(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_DATA_POOL, AsDataPool))
#define AS_DATA_POOL_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_DATA_POOL, AsDataPoolClass))
#define AS_IS_DATA_POOL(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_DATA_POOL))
#define AS_IS_DATA_POOL_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_DATA_POOL))
#define AS_DATA_POOL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_DATA_POOL, AsDataPoolClass))

G_BEGIN_DECLS

typedef struct _AsDataPool		AsDataPool;
typedef struct _AsDataPoolClass	AsDataPoolClass;

struct _AsDataPool
{
	GObject parent;
};

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
	void (*_as_reserved7) (void);
	void (*_as_reserved8) (void);
};

GType		 	as_data_pool_get_type (void);
AsDataPool		*as_data_pool_new (void);

void			as_data_pool_initialize (AsDataPool *dpool);
gchar**			as_data_pool_get_watched_locations (AsDataPool *dpool);

gboolean		as_data_pool_update (AsDataPool *dpool);
GList*			as_data_pool_get_components (AsDataPool *dpool);

void			as_data_pool_set_xml_paths (AsDataPool *dpool,
											gchar** values);
void			as_data_pool_set_dep11_paths (AsDataPool *dpool,
											gchar** values);
void			as_data_pool_set_appinstall_paths (AsDataPool *dpool,
											gchar** values);

G_END_DECLS

#endif /* __AS_DATAPOOL_H */
