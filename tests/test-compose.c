/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2025 Matthias Klumpp <matthias@tenstral.net>
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
#include <locale.h>
#include <unistd.h>

#include "appstream-compose.h"
#include "asc-utils-metainfo.h"
#include "asc-utils-l10n.h"
#include "asc-utils-screenshots.h"
#include "asc-utils-fonts.h"
#include "asc-media-private.h"

#include "as-utils-private.h"
#include "as-test-utils.h"

static gchar *datadir = NULL;
static gchar *workdir = NULL;

typedef struct {
	gchar *path;
} Fixture;

/**
 * asx_assert_no_results_issue:
 */
static void
asx_assert_no_hints_in_result (AscResult *cres)
{
	g_autoptr(GPtrArray) hints = asc_result_fetch_hints_all (cres);

	if (hints->len > 0) {
		g_printerr ("--------\nHints:");
		for (guint i = 0; i < hints->len; i++) {
			g_autofree gchar *text = NULL;
			AscHint *hint = ASC_HINT (g_ptr_array_index (hints, i));
			text = asc_hint_format_explanation (hint);
			g_printerr ("\n%s\n", text);
		}
	}
	g_assert_cmpint (hints->len, ==, 0);
}

/**
 * asx_build_workdir_path:
 *
 * Build a path for @name within this test run's private working directory, so
 * concurrent runs never write to the same location.
 */
static gchar *
asx_build_workdir_path (const gchar *name)
{
	return g_build_filename (workdir, name, NULL);
}

/**
 * test_utils:
 *
 * Test global and utility functions.
 */
static void
test_utils (void)
{
	gchar *tmp;

	/* global ID */
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

	/* filename from URL */
	tmp = asc_filename_from_url ("https://example.com/file.txt");
	g_assert_cmpstr (tmp, ==, "file.txt");
	g_free (tmp);

	tmp = asc_filename_from_url ("https://example.com/file.txt?format=raw");
	g_assert_cmpstr (tmp, ==, "file.txt");
	g_free (tmp);

	tmp = asc_filename_from_url ("https://example.com//page.html#anchor");
	g_assert_cmpstr (tmp, ==, "page.html");
	g_free (tmp);

	tmp = asc_filename_from_url ("https://example.com/#");
	g_assert_cmpstr (tmp, ==, "example.com");
	g_free (tmp);

	tmp = asc_filename_from_url ("https://example.com/?/");
	g_assert_cmpstr (tmp, ==, "example.com");
	g_free (tmp);

	tmp = asc_filename_from_url ("https://example.com/sc%72eenshot.png");
	g_assert_cmpstr (tmp, ==, "screenshot.png");
	g_free (tmp);

	/* invalid escape sequences must not make us fail */
	tmp = asc_filename_from_url ("https://example.com/sc%zzeenshot.png");
	g_assert_cmpstr (tmp, ==, "sc%zzeenshot.png");
	g_free (tmp);

	tmp = asc_filename_from_url ("https://example.com/screenshot.png%");
	g_assert_cmpstr (tmp, ==, "screenshot.png%");
	g_free (tmp);

	tmp = asc_filename_from_url ("https://example.com/screen%00shot.png");
	g_assert_cmpstr (tmp, ==, "screen%00shot.png");
	g_free (tmp);

	tmp = asc_filename_from_url ("%zz");
	g_assert_cmpstr (tmp, ==, "%zz");
	g_free (tmp);

	tmp = asc_filename_from_url ("");
	g_assert_nonnull (tmp);
	g_assert_cmpint (strlen (tmp), ==, 4);
	g_free (tmp);

	g_assert_null (asc_filename_from_url (NULL));
}

/**
 * test_compose_issue_tag_sanity:
 */
static void
test_compose_issue_tag_sanity (void)
{
	g_autoptr(GHashTable) tag_map = NULL;
	g_auto(GStrv) all_hint_tags = NULL;

	tag_map = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	all_hint_tags = asc_globals_get_hint_tags ();
	for (guint i = 0; all_hint_tags[i] != NULL; i++) {
		gboolean r = g_hash_table_add (tag_map, all_hint_tags[i]);
		if (!r) {
			g_critical ("Duplicate compose issue-tag '%s' found in tag list.",
				    all_hint_tags[i]);
			g_assert_not_reached ();
		}
	}
}

/**
 * test_media_process_image:
 *
 * Process an image via the media worker process.
 */
