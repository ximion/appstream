/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-utils-private.h"

#include "asc-image.h"

struct {
	gint	width;
	gint	height;
} target_screenshot_sizes[] = {
	{ 1248, 702 },
	{ 752, 423 },
	{ 624, 351 },
	{ 224, 126 },
	{ 0, 0 }
};

/**
 * asc_process_screenshot_images:
 */
static AsScreenshot*
asc_process_screenshot_images (AscResult *cres,
			       AsComponent *cpt,
			       AsScreenshot *scr,
			       AsCurl *acurl,
			       const gchar *scr_export_dir,
			       const gchar *scr_base_url,
			       gboolean store_screenshots,
			       guint scr_no)
{
	GPtrArray *imgs = NULL;
	g_autoptr(AsImage) orig_img = NULL;
	const gchar *orig_img_url = NULL;
	const gchar *orig_img_locale = NULL;
	const gchar *gcid = NULL;
	g_autoptr(GBytes) img_bytes = NULL;
	gconstpointer img_data;
	gsize img_data_len;
	guint source_scr_width;
	guint source_scr_height;
	gboolean thumbnails_generated = FALSE;
	g_autoptr(GError) error = NULL;

	imgs = as_screenshot_get_images (scr);
	if (imgs->len == 0) {
		asc_result_add_hint_simple (cres, cpt, "metainfo-screenshot-but-no-media");
		return NULL;
	}

	/* try to find the source image, in case upstream has provided their own thumbnails */
	for (guint i = 0; i < imgs->len; i++) {
		AsImage *img = AS_IMAGE (g_ptr_array_index (imgs, i));
		if (as_image_get_kind (img) == AS_IMAGE_KIND_SOURCE) {
			orig_img = g_object_ref (img);
			break;
		}
	}

	/* just take the first image if we couldn't find a source */
	if (orig_img == NULL)
		orig_img = g_object_ref (AS_IMAGE (g_ptr_array_index (imgs, 0)));

	/* drop metainfo images */
	g_ptr_array_remove_range (imgs, 0, imgs->len);

	orig_img_url = as_image_get_url (orig_img);
	orig_img_locale = as_image_get_locale (orig_img);

	if (orig_img_url == NULL)
		return NULL;

	/* download our image */
	img_bytes = as_curl_download_bytes (acurl, orig_img_url, &error);
	if (img_bytes == NULL) {
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-download-error",
				     "url", orig_img_url,
				     "error", error->message,
				     NULL);
		return NULL;
	}
	img_data = g_bytes_get_data (img_bytes, &img_data_len);

	gcid = asc_result_gcid_for_component (cres, cpt);
	if (gcid == NULL) {
		asc_result_add_hint (cres,
				     cpt,
				     "internal-error",
				     "msg", "No global ID could be found for component when processing screenshot images.",
				     NULL);
		return NULL;
	}

	/* ensure export dir exists */
	if (g_mkdir_with_parents (scr_export_dir, 0755) != 0)
		g_warning ("Failed to create directory tree %s", scr_export_dir);


	{
		g_autoptr(AscImage) src_image = NULL;
		g_autofree gchar *src_img_name = NULL;
		g_autofree gchar *src_img_path = NULL;
		g_autofree gchar *src_img_url = NULL;
		g_autoptr(AsImage) simg = NULL;

		src_img_name = g_strdup_printf ("image-%i_orig.png", scr_no);
		src_img_path = g_build_filename (scr_export_dir, src_img_name, NULL);
		src_img_url = g_build_filename (scr_base_url, src_img_name, NULL);

		/* save the source screenshot as PNG image */
		src_image = asc_image_new_from_data (img_data,
						     img_data_len,
						     0, /* destination size */
						     FALSE, /* compressed */
						     ASC_IMAGE_LOAD_FLAG_NONE,
						     &error);
		if (error != NULL) {
			g_autofree gchar *msg = g_strdup_printf ("Could not load source screenshot for storing: %s", error->message);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-save-error",
					     "url", orig_img_url,
					     "error", msg,
					     NULL);
			return NULL;
		}

		if (!asc_image_save_filename (src_image,
						src_img_path,
						0, 0,
						ASC_IMAGE_SAVE_FLAG_OPTIMIZE,
						&error)) {
			g_autofree gchar *msg = g_strdup_printf ("Can not store source screenshot: %s", error->message);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-save-error",
					     "url", orig_img_url,
					     "error", msg,
					     NULL);
			return NULL;
		}

		simg = as_image_new ();
		as_image_set_kind (simg, AS_IMAGE_KIND_SOURCE);
		as_image_set_locale (simg, orig_img_locale);

		source_scr_width = asc_image_get_width (src_image);
		source_scr_height = asc_image_get_height (src_image);
		as_image_set_width (simg, source_scr_width);
		as_image_set_height (simg, source_scr_height);

		/* if we should not create a screenshots store, delete the just-downloaded file and set
		 * the original upstream URL as source.
		 * we still needed to download the screenshot to get information about its size. */
		if (!store_screenshots) {
			as_image_set_url (simg, orig_img_url);
			as_screenshot_add_image (scr, simg);

			/* drop screenshot storage directory, in this mode it is only ever used temporarily */
			as_utils_delete_dir_recursive (scr_export_dir);
			return scr;
		}

		as_image_set_url (simg, src_img_url);
		as_screenshot_add_image (scr, simg);
	}

	/* generate & save thumbnails for the screenshot image */
	thumbnails_generated = FALSE;
	for (guint i = 0; target_screenshot_sizes[i].width != 0; i++) {
		g_autoptr(AscImage) thumb = NULL;
		g_autoptr(AsImage) img = NULL;
		g_autofree gchar *thumb_img_name = NULL;
		g_autofree gchar *thumb_img_path = NULL;
		g_autofree gchar *thumb_img_url = NULL;

		guint target_width = target_screenshot_sizes[i].width;
		guint target_height = target_screenshot_sizes[i].height;

		/* ensure we will only downscale the screenshot for thumbnailing */
		if (target_width > source_scr_width)
			continue;
		if (target_height > source_scr_height)
			continue;

		thumb = asc_image_new_from_data (img_data,
						 img_data_len,
						 0, /* destination size */
						 FALSE, /* compressed */
						 ASC_IMAGE_LOAD_FLAG_NONE,
						 &error);
		if (error != NULL) {
			g_autofree gchar *msg = g_strdup_printf ("Could not load source screenshot for thumbnailing: %s", error->message);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-save-error",
					     "url", orig_img_url,
					     "error", msg,
					     NULL);
			g_error_free (g_steal_pointer (&error));
			continue;
		}

		if (target_width > target_height)
			asc_image_scale_to_width (thumb, target_width);
		else
			asc_image_scale_to_height (thumb, target_height);

		/* create thumbnail storage path and URL component*/
		thumb_img_name = g_strdup_printf ("image-%i_%ix%i.png",
						  scr_no,
						  asc_image_get_width (thumb),
						  asc_image_get_height (thumb));
		thumb_img_path = g_build_filename (scr_export_dir, thumb_img_name, NULL);
		thumb_img_url = g_build_filename (scr_base_url, thumb_img_name, NULL);

		/* store the thumbnail image on disk */
		if (!asc_image_save_filename (thumb,
						thumb_img_path,
						0, 0,
						ASC_IMAGE_SAVE_FLAG_OPTIMIZE,
						&error)) {
			g_autofree gchar *msg = g_strdup_printf ("Can not store thumbnail image: %s", error->message);
			asc_result_add_hint (cres,
					     cpt,
					     "screenshot-save-error",
					     "url", orig_img_url,
					     "error", msg,
					     NULL);
			g_error_free (g_steal_pointer (&error));
			continue;
		}

		// finally prepare the thumbnail definition and add it to the metadata
		img = as_image_new ();
		as_image_set_locale (img, orig_img_locale);
		as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
		as_image_set_width (img, asc_image_get_width (thumb));
		as_image_set_height (img, asc_image_get_height (thumb));
		as_image_set_url (img, thumb_img_url);
		as_screenshot_add_image (scr, img);

		thumbnails_generated = TRUE;
	}

	if (!thumbnails_generated)
		asc_result_add_hint (cres,
				     cpt,
				     "screenshot-no-thumbnails",
				     "url", orig_img_url,
				      NULL);

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
			 const gchar *media_export_root,
			 gboolean store_screenshots)
{
	GPtrArray *screenshots = NULL;
	g_autoptr(GPtrArray) valid_scrs = NULL;
	const gchar *gcid = NULL;
	g_autofree gchar *scr_export_dir = NULL;
	g_autofree gchar *scr_base_url = NULL;

	screenshots = as_component_get_screenshots (cpt);
	if (screenshots->len == 0)
		return;

	gcid = asc_result_gcid_for_component (cres, cpt);
	if (gcid == NULL) {
		asc_result_add_hint (cres,
				     cpt,
				     "internal-error",
				     "msg", "No global ID could be found for component when processing screenshots.",
				     NULL);
		return;
	}

	scr_export_dir = g_build_filename (media_export_root, gcid, "screenshots", NULL);
	scr_base_url = g_build_path (G_DIR_SEPARATOR_S, gcid, "screenshots", NULL);

	valid_scrs = g_ptr_array_new_with_free_func (g_object_unref);
	for (guint i = 0; i < screenshots->len; i++) {
		AsScreenshot *scr = AS_SCREENSHOT (g_ptr_array_index (screenshots, i));
		AsScreenshot *res_scr = NULL;

		if (as_screenshot_get_media_kind (scr) == AS_SCREENSHOT_MEDIA_KIND_VIDEO) {
			/* TODO: Code for this needs to be ported from appstream-generator */
		} else {
			res_scr = asc_process_screenshot_images (cres,
								 cpt,
								 scr,
								 acurl,
								 scr_export_dir,
								 scr_base_url,
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
