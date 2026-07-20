/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-utils-screenshots
 * @short_description: Screenshots processing functions for appstream-compose
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-utils-screenshots.h"

#include <glib/gstdio.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>

#include "as-utils-private.h"

#include "asc-globals.h"
#include "asc-utils.h"
#include "asc-media-private.h"

struct {
	gint width;
	gint height;
} target_screenshot_sizes[] = {
	{ 1248, 702 },
	{ 752,  423 },
	{ 624,  351 },
	{ 224,  126 },
	{ 0,    0   }
};

static AscVideoInfo *
asc_video_info_new (void)
{
	AscVideoInfo *vinfo;
	vinfo = g_new0 (AscVideoInfo, 1);
	return vinfo;
}

void
asc_video_info_free (AscVideoInfo *vinfo)
{
	if (vinfo == NULL)
		return;
	g_free (vinfo->codec_name);
	g_free (vinfo->audio_codec_name);
	g_free (vinfo->format_name);
	g_free (vinfo);
}

static gsize
asc_get_filesize (const gchar *filename)
{
	struct stat st;
	if (lstat (filename, &st) == -1) {
		g_debug ("Unable to determine size of file '%s': %s", filename, g_strerror (errno));
		return 0;
	}
	return st.st_size;
}

/**
 * asc_extract_video_info: (skip):
 */
AscVideoInfo *
asc_extract_video_info (AscResult *cres, AsComponent *cpt, AscMedia *media, const gchar *vid_fname)
{
	AscVideoInfo *vinfo = NULL;
	g_autofree gchar *vid_basename = NULL;
	g_autoptr(GMappedFile) mfile = NULL;
	g_autoptr(GBytes) vid_bytes = NULL;
	gboolean audio_okay = FALSE;
	g_autoptr(GError) error = NULL;

	if (asc_globals_get_ffprobe_binary () == NULL)
		return NULL;
	if (vid_fname == NULL)
		return NULL;

	vinfo = asc_video_info_new ();
	vid_basename = g_path_get_basename (vid_fname);

	mfile = g_mapped_file_new (vid_fname, FALSE, &error);
	if (mfile == NULL) {
		g_warning ("Unable to read video file '%s': %s", vid_fname, error->message);
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-video-check-failed",
				     "fname",
				     vid_basename,
				     "msg",
				     error->message,
				     NULL);
		return vinfo;
	}
	vid_bytes = g_mapped_file_get_bytes (mfile);

	/* have the media worker run ffprobe on the video */
	if (!asc_media_probe_video (media,
				    vid_bytes,
				    vid_basename,
				    &vinfo->codec_name,
				    &vinfo->audio_codec_name,
				    &vinfo->format_name,
				    &vinfo->width,
				    &vinfo->height,
				    &error)) {
		g_warning ("Failed to probe video '%s': %s", vid_fname, error->message);
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-video-check-failed",
				     "fname",
				     vid_basename,
				     "msg",
				     error->message,
				     NULL);
		return vinfo;
	}

	/* Check whether the video container is a supported format
	 * Since WebM is a subset of Matroska, FFmpeg lists them as one thing
	 * and us distinguishing by file extension here is a bit artificial. */
	if (g_strstr_len (vinfo->format_name, -1, "matroska") != NULL)
		vinfo->container_kind = AS_VIDEO_CONTAINER_KIND_MKV;
	if (g_strstr_len (vinfo->format_name, -1, "webm") != NULL) {
		if (g_str_has_suffix (vid_basename, ".webm"))
			vinfo->container_kind = AS_VIDEO_CONTAINER_KIND_WEBM;
	}

	/* check codec */
	if (g_strcmp0 (vinfo->codec_name, "av1") == 0)
		vinfo->codec_kind = AS_VIDEO_CODEC_KIND_AV1;
	else if (g_strcmp0 (vinfo->codec_name, "vp9") == 0)
		vinfo->codec_kind = AS_VIDEO_CODEC_KIND_VP9;

	/* check for audio */
	audio_okay = TRUE;
	if (vinfo->audio_codec_name != NULL) {
		/* this video has an audio track... meh. */
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-video-has-audio",
				     "fname",
				     vid_basename,
				     NULL);
		if (g_strcmp0 (vinfo->audio_codec_name, "opus") != 0) {
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-video-audio-codec-unsupported",
					     "fname",
					     vid_basename,
					     "codec",
					     vinfo->audio_codec_name,
					     NULL);
			audio_okay = FALSE;
		}
	}

	/* A video file may contain multiple streams, so this check isn't extensive, but it protects against 99% of cases where
	 * people were using unsupported formats. */
	vinfo->is_acceptable = (vinfo->container_kind != AS_VIDEO_CONTAINER_KIND_UNKNOWN) &&
			       (vinfo->codec_kind != AS_VIDEO_CODEC_KIND_UNKNOWN) && audio_okay;

	if (!vinfo->is_acceptable) {
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-video-format-unsupported",
				     "fname",
				     vid_basename,
				     "codec",
				     vinfo->codec_name,
				     "container",
				     vinfo->format_name,
				     NULL);
	}

	return vinfo;
}

