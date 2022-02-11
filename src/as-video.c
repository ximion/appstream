/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-video
 * @short_description: Object representing a video used in a screenshot.
 *
 * Screenshot may have a video instead of a static image associated with them.
 * This object allows access to the video and basic information about it.
 *
 * See also: #AsScreenshot, #AsImage
 */

#include "config.h"
#include "as-video.h"
#include "as-video-private.h"

#include "as-utils-private.h"

typedef struct
{
	AsVideoCodecKind	codec;
	AsVideoContainerKind 	container;

	gchar		*url;
	guint		width;
	guint		height;
	gchar		*locale;
} AsVideoPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsVideo, as_video, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_video_get_instance_private (o))

/**
 * as_video_finalize:
 **/
static void
as_video_finalize (GObject *object)
{
	AsVideo *video = AS_VIDEO (object);
	AsVideoPrivate *priv = GET_PRIVATE (video);

	g_free (priv->url);
	g_free (priv->locale);

	G_OBJECT_CLASS (as_video_parent_class)->finalize (object);
}

/**
 * as_video_init:
 **/
static void
as_video_init (AsVideo *video)
{
}

/**
 * as_video_codec_kind_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsVideoCodecKind, or %AS_VIDEO_CODEC_KIND_UNKNOWN for unknown.
 *
 **/
AsVideoCodecKind
as_video_codec_kind_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "av1") == 0)
		return AS_VIDEO_CODEC_KIND_AV1;
	if (g_strcmp0 (str, "vp9") == 0)
		return AS_VIDEO_CODEC_KIND_VP9;
	return AS_VIDEO_CODEC_KIND_UNKNOWN;
}

/**
 * as_video_codec_kind_to_string:
 * @kind: the #AsVideoCodecKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @codec
 *
 **/
const gchar*
as_video_codec_kind_to_string (AsVideoCodecKind kind)
{
	if (kind == AS_VIDEO_CODEC_KIND_AV1)
		return "av1";
	if (kind == AS_VIDEO_CODEC_KIND_VP9)
		return "vp9";
	return NULL;
}

/**
 * as_video_container_kind_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsVideoContainerKind, or %AS_VIDEO_CONTAINER_KIND_UNKNOWN for unknown.
 *
 **/
AsVideoContainerKind
as_video_container_kind_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "matroska") == 0)
		return AS_VIDEO_CONTAINER_KIND_MKV;
	if (g_strcmp0 (str, "webm") == 0)
		return AS_VIDEO_CONTAINER_KIND_WEBM;
	if (g_strcmp0 (str, "mkv") == 0)
		return AS_VIDEO_CONTAINER_KIND_MKV;
	return AS_VIDEO_CONTAINER_KIND_UNKNOWN;
}

/**
 * as_video_container_kind_to_string:
 * @kind: the #AsVideoContainerKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 **/
const gchar*
as_video_container_kind_to_string (AsVideoContainerKind kind)
{
	if (kind == AS_VIDEO_CONTAINER_KIND_MKV)
		return "matroska";
	if (kind == AS_VIDEO_CONTAINER_KIND_WEBM)
		return "webm";
	return NULL;
}

/**
 * as_video_set_codec_kind:
 * @video: a #AsVideo instance.
 * @kind: the #AsVideoCodecKind, e.g. %AS_VIDEO_CODEC_KIND_AV1.
 *
 * Sets the video codec.
 *
 **/
void
as_video_set_codec_kind (AsVideo *video, AsVideoCodecKind kind)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	priv->codec = kind;
}

/**
 * as_video_get_codec_kind:
 * @video: a #AsVideo instance.
 *
 * Gets the video codec, if known.
 *
 * Returns: the #AsVideoCodecKind
 *
 **/
AsVideoCodecKind
as_video_get_codec_kind (AsVideo *video)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	return priv->codec;
}

/**
 * as_video_set_container_kind:
 * @video: a #AsVideo instance.
 * @kind: the #AsVideoContainerKind, e.g. %AS_VIDEO_CONTAINER_KIND_MKV.
 *
 * Sets the video container.
 *
 **/
void
as_video_set_container_kind (AsVideo *video, AsVideoContainerKind kind)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	priv->container = kind;
}

/**
 * as_video_get_container_kind:
 * @video: a #AsVideo instance.
 *
 * Gets the video container format, if known.
 *
 * Returns: the #AsVideoContainerKind
 *
 **/
