/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include <glib.h>
#include "appstream-compose.h"
#include "asc-font-private.h"

#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * test_read_fontinfo:
 *
 * Extract font information from a font file.
 */
static void
test_read_fontinfo ()
{
	g_autofree gchar *font_fname = NULL;
	g_autoptr(AscFont) font = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *data = NULL;
	gsize data_len;
	g_autoptr(GList) lang_list = NULL;
	guint i;
	const gchar *expected_langs[] =  { "aa", "ab", "af", "ak", "an", "ast", "av", "ay", "az-az", "ba", "be", "ber-dz", "bg", "bi", "bin",
					   "bm", "br", "bs", "bua", "ca", "ce", "ch", "chm", "co", "crh", "cs", "csb", "cu", "cv", "cy", "da",
					   "de", "ee", "el", "en", "eo", "es", "et", "eu", "fat", "ff", "fi", "fil", "fj", "fo", "fr", "fur",
					   "fy", "ga", "gd", "gl", "gn", "gv", "ha", "haw", "ho", "hr", "hsb", "ht", "hu", "hz", "ia", "id",
					   "ie", "ig", "ik", "io", "is", "it", "jv", "kaa", "kab", "ki", "kj", "kk", "kl", "kr", "ku-am",
					   "ku-tr", "kum", "kv", "kw", "kwm", "ky", "la", "lb", "lez", "lg", "li", "ln", "lt","lv", "mg", "mh",
					   "mi", "mk", "mn-mn", "mo", "ms", "mt", "na", "nb", "nds", "ng", "nl", "nn", "no", "nr", "nso", "nv",
					   "ny", "oc", "om", "os", "pap-an", "pap-aw", "pl", "pt", "qu", "quz", "rm", "rn", "ro", "ru", "rw",
					   "sah", "sc", "sco", "se", "sel", "sg", "sh", "shs", "sk", "sl", "sm","sma", "smj", "smn", "sms", "sn",
					   "so", "sq", "sr", "ss", "st", "su", "sv", "sw", "tg", "tk", "tl", "tn", "to", "tr", "ts", "tt", "tw",
					   "ty", "tyv", "uk", "uz", "ve", "vi", "vo", "vot", "wa", "wen", "wo", "xh", "yap", "yo", "za", "zu", NULL };

	font_fname = g_build_filename (datadir, "NotoSans-Regular.ttf", NULL);

	/* test reading from file */
	font = asc_font_new_from_file (font_fname, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (asc_font_get_family (font), ==, "Noto Sans");
	g_assert_cmpstr (asc_font_get_style (font), ==, "Regular");
	g_object_unref (font);
	font = NULL;

	/* test reading from memory */
	g_file_get_contents (font_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	font = asc_font_new_from_data (data, data_len, "NotoSans-Regular.ttf", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (asc_font_get_family (font), ==, "Noto Sans");
	g_assert_cmpstr (asc_font_get_style (font), ==, "Regular");
	g_assert_cmpint (asc_font_get_charset (font), ==, FT_ENCODING_UNICODE);
	g_assert_cmpstr (asc_font_get_homepage (font), ==, "http://www.monotype.com/studio");
	g_assert_cmpstr (asc_font_get_description (font), ==, "Data hinted. Designed by Monotype design team.");

	lang_list = asc_font_get_language_list (font);
	i = 0;
	for (GList *l = lang_list; l != NULL; l = l->next) {
		g_assert (expected_langs[i] != NULL);
		g_assert_cmpstr (expected_langs[i], ==, l->data);
		i++;
	}

	/* uses "Noto Sans" */
	g_assert_cmpstr (asc_font_get_sample_text (font), ==, "My grandfather picks up quartz and valuable onyx jewels.");
	g_assert_cmpstr (asc_font_find_pangram(font, "en", "Noto Sans"), ==, "My grandfather picks up quartz and valuable onyx jewels.");

	g_assert_cmpstr (asc_font_find_pangram(font, "en", "aaaaa"), ==, "Pack my box with five dozen liquor jugs.");
	g_assert_cmpstr (asc_font_find_pangram(font, "en", "abcdefg"), ==, "Five or six big jet planes zoomed quickly past the tower.");
}

/**
 * test_image_transform:
 *
 * Test image related things, like transformations.
 */
static void
test_image_transform ()
{
	g_autoptr(GHashTable) supported_fmts = NULL;
	g_autofree gchar *sample_img_fname = NULL;
	g_autoptr(AscImage) image = NULL;
	g_autoptr(GError) error = NULL;

	g_autofree gchar *data = NULL;
	gsize data_len;

	/* check if our GdkPixbuf supports the minimum amount of image formats we need */
	supported_fmts = asc_image_supported_format_names ();
	g_assert (g_hash_table_contains (supported_fmts, "png"));
	g_assert (g_hash_table_contains (supported_fmts, "svg"));
	g_assert (g_hash_table_contains (supported_fmts, "jpeg"));

	sample_img_fname = g_build_filename (datadir, "appstream-logo.png", NULL);

	/* load image from file */
	image = asc_image_new_from_file (sample_img_fname, &error);
	g_assert_no_error (error);

	g_assert_cmpint (asc_image_get_width (image), ==, 136);
	g_assert_cmpint (asc_image_get_height (image), ==, 144);

	/* scale image */
	asc_image_scale (image, 64, 64);
	g_assert_cmpint (asc_image_get_width (image), ==, 64);
	g_assert_cmpint (asc_image_get_height (image), ==, 64);

	asc_image_save_png (image, "/tmp/asc-iscale_test.png", &error);
	g_assert_no_error (error);

	g_object_unref (image);
	image = NULL;

	/* test reading image from memory */
	g_file_get_contents (sample_img_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	image = asc_image_new_from_data (data, data_len, &error);
	g_assert_no_error (error);

	asc_image_scale (image, 124, 124);
	asc_image_save_png (image, "/tmp/asc-iscale-d_test.png", &error);
	g_assert_no_error (error);
}

/**
 * test_canvas:
 *
 * Test canvas.
 */
static void
test_canvas ()
{
	g_autofree gchar *sample_svg_fname = NULL;
	g_autofree gchar *font_fname = NULL;
	g_autofree gchar *data = NULL;
	gsize data_len;
	g_autoptr(AscCanvas) cv = NULL;
	g_autoptr(AscFont) font = NULL;
	g_autoptr(GInputStream) stream = NULL;
	g_autoptr(GError) error = NULL;

	sample_svg_fname = g_build_filename (datadir, "table.svgz", NULL);

	/* read & render compressed SVG file */
	g_file_get_contents (sample_svg_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	stream = g_memory_input_stream_new_from_data (data, data_len, NULL);

	cv = asc_canvas_new (512, 512);
	asc_canvas_render_svg (cv, stream, &error);
	g_assert_no_error (error);

	asc_canvas_save_png (cv, "/tmp/asc-svgrender_test1.png", &error);
	g_assert_no_error (error);

	g_object_unref (cv);
	cv = NULL;

	/* test font rendering */
	font_fname = g_build_filename (datadir, "NotoSans-Regular.ttf", NULL);
	font = asc_font_new_from_file (font_fname, &error);
	g_assert_no_error (error);

	cv = asc_canvas_new (400, 100);

	asc_canvas_draw_text (cv,
			      font,
			      "Hello World!\nSecond Line!\nThird line - äöüß!\nA very, very, very long line.",
			      -1, -1,
			      &error);
	g_assert_no_error (error);

	asc_canvas_save_png (cv, "/tmp/asc-fontrender_test1.png", &error);
	g_assert_no_error (error);
}

/**
 * test_compose_hints:
 *
 * Test compose hints and issue reporting.
 */
static void
test_compose_hints ()
{
	g_autoptr(AscHint) hint = NULL;
	g_autofree gchar *tmp = NULL;

	hint = asc_hint_new ();
	asc_hint_set_tag (hint, "dev-testsuite-test");
	asc_hint_set_severity (hint, AS_ISSUE_SEVERITY_INFO);
	g_assert (asc_hint_is_valid (hint));
	g_assert (!asc_hint_is_error (hint));

	asc_hint_set_explanation_template (hint,
					   "This is an explanation for {name} which contains {amount} placeholders, including one left {invalid} intentionally.");
	asc_hint_add_explanation_var (hint, "name", "the compose testsuite");
	asc_hint_add_explanation_var (hint, "amount", "3");

	tmp = asc_hint_format_explanation (hint);
	g_assert_cmpstr (tmp, ==, "This is an explanation for the compose testsuite which contains 3 placeholders, including one left {invalid} intentionally.");
}

int
main (int argc, char **argv)
{
	int ret;

	if (argc == 0) {
		g_error ("No test directory specified!");
		return 1;
	}

	g_assert (argv[1] != NULL);
	datadir = g_build_filename (argv[1], "samples", "compose", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/AppStream/Compose/FontInfo", test_read_fontinfo);
	g_test_add_func ("/AppStream/Compose/Image", test_image_transform);
	g_test_add_func ("/AppStream/Compose/Canvas", test_canvas);
	g_test_add_func ("/AppStream/Compose/Hints", test_compose_hints);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