static void
test_media_process_image (void)
{
	g_autoptr(AscMedia) media = asc_media_new ();
	g_autoptr(GError) error = NULL;
	g_autofree gchar *sample_img_fname = NULL;
	g_autofree gchar *data = NULL;
	g_autoptr(GBytes) img_bytes = NULL;
	g_autoptr(GPtrArray) targets = NULL;
	g_autofree gchar *out_dir = NULL;
	g_autofree gchar *out_fname_orig = NULL;
	g_autofree gchar *out_fname_scaled = NULL;
	AscImageTarget *target;
	gsize data_len;
	gint src_width = 0;
	gint src_height = 0;
	gboolean ret;

	sample_img_fname = g_build_filename (datadir, "appstream-logo.png", NULL);
	g_file_get_contents (sample_img_fname, &data, &data_len, &error);
	g_assert_no_error (error);
	img_bytes = g_bytes_new_take (g_steal_pointer (&data), data_len);

	out_dir = asx_build_workdir_path ("media-image-test");
	g_assert_cmpint (g_mkdir_with_parents (out_dir, 0755), ==, 0);

	/* read the image dimensions without storing any rendition */
	ret = asc_media_process_image (media,
				       img_bytes,
				       ASC_IMAGE_FORMAT_UNKNOWN,
				       0,
				       0,
				       ASC_IMAGE_LOAD_FLAG_NONE,
				       NULL, /* out dir */
				       NULL, /* targets */
				       &src_width,
				       &src_height,
				       &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_cmpint (src_width, ==, 136);
	g_assert_cmpint (src_height, ==, 144);

	/* store the image in its original size and a scaled-down rendition */
	targets = g_ptr_array_new_with_free_func ((GDestroyNotify) asc_image_target_free);
	g_ptr_array_add (targets,
			 asc_image_target_new ("orig.png", ASC_IMAGE_SCALE_MODE_NONE, 0, 0));
	g_ptr_array_add (targets,
			 asc_image_target_new ("64.png", ASC_IMAGE_SCALE_MODE_FIT_HEIGHT, 0, 64));
	g_ptr_array_add (
	    targets,
	    asc_image_target_new ("too-big.png", ASC_IMAGE_SCALE_MODE_FIT_WIDTH, 4000, 0));
	target = g_ptr_array_index (targets, 2);
	target->only_downscale = TRUE;

	src_width = 0;
	src_height = 0;
	ret = asc_media_process_image (media,
				       img_bytes,
				       ASC_IMAGE_FORMAT_UNKNOWN,
				       0,
				       0,
				       ASC_IMAGE_LOAD_FLAG_NONE,
				       out_dir,
				       targets,
				       &src_width,
				       &src_height,
				       &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_assert_cmpint (src_width, ==, 136);
	g_assert_cmpint (src_height, ==, 144);

	target = g_ptr_array_index (targets, 0);
	g_assert_false (target->skipped);
	g_assert_null (target->error_msg);
	g_assert_cmpint (target->result_width, ==, 136);
	g_assert_cmpint (target->result_height, ==, 144);

	target = g_ptr_array_index (targets, 1);
	g_assert_false (target->skipped);
	g_assert_null (target->error_msg);
	g_assert_cmpint (target->result_height, ==, 64);
	g_assert_cmpint (target->result_width, ==, 60);

	/* the upscaling rendition must have been skipped */
	target = g_ptr_array_index (targets, 2);
	g_assert_true (target->skipped);
	g_assert_null (target->error_msg);

	out_fname_orig = g_build_filename (out_dir, "orig.png", NULL);
	out_fname_scaled = g_build_filename (out_dir, "64.png", NULL);
	g_assert_true (g_file_test (out_fname_orig, G_FILE_TEST_EXISTS));
	g_assert_true (g_file_test (out_fname_scaled, G_FILE_TEST_EXISTS));
	{
		g_autofree gchar *check_fname = g_build_filename (out_dir, "too-big.png", NULL);
		g_assert_false (g_file_test (check_fname, G_FILE_TEST_EXISTS));
	}
}

/**
 * test_media_font:
 *
 * Read font metadata and render font media via the media worker process.
 */
static void
test_media_font (void)
{
	g_autoptr(AscMedia) media = asc_media_new ();
	g_autoptr(GError) error = NULL;
	g_autofree gchar *font_fname = NULL;
	g_autofree gchar *data = NULL;
	g_autoptr(GBytes) font_bytes = NULL;
	g_autoptr(AscFontInfo) finfo = NULL;
	g_autoptr(GPtrArray) targets = NULL;
	g_autofree gchar *out_dir = NULL;
	AscImageTarget *target;
	gsize data_len;
	gboolean ret;

	font_fname = g_build_filename (datadir, "Raleway-Regular.ttf", NULL);
	g_file_get_contents (font_fname, &data, &data_len, &error);
	g_assert_no_error (error);
	font_bytes = g_bytes_new_take (g_steal_pointer (&data), data_len);

	/* read font metadata */
	finfo = asc_media_read_font_info (media,
					  font_bytes,
					  "Raleway-Regular.ttf",
					  NULL, /* preferred language */
					  NULL, /* extra languages */
					  NULL, /* custom sample text */
					  NULL, /* custom icon text */
					  &error);
	g_assert_no_error (error);
	g_assert_nonnull (finfo);

	g_assert_cmpstr (finfo->family, ==, "Raleway");
	g_assert_cmpstr (finfo->style, ==, "Regular");
	g_assert_cmpstr (finfo->id, ==, "raleway-regular");
	g_assert_cmpstr (finfo->homepage, ==, "http://pixelspread.com");
	g_assert_nonnull (finfo->sample_text);
	g_assert_nonnull (finfo->languages);
	g_assert_true (g_strv_contains ((const gchar *const *) finfo->languages, "en"));

	out_dir = asx_build_workdir_path ("media-font-test");
	g_assert_cmpint (g_mkdir_with_parents (out_dir, 0755), ==, 0);

	/* render a font specimen card */
	targets = g_ptr_array_new_with_free_func ((GDestroyNotify) asc_image_target_free);
	g_ptr_array_add (targets,
			 asc_image_target_new ("card.png", ASC_IMAGE_SCALE_MODE_EXACT, 752, 423));
	ret = asc_media_render_font_card (media,
					  font_bytes,
					  "Raleway-Regular.ttf",
					  NULL, /* preferred language */
					  NULL, /* extra languages */
					  NULL, /* custom sample text */
					  NULL, /* custom icon text */
					  NULL, /* info label */
					  out_dir,
					  targets,
					  &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	target = g_ptr_array_index (targets, 0);
	g_assert_null (target->error_msg);
	g_assert_cmpint (target->result_width, ==, 752);
	g_assert_cmpint (target->result_height, >, 0);
	{
		g_autofree gchar *check_fname = g_build_filename (out_dir, "card.png", NULL);
		g_assert_true (g_file_test (check_fname, G_FILE_TEST_EXISTS));
	}

	/* render a font icon */
	g_ptr_array_set_size (targets, 0);
	g_ptr_array_add (targets,
			 asc_image_target_new ("icon.png", ASC_IMAGE_SCALE_MODE_EXACT, 64, 64));
	ret = asc_media_render_font_icon (media,
					  font_bytes,
					  "Raleway-Regular.ttf",
					  NULL, /* preferred language */
					  NULL, /* extra languages */
					  NULL, /* custom sample text */
					  NULL, /* custom icon text */
					  out_dir,
					  targets,
					  &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	target = g_ptr_array_index (targets, 0);
	g_assert_null (target->error_msg);
	g_assert_cmpint (target->result_width, ==, 64);
	g_assert_cmpint (target->result_height, ==, 64);
	{
		g_autofree gchar *check_fname = g_build_filename (out_dir, "icon.png", NULL);
		g_assert_true (g_file_test (check_fname, G_FILE_TEST_EXISTS));
	}
}

/**
 * test_media_worker_failure:
 *
 * Test that failures of the media worker process are handled gracefully.
 */
static void
test_media_worker_failure (void)
{
	g_autoptr(AscMedia) media = asc_media_new ();
	g_autoptr(GError) error = NULL;
	g_autoptr(GBytes) bad_bytes = NULL;
	gboolean ret;

	/* a worker that quits immediately must yield a proper error */
	asc_media_set_worker_path (media, "/bin/false");
	ret = asc_media_ensure_worker (media, &error);
	g_assert_error (error, ASC_MEDIA_ERROR, ASC_MEDIA_ERROR_DEAD_WORKER);
	g_assert_false (ret);
	g_clear_error (&error);

	/* the instance must recover once a functional worker is available again */
	asc_media_set_worker_path (media, NULL);
	ret = asc_media_ensure_worker (media, &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* a broken input must fail the operation, but not the worker */
	bad_bytes = as_gbytes_from_literal ("This is not a valid image.");
	ret = asc_media_process_image (media,
				       bad_bytes,
				       ASC_IMAGE_FORMAT_UNKNOWN,
				       0,
				       0,
				       ASC_IMAGE_LOAD_FLAG_NONE,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       &error);
	g_assert_false (ret);
	g_assert_nonnull (error);
	g_assert_false (g_error_matches (error, ASC_MEDIA_ERROR, ASC_MEDIA_ERROR_DEAD_WORKER));
	g_clear_error (&error);

	/* ... and the worker must still respond afterwards */
	ret = asc_media_ensure_worker (media, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
}

/**
 * test_compose_hints:
 *
 * Test compose hints and issue reporting.
 */
static void
test_compose_hints (void)
{
	g_autoptr(AscHint) hint = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *tmp = NULL;

	hint = asc_hint_new_for_tag ("internal-unknown-tag", &error);
	g_assert_no_error (error);
	g_assert_nonnull (hint);

	g_assert_cmpstr (asc_hint_get_tag (hint), ==, "internal-unknown-tag");
	g_assert_cmpint (asc_hint_get_severity (hint), ==, AS_ISSUE_SEVERITY_ERROR);
	g_assert_cmpstr (asc_hint_get_explanation_template (hint),
			 ==,
			 "The given tag was unknown. Please file an issue against AppStream.");
	g_assert_true (asc_hint_is_valid (hint));
	g_assert_true (asc_hint_is_error (hint));

	asc_hint_set_tag (hint, "dev-testsuite-test");
	asc_hint_set_severity (hint, AS_ISSUE_SEVERITY_INFO);
	g_assert_true (asc_hint_is_valid (hint));
	g_assert_true (!asc_hint_is_error (hint));

	asc_hint_set_explanation_template (
	    hint,
	    "This is an explanation for {{name}} which contains {{amount}} placeholders, "
	    "including one {odd} one and one left {{invalid}} intentionally.");
	asc_hint_add_explanation_var (hint, "name", "the compose testsuite");
	asc_hint_add_explanation_var (hint, "amount", "3");

	tmp = asc_hint_format_explanation (hint);
	g_assert_cmpstr (
	    tmp,
	    ==,
	    "This is an explanation for the compose testsuite which contains 3 placeholders, "
	    "including one {odd} one and one left {{invalid}} intentionally.");
}

/**
 * test_compose_result:
 *
 * Test the result object.
 */
static void
test_compose_result (void)
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
	g_assert_true (ret);

	ret = asc_result_add_hint (cres,
				   cpt,
				   "x-dev-testsuite-info",
				   "var1",
				   "testvalue-info",
				   NULL);
	g_assert_true (ret);

	g_assert_cmpint (asc_result_components_count (cres), ==, 1);
	g_assert_cmpint (asc_result_hints_count (cres), ==, 1);

#ifdef HAVE_BLAKE3
	g_assert_cmpstr (asc_result_gcid_for_component (cres, cpt),
			 ==,
			 "org/freedesktop/appstream.dummy/9dc221733838ad255d8a34978e062171");
#endif
	ret = asc_result_update_component_gcid_with_string (cres, cpt, "<moredata>");
	g_assert_true (ret);
#ifdef HAVE_BLAKE3
	g_assert_cmpstr (asc_result_gcid_for_component (cres, cpt),
			 ==,
			 "org/freedesktop/appstream.dummy/027ffd3526b3b38dd775f3fd045d40eb");
#endif

	g_assert_true (asc_result_get_component (cres, "org.freedesktop.appstream.dummy") == cpt);

	ret = asc_result_add_hint (cres,
				   cpt,
				   "x-dev-testsuite-error",
				   "var1",
				   "testvalue-error",
				   NULL);
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
test_compose_desktop_entry (void)
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
	g_autoptr(GBytes)
		      de_bytes = as_gbytes_from_literal ("[Desktop Entry]\n"
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
					    AS_FORMAT_VERSION_LATEST,
					    NULL,
					    NULL);
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
					    AS_FORMAT_VERSION_LATEST,
					    NULL,
					    NULL);
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
					    AS_FORMAT_VERSION_LATEST,
					    NULL,
					    NULL);
	g_assert_nonnull (cpt);
	g_clear_pointer (&cpt, g_object_unref);

	cpt = asc_result_get_component (cres, "org.example.foobar");
	g_assert_nonnull (cpt);
	cpt = g_object_ref (cpt);
	g_assert_cmpint (asc_result_hints_count (cres), ==, 0);

	g_assert_cmpstr (as_component_get_name (cpt), ==, "TestX");
	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Summary of TestX");

	as_component_set_context_locale (cpt, "C.UTF-8");
	tmp = as_ptr_array_strjoin (as_component_get_keywords (cpt), ", ");
	g_assert_cmpstr (tmp, ==, "Hobbes, Bentham, Locke");
	g_free (tmp);

	/* test launchable */
	launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_nonnull (launch);

	g_assert_cmpint (as_launchable_get_entries (launch)->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (as_launchable_get_entries (launch), 0),
			 ==,
			 "org.example.foobar.desktop");
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
					    AS_FORMAT_VERSION_LATEST,
					    NULL,
					    NULL);
	g_assert_nonnull (cpt);

	as_component_set_context_locale (cpt, "C.UTF-8");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "GNOME Breakout");
	g_assert_cmpstr (as_component_get_summary (cpt),
			 ==,
			 "Play a clone of the classic arcade game Breakout for GNOME");
	as_component_set_context_locale (cpt, "de");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "GNOME Breakout");
	g_assert_cmpstr (
	    as_component_get_summary (cpt),
	    ==,
	    "Play a clone of the classic arcade game Breakout for GNOME"); /* not loaded, contains bad UTF-8 */
	as_component_set_context_locale (cpt, "tr");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Gnome Breakout");
	g_assert_cmpstr (as_component_get_summary (cpt),
			 ==,
			 "Play a clone of the classic arcade game Breakout for GNOME");

	/* we should have two warnings about the bad UTF-8 */
	g_assert_cmpint (asc_result_hints_count (cres), ==, 2);
	hints = asc_result_get_hints (cres, "gnome-breakout.desktop");
	g_assert_cmpint (hints->len, ==, 2);
	for (guint i = 0; i < hints->len; i++) {
		AscHint *hint = ASC_HINT (g_ptr_array_index (hints, i));
		g_assert_cmpstr (asc_hint_get_tag (hint), ==, "asv-desktop-entry-bad-data");
	}
	g_clear_pointer (&cpt, g_object_unref);
}

