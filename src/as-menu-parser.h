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

#ifndef __AS_MENUPARSER_H
#define __AS_MENUPARSER_H

#include <glib-object.h>

#define AS_TYPE_MENU_PARSER (as_menu_parser_get_type ())
#define AS_MENU_PARSER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_MENU_PARSER, AsMenuParser))
#define AS_MENU_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_MENU_PARSER, AsMenuParserClass))
#define AS_IS_MENU_PARSER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_MENU_PARSER))
#define AS_IS_MENU_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_MENU_PARSER))
#define AS_MENU_PARSER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_MENU_PARSER, AsMenuParserClass))

G_BEGIN_DECLS

typedef struct _AsMenuParser AsMenuParser;
typedef struct _AsMenuParserClass AsMenuParserClass;
typedef struct _AsMenuParserPrivate AsMenuParserPrivate;

struct _AsMenuParser {
	GObject parent_instance;
	AsMenuParserPrivate *priv;
};

struct _AsMenuParserClass {
	GObjectClass parent_class;
};

GType					as_menu_parser_get_type (void) G_GNUC_CONST;

AsMenuParser*			as_menu_parser_new (void);
AsMenuParser*			as_menu_parser_construct (GType object_type);
AsMenuParser*			as_menu_parser_new_from_file (const gchar* menu_file);
AsMenuParser*			as_menu_parser_construct_from_file (GType object_type, const gchar* menu_file);
void					as_menu_parser_set_update_category_data (AsMenuParser* self, gboolean value);
GList*					as_menu_parser_parse (AsMenuParser* self);
gboolean				as_menu_parser_get_update_category_data (AsMenuParser* self);

GList*					as_get_system_categories (void);

G_END_DECLS

#endif /* __AS_MENUPARSER_H */
