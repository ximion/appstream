/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2017 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-image-private.h"

typedef struct
{
	AsScreenshotKind kind;
	GHashTable *caption;
	GPtrArray *images;
	GPtrArray *images_lang;
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
	g_ptr_array_unref (priv->images_lang);
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
	priv->caption = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->images = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->images_lang = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
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
 * Gets the images for this screenshots. Only images valid for the current
 * language are returned. We return all sizes.
 *
 * Returns: (transfer none) (element-type AsImage): an array
 **/
GPtrArray*
as_screenshot_get_images (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	if (priv->images_lang->len == 0)
		return as_screenshot_get_images_all (screenshot);
	return priv->images_lang;
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

	if (as_utils_locale_is_compatible (as_image_get_locale (image), priv->active_locale))
		g_ptr_array_add (priv->images_lang, g_object_ref (image));
}

/**
 * as_screenshot_get_caption:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the image caption
 *
 * Returns: the caption
 **/
const gchar*
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
	guint i;

	g_free (priv->active_locale);
	priv->active_locale = g_strdup (locale);

	/* rebuild our list of images suitable for the current locale */
	g_ptr_array_unref (priv->images_lang);
	priv->images_lang = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->images->len; i++) {
		AsImage *img = g_ptr_array_index (priv->images, i);
		if (!as_utils_locale_is_compatible (as_image_get_locale (img), priv->active_locale))
			continue;
		g_ptr_array_add (priv->images_lang, g_object_ref (img));
	}
}

/**
 * as_screenshot_get_images_all:
 * @screenshot: an #AsScreenshot instance.
 *
 * Returns an array of all images we have, regardless of their
 * size and language.
 *
 * Returns: (transfer none) (element-type AsImage): an array
 *
 * Since: 0.10
 **/
GPtrArray*
as_screenshot_get_images_all (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->images;
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
 * as_screenshot_load_from_xml:
 * @screenshot: an #AsScreenshot instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads data from an XML node.
 **/
gboolean
as_screenshot_load_from_xml (AsScreenshot *screenshot, AsContext *ctx, xmlNode *node, GError **error)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	xmlNode *iter;
	g_autofree gchar *prop = NULL;

	/* propagate locale */
	as_screenshot_set_active_locale (screenshot, as_context_get_locale (ctx));

	prop = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	if (g_strcmp0 (prop, "default") == 0)
		priv->kind = AS_SCREENSHOT_KIND_DEFAULT;
	else
		priv->kind = AS_SCREENSHOT_KIND_EXTRA;

	for (iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		if (g_strcmp0 (node_name, "image") == 0) {
			g_autoptr(AsImage) image = as_image_new ();
			if (as_image_load_from_xml (image, ctx, iter, NULL))
				as_screenshot_add_image (screenshot, image);
		} else if (g_strcmp0 (node_name, "caption") == 0) {
			g_autofree gchar *content = NULL;
			g_autofree gchar *lang = NULL;

			content = as_xml_get_node_value (iter);
			if (content == NULL)
				continue;

			lang = as_xmldata_get_node_locale (ctx, iter);
			if (lang != NULL)
				as_screenshot_set_caption (screenshot, content, lang);
		}
	}

	return TRUE;
}

/**
 * as_screenshot_to_xml_node:
 * @screenshot: an #AsScreenshot instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_screenshot_to_xml_node (AsScreenshot *screenshot, AsContext *ctx, xmlNode *root)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	xmlNode *subnode;
	guint i;

	subnode = xmlNewChild (root, NULL, (xmlChar*) "screenshot", NULL);
	if (priv->kind == AS_SCREENSHOT_KIND_DEFAULT)
		xmlNewProp (subnode, (xmlChar*) "type", (xmlChar*) "default");

	as_xml_add_localized_text_node (subnode, "caption", priv->caption);

	for (i = 0; i < priv->images->len; i++) {
		AsImage *image = AS_IMAGE (g_ptr_array_index (priv->images, i));
		as_image_to_xml_node (image, ctx, subnode);
	}
}

/**
 * as_screenshot_load_from_yaml:
 * @screenshot: an #AsScreenshot instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_screenshot_load_from_yaml (AsScreenshot *screenshot, AsContext *ctx, GNode *node, GError **error)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GNode *n;

	/* propagate locale */
	as_screenshot_set_active_locale (screenshot, as_context_get_locale (ctx));

	for (n = node->children; n != NULL; n = n->next) {
		GNode *in;
		const gchar *key = as_yaml_node_get_key (n);
		const gchar *value = as_yaml_node_get_value (n);

		if (g_strcmp0 (key, "default") == 0) {
			if (g_strcmp0 (value, "yes") == 0)
				priv->kind = AS_SCREENSHOT_KIND_DEFAULT;
			else
				priv->kind = AS_SCREENSHOT_KIND_EXTRA;
		} else if (g_strcmp0 (key, "caption") == 0) {
			gchar *lvalue;
			/* the caption is a localized element */
			lvalue = as_yaml_get_localized_value (ctx, n, NULL);
			as_screenshot_set_caption (screenshot, lvalue, NULL);
		} else if (g_strcmp0 (key, "source-image") == 0) {
			/* there can only be one source image */
			g_autoptr(AsImage) image = as_image_new ();
			if (as_image_load_from_yaml (image, ctx, n, AS_IMAGE_KIND_SOURCE, NULL))
				as_screenshot_add_image (screenshot, image);
		} else if (g_strcmp0 (key, "thumbnails") == 0) {
			/* the thumbnails are a list of images */
			for (in = n->children; in != NULL; in = in->next) {
				g_autoptr(AsImage) image = as_image_new ();
				if (as_image_load_from_yaml (image, ctx, in, AS_IMAGE_KIND_THUMBNAIL, NULL))
					as_screenshot_add_image (screenshot, image);
			}
		} else {
			as_yaml_print_unknown ("screenshot", key);
		}
	}

	return TRUE;
}

/**
 * as_screenshot_emit_yaml:
 * @screenshot: an #AsScreenshot instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_screenshot_emit_yaml (AsScreenshot *screenshot, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	guint i;
	AsImage *source_img = NULL;

	as_yaml_mapping_start (emitter);

	if (priv->kind == AS_SCREENSHOT_KIND_DEFAULT)
		as_yaml_emit_entry (emitter, "default", "true");

	as_yaml_emit_localized_entry (emitter, "caption", priv->caption);

	as_yaml_emit_scalar (emitter, "thumbnails");
	as_yaml_sequence_start (emitter);
	for (i = 0; i < priv->images->len; i++) {
		AsImage *img = AS_IMAGE (g_ptr_array_index (priv->images, i));

		if (as_image_get_kind (img) == AS_IMAGE_KIND_SOURCE) {
			source_img = img;
			continue;
		}

		as_image_emit_yaml (img, ctx, emitter);
	}
	as_yaml_sequence_end (emitter);

	/* we *must* have a source-image by now if the data follows the spec, but better be safe... */
	if (source_img != NULL) {
		as_yaml_emit_scalar (emitter, "source-image");
		as_image_emit_yaml (source_img, ctx, emitter);
	}

	as_yaml_mapping_end (emitter);
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
