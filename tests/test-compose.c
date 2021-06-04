/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2021 Matthias Klumpp <matthias@tenstral.net>
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
#include "asc-utils-metainfo.h"

#include "as-test-utils.h"

static gchar *datadir = NULL;

typedef struct
{
	gchar *path;
} Fixture;

/**
 * test_utils:
 *
 * Test global and utility functions.
 */
static void
test_utils ()
{
	gchar *tmp;

	tmp = asc_build_component_global_id ("foobar.desktop", "DEADBEEF");
	g_assert_cmpstr (tmp, ==, "f/fo/foobar.desktop/DEADBEEF");
	g_free (tmp);

	tmp = asc_build_component_global_id ("org.gnome.yelp.desktop", "DEADBEEF");
	g_assert_cmpstr (tmp, ==, "org/gnome/yelp.desktop/DEADBEEF");
	g_free (tmp);

	tmp = asc_build_component_global_id ("noto-cjk.font", "DEADBEEF");
	g_assert_cmpstr (tmp, ==, "n/no/noto-cjk.font/DEADBEEF");
	g_free (tmp);

	tmp = asc_build_component_global_id ("io.sample.awesomeapp.sdk", "ABAD1DEA");
	g_assert_cmpstr (tmp, ==, "io/sample/awesomeapp.sdk/ABAD1DEA");
	g_free (tmp);

	tmp = asc_build_component_global_id ("io.sample.awesomeapp.sdk", NULL);
	g_assert_cmpstr (tmp, ==, "io/sample/awesomeapp.sdk/last");
	g_free (tmp);
}

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
	gboolean ret;

	g_autofree gchar *data = NULL;
	gsize data_len;

	/* check if our GdkPixbuf supports the minimum amount of image formats we need */
	supported_fmts = asc_image_supported_format_names ();
	g_assert (g_hash_table_contains (supported_fmts, "png"));
	g_assert (g_hash_table_contains (supported_fmts, "svg"));
	g_assert (g_hash_table_contains (supported_fmts, "jpeg"));

	sample_img_fname = g_build_filename (datadir, "appstream-logo.png", NULL);

	/* load image from file */
	image = asc_image_new_from_file (sample_img_fname,
					 0,
					 ASC_IMAGE_LOAD_FLAG_NONE,
					 &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);

	g_assert_cmpint (asc_image_get_width (image), ==, 136);
	g_assert_cmpint (asc_image_get_height (image), ==, 144);

	/* scale image */
	asc_image_scale (image, 64, 64);
	g_assert_cmpint (asc_image_get_width (image), ==, 64);
	g_assert_cmpint (asc_image_get_height (image), ==, 64);

	ret = asc_image_save_filename (image,
				      "/tmp/asc-iscale_test.png",
				      0, 0,
				      ASC_IMAGE_SAVE_FLAG_NONE,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (image);
	image = NULL;

	/* test reading image from memory */
	g_file_get_contents (sample_img_fname, &data, &data_len, &error);
	g_assert_no_error (error);

	image = asc_image_new_from_data (data, data_len,
					 0,
					 ASC_IMAGE_LOAD_FLAG_NONE,
					 &error);
	g_assert_no_error (error);
	g_assert_nonnull (image);

	asc_image_scale (image, 124, 124);
	ret = asc_image_save_filename (image,
				      "/tmp/asc-iscale-d_test.png",
				      0, 0,
				      ASC_IMAGE_SAVE_FLAG_NONE,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);
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
	g_autoptr(GError) error = NULL;
	g_autofree gchar *tmp = NULL;

	hint = asc_hint_new_for_tag ("internal-unknown-tag", &error);
	g_assert_no_error (error);
	g_assert_nonnull (hint);

	g_assert_cmpstr (asc_hint_get_tag (hint), ==, "internal-unknown-tag");
	g_assert_cmpint (asc_hint_get_severity (hint), ==, AS_ISSUE_SEVERITY_ERROR);
	g_assert_cmpstr (asc_hint_get_explanation_template (hint), ==, "The given tag was unknown. This is a bug.");
	g_assert (asc_hint_is_valid (hint));
	g_assert (asc_hint_is_error (hint));

	asc_hint_set_tag (hint, "dev-testsuite-test");
	asc_hint_set_severity (hint, AS_ISSUE_SEVERITY_INFO);
	g_assert (asc_hint_is_valid (hint));
	g_assert (!asc_hint_is_error (hint));

	asc_hint_set_explanation_template (hint,
					   "This is an explanation for {{name}} which contains {{amount}} placeholders, "
					   "including one {odd} one and one left {{invalid}} intentionally.");
	asc_hint_add_explanation_var (hint, "name", "the compose testsuite");
	asc_hint_add_explanation_var (hint, "amount", "3");

	tmp = asc_hint_format_explanation (hint);
	g_assert_cmpstr (tmp, ==, "This is an explanation for the compose testsuite which contains 3 placeholders, "
				  "including one {odd} one and one left {{invalid}} intentionally.");
}

