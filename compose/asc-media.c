/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-media
 * @short_description: Sandboxable media processing operations.
 * @include: appstream-compose.h
 *
 * #AscMedia provides all high-level media processing operations that
 * appstream-compose needs (processing images, extracting font metadata,
 * rendering font specimens, probing videos).
 *
 * The actual media processing is performed by a separate worker process,
 * so parsing of untrusted media data is isolated.
 * Input data is passed to the worker exclusively via sealed memory file
 * descriptors, and every result is written into a descriptor that we opened
 * for it beforehand, so the worker never resolves a filesystem path and needs
 * no write access of its own at all - it can be sandboxed tightly.
 *
 * We can not know in advance which renditions the worker will actually
 * produce, as it decides that from the source image it alone has parsed. So we
 * hand it one throwaway output file per candidate rendition and only give the
 * ones it used their final name afterwards, which also makes publishing a
 * result atomic.
 *
 * An #AscMedia instance manages the lifecycle of exactly one worker process,
 * which is spawned lazily on first use and respawned (up to a limit) in case
 * it crashes. Instances must not be shared between threads - each thread
 * processing media must use its own #AscMedia instance.
 */

/* for O_TMPFILE */
#define _GNU_SOURCE
#include "config.h"
#include "asc-media.h"
#include "asc-media-private.h"

#include <errno.h>
#include <fcntl.h>
#include <gio/gunixfdlist.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "as-utils-private.h"
#include "asc-globals-private.h"
#include "asc-media-ipc.h"

/* how often we try to respawn a crashed worker before giving up */
#define ASC_MEDIA_RESPAWN_LIMIT 3

/* how long we wait for the worker to quit on its own before we kill it */
#define ASC_MEDIA_SHUTDOWN_TIMEOUT_SEC 30

struct _AscMedia {
	GObject parent_instance;
};

typedef struct {
	gchar *worker_path;
	guint timeout_secs;
	guint32 memory_limit_mb;

	GSubprocess *worker_proc;
	GSocket *socket;
	gchar *worker_sandbox;

	guint32 last_request_id;
	guint failure_count;
} AscMediaPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscMedia, asc_media, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_media_get_instance_private (o))

/**
 * asc_media_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 1.2.0
 **/
GQuark
asc_media_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AscMediaError");
	return quark;
}

/**
 * asc_media_error_is_worker_failure:
 * @error: (nullable): A #GError returned by a media operation.
 *
 * Check whether the given error indicates that the media worker itself
 * malfunctioned, as opposed to the processed media simply being broken.
 * Only the former should be reported to users as a worker problem.
 *
 * Returns: %TRUE if the worker misbehaved.
 *
 * Since: 1.2.0
 */
gboolean
asc_media_error_is_worker_failure (const GError *error)
{
	if (error == NULL || error->domain != ASC_MEDIA_ERROR)
		return FALSE;
	return error->code == ASC_MEDIA_ERROR_DEAD_WORKER ||
	       error->code == ASC_MEDIA_ERROR_TIMEOUT || error->code == ASC_MEDIA_ERROR_PROTOCOL;
}

/**
 * asc_image_format_to_string:
 * @format: the %AscImageFormat.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @format
 *
 * Since: 0.13.0
 **/
const gchar *
asc_image_format_to_string (AscImageFormat format)
{
	if (format == ASC_IMAGE_FORMAT_PNG)
		return "png";
	if (format == ASC_IMAGE_FORMAT_JXL)
		return "jxl";
	if (format == ASC_IMAGE_FORMAT_SVG)
		return "svg";
	if (format == ASC_IMAGE_FORMAT_SVGZ)
		return "svgz";
	if (format == ASC_IMAGE_FORMAT_AVIF)
		return "avif";
	if (format == ASC_IMAGE_FORMAT_WEBP)
		return "webp";
	if (format == ASC_IMAGE_FORMAT_JPEG)
		return "jpeg";
	if (format == ASC_IMAGE_FORMAT_GIF)
		return "gif";

	return NULL;
}

/**
 * asc_image_format_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AscImageFormat or %ASC_IMAGE_FORMAT_UNKNOWN for unknown
 *
 * Since: 0.13.0
 **/
AscImageFormat
asc_image_format_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "png") == 0)
		return ASC_IMAGE_FORMAT_PNG;
	if (g_strcmp0 (str, "jxl") == 0)
		return ASC_IMAGE_FORMAT_JXL;
	if (g_strcmp0 (str, "svg") == 0)
		return ASC_IMAGE_FORMAT_SVG;
	if (g_strcmp0 (str, "svgz") == 0)
		return ASC_IMAGE_FORMAT_SVGZ;
	if (g_strcmp0 (str, "avif") == 0)
		return ASC_IMAGE_FORMAT_AVIF;
	if (g_strcmp0 (str, "webp") == 0)
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_strcmp0 (str, "jpeg") == 0)
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_strcmp0 (str, "gif") == 0)
		return ASC_IMAGE_FORMAT_GIF;

	return ASC_IMAGE_FORMAT_UNKNOWN;
}

/**
 * asc_image_format_from_filename:
 * @fname: the filename.
 *
 * Returns the image format type based on the given file's filename.
 *
 * Returns: a #AscImageFormat or %ASC_IMAGE_FORMAT_UNKNOWN for unknown
 *
 * Since: 0.13.0
 **/
AscImageFormat
asc_image_format_from_filename (const gchar *fname)
{
	g_autofree gchar *fname_low = g_ascii_strdown (fname, -1);

	if (g_str_has_suffix (fname_low, ".png"))
		return ASC_IMAGE_FORMAT_PNG;
	if (g_str_has_suffix (fname_low, ".jxl"))
		return ASC_IMAGE_FORMAT_JXL;
	if (g_str_has_suffix (fname_low, ".avif"))
		return ASC_IMAGE_FORMAT_AVIF;
	if (g_str_has_suffix (fname_low, ".webp"))
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_str_has_suffix (fname_low, ".svg"))
		return ASC_IMAGE_FORMAT_SVG;
	if (g_str_has_suffix (fname_low, ".svgz"))
		return ASC_IMAGE_FORMAT_SVGZ;
	if (g_str_has_suffix (fname_low, ".jpeg") || g_str_has_suffix (fname_low, ".jpg"))
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_str_has_suffix (fname_low, ".gif"))
		return ASC_IMAGE_FORMAT_GIF;

	return ASC_IMAGE_FORMAT_UNKNOWN;
}

/**
 * AscImageSource:
 *
 * Describes the image a media operation should read, and how it should be
 * loaded. After the operation has run, it also carries the dimensions the
 * source image was loaded at.
 *
 * Since: 1.2.0
 */
struct _AscImageSource {
	GBytes *data;
	gint render_width;
	gint render_height;

	/* result fields, set by the media operation */
	gint width;
	gint height;
};

/**
 * asc_image_source_new:
 * @data: The raw image data to process.
 *
 * Create a new #AscImageSource for the given image data.
 *
 * Returns: (transfer full): an #AscImageSource
 *
 * Since: 1.2.0
 */
AscImageSource *
asc_image_source_new (GBytes *data)
{
	AscImageSource *source;

	g_return_val_if_fail (data != NULL, NULL);

	source = g_new0 (AscImageSource, 1);
	source->data = g_bytes_ref (data);

	return source;
}

/**
 * asc_image_source_free: (skip)
 * @source: an #AscImageSource
 *
 * Free an #AscImageSource. Bindings must not call this: they own the boxed value and
 * release it themselves, so freeing it here would free it twice.
 *
 * Since: 1.2.0
 */
void
asc_image_source_free (AscImageSource *source)
{
	if (source == NULL)
		return;
	g_bytes_unref (source->data);
	g_free (source);
}

/**
 * asc_image_source_copy:
 * @source: an #AscImageSource
 *
 * Create a copy of an #AscImageSource, including any result data
 * it may already carry.
 *
 * Returns: (transfer full): a new #AscImageSource
 *
 * Since: 1.2.0
 */
AscImageSource *
asc_image_source_copy (AscImageSource *source)
{
	AscImageSource *copy;

	g_return_val_if_fail (source != NULL, NULL);

	copy = g_new0 (AscImageSource, 1);
	*copy = *source;
	copy->data = g_bytes_ref (source->data);

	return copy;
}

G_DEFINE_BOXED_TYPE (AscImageSource, asc_image_source, asc_image_source_copy, asc_image_source_free)

/**
 * asc_image_source_get_data:
 * @source: an #AscImageSource
 *
 * Get the raw image data this source was created for.
 *
 * Returns: (transfer none): The image data.
 *
 * Since: 1.2.0
 */
GBytes *
asc_image_source_get_data (AscImageSource *source)
{
	g_return_val_if_fail (source != NULL, NULL);
	return source->data;
}

/**
 * asc_image_source_get_render_size:
 * @source: an #AscImageSource
 * @width: (out) (optional): Destination of the render width.
 * @height: (out) (optional): Destination of the render height.
 *
 * Get the size vector graphics are rendered at.
 *
 * Since: 1.2.0
 */
void
asc_image_source_get_render_size (AscImageSource *source, gint *width, gint *height)
{
	g_return_if_fail (source != NULL);
	if (width != NULL)
		*width = source->render_width;
	if (height != NULL)
		*height = source->render_height;
}

