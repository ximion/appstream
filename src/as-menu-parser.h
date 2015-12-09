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

#ifndef __AS_MENUPARSER_H
#define __AS_MENUPARSER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_MENU_PARSER (as_menu_parser_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsMenuParser, as_menu_parser, AS, MENU_PARSER, GObject)

struct _AsMenuParserClass
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


AsMenuParser		*as_menu_parser_new (void);
AsMenuParser		*as_menu_parser_new_from_file (const gchar *menu_file);

GList			*as_menu_parser_parse (AsMenuParser *mp);

gboolean		as_menu_parser_get_update_category_data (AsMenuParser *mp);
void			as_menu_parser_set_update_category_data (AsMenuParser *mp,
								 gboolean value);

GList			*as_get_system_categories (void);

G_END_DECLS

#endif /* __AS_MENUPARSER_H */