/**
 * test_compose_result:
 *
 * Test the result object.
 */
static void
test_compose_result ()
{
	g_autoptr(AscResult) cres = NULL;
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(GError) error = NULL;
	GPtrArray *hints;
	gchar *tmp;
	gboolean ret;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.freedesktop.appstream.dummy");

	cres = asc_result_new ();
	ret = asc_result_add_component_with_string (cres, cpt, "<testdata>", &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = asc_result_add_hint (cres, cpt,
				   "x-dev-testsuite-info",
				   "var1", "testvalue-info", NULL);
	g_assert_true (ret);

	g_assert_cmpint (asc_result_components_count (cres), ==, 1);
	g_assert_cmpint (asc_result_hints_count (cres), ==, 1);

	ret = asc_result_update_component_gcid_with_string (cres, cpt, "<moredata>");
	g_assert_true (ret);

	g_assert_true (asc_result_get_component (cres, "org.freedesktop.appstream.dummy") == cpt);

	ret = asc_result_add_hint (cres, cpt,
				   "x-dev-testsuite-error",
				   "var1", "testvalue-error", NULL);
	g_assert_false (ret);

	/* component no longer exists after an error, so this should fail now */
	ret = asc_result_update_component_gcid_with_string (cres, cpt, "<moredata>");
	g_assert_false (ret);

	g_assert_cmpint (asc_result_components_count (cres), ==, 0);
	g_assert_cmpint (asc_result_hints_count (cres), ==, 2);

	hints = asc_result_get_hints (cres, "org.freedesktop.appstream.dummy");
	g_assert_cmpint (hints->len, ==, 2);

	tmp = asc_hint_format_explanation (ASC_HINT (g_ptr_array_index (hints, 0)));
	g_assert_cmpstr (tmp, ==, "Dummy info hint for the testsuite. Var1: testvalue-info.");
	g_free (tmp);

	tmp = asc_hint_format_explanation (ASC_HINT (g_ptr_array_index (hints, 1)));
	g_assert_cmpstr (tmp, ==, "Dummy error hint for the testsuite. Var1: testvalue-error.");
	g_free (tmp);
}

/**
 * test_compose_desktop_entry:
 */
static void
test_compose_desktop_entry ()
{
	g_autoptr(GError) error = NULL;
	g_autoptr(AscResult) cres = NULL;
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsComponent) ecpt = NULL;
	g_autofree gchar *de_fname = NULL;
	g_autoptr(GBytes) de_bytes2 = NULL;
	gchar *contents;
	gsize contents_len;
	gchar *tmp;
	AsLaunchable *launch;
	GPtrArray *hints;
	g_autoptr(GBytes) de_bytes = as_gbytes_from_literal ("[Desktop Entry]\n"
							     "Type=Application\n"
							     "Name=FooBar\n"
							     "Name[de_DE]=FööBär\n"
							     "Comment=A foo-ish bar.\n"
							     "Keywords=Hobbes;Bentham;Locke;\n"
							     "Keywords[de_DE]=Heidegger;Kant;Hegel;\n");

	cres = asc_result_new ();

	/* test parsing standalone desktop-entry file */
	cpt = asc_parse_desktop_entry_data (cres,
					    NULL, /* cpt */
					    de_bytes,
					    "foobar.desktop",
					    FALSE, /* don't ignore nodisplay */
					    AS_FORMAT_VERSION_CURRENT,
					    NULL, NULL);
	g_assert_nonnull (cpt);
	g_clear_pointer (&cpt, g_object_unref);

	cpt = asc_result_get_component (cres, "foobar.desktop");
	g_assert_nonnull (cpt);
	cpt = g_object_ref (cpt);

	g_assert_cmpstr (as_component_get_name (cpt), ==, "FooBar");
	g_assert_cmpint (asc_result_hints_count (cres), ==, 0);
	g_clear_pointer (&cpt, g_object_unref);

	/* test component-id trimming */
	g_object_unref (cres);
	cres = asc_result_new ();
	cpt = asc_parse_desktop_entry_data (cres,
					    NULL, /* cpt */
					    de_bytes,
					    "org.example.foobar.desktop",
					    FALSE, /* don't ignore nodisplay */
					    AS_FORMAT_VERSION_CURRENT,
					    NULL, NULL);
	g_assert_nonnull (cpt);
	g_clear_pointer (&cpt, g_object_unref);

	cpt = asc_result_get_component (cres, "org.example.foobar");
	g_assert_nonnull (cpt);
	cpt = g_object_ref (cpt);
	g_clear_pointer (&cpt, g_object_unref);
	g_assert_cmpint (asc_result_hints_count (cres), ==, 0);

	/* test preexisting component */
	g_object_unref (cres);
	cres = asc_result_new ();

	ecpt = as_component_new ();
	as_component_set_kind (ecpt, AS_COMPONENT_KIND_DESKTOP_APP);
	as_component_set_id (ecpt, "org.example.foobar");
	as_component_set_name (ecpt, "TestX", "C");
	as_component_set_summary (ecpt, "Summary of TestX", "C");
	asc_result_add_component_with_string (cres, ecpt, "<testdata>", NULL);

	cpt = asc_parse_desktop_entry_data (cres,
					    ecpt,
					    de_bytes,
					    "org.example.foobar.desktop",
					    TRUE, /* ignore nodisplay */
					    AS_FORMAT_VERSION_CURRENT,
					    NULL, NULL);
	g_assert_nonnull (cpt);
	g_clear_pointer (&cpt, g_object_unref);

	cpt = asc_result_get_component (cres, "org.example.foobar");
	g_assert_nonnull (cpt);
	cpt = g_object_ref (cpt);
	g_assert_cmpint (asc_result_hints_count (cres), ==, 0);

	g_assert_cmpstr (as_component_get_name (cpt), ==, "TestX");
	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Summary of TestX");


	as_component_set_active_locale (cpt, "C.UTF-8");
	tmp = g_strjoinv (", ", as_component_get_keywords (cpt));
	g_assert_cmpstr (tmp, ==, "Hobbes, Bentham, Locke");
	g_free (tmp);

	/* test launchable */
	launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_nonnull (launch);

	g_assert_cmpint (as_launchable_get_entries (launch)->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (as_launchable_get_entries (launch), 0), ==, "org.example.foobar.desktop");
	g_clear_pointer (&cpt, g_object_unref);

	/* from file with damaged UTF-8 */
	de_fname = g_build_filename (datadir, "gnome-breakout_badUTF-8.desktop", NULL);
	g_file_get_contents (de_fname, &contents, &contents_len, &error);
	g_assert_no_error (error);
	de_bytes2 = g_bytes_new_take (contents, contents_len);

	g_object_unref (cres);
	cres = asc_result_new ();
	cpt = asc_parse_desktop_entry_data (cres,
					    NULL, /* cpt */
					    de_bytes2,
					    "gnome-breakout.desktop",
					    FALSE, /* don't ignore nodisplay */
					    AS_FORMAT_VERSION_CURRENT,
					    NULL, NULL);
	g_assert_nonnull (cpt);

	as_component_set_active_locale (cpt, "C.UTF-8");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "GNOME Breakout");
	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Play a clone of the classic arcade game Breakout for GNOME");
	as_component_set_active_locale (cpt, "de");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "GNOME Breakout");
	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Play a clone of the classic arcade game Breakout for GNOME"); /* not loaded, contains bad UTF-8 */
	as_component_set_active_locale (cpt, "tr");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Gnome Breakout");
	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Play a clone of the classic arcade game Breakout for GNOME");

	/* we should have two warnings about the bad UTF-8 */
	g_assert_cmpint (asc_result_hints_count (cres), ==, 2);
	hints = asc_result_get_hints (cres, "gnome-breakout.desktop");
	g_assert_cmpint (hints->len, ==, 2);
	for (guint i = 0; i < hints->len; i++) {
		AscHint *hint = ASC_HINT (g_ptr_array_index (hints, i));
		g_assert_cmpstr (asc_hint_get_tag (hint), ==, "desktop-entry-bad-data");
	}
	g_clear_pointer (&cpt, g_object_unref);
}

