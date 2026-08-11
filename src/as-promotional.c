/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2024 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-promotional
 * @short_description: Object representing a single promotional banner
 *
 * Promotional banners are hero or banner images and videos used in
 * application stores and other contexts to present an application
 * attractively. They are distinct from screenshots in that they are
 * not required to be actual screen captures.
 *
 * The @contains_text attribute must be set explicitly to indicate whether
 * the promotional media contains text integral to understanding it. Its
 * absence triggers a validator warning.
 *
 * See also: #AsImage, #AsVideo, #AsScreenshot
 */

#include "as-promotional.h"
#include "as-promotional-private.h"

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-context-private.h"
#include "as-image-private.h"
#include "as-video-private.h"

typedef struct {
	AsPromotionalMediaKind	 media_kind;
	AsPromotionalContainsText contains_text;
	GRefString		*environment;
	GHashTable		*caption;

	GPtrArray		*images;
	GPtrArray		*images_lang;
	GPtrArray		*videos;
	GPtrArray		*videos_lang;

	gint			 position;
	AsContext		*context;
} AsPromotionalPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsPromotional, as_promotional, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_promotional_get_instance_private (o))

/**
 * as_promotional_init:
 **/
static void
as_promotional_init (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);

	priv->position = -1;
	priv->media_kind = AS_PROMOTIONAL_MEDIA_KIND_IMAGE;
	priv->contains_text = AS_PROMOTIONAL_CONTAINS_TEXT_UNKNOWN;
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
 * as_promotional_finalize:
 **/
static void
as_promotional_finalize (GObject *object)
{
	AsPromotional *promotional = AS_PROMOTIONAL (object);
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);

	g_ptr_array_unref (priv->images);
	g_ptr_array_unref (priv->images_lang);
	g_ptr_array_unref (priv->videos);
	g_ptr_array_unref (priv->videos_lang);
	g_hash_table_unref (priv->caption);
	as_ref_string_release (priv->environment);
	if (priv->context != NULL)
		g_object_unref (priv->context);

	G_OBJECT_CLASS (as_promotional_parent_class)->finalize (object);
}

/**
 * as_promotional_class_init:
 **/
static void
as_promotional_class_init (AsPromotionalClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_promotional_finalize;
}

/**
 * as_promotional_get_media_kind:
 * @promotional: a #AsPromotional instance.
 *
 * Gets the promotional media kind.
 *
 * Returns: a #AsPromotionalMediaKind
 **/
AsPromotionalMediaKind
as_promotional_get_media_kind (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return priv->media_kind;
}

/**
 * as_promotional_get_contains_text:
 * @promotional: a #AsPromotional instance.
 *
 * Gets whether this promotional item contains text that is integral
 * to understanding it.
 *
 * Returns: a #AsPromotionalContainsText
 **/
AsPromotionalContainsText
as_promotional_get_contains_text (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return priv->contains_text;
}

/**
 * as_promotional_set_contains_text:
 * @promotional: a #AsPromotional instance.
 * @contains_text: the #AsPromotionalContainsText value.
 *
 * Sets whether this promotional item contains text integral to understanding it.
 * This attribute must be set explicitly; its absence triggers a validator warning.
 **/
void
as_promotional_set_contains_text (AsPromotional *promotional,
				  AsPromotionalContainsText contains_text)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	priv->contains_text = contains_text;
}

/**
 * as_promotional_get_environment:
 * @promotional: a #AsPromotional instance.
 *
 * Get the GUI environment ID associated with this promotional, if any.
 * E.g. "plasma-mobile" or "gnome:dark".
 *
 * Returns: (nullable): The GUI environment ID, or %NULL if none set.
 **/
const gchar *
as_promotional_get_environment (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return priv->environment;
}

/**
 * as_promotional_set_environment:
 * @promotional: a #AsPromotional instance.
 * @env_id: (nullable): the GUI environment ID, e.g. "plasma-mobile" or "gnome:dark"
 *
 * Sets the GUI environment ID of this promotional.
 **/
void
as_promotional_set_environment (AsPromotional *promotional, const gchar *env_id)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	as_ref_string_assign_safe (&priv->environment, env_id);
}