/**
 * asc_process_screenshot_videos: (skip):
 */
static AsScreenshot *
asc_process_screenshot_videos (AscResult *cres,
			       AsComponent *cpt,
			       AsScreenshot *scr,
			       AsCurl *acurl,
			       AscMedia *media,
			       const gchar *scr_export_dir,
			       const gchar *scr_base_url,
			       const gssize max_size_bytes,
			       gboolean store_screenshots,
			       guint scr_no)
{
	GPtrArray *vids = NULL;
	g_autoptr(GPtrArray) valid_vids = NULL;

	vids = as_screenshot_get_videos_all (scr);
	if (vids->len == 0) {
		asc_result_add_hint_simple (cres, cpt, "metainfo-screenshot-but-no-media");
		return NULL;
	}

	/* if size is zero, we can't store any screenshots */
	if (max_size_bytes == 0)
		store_screenshots = FALSE;

	/* ensure export dir exists */
	if (g_mkdir_with_parents (scr_export_dir, 0755) != 0)
		g_warning ("Failed to create directory tree '%s'", scr_export_dir);

	valid_vids = g_ptr_array_new_with_free_func (g_object_unref);
	for (guint i = 0; i < vids->len; i++) {
		const gchar *orig_vid_url = NULL;
		const gchar *video_locale = NULL;
		g_autofree gchar *scr_vid_name = NULL;
		g_autofree gchar *scr_vid_path = NULL;
		g_autofree gchar *scr_vid_url = NULL;
		g_autofree gchar *fname_from_url = NULL;
		AscVideoInfo *vinfo = NULL;
		g_autoptr(GError) error = NULL;
		gsize video_size;
		AsVideo *vid = AS_VIDEO (g_ptr_array_index (vids, i));

		orig_vid_url = as_video_get_url (vid);
		if (orig_vid_url == NULL)
			continue;

		video_locale = as_video_get_locale (vid);
		fname_from_url = asc_filename_from_url (orig_vid_url);

		if (g_strcmp0 (video_locale, "C") == 0)
			scr_vid_name = g_strdup_printf ("vid%i-%i_%s", scr_no, i, fname_from_url);
		else
			scr_vid_name = g_strdup_printf ("vid%i-%i_%s_%s",
							scr_no,
							i,
							fname_from_url,
							video_locale);
		scr_vid_path = g_build_filename (scr_export_dir, scr_vid_name, NULL);
		scr_vid_url = g_build_filename (scr_base_url, scr_vid_name, NULL);

		if (!as_curl_download_to_filename (acurl, orig_vid_url, scr_vid_path, &error)) {
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-download-error",
					     "url",
					     orig_vid_url,
					     "error",
					     error->message,
					     NULL);
			return NULL;
		}

		video_size = asc_get_filesize (scr_vid_path);
		if (max_size_bytes > 0 && video_size > (gsize) max_size_bytes) {
			g_autofree gchar *max_vid_size_str = NULL;
			g_autofree gchar *vid_size_str = NULL;

			max_vid_size_str = g_format_size (max_size_bytes);
			vid_size_str = g_format_size (video_size);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-video-too-big",
					     "fname",
					     scr_vid_name,
					     "max_size",
					     max_vid_size_str,
					     "size",
					     vid_size_str,
					     NULL);
			g_unlink (scr_vid_path);
			continue;
		}

		vinfo = asc_extract_video_info (cres, cpt, media, scr_vid_path);
		/* if vinfo is NULL, we couldn't gather the required info because ffprobe is missing.
		 * continue with incomplete metadata in that case */
		if (vinfo != NULL) {
			if (!vinfo->is_acceptable) {
				asc_video_info_free (vinfo);
				g_remove (scr_vid_path);
				/* we already marked the screenshot to be ignored at this point */
				continue;
			}

			as_video_set_codec_kind (vid, vinfo->codec_kind);
			as_video_set_container_kind (vid, vinfo->container_kind);
			as_video_set_width (vid, vinfo->width);
			as_video_set_height (vid, vinfo->height);

			asc_video_info_free (vinfo);
		}

		/* if we should not create a screenshots store, we'll later delete the just-downloaded file
		 * and set the original upstream URL as source.
		 * we still needed to download the video to get information about its size and ensure
		 * its metadata is correct. */
		if (store_screenshots)
			as_video_set_url (vid, scr_vid_url);
		else
			as_video_set_url (vid, orig_vid_url);

		g_ptr_array_add (valid_vids, g_object_ref (vid));
	}

	/* if we don't store screenshots, the export dir is only a temporary cache */
	if (!store_screenshots)
		as_utils_delete_dir_recursive (scr_export_dir);

	/* if we have no valid videos, ignore the screenshot */
	if (valid_vids->len == 0)
		return NULL;

	/* drop all videos */
	g_ptr_array_remove_range (vids, 0, vids->len);

	/* add the valid ones back */
	for (guint i = 0; i < valid_vids->len; i++)
		as_screenshot_add_video (scr, AS_VIDEO (g_ptr_array_index (valid_vids, i)));

	return g_object_ref (scr);
}

