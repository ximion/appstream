/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-utils-fonts
 * @short_description: Helper functions for generating metadata from fonts
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-utils-fonts.h"

#include "as-utils-private.h"
#include "asc-globals.h"
#include "asc-font.h"
#include "asc-canvas.h"
#include "asc-canvas-private.h"

struct {
	gint	width;
	gint	height;
} font_screenshot_sizes[] = {
	{ 1024, 78 },
	{ 640, 48 },
	{ 0, 0 }
};

/**
 * asc_render_font_screenshots:
 *
 * Render a "screenshot" sample for this font.
 */
static gboolean
asc_render_font_screenshots (AscResult *cres,
			     GPtrArray *fonts,
			     const gchar *cpt_screenshots_path,
			     AsComponent *cpt)
{
	gboolean first = TRUE;
	g_mkdir_with_parents (cpt_screenshots_path, 0755);

	for (guint i = 0; i < fonts->len; i++) {
		const gchar *font_id = NULL;
		const gchar *custom_sample_text = NULL;
		g_autofree gchar *scr_caption = NULL;
		g_autofree gchar *scr_url_root = NULL;
		g_autoptr(AsScreenshot) scr = NULL;
		AscFont *font = ASC_FONT (g_ptr_array_index (fonts, i));

		font_id = asc_font_get_id (font);
		if (as_is_empty (font_id)) {
			g_warning ("%s: Ignored font for screenshot rendering due to missing ID.",
				   as_component_get_id (cpt));
			continue;
		}

		scr = as_screenshot_new ();
		if (first) {
			as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_DEFAULT);
			first = FALSE;
		} else {
			as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_EXTRA);
		}
		scr_caption = g_strconcat (asc_font_get_family (font),
					   " ",
					   asc_font_get_style (font),
					   NULL);
		as_screenshot_set_caption (scr, scr_caption, "C");

		/* check if we have a custom sample text value (useful for symbolic fonts)
		 * we set this value for every fonr in the font-bundle, there is no way for this
		 * hack to select which font face should have the sample text.
		 * Since this hack only affects very few exotic fonts and should generally not
		 * be used, this should not be an issue. */
		custom_sample_text = as_component_get_custom_value (cpt, "FontSampleText");
		if (!as_is_empty (custom_sample_text))
			asc_font_set_sample_text (font, custom_sample_text);

		scr_url_root = g_build_filename (asc_result_gcid_for_component (cres, cpt), "screenshots", NULL);

		for (guint j = 0; font_screenshot_sizes[j].width > 0; j++) {
			g_autofree gchar *img_name = NULL;
			g_autofree gchar *img_filename = NULL;
			g_autofree gchar *img_url = NULL;
			g_autoptr(AsImage) img = NULL;

			img_name = g_strdup_printf ("image-%s_%ix%i.png",
						    font_id,
						    font_screenshot_sizes[j].width,
						    font_screenshot_sizes[j].height);
			img_filename = g_build_filename (cpt_screenshots_path, img_name, NULL);
			img_url = g_build_filename (scr_url_root, img_name, NULL);

			if (!g_file_test (img_filename, G_FILE_TEST_EXISTS)) {
				g_autoptr(AscCanvas) cv = NULL;
				g_autoptr(GError) tmp_error = NULL;
				gboolean ret;

				/* we didn't create a screenshot image yet - let's render it! */
				cv = asc_canvas_new (font_screenshot_sizes[j].width,
						     font_screenshot_sizes[j].height);
				ret = asc_canvas_draw_text_line (cv,
								 font,
								 asc_font_get_sample_text (font),
								 -1, /* border width */
								 &tmp_error);
				if (!ret) {
					asc_result_add_hint (cres, cpt,
								"font-render-error",
								"name", asc_font_get_fullname (font),
								"error", tmp_error->message,
								NULL);
					continue;
				}

				g_debug ("Saving font screenshot image: %s", img_name);
				ret = asc_canvas_save_png (cv, img_filename, &tmp_error);
				if (!ret) {
					asc_result_add_hint (cres, cpt,
								"font-render-error",
								"name", asc_font_get_fullname (font),
								"error", tmp_error->message,
								NULL);
					continue;
				}
			}

			img = as_image_new ();
			as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
			as_image_set_width (img, font_screenshot_sizes[j].width);
			as_image_set_height (img, font_screenshot_sizes[j].height);
			as_image_set_url (img, img_url);

			as_screenshot_add_image (scr, img);
		}
	}

	return TRUE;
}

