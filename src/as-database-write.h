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

#ifndef __AS_DATABASEWRITE_H
#define __AS_DATABASEWRITE_H

#include <glib-object.h>
#include "appstream_internal.h"
#include "appstream.h"

#define AS_TYPE_DATABASE_WRITE (as_database_write_get_type ())
#define AS_DATABASE_WRITE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_DATABASE_WRITE, AsDatabaseWrite))
#define AS_DATABASE_WRITE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_DATABASE_WRITE, AsDatabaseWriteClass))
#define AS_IS_DATABASE_WRITE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_DATABASE_WRITE))
#define AS_IS_DATABASE_WRITE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_DATABASE_WRITE))
#define AS_DATABASE_WRITE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_DATABASE_WRITE, AsDatabaseWriteClass))

G_BEGIN_DECLS

typedef struct _AsDatabaseWrite AsDatabaseWrite;
typedef struct _AsDatabaseWriteClass AsDatabaseWriteClass;
typedef struct _AsDatabaseWritePrivate AsDatabaseWritePrivate;

struct _AsDatabaseWrite {
	AsDatabase parent_instance;
	AsDatabaseWritePrivate * priv;
};

struct _AsDatabaseWriteClass {
	AsDatabaseClass parent_class;
};

GType					as_database_write_get_type (void) G_GNUC_CONST;

AsDatabaseWrite*		as_database_write_new (void);
AsDatabaseWrite*		as_database_write_construct (GType object_type);
gboolean				as_database_write_rebuild (AsDatabaseWrite* self, GPtrArray* cpt_array);

G_END_DECLS

#endif /* __AS_DATABASEWRITE_H */