/**
 * as_promotional_get_active_locale:
 *
 * Get the current active locale, which is used to get localized messages.
 */
static const gchar *
as_promotional_get_active_locale (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	const gchar *locale;

	/* ensure we have a context */
	if (priv->context == NULL) {
		g_autoptr(AsContext) context = as_context_new ();
		as_promotional_set_context (promotional, context);
	}

	locale = as_context_get_locale (priv->context);
	if (locale == NULL)
		return "C";
	else
		return locale;
}

/**
 * as_promotional_get_images:
 * @promotional: a #AsPromotional instance.
 *
 * Gets the images for this promotional. Only images valid for the current
 * language are returned.
 *
 * Returns: (transfer none) (element-type AsImage): an array
 **/
GPtrArray *
as_promotional_get_images (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	if (priv->images_lang->len == 0)
		return as_promotional_get_images_all (promotional);
	return priv->images_lang;
}

/**
 * as_promotional_get_image:
 * @promotional: a #AsPromotional instance.
 * @width: target width
 * @height: target height
 * @scale: the target scaling factor.
 *
 * Gets the AsImage closest to the target size. The #AsImage may not actually
 * be the requested size. Only images for the current active locale (or fallback)
 * are considered.
 *
 * Returns: (transfer none) (nullable): an #AsImage, or %NULL
 **/
AsImage *
as_promotional_get_image (AsPromotional *promotional, guint width, guint height, guint scale)
{
	AsImage *im_best = NULL;
	gint64 best_size = G_MAXINT64;
	GPtrArray *images;

	g_return_val_if_fail (AS_IS_PROMOTIONAL (promotional), NULL);
	g_return_val_if_fail (scale >= 1, NULL);

	images = as_promotional_get_images (promotional);
	for (guint current_scale = scale; current_scale > 0; current_scale--) {
		guint64 scaled_width;
		guint64 scaled_height;

		scaled_width = (guint64) width * current_scale;
		scaled_height = (guint64) height * current_scale;

		for (guint i = 0; i < images->len; i++) {
			gint64 tmp;
			AsImage *im = g_ptr_array_index (images, i);
			guint im_scale = as_image_get_scale (im);

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
 * as_promotional_add_image:
 * @promotional: a #AsPromotional instance.
 * @image: a #AsImage instance.
 *
 * Adds an image to the promotional.
 **/
void
as_promotional_add_image (AsPromotional *promotional, AsImage *image)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	g_ptr_array_add (priv->images, g_object_ref (image));

	if (as_utils_locale_is_compatible (as_image_get_locale (image),
					   as_promotional_get_active_locale (promotional)))
		g_ptr_array_add (priv->images_lang, g_object_ref (image));
}

/**
 * as_promotional_clear_images:
 * @promotional: a #AsPromotional instance.
 *
 * Remove all images associated with this promotional.
 **/
void
as_promotional_clear_images (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);

	g_ptr_array_remove_range (priv->images, 0, priv->images->len);
	g_ptr_array_remove_range (priv->images_lang, 0, priv->images_lang->len);
}

/**
 * as_promotional_get_videos_all:
 * @promotional: a #AsPromotional instance.
 *
 * Returns an array of all videos, regardless of locale.
 *
 * Returns: (transfer none) (element-type AsVideo): an array
 **/
GPtrArray *
as_promotional_get_videos_all (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return priv->videos;
}

/**
 * as_promotional_get_videos:
 * @promotional: a #AsPromotional instance.
 *
 * Gets the videos for this promotional. Only videos valid for the current
 * language selection are returned.
 *
 * Returns: (transfer none) (element-type AsVideo): an array
 **/
GPtrArray *
as_promotional_get_videos (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	if (priv->videos_lang->len == 0)
		return priv->videos;
	return priv->videos_lang;
}

/**
 * as_promotional_add_video:
 * @promotional: a #AsPromotional instance.
 * @video: a #AsVideo instance.
 *
 * Adds a video to the promotional.
 **/
void
as_promotional_add_video (AsPromotional *promotional, AsVideo *video)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	priv->media_kind = AS_PROMOTIONAL_MEDIA_KIND_VIDEO;
	g_ptr_array_add (priv->videos, g_object_ref (video));

	if (as_utils_locale_is_compatible (as_video_get_locale (video),
					   as_promotional_get_active_locale (promotional)))
		g_ptr_array_add (priv->videos_lang, g_object_ref (video));
}