/**
 * asc_image_source_set_render_size:
 * @source: an #AscImageSource
 * @width: Width to render at, or 0 for the native size.
 * @height: Height to render at, or 0 for the native size.
 *
 * Set the size vector graphics should be rendered at. This has no effect
 * on raster images, which are always loaded at their native size.
 *
 * Since: 1.2.0
 */
void
asc_image_source_set_render_size (AscImageSource *source, gint width, gint height)
{
	g_return_if_fail (source != NULL);
	source->render_width = width;
	source->render_height = height;
}

/**
 * asc_image_source_get_width:
 * @source: an #AscImageSource
 *
 * Get the width the source image was loaded at. Only valid after the media
 * operation this source was passed to has completed successfully.
 *
 * Returns: The source width in pixels, or 0 if it is not known yet.
 *
 * Since: 1.2.0
 */
gint
asc_image_source_get_width (AscImageSource *source)
{
	g_return_val_if_fail (source != NULL, 0);
	return source->width;
}

/**
 * asc_image_source_get_height:
 * @source: an #AscImageSource
 *
 * Get the height the source image was loaded at. Only valid after the media
 * operation this source was passed to has completed successfully.
 *
 * Returns: The source height in pixels, or 0 if it is not known yet.
 *
 * Since: 1.2.0
 */
gint
asc_image_source_get_height (AscImageSource *source)
{
	g_return_val_if_fail (source != NULL, 0);
	return source->height;
}

/**
 * AscImageTarget:
 *
 * Describes one requested output rendition of a media operation, and carries
 * its result data after the operation was run. Renditions that were skipped
 * due to one of their source-size conditions are not an error: the operation
 * still succeeds, and %asc_image_target_get_skipped will return %TRUE.
 *
 * Since: 1.2.0
 */
struct _AscImageTarget {
	gchar *name;
	AscImageScaleMode scale_mode;
	gint width;
	gint height;
	AscImageSaveFlags save_flags;
	gboolean only_downscale;
	gint min_src_width;
	gint min_src_height;
	gint max_src_width;
	gint max_src_height;

	/* result fields, set by the media operation */
	gboolean skipped;
	gint result_width;
	gint result_height;
	gchar *error_msg;
};

/**
 * asc_image_target_new:
 * @name: Filename the rendition should be stored as.
 * @scale_mode: an #AscImageScaleMode
 * @width: Target width (used depending on @scale_mode).
 * @height: Target height (used depending on @scale_mode).
 *
 * Create a new #AscImageTarget. The image format the rendition is stored in
 * is derived from the file extension of @name.
 *
 * @name must be a single path segment: it is resolved relative to the output
 * directory of the media operation, and must not contain any directory
 * components or refer to a parent directory.
 *
 * Returns: (transfer full): an #AscImageTarget
 *
 * Since: 1.2.0
 */
AscImageTarget *
asc_image_target_new (const gchar *name, AscImageScaleMode scale_mode, gint width, gint height)
{
	AscImageTarget *target;

	target = g_new0 (AscImageTarget, 1);
	target->name = g_strdup (name);
	target->scale_mode = scale_mode;
	target->width = width;
	target->height = height;

	return target;
}

/**
 * asc_image_target_free: (skip)
 * @target: an #AscImageTarget
 *
 * Free an #AscImageTarget. Bindings must not call this: they own the boxed value and
 * release it themselves, so freeing it here would free it twice.
 *
 * Since: 1.2.0
 */
void
asc_image_target_free (AscImageTarget *target)
{
	if (target == NULL)
		return;
	g_free (target->name);
	g_free (target->error_msg);
	g_free (target);
}

/**
 * asc_image_target_copy:
 * @target: an #AscImageTarget
 *
 * Create a deep copy of an #AscImageTarget, including any result data
 * it may already carry.
 *
 * Returns: (transfer full): a new #AscImageTarget
 *
 * Since: 1.2.0
 */
AscImageTarget *
asc_image_target_copy (AscImageTarget *target)
{
	AscImageTarget *copy;

	g_return_val_if_fail (target != NULL, NULL);

	copy = g_new0 (AscImageTarget, 1);
	*copy = *target;
	copy->name = g_strdup (target->name);
	copy->error_msg = g_strdup (target->error_msg);

	return copy;
}

G_DEFINE_BOXED_TYPE (AscImageTarget, asc_image_target, asc_image_target_copy, asc_image_target_free)

/**
 * asc_image_target_get_name:
 * @target: an #AscImageTarget
 *
 * Get the filename this rendition is stored as.
 *
 * Returns: The rendition filename.
 *
 * Since: 1.2.0
 */
const gchar *
asc_image_target_get_name (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, NULL);
	return target->name;
}

/**
 * asc_image_target_get_scale_mode:
 * @target: an #AscImageTarget
 *
 * Get the scaling mode this rendition is created with.
 *
 * Returns: an #AscImageScaleMode
 *
 * Since: 1.2.0
 */
AscImageScaleMode
asc_image_target_get_scale_mode (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, ASC_IMAGE_SCALE_MODE_NONE);
	return target->scale_mode;
}

/**
 * asc_image_target_get_width:
 * @target: an #AscImageTarget
 *
 * Get the requested width of this rendition. Depending on the scale mode,
 * this may differ from the width the rendition is actually stored at.
 *
 * Returns: The requested width in pixels.
 *
 * Since: 1.2.0
 */
gint
asc_image_target_get_width (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, 0);
	return target->width;
}

/**
 * asc_image_target_get_height:
 * @target: an #AscImageTarget
 *
 * Get the requested height of this rendition. Depending on the scale mode,
 * this may differ from the height the rendition is actually stored at.
 *
 * Returns: The requested height in pixels.
 *
 * Since: 1.2.0
 */
gint
asc_image_target_get_height (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, 0);
	return target->height;
}

/**
 * asc_image_target_get_save_flags:
 * @target: an #AscImageTarget
 *
 * Get the flags used when storing this rendition.
 *
 * Returns: the #AscImageSaveFlags in use.
 *
 * Since: 1.2.0
 */
AscImageSaveFlags
asc_image_target_get_save_flags (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, ASC_IMAGE_SAVE_FLAG_NONE);
	return target->save_flags;
}

/**
 * asc_image_target_set_save_flags:
 * @target: an #AscImageTarget
 * @flags: the #AscImageSaveFlags to use.
 *
 * Set the flags used when storing this rendition.
 *
 * Since: 1.2.0
 */
void
asc_image_target_set_save_flags (AscImageTarget *target, AscImageSaveFlags flags)
{
	g_return_if_fail (target != NULL);
	target->save_flags = flags;
}

/**
 * asc_image_target_get_only_downscale:
 * @target: an #AscImageTarget
 *
 * Get whether this rendition should be skipped rather than
 * considering to upscale it to the target size(s).
 *
 * Returns: %TRUE if the rendition is never upscaled.
 *
 * Since: 1.2.0
 */
gboolean
asc_image_target_get_only_downscale (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, FALSE);
	return target->only_downscale;
}

/**
 * asc_image_target_set_only_downscale:
 * @target: an #AscImageTarget
 * @only_downscale: %TRUE to never upscale.
 *
 * Set whether this rendition should be skipped in case creating it would
 * mean upscaling the source image.
 *
 * Since: 1.2.0
 */
void
asc_image_target_set_only_downscale (AscImageTarget *target, gboolean only_downscale)
{
	g_return_if_fail (target != NULL);
	target->only_downscale = only_downscale;
}

/**
 * asc_image_target_get_source_size_range:
 * @target: an #AscImageTarget
 * @min_width: (out) (optional): Destination of the minimum source image width.
 * @min_height: (out) (optional): Destination of the minimum source image height.
 * @max_width: (out) (optional): Destination of the maximum source image width.
 * @max_height: (out) (optional): Destination of the maximum source image height.
 *
 * Get the source image dimensions this rendition is restricted to.
 * A value of 0 means that the respective dimension is not limited.
 *
 * Since: 1.2.0
 */
void
asc_image_target_get_source_size_range (AscImageTarget *target,
					gint *min_width,
					gint *min_height,
					gint *max_width,
					gint *max_height)
{
	g_return_if_fail (target != NULL);
	if (min_width != NULL)
		*min_width = target->min_src_width;
	if (min_height != NULL)
		*min_height = target->min_src_height;
	if (max_width != NULL)
		*max_width = target->max_src_width;
	if (max_height != NULL)
		*max_height = target->max_src_height;
}

/**
 * asc_image_target_set_source_size_range:
 * @target: an #AscImageTarget
 * @min_width: Minimum source image width, or 0 for no limit.
 * @min_height: Minimum source image height, or 0 for no limit.
 * @max_width: Maximum source image width, or 0 for no limit.
 * @max_height: Maximum source image height, or 0 for no limit.
 *
 * Restrict the source images this rendition is created for. If the source
 * image's dimensions fall outside of the given range, the rendition is
 * skipped and no file is written for it, while the media operation as a
 * whole still succeeds.
 *
 * Since: 1.2.0
 */
void
asc_image_target_set_source_size_range (AscImageTarget *target,
					gint min_width,
					gint min_height,
					gint max_width,
					gint max_height)
{
	g_return_if_fail (target != NULL);
	target->min_src_width = min_width;
	target->min_src_height = min_height;
	target->max_src_width = max_width;
	target->max_src_height = max_height;
}