/**
 * asc_render_font_icon:
 *
 * Render an icon for this font package using one of its fonts.
 * (Since we have no better way to do this, we just pick the first font
 * at time)
 **/
static gboolean
asc_render_font_icon (AscResult *cres,
		      AscUnit *unit,
		      AscFont *font,
		      const gchar *cpt_icons_path,
		      const gchar *icons_export_dir,
		      AsComponent *cpt,
		      AscIconPolicy *icon_policy)
{
	AscIconPolicyIter iter;
	guint size;
	guint scale_factor;
	AscIconState icon_state;

	asc_icon_policy_iter_init (&iter, icon_policy);
	while (asc_icon_policy_iter_next (&iter, &size, &scale_factor, &icon_state)) {
		g_autofree gchar *size_str = NULL;
		g_autofree gchar *icon_dir = NULL;
		g_autofree gchar *icon_name = NULL;
		g_autofree gchar *icon_full_path = NULL;
		const gchar *custom_icon_text = NULL;

		/* skip icon if it should be skipped */
		if (icon_state == ASC_ICON_STATE_IGNORED)
			continue;

		size_str = (scale_factor == 1)
				? g_strdup_printf ("%ix%i",
						   size, size)
				: g_strdup_printf ("%ix%i@%i",
						   size, size,
						   scale_factor);
		icon_dir = g_build_filename (cpt_icons_path, size_str, NULL);
		g_mkdir_with_parents (icon_dir, 0755);

		/* check if we have a custom icon text value (useful for symbolic fonts) */
		custom_icon_text = as_component_get_custom_value (cpt, "FontIconText");
		if (!as_is_empty (custom_icon_text))
			asc_font_set_sample_icon_text (font, custom_icon_text); /* Font will ensure that the value does not exceed 3 chars */

		icon_name = g_strdup_printf ("%s_%s.png",
						asc_unit_get_bundle_id_safe (unit),
						asc_font_get_id (font));
		icon_full_path = g_build_filename (icon_dir, icon_name, NULL);

		if (!g_file_test (icon_full_path, G_FILE_TEST_EXISTS)) {
			g_autoptr(AscCanvas) cv = NULL;
			g_autoptr(GError) tmp_error = NULL;
			gboolean ret;

			/* we didn't create an icon yet - let's render it! */
			cv = asc_canvas_new (size * scale_factor,
						size * scale_factor);
			ret = asc_canvas_draw_text_line (cv,
							 font,
							 asc_font_get_sample_icon_text (font),
							 -1, /* border width */
							 &tmp_error);
			if (!ret) {
				asc_result_add_hint (cres, cpt,
						     "font-render-error",
						     "name", asc_font_get_fullname (font),
						     "error", tmp_error->message,
						     NULL);
				continue;
			}
			g_debug ("Saving font icon: %s/%s", size_str, icon_name);
			ret = asc_canvas_save_png (cv, icon_full_path, &tmp_error);
			if (!ret) {
				asc_result_add_hint (cres, cpt,
						     "font-render-error",
						     "name", asc_font_get_fullname (font),
						     "error", tmp_error->message,
						     NULL);
				continue;
			}

			if (icons_export_dir != NULL) {
				g_autofree gchar *icon_export_dir = NULL;
				g_autofree gchar *icon_export_fname = NULL;

				g_debug ("Copying icon to icon export dir: %s/%s", size_str, icon_name);
				icon_export_dir = g_build_filename (icons_export_dir, size_str, NULL);
				icon_export_fname = g_build_filename (icon_export_dir, icon_name, NULL);

				g_mkdir_with_parents (icon_export_dir, 0755);
				if (!as_copy_file (icon_full_path, icon_export_fname, &tmp_error)) {
					g_autofree gchar *tmp_icon_fname = g_strdup_printf ("%s/%s", size_str, icon_name);
					g_warning ("Unable to write exported icon: %s", icon_export_fname);
					asc_result_add_hint (cres, cpt,
							     "icon-write-error",
							     "fname", tmp_icon_fname,
							     "msg", tmp_error->message,
							     NULL);
					continue;
				}
			}
		}

		if (icon_state != ASC_ICON_STATE_REMOTE_ONLY) {
			g_autoptr(AsIcon) icon = as_icon_new ();
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
			as_icon_set_width (icon, size);
			as_icon_set_height (icon, size);
			as_icon_set_scale (icon, scale_factor);
			as_icon_set_name (icon, icon_name);
			as_component_add_icon (cpt, icon);
		}

		if (icon_state != ASC_ICON_STATE_CACHED_ONLY) {
			const gchar *gcid = NULL;
			g_autofree gchar *remote_icon_url = NULL;
			g_autoptr(AsIcon) icon = as_icon_new ();

			gcid = asc_result_gcid_for_component (cres, cpt);
			if (as_is_empty (gcid)) {
				asc_result_add_hint (cres,
						     cpt,
						     "internal-error",
						     "msg", "No global ID could be found for component when processing fonts.",
						     NULL);
				return FALSE;
			}

			remote_icon_url = g_build_filename (gcid, "icons", size_str, icon_name, NULL);
			as_icon_set_kind (icon, AS_ICON_KIND_REMOTE);
			as_icon_set_width (icon, size);
			as_icon_set_height (icon, size);
			as_icon_set_scale (icon, scale_factor);
			as_icon_set_url (icon, remote_icon_url);
			as_component_add_icon (cpt, icon);
		}
	}

	return TRUE;
}

