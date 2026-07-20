/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2026 Matthias Klumpp <matthias@tenstral.net>
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

/* Tests for the internal media processing code of the asc-mediaworker
 * helper binary. The code is exercised in-process here - the IPC interface
 * used by libappstream-compose is tested via AscMedia in test-compose.c. */

#include <glib.h>
#include <locale.h>
#include <fcntl.h>
#include <unistd.h>

#include "asc-media.h"
#include "asw-font-private.h"
#include "asw-image-private.h"
#include "asw-canvas.h"

#include "as-utils-private.h"
#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * test_read_fontinfo:
 *
 * Extract font information from a font file.
 */
static void
test_read_fontinfo (void)
{
	g_autofree gchar *font_fname = NULL;
	g_autoptr(AswFont) font = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *data = NULL;
	gsize data_len;
	g_autoptr(GList) lang_list = NULL;
	const gchar *expected_langs_old_fontconfig[] = {
		"aa",	  "ab",	   "af",  "an",	 "ast",	  "av",	   "ay",  "az-az", "ba",  "be",
		"bg",	  "bi",	   "bin", "br",	 "bs",	  "bua",   "ca",  "ce",	   "ch",  "chm",
		"co",	  "crh",   "cs",  "csb", "cv",	  "cy",	   "da",  "de",	   "en",  "eo",
		"es",	  "et",	   "eu",  "fi",	 "fil",	  "fj",	   "fo",  "fr",	   "fur", "fy",
		"gd",	  "gl",	   "gn",  "gv",	 "haw",	  "ho",	   "hr",  "hsb",   "ht",  "hu",
		"ia",	  "id",	   "ie",  "ig",	 "ik",	  "io",	   "is",  "it",	   "jv",  "kaa",
		"ki",	  "kj",	   "kk",  "kl",	 "ku-am", "ku-tr", "kum", "kv",	   "kw",  "kwm",
		"ky",	  "la",	   "lb",  "lez", "lg",	  "li",	   "lt",  "lv",	   "mg",  "mh",
		"mk",	  "mn-mn", "mo",  "ms",	 "mt",	  "na",	   "nb",  "nds",   "ng",  "nl",
		"nn",	  "no",	   "nr",  "nso", "nv",	  "ny",	   "oc",  "om",	   "os",  "pap-an",
		"pap-aw", "pl",	   "pt",  "qu",	 "quz",	  "rm",	   "rn",  "ro",	   "ru",  "rw",
		"sah",	  "sc",	   "se",  "sel", "sg",	  "sh",	   "sk",  "sl",	   "sm",  "sma",
		"smj",	  "smn",   "sn",  "so",	 "sq",	  "sr",	   "ss",  "st",	   "su",  "sv",
		"sw",	  "tg",	   "tk",  "tl",	 "tn",	  "to",	   "tr",  "ts",	   "tt",  "ty",
		"tyv",	  "uk",	   "uz",  "vi",	 "vo",	  "vot",   "wa",  "wen",   "wo",  "xh",
		"yap",	  "za",	   "zu",  NULL
	};
	const gchar *expected_langs[] = {
		"aa",	  "ab",	    "af",  "agr", "an",	 "ast", "av",  "ay",  "ayc",   "az-az",
		"ba",	  "be",	    "bem", "bg",  "bi",	 "bin", "br",  "bs",  "bua",   "ca",
		"ce",	  "ch",	    "chm", "co",  "crh", "cs",	"csb", "cv",  "cy",    "da",
		"de",	  "dsb",    "en",  "eo",  "es",	 "et",	"eu",  "fi",  "fil",   "fj",
		"fo",	  "fr",	    "fur", "fy",  "gd",	 "gl",	"gn",  "gv",  "haw",   "ho",
		"hr",	  "hsb",    "ht",  "hu",  "ia",	 "id",	"ie",  "ig",  "ik",    "io",
		"is",	  "it",	    "jv",  "kaa", "ki",	 "kj",	"kk",  "kl",  "ku-am", "ku-tr",
		"kum",	  "kv",	    "kw",  "kwm", "ky",	 "la",	"lb",  "lez", "lg",    "li",
		"lij",	  "lt",	    "lv",  "mfe", "mg",	 "mh",	"mhr", "miq", "mjw",   "mk",
		"mn-mn",  "mo",	    "ms",  "mt",  "na",	 "nb",	"nds", "ng",  "nhn",   "niu",
		"nl",	  "nn",	    "no",  "nr",  "nso", "nv",	"ny",  "oc",  "om",    "os",
		"pap-an", "pap-aw", "pl",  "pt",  "qu",	 "quz", "rm",  "rn",  "ro",    "ru",
		"rw",	  "sah",    "sc",  "se",  "sel", "sg",	"sgs", "sh",  "sk",    "sl",
		"sm",	  "sma",    "smj", "smn", "sn",	 "so",	"sq",  "sr",  "ss",    "st",
		"su",	  "sv",	    "sw",  "szl", "tg",	 "tk",	"tl",  "tn",  "to",    "tpi",
		"tr",	  "ts",	    "tt",  "ty",  "tyv", "uk",	"unm", "uz",  "vi",    "vo",
		"vot",	  "wa",	    "wae", "wen", "wo",	 "xh",	"yap", "yuw", "za",    "zu",
		NULL
	};

	font_fname = g_build_filename (datadir, "Raleway-Regular.ttf", NULL);

	/* test reading from file */
	font = asw_font_new_from_file (font_fname, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (asw_font_get_family (font), ==, "Raleway");
	g_assert_cmpstr (asw_font_get_style (font), ==, "Regular");
	g_object_unref (font);
	font = NULL;

	/* test reading from memory */
	g_file_get_contents (font_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	font = asw_font_new_from_data (data, data_len, "Raleway-Regular.ttf", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (asw_font_get_family (font), ==, "Raleway");
	g_assert_cmpstr (asw_font_get_style (font), ==, "Regular");
	g_assert_cmpint (asw_font_get_charset (font), ==, FT_ENCODING_UNICODE);
	g_assert_cmpstr (asw_font_get_homepage (font), ==, "http://pixelspread.com");
	g_assert_cmpstr (asw_font_get_description (font),
			 ==,
			 "Raleway is an elegant sans-serif typeface family. "
			 "Initially designed by Matt McInerney as a single thin weight, "
			 "it was expanded into a 9 weight family by Pablo Impallari and "
			 "Rodrigo Fuenzalida in 2012 and iKerned by Igino Marini. "
			 "It is a display face and the download features both old style "
			 "and lining numerals, standard and discretionary ligatures, a "
			 "pretty complete set of diacritics, as well as a stylistic "
			 "alternate inspired by more geometric sans-serif typefaces "
			 "than its neo-grotesque inspired default character set.");

	lang_list = asw_font_get_language_list (font);

	{
		guint i = 0;
		gboolean fc_lang_success = TRUE;
		for (GList *l = lang_list; l != NULL; l = l->next) {
			g_assert_nonnull (expected_langs_old_fontconfig[i]);
			if (!as_str_equal0 (expected_langs_old_fontconfig[i], l->data)) {
				fc_lang_success = FALSE;
				break;
			}
			i++;
		}
		if (!fc_lang_success) {
			i = 0;
			for (GList *l = lang_list; l != NULL; l = l->next) {
				g_assert_nonnull (expected_langs[i]);
				g_assert_cmpstr (expected_langs[i], ==, l->data);
				i++;
			}
		}
	}

	/* uses "Noto Sans" */
	g_assert_cmpstr (asw_font_get_sample_text (font),
			 ==,
			 "A mad boxer shot a quick, gloved jab to the jaw of his dizzy opponent.");
	g_assert_cmpstr (asw_font_find_pangram (font, "en", "Raleway"),
			 ==,
			 "A mad boxer shot a quick, gloved jab to the jaw of his dizzy opponent.");

	g_assert_cmpstr (asw_font_find_pangram (font, "en", "aaaaa"),
			 ==,
			 "Pack my box with five dozen liquor jugs.");
	g_assert_cmpstr (asw_font_find_pangram (font, "en", "abcdefg"),
			 ==,
			 "Five or six big jet planes zoomed quickly past the tower.");
}

/**
 * test_font_from_fd:
 *
 * Read a font the way the worker does, from a file descriptor.
 */
static void
test_font_from_fd (void)
{
	g_autofree gchar *font_fname = NULL;
	g_autoptr(AswFont) font = NULL;
	g_autoptr(GError) error = NULL;
	gint fd;

	font_fname = g_build_filename (datadir, "Raleway-Regular.ttf", NULL);
	fd = open (font_fname, O_RDONLY);
	g_assert_cmpint (fd, >=, 0);

	font = asw_font_new_from_fd (fd, "Raleway-Regular.ttf", &error);
	close (fd);
	g_assert_no_error (error);

	g_assert_cmpstr (asw_font_get_family (font), ==, "Raleway");
	g_assert_cmpstr (asw_font_get_style (font), ==, "Regular");
	g_assert_cmpstr (asw_font_get_id (font), ==, "raleway-regular");
}

/**
 * test_image_transform:
 *
 * Test image related things, like transformations.
 */
static void
test_image_transform (void)
{
	g_autoptr(GHashTable) supported_fmts = NULL;
	g_autofree gchar *sample_img_fname = NULL;
	g_autofree gchar *sample_jxl_img_fname = NULL;
	g_autoptr(AswImage) image = NULL;
	g_autoptr(GError) error = NULL;
	gboolean ret;

	g_autofree gchar *data = NULL;
	gsize data_len;

	/* check if our GdkPixbuf supports the minimum amount of image formats we need */
	supported_fmts = asw_image_supported_format_names ();
	g_assert_true (g_hash_table_contains (supported_fmts, "png"));
	g_assert_true (g_hash_table_contains (supported_fmts, "svg"));
	g_assert_true (g_hash_table_contains (supported_fmts, "jpeg"));

	sample_img_fname = g_build_filename (datadir, "appstream-logo.png", NULL);
	sample_jxl_img_fname = g_build_filename (datadir, "image.jxl", NULL);

	/* load image from file */
	image = asw_image_new_from_file (sample_img_fname,
					 -1,
					 -1,
					 ASW_IMAGE_LOAD_FLAG_NONE,
					 &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);

	g_assert_cmpint (asw_image_get_width (image), ==, 136);
	g_assert_cmpint (asw_image_get_height (image), ==, 144);

	/* scale image */
	asw_image_scale (image, 64, 64);
	g_assert_cmpint (asw_image_get_width (image), ==, 64);
	g_assert_cmpint (asw_image_get_height (image), ==, 64);

	ret = asw_image_save_filename (image,
				       "/tmp/asw-iscale_test.png",
				       0,
				       0,
				       ASW_IMAGE_SAVE_FLAG_NONE,
				       &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	g_clear_object (&image);

	/* test reading image from memory */
	g_file_get_contents (sample_img_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	image = asw_image_new_from_data (data,
					 data_len,
					 -1,
					 -1,
					 ASW_IMAGE_LOAD_FLAG_NONE,
					 ASW_IMAGE_FORMAT_UNKNOWN,
					 &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);

	asw_image_scale (image, 124, 124);
	ret = asw_image_save_filename (image,
				       "/tmp/asw-iscale-d_test.png",
				       0,
				       0,
				       ASW_IMAGE_SAVE_FLAG_NONE,
				       &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_clear_object (&image);

	/* test loading a JPEG-XL image */
	image = asw_image_new_from_file (sample_jxl_img_fname,
					 -1,
					 -1,
					 ASW_IMAGE_LOAD_FLAG_NONE,
					 &error);
	if (g_hash_table_contains (supported_fmts, "jxl")) {
		g_assert_no_error (error);
		g_assert_nonnull (image);

		g_assert_cmpint (asw_image_get_width (image), ==, 64);
		g_assert_cmpint (asw_image_get_height (image), ==, 64);
	} else {
		g_assert_error (error, ASC_MEDIA_ERROR, ASC_MEDIA_ERROR_UNSUPPORTED);
		g_assert_null (image);
		g_clear_error (&error);
	}
	g_clear_object (&image);
}

/**
 * test_canvas:
 *
 * Test canvas.
 */
static void
test_canvas (void)
{
	g_autofree gchar *sample_svg_fname = NULL;
	g_autofree gchar *font_fname = NULL;
	g_autofree gchar *data = NULL;
	gsize data_len;
	gint cv_size;
	gint text_border_width, shape_border_width;
	AswCanvasShape bg_shape;
	g_autoptr(AswCanvas) cv = NULL;
	g_autoptr(AswFont) font = NULL;
	g_autoptr(GInputStream) stream = NULL;
	g_autoptr(GError) error = NULL;

	sample_svg_fname = g_build_filename (datadir, "table.svgz", NULL);

	/* read & render compressed SVG file */
	g_file_get_contents (sample_svg_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	stream = g_memory_input_stream_new_from_data (data, data_len, NULL);

	cv = asw_canvas_new (512, 512);
	asw_canvas_render_svg (cv, stream, &error);
	g_assert_no_error (error);

	asw_canvas_save_png (cv, "/tmp/asw-svgrender_test1.png", &error);
	g_assert_no_error (error);

	g_object_unref (cv);
	cv = NULL;

	/* test font rendering */
	font_fname = g_build_filename (datadir, "Raleway-Regular.ttf", NULL);
	font = asw_font_new_from_file (font_fname, &error);
	g_assert_no_error (error);

	cv = asw_canvas_new (400, 100);

	asw_canvas_draw_text (
	    cv,
	    font,
	    "Hello World!\nSecond Line!\nThird line - äöüß!\nA very, very, very long line.",
	    -1,
	    -1,
	    &error);
	g_assert_no_error (error);

	asw_canvas_save_png (cv, "/tmp/asw-fontrender_test1.png", &error);
	g_assert_no_error (error);
	g_object_unref (cv);

	cv_size = 128;
	bg_shape = ASW_CANVAS_SHAPE_CVL_TRIANGLE;
	shape_border_width = (gint) (cv_size * 0.032);
	text_border_width = asw_calculate_text_border_width_for_icon_shape (bg_shape,
									    cv_size,
									    shape_border_width);

	cv = asw_canvas_new (cv_size, cv_size);
	asw_canvas_draw_shape (cv,
			       bg_shape,
			       shape_border_width,
			       0.84, /* red */
			       0.84, /* green */
			       0.84, /* blue */
			       &error);
	g_assert_no_error (error);

	asw_canvas_draw_text_line (cv,
				   font,
				   "Aa",
				   text_border_width,
				   bg_shape == ASW_CANVAS_SHAPE_CVL_TRIANGLE
				       ? (gint) ((cv_size / 2.0 - shape_border_width) * 0.15)
				       : 0,
				   &error);
	g_assert_no_error (error);
	asw_canvas_save_png (cv, "/tmp/asw-fontrender_test2.png", &error);
	g_assert_no_error (error);
}

/**
 * test_render_font_card:
 *
 * Test font-card rendering on canvas.
 */
static void
test_render_font_card (void)
{
	g_autofree gchar *font_fname = NULL;
	g_autoptr(AswFont) font = NULL;
	g_autoptr(AswCanvas) canvas = NULL;
	g_autoptr(GError) error = NULL;

	font_fname = g_build_filename (datadir, "Raleway-Regular.ttf", NULL);
	font = asw_font_new_from_file (font_fname, &error);
	g_assert_no_error (error);

	canvas = asw_canvas_new (800, 600);
	asw_canvas_draw_font_card (canvas,
				   font,
				   NULL, /* default info label */
				   asw_font_find_pangram (font, "en", "Raleway"), /* pangram */
				   NULL, /* default bg letter */
				   -1,	 /* default margin    */
				   &error);
	g_assert_no_error (error);

	asw_canvas_save_png (canvas, "/tmp/asw-font-card.png", &error);
	g_assert_no_error (error);
}

/**
 * test_render_font_files:
 *
 * Test the high-level font rendering entry points used
 * by the worker's request handlers.
 */
static void
test_render_font_files (void)
{
	g_autofree gchar *font_fname = NULL;
	g_autoptr(AswFont) font = NULL;
	g_autoptr(GError) error = NULL;
	gint width = 0;
	gint height = 0;
	gboolean ret;

	font_fname = g_build_filename (datadir, "Raleway-Regular.ttf", NULL);
	font = asw_font_new_from_file (font_fname, &error);
	g_assert_no_error (error);

	ret = asw_font_render_card_to_file (font,
					    "/tmp/asw-font-card-hl.png",
					    752,
					    423,
					    NULL, /* default info label */
					    &width,
					    &height,
					    &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_cmpint (width, ==, 752);
	g_assert_cmpint (height, >, 0);
	g_assert_true (g_file_test ("/tmp/asw-font-card-hl.png", G_FILE_TEST_EXISTS));

	ret = asw_font_render_icon_to_file (font,
					    "/tmp/asw-font-icon-hl.png",
					    64,
					    &width,
					    &height,
					    &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_cmpint (width, ==, 64);
	g_assert_cmpint (height, ==, 64);
	g_assert_true (g_file_test ("/tmp/asw-font-icon-hl.png", G_FILE_TEST_EXISTS));
}

int
main (int argc, char **argv)
{
	int ret;

	setlocale (LC_ALL, "");

	if (argc == 0) {
		g_error ("No test directory specified!");
		return 1;
	}

	g_assert_nonnull (argv[1]);
	datadir = g_build_filename (argv[1], "samples", "compose", NULL);
	g_assert_true (g_file_test (datadir, G_FILE_TEST_EXISTS));

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/AppStream/ComposeWorker/FontInfo", test_read_fontinfo);
	g_test_add_func ("/AppStream/ComposeWorker/FontFromFd", test_font_from_fd);
	g_test_add_func ("/AppStream/ComposeWorker/Image", test_image_transform);
	g_test_add_func ("/AppStream/ComposeWorker/Canvas", test_canvas);
	g_test_add_func ("/AppStream/ComposeWorker/FontCard", test_render_font_card);
	g_test_add_func ("/AppStream/ComposeWorker/FontRenderFiles", test_render_font_files);

	ret = g_test_run ();
	g_free (datadir);

	return ret;
}
