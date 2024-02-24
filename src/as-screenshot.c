/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
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
 * Screenshots have a localized caption and contain either a set of images
 * of different resolution or a video screencast showcasing the software.
 *
 * See also: #AsImage, #AsVideo
 */

#include "as-screenshot.h"
#include "as-screenshot-private.h"

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-context-private.h"
#include "as-image-private.h"
#include "as-video-private.h"

typedef struct {
	AsScreenshotKind kind;
	AsScreenshotMediaKind media_kind;
	GRefString *environment;
	GHashTable *caption;

	GPtrArray *images;
	GPtrArray *images_lang;
	GPtrArray *videos;
	GPtrArray *videos_lang;

	gint position;
	AsContext *context;
} AsScreenshotPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsScreenshot, as_screenshot, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_screenshot_get_instance_private (o))

/**
 * as_screenshot_init:
 **/
static void
as_screenshot_init (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	priv->position = -1;
	priv->kind = AS_SCREENSHOT_KIND_EXTRA;
	priv->media_kind = AS_SCREENSHOT_MEDIA_KIND_IMAGE;
	priv->caption = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       (GDestroyNotify) as_ref_string_release,
					       g_free);
	priv->images = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->images_lang = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->videos = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->videos_lang = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * as_screenshot_finalize:
 **/
static void
as_screenshot_finalize (GObject *object)
{
	AsScreenshot *screenshot = AS_SCREENSHOT (object);
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	g_ptr_array_unref (priv->images);
	g_ptr_array_unref (priv->images_lang);
	g_ptr_array_unref (priv->videos);
	g_ptr_array_unref (priv->videos_lang);
	g_hash_table_unref (priv->caption);
	as_ref_string_release (priv->environment);
	if (priv->context != NULL)
		g_object_unref (priv->context);

	G_OBJECT_CLASS (as_screenshot_parent_class)->finalize (object);
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
	if ((g_strcmp0 (kind, "") == 0) || (kind == NULL))
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
 * as_screenshot_get_media_kind:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the screenshot media kind.
 *
 * Returns: a #AsScreenshotMediaKind
 **/
AsScreenshotMediaKind
as_screenshot_get_media_kind (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->media_kind;
}

/**
 * as_screenshot_get_environment:
 * @screenshot: a #AsScreenshot instance.
 *
 * Get the GUI environment ID of this screenshot, if any
 * is associated with it. E.g. "plasma-mobile" or "gnome:dark".
 *
 * Returns: (nullable): The GUI environment ID the screenshot was recorded in, or %NULL if none set.
 **/
const gchar *
as_screenshot_get_environment (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->environment;
}

/**
 * as_screenshot_set_environment:
 * @screenshot: a #AsScreenshot instance.
 * @env_id: (nullable): the GUI environment ID, e.g. "plasma-mobile" or "gnome:dark"
 *
 * Sets the GUI environment ID of this screenshot.
 **/
void
as_screenshot_set_environment (AsScreenshot *screenshot, const gchar *env_id)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	as_ref_string_assign_safe (&priv->environment, env_id);
}

/**
 * as_screenshot_get_active_locale:
 *
 * Get the current active locale, which
 * is used to get localized messages.
 */