/**
 * as_promotional_get_caption:
 * @promotional: a #AsPromotional instance.
 *
 * Gets the promotional caption.
 *
 * Returns: (nullable): the caption, or %NULL if not set.
 **/
const gchar *
as_promotional_get_caption (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return as_context_localized_ht_get (priv->context,
					    priv->caption,
					    NULL /* locale override */);
}

/**
 * as_promotional_set_caption:
 * @promotional: a #AsPromotional instance.
 * @caption: the caption text.
 * @locale: (nullable): the locale, or %NULL for the current locale.
 *
 * Sets a caption on the promotional.
 **/
void
as_promotional_set_caption (AsPromotional *promotional,
			    const gchar   *caption,
			    const gchar   *locale)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	as_context_localized_ht_set (priv->context, priv->caption, caption, locale);
}

/**
 * as_promotional_is_valid:
 * @promotional: a #AsPromotional instance.
 *
 * Performs a quick validation on this promotional.
 *
 * Returns: %TRUE if the promotional is a valid #AsPromotional
 **/
gboolean
as_promotional_is_valid (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return priv->images->len > 0 || priv->videos->len > 0;
}

/**
 * as_promotional_rebuild_suitable_media_list:
 *
 * Rebuild lists of images or videos suitable for the selected locale.
 */
static void
as_promotional_rebuild_suitable_media_list (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	gboolean all_locale = FALSE;
	const gchar *active_locale = as_promotional_get_active_locale (promotional);

	all_locale = as_context_get_locale_use_all (priv->context);

	g_ptr_array_unref (priv->images_lang);
	priv->images_lang = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (guint i = 0; i < priv->images->len; i++) {
		AsImage *img = AS_IMAGE (g_ptr_array_index (priv->images, i));
		if (!all_locale &&
		    !as_utils_locale_is_compatible (as_image_get_locale (img), active_locale))
			continue;
		g_ptr_array_add (priv->images_lang, g_object_ref (img));
	}

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
 * as_promotional_set_context_locale:
 * @promotional: a #AsPromotional instance.
 * @locale: the new locale.
 *
 * Set the active locale on the context associated with this promotional,
 * creating a new context if none exists yet.
 */
void
as_promotional_set_context_locale (AsPromotional *promotional, const gchar *locale)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);

	if (priv->context == NULL) {
		g_autoptr(AsContext) context = as_context_new ();
		as_promotional_set_context (promotional, context);
	}
	as_context_set_locale (priv->context, locale);
	as_promotional_rebuild_suitable_media_list (promotional);
}

/**
 * as_promotional_get_images_all:
 * @promotional: a #AsPromotional instance.
 *
 * Returns an array of all images, regardless of size and language.
 *
 * Returns: (transfer none) (element-type AsImage): an array
 **/
GPtrArray *
as_promotional_get_images_all (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return priv->images;
}

/**
 * as_promotional_get_context:
 * @promotional: a #AsPromotional instance.
 *
 * Returns the #AsContext associated with this promotional.
 *
 * Returns: (transfer none) (nullable): the #AsContext used by this promotional.
 **/
AsContext *
as_promotional_get_context (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return priv->context;
}

/**
 * as_promotional_set_context:
 * @promotional: a #AsPromotional instance.
 * @context: the #AsContext.
 *
 * Sets the document context this promotional is associated with.
 **/
void
as_promotional_set_context (AsPromotional *promotional, AsContext *context)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	if (priv->context != NULL)
		g_object_unref (priv->context);

	if (context == NULL)
		priv->context = NULL;
	else
		priv->context = g_object_ref (context);

	as_promotional_rebuild_suitable_media_list (promotional);
}

/**
 * as_promotional_get_position:
 * @promotional: a #AsPromotional instance.
 *
 * Gets the ordering priority of this promotional.
 *
 * Returns: the position, or -1 if unknown.
 **/
