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

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