/**
 * asc_image_target_get_skipped:
 * @target: an #AscImageTarget
 *
 * Check whether this rendition was skipped because the source image did not
 * satisfy its source-size conditions. Only valid after the media operation
 * this target was passed to has completed successfully.
 *
 * Returns: %TRUE if no file was written for this rendition.
 *
 * Since: 1.2.0
 */
gboolean
asc_image_target_get_skipped (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, FALSE);
	return target->skipped;
}

/**
 * asc_image_target_get_result_width:
 * @target: an #AscImageTarget
 *
 * Get the actual width of the stored rendition.
 *
 * Returns: The rendition width in pixels, or 0 if it was not stored.
 *
 * Since: 1.2.0
 */
gint
asc_image_target_get_result_width (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, 0);
	return target->result_width;
}

/**
 * asc_image_target_get_result_height:
 * @target: an #AscImageTarget
 *
 * Get the actual height of the stored rendition.
 *
 * Returns: The rendition height in pixels, or 0 if it was not stored.
 *
 * Since: 1.2.0
 */
gint
asc_image_target_get_result_height (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, 0);
	return target->result_height;
}

/**
 * asc_image_target_get_error_message:
 * @target: an #AscImageTarget
 *
 * Get the error message in case creating this particular rendition failed.
 * A failed rendition does not fail the media operation as a whole.
 *
 * Returns: (nullable): The error message, or %NULL if this rendition was fine.
 *
 * Since: 1.2.0
 */
const gchar *
asc_image_target_get_error_message (AscImageTarget *target)
{
	g_return_val_if_fail (target != NULL, NULL);
	return target->error_msg;
}

/**
 * asc_font_info_free:
 *
 * Free an #AscFontInfo.
 */
void
asc_font_info_free (AscFontInfo *info)
{
	if (info == NULL)
		return;
	g_free (info->family);
	g_free (info->style);
	g_free (info->fullname);
	g_free (info->id);
	g_free (info->description);
	g_free (info->homepage);
	g_strfreev (info->languages);
	g_free (info->preferred_language);
	g_free (info->sample_text);
	g_free (info->sample_icon_text);
	g_free (info);
}

static AscFontInfo *
asc_font_info_copy (AscFontInfo *info)
{
	AscFontInfo *copy;

	copy = g_new0 (AscFontInfo, 1);
	copy->family = g_strdup (info->family);
	copy->style = g_strdup (info->style);
	copy->fullname = g_strdup (info->fullname);
	copy->id = g_strdup (info->id);
	copy->description = g_strdup (info->description);
	copy->homepage = g_strdup (info->homepage);
	copy->languages = g_strdupv (info->languages);
	copy->preferred_language = g_strdup (info->preferred_language);
	copy->sample_text = g_strdup (info->sample_text);
	copy->sample_icon_text = g_strdup (info->sample_icon_text);

	return copy;
}

G_DEFINE_BOXED_TYPE (AscFontInfo, asc_font_info, asc_font_info_copy, asc_font_info_free)

static void asc_media_shutdown_worker (AscMedia *media, gboolean force);
static gchar *asc_media_lookup_dup_nonempty (GVariant *dict, const gchar *key);

static void
asc_media_finalize (GObject *object)
{
	AscMedia *media = ASC_MEDIA (object);
	AscMediaPrivate *priv = GET_PRIVATE (media);

	asc_media_shutdown_worker (media, FALSE);

	g_free (priv->worker_path);
	g_free (priv->worker_sandbox);

	G_OBJECT_CLASS (asc_media_parent_class)->finalize (object);
}

static void
asc_media_init (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);

	priv->timeout_secs = 120;
	priv->worker_path = g_strdup (asc_globals_get_mediaworker_binary ());
}

static void
asc_media_class_init (AscMediaClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_media_finalize;
}

/**
 * asc_media_new:
 *
 * Creates a new #AscMedia.
 *
 * Since: 1.2.0
 **/
AscMedia *
asc_media_new (void)
{
	AscMedia *media;
	media = g_object_new (ASC_TYPE_MEDIA, NULL);
	return ASC_MEDIA (media);
}

/**
 * asc_media_get_request_timeout:
 * @media: an #AscMedia instance.
 *
 * Get the request timeout in seconds.
 *
 * Since: 1.2.0
 */
guint
asc_media_get_request_timeout (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_return_val_if_fail (ASC_IS_MEDIA (media), 0);

	return priv->timeout_secs;
}

/**
 * asc_media_set_request_timeout:
 * @media: an #AscMedia instance.
 * @seconds: New timeout in seconds, 0 for no timeout.
 *
 * Set the time a single media operation may take before the worker
 * is assumed to hang and gets killed.
 *
 * Since: 1.2.0
 */
void
asc_media_set_request_timeout (AscMedia *media, guint seconds)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_return_if_fail (ASC_IS_MEDIA (media));

	priv->timeout_secs = seconds;
	if (priv->socket != NULL)
		g_socket_set_timeout (priv->socket, priv->timeout_secs);
}

/**
 * asc_media_get_worker_path:
 * @media: an #AscMedia instance.
 *
 * Get the path to the worker binary this instance will use,
 * or %NULL if no worker binary was found at all.
 *
 * Since: 1.2.0
 */
const gchar *
asc_media_get_worker_path (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_return_val_if_fail (ASC_IS_MEDIA (media), NULL);

	return priv->worker_path;
}

/**
 * asc_media_set_worker_path:
 * @media: an #AscMedia instance.
 * @path: Path to an asc-mediaworker binary, or %NULL to use the default.
 *
 * Override the media worker binary used by this instance.
 *
 * Since: 1.2.0
 */
void
asc_media_set_worker_path (AscMedia *media, const gchar *path)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_return_if_fail (ASC_IS_MEDIA (media));

	if (path == NULL)
		path = asc_globals_get_mediaworker_binary ();
	as_assign_string_safe (priv->worker_path, path);
}

/**
 * asc_media_get_worker_sandbox:
 * @media: an #AscMedia instance.
 *
 * Get the sandboxing level the running media worker reported for itself, as one
 * of "landlock", "landlock-partial" or "none". %NULL if no worker is running yet.
 *
 * This is for diagnostics and the test suite; there is deliberately no way to
 * demand a particular level yet.
 */
const gchar *
asc_media_get_worker_sandbox (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	return priv->worker_sandbox;
}

/**
 * asc_media_get_memory_limit:
 * @media: an #AscMedia instance.
 *
 * Get the address space limit applied to the worker process in MiB,
 * or 0 if the worker may use as much memory as it wants.
 *
 * Since: 1.2.0
 */
guint32
asc_media_get_memory_limit (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_return_val_if_fail (ASC_IS_MEDIA (media), 0);

	return priv->memory_limit_mb;
}

/**
 * asc_media_set_memory_limit:
 * @media: an #AscMedia instance.
 * @limit_mib: Address space limit for the worker process in MiB, 0 for no limit.
 *
 * Limit the amount of memory the worker process may use.
 * The limit is applied when the next worker process is spawned.
 *
 * Since: 1.2.0
 */
void
asc_media_set_memory_limit (AscMedia *media, guint32 limit_mib)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_return_if_fail (ASC_IS_MEDIA (media));

	priv->memory_limit_mb = limit_mib;
}

/**
 * asc_media_describe_worker_death:
 *
 * Create a human-readable description of how and why the worker
 * process died.
 */
static gchar *
asc_media_describe_worker_death (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_autoptr(GString) desc = g_string_new (NULL);

	if (priv->worker_proc != NULL) {
		if (g_subprocess_get_if_exited (priv->worker_proc))
			g_string_append_printf (desc,
						"Worker exited with status %i.",
						g_subprocess_get_exit_status (priv->worker_proc));
		else if (g_subprocess_get_if_signaled (priv->worker_proc))
			g_string_append_printf (desc,
						"Worker was killed by signal %i.",
						g_subprocess_get_term_sig (priv->worker_proc));
		else
			g_string_append (desc, "Worker terminated unexpectedly.");
	} else {
		g_string_append (desc, "Worker terminated unexpectedly.");
	}

	return g_string_free (g_steal_pointer (&desc), FALSE);
}

typedef struct {
	GSubprocess *proc;
	GMainLoop *loop;
} AscMediaWaitHelper;

static void
asc_media_worker_exited_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	AscMediaWaitHelper *helper = user_data;
	g_subprocess_wait_finish (G_SUBPROCESS (source), result, NULL);
	g_main_loop_quit (helper->loop);
}

static gboolean
asc_media_worker_exit_timeout_cb (gpointer user_data)
{
	AscMediaWaitHelper *helper = user_data;

	/* the worker ignored our request to quit, so we have to be more insistent.
	 * we keep the loop running, as the process will now terminate for sure */
	g_warning ("Media worker did not terminate in time, killing it.");
	g_subprocess_force_exit (helper->proc);

	return G_SOURCE_REMOVE;
}

/**
 * asc_media_wait_for_worker_exit:
 *
 * Wait for the worker process to terminate, killing it in case it
 * does not react to our request to quit in time.
 */
