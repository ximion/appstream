/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:as-bundle
 * @short_description: Description of bundles the #AsComponent is shipped with.
 * @include: appstream.h
 *
 * This class provides information contained in an AppStream bundle tag.
 * See https://www.freedesktop.org/software/appstream/docs/chap-CollectionData.html#tag-ct-bundle
 * for more information.
 *
 * See also: #AsComponent
 */

#include "config.h"
#include "as-bundle.h"

typedef struct
{
	AsBundleKind	kind;
	gchar		*id;
} AsBundlePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsBundle, as_bundle, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_bundle_get_instance_private (o))

/**
 * as_bundle_kind_to_string:
 * @kind: the %AsBundleKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.8.0
 **/
const gchar*
as_bundle_kind_to_string (AsBundleKind kind)
{
	if (kind == AS_BUNDLE_KIND_PACKAGE)
		return "package";
	if (kind == AS_BUNDLE_KIND_LIMBA)
		return "limba";
	if (kind == AS_BUNDLE_KIND_FLATPAK)
		return "flatpak";
	if (kind == AS_BUNDLE_KIND_APPIMAGE)
		return "appimage";
	if (kind == AS_BUNDLE_KIND_SNAP)
		return "snap";
	return "unknown";
}

/**
 * as_bundle_kind_from_string:
 * @bundle_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsBundleKind or %AS_BUNDLE_KIND_UNKNOWN for unknown
 **/
AsBundleKind
as_bundle_kind_from_string (const gchar *bundle_str)
{
	if (g_strcmp0 (bundle_str, "package") == 0)
		return AS_BUNDLE_KIND_PACKAGE;
	if (g_strcmp0 (bundle_str, "limba") == 0)
		return AS_BUNDLE_KIND_LIMBA;
	if (g_strcmp0 (bundle_str, "flatpak") == 0)
		return AS_BUNDLE_KIND_FLATPAK;
	if (g_strcmp0 (bundle_str, "appimage") == 0)
		return AS_BUNDLE_KIND_APPIMAGE;
	if (g_strcmp0 (bundle_str, "snap") == 0)
		return AS_BUNDLE_KIND_SNAP;
	return AS_BUNDLE_KIND_UNKNOWN;
}

static void
as_bundle_finalize (GObject *object)
{
	AsBundle *bundle = AS_BUNDLE (object);
	AsBundlePrivate *priv = GET_PRIVATE (bundle);

	g_free (priv->id);

	G_OBJECT_CLASS (as_bundle_parent_class)->finalize (object);
}

static void
as_bundle_init (AsBundle *bundle)
{
}

static void
as_bundle_class_init (AsBundleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_bundle_finalize;
}

/**
 * as_bundle_get_id:
 * @bundle: an #AsBundle instance.
 *
 * Gets the ID for this bundle.
 *
 * Returns: ID, e.g. "foobar-1.0.2"
 *
 * Since: 0.10
 **/
const gchar*
as_bundle_get_id (AsBundle *bundle)
{
	AsBundlePrivate *priv = GET_PRIVATE (bundle);
	return priv->id;
}

/**
 * as_bundle_set_id:
 * @bundle: an #AsBundle instance.
 * @id: the URL.
 *
 * Sets the ID for this bundle.
 *
 * Since: 0.10
 **/
void
as_bundle_set_id (AsBundle *bundle, const gchar *id)
{
	AsBundlePrivate *priv = GET_PRIVATE (bundle);
	g_free (priv->id);
	priv->id = g_strdup (id);
}

/**
 * as_bundle_get_kind:
 * @bundle: an #AsBundle instance.
 *
 * Gets the bundle kind.
 *
 * Returns: the #AsBundleKind
 *
 * Since: 0.10
 **/
AsBundleKind
as_bundle_get_kind (AsBundle *bundle)
{
	AsBundlePrivate *priv = GET_PRIVATE (bundle);
	return priv->kind;
}

/**
 * as_bundle_set_kind:
 * @bundle: an #AsBundle instance.
 * @kind: the #AsBundleKind, e.g. %AS_BUNDLE_KIND_LIMBA.
 *
 * Sets the bundle kind.
 *
 * Since: 0.10
 **/
void
as_bundle_set_kind (AsBundle *bundle, AsBundleKind kind)
{
	AsBundlePrivate *priv = GET_PRIVATE (bundle);
	priv->kind = kind;
}

/**
 * as_bundle_new:
 *
 * Creates a new #AsBundle.
 *
 * Returns: (transfer full): a #AsBundle
 *
 * Since: 0.10
 **/
AsBundle*
as_bundle_new (void)
{
	AsBundle *bundle;
	bundle = g_object_new (AS_TYPE_BUNDLE, NULL);
	return AS_BUNDLE (bundle);
}