gint
as_promotional_get_position (AsPromotional *promotional)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	return priv->position;
}

/**
 * as_promotional_set_position:
 * @promotional: a #AsPromotional instance.
 * @pos: the position.
 *
 * Sets the ordering priority of this promotional in the promotionals list.
 **/
void
as_promotional_set_position (AsPromotional *promotional, gint pos)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	priv->position = pos;
}

/**
 * as_promotional_load_from_xml:
 * @promotional: a #AsPromotional instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads data from an XML node.
 **/
gboolean
as_promotional_load_from_xml (AsPromotional *promotional,
			      AsContext     *ctx,
			      xmlNode	    *node,
			      GError	   **error)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	xmlNode *iter;
	g_autofree gchar *prop = NULL;

	/* contains-text attribute */
	prop = as_xml_get_prop_value (node, "contains-text");
	if (g_strcmp0 (prop, "true") == 0)
		priv->contains_text = AS_PROMOTIONAL_CONTAINS_TEXT_YES;
	else if (g_strcmp0 (prop, "false") == 0)
		priv->contains_text = AS_PROMOTIONAL_CONTAINS_TEXT_NO;
	else
		priv->contains_text = AS_PROMOTIONAL_CONTAINS_TEXT_UNKNOWN;
	g_clear_pointer (&prop, g_free);

	/* environment attribute */
	as_ref_string_assign_transfer (&priv->environment,
				       as_xml_get_prop_value_refstr (node, "environment"));

	/* promotional media children */
	for (iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar *) iter->name;

		if (g_strcmp0 (node_name, "image") == 0) {
			g_autoptr(AsImage) image = as_image_new ();
			if (as_image_load_from_xml (image, ctx, iter, NULL))
				as_promotional_add_image (promotional, image);
		} else if (g_strcmp0 (node_name, "video") == 0) {
			g_autoptr(AsVideo) video = as_video_new ();
			if (as_video_load_from_xml (video, ctx, iter, NULL))
				as_promotional_add_video (promotional, video);
		} else if (g_strcmp0 (node_name, "caption") == 0) {
			g_autofree gchar *content = NULL;
			g_autofree gchar *lang = NULL;

			content = as_xml_get_node_value (iter);
			if (content == NULL)
				continue;

			lang = as_xml_get_node_locale_match (ctx, iter);
			if (lang != NULL)
				as_promotional_set_caption (promotional, content, lang);
		}
	}

	/* propagate context last so the image list for the selected locale is rebuilt properly */
	as_promotional_set_context (promotional, ctx);

	return TRUE;
}