static void
asc_media_wait_for_worker_exit (GSubprocess *proc)
{
	g_autoptr(GMainContext) context = g_main_context_new ();
	g_autoptr(GMainLoop) loop = NULL;
	g_autoptr(GSource) timeout_source = NULL;
	AscMediaWaitHelper helper;

	g_main_context_push_thread_default (context);

	loop = g_main_loop_new (context, FALSE);
	helper.proc = proc;
	helper.loop = loop;

	timeout_source = g_timeout_source_new_seconds (ASC_MEDIA_SHUTDOWN_TIMEOUT_SEC);
	g_source_set_callback (timeout_source, asc_media_worker_exit_timeout_cb, &helper, NULL);
	g_source_attach (timeout_source, context);

	g_subprocess_wait_async (proc, NULL, asc_media_worker_exited_cb, &helper);
	g_main_loop_run (loop);

	g_source_destroy (timeout_source);
	g_main_context_pop_thread_default (context);
}

/**
 * asc_media_shutdown_worker:
 * @force: Whether to kill the worker immediately instead of asking it to quit.
 *
 * Terminate the worker process (if it is running) and clean up
 * all connection state.
 */
static void
asc_media_shutdown_worker (AscMedia *media, gboolean force)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);

	if (priv->worker_proc != NULL) {
		gboolean clean_quit = FALSE;

		if (!force && priv->socket != NULL) {
			g_autoptr(GVariant) payload = NULL;
			g_autoptr(GError) tmp_error = NULL;
			guint32 rid, status;
			gboolean eof = FALSE;

			/* ask the worker politely to quit */
			if (asc_media_ipc_send_request (priv->socket,
							++priv->last_request_id,
							ASC_MEDIA_OP_SHUTDOWN,
							NULL,
							NULL,
							NULL, /* cancellable */
							&tmp_error)) {
				clean_quit = asc_media_ipc_receive_response (priv->socket,
									     &rid,
									     &status,
									     &payload,
									     &eof,
									     NULL, /* cancellable */
									     &tmp_error);
			}
		}

		if (force) {
			g_subprocess_force_exit (priv->worker_proc);
			g_subprocess_wait (priv->worker_proc, NULL, NULL);
		} else {
			if (!clean_quit) {
				/* closing our socket end makes the worker exit on EOF */
				g_clear_object (&priv->socket);
			}
			asc_media_wait_for_worker_exit (priv->worker_proc);
		}
	}

	g_clear_object (&priv->socket);
	g_clear_object (&priv->worker_proc);
	g_clear_pointer (&priv->worker_sandbox, g_free);
}

/**
 * asc_media_stop:
 * @media: an #AscMedia instance.
 *
 * Stop the worker process of this instance, if it is running.
 * A new worker will be spawned automatically if another media
 * operation is requested.
 *
 * Since: 1.2.0
 */
void
asc_media_stop (AscMedia *media)
{
	g_return_if_fail (ASC_IS_MEDIA (media));

	asc_media_shutdown_worker (media, FALSE);
}

/**
 * asc_media_worker_setup:
 *
 * Send the setup request with our current global configuration
 * to a freshly spawned worker.
 */
static gboolean
asc_media_worker_setup (AscMedia *media, GCancellable *cancellable, GError **error)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_autoptr(GVariant) payload = NULL;
	GVariantBuilder pb;
	const gchar *ffprobe_path = asc_globals_get_ffprobe_binary ();
	guint32 rid = 0;
	guint32 status = 0;
	gboolean eof = FALSE;

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "ffprobe-path",
			       g_variant_new_string (ffprobe_path ? ffprobe_path : ""));
	if (priv->memory_limit_mb > 0)
		g_variant_builder_add (&pb,
				       "{sv}",
				       "memory-limit-mb",
				       g_variant_new_uint32 (priv->memory_limit_mb));

	if (!asc_media_ipc_send_request (priv->socket,
					 ++priv->last_request_id,
					 ASC_MEDIA_OP_SETUP,
					 g_variant_builder_end (&pb),
					 NULL,
					 cancellable,
					 error))
		return FALSE;
	if (!asc_media_ipc_receive_response (priv->socket,
					     &rid,
					     &status,
					     &payload,
					     &eof,
					     cancellable,
					     error))
		return FALSE;
	if (rid != priv->last_request_id) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Worker replied to the wrong setup request.");
		return FALSE;
	}
	if (status != ASC_MEDIA_STATUS_OK) {
		g_autoptr(GError) worker_error = asc_media_ipc_error_from_payload (payload);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_PROTOCOL,
			     "Worker rejected its setup request: %s",
			     worker_error->message);
		return FALSE;
	}

	return TRUE;
}

/**
 * asc_media_spawn_worker:
 *
 * Spawn a new worker process and perform the initial handshake with it.
 */
static gboolean
asc_media_spawn_worker (AscMedia *media, GCancellable *cancellable, GError **error)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_autoptr(GSubprocessLauncher) launcher = NULL;
	g_autoptr(GVariant) hello = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autofree gchar *sandbox_detail = NULL;
	const gchar *program_version = NULL;
	guint32 protocol_version = 0;
	guint32 rid = 0;
	guint32 status = 0;
	gboolean eof = FALSE;
	int sv[2];

	if (priv->worker_path == NULL) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_DEAD_WORKER,
				     "Unable to find the mediaworker binary. Check if the "
				     "AppStream installation is complete.");
		return FALSE;
	}

	if (socketpair (AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) != 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Unable to create worker socket pair: %s",
			     g_strerror (errno));
		return FALSE;
	}

	/* anything the worker prints is for debugging, so we let it inherit stdout/stderr */
	launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
	g_subprocess_launcher_take_fd (launcher, sv[1], ASC_MEDIA_SOCKET_FD);

	priv->worker_proc = g_subprocess_launcher_spawn (launcher,
							 &tmp_error,
							 priv->worker_path,
							 NULL);
	/* drop the launcher immediately: it holds the worker-side socket fd open in our
	 * process, which would prevent us from seeing an EOF if the worker dies */
	g_clear_object (&launcher);
	if (priv->worker_proc == NULL) {
		close (sv[0]);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Unable to spawn media worker '%s': %s",
			     priv->worker_path,
			     tmp_error->message);
		return FALSE;
	}

	priv->socket = g_socket_new_from_fd (sv[0], &tmp_error);
	if (priv->socket == NULL) {
		close (sv[0]);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Unable to set up worker communication: %s",
			     tmp_error->message);
		asc_media_shutdown_worker (media, TRUE);
		return FALSE;
	}
	g_socket_set_blocking (priv->socket, TRUE);
	g_socket_set_timeout (priv->socket, priv->timeout_secs);

	/* receive the hello message and validate that the worker matches us exactly */
	if (!asc_media_ipc_receive_response (priv->socket,
					     &rid,
					     &status,
					     &hello,
					     &eof,
					     cancellable,
					     &tmp_error)) {
		g_autofree gchar *death_desc = NULL;
		asc_media_shutdown_worker (media, eof ? FALSE : TRUE);
		death_desc = asc_media_describe_worker_death (media);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Media worker vanished during handshake: %s %s",
			     tmp_error != NULL ? tmp_error->message : "",
			     death_desc);
		return FALSE;
	}

	g_variant_lookup (hello, "protocol-version", "u", &protocol_version);
	g_variant_lookup (hello, "program-version", "&s", &program_version);
	if (rid != 0 || status != ASC_MEDIA_STATUS_OK ||
	    protocol_version != ASC_MEDIA_PROTOCOL_VERSION ||
	    !as_str_equal0 (program_version, PACKAGE_VERSION)) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_PROTOCOL,
			     "Media worker '%s' is incompatible with this version of "
			     "libappstream-compose (worker version: %s, expected: %s).",
			     priv->worker_path,
			     program_version ? program_version : "unknown",
			     PACKAGE_VERSION);
		asc_media_shutdown_worker (media, TRUE);
		return FALSE;
	}

	/* remember how well the worker managed to sandbox itself, purely so that it
	 * shows up in the log and the test suite can assert on it. The worker warns
	 * about problems on its own inherited stderr, so we must not warn again. */
	priv->worker_sandbox = asc_media_lookup_dup_nonempty (hello, "sandbox");
	if (priv->worker_sandbox == NULL)
		priv->worker_sandbox = g_strdup ("none");
	sandbox_detail = asc_media_lookup_dup_nonempty (hello, "sandbox-detail");

	/* configure the new worker */
	if (!asc_media_worker_setup (media, cancellable, &tmp_error)) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Unable to configure the media worker: %s",
			     tmp_error->message);
		asc_media_shutdown_worker (media, TRUE);
		return FALSE;
	}

	g_debug ("Media worker started: %s (sandbox: %s)",
		 priv->worker_path,
		 sandbox_detail != NULL ? sandbox_detail : priv->worker_sandbox);
	return TRUE;
}

/**
 * asc_media_ensure_worker:
 * @media: an #AscMedia instance.
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Ensure a media worker process is running, spawning one if necessary.
 * This function is called implicitly by all media operations, calling
 * it explicitly is only useful to make worker startup problems surface
 * early.
 *
 * Returns: %TRUE if the worker is running.
 *
 * Since: 1.2.0
 */
gboolean
asc_media_ensure_worker (AscMedia *media, GCancellable *cancellable, GError **error)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);

	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	if (priv->socket != NULL)
		return TRUE;

	if (priv->failure_count >= ASC_MEDIA_RESPAWN_LIMIT) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "The media worker failed %u times, refusing to restart it again.",
			     priv->failure_count);
		return FALSE;
	}

	if (!asc_media_spawn_worker (media, cancellable, error)) {
		priv->failure_count++;
		return FALSE;
	}

	return TRUE;
}