static gint
asc_font_cmp (gconstpointer a, gconstpointer b)
{
	AscFont *f1 = ASC_FONT (*((AscFont **) a));
	AscFont *f2 = ASC_FONT (*((AscFont **) b));
	return g_strcmp0 (asc_font_get_id (f1),
			  asc_font_get_id (f2));
}

/**
 * process_font_data_for_component:
 */
static void
process_font_data_for_component (AscResult *cres,
				 AscUnit *unit,
				 AsComponent *cpt,
				 GHashTable *all_fonts,
				 const gchar *media_export_root,
				 const gchar *icons_export_dir,
				 AscIconPolicy *icon_policy,
				 AscComposeFlags flags)
{
	const gchar *gcid = NULL;
	AsProvided *provided = NULL;
	g_autoptr(GPtrArray) font_hints = NULL;
	g_autoptr(GPtrArray) selected_fonts = NULL;
	g_autofree gchar *cpt_icons_path = NULL;
	g_autofree gchar *cpt_screenshots_path = NULL;
	g_autoptr(GList) cpt_languages = NULL;
	gboolean has_icon = FALSE;

	gcid = asc_result_gcid_for_component (cres, cpt);
	if (as_is_empty (gcid)) {
		asc_result_add_hint (cres,
				     cpt,
				     "internal-error",
				     "msg", "No global ID could be found for component when processing fonts.",
				     NULL);
		return;
	}

	font_hints = g_ptr_array_new_with_free_func (g_free);
	provided = as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_FONT);
	if (provided != NULL) {
		GPtrArray *items = as_provided_get_items (provided);
		for (guint i = 0; i < items->len; i++) {
			const gchar *font_full_name = g_ptr_array_index (items, i);
			g_ptr_array_add (font_hints,
					 g_utf8_strdown (font_full_name, -1));
		}
	}

	/* data export paths */
	if (media_export_root == NULL)
		cpt_icons_path = g_build_filename (asc_globals_get_tmp_dir (), gcid, NULL);
	else
		cpt_icons_path = g_build_filename (media_export_root, gcid, "icons", NULL);

	if (media_export_root != NULL)
		cpt_screenshots_path = g_build_filename (media_export_root, gcid, "screenshots", NULL);

	/* if we have no fonts hints, we simply process all the fonts
	 * that we found in this unit. */
	if (font_hints->len == 0) {
		gboolean regular_found = FALSE;
		g_autoptr(GPtrArray) tmp_array = NULL;
		GHashTableIter ht_iter;
		gpointer ht_value;

		selected_fonts = g_ptr_array_new_full (g_hash_table_size (all_fonts),
						       g_object_unref);

		tmp_array = g_ptr_array_new_full (g_hash_table_size (all_fonts),
						  g_object_unref);
		g_hash_table_iter_init (&ht_iter, all_fonts);
		while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value))
			g_ptr_array_add (tmp_array,
					 g_object_ref (ASC_FONT (ht_value)));

		/* prepend fonts that contain "regular" so we prefer the regular
		 * font face for rendering samples over the other styles
		 * also ensure that the font style list is sorted for more
		 * deterministic results. */
		g_ptr_array_sort (tmp_array, asc_font_cmp);
		for (guint i = 0; i < tmp_array->len; i++) {
			g_autofree gchar *font_style_id = NULL;
			AscFont *font = ASC_FONT (g_ptr_array_index (tmp_array, i));

			font_style_id = g_utf8_strdown (asc_font_get_style (font), -1);
			if (!regular_found && g_strstr_len (font_style_id, -1, "regular") != NULL) {
				g_ptr_array_insert (selected_fonts, 0, g_object_ref (font));
				/* if we found a font which has a style that equals "regular",
				 * we can stop searching for the preferred font */
				if (g_strcmp0 (font_style_id, "regular") == 0)
					regular_found = TRUE;
			} else {
				g_ptr_array_add (selected_fonts, g_object_ref (font));
			}
		}
	} else {
		selected_fonts = g_ptr_array_new_full (font_hints->len, g_object_unref);

		/* Find fonts based on the hints we have.
		 * The hint as well as the dictionary keys are all lowercased, so we
		 * can do case-insensitive matching here. */
		for (guint i = 0; i < font_hints->len; i++) {
			AscFont *font;
			const gchar *font_hint = g_ptr_array_index (font_hints, i);

			font = g_hash_table_lookup (all_fonts, font_hint);
			if (font == NULL)
				continue;
			g_ptr_array_add (selected_fonts, g_object_ref (font));
		}
	}

	/* we have nothing to do if we did not select any font
	 * (this is a bug, since we filtered for font metainfo previously) */
	if (selected_fonts->len == 0) {
		GHashTableIter ht_iter;
		gpointer ht_value;
		g_autoptr(GString) font_names_str = NULL;

		font_names_str = g_string_new ("");
		g_hash_table_iter_init (&ht_iter, all_fonts);
		while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value))
			g_string_append_printf (font_names_str,
						"%s | ",
						asc_font_get_fullname (ASC_FONT (ht_value)));
		if (font_names_str->len > 4)
			g_string_truncate (font_names_str, font_names_str->len - 3);

		asc_result_add_hint (cres,
				     cpt,
				     "font-metainfo-but-no-font",
				     "font_names", font_names_str->str,
				     NULL);
		return;
	}

	/* language information of fonts is often completely wrong. In case there was a metainfo file
	 * with languages explicitly set, we take the first language and prefer that over the others. */
	cpt_languages = as_component_get_languages (cpt);
	if (cpt_languages != NULL) {
		const gchar *first_lang = cpt_languages->data;

		for (guint i = 0; i < selected_fonts->len; i++) {
			AscFont *font = ASC_FONT (g_ptr_array_index (selected_fonts, i));
			asc_font_set_preferred_language (font, first_lang);
		}

		/* add languages mentioned in the metainfo file to list of supported languages
		 * of the respective font */
		for (GList *item = cpt_languages->next; item != NULL; item = item->next) {
			for (guint i = 0; i < selected_fonts->len; i++) {
				AscFont *font = ASC_FONT (g_ptr_array_index (selected_fonts, i));
				asc_font_add_language (font, item->data);
			}
		}
	}

	g_debug ("Rendering font data for %s", gcid);

	/* process font files */
	has_icon = FALSE;
	for (guint i = 0; i < selected_fonts->len; i++) {
		g_autoptr(GList) language_list = NULL;
		AscFont *font = ASC_FONT (g_ptr_array_index (selected_fonts, i));

		g_debug ("Processing font '%s'", asc_font_get_id (font));

		/* add language information */
		language_list = asc_font_get_language_list (font);
		for (GList *l = language_list; l != NULL; l = l->next) {
			/* we have no idea how well the font supports the language's script,
			 * but since it advertises support in its metadata, we just assume 100% here */
			as_component_add_language (cpt,
						   (const gchar*) l->data,
						   100);
		}

		/* render an icon for our font */
		if (!has_icon)
			has_icon = asc_render_font_icon (cres,
							 unit,
							 font,
							 cpt_icons_path,
							 icons_export_dir,
							 cpt,
							 icon_policy);

		/* set additional metadata. The font metadata might be terrible, but if the data is bad
		 * it hopefully motivates people to write proper metainfo files. */
		if (as_is_empty (as_component_get_description (cpt)) &&
		    !as_is_empty (asc_font_get_description (font)))
			as_component_set_description (cpt, asc_font_get_description (font), "C");

		if (as_is_empty (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE)) &&
		    !as_is_empty (asc_font_get_homepage (font)))
			as_component_add_url (cpt, AS_URL_KIND_HOMEPAGE, asc_font_get_homepage (font));
	}

	/* render all sample screenshots for all font styles we have */
	if (as_flags_contains (flags, ASC_COMPOSE_FLAG_STORE_SCREENSHOTS)) {
		if (cpt_screenshots_path == NULL)
			g_warning ("Screenshot storage is enabled, but no screenshot media path could be constructed for %s.",
				   as_component_get_id (cpt));
		else
			asc_render_font_screenshots (cres,
						     selected_fonts,
						     cpt_screenshots_path,
						     cpt);
	}
}