/**
 * asc_process_screenshot_images_lang:
 *
 * Process image @orig_img for screenshot @scr and language @locale
 *
 * Returns: %TRUE on success, %FALSE if we rejected this screenshot.
 */
static gboolean
asc_process_screenshot_images_lang (AscResult *cres,
				    AsComponent *cpt,
				    AsScreenshot *scr,
				    AsImage *orig_img,
				    const gchar *locale,
				    AsCurl *acurl,
				    AscMedia *media,
				    const gchar *scr_export_dir,
				    const gchar *scr_base_url,
				    const gssize max_size_bytes,
				    gboolean store_screenshots,
				    guint scr_no)
{
	const gchar *orig_img_url = NULL;
	const gchar *gcid = NULL;
	g_autoptr(GBytes) img_bytes = NULL;
	gsize img_data_len;
	gint source_scr_width;
	gint source_scr_height;
	guint source_scr_scale;
	gboolean thumbnails_generated = FALSE;
	g_autoptr(GError) error = NULL;

	orig_img_url = as_image_get_url (orig_img);
	if (orig_img_url == NULL)
		return FALSE;

	/* if size is zero, we can't store any screenshots */
	if (max_size_bytes == 0)
		store_screenshots = FALSE;

	{
		AscImageFormat image_format = asc_image_format_from_filename (orig_img_url);

		/* we do not allow vector graphics as screenshots */
		if (image_format == ASC_IMAGE_FORMAT_SVG || image_format == ASC_IMAGE_FORMAT_SVGZ) {
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-image-is-svg",
					     "url",
					     orig_img_url,
					     NULL);
			return FALSE;
		}
	}

	/* download our image */
	img_bytes = as_curl_download_bytes (acurl, orig_img_url, &error);
	if (img_bytes == NULL) {
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-download-error",
				     "url",
				     orig_img_url,
				     "error",
				     error->message,
				     NULL);
		return FALSE;
	}
	img_data_len = g_bytes_get_size (img_bytes);

	gcid = asc_result_gcid_for_component (cres, cpt);
	if (gcid == NULL) {
		asc_result_add_hint (
		    cres,
		    cpt,
		    "internal-error",
		    "msg",
		    "No global ID could be found for component when processing screenshot images.",
		    NULL);
		return FALSE;
	}

	if (max_size_bytes > 0 && img_data_len > (gsize) max_size_bytes) {
		g_autofree gchar *max_img_size_str = NULL;
		g_autofree gchar *img_size_str = NULL;

		max_img_size_str = g_format_size (max_size_bytes);
		img_size_str = g_format_size (img_data_len);
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-image-too-big",
				     "fname",
				     orig_img_url,
				     "max_size",
				     max_img_size_str,
				     "size",
				     img_size_str,
				     NULL);
		return FALSE;
	}

	source_scr_scale = as_image_get_scale (orig_img);

	/* if we should not create a screenshots store, we only read the image
	 * dimensions and keep the original upstream URL as source */
	if (!store_screenshots) {
		g_autoptr(AsImage) simg = NULL;
		gint src_width = 0;
		gint src_height = 0;

		if (!asc_media_process_image (media,
					      img_bytes,
					      ASC_IMAGE_FORMAT_UNKNOWN,
					      0, /* load width */
					      0, /* load height */
					      ASC_IMAGE_LOAD_FLAG_NONE,
					      NULL, /* output dir */
					      NULL, /* targets */
					      &src_width,
					      &src_height,
					      &error)) {
			g_autofree gchar *msg = g_strdup_printf (
			    "Could not load source screenshot for storing: %s",
			    error->message);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-save-error",
					     "url",
					     orig_img_url,
					     "error",
					     msg,
					     NULL);
			return FALSE;
		}

		simg = as_image_new ();
		as_image_set_kind (simg, AS_IMAGE_KIND_SOURCE);
		as_image_set_locale (simg, locale);
		as_image_set_width (simg, src_width);
		as_image_set_height (simg, src_height);
		as_image_set_scale (simg, source_scr_scale);
		as_image_set_url (simg, orig_img_url);
		as_screenshot_add_image (scr, simg);
		return TRUE;
	}

	/* ensure export dir exists */
	if (g_mkdir_with_parents (scr_export_dir, 0755) != 0)
		g_warning ("Failed to create directory tree '%s'", scr_export_dir);

	{
		g_autoptr(GPtrArray) img_targets = NULL;
		g_autofree gchar *src_img_name = NULL;
		g_autofree gchar *src_img_url = NULL;
		AscImageTarget *src_target = NULL;
		g_autoptr(AsImage) simg = NULL;
		gint src_width = 0;
		gint src_height = 0;

		if (g_strcmp0 (locale, "C") == 0)
			src_img_name = g_strdup_printf ("image-%i_orig.png", scr_no);
		else
			src_img_name = g_strdup_printf ("image-%i_%s_orig.png", scr_no, locale);
		src_img_url = g_build_filename (scr_base_url, src_img_name, NULL);

		/* save the source screenshot as PNG image, via the media worker */
		img_targets = g_ptr_array_new_with_free_func (
		    (GDestroyNotify) asc_image_target_free);
		src_target = asc_image_target_new (src_img_name, ASC_IMAGE_SCALE_MODE_NONE, 0, 0);
		src_target->save_flags = ASC_IMAGE_SAVE_FLAG_OPTIMIZE;
		g_ptr_array_add (img_targets, src_target);

		if (!asc_media_process_image (media,
					      img_bytes,
					      ASC_IMAGE_FORMAT_UNKNOWN,
					      0, /* load width */
					      0, /* load height */
					      ASC_IMAGE_LOAD_FLAG_NONE,
					      scr_export_dir,
					      img_targets,
					      &src_width,
					      &src_height,
					      &error)) {
			g_autofree gchar *msg = g_strdup_printf (
			    "Could not load source screenshot for storing: %s",
			    error->message);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-save-error",
					     "url",
					     orig_img_url,
					     "error",
					     msg,
					     NULL);
			return FALSE;
		}
		if (src_target->error_msg != NULL) {
			g_autofree gchar *msg = g_strdup_printf (
			    "Can not store source screenshot: %s",
			    src_target->error_msg);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-save-error",
					     "url",
					     orig_img_url,
					     "error",
					     msg,
					     NULL);
			return FALSE;
		}

		source_scr_width = src_width;
		source_scr_height = src_height;

		simg = as_image_new ();
		as_image_set_kind (simg, AS_IMAGE_KIND_SOURCE);
		as_image_set_locale (simg, locale);
		as_image_set_width (simg, source_scr_width);
		as_image_set_height (simg, source_scr_height);
		as_image_set_scale (simg, source_scr_scale);
		as_image_set_url (simg, src_img_url);
		as_screenshot_add_image (scr, simg);
	}

	/* generate & save thumbnails for the screenshot image
	 * NOTE: we ignore higher scaling factors for thumbnailing for now */
	thumbnails_generated = FALSE;
	if (source_scr_scale <= 1) {
		g_autoptr(GPtrArray) thumb_targets = NULL;

		thumb_targets = g_ptr_array_new_with_free_func (
		    (GDestroyNotify) asc_image_target_free);
		for (guint i = 0; target_screenshot_sizes[i].width != 0; i++) {
			g_autofree gchar *thumb_img_name = NULL;
			AscImageTarget *target = NULL;
			gint thumb_width;
			gint thumb_height;
			double scale;

			gint target_width = target_screenshot_sizes[i].width;
			gint target_height = target_screenshot_sizes[i].height;

			/* calculate the expected thumbnail dimensions, as we need to know
			 * the file names before dispatching the render request */
			if (target_width > target_height) {
				scale = (double) target_width / (double) source_scr_width;
				thumb_width = target_width;
				thumb_height = floor (source_scr_height * scale);
			} else {
				scale = (double) target_height / (double) source_scr_height;
				thumb_width = floor (source_scr_width * scale);
				thumb_height = target_height;
			}

			/* create thumbnail storage name */
			if (g_strcmp0 (locale, "C") == 0)
				thumb_img_name = g_strdup_printf ("image-%i_%ix%i@%i.png",
								  scr_no,
								  thumb_width,
								  thumb_height,
								  1);
			else
				thumb_img_name = g_strdup_printf ("image-%i_%ix%i@%i_%s.png",
								  scr_no,
								  thumb_width,
								  thumb_height,
								  1,
								  locale);

			target = asc_image_target_new (thumb_img_name,
						       target_width > target_height
							   ? ASC_IMAGE_SCALE_MODE_FIT_WIDTH
							   : ASC_IMAGE_SCALE_MODE_FIT_HEIGHT,
						       target_width,
						       target_height);
			target->save_flags = ASC_IMAGE_SAVE_FLAG_OPTIMIZE;
			/* ensure we will only downscale the screenshot for thumbnailing */
			target->only_downscale = TRUE;
			g_ptr_array_add (thumb_targets, target);
		}

		if (!asc_media_process_image (media,
					      img_bytes,
					      ASC_IMAGE_FORMAT_UNKNOWN,
					      0, /* load width */
					      0, /* load height */
					      ASC_IMAGE_LOAD_FLAG_NONE,
					      scr_export_dir,
					      thumb_targets,
					      NULL,
					      NULL,
					      &error)) {
			g_autofree gchar *msg = g_strdup_printf (
			    "Could not load source screenshot for thumbnailing: %s",
			    error->message);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-save-error",
					     "url",
					     orig_img_url,
					     "error",
					     msg,
					     NULL);
			g_error_free (g_steal_pointer (&error));
			g_ptr_array_set_size (thumb_targets, 0);
		}

		for (guint i = 0; i < thumb_targets->len; i++) {
			g_autoptr(AsImage) img = NULL;
			g_autofree gchar *thumb_img_url = NULL;
			AscImageTarget *target = g_ptr_array_index (thumb_targets, i);

			if (target->skipped)
				continue;
			if (target->error_msg != NULL) {
				g_autofree gchar *msg = g_strdup_printf (
				    "Can not store thumbnail image: %s",
				    target->error_msg);
				asc_result_add_hint (cres,
						     cpt,
						     "screenshot-save-error",
						     "url",
						     orig_img_url,
						     "error",
						     msg,
						     NULL);
				continue;
			}

			/* finally prepare the thumbnail definition and add it to the metadata */
			thumb_img_url = g_build_filename (scr_base_url, target->name, NULL);
			img = as_image_new ();
			as_image_set_locale (img, locale);
			as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
			as_image_set_width (img, target->result_width);
			as_image_set_height (img, target->result_height);
			as_image_set_url (img, thumb_img_url);
			as_screenshot_add_image (scr, img);

			thumbnails_generated = TRUE;
		}
	}

	if (!thumbnails_generated)
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-no-thumbnails",
				     "url",
				     orig_img_url,
				     NULL);

	return TRUE;
}