/**
 * asc_media_call:
 *
 * Perform a media operation: send the request, wait for the response
 * and handle all worker failure modes.
 *
 * Returns: (transfer full): The response payload, or %NULL on any error.
 */
static GVariant *
asc_media_call (AscMedia *media,
		AscMediaOp op,
		GVariant *params,
		GUnixFDList *fds,
		GCancellable *cancellable,
		GError **error)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_autoptr(GVariant) params_ref = g_variant_ref_sink (params);
	g_autoptr(GVariant) payload = NULL;
	g_autoptr(GError) tmp_error = NULL;
	guint32 request_id;
	guint32 rid = 0;
	guint32 status = 0;
	gboolean eof = FALSE;

	if (!asc_media_ensure_worker (media, cancellable, error))
		return NULL;

	request_id = ++priv->last_request_id;
	if (!asc_media_ipc_send_request (priv->socket,
					 request_id,
					 op,
					 params_ref,
					 fds,
					 cancellable,
					 &tmp_error))
		goto worker_failed;

	if (!asc_media_ipc_receive_response (priv->socket,
					     &rid,
					     &status,
					     &payload,
					     &eof,
					     cancellable,
					     &tmp_error)) {
		if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			/* the worker is mid-operation and its response would desynchronize
			 * the next request, so it has to go - but this is not its fault
			 * and must not count towards the respawn limit */
			asc_media_shutdown_worker (media, TRUE);
			g_propagate_error (error, g_steal_pointer (&tmp_error));
			return NULL;
		}
		if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) {
			priv->failure_count++;
			asc_media_shutdown_worker (media, TRUE);
			g_set_error (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_TIMEOUT,
				     "Media operation timed out after %u seconds, the worker "
				     "process was killed.",
				     priv->timeout_secs);
			return NULL;
		}
		goto worker_failed;
	}

	if (rid != request_id) {
		priv->failure_count++;
		asc_media_shutdown_worker (media, TRUE);
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received a worker response for the wrong request, "
				     "terminated the worker.");
		return NULL;
	}

	if (status != ASC_MEDIA_STATUS_OK) {
		/* the operation failed, but the worker itself is healthy */
		if (error != NULL)
			*error = asc_media_ipc_error_from_payload (payload);
		return NULL;
	}

	priv->failure_count = 0;
	return g_steal_pointer (&payload);

worker_failed:
	priv->failure_count++;
	{
		g_autofree gchar *death_desc = NULL;
		asc_media_shutdown_worker (media, eof ? FALSE : TRUE);
		death_desc = asc_media_describe_worker_death (media);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Media worker failed: %s %s",
			     tmp_error != NULL ? tmp_error->message : "",
			     death_desc);
	}
	return NULL;
}

/**
 * asc_media_fdlist_append:
 *
 * Add a file descriptor to the list, returning its handle value
 * for referencing it in message parameters.
 * The passed fd is consumed.
 */
static gint
asc_media_fdlist_append (GUnixFDList *fds, gint fd, GError **error)
{
	gint handle;

	handle = g_unix_fd_list_append (fds, fd, error);
	close (fd);
	return handle;
}

/**
 * asc_media_open_out_dir:
 *
 * Open an output directory, creating it if needed.
 */
static gint
asc_media_open_out_dir (const gchar *out_dir, GError **error)
{
	gint fd;

	if (g_mkdir_with_parents (out_dir, 0755) != 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Unable to create media output directory '%s': %s",
			     out_dir,
			     g_strerror (errno));
		return -1;
	}

	fd = open (out_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Unable to open media output directory '%s': %s",
			     out_dir,
			     g_strerror (errno));
		return -1;
	}
	return fd;
}

/**
 * AscMediaOutSlot:
 *
 * One pre-opened output file that the media worker may write a rendition into.
 *
 * The worker only ever receives @fd: it can neither pick a location nor create
 * a file of its own. Once it has told us which slots it actually used, we give
 * those their final name and drop the rest.
 */
typedef struct {
	gint fd;		/* writable descriptor handed to the worker */
	gchar *tmp_name;	/* temporary entry in the output directory, or %NULL */
	AscImageTarget *target; /* the rendition this slot belongs to */
} AscMediaOutSlot;

/**
 * asc_media_out_slot_free:
 */
static void
asc_media_out_slot_free (AscMediaOutSlot *slot)
{
	if (slot == NULL)
		return;
	if (slot->fd >= 0)
		close (slot->fd);
	g_free (slot->tmp_name);
	g_free (slot);
}

/**
 * asc_media_make_tmp_name:
 *
 * Build a name for a temporary entry in an output directory. The leading dot
 * keeps these out of the way and makes a collision with a rendition name
 * impossible, as %as_path_segment_verify rejects names that start with one.
 */
static gchar *
asc_media_make_tmp_name (void)
{
	return g_strdup_printf (".asc-tmp-%08x", g_random_int ());
}

#ifdef O_TMPFILE

/**
 * asc_media_use_tmpfile:
 *
 * Check whether output slots can be unnamed O_TMPFILE inodes.
 *
 * Giving such an inode a name requires /proc, which is not necessarily mounted
 * inside a build chroot, so probe for it once and fall back to named temporary
 * files where it is missing.
 */
static gboolean
asc_media_use_tmpfile (void)
{
	static gsize initialized = 0;
	static gboolean have_proc_fd = FALSE;

	if (g_once_init_enter (&initialized)) {
		gboolean found = g_file_test ("/proc/self/fd", G_FILE_TEST_IS_DIR);
		if (!found)
			g_debug ("No /proc available, using named temporary media files.");
		have_proc_fd = found;
		g_once_init_leave (&initialized, 1);
	}

	return have_proc_fd;
}

#endif

/**
 * asc_media_out_slot_new:
 * @dir_fd: Descriptor of the directory the rendition will end up in.
 * @target: The rendition this slot is for.
 * @error: A #GError or %NULL
 *
 * Create an output slot for a single rendition.
 *
 * We prefer an unnamed O_TMPFILE inode: it never appears in the directory and
 * simply ceases to exist if we never link it into place, so a crash can not
 * litter the output tree. Filesystems without support for it - and platforms
 * that lack the flag entirely, such as FreeBSD - get a hidden temporary file
 * that we rename or unlink once the worker is done.
 *
 * Returns: (transfer full): a new #AscMediaOutSlot, or %NULL on error.
 */
static AscMediaOutSlot *
asc_media_out_slot_new (gint dir_fd, AscImageTarget *target, GError **error)
{
	AscMediaOutSlot *slot;
	g_autofree gchar *tmp_name = NULL;
	gint fd = -1;
	gint saved_errno = 0;

#ifdef O_TMPFILE
	if (asc_media_use_tmpfile ()) {
		fd = openat (dir_fd, ".", O_TMPFILE | O_WRONLY | O_CLOEXEC, 0644);
		saved_errno = errno;
		/* EOPNOTSUPP/EISDIR/EINVAL all mean "this filesystem can not do
		 * O_TMPFILE". Glibc folds O_DIRECTORY into O_TMPFILE, so a filesystem
		 * without support cheerfully opens the directory itself instead and we
		 * end up seeing EISDIR. */
		if (fd < 0 && saved_errno != EOPNOTSUPP && saved_errno != ENOTSUP &&
		    saved_errno != EISDIR && saved_errno != EINVAL) {
			g_set_error (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_FAILED,
				     "Unable to create an output file for '%s': %s",
				     target->name,
				     g_strerror (saved_errno));
			return NULL;
		}
	}
#endif

	if (fd < 0) {
		/* fall back to a named temporary file that we rename into place */
		for (guint i = 0; i < 100; i++) {
			g_autofree gchar *candidate = asc_media_make_tmp_name ();

			fd = openat (dir_fd,
				     candidate,
				     O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC,
				     0644);
			saved_errno = errno;
			if (fd >= 0) {
				tmp_name = g_steal_pointer (&candidate);
				break;
			}
			if (saved_errno != EEXIST)
				break;
		}
		if (fd < 0) {
			g_set_error (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_FAILED,
				     "Unable to create a temporary output file for '%s': %s",
				     target->name,
				     g_strerror (saved_errno));
			return NULL;
		}
	}

	slot = g_new0 (AscMediaOutSlot, 1);
	slot->fd = fd;
	slot->tmp_name = g_steal_pointer (&tmp_name);
	slot->target = target;

	return slot;
}

/**
 * asc_media_out_slot_commit:
 *
 * Publish the contents of a slot under the final name of its rendition,
 * replacing any previous file of that name atomically.
 */
