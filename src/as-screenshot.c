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

/**
 * SECTION:as-screenshot
 * @short_description: Object representing a single screenshot
 *
 * Screenshots have a localized caption and also contain a number of images
 * of different resolution.
 *
 * See also: #AsImage
 */

#include "as-screenshot.h"

#include "as-utils.h"

typedef struct _AsScreenshotPrivate	AsScreenshotPrivate;
struct _AsScreenshotPrivate
{
	AsScreenshotKind	kind;
	gchar				*caption;
	GPtrArray			*images;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsScreenshot, as_screenshot, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_screenshot_get_instance_private (o))

/**
 * as_screenshot_finalize:
 **/
static void
as_screenshot_finalize (GObject *object)
{
	AsScreenshot *screenshot = AS_SCREENSHOT (object);
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	g_ptr_array_unref (priv->images);
	g_free (priv->caption);

	G_OBJECT_CLASS (as_screenshot_parent_class)->finalize (object);
}

/**
 * as_screenshot_init:
 **/
static void
as_screenshot_init (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->kind = AS_SCREENSHOT_KIND_NORMAL;
	priv->images = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->caption = g_strdup ("");
}

/**
 * as_screenshot_class_init:
 **/
static void
as_screenshot_class_init (AsScreenshotClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_screenshot_finalize;
}

/**
 * as_screenshot_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a %AsScreenshotKind, or
 *                           %AS_SCREENSHOT_KIND_UNKNOWN if not known.
 *
 **/
AsScreenshotKind
as_screenshot_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "normal") == 0)
		return AS_SCREENSHOT_KIND_NORMAL;
	if (g_strcmp0 (kind, "default") == 0)
		return AS_SCREENSHOT_KIND_DEFAULT;
	return AS_SCREENSHOT_KIND_UNKNOWN;
}

/**
 * as_screenshot_kind_to_string:
 * @kind: the #AsScreenshotKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 **/
const gchar *
as_screenshot_kind_to_string (AsScreenshotKind kind)
{
	if (kind == AS_SCREENSHOT_KIND_NORMAL)
		return "normal";
	if (kind == AS_SCREENSHOT_KIND_DEFAULT)
		return "default";
	return NULL;
}

/**
 * as_screenshot_get_kind:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the screenshot kind.
 *
 * Returns: a #AsScreenshotKind
 *
 **/
AsScreenshotKind
as_screenshot_get_kind (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->kind;
}

/**
 * as_screenshot_get_images:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the image sizes included in the screenshot.
 *
 * Returns: (element-type AsImage) (transfer none): an array
 *
 **/
GPtrArray*
as_screenshot_get_images (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->images;
}

/**
 * as_screenshot_get_caption:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the image caption
 *
 * Returns: the caption
 *
 **/
const gchar *
as_screenshot_get_caption (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->caption;
}

/**
 * as_screenshot_set_kind:
 * @screenshot: a #AsScreenshot instance.
 * @kind: the #AsScreenshotKind.
 *
 * Sets the screenshot kind.
 *
 **/
void
as_screenshot_set_kind (AsScreenshot *screenshot, AsScreenshotKind kind)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->kind = kind;
}

/**
 * as_screenshot_add_image:
 * @screenshot: a #AsScreenshot instance.
 * @image: a #AsImage instance.
 *
 * Adds an image to the screenshot.
 *
 **/
void
as_screenshot_add_image (AsScreenshot *screenshot, AsImage *image)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	g_ptr_array_add (priv->images, g_object_ref (image));
}

/**
 * as_screenshot_set_caption:
 * @screenshot: a #AsScreenshot instance.
 * @caption: the caption text.
 *
 * Sets a caption on the screenshot
 *
 **/
void
as_screenshot_set_caption (AsScreenshot *screenshot, const gchar *caption)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	g_free (priv->caption);
	priv->caption = g_strdup (caption);
}

/**
 * as_screenshot_is_valid:
 * @screenshot: a #AsScreenshot instance.
 *
 * Performs a quick validation on this screenshot
 *
 * Returns: TRUE if the screenshot is a complete #AsScreenshot
 *
 **/
gboolean
as_screenshot_is_valid (AsScreenshot *sshot)
{
	return sshot->priv->images->len > 0;
}

/**
 * as_screenshot_new:
 *
 * Creates a new #AsScreenshot.
 *
 * Returns: (transfer full): a #AsScreenshot
 *
 **/
AsScreenshot *
as_screenshot_new (void)
{
	AsScreenshot *screenshot;
	screenshot = g_object_new (AS_TYPE_SCREENSHOT, NULL);
	return AS_SCREENSHOT (screenshot);
}
