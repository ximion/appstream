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

/**
 * AsProvidesKind:
 * @AS_PROVIDES_KIND_UNKNOWN:	Unknown kind
 * @AS_PROVIDES_KIND_LIBRARY:	A shared library
 * @AS_PROVIDES_KIND_BINARY:	A binary installed into a directory in PATH
 * @AS_PROVIDES_KIND_FONT:		A font
 * @AS_PROVIDES_KIND_MODALIAS:	A modalias
 * @AS_PROVIDES_KIND_FIRMWARE:	Kernel firmware
 * @AS_PROVIDES_KIND_PYTHON2:	A Python2 module
 * @AS_PROVIDES_KIND_PYTHON3:	A Python3 module
 * @AS_PROVIDES_KIND_MIMETYPE:	Provides a handler for a mimetype
 * @AS_PROVIDES_KIND_DBUS:		A DBus service name
 *
 * Public interfaces components can provide.
 **/
typedef enum  {
	AS_PROVIDES_KIND_UNKNOWN,
	AS_PROVIDES_KIND_LIBRARY,
	AS_PROVIDES_KIND_BINARY,
	AS_PROVIDES_KIND_FONT,
	AS_PROVIDES_KIND_MODALIAS,
	AS_PROVIDES_KIND_FIRMWARE,
	AS_PROVIDES_KIND_PYTHON2,
	AS_PROVIDES_KIND_PYTHON3,
	AS_PROVIDES_KIND_MIMETYPE,
	AS_PROVIDES_KIND_DBUS,
	/* < private > */
	AS_PROVIDES_KIND_LAST
} AsProvidesKind;

const gchar*		as_provides_kind_to_string (AsProvidesKind kind);
AsProvidesKind		as_provides_kind_from_string (const gchar *kind_str);

gchar*				as_provides_item_create (AsProvidesKind kind, const gchar *value, const gchar *data);
AsProvidesKind		as_provides_item_get_kind (const gchar *item);
gchar*				as_provides_item_get_value (const gchar *item);

G_END_DECLS

#endif /* __AS_PROVIDES_H */
