/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include "asc-media-private.h"

struct {
	gint width;
	gint height;
} font_screenshot_sizes[] = {
	{ 1560, 878 },
	{ 752,  423 },
	{ 624,  351 },
	{ 0,    0   }
};

/**
 * AscFontEntry:
 *
 * A font found in the processed unit: its raw data together
 * with the metadata extracted by the media worker.
 */
typedef struct {
	AscFontInfo *info;
	GBytes *data;
	gchar *basename;
} AscFontEntry;

static AscFontEntry *
asc_font_entry_new (AscFontInfo *info, GBytes *data, const gchar *basename)
{
	AscFontEntry *entry;

	entry = g_new0 (AscFontEntry, 1);
	entry->info = info;
	entry->data = g_bytes_ref (data);
	entry->basename = g_strdup (basename);

	return entry;
}

static void
asc_font_entry_free (AscFontEntry *entry)
{
	if (entry == NULL)
		return;
	asc_font_info_free (entry->info);
	g_bytes_unref (entry->data);
	g_free (entry->basename);
	g_free (entry);
}

/**
 * asc_render_font_screenshots:
 *
 * Render "screenshot" samples for the selected fonts.
 */
static gboolean
asc_render_font_screenshots (AscResult *cres,
			     GPtrArray *fonts,
			     AscMedia *media,
			     const gchar *cpt_screenshots_path,
			     AsComponent *cpt,
			     const gchar *preferred_lang,
			     const gchar *const *extra_langs)
{
	gboolean first = TRUE;
	g_mkdir_with_parents (cpt_screenshots_path, 0755);

	for (guint i = 0; i < fonts->len; i++) {
		const gchar *font_id = NULL;
		const gchar *custom_sample_text = NULL;
		const gchar *custom_icon_text = NULL;
		g_autofree gchar *scr_caption = NULL;
		g_autofree gchar *scr_url_root = NULL;
		g_autoptr(AsScreenshot) scr = NULL;
		g_autoptr(GPtrArray) targets = NULL;
		g_autoptr(GError) tmp_error = NULL;
		AscFontEntry *entry = g_ptr_array_index (fonts, i);

		font_id = entry->info->id;
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
		scr_caption = g_strconcat (entry->info->family, " ", entry->info->style, NULL);
		as_screenshot_set_caption (scr, scr_caption, "C");

		/* check if we have custom sample text values (useful for symbolic fonts)
		 * we use these values for every font in the font-bundle, there is no way for this
		 * hack to select which font face should have the sample text.
		 * Since this hack only affects very few exotic fonts and should generally not
		 * be used, this should not be an issue. */
		custom_sample_text = as_component_get_custom_value (cpt, "FontSampleText");
		custom_icon_text = as_component_get_custom_value (cpt, "FontIconText");

		scr_url_root = g_build_filename (asc_result_gcid_for_component (cres, cpt),
						 "screenshots",
						 NULL);

		targets = g_ptr_array_new_with_free_func ((GDestroyNotify) asc_image_target_free);
		for (guint j = 0; font_screenshot_sizes[j].width > 0; j++) {
			g_autofree gchar *img_name = NULL;

			img_name = g_strdup_printf ("image-%s_%i.png",
						    font_id,
						    font_screenshot_sizes[j].width);
			g_ptr_array_add (targets,
					 asc_image_target_new (img_name,
							       ASC_IMAGE_SCALE_MODE_EXACT,
							       font_screenshot_sizes[j].width,
							       font_screenshot_sizes[j].height));
		}

		/* render the font specimen cards via the media worker */
		if (!asc_media_render_font_card (media,
						 entry->data,
						 entry->basename,
						 preferred_lang,
						 extra_langs,
						 custom_sample_text,
						 custom_icon_text,
						 NULL, /* default info label */
						 cpt_screenshots_path,
						 targets,
						 &tmp_error)) {
			asc_result_add_hint (cres,
					     cpt,
					     "font-render-error",
					     "name",
					     entry->info->fullname,
					     "error",
					     tmp_error->message,
					     NULL);
			continue;
		}

		for (guint j = 0; j < targets->len; j++) {
			g_autofree gchar *img_url = NULL;
			g_autoptr(AsImage) img = NULL;
			AscImageTarget *target = g_ptr_array_index (targets, j);

			if (target->error_msg != NULL) {
				asc_result_add_hint (cres,
						     cpt,
						     "font-render-error",
						     "name",
						     entry->info->fullname,
						     "error",
						     target->error_msg,
						     NULL);
				continue;
			}
			g_debug ("Saved font screenshot image: %s", target->name);

			img_url = g_build_filename (scr_url_root, target->name, NULL);
			img = as_image_new ();
			as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
			as_image_set_width (img, target->result_width);
			as_image_set_height (img, target->result_height);
			as_image_set_url (img, img_url);

			as_screenshot_add_image (scr, img);
		}

		if (as_screenshot_is_valid (scr))
			as_component_add_screenshot (cpt, scr);
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
		      AscFontEntry *entry,
		      AscMedia *media,
		      const gchar *cpt_icons_path,
		      const gchar *icons_export_dir,
		      AsComponent *cpt,
		      AscIconPolicy *icon_policy,
		      const gchar *preferred_lang,
		      const gchar *const *extra_langs)
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
			       ? g_strdup_printf ("%ix%i", size, size)
			       : g_strdup_printf ("%ix%i@%i", size, size, scale_factor);
		icon_dir = g_build_filename (cpt_icons_path, size_str, NULL);
		g_mkdir_with_parents (icon_dir, 0755);

		/* check if we have a custom icon text value (useful for symbolic fonts)
		 * the media worker will ensure that the value does not exceed 3 chars */
		custom_icon_text = as_component_get_custom_value (cpt, "FontIconText");

		icon_name = g_strdup_printf ("%s_%s.png",
					     asc_unit_get_bundle_id_safe (unit),
					     entry->info->id);
		icon_full_path = g_build_filename (icon_dir, icon_name, NULL);

		if (!g_file_test (icon_full_path, G_FILE_TEST_EXISTS)) {
			g_autoptr(GPtrArray) targets = NULL;
			g_autoptr(GError) tmp_error = NULL;
			AscImageTarget *target = NULL;

			/* we didn't create an icon yet - let the media worker render it! */
			targets = g_ptr_array_new_with_free_func (
			    (GDestroyNotify) asc_image_target_free);
			target = asc_image_target_new (icon_name,
						       ASC_IMAGE_SCALE_MODE_EXACT,
						       size * scale_factor,
						       size * scale_factor);
			g_ptr_array_add (targets, target);

			g_debug ("Rendering font icon: %s/%s", size_str, icon_name);
			if (!asc_media_render_font_icon (media,
							 entry->data,
							 entry->basename,
							 preferred_lang,
							 extra_langs,
							 NULL, /* custom sample text */
							 custom_icon_text,
							 icon_dir,
							 targets,
							 &tmp_error)) {
				asc_result_add_hint (cres,
						     cpt,
						     "font-render-error",
						     "name",
						     entry->info->fullname,
						     "error",
						     tmp_error->message,
						     NULL);
				continue;
			}
			if (target->error_msg != NULL) {
				asc_result_add_hint (cres,
						     cpt,
						     "font-render-error",
						     "name",
						     entry->info->fullname,
						     "error",
						     target->error_msg,
						     NULL);
				continue;
			}

			if (icons_export_dir != NULL) {
				g_autofree gchar *icon_export_dir = NULL;
				g_autofree gchar *icon_export_fname = NULL;

				g_debug ("Copying icon to icon export dir: %s/%s",
					 size_str,
					 icon_name);
				icon_export_dir = g_build_filename (icons_export_dir,
								    size_str,
								    NULL);
				icon_export_fname = g_build_filename (icon_export_dir,
								      icon_name,
								      NULL);

				g_mkdir_with_parents (icon_export_dir, 0755);
				if (!as_copy_file (icon_full_path, icon_export_fname, &tmp_error)) {
					g_autofree gchar *tmp_icon_fname = g_strdup_printf (
					    "%s/%s",
					    size_str,
					    icon_name);
					g_warning ("Unable to write exported icon: %s",
						   icon_export_fname);
					asc_result_add_hint (cres,
							     cpt,
							     "icon-write-error",
							     "fname",
							     tmp_icon_fname,
							     "msg",
							     tmp_error->message,
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
						     "msg",
						     "No global ID could be found for component "
						     "when processing fonts.",
						     NULL);
				return FALSE;
			}

			remote_icon_url = g_build_filename (gcid,
							    "icons",
							    size_str,
							    icon_name,
							    NULL);
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
asc_font_entry_cmp (gconstpointer a, gconstpointer b)
{
	AscFontEntry *e1 = *((AscFontEntry **) a);
	AscFontEntry *e2 = *((AscFontEntry **) b);
	return g_strcmp0 (e1->info->id, e2->info->id);
}

/**
 * process_font_data_for_component:
 */
static void
process_font_data_for_component (AscResult *cres,
				 AscUnit *unit,
				 AsComponent *cpt,
				 GHashTable *all_fonts,
				 AscMedia *media,
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
	g_autoptr(GPtrArray) extra_langs_array = NULL;
	g_autofree const gchar **extra_langs = NULL;
	const gchar *preferred_lang = NULL;
	gboolean has_icon = FALSE;

	gcid = asc_result_gcid_for_component (cres, cpt);
	if (as_is_empty (gcid)) {
		asc_result_add_hint (
		    cres,
		    cpt,
		    "internal-error",
		    "msg",
		    "No global ID could be found for component when processing fonts.",
		    NULL);
		return;
	}

	font_hints = g_ptr_array_new_with_free_func (g_free);
	provided = as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_FONT);
	if (provided != NULL) {
		GPtrArray *items = as_provided_get_items (provided);
		for (guint i = 0; i < items->len; i++) {
			const gchar *font_full_name = g_ptr_array_index (items, i);
			g_ptr_array_add (font_hints, g_utf8_strdown (font_full_name, -1));
		}
	}

	/* data export paths */
	if (media_export_root == NULL)
		cpt_icons_path = g_build_filename (asc_globals_get_tmp_dir (), gcid, NULL);
	else
		cpt_icons_path = g_build_filename (media_export_root, gcid, "icons", NULL);

	if (media_export_root != NULL)
		cpt_screenshots_path = g_build_filename (media_export_root,
							 gcid,
							 "screenshots",
							 NULL);

	/* if we have no fonts hints, we simply process all the fonts
	 * that we found in this unit. */
	if (font_hints->len == 0) {
		gboolean regular_found = FALSE;
		g_autoptr(GPtrArray) tmp_array = NULL;
		GHashTableIter ht_iter;
		gpointer ht_value;

		selected_fonts = g_ptr_array_new_full (g_hash_table_size (all_fonts), NULL);

		tmp_array = g_ptr_array_new_full (g_hash_table_size (all_fonts), NULL);
		g_hash_table_iter_init (&ht_iter, all_fonts);
		while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value))
			g_ptr_array_add (tmp_array, ht_value);

		/* prepend fonts that contain "regular" so we prefer the regular
		 * font face for rendering samples over the other styles
		 * also ensure that the font style list is sorted for more
		 * deterministic results. */
		g_ptr_array_sort (tmp_array, asc_font_entry_cmp);
		for (guint i = 0; i < tmp_array->len; i++) {
			g_autofree gchar *font_style_id = NULL;
			AscFontEntry *entry = g_ptr_array_index (tmp_array, i);

			font_style_id = g_utf8_strdown (entry->info->style ? entry->info->style
									   : "",
							-1);
			if (!regular_found && g_strstr_len (font_style_id, -1, "regular") != NULL) {
				g_ptr_array_insert (selected_fonts, 0, entry);
				/* if we found a font which has a style that equals "regular",
				 * we can stop searching for the preferred font */
				if (g_strcmp0 (font_style_id, "regular") == 0)
					regular_found = TRUE;
			} else {
				g_ptr_array_add (selected_fonts, entry);
			}
		}
	} else {
		selected_fonts = g_ptr_array_new_full (font_hints->len, NULL);

		/* Find fonts based on the hints we have.
		 * The hint as well as the dictionary keys are all lowercased, so we
		 * can do case-insensitive matching here. */
		for (guint i = 0; i < font_hints->len; i++) {
			AscFontEntry *entry;
			const gchar *font_hint = g_ptr_array_index (font_hints, i);

			entry = g_hash_table_lookup (all_fonts, font_hint);
			if (entry == NULL)
				continue;
			g_ptr_array_add (selected_fonts, entry);
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
		while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value)) {
			AscFontEntry *entry = ht_value;
			g_string_append_printf (font_names_str, "%s | ", entry->info->fullname);
		}
		if (font_names_str->len > 4)
			g_string_truncate (font_names_str, font_names_str->len - 3);

		asc_result_add_hint (cres,
				     cpt,
				     "font-metainfo-but-no-font",
				     "font_names",
				     font_names_str->str,
				     NULL);
		return;
	}

	/* language information of fonts is often completely wrong. In case there was a metainfo file
	 * with languages explicitly set, we take the first language and prefer that over the others.
	 * Languages mentioned in the metainfo file are also considered supported by the respective
	 * fonts. These overrides are passed to the media worker with every render request. */
	cpt_languages = as_component_get_languages (cpt);
	if (cpt_languages != NULL) {
		preferred_lang = cpt_languages->data;

		extra_langs_array = g_ptr_array_new ();
		for (GList *item = cpt_languages->next; item != NULL; item = item->next)
			g_ptr_array_add (extra_langs_array, item->data);
		g_ptr_array_add (extra_langs_array, NULL);
		extra_langs = (const gchar **) extra_langs_array->pdata;
	}

	g_debug ("Rendering font data for %s", gcid);

	/* try to render icon with "Regular"-style font, if we find none we just pick the first font in the list */
	has_icon = FALSE;
	for (guint i = 0; i < selected_fonts->len; i++) {
		AscFontEntry *entry = g_ptr_array_index (selected_fonts, i);

		if (!as_str_equal0 (entry->info->style, "Regular") &&
		    !as_str_equal0 (entry->info->style, "regular"))
			continue;
		g_debug ("Rendering font icon using '%s'", entry->info->id);
		has_icon = asc_render_font_icon (cres,
						 unit,
						 entry,
						 media,
						 cpt_icons_path,
						 icons_export_dir,
						 cpt,
						 icon_policy,
						 preferred_lang,
						 extra_langs);
		break;
	}

	/* process font files */
	for (guint i = 0; i < selected_fonts->len; i++) {
		AscFontEntry *entry = g_ptr_array_index (selected_fonts, i);

		g_debug ("Processing font '%s'", entry->info->id);

		/* add language information */
		if (entry->info->languages != NULL) {
			for (guint j = 0; entry->info->languages[j] != NULL; j++) {
				/* we have no idea how well the font supports the language's script,
				 * but since it advertises support in its metadata, we just assume 100% here */
				as_component_add_language (cpt, entry->info->languages[j], 100);
			}
		}
		/* languages mentioned in the metainfo data are supported as well */
		for (GList *item = cpt_languages ? cpt_languages->next : NULL; item != NULL;
		     item = item->next)
			as_component_add_language (cpt, item->data, 100);

		/* render an icon for our font */
		if (!has_icon)
			has_icon = asc_render_font_icon (cres,
							 unit,
							 entry,
							 media,
							 cpt_icons_path,
							 icons_export_dir,
							 cpt,
							 icon_policy,
							 preferred_lang,
							 extra_langs);

		/* set additional metadata. The font metadata might be terrible, but if the data is bad
		 * it hopefully motivates people to write proper metainfo files. */
		if (as_is_empty (as_component_get_description (cpt)) &&
		    !as_is_empty (entry->info->description))
			as_component_set_description (cpt, entry->info->description, "C");

		if (as_is_empty (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE)) &&
		    !as_is_empty (entry->info->homepage))
			as_component_add_url (cpt, AS_URL_KIND_HOMEPAGE, entry->info->homepage);
	}

	/* render all sample screenshots for all font styles we have */
	if (as_flags_contains (flags, ASC_COMPOSE_FLAG_STORE_SCREENSHOTS)) {
		if (cpt_screenshots_path == NULL)
			g_warning ("Screenshot storage is enabled, but no screenshot media path "
				   "could be constructed for %s.",
				   as_component_get_id (cpt));
		else
			asc_render_font_screenshots (cres,
						     selected_fonts,
						     media,
						     cpt_screenshots_path,
						     cpt,
						     preferred_lang,
						     extra_langs);
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
		   AscMedia *media,
		   const gchar *prefix,
		   const gchar *media_export_root,
		   const gchar *icons_export_dir,
		   AscIconPolicy *icon_policy,
		   AscComposeFlags flags)
{
	GPtrArray *contents = NULL;
	g_autofree gchar *fonts_dir = NULL;
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

	fonts_dir = g_build_filename (prefix, "share", "fonts", NULL);
	all_fonts = g_hash_table_new_full (g_str_hash,
					   g_str_equal,
					   g_free,
					   (GDestroyNotify) asc_font_entry_free);

	/* create a map of all fonts that this unit contains */
	contents = asc_unit_get_contents (unit);
	for (guint i = 0; i < contents->len; i++) {
		g_autoptr(GBytes) font_bytes = NULL;
		g_autoptr(GError) tmp_error = NULL;
		AscFontInfo *finfo = NULL;
		g_autofree gchar *basename = NULL;
		g_autofree gchar *font_fullname_lower = NULL;
		const gchar *fname = g_ptr_array_index (contents, i);
		if (!g_str_has_prefix (fname, fonts_dir))
			continue;
		if (!g_str_has_suffix (fname, ".ttf") && !g_str_has_suffix (fname, ".otf") &&
		    !g_str_has_suffix (fname, ".ttc"))
			continue;

		basename = g_path_get_basename (fname);
		font_bytes = asc_unit_read_data (unit, fname, &tmp_error);
		if (font_bytes == NULL) {
			asc_result_add_hint (cres,
					     NULL,
					     "file-read-error",
					     "fname",
					     fname,
					     "msg",
					     tmp_error->message,
					     NULL);
			continue;
		}

		/* have the media worker extract the font metadata */
		finfo = asc_media_read_font_info (media,
						  font_bytes,
						  basename,
						  NULL, /* preferred language */
						  NULL, /* extra languages */
						  NULL, /* custom sample text */
						  NULL, /* custom icon text */
						  &tmp_error);
		if (finfo == NULL) {
			asc_result_add_hint (cres,
					     NULL,
					     "font-load-error",
					     "fname",
					     fname,
					     "unit_name",
					     asc_unit_get_bundle_id (unit),
					     "error",
					     tmp_error->message,
					     NULL);
			continue;
		}

		g_debug ("Found font %s/%s", basename, finfo->fullname);
		font_fullname_lower = g_utf8_strdown (finfo->fullname, -1);
		g_hash_table_insert (all_fonts,
				     g_steal_pointer (&font_fullname_lower),
				     asc_font_entry_new (finfo, font_bytes, basename));
	}

	/* process fonts for all components */
	for (guint i = 0; i < font_cpts->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (font_cpts, i));
		process_font_data_for_component (cres,
						 unit,
						 cpt,
						 all_fonts,
						 media,
						 media_export_root,
						 icons_export_dir,
						 icon_policy,
						 flags);
	}
}
