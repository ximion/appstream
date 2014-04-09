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

#ifndef __AS_SEARCHQUERY_H
#define __AS_SEARCHQUERY_H

#include <glib-object.h>

#define AS_TYPE_SEARCH_QUERY (as_search_query_get_type ())
#define AS_SEARCH_QUERY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_SEARCH_QUERY, AsSearchQuery))
#define AS_SEARCH_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_SEARCH_QUERY, AsSearchQueryClass))
#define AS_IS_SEARCH_QUERY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_SEARCH_QUERY))
#define AS_IS_SEARCH_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_SEARCH_QUERY))
#define AS_SEARCH_QUERY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_SEARCH_QUERY, AsSearchQueryClass))

G_BEGIN_DECLS

typedef struct _AsSearchQuery AsSearchQuery;
typedef struct _AsSearchQueryClass AsSearchQueryClass;
typedef struct _AsSearchQueryPrivate AsSearchQueryPrivate;

struct _AsSearchQuery
{
	GObject parent_instance;
	AsSearchQueryPrivate * priv;
};

struct _AsSearchQueryClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

GType					as_search_query_get_type (void) G_GNUC_CONST;

AsSearchQuery*			as_search_query_new (const gchar* term);
AsSearchQuery*			as_search_query_construct (GType object_type,
												   const gchar* term);
void					as_search_query_set_search_term (AsSearchQuery* self,
														 const gchar* value);
gboolean				as_search_query_get_search_all_categories (AsSearchQuery* self);
gchar**					as_search_query_get_categories (AsSearchQuery* self);
void					as_search_query_set_search_all_categories (AsSearchQuery* self);
void					as_search_query_set_categories (AsSearchQuery* self,
														gchar** value);
void					as_search_query_set_categories_from_string (AsSearchQuery* self,
																	const gchar* categories_str);
void					as_search_query_sanitize_search_term (AsSearchQuery* self);
const gchar*			as_search_query_get_search_term (AsSearchQuery* self);

G_END_DECLS

#endif /* __AS_SEARCHQUERY_H */
