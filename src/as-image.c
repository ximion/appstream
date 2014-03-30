/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C)      2014 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:as-image
 * @short_description: Object representing a single image used in a screenshot.
 *
 * Screenshot may have multiple versions of an image in different resolutions
 * or aspect ratios. This object allows access to the location and size of a
 * single image.
 *
 * See also: #AsScreenshot
 */

#include "config.h"

#include "as-image-private.h"
#include "as-node-private.h"
#include "as-utils-private.h"

typedef struct _AsImagePrivate	AsImagePrivate;
struct _AsImagePrivate
{
	AsImageKind		 kind;
	gchar			*url;
	guint			 width;
	guint			 height;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsImage, as_image, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_image_get_instance_private (o))

/**
 * as_image_finalize:
 **/
static void
as_image_finalize (GObject *object)
{
	AsImage *image = AS_IMAGE (object);
	AsImagePrivate *priv = GET_PRIVATE (image);

	g_free (priv->url);

	G_OBJECT_CLASS (as_image_parent_class)->finalize (object);
}

/**
 * as_image_init:
 **/
static void
as_image_init (AsImage *image)
{
}

/**
 * as_image_class_init:
 **/
static void
as_image_class_init (AsImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_image_finalize;
}


/**
 * as_image_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsImageKind, or %AS_IMAGE_KIND_UNKNOWN for unknown.
 *
 **/
AsImageKind
as_image_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "source") == 0)
		return AS_IMAGE_KIND_SOURCE;
	if (g_strcmp0 (kind, "thumbnail") == 0)
		return AS_IMAGE_KIND_THUMBNAIL;
	return AS_IMAGE_KIND_UNKNOWN;
}

/**
 * as_image_kind_to_string:
 * @kind: the #AsImageKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 **/
const gchar *
as_image_kind_to_string (AsImageKind kind)
{
	if (kind == AS_IMAGE_KIND_SOURCE)
		return "source";
	if (kind == AS_IMAGE_KIND_THUMBNAIL)
		return "thumbnail";
	return NULL;
}

/**
 * as_image_get_url:
 * @image: a #AsImage instance.
 *
 * Gets the full qualified URL for the image, usually pointing at some mirror.
 *
 * Returns: URL
 *
 **/
const gchar *
as_image_get_url (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->url;
}

/**
 * as_image_get_width:
 * @image: a #AsImage instance.
 *
 * Gets the image width.
 *
 * Returns: width in pixels
 *
 **/
guint
as_image_get_width (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->width;
}

/**
 * as_image_get_height:
 * @image: a #AsImage instance.
 *
 * Gets the image height.
 *
 * Returns: height in pixels
 *
 **/
guint
as_image_get_height (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->height;
}

/**
 * as_image_get_kind:
 * @image: a #AsImage instance.
 *
 * Gets the image kind.
 *
 * Returns: the #AsImageKind
 *
 **/
AsImageKind
as_image_get_kind (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->kind;
}

/**
 * as_image_set_url:
 * @image: a #AsImage instance.
 * @url: the URL.
 *
 * Sets the fully-qualified mirror URL to use for the image.
 *
 **/
void
as_image_set_url (AsImage *image, const gchar *url)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	g_free (priv->url);
	priv->url = as_strdup (url);
}

/**
 * as_image_set_width:
 * @image: a #AsImage instance.
 * @width: the width in pixels.
 *
 * Sets the image width.
 *
 **/
void
as_image_set_width (AsImage *image, guint width)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->width = width;
}

/**
 * as_image_set_height:
 * @image: a #AsImage instance.
 * @height: the height in pixels.
 *
 * Sets the image height.
 *
 **/
void
as_image_set_height (AsImage *image, guint height)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->height = height;
}

/**
 * as_image_set_kind:
 * @image: a #AsImage instance.
 * @kind: the #AsImageKind, e.g. %AS_IMAGE_KIND_THUMBNAIL.
 *
 * Sets the image kind.
 *
 **/
void
as_image_set_kind (AsImage *image, AsImageKind kind)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->kind = kind;
}

/**
 * as_image_new:
 *
 * Creates a new #AsImage.
 *
 * Returns: (transfer full): a #AsImage
 *
 **/
AsImage *
as_image_new (void)
{
	AsImage *image;
	image = g_object_new (AS_TYPE_IMAGE, NULL);
	return AS_IMAGE (image);
}