/**
 * asc_process_screenshot_images:
 *
 * Returns: (transfer full): The processed screenshot.
 */
static AsScreenshot *
asc_process_screenshot_images (AscResult *cres,
			       AsComponent *cpt,
			       AsScreenshot *scr,
			       AsCurl *acurl,
			       AscMedia *media,
			       const gchar *scr_export_dir,
			       const gchar *scr_base_url,
			       const gssize max_size_bytes,
			       gboolean store_screenshots,
			       guint scr_no)
{
	GPtrArray *imgs = NULL;
	GHashTableIter ht_iter;
	gpointer ht_key, ht_value;
	g_autoptr(GHashTable) ht_lang_img = g_hash_table_new_full (g_str_hash,
								   g_str_equal,
								   g_free,
								   g_object_unref);

	imgs = as_screenshot_get_images_all (scr);
	if (imgs->len == 0) {
		asc_result_add_hint_simple (cres, cpt, "metainfo-screenshot-but-no-media");
		return NULL;
	}

	/* try to find the source images, in case upstream has provided their own thumbnails.
	 * We use a hash-table to remove any possible duplicate original images of the same locale */
	for (guint i = 0; i < imgs->len; i++) {
		AsImage *img = AS_IMAGE (g_ptr_array_index (imgs, i));
		if (as_image_get_kind (img) == AS_IMAGE_KIND_SOURCE)
			g_hash_table_insert (ht_lang_img,
					     g_strdup (as_image_get_locale (img)),
					     g_object_ref (img));
	}

	/* just take the first image if we couldn't find a source */
	if (g_hash_table_size (ht_lang_img) == 0)
		g_hash_table_insert (ht_lang_img,
				     g_strdup ("C"),
				     g_object_ref (AS_IMAGE (g_ptr_array_index (imgs, 0))));

	/* drop metainfo images */
	as_screenshot_clear_images (scr);

	/* process images per locale */
	g_hash_table_iter_init (&ht_iter, ht_lang_img);
	while (g_hash_table_iter_next (&ht_iter, &ht_key, &ht_value))
		if (!asc_process_screenshot_images_lang (cres,
							 cpt,
							 scr,
							 AS_IMAGE (ht_value),
							 (const gchar *) ht_key,
							 acurl,
							 media,
							 scr_export_dir,
							 scr_base_url,
							 max_size_bytes,
							 store_screenshots,
							 scr_no))
			return NULL;

	return g_object_ref (scr);
}

