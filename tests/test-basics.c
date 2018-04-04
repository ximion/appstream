/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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
#include "appstream.h"
#include "as-component-private.h"

#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * test_categories:
 *
 * Test #AsCategory properties.
 */
static void
test_categories ()
{
	g_autoptr(GPtrArray) default_cats;

	default_cats = as_get_default_categories (TRUE);
	g_assert_cmpint (default_cats->len, ==, 10);
}

/**
 * test_simplemarkup:
 *
 * Test as_description_markup_convert_simple()
 */
static void
test_simplemarkup ()
{
	g_autofree gchar *str = NULL;
	GError *error = NULL;

	str = as_markup_convert_simple ("<p>Test!</p><p>Blah.</p><ul><li>A</li><li>B</li></ul><p>End.</p>", &error);
	g_assert_no_error (error);

	g_assert (g_strcmp0 (str, "Test!\n\nBlah.\n • A\n • B\n\nEnd.") == 0);
}

/**
 * _get_dummy_strv:
 */
static gchar**
_get_dummy_strv (const gchar *value)
{
	gchar **strv;

	strv = g_new0 (gchar*, 1 + 2);
	strv[0] = g_strdup (value);
	strv[1] = NULL;

	return strv;
}

/**
 * test_component:
 *
 * Test basic properties of an #AsComponent.
 */
static void
test_component ()
{
	AsComponent *cpt;
	AsMetadata *metad;
	gchar *str;
	gchar *str2;
	gchar **strv;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);

	as_component_set_id (cpt, "org.example.test.desktop");
	as_component_set_name (cpt, "Test", NULL);
	as_component_set_summary (cpt, "It does things", NULL);

	strv = _get_dummy_strv ("fedex");
	as_component_set_pkgnames (cpt, strv);
	g_strfreev (strv);

	metad = as_metadata_new ();
	as_metadata_add_component (metad, cpt);
	str = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, NULL);
	str2 = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, NULL);
	g_object_unref (metad);
	g_debug ("%s", str2);

	g_assert_cmpstr (str, ==, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				  "<component type=\"desktop-application\">\n"
				  "  <id>org.example.test.desktop</id>\n"
				  "  <name>Test</name>\n"
				  "  <summary>It does things</summary>\n"
				  "  <pkgname>fedex</pkgname>\n"
				  "</component>\n");
	g_assert_cmpstr (str2, ==, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				   "<components version=\"0.12\">\n"
				   "  <component type=\"desktop-application\">\n"
				   "    <id>org.example.test.desktop</id>\n"
				   "    <name>Test</name>\n"
				   "    <summary>It does things</summary>\n"
				   "    <pkgname>fedex</pkgname>\n"
				   "  </component>\n"
				   "</components>\n");

	g_free (str);
	g_free (str2);
}

/**
 * test_translation_fallback:
 *
 * Test that the AS_VALUE_FLAGS_NO_TRANSLATION_FALLBACK flag works.
 */
static void
test_translation_fallback (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsValueFlags flags;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);
	as_component_set_id (cpt, "org.example.ATargetComponent");
	as_component_set_description (cpt, "<p>It's broken!</p>", "C");
	flags = as_component_get_value_flags (cpt);

	/* there is no de translation */
	as_component_set_active_locale (cpt, "de");
	g_assert_nonnull (as_component_get_description (cpt));

	/* if the flag is set, we don't fall back to C */
	as_flags_add (flags, AS_VALUE_FLAG_NO_TRANSLATION_FALLBACK);
	as_component_set_value_flags (cpt, flags);
	g_assert_null (as_component_get_description (cpt));

	/* ...but after removing it, again we do */
	as_flags_remove (flags, AS_VALUE_FLAG_NO_TRANSLATION_FALLBACK);
	as_component_set_value_flags (cpt, flags);
	g_assert_nonnull (as_component_get_description (cpt));
}

/**
 * test_spdx:
 *
 * Test SPDX license description parsing.
 */
