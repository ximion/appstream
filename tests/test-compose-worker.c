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
#include <glib/gstdio.h>
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "asc-media.h"
#include "asw-font-private.h"
#include "asw-image-private.h"
#include "asw-canvas.h"
#include "asw-sandbox.h"

#include "as-utils-private.h"
#include "as-test-utils.h"

static gchar *datadir = NULL;
static gchar *workdir = NULL;

/**
 * asx_build_workdir_path:
 *
 * Build a path for @name within this test run's private working directory.
 */
static gchar *
asx_build_workdir_path (const gchar *name)
{
	return g_build_filename (workdir, name, NULL);
}

/**
 * asx_open_out_fd:
 *
 * Open a writable descriptor for @fname.
 */
static gint
asx_open_out_fd (const gchar *fname)
{
	gint fd = g_open (fname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	g_assert_cmpint (fd, >=, 0);
	return fd;
}

/**
 * asx_image_save_path:
 *
 * Encode @image into a newly created file at @fname, deriving the target
 * format from its extension the way #AscMedia does.
 */
static gboolean
asx_image_save_path (AswImage *image,
		     const gchar *fname,
		     gint width,
		     gint height,
		     AscImageSaveFlags flags,
		     GError **error)
{
	AscImageFormat format = asc_image_format_from_filename (fname);
	gboolean ret;
	gint fd;

	fd = asx_open_out_fd (fname);
	ret = asw_image_save_fd (image, fd, format, width, height, flags, error);
	close (fd);
	return ret;
}

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
	g_autofree gchar *sample_svgz_img_fname = NULL;
	g_autoptr(AswImage) image = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *out_fname = NULL;
	gboolean ret;

	g_autofree gchar *data = NULL;
	gsize data_len;

	/* check if our libvips supports the minimum amount of image formats we need */
	supported_fmts = asw_image_supported_format_names ();
	g_assert_true (g_hash_table_contains (supported_fmts, "png"));
	g_assert_true (g_hash_table_contains (supported_fmts, "svg"));
	g_assert_true (g_hash_table_contains (supported_fmts, "jpeg"));
	g_assert_true (g_hash_table_contains (supported_fmts, "jxl"));

	sample_img_fname = g_build_filename (datadir, "appstream-logo.png", NULL);
	sample_jxl_img_fname = g_build_filename (datadir, "image.jxl", NULL);
	sample_svgz_img_fname = g_build_filename (datadir, "table.svgz", NULL);

	/* load image from file */
	image = asw_image_new_from_file (sample_img_fname, -1, -1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);

	g_assert_cmpint (asw_image_get_width (image), ==, 136);
	g_assert_cmpint (asw_image_get_height (image), ==, 144);

	/* scale image */
	asw_image_scale (image, 64, 64);
	g_assert_cmpint (asw_image_get_width (image), ==, 64);
	g_assert_cmpint (asw_image_get_height (image), ==, 64);

	out_fname = asx_build_workdir_path ("asw-iscale_test.png");
	ret = asx_image_save_path (image, out_fname, 0, 0, ASC_IMAGE_SAVE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	g_clear_object (&image);

	/* test reading image from memory */
	g_file_get_contents (sample_img_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	image = asw_image_new_from_data (data, data_len, -1, -1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);

	asw_image_scale (image, 124, 124);
	g_clear_pointer (&out_fname, g_free);
	out_fname = asx_build_workdir_path ("asw-iscale-d_test.png");
	ret = asx_image_save_path (image, out_fname, 0, 0, ASC_IMAGE_SAVE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_clear_object (&image);

	/* vector graphics are rendered at the requested size, and GZip-compressed
	 * SVG data is decompressed by the image loader itself */
	g_clear_pointer (&data, g_free);
	g_file_get_contents (sample_svgz_img_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	image = asw_image_new_from_data (data, data_len, 96, 96, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);
	g_assert_cmpint (asw_image_get_width (image), ==, 96);
	g_assert_cmpint (asw_image_get_height (image), ==, 96);
	g_clear_object (&image);

	/* ... the same, but read from a file */
	image = asw_image_new_from_file (sample_svgz_img_fname, 96, 96, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);
	g_assert_cmpint (asw_image_get_width (image), ==, 96);
	g_assert_cmpint (asw_image_get_height (image), ==, 96);

	g_clear_pointer (&out_fname, g_free);
	out_fname = asx_build_workdir_path ("asw-svgzrender_test.jxl");
	ret = asx_image_save_path (image, out_fname, 0, 0, ASC_IMAGE_SAVE_FLAG_LOSSLESS, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_true (g_file_test (out_fname, G_FILE_TEST_EXISTS));
	g_clear_object (&image);

	/* test loading a JPEG-XL image */
	image = asw_image_new_from_file (sample_jxl_img_fname, -1, -1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);

	g_assert_cmpint (asw_image_get_width (image), ==, 64);
	g_assert_cmpint (asw_image_get_height (image), ==, 64);

	/* the save format is implied by the filename extension */
	g_clear_pointer (&out_fname, g_free);
	out_fname = asx_build_workdir_path ("asw-isave_test.jxl");
	ret = asx_image_save_path (image, out_fname, 0, 0, ASC_IMAGE_SAVE_FLAG_LOSSLESS, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_cmpint (asc_image_format_from_filename (out_fname), ==, ASC_IMAGE_FORMAT_JXL);

	/* we can not save images in formats that we only know how to read */
	g_clear_pointer (&out_fname, g_free);
	out_fname = asx_build_workdir_path ("asw-isave_test.webp");
	ret = asx_image_save_path (image, out_fname, 0, 0, ASC_IMAGE_SAVE_FLAG_NONE, &error);
	g_assert_error (error, ASC_MEDIA_ERROR, ASC_MEDIA_ERROR_UNSUPPORTED);
	g_assert_false (ret);
	g_clear_error (&error);

	/* ...and we refuse to guess if we were not told a format. Deriving one from
	 * the filename is the client's job now, so this is all the worker can check
	 * (see test-compose.c for the naming rules #AscMedia applies). */
	g_assert_cmpint (asc_image_format_from_filename ("asw-isave_test-noext"),
			 ==,
			 ASC_IMAGE_FORMAT_UNKNOWN);
	{
		gint fd;

		g_clear_pointer (&out_fname, g_free);
		out_fname = asx_build_workdir_path ("asw-isave_test-noext");
		fd = asx_open_out_fd (out_fname);
		ret = asw_image_save_fd (image,
					 fd,
					 ASC_IMAGE_FORMAT_UNKNOWN,
					 0,
					 0,
					 ASC_IMAGE_SAVE_FLAG_NONE,
					 &error);
		close (fd);
	}
	g_assert_error (error, ASC_MEDIA_ERROR, ASC_MEDIA_ERROR_UNSUPPORTED);
	g_assert_false (ret);
	g_clear_error (&error);

	g_clear_object (&image);
}

/**
 * asx_assert_pixel_is_marker:
 *
 * Check that the pixel at the given position is the red marker of the test drawing.
 */
static void
asx_assert_pixel_is_marker (AswImage *image, gint x, gint y)
{
	g_autofree double *point = NULL;
	int point_len = 0;

	if (vips_getpoint (asw_image_get_vips (image), &point, &point_len, x, y, NULL) != 0)
		g_error ("Unable to read pixel at %d/%d: %s", x, y, vips_error_buffer ());

	g_assert_cmpint (point_len, >=, 3);
	g_assert_cmpfloat (point[0], >, 200);
	g_assert_cmpfloat (point[1], <, 60);
	g_assert_cmpfloat (point[2], <, 60);
}

/* libvips only lays vector graphics out at the DPI it is given since 8.17.2. Older versions
 * lay every drawing out at 72dpi and merely zoom the result, so the resolution we ask for and
 * the scale that cancels it out annul each other there, and drawings keep being cut off at
 * their 72dpi viewport - which no option of the loader can do anything about. */
#define ASX_SVG_LAYOUT_USES_DPI \
	(VIPS_MAJOR_VERSION * 10000 + VIPS_MINOR_VERSION * 100 + VIPS_MICRO_VERSION >= 81702)

/**
 * test_image_physical_size:
 *
 * Vector graphics sized in physical units, without a viewBox, have to be laid out at the
 * 96dpi the SVG specification prescribes - at 72dpi their viewport is only 3/4 as large
 * and everything reaching beyond it is cut away.
 */
static void
test_image_physical_size (void)
{
	/* This drawing is 96x96 points, which makes it 128x128 pixels large. The marker in
	 * its lower right corner sits outside of a viewport that was set up at 72dpi, so it
	 * only shows up in a rendering that contains the whole drawing. */
	const gchar *svg_data =
	    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"96pt\" height=\"96pt\">\n"
	    "  <rect x=\"0\" y=\"0\" width=\"128\" height=\"128\" fill=\"#0000ff\" />\n"
	    "  <rect x=\"100\" y=\"100\" width=\"28\" height=\"28\" fill=\"#ff0000\" />\n"
	    "</svg>\n";

	g_autofree gchar *svg_fname = NULL;
	g_autoptr(AswImage) image = NULL;
	g_autoptr(GError) error = NULL;

	if (!ASX_SVG_LAYOUT_USES_DPI) {
		g_test_skip ("libvips " VIPS_VERSION " lays vector graphics out at 72dpi");
		return;
	}

	svg_fname = asx_build_workdir_path ("asw-physical-size.svg");
	g_file_set_contents (svg_fname, svg_data, -1, &error);
	g_assert_no_error (error);

	/* loaded at its native size, this is the size the drawing actually has */
	image = asw_image_new_from_file (svg_fname, -1, -1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);
	g_assert_cmpint (asw_image_get_width (image), ==, 128);
	g_assert_cmpint (asw_image_get_height (image), ==, 128);
	g_clear_object (&image);

	/* the same, read from memory */
	image = asw_image_new_from_data (svg_data, (gssize) strlen (svg_data), -1, -1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);
	g_assert_cmpint (asw_image_get_width (image), ==, 128);
	g_assert_cmpint (asw_image_get_height (image), ==, 128);
	g_clear_object (&image);

	/* rasterized at half its size, the marker has to be in there as well */
	image = asw_image_new_from_file (svg_fname, 64, 64, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);
	g_assert_cmpint (asw_image_get_width (image), ==, 64);
	g_assert_cmpint (asw_image_get_height (image), ==, 64);

	asx_assert_pixel_is_marker (image, 60, 60);
	g_clear_object (&image);

	/* ... and the same when the data is rasterized straight from memory */
	image = asw_image_new_from_data (svg_data, (gssize) strlen (svg_data), 64, 64, &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);
	asx_assert_pixel_is_marker (image, 60, 60);
}

/**
 * test_canvas:
 *
 * Test canvas.
 */
static void
test_canvas (void)
{
	g_autofree gchar *font_fname = NULL;
	gint cv_size;
	gint text_border_width, shape_border_width;
	AswCanvasShape bg_shape;
	g_autofree gchar *out_fname = NULL;
	g_autoptr(AswCanvas) cv = NULL;
	g_autoptr(AswFont) font = NULL;
	g_autoptr(GError) error = NULL;

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

	g_clear_pointer (&out_fname, g_free);
	out_fname = asx_build_workdir_path ("asw-fontrender_test1.png");
	{
		gint fd = asx_open_out_fd (out_fname);
		asw_canvas_save_png_fd (cv, fd, &error);
		close (fd);
	}
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
	g_clear_pointer (&out_fname, g_free);
	out_fname = asx_build_workdir_path ("asw-fontrender_test2.png");
	{
		gint fd = asx_open_out_fd (out_fname);
		asw_canvas_save_png_fd (cv, fd, &error);
		close (fd);
	}
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
	g_autofree gchar *out_fname = NULL;
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

	out_fname = asx_build_workdir_path ("asw-font-card.png");
	{
		gint fd = asx_open_out_fd (out_fname);
		asw_canvas_save_png_fd (canvas, fd, &error);
		close (fd);
	}
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
	g_autofree gchar *card_fname = NULL;
	g_autofree gchar *icon_fname = NULL;
	g_autofree gchar *jxl_icon_fname = NULL;
	g_autoptr(AswFont) font = NULL;
	g_autoptr(GError) error = NULL;
	gint card_fd, icon_fd, jxl_icon_fd;
	gint width = 0;
	gint height = 0;
	gboolean ret;

	font_fname = g_build_filename (datadir, "Raleway-Regular.ttf", NULL);
	font = asw_font_new_from_file (font_fname, &error);
	g_assert_no_error (error);

	card_fname = asx_build_workdir_path ("asw-font-card-hl.png");
	card_fd = asx_open_out_fd (card_fname);
	ret = asw_font_render_card_to_fd (font,
					  card_fd,
					  ASC_IMAGE_FORMAT_PNG,
					  752,
					  423,
					  NULL, /* default info label */
					  &width,
					  &height,
					  &error);
	close (card_fd);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_cmpint (width, ==, 752);
	g_assert_cmpint (height, >, 0);
	g_assert_true (g_file_test (card_fname, G_FILE_TEST_EXISTS));

	icon_fname = asx_build_workdir_path ("asw-font-icon-hl.png");
	icon_fd = asx_open_out_fd (icon_fname);
	ret = asw_font_render_icon_to_fd (font,
					  icon_fd,
					  ASC_IMAGE_FORMAT_PNG,
					  64,
					  &width,
					  &height,
					  &error);
	close (icon_fd);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_cmpint (width, ==, 64);
	g_assert_cmpint (height, ==, 64);
	g_assert_true (g_file_test (icon_fname, G_FILE_TEST_EXISTS));

	/* render a font icon as JPEG-XL, which is the default format for compose runs */
	jxl_icon_fname = asx_build_workdir_path ("asw-font-icon-hl.jxl");
	jxl_icon_fd = asx_open_out_fd (jxl_icon_fname);
	ret = asw_font_render_icon_to_fd (font,
					  jxl_icon_fd,
					  ASC_IMAGE_FORMAT_JXL,
					  64,
					  &width,
					  &height,
					  &error);
	close (jxl_icon_fd);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_cmpint (width, ==, 64);
	g_assert_cmpint (height, ==, 64);
	g_assert_true (g_file_test (jxl_icon_fname, G_FILE_TEST_EXISTS));
}

/**
 * test_sandbox_subprocess:
 *
 * Enter the sandbox and check that it restricts what we expect it to, and nothing
 * more. This runs in a subprocess because a Landlock domain can never be left
 * again - sandboxing the test binary itself would break every test after this one.
 */
static void
test_sandbox_subprocess (void)
{
	AswSandboxInfo info;
	g_autofree gchar *existing_fname = NULL;
	g_autofree gchar *new_fname = NULL;
	g_autofree gchar *new_dirname = NULL;
	g_autofree gchar *link_fname = NULL;
	gint preopened_fd;
	gint fd;

	/* a file we own from before the sandbox, standing in for the output
	 * descriptors that the client hands the real worker */
	existing_fname = asx_build_workdir_path ("asw-sandbox-preopened.bin");
	preopened_fd = asx_open_out_fd (existing_fname);

	new_fname = asx_build_workdir_path ("asw-sandbox-must-not-exist.bin");
	new_dirname = asx_build_workdir_path ("asw-sandbox-must-not-exist.dir");
	link_fname = asx_build_workdir_path ("asw-sandbox-must-not-exist.link");

	asw_sandbox_apply (&info);
	g_assert_cmpint (info.abi_version, >, 0);
	g_assert_true (info.state == ASW_SANDBOX_STATE_ENFORCED ||
		       info.state == ASW_SANDBOX_STATE_PARTIAL);
	g_assert_true (info.fs_writes_denied);

	/* creating, modifying and removing anything is gone */
	g_assert_cmpint (g_open (new_fname, O_CREAT | O_WRONLY, 0644), ==, -1);
	g_assert_cmpint (errno, ==, EACCES);
	g_assert_cmpint (g_mkdir (new_dirname, 0755), ==, -1);
	g_assert_cmpint (g_open (existing_fname, O_WRONLY, 0), ==, -1);
	g_assert_cmpint (errno, ==, EACCES);
	g_assert_cmpint (symlink (existing_fname, link_fname), ==, -1);
	g_assert_cmpint (g_unlink (existing_fname), ==, -1);

	/* ...but reading is untouched, so libvips modules and font data stay loadable */
	fd = g_open (existing_fname, O_RDONLY, 0);
	g_assert_cmpint (fd, >=, 0);
	close (fd);
	{
		g_autoptr(GDir) dir = g_dir_open (datadir, 0, NULL);
		g_assert_nonnull (dir);
	}

	/* the whole design rests on this one: a descriptor obtained before the
	 * sandbox went up is still writable afterwards */
	g_assert_cmpint (write (preopened_fd, "sandboxed", 9), ==, 9);
	close (preopened_fd);

	/* execute is untouched as well, so ffprobe can still be spawned */
	{
		g_autofree gchar *true_path = g_find_program_in_path ("true");
		if (true_path != NULL) {
			g_autoptr(GError) error = NULL;
			const gchar *argv[] = { true_path, NULL };
			gboolean ret = g_spawn_sync (NULL,
						     (gchar **) argv,
						     NULL,
						     G_SPAWN_DEFAULT,
						     NULL,
						     NULL,
						     NULL,
						     NULL,
						     NULL,
						     &error);
			g_assert_no_error (error);
			g_assert_true (ret);
		}
	}

	/* truncation is a separate right that only exists from ABI 3 on */
	if (info.abi_version >= 3)
		g_assert_cmpint (truncate (existing_fname, 0), ==, -1);

	/* TCP is denied outright from ABI 4 on. Landlock refuses this before any
	 * connection is attempted, so we do not need anything listening. */
	if (info.tcp_denied) {
		struct sockaddr_in addr;
		gint sock = socket (AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

		if (sock < 0)
			g_error ("Unable to create test socket: %s", g_strerror (errno));
		memset (&addr, 0, sizeof (addr));
		addr.sin_family = AF_INET;
		addr.sin_port = g_htons (80);
		addr.sin_addr.s_addr = g_htonl (INADDR_LOOPBACK);
		g_assert_cmpint (connect (sock, (struct sockaddr *) &addr, sizeof (addr)), ==, -1);
		g_assert_cmpint (errno, ==, EACCES);
		close (sock);
	}
}

/**
 * test_sandbox:
 *
 * Drive the sandbox subprocess check.
 */
static void
test_sandbox (void)
{
	if (asw_sandbox_probe_abi () == 0) {
		g_test_skip ("Landlock is not available on this kernel.");
		return;
	}

	g_test_trap_subprocess ("/AppStream/ComposeWorker/Sandbox/subprocess",
				0,
				G_TEST_SUBPROCESS_INHERIT_STDERR);
	g_test_trap_assert_passed ();
}

/**
 * test_sandbox_disabled_subprocess:
 *
 * With the escape hatch set, nothing may be restricted at all.
 */
static void
test_sandbox_disabled_subprocess (void)
{
	AswSandboxInfo info;
	g_autofree gchar *fname = NULL;
	gint fd;

	asw_sandbox_apply (&info);
	g_assert_cmpint (info.state, ==, ASW_SANDBOX_STATE_DISABLED);
	g_assert_false (info.fs_writes_denied);

	fname = asx_build_workdir_path ("asw-sandbox-disabled.bin");
	fd = g_open (fname, O_CREAT | O_WRONLY, 0644);
	g_assert_cmpint (fd, >=, 0);
	close (fd);
}

/**
 * test_sandbox_disabled:
 *
 * Drive the escape-hatch check.
 */
static void
test_sandbox_disabled (void)
{
	if (asw_sandbox_probe_abi () == 0) {
		g_test_skip ("Landlock is not available on this kernel.");
		return;
	}

	g_setenv ("ASC_NO_SANDBOX", "1", TRUE);
	g_test_trap_subprocess ("/AppStream/ComposeWorker/SandboxDisabled/subprocess",
				0,
				G_TEST_SUBPROCESS_INHERIT_STDERR);
	g_unsetenv ("ASC_NO_SANDBOX");
	g_test_trap_assert_passed ();
}

int
main (int argc, char **argv)
{
	int ret;
	g_autoptr(GError) error = NULL;

	setlocale (LC_ALL, "");

	/* The sandbox tests re-run this binary through g_test_trap_subprocess(), which
	 * does not pass our own arguments on, so the data location travels to the child
	 * through the environment instead. */
	if (g_getenv ("ASX_TEST_DATADIR") != NULL) {
		datadir = g_strdup (g_getenv ("ASX_TEST_DATADIR"));
	} else {
		if (argc <= 1 || argv[1] == NULL) {
			g_error ("No test directory specified!");
			return 1;
		}
		datadir = g_build_filename (argv[1], "samples", "compose", NULL);
		g_setenv ("ASX_TEST_DATADIR", datadir, TRUE);
	}
	g_assert_true (g_file_test (datadir, G_FILE_TEST_EXISTS));

	/* location for temporary test data */
	workdir = g_dir_make_tmp ("as-compose-worker-test_XXXXXX", NULL);
	g_assert_nonnull (workdir);

	if (!asw_image_backend_init (argv[0], &error))
		g_error ("Failed to initialize image processing backend: %s", error->message);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/AppStream/ComposeWorker/FontInfo", test_read_fontinfo);
	g_test_add_func ("/AppStream/ComposeWorker/FontFromFd", test_font_from_fd);
	g_test_add_func ("/AppStream/ComposeWorker/Image", test_image_transform);
	g_test_add_func ("/AppStream/ComposeWorker/ImagePhysicalSize", test_image_physical_size);
	g_test_add_func ("/AppStream/ComposeWorker/Canvas", test_canvas);
	g_test_add_func ("/AppStream/ComposeWorker/FontCard", test_render_font_card);
	g_test_add_func ("/AppStream/ComposeWorker/FontRenderFiles", test_render_font_files);
	g_test_add_func ("/AppStream/ComposeWorker/Sandbox", test_sandbox);
	g_test_add_func ("/AppStream/ComposeWorker/Sandbox/subprocess", test_sandbox_subprocess);
	g_test_add_func ("/AppStream/ComposeWorker/SandboxDisabled", test_sandbox_disabled);
	g_test_add_func ("/AppStream/ComposeWorker/SandboxDisabled/subprocess",
			 test_sandbox_disabled_subprocess);

	ret = g_test_run ();
	as_utils_delete_dir_recursive (workdir);
	g_free (workdir);
	g_free (datadir);
	asw_image_backend_shutdown ();

	return ret;
}
