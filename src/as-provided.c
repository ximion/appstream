/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-provided.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <glib.h>

#include "as-utils.h"

/**
 * SECTION:as-provided
 * @short_description: Description of the provided-items in components
 * @include: appstream.h
 *
 * Components can provide various items, like libraries, Python-modules,
 * firmware, binaries, etc.
 * Functions to work with these items are provided here.
 *
 * See also: #AsComponent
 */

typedef struct
{
	AsProvidedKind	kind;
	GHashTable	*items;

	GPtrArray	*items_array;
} AsProvidedPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsProvided, as_provided, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_provided_get_instance_private (o))

/**
 * as_provided_kind_to_string:
 * @kind: the #AsProvidedKind.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 **/
const gchar*
as_provided_kind_to_string (AsProvidedKind kind)
{
	if (kind == AS_PROVIDED_KIND_LIBRARY)
		return "lib";
	if (kind == AS_PROVIDED_KIND_BINARY)
		return "bin";
	if (kind == AS_PROVIDED_KIND_MIMETYPE)
		return "mimetype";
	if (kind == AS_PROVIDED_KIND_FONT)
		return "font";
	if (kind == AS_PROVIDED_KIND_MODALIAS)
		return "modalias";
	if (kind == AS_PROVIDED_KIND_PYTHON_2)
		return "python2";
	if (kind == AS_PROVIDED_KIND_PYTHON)
		return "python";
	if (kind == AS_PROVIDED_KIND_DBUS_SYSTEM)
		return "dbus:system";
	if (kind == AS_PROVIDED_KIND_DBUS_USER)
		return "dbus:user";
	if (kind == AS_PROVIDED_KIND_FIRMWARE_RUNTIME)
		return "firmware:runtime";
	if (kind == AS_PROVIDED_KIND_FIRMWARE_FLASHED)
		return "firmware:flashed";
	return "unknown";
}

/**
 * as_provided_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsProvidedKind or %AS_PROVIDED_KIND_UNKNOWN for unknown
 **/
AsProvidedKind
as_provided_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "lib") == 0)
		return AS_PROVIDED_KIND_LIBRARY;
	if (g_strcmp0 (kind_str, "bin") == 0)
		return AS_PROVIDED_KIND_BINARY;
	if (g_strcmp0 (kind_str, "mimetype") == 0)
		return AS_PROVIDED_KIND_MIMETYPE;
	if (g_strcmp0 (kind_str, "font") == 0)
		return AS_PROVIDED_KIND_FONT;
	if (g_strcmp0 (kind_str, "modalias") == 0)
		return AS_PROVIDED_KIND_MODALIAS;
	if (g_strcmp0 (kind_str, "python2") == 0)
		return AS_PROVIDED_KIND_PYTHON_2;
	if (g_strcmp0 (kind_str, "python") == 0)
		return AS_PROVIDED_KIND_PYTHON;
	if (g_strcmp0 (kind_str, "dbus:system") == 0)
		return AS_PROVIDED_KIND_DBUS_SYSTEM;
	if (g_strcmp0 (kind_str, "dbus:user") == 0)
		return AS_PROVIDED_KIND_DBUS_USER;
	if (g_strcmp0 (kind_str, "firmware:runtime") == 0)
		return AS_PROVIDED_KIND_FIRMWARE_RUNTIME;
	if (g_strcmp0 (kind_str, "firmware:flashed") == 0)
		return AS_PROVIDED_KIND_FIRMWARE_FLASHED;
	return AS_PROVIDED_KIND_UNKNOWN;
}

/**
 * as_provided_kind_to_l10n_string:
 * @kind: the #AsProvidedKind.
 *
 * Converts the enumerated value to a localized text representation,
 * using the plural forms (e.g. "Libraries" instead of "Library").
 *
 * This can be useful when displaying provided items in GUI dialogs.
 *
 * Returns: Pluralized, l10n string version of @kind
 **/