static void
setup (Fixture *fixture, gconstpointer user_data)
{
	fixture->path = g_strdup (g_getenv ("PATH"));
	/* not unset because glib has a hardcoded fallback */
	g_setenv ("PATH", "", TRUE);
	asc_globals_clear ();
}

static void
teardown (Fixture *fixture, gconstpointer user_data)
{
	g_setenv ("PATH", fixture->path, TRUE);
	g_clear_pointer (&fixture->path, g_free);
	asc_globals_clear ();
}

/**
 * test_compose_optipng_not_found:
 */
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

/**
 * asx_test_make_file:
 *
 * Create a file named @name with @contents in @dir.
 */
static void
asx_test_make_file (const gchar *dir, const gchar *name, const gchar *contents)
{
	g_autoptr(GError) error = NULL;
	g_autofree gchar *fname = g_build_filename (dir, name, NULL);
	gboolean ret;

	ret = g_file_set_contents (fname, contents, -1, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
}

/**
 * asx_test_make_link:
 *
 * Create a symlink named @name in @dir which points at @target.
 */
static void
asx_test_make_link (const gchar *dir, const gchar *name, const gchar *target)
{
	g_autofree gchar *fname = g_build_filename (dir, name, NULL);
	g_assert_cmpint (symlink (target, fname), ==, 0);
}

/**
 * asx_assert_unit_reads:
 *
 * Assert that @path resolves within @unit and yields exactly @contents.
 */
static void
asx_assert_unit_reads (AscUnit *unit, const gchar *path, const gchar *contents)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GBytes) data = NULL;

	g_assert_true (asc_unit_file_exists (unit, path));
	data = asc_unit_read_data (unit, path, &error);
	g_assert_no_error (error);
	g_assert_nonnull (data);

	g_assert_cmpint (g_bytes_get_size (data), ==, strlen (contents));
	/* an empty file has nothing mapped, so there is no data to compare */
	if (contents[0] != '\0')
		g_assert_cmpmem (g_bytes_get_data (data, NULL),
				 g_bytes_get_size (data),
				 contents,
				 strlen (contents));
}