static const gchar *
as_screenshot_get_active_locale (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	const gchar *locale;

	/* ensure we have a context */
	if (priv->context == NULL) {
		g_autoptr(AsContext) context = as_context_new ();
		as_screenshot_set_context (screenshot, context);
	}

	locale = as_context_get_locale (priv->context);
	if (locale == NULL)
		return "C";
	else
		return locale;
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
GPtrArray *
as_screenshot_get_images (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	if (priv->images_lang->len == 0)
		return as_screenshot_get_images_all (screenshot);
	return priv->images_lang;
}

/**
 * as_screenshot_get_image:
 * @screenshot: a #AsScreenshot instance.
 * @width: target width
 * @height: target height
 * @scale: the target scaling factor.
 *
 * Gets the AsImage closest to the target size. The #AsImage may not actually
 * be the requested size, and the application may have to pad / rescale the
 * image to make it fit.
 * Only images for the current active locale (or fallback, if images are not localized)
 * are considered.
 *
 * Returns: (transfer none) (nullable): an #AsImage, or %NULL
 *
 * Since: 0.14.0
 **/
AsImage *
as_screenshot_get_image (AsScreenshot *screenshot, guint width, guint height, guint scale)
{
	AsImage *im_best = NULL;
	gint64 best_size = G_MAXINT64;
	GPtrArray *images;

	g_return_val_if_fail (AS_IS_SCREENSHOT (screenshot), NULL);
	g_return_val_if_fail (scale >= 1, NULL);

	images = as_screenshot_get_images (screenshot);
	for (guint current_scale = scale; current_scale > 0; current_scale--) {
		guint64 scaled_width;
		guint64 scaled_height;

		scaled_width = (guint64) width * current_scale;
		scaled_height = (guint64) height * current_scale;

		for (guint i = 0; i < images->len; i++) {
			gint64 tmp;
			AsImage *im = g_ptr_array_index (images, i);
			guint im_scale = as_image_get_scale (im);

			/* skip image if we aren't looking into its scaling factor yet */
			if (im_scale != current_scale)
				continue;

			tmp = ABS ((gint64) (scaled_width * scaled_height) -
				   (gint64) (as_image_get_width (im) * as_image_get_height (im)));
			if (tmp < best_size) {
				best_size = tmp;
				im_best = im;
			}
		}
	}

	return im_best;
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

	if (as_utils_locale_is_compatible (as_image_get_locale (image),
					   as_screenshot_get_active_locale (screenshot)))
		g_ptr_array_add (priv->images_lang, g_object_ref (image));
}

/**
 * as_screenshot_clear_images:
 * @screenshot: a #AsScreenshot instance.
 *
 * Remove all images associated with this screenshot.
 *
 * Since: 0.15.4
 **/
void
as_screenshot_clear_images (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	g_ptr_array_remove_range (priv->images, 0, priv->images->len);
	g_ptr_array_remove_range (priv->images_lang, 0, priv->images_lang->len);
}

/**
 * as_screenshot_get_videos_all:
 * @screenshot: a #AsScreenshot instance.
 *
 * Returns an array of all screencast videos we have, regardless of their
 * size and locale.
 *
 * Returns: (transfer none) (element-type AsVideo): an array
 **/
GPtrArray *
as_screenshot_get_videos_all (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->images;
}

/**
 * as_screenshot_get_videos:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the videos for this screenshots. Only videos valid for the current
 * language selection are returned. We return all sizes.
 *
 * Returns: (transfer none) (element-type AsVideo): an array
 **/
GPtrArray *
as_screenshot_get_videos (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	if (priv->videos_lang->len == 0)
		return priv->videos;
	return priv->videos_lang;
}

/**
 * as_screenshot_add_video:
 * @screenshot: a #AsScreenshot instance.
 * @video: a #AsVideo instance.
 *
 * Adds a video to the screenshot.
 **/
void
as_screenshot_add_video (AsScreenshot *screenshot, AsVideo *video)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->media_kind = AS_SCREENSHOT_MEDIA_KIND_VIDEO;
	g_ptr_array_add (priv->videos, g_object_ref (video));

	if (as_utils_locale_is_compatible (as_video_get_locale (video),
					   as_screenshot_get_active_locale (screenshot)))
		g_ptr_array_add (priv->videos_lang, g_object_ref (video));
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
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return as_context_localized_ht_get (priv->context,
					    priv->caption,
					    NULL /* locale override */);
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
	as_context_localized_ht_set (priv->context, priv->caption, caption, locale);
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
 * as_screenshot_rebuild_suitable_media_list:
 * @screenshot: a #AsScreenshot instance.
 *
 * Rebuild list of images or videos suitable for the selected locale.
 */
static void
as_screenshot_rebuild_suitable_media_list (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	gboolean all_locale = FALSE;
	const gchar *active_locale = as_screenshot_get_active_locale (screenshot);

	all_locale = as_context_get_locale_use_all (priv->context);

	/* rebuild our list of images suitable for the current locale */
	g_ptr_array_unref (priv->images_lang);
	priv->images_lang = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (guint i = 0; i < priv->images->len; i++) {
		AsImage *img = AS_IMAGE (g_ptr_array_index (priv->images, i));
		if (!all_locale &&
		    !as_utils_locale_is_compatible (as_image_get_locale (img), active_locale))
			continue;
		g_ptr_array_add (priv->images_lang, g_object_ref (img));
	}

	/* rebuild our list of videos suitable for the current locale */
	g_ptr_array_unref (priv->videos_lang);
	priv->videos_lang = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (guint i = 0; i < priv->videos->len; i++) {
		AsVideo *vid = AS_VIDEO (g_ptr_array_index (priv->videos, i));
		if (!all_locale &&
		    !as_utils_locale_is_compatible (as_video_get_locale (vid), active_locale))
			continue;
		g_ptr_array_add (priv->videos_lang, g_object_ref (vid));
	}
}

/**
 * as_screenshot_set_context_locale:
 * @cpt: a #AsComponent instance.
 * @locale: the new locale.
 *
 * Set the active locale on the context assoaiacted with this screenshot,
 * creating a new context for the screenshot if none exists yet.
 *
 * Please not that this will flip the locale of all other components and
 * entities that use the same context as well!
 */
void
as_screenshot_set_context_locale (AsScreenshot *screenshot, const gchar *locale)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	if (priv->context == NULL) {
		g_autoptr(AsContext) context = as_context_new ();
		as_screenshot_set_context (screenshot, context);
	}
	as_context_set_locale (priv->context, locale);
	as_screenshot_rebuild_suitable_media_list (screenshot);
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
GPtrArray *
as_screenshot_get_images_all (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->images;
}

/**
 * as_screenshot_get_context:
 * @screenshot: an #AsScreenshot instance.
 *
 * Returns the #AsContext associated with this screenshot.
 * This function may return %NULL if no context is set.
 *
 * Returns: (transfer none) (nullable): the #AsContext used by this screenshot.
 *
 * Since: 0.11.2
 */
AsContext *
as_screenshot_get_context (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->context;
}

/**
 * as_screenshot_set_context:
 * @screenshot: an #AsScreenshot instance.
 * @context: the #AsContext.
 *
 * Sets the document context this screenshot is associated
 * with.
 *
 * Since: 0.11.2
 */
void
as_screenshot_set_context (AsScreenshot *screenshot, AsContext *context)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	if (priv->context != NULL)
		g_object_unref (priv->context);

	if (context == NULL)
		priv->context = NULL;
	else
		priv->context = g_object_ref (context);

	as_screenshot_rebuild_suitable_media_list (screenshot);
}

