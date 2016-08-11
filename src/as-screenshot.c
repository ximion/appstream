/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C)      2014 Richard Hughes <richard@hughsie.com>
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
 * SECTION:as-screenshot
 * @short_description: Object representing a single screenshot
 *
 * Screenshots have a localized caption and also contain a number of images
 * of different resolution.
 *
 * See also: #AsImage
 */

#include "as-screenshot.h"
#include "as-screenshot-private.h"

#include "as-utils-private.h"
#include "as-utils.h"

typedef struct
{
	AsScreenshotKind kind;
	GHashTable *caption;
	GPtrArray *images;
	gchar *active_locale;
} AsScreenshotPrivate;

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

	g_free (priv->active_locale);
	g_ptr_array_unref (priv->images);
	g_hash_table_unref (priv->caption);

	G_OBJECT_CLASS (as_screenshot_parent_class)->finalize (object);
}

/**
 * as_screenshot_init:
 **/
static void
as_screenshot_init (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	priv->active_locale = g_strdup ("C");
	priv->kind = AS_SCREENSHOT_KIND_EXTRA;
	priv->images = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->caption = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
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
 * Returns: a %AsScreenshotKind, or %AS_SCREENSHOT_KIND_UNKNOWN if not known.
 **/
AsScreenshotKind
as_screenshot_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "default") == 0)
		return AS_SCREENSHOT_KIND_DEFAULT;
	if (g_strcmp0 (kind, "extra") == 0)
		return AS_SCREENSHOT_KIND_EXTRA;
	return AS_SCREENSHOT_KIND_UNKNOWN;
}

/**
 * as_screenshot_kind_to_string:
 * @kind: the #AsScreenshotKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar *
as_screenshot_kind_to_string (AsScreenshotKind kind)
{
	if (kind == AS_SCREENSHOT_KIND_DEFAULT)
		return "default";
	if (kind == AS_SCREENSHOT_KIND_EXTRA)
		return "extra";
	return NULL;
}

/**
 * as_screenshot_get_kind:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the screenshot kind.
 *
 * Returns: a #AsScreenshotKind
 **/
AsScreenshotKind
as_screenshot_get_kind (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->kind;
}

/**
 * as_screenshot_set_kind:
 * @screenshot: a #AsScreenshot instance.
 * @kind: the #AsScreenshotKind.
 *
 * Sets the screenshot kind.
 **/
void
as_screenshot_set_kind (AsScreenshot *screenshot, AsScreenshotKind kind)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->kind = kind;
}

/**
 * as_screenshot_get_images:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the image sizes included in the screenshot.
 *
 * Returns: (element-type AsImage) (transfer none): an array
 **/
GPtrArray*
as_screenshot_get_images (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->images;
}

/**
 * as_screenshot_add_image:
 * @screenshot: a #AsScreenshot instance.
 * @image: a #AsImage instance.
 *
 * Adds an image to the screenshot.
 **/
void
as_screenshot_add_image (AsScreenshot *screenshot, AsImage *image)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	g_ptr_array_add (priv->images, g_object_ref (image));
}

/**
 * as_screenshot_get_caption:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the image caption
 *
 * Returns: the caption
 **/
const gchar *
as_screenshot_get_caption (AsScreenshot *screenshot)
{
	const gchar *caption;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	caption = g_hash_table_lookup (priv->caption, priv->active_locale);
	if (caption == NULL) {
		/* fall back to untranslated / default */
		caption = g_hash_table_lookup (priv->caption, "C");
	}

	return caption;
}

/**
 * as_screenshot_set_caption:
 * @screenshot: a #AsScreenshot instance.
 * @caption: the caption text.
 *
 * Sets a caption on the screenshot
 **/
void
as_screenshot_set_caption (AsScreenshot *screenshot, const gchar *caption, const gchar *locale)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	if (locale == NULL)
		locale = priv->active_locale;

	g_hash_table_insert (priv->caption,
				as_locale_strip_encoding (g_strdup (locale)),
				g_strdup (caption));
}

/**
 * as_screenshot_is_valid:
 * @screenshot: a #AsScreenshot instance.
 *
 * Performs a quick validation on this screenshot
 *
 * Returns: TRUE if the screenshot is a complete #AsScreenshot
 **/
gboolean
as_screenshot_is_valid (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->images->len > 0;
}

/**
 * as_screenshot_get_active_locale:
 *
 * Get the current active locale, which
 * is used to get localized messages.
 */
gchar*
as_screenshot_get_active_locale (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->active_locale;
}

/**
 * as_screenshot_set_active_locale:
 *
 * Set the current active locale, which
 * is used to get localized messages.
 * If the #AsComponent linking this #AsScreenshot was fetched
 * from a localized database, usually only
 * one locale is available.
 */
void
as_screenshot_set_active_locale (AsScreenshot *screenshot, const gchar *locale)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	g_free (priv->active_locale);
	priv->active_locale = g_strdup (locale);
}

/**
 * as_screenshot_get_images_localized:
 * @screenshot: an #AsScreenshot instance.
 *
 * Returns all images that are compatible with a specific locale.
 *
 * Returns: (element-type AsImage) (transfer container): an array
 *
 * Since: 0.9.5
 **/
GPtrArray *
as_screenshot_get_images_localized (AsScreenshot *screenshot)
{
	AsImage *img;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GPtrArray *res;
	guint i;

	/* user wants a specific locale */
	res = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->images->len; i++) {
		img = g_ptr_array_index (priv->images, i);
		if (!as_utils_locale_is_compatible (as_image_get_locale (img), priv->active_locale))
			continue;
		g_ptr_array_add (res, g_object_ref (img));
	}
	return res;
}

/**
 * as_screenshot_get_caption_table:
 *
 * Internal function.
 */
GHashTable*
as_screenshot_get_caption_table (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->caption;
}

/**
 * as_screenshot_new:
 *
 * Creates a new #AsScreenshot.
 *
 * Returns: (transfer full): a #AsScreenshot
 **/
AsScreenshot *
as_screenshot_new (void)
{
	AsScreenshot *screenshot;
	screenshot = g_object_new (AS_TYPE_SCREENSHOT, NULL);
	return AS_SCREENSHOT (screenshot);
}
