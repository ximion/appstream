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

#ifndef __AS_CATEGORY_H
#define __AS_CATEGORY_H

#include <glib-object.h>

#define AS_TYPE_CATEGORY (as_category_get_type ())
#define AS_CATEGORY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_CATEGORY, AsCategory))
#define AS_CATEGORY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_CATEGORY, AsCategoryClass))
#define AS_IS_CATEGORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_CATEGORY))
#define AS_IS_CATEGORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_CATEGORY))
#define AS_CATEGORY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_CATEGORY, AsCategoryClass))

G_BEGIN_DECLS

typedef struct _AsCategory AsCategory;
typedef struct _AsCategoryClass AsCategoryClass;
typedef struct _AsCategoryPrivate AsCategoryPrivate;

struct _AsCategory {
	GObject parent_instance;
	AsCategoryPrivate * priv;
};

struct _AsCategoryClass {
	GObjectClass parent_class;
};

GType					as_category_get_type (void) G_GNUC_CONST;

AsCategory*				as_category_new (void);
AsCategory*				as_category_construct (GType object_type);
void					as_category_complete (AsCategory* self);
const gchar*			as_category_get_directory (AsCategory* self);
const gchar*			as_category_get_name (AsCategory* self);
void					as_category_set_icon (AsCategory* self, const gchar* value);
void					as_category_set_name (AsCategory* self, const gchar* value);
const gchar*			as_category_get_summary (AsCategory* self);
const gchar*			as_category_get_icon (AsCategory* self);
void					as_category_add_subcategory (AsCategory* self, AsCategory* cat);
void					as_category_remove_subcategory (AsCategory* self, AsCategory* cat);
gboolean				as_category_has_subcategory (AsCategory* self);
void					as_category_set_directory (AsCategory* self, const gchar* value);
GList*					as_category_get_included (AsCategory* self);
GList*					as_category_get_excluded (AsCategory* self);
gint					as_category_get_level (AsCategory* self);
void					as_category_set_level (AsCategory* self, gint value);
GList*					as_category_get_subcategories (AsCategory* self);

G_END_DECLS

#endif /* __AS_CATEGORY_H */