static gboolean
asc_media_out_slot_commit (AscMediaOutSlot *slot, gint dir_fd, GError **error)
{
	g_autofree gchar *link_name = NULL;

	if (slot->tmp_name == NULL) {
#ifdef O_TMPFILE
		g_autofree gchar *proc_path = g_strdup_printf ("/proc/self/fd/%i", slot->fd);

		/* Linking an unnamed inode with AT_EMPTY_PATH requires
		 * CAP_DAC_READ_SEARCH, so we go through /proc instead, which is the
		 * documented way to give an O_TMPFILE inode a name. We link it under a
		 * temporary name first, so that the final rename is atomic. */
		link_name = asc_media_make_tmp_name ();
		if (linkat (AT_FDCWD, proc_path, dir_fd, link_name, AT_SYMLINK_FOLLOW) != 0) {
			g_set_error (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_FAILED,
				     "Unable to store the rendition '%s': %s",
				     slot->target->name,
				     g_strerror (errno));
			return FALSE;
		}
#else
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_FAILED,
				     "Output slot was neither named nor an unnamed inode.");
		return FALSE;
#endif
	} else {
		link_name = g_steal_pointer (&slot->tmp_name);
	}

	if (renameat (dir_fd, link_name, dir_fd, slot->target->name) != 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Unable to store the rendition '%s': %s",
			     slot->target->name,
			     g_strerror (errno));
		unlinkat (dir_fd, link_name, 0);
		return FALSE;
	}

	return TRUE;
}

/**
 * asc_media_out_slot_discard:
 *
 * Throw away a slot the worker did not use. An unnamed inode disappears on its
 * own once we close the descriptor, a named one has to be removed.
 */
static void
asc_media_out_slot_discard (AscMediaOutSlot *slot, gint dir_fd)
{
	if (slot->tmp_name == NULL)
		return;
	if (unlinkat (dir_fd, slot->tmp_name, 0) != 0)
		g_debug ("Unable to remove temporary media file '%s': %s",
			 slot->tmp_name,
			 g_strerror (errno));
	g_clear_pointer (&slot->tmp_name, g_free);
}

/**
 * asc_media_out_slots_discard_all:
 *
 * Drop every slot that was not published yet. Committing a slot clears its
 * temporary name, so this is a no-op for the ones we already stored and can be
 * run unconditionally on the way out.
 */
static void
asc_media_out_slots_discard_all (GPtrArray *slots, gint dir_fd)
{
	if (slots == NULL || dir_fd < 0)
		return;
	for (guint i = 0; i < slots->len; i++)
		asc_media_out_slot_discard (g_ptr_array_index (slots, i), dir_fd);
}

/**
 * asc_media_optimize_png:
 *
 * Shrink a finished PNG rendition with optipng, if that is enabled and the
 * binary was found.
 *
 * NOTE: This only handles trusted data we received from the sandboxed worker,
 * so we can run it outside of the sandbox.
 */
static gboolean
asc_media_optimize_png (const gchar *fname, GError **error)
{
	const gchar *optipng_path;
	const gchar *argv[3] = { NULL, NULL, NULL };
	g_autofree gchar *opng_stdout = NULL;
	g_autofree gchar *opng_stderr = NULL;
	g_autoptr(GError) tmp_error = NULL;
	gint exit_status = 0;

	if (!asc_globals_get_use_optipng ())
		return TRUE;

	optipng_path = asc_globals_get_optipng_binary ();
	if (optipng_path == NULL) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_FAILED,
				     "optipng not found in $PATH");
		return FALSE;
	}

	argv[0] = optipng_path;
	argv[1] = fname;

	/* NOTE: Maybe add an option to run optipng with stronger optimization? (>= -o4) */
	if (!g_spawn_sync (NULL, /* working directory */
			   (gchar **) argv,
			   NULL, /* envp */
			   G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
			   NULL, /* child setup */
			   NULL, /* user data */
			   &opng_stdout,
			   &opng_stderr,
			   &exit_status,
			   &tmp_error)) {
		g_propagate_prefixed_error (error,
					    g_steal_pointer (&tmp_error),
					    "Failed to spawn optipng.");
		return FALSE;
	}

	if (exit_status != 0) {
		/* FIXME: Maybe emit this as proper error, instead of just logging it? */
		g_warning ("Optipng on '%s' failed with error code %i: %s%s",
			   fname,
			   exit_status,
			   opng_stderr ? opng_stderr : "",
			   opng_stdout ? opng_stdout : "");
	}

	return TRUE;
}

/**
 * asc_media_lookup_dup_nonempty:
 *
 * Fetch a string from a vardict, returning %NULL instead of
 * an empty string.
 */
static gchar *
asc_media_lookup_dup_nonempty (GVariant *dict, const gchar *key)
{
	const gchar *value = NULL;

	g_variant_lookup (dict, key, "&s", &value);
	if (as_is_empty (value))
		return NULL;
	return g_strdup (value);
}

/**
 * asc_media_apply_slot_results:
 *
 * Transfer the per-rendition results from a response payload into the caller's
 * target structures, and publish the output slots the worker actually wrote to.
 *
 * A rendition that the worker skipped or failed on is not an error: its slot is
 * simply thrown away and the outcome recorded in the target.
 */
static gboolean
asc_media_apply_slot_results (GVariant *payload,
			      GPtrArray *slots,
			      gint dir_fd,
			      const gchar *out_dir,
			      GError **error)
{
	g_autoptr(GVariant) results = NULL;
	GVariantIter results_iter;
	GVariant *result_dict = NULL;
	guint i = 0;

	results = g_variant_lookup_value (payload, "results", G_VARIANT_TYPE ("aa{sv}"));
	if (results == NULL || g_variant_n_children (results) != slots->len) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Worker did not return results for all requested renditions.");
		return FALSE;
	}

	g_variant_iter_init (&results_iter, results);
	while (g_variant_iter_next (&results_iter, "@a{sv}", &result_dict)) {
		g_autoptr(GVariant) result = result_dict;
		g_autoptr(GError) tmp_error = NULL;
		AscMediaOutSlot *slot = g_ptr_array_index (slots, i++);
		AscImageTarget *target = slot->target;

		g_variant_lookup (result, "skipped", "b", &target->skipped);
		g_variant_lookup (result, "width", "i", &target->result_width);
		g_variant_lookup (result, "height", "i", &target->result_height);
		target->error_msg = asc_media_lookup_dup_nonempty (result, "error");

		if (target->skipped || target->error_msg != NULL) {
			asc_media_out_slot_discard (slot, dir_fd);
			continue;
		}

		if (!asc_media_out_slot_commit (slot, dir_fd, &tmp_error)) {
			target->error_msg = g_strdup (tmp_error->message);
			continue;
		}

		/* optipng only ever applies to PNG images */
		if (asc_image_format_from_filename (target->name) == ASC_IMAGE_FORMAT_PNG &&
		    as_flags_contains (target->save_flags, ASC_IMAGE_SAVE_FLAG_OPTIMIZE)) {
			g_autofree gchar *fname = NULL;

			fname = g_build_filename (out_dir, target->name, NULL);
			if (!asc_media_optimize_png (fname, &tmp_error))
				target->error_msg = g_strdup (tmp_error->message);
		}
	}

	return TRUE;
}

/**
 * asc_media_prepare_target:
 *
 * Validate a rendition target and reset its result fields, returning the image
 * format it should be encoded in.
 *
 * Returns: the target format, or %ASC_IMAGE_FORMAT_UNKNOWN if it is unusable.
 */
static AscImageFormat
asc_media_prepare_target (AscImageTarget *target)
{
	AscImageFormat format;

	target->skipped = FALSE;
	target->result_width = 0;
	target->result_height = 0;
	g_clear_pointer (&target->error_msg, g_free);

	if (!as_path_segment_verify (target->name)) {
		target->error_msg = g_strdup_printf ("Invalid rendition file name: %s",
						     target->name ? target->name : "(null)");
		return ASC_IMAGE_FORMAT_UNKNOWN;
	}

	format = asc_image_format_from_filename (target->name);
	if (format == ASC_IMAGE_FORMAT_UNKNOWN)
		target->error_msg = g_strdup_printf (
		    "Unable to determine the image format to save '%s' as.",
		    target->name);

	return format;
}

/**
 * asc_media_add_out_slot:
 *
 * Create an output slot for @target and reference its descriptor from the
 * entry currently being built.
 */
static gboolean
asc_media_add_out_slot (GVariantBuilder *eb,
			GUnixFDList *fds,
			GPtrArray *slots,
			gint dir_fd,
			AscImageTarget *target,
			GError **error)
{
	AscMediaOutSlot *slot;
	gint handle;

	if (slots->len >= ASC_MEDIA_MAX_OUT_SLOTS) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "A single media request can not produce more than %i files.",
			     ASC_MEDIA_MAX_OUT_SLOTS);
		return FALSE;
	}

	slot = asc_media_out_slot_new (dir_fd, target, error);
	if (slot == NULL)
		return FALSE;
	g_ptr_array_add (slots, slot);

	/* the list duplicates the descriptor, the slot keeps owning ours */
	handle = g_unix_fd_list_append (fds, slot->fd, error);
	if (handle < 0)
		return FALSE;
	g_variant_builder_add (eb, "{sv}", "fd", g_variant_new_handle (handle));

	return TRUE;
}

/**
 * asc_media_process_image:
 * @media: an #AscMedia instance.
 * @source: (not nullable): The #AscImageSource describing the image to process.
 * @targets: (nullable) (element-type AscImageTarget): Renditions to generate, or %NULL to just read image info.
 * @out_dir: Directory to store the renditions in, may be %NULL if @targets is empty.
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Load an image (in the media worker process) and store an arbitrary set of
 * scaled renditions of it in the given output directory. The directory is
 * created if it does not exist yet.
 *
 * On success, the dimensions the source image was loaded at are recorded in
 * @source, and the result fields of all @targets are updated. Individual
 * renditions may still have failed - this is recorded in their error message
 * and does not affect the return value of this function.
 *
 * Cancelling this operation terminates the media worker process, as its
 * pending reply would otherwise desynchronize the next request. A fresh
 * worker is spawned automatically for the next operation.
 *
 * Returns: %TRUE if the image was processed.
 *
 * Since: 1.2.0
 */