static void
test_spdx (void)
{
	gchar **tok;
	gchar *tmp;

	/* simple */
	tok = as_spdx_license_tokenize ("LGPL-2.0+");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+");
	g_strfreev (tok);
	g_free (tmp);

	/* empty */
	tok = as_spdx_license_tokenize ("");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "");
	g_strfreev (tok);
	g_free (tmp);

	/* invalid */
	tok = as_spdx_license_tokenize (NULL);
	g_assert (tok == NULL);

	/* random */
	tok = as_spdx_license_tokenize ("Public Domain");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "Public Domain");
	g_strfreev (tok);
	g_free (tmp);

	/* multiple licences */
	tok = as_spdx_license_tokenize ("LGPL-2.0+ AND GPL-2.0 AND LGPL-3.0");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+  &  @GPL-2.0  &  @LGPL-3.0");
	g_strfreev (tok);
	g_free (tmp);

	/* multiple licences, using the new style */
	tok = as_spdx_license_tokenize ("LGPL-2.0-or-later AND GPL-2.0-only");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+  &  @GPL-2.0");
	g_strfreev (tok);
	g_free (tmp);

	/* multiple licences, deprectated 'and' & 'or' */
	tok = as_spdx_license_tokenize ("LGPL-2.0+ and GPL-2.0 or LGPL-3.0");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+  &  @GPL-2.0  |  @LGPL-3.0");
	g_strfreev (tok);
	g_free (tmp);

	/* brackets */
	tok = as_spdx_license_tokenize ("LGPL-2.0+ and (GPL-2.0 or GPL-2.0+) and MIT");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+  &  (  @GPL-2.0  |  @GPL-2.0+  )  &  @MIT");
	g_strfreev (tok);
	g_free (tmp);

	/* detokenisation */
	tok = as_spdx_license_tokenize ("LGPLv2+ and (QPL or GPLv2) and MIT");
	tmp = as_spdx_license_detokenize (tok);
	g_assert_cmpstr (tmp, ==, "LGPLv2+ AND (QPL OR GPLv2) AND MIT");
	g_strfreev (tok);
	g_free (tmp);

	/* "+" operator */
	tok = as_spdx_license_tokenize ("CC-BY-SA-3.0+ AND Zlib");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@CC-BY-SA-3.0  +  &  @Zlib");
	g_free (tmp);
	tmp = as_spdx_license_detokenize (tok);
	g_assert_cmpstr (tmp, ==, "CC-BY-SA-3.0+ AND Zlib");
	g_strfreev (tok);
	g_free (tmp);

	/* detokenisation literals */
	tok = as_spdx_license_tokenize ("Public Domain");
	tmp = as_spdx_license_detokenize (tok);
	g_assert_cmpstr (tmp, ==, "Public Domain");
	g_strfreev (tok);
	g_free (tmp);

	/* invalid tokens */
	tmp = as_spdx_license_detokenize (NULL);
	g_assert (tmp == NULL);

	/* leading brackets */
	tok = as_spdx_license_tokenize ("(MPLv1.1 or LGPLv3+) and LGPLv3");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "(  MPLv1.1  |  LGPLv3+  )  &  LGPLv3");
	g_strfreev (tok);
	g_free (tmp);

	/*  trailing brackets */
	tok = as_spdx_license_tokenize ("MPLv1.1 and (LGPLv3 or GPLv3)");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "MPLv1.1  &  (  LGPLv3  |  GPLv3  )");
	g_strfreev (tok);
	g_free (tmp);

	/*  deprecated names */
	tok = as_spdx_license_tokenize ("CC0 and (CC0 or CC0)");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@CC0-1.0  &  (  @CC0-1.0  |  @CC0-1.0  )");
	g_strfreev (tok);
	g_free (tmp);

	/* SPDX strings */
	g_assert (as_is_spdx_license_expression ("CC0-1.0"));
	g_assert (as_is_spdx_license_expression ("CC0"));
	g_assert (as_is_spdx_license_expression ("LicenseRef-proprietary"));
	g_assert (as_is_spdx_license_expression ("CC0-1.0 and GFDL-1.3"));
	g_assert (as_is_spdx_license_expression ("CC0-1.0 AND GFDL-1.3"));
	g_assert (as_is_spdx_license_expression ("CC-BY-SA-3.0+"));
	g_assert (as_is_spdx_license_expression ("CC-BY-SA-3.0+ AND Zlib"));
	g_assert (as_is_spdx_license_expression ("NOASSERTION"));
	g_assert (!as_is_spdx_license_expression ("CC0 dave"));
	g_assert (!as_is_spdx_license_expression (""));
	g_assert (!as_is_spdx_license_expression (NULL));

	/* importing non-SPDX formats */
	tmp = as_license_to_spdx_id ("CC0 and (Public Domain and GPLv3+ with exceptions)");
	g_assert_cmpstr (tmp, ==, "CC0-1.0 AND (LicenseRef-public-domain AND GPL-3.0+)");
	g_free (tmp);

	/* licenses suitable for metadata licensing */
	g_assert (as_license_is_metadata_license ("CC0"));
	g_assert (as_license_is_metadata_license ("CC0-1.0"));
	g_assert (as_license_is_metadata_license ("0BSD"));
	g_assert (as_license_is_metadata_license ("MIT AND FSFAP"));
	g_assert (!as_license_is_metadata_license ("GPL-2.0 AND FSFAP"));
}

