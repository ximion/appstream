/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_PROVIDES_H
#define __AS_PROVIDES_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum  {
	AS_PROVIDES_KIND_UNKNOWN,
	AS_PROVIDES_KIND_LIBRARY,
	AS_PROVIDES_KIND_BINARY,
	AS_PROVIDES_KIND_FONT,
	AS_PROVIDES_KIND_MODALIAS,
	AS_PROVIDES_KIND_FIRMWARE,
	AS_PROVIDES_KIND_PYTHON2,
	AS_PROVIDES_KIND_PYTHON3,
	AS_PROVIDES_KIND_LAST
} AsProvidesKind;

const gchar*		as_provides_kind_to_string (AsProvidesKind kind);
AsProvidesKind		as_provides_kind_from_string (const gchar *kind_str);

gchar*				as_provides_item_create (AsProvidesKind kind, const gchar *value);
AsProvidesKind		as_provides_item_get_kind (const gchar *item);
gchar*				as_provides_item_get_value (const gchar *item);

G_END_DECLS

#endif /* __AS_PROVIDES_H */