/**
 * as_promotional_to_xml_node:
 * @promotional: a #AsPromotional instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_promotional_to_xml_node (AsPromotional *promotional, AsContext *ctx, xmlNode *root)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	xmlNode *subnode;

	subnode = as_xml_add_node (root, "promotional");

	if (priv->contains_text == AS_PROMOTIONAL_CONTAINS_TEXT_YES)
		as_xml_add_text_prop (subnode, "contains-text", "true");
	else if (priv->contains_text == AS_PROMOTIONAL_CONTAINS_TEXT_NO)
		as_xml_add_text_prop (subnode, "contains-text", "false");

	if (priv->environment != NULL)
		as_xml_add_text_prop (subnode, "environment", priv->environment);

	as_xml_add_localized_text_node (subnode, "caption", priv->caption);

	if (priv->media_kind == AS_PROMOTIONAL_MEDIA_KIND_IMAGE) {
		for (guint i = 0; i < priv->images->len; i++) {
			AsImage *image = AS_IMAGE (g_ptr_array_index (priv->images, i));
			as_image_to_xml_node (image, ctx, subnode);
		}
	} else if (priv->media_kind == AS_PROMOTIONAL_MEDIA_KIND_VIDEO) {
		for (guint i = 0; i < priv->videos->len; i++) {
			AsVideo *video = AS_VIDEO (g_ptr_array_index (priv->videos, i));
			as_video_to_xml_node (video, ctx, subnode);
		}
	}
}

/**
 * as_promotional_load_from_yaml:
 * @promotional: a #AsPromotional instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_promotional_load_from_yaml (AsPromotional  *promotional,
			       AsContext       *ctx,
			       struct fy_node  *node,
			       GError	      **error)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);

	AS_YAML_MAPPING_FOREACH (npair, node) {
		const gchar *key = as_yaml_node_get_key0 (npair);
		struct fy_node *nval = fy_node_pair_value (npair);

		if (g_strcmp0 (key, "contains-text") == 0) {
			const gchar *value = as_yaml_node_get_value0 (npair);
			if (g_strcmp0 (value, "true") == 0)
				priv->contains_text = AS_PROMOTIONAL_CONTAINS_TEXT_YES;
			else if (g_strcmp0 (value, "false") == 0)
				priv->contains_text = AS_PROMOTIONAL_CONTAINS_TEXT_NO;
			else
				priv->contains_text = AS_PROMOTIONAL_CONTAINS_TEXT_UNKNOWN;
		} else if (g_strcmp0 (key, "environment") == 0) {
			as_ref_string_assign_transfer (&priv->environment,
						       as_yaml_node_get_value_refstr (npair));
		} else if (g_strcmp0 (key, "caption") == 0) {
			as_yaml_set_localized_table (ctx, nval, priv->caption);
		} else if (g_strcmp0 (key, "source-image") == 0) {
			g_autoptr(AsImage) image = as_image_new ();
			if (as_image_load_from_yaml (image, ctx, nval, AS_IMAGE_KIND_SOURCE, NULL))
				as_promotional_add_image (promotional, image);
		} else if (g_strcmp0 (key, "thumbnails") == 0) {
			AS_YAML_SEQUENCE_FOREACH (in, nval) {
				g_autoptr(AsImage) image = as_image_new ();
				if (as_image_load_from_yaml (image,
							     ctx,
							     in,
							     AS_IMAGE_KIND_THUMBNAIL,
							     NULL))
					as_promotional_add_image (promotional, image);
			}
		} else if (g_strcmp0 (key, "videos") == 0) {
			AS_YAML_SEQUENCE_FOREACH (vn, nval) {
				g_autoptr(AsVideo) video = as_video_new ();
				if (as_video_load_from_yaml (video, ctx, vn, NULL))
					as_promotional_add_video (promotional, video);
			}
		} else {
			as_yaml_print_unknown ("promotional", key, -1);
		}
	}

	/* propagate context last so the image list for the selected locale is rebuilt properly */
	as_promotional_set_context (promotional, ctx);

	return TRUE;
}

/**
 * as_promotional_emit_yaml:
 * @promotional: a #AsPromotional instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_promotional_emit_yaml (AsPromotional	      *promotional,
			  AsContext	      *ctx,
			  struct fy_emitter   *emitter)
{
	AsPromotionalPrivate *priv = GET_PRIVATE (promotional);
	AsImage *source_img = NULL;

	as_yaml_mapping_start (emitter);

	if (priv->contains_text == AS_PROMOTIONAL_CONTAINS_TEXT_YES)
		as_yaml_emit_entry (emitter, "contains-text", "true");
	else if (priv->contains_text == AS_PROMOTIONAL_CONTAINS_TEXT_NO)
		as_yaml_emit_entry (emitter, "contains-text", "false");

	if (priv->environment != NULL)
		as_yaml_emit_entry (emitter, "environment", priv->environment);

	as_yaml_emit_localized_entry (emitter, "caption", priv->caption);

	if (priv->media_kind == AS_PROMOTIONAL_MEDIA_KIND_IMAGE) {
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

		if (source_img != NULL) {
			as_yaml_emit_scalar (emitter, "source-image");
			as_image_emit_yaml (source_img, ctx, emitter);
		}
	} else if (priv->media_kind == AS_PROMOTIONAL_MEDIA_KIND_VIDEO) {
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
 * as_promotional_new:
 *
 * Creates a new #AsPromotional.
 *
 * Returns: (transfer full): a #AsPromotional
 **/
AsPromotional *
as_promotional_new (void)
{
	AsPromotional *promotional;
	promotional = g_object_new (AS_TYPE_PROMOTIONAL, NULL);
	return AS_PROMOTIONAL (promotional);
}