const gchar*
as_provided_kind_to_l10n_string (AsProvidedKind kind)
{
	if (kind == AS_PROVIDED_KIND_LIBRARY)
		return _("Libraries");
	if (kind == AS_PROVIDED_KIND_BINARY)
		return _("Binaries");
	if (kind == AS_PROVIDED_KIND_MIMETYPE)
		return _("Mimetypes");
	if (kind == AS_PROVIDED_KIND_FONT)
		return _("Fonts");
	if (kind == AS_PROVIDED_KIND_MODALIAS)
		return _("Modaliases");
	if (kind == AS_PROVIDED_KIND_PYTHON_2)
		return _("Python (Version 2)");
	if (kind == AS_PROVIDED_KIND_PYTHON)
		return _("Python 3");
	if (kind == AS_PROVIDED_KIND_DBUS_SYSTEM)
		return _("DBus System Services");
	if (kind == AS_PROVIDED_KIND_DBUS_USER)
		return _("DBus Session Services");
	if (kind == AS_PROVIDED_KIND_FIRMWARE_RUNTIME)
		return _("Runtime Firmware");
	if (kind == AS_PROVIDED_KIND_FIRMWARE_FLASHED)
		return _("Flashed Firmware");
	return as_provided_kind_to_string (kind);
}

/**
 * as_provided_finalize:
 **/
static void
as_provided_finalize (GObject *object)
{
	AsProvided *prov = AS_PROVIDED (object);
	AsProvidedPrivate *priv = GET_PRIVATE (prov);

	g_hash_table_unref (priv->items);

	G_OBJECT_CLASS (as_provided_parent_class)->finalize (object);
}

/**
 * as_provided_init:
 **/
static void
as_provided_init (AsProvided *prov)
{
	AsProvidedPrivate *priv = GET_PRIVATE (prov);

	priv->kind = AS_PROVIDED_KIND_UNKNOWN;
	priv->items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	priv->items_array = g_ptr_array_new ();
}

/**
 * as_provided_class_init:
 **/
static void
as_provided_class_init (AsProvidedClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_provided_finalize;
}

/**
 * as_provided_get_kind:
 * @prov: a #AsProvided instance.
 *
 * The kind of items this #AsProvided object stores.
 *
 * Returns: an enum of type #AsProvidedKind
 */
AsProvidedKind
as_provided_get_kind (AsProvided *prov)
{
	AsProvidedPrivate *priv = GET_PRIVATE (prov);
	return priv->kind;
}

/**
 * as_provided_set_kind:
 * @prov: a #AsProvided instance.
 * @kind: the new #AsProvidedKind
 *
 * Set the kind of items this #AsProvided object stores.
 */
void
as_provided_set_kind (AsProvided *prov, AsProvidedKind kind)
{
	AsProvidedPrivate *priv = GET_PRIVATE (prov);
	priv->kind = kind;
}

/**
 * as_provided_has_item:
 * @prov: a #AsProvided instance.
 * @item: the name of a provided item, e.g. "audio/x-vorbis" (in case the provided kind is a mimetype)
 *
 * Check if the current #AsProvided contains an item
 * of the given name.
 *
 * Returns: %TRUE if found.
 */
gboolean
as_provided_has_item (AsProvided *prov, const gchar *item)
{
	AsProvidedPrivate *priv = GET_PRIVATE (prov);
	return g_hash_table_contains (priv->items, item);
}

/**
 * as_provided_get_items:
 * @prov: a #AsProvided instance.
 *
 * Get an array of provided data.
 *
 * Returns: (transfer none) (element-type utf8): An utf-8 array of provided items, free with g_free()
 */
GPtrArray*
as_provided_get_items (AsProvided *prov)
{
	g_autofree gchar **strv = NULL;
	guint i;
	AsProvidedPrivate *priv = GET_PRIVATE (prov);
	if (priv->items_array != NULL)
		return priv->items_array;

	strv = (gchar**) g_hash_table_get_keys_as_array (priv->items, NULL);
	priv->items_array = g_ptr_array_new ();
	for (i = 0; strv[i] != NULL; i++) {
		g_ptr_array_add (priv->items_array, strv[i]);
	}
	return priv->items_array;
}

/**
 * as_provided_add_item:
 * @prov: a #AsProvided instance.
 *
 * Add a new provided item.
 */
void
as_provided_add_item (AsProvided *prov, const gchar *item)
{
	AsProvidedPrivate *priv = GET_PRIVATE (prov);
	g_hash_table_add (priv->items, g_strdup (item));

	/* invalidate list */
	if (priv->items_array != NULL) {
		g_ptr_array_unref (priv->items_array);
		priv->items_array = NULL;
	}
}

/**
 * as_provided_new:
 *
 * Creates a new #AsProvided.
 *
 * Returns: (transfer full): a #AsProvided
 **/
AsProvided*
as_provided_new (void)
{
	AsProvided *prov;
	prov = g_object_new (AS_TYPE_PROVIDED, NULL);
	return AS_PROVIDED (prov);
}