/**
 * asx_assert_unit_rejects:
 *
 * Assert that @path does not resolve within @unit, failing with @code.
 */
static void
asx_assert_unit_rejects (AscUnit *unit, const gchar *path, gint code)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GBytes) data = NULL;

	g_assert_false (asc_unit_file_exists (unit, path));
	data = asc_unit_read_data (unit, path, &error);
	g_assert_error (error, G_FILE_ERROR, code);
	g_assert_null (data);
}

/**
 * test_compose_directory_unit:
 */
static void
test_compose_directory_unit (void)
{
	g_autoptr(GError) error = NULL;
	gboolean ret;
	GPtrArray *contents;
	g_autoptr(AscDirectoryUnit) dirunit = asc_directory_unit_new (datadir);

	ret = asc_unit_open (ASC_UNIT (dirunit), &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* NOTE: Links that we can not resolve within the unit are still listed, so an
	 * error can be reported to the user instead of silently dropping the data. Both
	 * "/appstream-logo.png" (which points out of the sample tree) and the dangling
	 * "badlink" metainfo file are of that kind. */
	contents = asc_unit_get_contents (ASC_UNIT (dirunit));
	g_assert_cmpint (contents->len, ==, 17);
	as_sort_strings (contents);

	g_assert_cmpstr (g_ptr_array_index (contents, 0), ==, "/Raleway-Regular.ttf");
	g_assert_cmpstr (g_ptr_array_index (contents, 5), ==, "/sample-video.mkv");
	g_assert_cmpstr (g_ptr_array_index (contents, 16),
			 ==,
			 "/usr/share/metainfo/org.example.nonexistent-badlink.metainfo.xml");

	/* a symlink using a relative path that stays inside the unit resolves normally */
	g_assert_true (
	    asc_unit_file_exists (ASC_UNIT (dirunit),
				  "/usr/share/fonts/truetype/raleway/Raleway-Regular.ttf"));

	/* read existent data */
	asx_assert_unit_reads (ASC_UNIT (dirunit), "/usr/dummy", "Hello Universe!\n");

	/* read nonexistent data */
	asx_assert_unit_rejects (ASC_UNIT (dirunit), "/nonexistent", G_FILE_ERROR_NOENT);
}

/**
 * test_compose_directory_unit_escape:
 *
 * Data referenced from metainfo files is untrusted, so a directory unit must never
 * hand out files from outside of its root directory.
 * Paths are resolved as if the unit's root was the root of the filesystem.
 */
static void
test_compose_directory_unit_escape (void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(AscDirectoryUnit) dirunit = NULL;
	g_autoptr(AscDirectoryUnit) linked_dirunit = NULL;
	g_autoptr(AscDirectoryUnit) partial_dirunit = NULL;
	g_autoptr(AscDirectoryUnit) escape_dirunit = NULL;
	GPtrArray *contents;
	g_autofree gchar *contents_str = NULL;
	g_autofree gchar *tmp_root = NULL;
	g_autofree gchar *unit_root = NULL;
	g_autofree gchar *unit_subdir = NULL;
	g_autofree gchar *unit_datadir = NULL;
	g_autofree gchar *secret_fname = NULL;
	g_autofree gchar *sibling_dir = NULL;
	g_autofree gchar *root_link = NULL;
	gboolean ret;

	/* create a "secret" file right next to, but outside of, the unit root, as well
	 * as a sibling directory of the root whose name has the root's name as prefix */
	tmp_root = asx_build_workdir_path ("escape");
	g_assert_cmpint (g_mkdir_with_parents (tmp_root, 0755), ==, 0);
	asx_test_make_file (tmp_root, "secret.txt", "TOP SECRET");
	secret_fname = g_build_filename (tmp_root, "secret.txt", NULL);

	sibling_dir = g_build_filename (tmp_root, "rootfoo", NULL);
	g_assert_cmpint (g_mkdir_with_parents (sibling_dir, 0755), ==, 0);
	asx_test_make_file (sibling_dir, "secret.txt", "TOP SECRET");

	unit_root = g_build_filename (tmp_root, "root", NULL);
	unit_subdir = g_build_filename (unit_root, "usr", NULL);
	g_assert_cmpint (g_mkdir_with_parents (unit_subdir, 0755), ==, 0);
	asx_test_make_file (unit_subdir, "data.txt", "Hello!");

	/* an empty file still has to be readable, even though there is nothing to map */
	asx_test_make_file (unit_subdir, "empty.txt", "");

	/* place a symlink inside the unit that points at the file outside of it, one
	 * that stays within the unit, one that is absolute and therefore only makes
	 * sense when read as a path within the unit, one that climbs out of the unit
	 * using relative segments, and one that loops */
	asx_test_make_link (unit_subdir, "escape-link", secret_fname);
	asx_test_make_link (unit_subdir, "inner-link", "data.txt");
	asx_test_make_link (unit_subdir, "abs-link", "/usr/data.txt");
	asx_test_make_link (unit_subdir, "updown-link", "../../secret.txt");
	asx_test_make_link (unit_subdir, "loop-link", "loop-link");

	dirunit = asc_directory_unit_new (unit_root);
	ret = asc_unit_open (ASC_UNIT (dirunit), &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* links that do not resolve within the unit are still listed, so that the
	 * failure to read them can be reported as an issue later on */
	contents = asc_unit_get_contents (ASC_UNIT (dirunit));
	as_sort_strings (contents);
	contents_str = as_ptr_array_strjoin (contents, " ");
	g_assert_cmpstr (contents_str,
			 ==,
			 "/usr/abs-link /usr/data.txt /usr/empty.txt /usr/escape-link "
			 "/usr/inner-link /usr/loop-link /usr/updown-link");

	/* a symlink that stays within the unit must still resolve, an absolute link
	 * target is resolved from the unit's root rather than from the root of the
	 * filesystem that we happen to run on, and an empty file is simply empty */
	asx_assert_unit_reads (ASC_UNIT (dirunit), "/usr/inner-link", "Hello!");
	asx_assert_unit_reads (ASC_UNIT (dirunit), "/usr/abs-link", "Hello!");
	asx_assert_unit_reads (ASC_UNIT (dirunit), "/usr/empty.txt", "");

	/* escaping the unit must not be possible, neither via relative path segments nor
	 * via a symlink. A relative link that climbs out of the unit is pinned at its
	 * root, so it resolves to a location that does not exist rather than to the file
	 * outside, and the sibling directory of the root can not be reached by climbing
	 * out of it either ("../rootfoo" vs. "../root"). A link that never resolves must
	 * not send us in circles. */
	asx_assert_unit_rejects (ASC_UNIT (dirunit), "/usr/../../secret.txt", G_FILE_ERROR_NOENT);
	asx_assert_unit_rejects (ASC_UNIT (dirunit), "/usr/escape-link", G_FILE_ERROR_NOENT);
	asx_assert_unit_rejects (ASC_UNIT (dirunit), "/usr/updown-link", G_FILE_ERROR_NOENT);
	asx_assert_unit_rejects (ASC_UNIT (dirunit), "/../rootfoo", G_FILE_ERROR_NOENT);
	asx_assert_unit_rejects (ASC_UNIT (dirunit), "/usr/loop-link", G_FILE_ERROR_LOOP);

	/* a root directory that is reached through a symlink itself must not make
	 * everything below it appear to be outside of the unit */
	root_link = g_build_filename (tmp_root, "root-link", NULL);
	g_assert_cmpint (symlink (unit_root, root_link), ==, 0);

	linked_dirunit = asc_directory_unit_new (root_link);
	ret = asc_unit_open (ASC_UNIT (linked_dirunit), &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	asx_assert_unit_reads (ASC_UNIT (linked_dirunit), "/usr/abs-link", "Hello!");
	asx_assert_unit_rejects (ASC_UNIT (linked_dirunit), "/usr/escape-link", G_FILE_ERROR_NOENT);

	/* a unit that indexes selected locations only has to resolve those within the
	 * unit as well: one of them is a symlink to another location in the unit, the
	 * other one leaves the unit entirely. */
	unit_datadir = g_build_filename (unit_root, "as-compose-test-data", NULL);
	g_assert_cmpint (g_mkdir_with_parents (unit_datadir, 0755), ==, 0);
	asx_test_make_file (unit_datadir, "astest-thing.txt", "Hi!");

	asx_test_make_link (unit_root, "share", "/as-compose-test-data");
	asx_test_make_link (unit_root, "escape-dir", sibling_dir);

	partial_dirunit = asc_directory_unit_new (unit_root);
	asc_unit_add_relevant_path (ASC_UNIT (partial_dirunit), "/share");
	ret = asc_unit_open (ASC_UNIT (partial_dirunit), &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* the data is listed at the location it is expected at, not at the one it is
	 * physically stored at */
	contents = asc_unit_get_contents (ASC_UNIT (partial_dirunit));
	g_assert_cmpint (contents->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (contents, 0), ==, "/share/astest-thing.txt");

	/* a directory reached through an absolute in-unit link is a directory, while one
	 * that leaves the unit does not exist as far as the unit is concerned */
	g_assert_true (asc_unit_dir_exists (ASC_UNIT (partial_dirunit), "/share"));
	g_assert_false (asc_unit_dir_exists (ASC_UNIT (partial_dirunit), "/escape-dir"));

	asx_assert_unit_reads (ASC_UNIT (partial_dirunit), "/share/astest-thing.txt", "Hi!");

	/* a location that leaves the unit must not be walked at all */
	escape_dirunit = asc_directory_unit_new (unit_root);
	asc_unit_add_relevant_path (ASC_UNIT (escape_dirunit), "/escape-dir");
	ret = asc_unit_open (ASC_UNIT (escape_dirunit), &error);
	g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
	g_assert_false (ret);
	g_clear_error (&error);
	g_assert_cmpint (asc_unit_get_contents (ASC_UNIT (escape_dirunit))->len, ==, 0);
}

/**
 * test_compose_locale_stats:
 */
static void
test_compose_locale_stats (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(AscResult) cres = NULL;
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsTranslation) tr = NULL;
	g_autoptr(AscDirectoryUnit) dirunit = asc_directory_unit_new (datadir);

	/* open sample data directory unit */
	ret = asc_unit_open (ASC_UNIT (dirunit), &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* create dummy result with a dummy component */
	cpt = as_component_new ();
	as_component_set_id (cpt, "org.freedesktop.appstream.dummy");

	tr = as_translation_new ();
	as_translation_set_kind (tr, AS_TRANSLATION_KIND_GETTEXT);
	as_translation_set_id (tr, "app");
	as_component_add_translation (cpt, tr);

	cres = asc_result_new ();
	ret = asc_result_add_component_with_string (cres, cpt, "<testdata>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* try loading a Gettext translation */
	asc_read_translation_status (cres, ASC_UNIT (dirunit), "/usr", 25);
	asx_assert_no_hints_in_result (cres);
	g_assert_cmpint (as_component_get_language (cpt, "en_GB"), ==, 100);
	g_assert_cmpint (as_component_get_language (cpt, "ru"), ==, 33);

	/* the source locale should be 100% translated */
	g_assert_cmpint (as_component_get_language (cpt, "en_US"), ==, 100);

	/* try loading Qt translations, style 1 */
	as_component_clear_languages (cpt);
	as_translation_set_kind (tr, AS_TRANSLATION_KIND_QT);
	as_translation_set_id (tr, "kdeapp1/translations/kdeapp");
	as_component_add_translation (cpt, tr);

	asc_read_translation_status (cres, ASC_UNIT (dirunit), "/usr", 25);
	asx_assert_no_hints_in_result (cres);
	g_assert_cmpint (as_component_get_language (cpt, "fr"), ==, 100);
	g_assert_cmpint (as_component_get_language (cpt, "de"), ==, -1);

	/* the source locale should be 100% translated */
	g_assert_cmpint (as_component_get_language (cpt, "en_US"), ==, 100);

	/* try loading Qt translations, style 2 */
	as_component_clear_languages (cpt);
	as_translation_set_kind (tr, AS_TRANSLATION_KIND_QT);
	as_translation_set_id (tr, "kdeapp2/translations/kdeapp");
	as_component_add_translation (cpt, tr);

	asc_read_translation_status (cres, ASC_UNIT (dirunit), "/usr", 25);
	asx_assert_no_hints_in_result (cres);
	g_assert_cmpint (as_component_get_language (cpt, "fr"), ==, 100);
	g_assert_cmpint (as_component_get_language (cpt, "de"), ==, -1);

	/* try loading Qt translations, style 3 */
	as_component_clear_languages (cpt);
	as_translation_set_kind (tr, AS_TRANSLATION_KIND_QT);
	as_translation_set_id (tr, "kdeapp3");
	as_component_add_translation (cpt, tr);

	asc_read_translation_status (cres, ASC_UNIT (dirunit), "/usr", 25);
	asx_assert_no_hints_in_result (cres);
	g_assert_cmpint (as_component_get_language (cpt, "fr"), ==, 100);
	g_assert_cmpint (as_component_get_language (cpt, "de"), ==, 100);
}

static void
test_compose_source_locale (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(AscResult) cres = NULL;
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsTranslation) tr = NULL;
	g_autoptr(AscDirectoryUnit) dirunit = asc_directory_unit_new (datadir);

	/* open sample data directory unit */
	ret = asc_unit_open (ASC_UNIT (dirunit), &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* create dummy result with a dummy component, and set a non-standard
	 * source locale on the translation */
	cpt = as_component_new ();
	as_component_set_id (cpt, "org.freedesktop.appstream.dummy");

	tr = as_translation_new ();
	as_translation_set_kind (tr, AS_TRANSLATION_KIND_GETTEXT);
	as_translation_set_id (tr, "app");
	as_translation_set_source_locale (tr, "de");
	as_component_add_translation (cpt, tr);

	cres = asc_result_new ();
	ret = asc_result_add_component_with_string (cres, cpt, "<testdata>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* try loading a Gettext translation */
	asc_read_translation_status (cres, ASC_UNIT (dirunit), "/usr", 25);
	asx_assert_no_hints_in_result (cres);
	g_assert_cmpint (as_component_get_language (cpt, "en_GB"), ==, 100);
	g_assert_cmpint (as_component_get_language (cpt, "ru"), ==, 33);

	/* the source locale should be 100% translated */
	g_assert_cmpint (as_component_get_language (cpt, "de"), ==, 100);

	/* and the default source locale should not be translated */
	g_assert_cmpint (as_component_get_language (cpt, "en_US"), ==, -1);
}

static void
test_compose_video_info (void)
{
	g_autoptr(AscResult) cres = NULL;
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AscMedia) media = asc_media_new ();
	g_autoptr(GError) error = NULL;
	gboolean ret = FALSE;
	g_autofree gchar *vid_fname = NULL;
	AscVideoInfo *vinfo = NULL;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.freedesktop.appstream.dummy");

	cres = asc_result_new ();
	ret = asc_result_add_component_with_string (cres, cpt, "<testdata>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	if (asc_globals_get_ffprobe_binary () == NULL) {
		g_print ("WARNING: Skipping video info test because `ffprobe` binary was not found "
			 "in PATH!\n");
		return;
	}

	vid_fname = g_build_filename (datadir, "sample-video.mkv", NULL);
	vinfo = asc_extract_video_info (cres, cpt, media, vid_fname);
	g_assert_nonnull (vinfo);

	g_assert_cmpstr (vinfo->codec_name, ==, "av1");
	g_assert_cmpstr (vinfo->audio_codec_name, ==, NULL);

	g_assert_cmpint (vinfo->width, ==, 640);
	g_assert_cmpint (vinfo->height, ==, 480);

	g_assert_cmpstr (vinfo->format_name, ==, "matroska,webm");

	g_assert_cmpint (vinfo->container_kind, ==, AS_VIDEO_CONTAINER_KIND_MKV);
	g_assert_cmpint (vinfo->codec_kind, ==, AS_VIDEO_CODEC_KIND_AV1);
	g_assert_true (vinfo->is_acceptable);

	asc_video_info_free (vinfo);
}

static void
test_compose_font (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(AscResult) cres = NULL;
	g_autoptr(AsMetadata) mdata = NULL;
	g_autoptr(AscIconPolicy) icon_policy = NULL;
	g_autoptr(AscMedia) media = asc_media_new ();
	g_autoptr(AscDirectoryUnit) dirunit = asc_directory_unit_new (datadir);
	g_autofree gchar *export_tmpdir = asx_build_workdir_path ("font-export");

	/* cleanup */
	if (g_file_test (export_tmpdir, G_FILE_TEST_EXISTS)) {
		ret = as_utils_delete_dir_recursive (export_tmpdir);
		g_assert_true (ret);
	}

	/* open sample data directory unit */
	asc_unit_set_bundle_id (ASC_UNIT (dirunit), "dummy");
	ret = asc_unit_open (ASC_UNIT (dirunit), &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* load dummy font component and register it */
	mdata = as_metadata_new ();
	as_metadata_set_locale (mdata, "C");
	as_metadata_set_format_style (mdata, AS_FORMAT_STYLE_METAINFO);
	{
		g_autoptr(GFile) file = NULL;
		g_autofree gchar *fname = g_build_filename (datadir,
							    "usr",
							    "share",
							    "metainfo",
							    "org.example.fonttest.metainfo.xml",
							    NULL);
		file = g_file_new_for_path (fname);
		ret = as_metadata_parse_file (mdata, file, AS_FORMAT_KIND_XML, &error);
		g_assert_no_error (error);
		g_assert_true (ret);
	}

	cres = asc_result_new ();
	ret = asc_result_add_component_with_string (cres,
						    as_metadata_get_component (mdata),
						    "<testdata_font/>",
						    &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	icon_policy = asc_icon_policy_new ();
	asc_process_fonts (cres,
			   ASC_UNIT (dirunit),
			   media,
			   "/usr",
			   export_tmpdir,
			   NULL, /* no icon export dir */
			   icon_policy,
			   ASC_COMPOSE_FLAG_STORE_SCREENSHOTS | ASC_COMPOSE_FLAG_PROCESS_FONTS);
	asx_assert_no_hints_in_result (cres);
}

static void
test_compose_icon_policy_serialize (void)
{
	g_autoptr(AscIconPolicy) ipolicy = NULL;
	g_autofree gchar *tmp = NULL;
	gboolean ret;
	g_autoptr(GError) error = NULL;

	ipolicy = asc_icon_policy_new ();
	tmp = asc_icon_policy_to_string (ipolicy);
	g_assert_cmpstr (tmp,
			 ==,
			 "48x48=cached,48x48@2=cached,64x64=cached,64x64@2=cached,"
			 "128x128=cached-remote,128x128@2=cached-remote");
	g_free (g_steal_pointer (&tmp));

	ret = asc_icon_policy_from_string (
	    ipolicy,
	    "48x48@2=ignored,64x64=cached,64x64@2=cached-remote,128x128=remote,128x128@2=remote",
	    &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_clear_error (&error);

	tmp = asc_icon_policy_to_string (ipolicy);
	g_assert_cmpstr (
	    tmp,
	    ==,
	    "48x48@2=ignored,64x64=cached,64x64@2=cached-remote,128x128=remote,128x128@2=remote");
	g_free (g_steal_pointer (&tmp));

	ret = asc_icon_policy_from_string (ipolicy, "48x48-2:ignored,64x64:cached", &error);
	g_assert_error (error, AS_UTILS_ERROR, AS_UTILS_ERROR_FAILED);
	g_assert_false (ret);
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

	/* location for temporary test data */
	workdir = g_dir_make_tmp ("as-compose-test_XXXXXX", NULL);
	g_assert_nonnull (workdir);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add ("/AppStream/Compose/OptipngNotfound",
		    Fixture,
		    NULL,
		    setup,
		    test_compose_optipng_not_found,
		    teardown);
	g_test_add_func ("/AppStream/Compose/Utils", test_utils);
	g_test_add_func ("/AppStream/Compose/IssueTagSanity", test_compose_issue_tag_sanity);
	g_test_add_func ("/AppStream/Compose/MediaImage", test_media_process_image);
	g_test_add_func ("/AppStream/Compose/MediaFont", test_media_font);
	g_test_add_func ("/AppStream/Compose/MediaWorkerFailure", test_media_worker_failure);
	g_test_add_func ("/AppStream/Compose/Hints", test_compose_hints);
	g_test_add_func ("/AppStream/Compose/Result", test_compose_result);
	g_test_add_func ("/AppStream/Compose/DesktopEntry", test_compose_desktop_entry);
	g_test_add_func ("/AppStream/Compose/DirectoryUnit", test_compose_directory_unit);
	g_test_add_func ("/AppStream/Compose/DirectoryUnitEscape",
			 test_compose_directory_unit_escape);
	g_test_add_func ("/AppStream/Compose/LocaleStats", test_compose_locale_stats);
	g_test_add_func ("/AppStream/Compose/SourceLocale", test_compose_source_locale);
	g_test_add_func ("/AppStream/Compose/VideoInfo", test_compose_video_info);
	g_test_add_func ("/AppStream/Compose/Font", test_compose_font);
	g_test_add_func ("/AppStream/Compose/IconPolicySerialize",
			 test_compose_icon_policy_serialize);

	ret = g_test_run ();
	as_utils_delete_dir_recursive (workdir);
	g_free (workdir);
	g_free (datadir);

	/* make sanitizers happy */
	asc_globals_clear ();

	return ret;
}