/**
 * as_screenshot_get_position:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the ordering priority of this screenshot.
 *
 * Returns: the position of this screenshot in the screenshot list, or -1 if unknown.
 **/
gint
as_screenshot_get_position (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->position;
}

/**
 * as_screenshot_set_position:
 * @screenshot: a #AsScreenshot instance.
 *
 * Sets the ordering priority / screenshot list position of
 * this screenshot.
 **/
void
as_screenshot_set_position (AsScreenshot *screenshot, gint pos)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->position = pos;
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
as_screenshot_load_from_xml (AsScreenshot *screenshot,
			     AsContext *ctx,
			     xmlNode *node,
			     GError **error)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	xmlNode *iter;
	g_autofree gchar *prop = NULL;
	gboolean children_found = FALSE;

	/* screenshot type */
	prop = as_xml_get_prop_value (node, "type");
	if (g_strcmp0 (prop, "default") == 0)
		priv->kind = AS_SCREENSHOT_KIND_DEFAULT;
	else
		priv->kind = AS_SCREENSHOT_KIND_EXTRA;

	/* environment */
	as_ref_string_assign_transfer (&priv->environment,
				       as_xml_get_prop_value_refstr (node, "environment"));

	/* screenshot media */
	for (iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar *) iter->name;
		children_found = TRUE;

		if (g_strcmp0 (node_name, "image") == 0) {
			g_autoptr(AsImage) image = as_image_new ();
			if (as_image_load_from_xml (image, ctx, iter, NULL))
				as_screenshot_add_image (screenshot, image);
		} else if (g_strcmp0 (node_name, "video") == 0) {
			g_autoptr(AsVideo) video = as_video_new ();
			if (as_video_load_from_xml (video, ctx, iter, NULL))
				as_screenshot_add_video (screenshot, video);
		} else if (g_strcmp0 (node_name, "caption") == 0) {
			g_autofree gchar *content = NULL;
			g_autofree gchar *lang = NULL;

			content = as_xml_get_node_value (iter);
			if (content == NULL)
				continue;

			lang = as_xml_get_node_locale_match (ctx, iter);
			if (lang != NULL)
				as_screenshot_set_caption (screenshot, content, lang);
		}
	}

	if (!children_found) {
		/* we are likely dealing with a legacy screenshot node, which does not have <image/> children,
		 * but instead contains the screenshot URL as text. This was briefly supported in an older AppStream
		 * version for metainfo files, but it should no longer be used.
		 * We support it here only for legacy compatibility. */
		g_autoptr(AsImage) image = as_image_new ();

		if (as_image_load_from_xml (image, ctx, node, NULL))
			as_screenshot_add_image (screenshot, image);
		else
			return FALSE; /* this screenshot is invalid */
	}

	/* propagate context - we do this last so the image list for the selected locale is rebuilt properly */
	as_screenshot_set_context (screenshot, ctx);

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

	subnode = as_xml_add_node (root, "screenshot");
	if (priv->kind == AS_SCREENSHOT_KIND_DEFAULT)
		as_xml_add_text_prop (subnode, "type", "default");
	if (priv->environment != NULL)
		as_xml_add_text_prop (subnode, "environment", priv->environment);

	as_xml_add_localized_text_node (subnode, "caption", priv->caption);

	if (priv->media_kind == AS_SCREENSHOT_MEDIA_KIND_IMAGE) {
		for (guint i = 0; i < priv->images->len; i++) {
			AsImage *image = AS_IMAGE (g_ptr_array_index (priv->images, i));
			as_image_to_xml_node (image, ctx, subnode);
		}
	} else if (priv->media_kind == AS_SCREENSHOT_MEDIA_KIND_VIDEO) {
		for (guint i = 0; i < priv->videos->len; i++) {
			AsVideo *video = AS_VIDEO (g_ptr_array_index (priv->videos, i));
			as_video_to_xml_node (video, ctx, subnode);
		}
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

	for (n = node->children; n != NULL; n = n->next) {
		GNode *in;
		const gchar *key = as_yaml_node_get_key (n);
		const gchar *value = as_yaml_node_get_value (n);

		if (g_strcmp0 (key, "default") == 0) {
			if ((g_strcmp0 (value, "true") == 0) || (g_strcmp0 (value, "yes") == 0))
				priv->kind = AS_SCREENSHOT_KIND_DEFAULT;
			else
				priv->kind = AS_SCREENSHOT_KIND_EXTRA;
		} else if (g_strcmp0 (key, "environment") == 0) {
			as_ref_string_assign_safe (&priv->environment, value);
		} else if (g_strcmp0 (key, "caption") == 0) {
			/* the caption is a localized element */
			as_yaml_set_localized_table (ctx, n, priv->caption);
		} else if (g_strcmp0 (key, "source-image") == 0) {
			/* there can only be one source image */
			g_autoptr(AsImage) image = as_image_new ();
			if (as_image_load_from_yaml (image, ctx, n, AS_IMAGE_KIND_SOURCE, NULL))
				as_screenshot_add_image (screenshot, image);
		} else if (g_strcmp0 (key, "thumbnails") == 0) {
			/* the thumbnails are a list of images */
			for (in = n->children; in != NULL; in = in->next) {
				g_autoptr(AsImage) image = as_image_new ();
				if (as_image_load_from_yaml (image,
							     ctx,
							     in,
							     AS_IMAGE_KIND_THUMBNAIL,
							     NULL))
					as_screenshot_add_image (screenshot, image);
			}
		} else if (g_strcmp0 (key, "videos") == 0) {
			for (in = n->children; in != NULL; in = in->next) {
				g_autoptr(AsVideo) video = as_video_new ();
				if (as_video_load_from_yaml (video, ctx, in, NULL))
					as_screenshot_add_video (screenshot, video);
			}
		} else {
			as_yaml_print_unknown ("screenshot", key);
		}
	}

	/* propagate context - we do this last so the image list for the selected locale is rebuilt properly */
	as_screenshot_set_context (screenshot, ctx);

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
	AsImage *source_img = NULL;

	as_yaml_mapping_start (emitter);

	if (priv->kind == AS_SCREENSHOT_KIND_DEFAULT)
		as_yaml_emit_entry (emitter, "default", "true");
	if (priv->environment != NULL)
		as_yaml_emit_entry (emitter, "environment", priv->environment);

	as_yaml_emit_localized_entry (emitter, "caption", priv->caption);

	if (priv->media_kind == AS_SCREENSHOT_MEDIA_KIND_IMAGE) {
		as_yaml_emit_scalar (emitter, "thumbnails");
		as_yaml_sequence_start (emitter);
		for (guint i = 0; i < priv->images->len; i++) {
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
	} else if (priv->media_kind == AS_SCREENSHOT_MEDIA_KIND_VIDEO) {
		as_yaml_emit_scalar (emitter, "videos");
		as_yaml_sequence_start (emitter);
		for (guint i = 0; i < priv->videos->len; i++) {
			AsVideo *video = AS_VIDEO (g_ptr_array_index (priv->videos, i));
			as_video_emit_yaml (video, ctx, emitter);
		}
		as_yaml_sequence_end (emitter);
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