/**
 * asc_process_fonts:
 *
 * Process any font data.
 */
void
asc_process_fonts (AscResult *cres,
		   AscUnit *unit,
		   const gchar *media_export_root,
		   const gchar *icons_export_dir,
		   AscIconPolicy *icon_policy,
		   AscComposeFlags flags)
{
	GPtrArray *contents = NULL;
	g_autoptr(GPtrArray) all_cpts = NULL;
	g_autoptr(GPtrArray) font_cpts = NULL;
	g_autoptr(GHashTable) all_fonts = NULL;

	/* collect all font components that interest us */
	font_cpts = g_ptr_array_new_with_free_func (g_object_unref);
	all_cpts = asc_result_fetch_components (cres);
	for (guint i = 0; i < all_cpts->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (all_cpts, i));
		if (as_component_get_kind (cpt) != AS_COMPONENT_KIND_FONT)
			continue;
		g_ptr_array_add (font_cpts, g_object_ref (cpt));
	}

	/* quit if we have no font component to process */
	if (font_cpts->len == 0)
		return;

	all_fonts = g_hash_table_new_full (g_str_hash, g_str_equal,
					   g_free, g_object_unref);

	/* create a map of all fonts that this unit contains */
	contents = asc_unit_get_contents (unit);
	for (guint i = 0; i < contents->len; i++) {
		g_autoptr(GBytes) font_bytes = NULL;
		const void *data;
		gsize data_len;
		g_autoptr(GError) tmp_error = NULL;
		g_autoptr(AscFont) font = NULL;
		g_autofree gchar *basename = NULL;
		g_autofree gchar *font_fullname_lower = NULL;
		const gchar *fname = g_ptr_array_index (contents, i);
		if (!g_str_has_prefix (fname, "/usr/share/fonts/"))
			continue;
		if (!g_str_has_suffix (fname, ".ttf") && !g_str_has_suffix (fname, ".otf"))
			continue;

		basename = g_path_get_basename (fname);
		font_bytes = asc_unit_read_data (unit, fname, &tmp_error);
		if (font_bytes == NULL) {
			asc_result_add_hint (cres, NULL,
						"file-read-error",
						"fname", fname,
						"msg", tmp_error->message,
						NULL);
			continue;
		}
		data = g_bytes_get_data (font_bytes, &data_len);

		font = asc_font_new_from_data (data, data_len, basename, &tmp_error);
		if (font == NULL) {
			asc_result_add_hint (cres, NULL,
						"font-load-error",
						"fname", fname,
						"unit_name", asc_unit_get_bundle_id (unit),
						"error", tmp_error->message,
						NULL);
			continue;
		}

		g_debug ("Found font %s/%s", basename, asc_font_get_fullname (font));
		font_fullname_lower = g_utf8_strdown (asc_font_get_fullname (font), -1);
		g_hash_table_insert (all_fonts,
				     g_steal_pointer (&font_fullname_lower),
				     g_steal_pointer (&font));
	}

	/* process fonts for all components */
	for (guint i = 0; i < font_cpts->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (font_cpts, i));
		process_font_data_for_component (cres,
						 unit,
						 cpt,
						 all_fonts,
						 media_export_root,
						 icons_export_dir,
						 icon_policy,
						 flags);
	}
}