AsVideoContainerKind
as_video_get_container_kind (AsVideo *video)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	return priv->container;
}

/**
 * as_video_get_url:
 * @video: a #AsVideo instance.
 *
 * Gets the full qualified URL for the video, usually pointing at a mirror or CDN server.
 *
 * Returns: a web URL
 *
 **/
const gchar*
as_video_get_url (AsVideo *video)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	return priv->url;
}

/**
 * as_video_set_url:
 * @video: a #AsVideo instance.
 * @url: the URL.
 *
 * Sets the fully-qualified URL to use for the video.
 *
 **/
void
as_video_set_url (AsVideo *video, const gchar *url)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	as_assign_string_safe (priv->url, url);
}

/**
 * as_video_get_width:
 * @video: a #AsVideo instance.
 *
 * Gets the video width, if known.
 *
 * Returns: width in pixels or 0 if unknown
 *
 **/
guint
as_video_get_width (AsVideo *video)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	return priv->width;
}

/**
 * as_video_set_width:
 * @video: a #AsVideo instance.
 * @width: the width in pixels.
 *
 * Sets the video width.
 *
 **/
void
as_video_set_width (AsVideo *video, guint width)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	priv->width = width;
}

/**
 * as_video_get_height:
 * @video: a #AsVideo instance.
 *
 * Gets the video height, if known.
 *
 * Returns: height in pixels or 0 if unknown
 *
 **/
guint
as_video_get_height (AsVideo *video)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	return priv->height;
}

/**
 * as_video_set_height:
 * @video: a #AsVideo instance.
 * @height: the height in pixels.
 *
 * Sets the video height.
 *
 **/
void
as_video_set_height (AsVideo *video, guint height)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	priv->height = height;
}

/**
 * as_video_get_locale:
 * @video: a #AsVideo instance.
 *
 * Get locale for this video.
 *
 * Returns: Locale string
 **/
const gchar*
as_video_get_locale (AsVideo *video)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	return priv->locale;
}

/**
 * as_video_set_locale:
 * @video: a #AsVideo instance.
 * @locale: the locale string.
 *
 * Sets the locale for this video.
 **/
void
as_video_set_locale (AsVideo *video, const gchar *locale)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	as_assign_string_safe (priv->locale, locale);
}

/**
 * as_video_load_from_xml:
 * @video: a #AsVideo instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads video data from an XML node.
 **/
gboolean
as_video_load_from_xml (AsVideo *video, AsContext *ctx, xmlNode *node, GError **error)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	g_autofree gchar *content = NULL;
	g_autofree gchar *codec_kind = NULL;
	g_autofree gchar *container_kind = NULL;
	g_autofree gchar *lang = NULL;
	gchar *str;

	content = as_xml_get_node_value (node);
	if (content == NULL)
		return FALSE;

	lang = as_xml_get_node_locale_match (ctx, node);

	/* check if this video is for us */
	if (lang == NULL)
		return FALSE;
	as_video_set_locale (video, lang);

	str = (gchar*) xmlGetProp (node, (xmlChar*) "width");
	if (str == NULL) {
		priv->width = 0;
	} else {
		priv->width = g_ascii_strtoll (str, NULL, 10);
		g_free (str);
	}

	str = (gchar*) xmlGetProp (node, (xmlChar*) "height");
	if (str == NULL) {
		priv->height = 0;
	} else {
		priv->height = g_ascii_strtoll (str, NULL, 10);
		g_free (str);
	}

	codec_kind = (gchar*) xmlGetProp (node, (xmlChar*) "codec");
	priv->codec = as_video_codec_kind_from_string (codec_kind);

	container_kind = (gchar*) xmlGetProp (node, (xmlChar*) "container");
	priv->container = as_video_container_kind_from_string (container_kind);

	if (!as_context_has_media_baseurl (ctx)) {
		/* no baseurl, we can just set the value as URL */
		as_video_set_url (video, content);
	} else {
		/* handle the media baseurl */
		g_free (priv->url);
		priv->url = g_build_filename (as_context_get_media_baseurl (ctx), content, NULL);
	}

	return TRUE;
}