/**
 * test_desktop_entry:
 *
 * Test reading a desktop-entry.
 */
static void
test_desktop_entry ()
{
	g_autoptr(AsMetadata) metad = NULL;
	g_autofree gchar *nautilus_de_fname = NULL;
	g_autofree gchar *ksysguard_de_fname = NULL;
	g_autofree gchar *expected_xml = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GError) error = NULL;
	AsComponent *cpt;
	GPtrArray *cpts;
	guint i;
	gchar *tmp;

	nautilus_de_fname = g_build_filename (datadir, "org.gnome.Nautilus.desktop", NULL);
	ksysguard_de_fname = g_build_filename (datadir, "org.kde.ksysguard.desktop", NULL);

	/* Nautilus */
	file = g_file_new_for_path (nautilus_de_fname);

	metad = as_metadata_new ();
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_UNKNOWN, &error);
	g_assert_no_error (error);
	cpt = as_metadata_get_component (metad);

	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.gnome.Nautilus");
	g_assert_cmpint (as_component_get_kind (cpt), ==, AS_COMPONENT_KIND_DESKTOP_APP);

	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Files");

	as_component_set_active_locale (cpt, "lt");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Failai");

	/* clear */
	g_object_unref (file);
	file = NULL;
	as_metadata_clear_components (metad);

	/* KSysGuard */
	file = g_file_new_for_path (ksysguard_de_fname);
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_UNKNOWN, &error);
	g_assert_no_error (error);
	cpt = as_metadata_get_component (metad);

	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.kde.ksysguard");
	g_assert_cmpint (as_component_get_kind (cpt), ==, AS_COMPONENT_KIND_DESKTOP_APP);

	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "KSysGuard");

	/* validate everything */

	/* add nautilus again */
	g_object_unref (file);
	file = g_file_new_for_path (nautilus_de_fname);
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_DESKTOP_ENTRY, &error);
	g_assert_no_error (error);

	/* adjust the priority */
	cpts = as_metadata_get_components (metad);
	for (i = 0; i < cpts->len; i++) {
		cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));
		as_component_set_priority (cpt, -1);
	}

	/* get expected XML */
	tmp = g_build_filename (datadir, "desktop-converted.xml", NULL);
	g_file_get_contents (tmp, &expected_xml, NULL, &error);
	g_assert_no_error (error);
	g_free (tmp);

	tmp = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);
	g_assert (as_test_compare_lines (tmp, expected_xml));
	g_free (tmp);
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
	datadir = g_build_filename (argv[1], "samples", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/AppStream/Categories", test_categories);
	g_test_add_func ("/AppStream/SimpleMarkupConvert", test_simplemarkup);
	g_test_add_func ("/AppStream/Component", test_component);
	g_test_add_func ("/AppStream/SPDX", test_spdx);
	g_test_add_func ("/AppStream/TranslationFallback", test_translation_fallback);
	g_test_add_func ("/AppStream/DesktopEntry", test_desktop_entry);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