/**
 * asc_process_screenshots:
 *
 * Download and resize screenshots and store them in our media export cache.
 */
void
asc_process_screenshots (AscResult *cres,
			 AsComponent *cpt,
			 AsCurl *acurl,
			 AscMedia *media,
			 const gchar *media_export_root,
			 const gchar *media_url_prefix,
			 const gssize max_size_bytes,
			 gboolean process_videos,
			 gboolean store_screenshots)
{
	GPtrArray *screenshots = NULL;
	g_autoptr(GPtrArray) valid_scrs = NULL;
	const gchar *gcid = NULL;
	g_autofree gchar *scr_export_dir = NULL;
	g_autofree gchar *scr_base_url = NULL;

	/* sanity check */
	if (media_export_root == NULL)
		store_screenshots = FALSE;

	screenshots = as_component_get_screenshots_all (cpt);
	if (screenshots->len == 0)
		return;

	gcid = asc_result_gcid_for_component (cres, cpt);
	if (gcid == NULL) {
		asc_result_add_hint (
		    cres,
		    cpt,
		    "internal-error",
		    "msg",
		    "No global ID could be found for component when processing screenshots.",
		    NULL);
		return;
	}

	/* if we shouldn't export screenshots, we store downloads in a temporary directory */
	if (store_screenshots)
		scr_export_dir = g_build_filename (media_export_root, gcid, "screenshots", NULL);
	else
		scr_export_dir = g_build_filename (asc_globals_get_tmp_dir (), gcid, NULL);

	/* the media URL prefix is used to embed absolute URLs for screenshots, if needed */
	if (media_url_prefix == NULL)
		scr_base_url = g_build_path (G_DIR_SEPARATOR_S, gcid, "screenshots", NULL);
	else
		scr_base_url = g_build_path (G_DIR_SEPARATOR_S,
					     media_url_prefix,
					     gcid,
					     "screenshots",
					     NULL);

	/* process screenshots */
	valid_scrs = g_ptr_array_new_with_free_func (g_object_unref);
	for (guint i = 0; i < screenshots->len; i++) {
		AsScreenshot *scr = AS_SCREENSHOT (g_ptr_array_index (screenshots, i));
		AsScreenshot *res_scr = NULL;

		if (as_screenshot_get_media_kind (scr) == AS_SCREENSHOT_MEDIA_KIND_VIDEO) {
			if (process_videos)
				res_scr = asc_process_screenshot_videos (cres,
									 cpt,
									 scr,
									 acurl,
									 media,
									 scr_export_dir,
									 scr_base_url,
									 max_size_bytes,
									 store_screenshots,
									 i + 1);
		} else {
			res_scr = asc_process_screenshot_images (cres,
								 cpt,
								 scr,
								 acurl,
								 media,
								 scr_export_dir,
								 scr_base_url,
								 max_size_bytes,
								 store_screenshots,
								 i + 1);
		}

		if (res_scr != NULL)
			g_ptr_array_add (valid_scrs, res_scr);
	}

	/* drop all preexisting screenshots from the MetaInfo data */
	g_ptr_array_remove_range (screenshots, 0, screenshots->len);

	/* add valid screenshots back */
	for (guint i = 0; i < valid_scrs->len; i++)
		as_component_add_screenshot (cpt,
					     AS_SCREENSHOT (g_ptr_array_index (valid_scrs, i)));
}