gboolean
asc_media_process_image (AscMedia *media,
			 AscImageSource *source,
			 GPtrArray *targets,
			 const gchar *out_dir,
			 GCancellable *cancellable,
			 GError **error)
{
	g_autoptr(GUnixFDList) fds = g_unix_fd_list_new ();
	g_autoptr(GVariant) payload = NULL;
	g_auto(GVariantBuilder) pb = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{sv}"));
	g_autoptr(GPtrArray) slots = NULL;
	gint dir_fd = -1;
	gint fd, handle;
	gboolean have_targets = targets != NULL && targets->len > 0;
	gboolean ret = FALSE;

	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);
	g_return_val_if_fail (source != NULL, FALSE);
	g_return_val_if_fail (!have_targets || out_dir != NULL, FALSE);

	fd = asc_memfd_new_sealed ("asc-image-data",
				   g_bytes_get_data (source->data, NULL),
				   g_bytes_get_size (source->data),
				   error);
	if (fd < 0)
		return FALSE;
	handle = asc_media_fdlist_append (fds, fd, error);
	if (handle < 0)
		return FALSE;
	g_variant_builder_add (&pb, "{sv}", "image-fd", g_variant_new_handle (handle));

	g_variant_builder_add (&pb,
			       "{sv}",
			       "load-width",
			       g_variant_new_int32 (source->render_width));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "load-height",
			       g_variant_new_int32 (source->render_height));

	slots = g_ptr_array_new_with_free_func ((GDestroyNotify) asc_media_out_slot_free);
	if (have_targets) {
		GVariantBuilder targets_builder;

		dir_fd = asc_media_open_out_dir (out_dir, error);
		if (dir_fd < 0)
			return FALSE;

		g_variant_builder_init (&targets_builder, G_VARIANT_TYPE ("aa{sv}"));
		for (guint i = 0; i < targets->len; i++) {
			AscImageTarget *target = g_ptr_array_index (targets, i);
			AscImageFormat format = asc_media_prepare_target (target);
			GVariantBuilder tb;

			/* targets we can not name a file for are reported right away and
			 * never reach the worker */
			if (format == ASC_IMAGE_FORMAT_UNKNOWN)
				continue;

			g_variant_builder_init (&tb, G_VARIANT_TYPE ("a{sv}"));
			if (!asc_media_add_out_slot (&tb, fds, slots, dir_fd, target, error)) {
				g_variant_builder_clear (&tb);
				g_variant_builder_clear (&targets_builder);
				goto out;
			}
			g_variant_builder_add (&tb,
					       "{sv}",
					       "format",
					       g_variant_new_uint32 (format));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "width",
					       g_variant_new_int32 (target->width));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "height",
					       g_variant_new_int32 (target->height));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "scale-mode",
					       g_variant_new_uint32 (target->scale_mode));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "save-flags",
					       g_variant_new_uint32 (target->save_flags));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "only-downscale",
					       g_variant_new_boolean (target->only_downscale));
			g_variant_builder_add (
			    &tb,
			    "{sv}",
			    "min-src-size",
			    g_variant_new ("(ii)", target->min_src_width, target->min_src_height));
			g_variant_builder_add (
			    &tb,
			    "{sv}",
			    "max-src-size",
			    g_variant_new ("(ii)", target->max_src_width, target->max_src_height));
			g_variant_builder_add_value (&targets_builder, g_variant_builder_end (&tb));
		}
		g_variant_builder_add (&pb,
				       "{sv}",
				       "targets",
				       g_variant_builder_end (&targets_builder));
	}

	payload = asc_media_call (media,
				  ASC_MEDIA_OP_PROCESS_IMAGE,
				  g_variant_builder_end (&pb),
				  fds,
				  cancellable,
				  error);
	if (payload == NULL)
		goto out;

	source->width = 0;
	source->height = 0;
	g_variant_lookup (payload, "src-width", "i", &source->width);
	g_variant_lookup (payload, "src-height", "i", &source->height);

	ret = asc_media_apply_slot_results (payload, slots, dir_fd, out_dir, error);

out:
	asc_media_out_slots_discard_all (slots, dir_fd);
	if (dir_fd >= 0)
		close (dir_fd);
	return ret;
}

/**
 * asc_media_add_font_params:
 *
 * Add the common parameters of all font operations to a request.
 */
static gboolean
asc_media_add_font_params (GVariantBuilder *pb,
			   GUnixFDList *fds,
			   GBytes *font_data,
			   const gchar *basename,
			   const gchar *preferred_language,
			   const gchar *const *extra_languages,
			   const gchar *custom_sample_text,
			   const gchar *custom_icon_text,
			   GError **error)
{
	gint fd, handle;

	fd = asc_memfd_new_sealed ("asc-font-data",
				   g_bytes_get_data (font_data, NULL),
				   g_bytes_get_size (font_data),
				   error);
	if (fd < 0)
		return FALSE;
	handle = asc_media_fdlist_append (fds, fd, error);
	if (handle < 0)
		return FALSE;
	g_variant_builder_add (pb, "{sv}", "font-fd", g_variant_new_handle (handle));
	g_variant_builder_add (pb, "{sv}", "basename", g_variant_new_string (basename));

	if (!as_is_empty (preferred_language))
		g_variant_builder_add (pb,
				       "{sv}",
				       "preferred-language",
				       g_variant_new_string (preferred_language));
	if (extra_languages != NULL && extra_languages[0] != NULL)
		g_variant_builder_add (pb,
				       "{sv}",
				       "extra-languages",
				       g_variant_new_strv (extra_languages, -1));
	if (!as_is_empty (custom_sample_text))
		g_variant_builder_add (pb,
				       "{sv}",
				       "sample-text",
				       g_variant_new_string (custom_sample_text));
	if (!as_is_empty (custom_icon_text))
		g_variant_builder_add (pb,
				       "{sv}",
				       "sample-icon-text",
				       g_variant_new_string (custom_icon_text));

	return TRUE;
}

/**
 * asc_media_read_font_info:
 * @media: an #AscMedia instance.
 * @font_data: The font file contents.
 * @basename: Basename of the font file, used for heuristics.
 * @preferred_language: (nullable): Language to prefer for font samples.
 * @extra_languages: (nullable): Additional languages this font supports.
 * @custom_sample_text: (nullable): Custom sample text override.
 * @custom_icon_text: (nullable): Custom icon text override (up to 3 characters).
 * @error: A #GError or %NULL
 *
 * Read all metadata from a font file (in the media worker process),
 * including the sample texts that would be used for rendering it.
 *
 * Returns: (transfer full): An #AscFontInfo, or %NULL on error.
 */
AscFontInfo *
asc_media_read_font_info (AscMedia *media,
			  GBytes *font_data,
			  const gchar *basename,
			  const gchar *preferred_language,
			  const gchar *const *extra_languages,
			  const gchar *custom_sample_text,
			  const gchar *custom_icon_text,
			  GError **error)
{
	g_autoptr(GUnixFDList) fds = g_unix_fd_list_new ();
	g_autoptr(GVariant) payload = NULL;
	g_autoptr(AscFontInfo) info = NULL;
	g_auto(GVariantBuilder) pb = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{sv}"));

	g_return_val_if_fail (ASC_IS_MEDIA (media), NULL);
	g_return_val_if_fail (font_data != NULL, NULL);
	g_return_val_if_fail (basename != NULL, NULL);

	if (!asc_media_add_font_params (&pb,
					fds,
					font_data,
					basename,
					preferred_language,
					extra_languages,
					custom_sample_text,
					custom_icon_text,
					error))
		return NULL;

	payload = asc_media_call (media,
				  ASC_MEDIA_OP_FONT_INFO,
				  g_variant_builder_end (&pb),
				  fds,
				  NULL, /* cancellable */
				  error);
	if (payload == NULL)
		return NULL;

	info = g_new0 (AscFontInfo, 1);
	info->family = asc_media_lookup_dup_nonempty (payload, "family");
	info->style = asc_media_lookup_dup_nonempty (payload, "style");
	info->fullname = asc_media_lookup_dup_nonempty (payload, "fullname");
	info->id = asc_media_lookup_dup_nonempty (payload, "id");
	info->description = asc_media_lookup_dup_nonempty (payload, "description");
	info->homepage = asc_media_lookup_dup_nonempty (payload, "homepage");
	info->preferred_language = asc_media_lookup_dup_nonempty (payload, "preferred-language");
	info->sample_text = asc_media_lookup_dup_nonempty (payload, "sample-text");
	info->sample_icon_text = asc_media_lookup_dup_nonempty (payload, "sample-icon-text");
	g_variant_lookup (payload, "languages", "^as", &info->languages);

	return g_steal_pointer (&info);
}

/**
 * asc_media_render_font:
 *
 * Shared implementation for font card & font icon rendering.
 */
