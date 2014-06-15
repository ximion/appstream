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

#ifndef __AS_DATABASEBUILDER_H
#define __AS_DATABASEBUILDER_H

#include <glib-object.h>

#define AS_TYPE_BUILDER (as_builder_get_type ())
#define AS_BUILDER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_BUILDER, AsBuilder))
#define AS_BUILDER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_BUILDER, AsBuilderClass))
#define AS_IS_BUILDER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_BUILDER))
#define AS_IS_BUILDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_BUILDER))
#define AS_BUILDER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_BUILDER, AsBuilderClass))

G_BEGIN_DECLS

typedef struct _AsBuilder AsBuilder;
typedef struct _AsBuilderClass AsBuilderClass;
typedef struct _AsBuilderPrivate AsBuilderPrivate;

struct _AsBuilder {
	GObject parent_instance;
	AsBuilderPrivate * priv;
};

struct _AsBuilderClass {
	GObjectClass parent_class;
};

GType as_builder_get_type (void) G_GNUC_CONST;

AsBuilder*		as_builder_new (void);
AsBuilder*		as_builder_construct (GType object_type);
AsBuilder*		as_builder_new_path (const gchar* dbpath);
AsBuilder*		as_builder_construct_path (GType object_type, const gchar* dbpath);

gboolean		as_builder_initialize (AsBuilder* self);
gboolean		as_builder_refresh_cache (AsBuilder* self, gboolean force);

G_END_DECLS

#endif /* __AS_DATABASEBUILDER_H */
