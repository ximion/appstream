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

#include "as-provides.h"

#include <glib.h>

#include "as-utils.h"

/**
 * as_provides_kind_to_string:
 * @kind: the #AsProvidesKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar *
as_provides_kind_to_string (AsProvidesKind kind)
{
	if (kind == AS_PROVIDES_KIND_LIBRARY)
		return "lib";
	if (kind == AS_PROVIDES_KIND_BINARY)
		return "bin";
	if (kind == AS_PROVIDES_KIND_PYTHON2)
		return "python2";
	if (kind == AS_PROVIDES_KIND_PYTHON3)
		return "python3";
	if (kind == AS_PROVIDES_KIND_FONT)
		return "font";
	return "unknown";
}

/**
 * as_provides_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsProvidesKind or %AS_PROVIDES_KIND_UNKNOWN for unknown
 **/
AsProvidesKind
as_provides_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "lib") == 0)
		return AS_PROVIDES_KIND_LIBRARY;
	if (g_strcmp0 (kind_str, "bin") == 0)
		return AS_PROVIDES_KIND_BINARY;
	if (g_strcmp0 (kind_str, "python2") == 0)
		return AS_PROVIDES_KIND_PYTHON2;
	if (g_strcmp0 (kind_str, "python3") == 0)
		return AS_PROVIDES_KIND_PYTHON3;
	if (g_strcmp0 (kind_str, "font") == 0)
		return AS_PROVIDES_KIND_FONT;
	return AS_PROVIDES_KIND_UNKNOWN;
}

/**
 * as_provides_item_create:
 *
 * Creates a new provides-item string, which
 * consists of a type-part describing the items type, and a name-part,
 * containing the name of the item. Both are separated by a colon,
 * so an item of type KIND_LIBRARY and name libappstream.so.0 will become
 * "lib:libappstream.so.0"
 *
 * @kind a #AsProvidesKind describing the type of the item string
 * @value the name of the item as string
 *
 * Returns: a new provides-item string. Free with g_free
 **/
gchar*
as_provides_item_create (AsProvidesKind kind, const gchar *value)
{
	const gchar *kind_str;
	gchar *res;
	kind_str = as_provides_kind_to_string (kind);

	res = g_strdup_printf ("%s:%s", kind_str, value);
	return res;
}

/**
 * as_provides_item_get_kind:
 *
 * Returns the type (kind) of a provides-item string
 * as #AsProvidesKind
 *
 * @item a valid provides-item string
 *
 * Returns: the kind of the given item
 **/
AsProvidesKind
as_provides_item_get_kind (const gchar *item)
{
	AsProvidesKind res;
	gchar **parts;

	res = AS_PROVIDES_KIND_UNKNOWN;

	parts = g_strsplit (item, ":", 1);
	/* return unknown if the item was not valid */
	if (g_strv_length (parts) < 2)
		goto out;
	res = as_provides_kind_from_string (parts[0]);

out:
	g_strfreev (parts);
	return res;
}

/**
 * as_provides_item_get_value:
 *
 * Returns the value (name) of a provides-item string
 *
 * @item a valid provides-item string
 *
 * Returns: the value of the given item, or NULL if the item was invalid
 **/
gchar*
as_provides_item_get_value (const gchar *item)
{
	gchar *res;
	gchar **parts;

	res = NULL;

	parts = g_strsplit (item, ":", 1);
	/* return unknown if the item was not valid */
	if (g_strv_length (parts) < 2)
		goto out;
	res = g_strdup (parts[1]);

out:
	g_strfreev (parts);
	return res;
}

