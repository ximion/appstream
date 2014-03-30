/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef __AS_DATABASE_H
#define __AS_DATABASE_H

#include <glib-object.h>
#include "as-search-query.h"

#define AS_TYPE_DATABASE (as_database_get_type ())
#define AS_DATABASE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_DATABASE, AsDatabase))
#define AS_DATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_DATABASE, AsDatabaseClass))
#define AS_IS_DATABASE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_DATABASE))
#define AS_IS_DATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_DATABASE))
#define AS_DATABASE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_DATABASE, AsDatabaseClass))

G_BEGIN_DECLS

typedef struct _AsDatabase AsDatabase;
typedef struct _AsDatabaseClass AsDatabaseClass;
typedef struct _AsDatabasePrivate AsDatabasePrivate;

struct _AsDatabase {
	GObject parent_instance;
	AsDatabasePrivate * priv;
};

struct _AsDatabaseClass {
	GObjectClass parent_class;
	gboolean (*open) (AsDatabase* self);
};

GType as_database_get_type (void) G_GNUC_CONST;

AsDatabase* as_database_new (void);
AsDatabase* as_database_construct (GType object_type);
void as_database_set_database_path (AsDatabase* self, const gchar* value);
gboolean as_database_open (AsDatabase* self);
const gchar* as_database_get_database_path (AsDatabase* self);
gboolean as_database_db_exists (AsDatabase* self);
GPtrArray* as_database_get_all_components (AsDatabase* self);
GPtrArray* as_database_find_components (AsDatabase* self, AsSearchQuery* query);
GPtrArray* as_database_find_components_by_str (AsDatabase* self, const gchar* search_str, const gchar* categories_str);

G_END_DECLS

#endif /* __AS_DATABASE_H */