static void
setup (Fixture *fixture, gconstpointer user_data)
{
	fixture->path = g_strdup (g_getenv ("PATH"));
	/* not unset because glib has a hardcoded fallback */
	g_setenv ("PATH", "", TRUE);
}

static void
teardown (Fixture *fixture, gconstpointer user_data)
{
	g_setenv ("PATH", fixture->path, TRUE);
	g_clear_pointer (&fixture->path, g_free);
}

static void
test_compose_optipng_not_found (Fixture *fixture, gconstpointer user_data)
{
	g_test_expect_message (G_LOG_DOMAIN,
			       G_LOG_LEVEL_CRITICAL,
			       "*Refusing to enable optipng: not found in $PATH");
	asc_globals_set_use_optipng (TRUE);
	g_assert_false (asc_globals_get_use_optipng ());
	g_test_assert_expected_messages ();
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

	g_test_add ("/AppStream/Compose/OptipngNotfound", Fixture, NULL, setup, test_compose_optipng_not_found, teardown);
	g_test_add_func ("/AppStream/Compose/Utils", test_utils);
	g_test_add_func ("/AppStream/Compose/FontInfo", test_read_fontinfo);
	g_test_add_func ("/AppStream/Compose/Image", test_image_transform);
	g_test_add_func ("/AppStream/Compose/Canvas", test_canvas);
	g_test_add_func ("/AppStream/Compose/Hints", test_compose_hints);
	g_test_add_func ("/AppStream/Compose/Result", test_compose_result);
	g_test_add_func ("/AppStream/Compose/DesktopEntry", test_compose_desktop_entry);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
