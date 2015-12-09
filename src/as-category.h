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

#ifndef __AS_CATEGORY_H
#define __AS_CATEGORY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_CATEGORY (as_category_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsCategory, as_category, AS, CATEGORY, GObject)

struct _AsCategoryClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

AsCategory		*as_category_new (void);
void			as_category_complete (AsCategory *cat);

const gchar		*as_category_get_directory (AsCategory *cat);
void			as_category_set_directory (AsCategory *cat,
							const gchar* value);

const gchar		*as_category_get_name (AsCategory *cat);
void			as_category_set_name (AsCategory *cat,
						const gchar *value);

const gchar		*as_category_get_summary (AsCategory *cat);
void			as_category_set_summary (AsCategory *cat,
						const gchar *value);

const gchar		*as_category_get_icon (AsCategory *cat);
void			as_category_set_icon (AsCategory *cat,
						const gchar* value);

gboolean		as_category_has_subcategory (AsCategory *cat);
void			as_category_add_subcategory (AsCategory *cat,
							AsCategory *subcat);
void			as_category_remove_subcategory (AsCategory *cat,
							AsCategory *subcat);

GList			*as_category_get_included (AsCategory *cat);
GList			*as_category_get_excluded (AsCategory *cat);

GList			*as_category_get_subcategories (AsCategory *cat);

gint			as_category_get_level (AsCategory *cat);
void			as_category_set_level (AsCategory *cat,
						gint value);

G_END_DECLS

#endif /* __AS_CATEGORY_H */