static gboolean
asc_media_render_font (AscMedia *media,
		       AscMediaOp op,
		       GBytes *font_data,
		       const gchar *basename,
		       const gchar *preferred_language,
		       const gchar *const *extra_languages,
		       const gchar *custom_sample_text,
		       const gchar *custom_icon_text,
		       const gchar *info_label,
		       const gchar *out_dir,
		       GPtrArray *targets,
		       GError **error)
{
	g_autoptr(GUnixFDList) fds = g_unix_fd_list_new ();
	g_autoptr(GVariant) payload = NULL;
	g_auto(GVariantBuilder) pb = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{sv}"));
	g_autoptr(GPtrArray) slots = NULL;
	GVariantBuilder entries_builder;
	gint dir_fd = -1;
	gboolean ret = FALSE;

	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);
	g_return_val_if_fail (font_data != NULL, FALSE);
	g_return_val_if_fail (basename != NULL, FALSE);
	g_return_val_if_fail (out_dir != NULL, FALSE);
	g_return_val_if_fail (targets != NULL && targets->len > 0, FALSE);

	if (!asc_media_add_font_params (&pb,
					fds,
					font_data,
					basename,
					preferred_language,
					extra_languages,
					custom_sample_text,
					custom_icon_text,
					error))
		return FALSE;

	if (!as_is_empty (info_label))
		g_variant_builder_add (&pb,
				       "{sv}",
				       "info-label",
				       g_variant_new_string (info_label));

	dir_fd = asc_media_open_out_dir (out_dir, error);
	if (dir_fd < 0)
		return FALSE;

	slots = g_ptr_array_new_with_free_func ((GDestroyNotify) asc_media_out_slot_free);
	g_variant_builder_init (&entries_builder, G_VARIANT_TYPE ("aa{sv}"));
	for (guint i = 0; i < targets->len; i++) {
		AscImageTarget *target = g_ptr_array_index (targets, i);
		AscImageFormat format = asc_media_prepare_target (target);
		GVariantBuilder eb;

		/* entries we can not name a file for are reported right away and
		 * never reach the worker */
		if (format == ASC_IMAGE_FORMAT_UNKNOWN)
			continue;

		g_variant_builder_init (&eb, G_VARIANT_TYPE ("a{sv}"));
		if (!asc_media_add_out_slot (&eb, fds, slots, dir_fd, target, error)) {
			g_variant_builder_clear (&eb);
			g_variant_builder_clear (&entries_builder);
			goto out;
		}
		g_variant_builder_add (&eb, "{sv}", "format", g_variant_new_uint32 (format));
		g_variant_builder_add (&eb, "{sv}", "width", g_variant_new_int32 (target->width));
		g_variant_builder_add (&eb, "{sv}", "height", g_variant_new_int32 (target->height));
		g_variant_builder_add_value (&entries_builder, g_variant_builder_end (&eb));
	}

	if (slots->len == 0) {
		/* nothing usable was requested, all targets carry their error already */
		g_variant_builder_clear (&entries_builder);
		g_variant_builder_clear (&pb);
		ret = TRUE;
		goto out;
	}
	g_variant_builder_add (&pb, "{sv}", "entries", g_variant_builder_end (&entries_builder));

	payload = asc_media_call (media,
				  op,
				  g_variant_builder_end (&pb),
				  fds,
				  NULL, /* cancellable */
				  error);
	if (payload == NULL)
		goto out;

	ret = asc_media_apply_slot_results (payload, slots, dir_fd, out_dir, error);

out:
	asc_media_out_slots_discard_all (slots, dir_fd);
	if (dir_fd >= 0)
		close (dir_fd);
	return ret;
}

/**
 * asc_media_render_font_card:
 * @media: an #AscMedia instance.
 * @font_data: The font file contents.
 * @basename: Basename of the font file, used for heuristics.
 * @preferred_language: (nullable): Language to prefer for font samples.
 * @extra_languages: (nullable): Additional languages this font supports.
 * @custom_sample_text: (nullable): Custom sample text override.
 * @custom_icon_text: (nullable): Custom icon text override.
 * @info_label: (nullable): Short info label for the card, or %NULL for the default.
 * @out_dir: Directory to store the rendered cards in.
 * @targets: (element-type AscImageTarget): Card sizes to render.
 * @error: A #GError or %NULL
 *
 * Render font specimen cards showcasing the given font (in the media worker
 * process) and store them as PNG images in the output directory.
 *
 * The card render may adjust the image height, so the target result
 * dimensions must be used by the caller. Failures of individual renditions
 * are recorded in the targets' %error_msg fields.
 *
 * Returns: %TRUE if the font was processed.
 */
gboolean
asc_media_render_font_card (AscMedia *media,
			    GBytes *font_data,
			    const gchar *basename,
			    const gchar *preferred_language,
			    const gchar *const *extra_languages,
			    const gchar *custom_sample_text,
			    const gchar *custom_icon_text,
			    const gchar *info_label,
			    const gchar *out_dir,
			    GPtrArray *targets,
			    GError **error)
{
	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);

	return asc_media_render_font (media,
				      ASC_MEDIA_OP_RENDER_FONT_CARD,
				      font_data,
				      basename,
				      preferred_language,
				      extra_languages,
				      custom_sample_text,
				      custom_icon_text,
				      info_label,
				      out_dir,
				      targets,
				      error);
}

/**
 * asc_media_render_font_icon:
 * @media: an #AscMedia instance.
 * @font_data: The font file contents.
 * @basename: Basename of the font file, used for heuristics.
 * @preferred_language: (nullable): Language to prefer for font samples.
 * @extra_languages: (nullable): Additional languages this font supports.
 * @custom_sample_text: (nullable): Custom sample text override.
 * @custom_icon_text: (nullable): Custom icon text override.
 * @out_dir: Directory to store the rendered icons in.
 * @targets: (element-type AscImageTarget): Icon sizes to render (the target width is used as canvas size).
 * @error: A #GError or %NULL
 *
 * Render icons for the given font (in the media worker process), each
 * consisting of a background shape with the font's sample icon text on
 * top, and store them as PNG images in the output directory.
 *
 * Failures of individual renditions are recorded in the targets'
 * %error_msg fields.
 *
 * Returns: %TRUE if the font was processed.
 */
gboolean
asc_media_render_font_icon (AscMedia *media,
			    GBytes *font_data,
			    const gchar *basename,
			    const gchar *preferred_language,
			    const gchar *const *extra_languages,
			    const gchar *custom_sample_text,
			    const gchar *custom_icon_text,
			    const gchar *out_dir,
			    GPtrArray *targets,
			    GError **error)
{
	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);

	return asc_media_render_font (media,
				      ASC_MEDIA_OP_RENDER_FONT_ICON,
				      font_data,
				      basename,
				      preferred_language,
				      extra_languages,
				      custom_sample_text,
				      custom_icon_text,
				      NULL, /* info label */
				      out_dir,
				      targets,
				      error);
}

/**
 * asc_media_probe_video:
 * @media: an #AscMedia instance.
 * @video_fname: Path to the video file to probe.
 * @codec_name: (out) (optional): Name of the video codec.
 * @audio_codec_name: (out) (optional): Name of the audio codec, or %NULL if the video has no audio.
 * @format_name: (out) (optional): Name of the container format.
 * @width: (out) (optional): Video width.
 * @height: (out) (optional): Video height.
 * @error: A #GError or %NULL
 *
 * Probe a video file for its container format, codecs and dimensions
 * (using ffprobe, run by the media worker process).
 *
 * The video file is passed to the worker as a read-only file descriptor,
 * so its contents never have to be held in memory.
 *
 * Returns: %TRUE if the video was probed successfully.
 */
gboolean
asc_media_probe_video (AscMedia *media,
		       const gchar *video_fname,
		       gchar **codec_name,
		       gchar **audio_codec_name,
		       gchar **format_name,
		       gint *width,
		       gint *height,
		       GError **error)
{
	g_autoptr(GUnixFDList) fds = g_unix_fd_list_new ();
	g_autoptr(GVariant) payload = NULL;
	g_auto(GVariantBuilder) pb = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{sv}"));
	gint fd, handle;

	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);
	g_return_val_if_fail (video_fname != NULL, FALSE);

	fd = open (video_fname, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Unable to open video file '%s': %s",
			     video_fname,
			     g_strerror (errno));
		return FALSE;
	}
	handle = asc_media_fdlist_append (fds, fd, error);
	if (handle < 0)
		return FALSE;
	g_variant_builder_add (&pb, "{sv}", "video-fd", g_variant_new_handle (handle));

	payload = asc_media_call (media,
				  ASC_MEDIA_OP_PROBE_VIDEO,
				  g_variant_builder_end (&pb),
				  fds,
				  NULL, /* cancellable */
				  error);
	if (payload == NULL)
		return FALSE;

	if (codec_name != NULL)
		*codec_name = asc_media_lookup_dup_nonempty (payload, "codec-name");
	if (audio_codec_name != NULL)
		*audio_codec_name = asc_media_lookup_dup_nonempty (payload, "audio-codec-name");
	if (format_name != NULL)
		*format_name = asc_media_lookup_dup_nonempty (payload, "format-name");
	if (width != NULL)
		g_variant_lookup (payload, "width", "i", width);
	if (height != NULL)
		g_variant_lookup (payload, "height", "i", height);

	return TRUE;
}