/**
 * as_video_to_xml_node:
 * @video: a #AsVideo instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_video_to_xml_node (AsVideo *video, AsContext *ctx, xmlNode *root)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	xmlNode* n_video = NULL;

	n_video = xmlNewTextChild (root, NULL,
				   (xmlChar*) "video",
				   (xmlChar*) priv->url);

	if (priv->codec != AS_VIDEO_CODEC_KIND_UNKNOWN)
		xmlNewProp (n_video, (xmlChar*) "codec", (xmlChar*) as_video_codec_kind_to_string (priv->codec));
	if (priv->container != AS_VIDEO_CONTAINER_KIND_UNKNOWN)
		xmlNewProp (n_video, (xmlChar*) "container", (xmlChar*) as_video_container_kind_to_string (priv->container));

	if ((priv->width > 0) && (priv->height > 0)) {
		gchar *size;

		size = g_strdup_printf("%i", priv->width);
		xmlNewProp (n_video, (xmlChar*) "width", (xmlChar*) size);
		g_free (size);

		size = g_strdup_printf("%i", priv->height);
		xmlNewProp (n_video, (xmlChar*) "height", (xmlChar*) size);
		g_free (size);
	}

	if ((priv->locale != NULL) && (g_strcmp0 (priv->locale, "C") != 0))
		xmlNewProp (n_video, (xmlChar*) "xml:lang", (xmlChar*) priv->locale);

	xmlAddChild (root, n_video);
}

/**
 * as_video_load_from_yaml:
 * @video: a #AsVideo instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_video_load_from_yaml (AsVideo *video, AsContext *ctx, GNode *node, GError **error)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	GNode *n;

	as_video_set_locale (video, "C");
	for (n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);
		const gchar *value = as_yaml_node_get_value (n);

		if (value == NULL)
			continue; /* there should be no key without value */

		if (g_strcmp0 (key, "width") == 0) {
			priv->width = g_ascii_strtoll (value, NULL, 10);

		} else if (g_strcmp0 (key, "height") == 0) {
			priv->height = g_ascii_strtoll (value, NULL, 10);

		} else if (g_strcmp0 (key, "codec") == 0) {
			priv->codec = as_video_codec_kind_from_string (value);

		} else if (g_strcmp0 (key, "container") == 0) {
			priv->container = as_video_container_kind_from_string (value);

		} else if (g_strcmp0 (key, "url") == 0) {
			if (as_context_has_media_baseurl (ctx)) {
				/* handle the media baseurl */
				g_free (priv->url);
				priv->url = g_build_filename (as_context_get_media_baseurl (ctx), value, NULL);
			} else {
				/* no baseurl, we can just set the value as URL */
				as_video_set_url (video, value);
			}
		} else if (g_strcmp0 (key, "lang") == 0) {
			as_video_set_locale (video, value);
		} else {
			as_yaml_print_unknown ("video", key);
		}
	}

	return TRUE;
}

/**
 * as_video_emit_yaml:
 * @video: a #AsVideo instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_video_emit_yaml (AsVideo *video, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsVideoPrivate *priv = GET_PRIVATE (video);
	g_autofree gchar *url = NULL;

	as_yaml_mapping_start (emitter);
	if (as_context_has_media_baseurl (ctx)) {
		if (g_str_has_prefix (priv->url, as_context_get_media_baseurl (ctx)))
			url = g_strdup (priv->url + strlen (as_context_get_media_baseurl (ctx)));
		else
			url = g_strdup (priv->url);
	} else {
		url = g_strdup (priv->url);
	}
	g_strstrip (url);

	as_yaml_emit_entry (emitter, "codec", as_video_codec_kind_to_string (priv->codec));
	as_yaml_emit_entry (emitter, "container", as_video_container_kind_to_string (priv->container));

	as_yaml_emit_entry (emitter, "url", url);
	if ((priv->width > 0) && (priv->height > 0)) {
		as_yaml_emit_entry_uint64 (emitter,
					   "width",
					   priv->width);

		as_yaml_emit_entry_uint64 (emitter,
					   "height",
					   priv->height);
	}
	if ((priv->locale != NULL) && (g_strcmp0 (priv->locale, "C") != 0))
		as_yaml_emit_entry (emitter, "lang", priv->locale);

	as_yaml_mapping_end (emitter);
}

/**
 * as_video_class_init:
 **/
static void
as_video_class_init (AsVideoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_video_finalize;
}

/**
 * as_video_new:
 *
 * Creates a new #AsVideo.
 *
 * Returns: (transfer full): a #AsVideo
 *
 **/
AsVideo*
as_video_new (void)
{
	AsVideo *video;
	video = g_object_new (AS_TYPE_VIDEO, NULL);
	return AS_VIDEO (video);
}
