/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-distro-details-private.h"
#include "as-utils-private.h"

#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * test_strstripnl:
 *
 * Test our version of strstrip.
 */
static void
test_strstripnl ()
{
	gchar *tmp;

	tmp = g_strdup ("     MyString      ");
	as_strstripnl (tmp);
	g_assert_cmpstr (tmp, ==, "MyString");
	g_free (tmp);

	tmp = g_strdup ("\n \n    My\nString \n    \n \n");
	as_strstripnl (tmp);
	g_assert_cmpstr (tmp, ==, "My\nString");
	g_free (tmp);

	tmp = g_strdup ("My\nString");
	as_strstripnl (tmp);
	g_assert_cmpstr (tmp, ==, "My\nString");
	g_free (tmp);

	tmp = g_strdup ("");
	as_strstripnl (tmp);
	g_assert_cmpstr (tmp, ==, "");
	g_free (tmp);
}

/**
 * test_random:
 */
static void
test_random ()
{
	g_autofree gchar *str1 = NULL;
	g_autofree gchar *str2 = NULL;

	str1 = as_random_alnum_string (24);
	g_assert_cmpint (strlen (str1), ==, 24);

	str2 = as_random_alnum_string (24);
	g_assert_cmpint (strlen (str2), ==, 24);

	g_assert_cmpstr (str1, !=, str2);
}

/**
 * test_safe_assign:
 *
 * Test safe variable assignment macros.
 */
static void
test_safe_assign ()
{
	gchar *tmp;
	g_autofree gchar *member1 = g_strdup ("Test A");
	g_autofree gchar *value1 = g_strdup ("New Value");
	g_autoptr(GPtrArray) member2 = g_ptr_array_new_with_free_func (g_free);
	g_autoptr(GPtrArray) value2 = g_ptr_array_new_with_free_func (g_free);

	/* assigning a variable to itself should be safe */
	tmp = member1;
	as_assign_string_safe (member1, member1);
	g_assert_cmpstr (member1, ==, "Test A");
	g_assert_true (tmp == member1);

	/* assign new literal */
	tmp = member1;
	as_assign_string_safe (member1, "Literal");
	g_assert_cmpstr (member1, ==, (const gchar*) "Literal");

	/* assign new value */
	tmp = member1;
	as_assign_string_safe (member1, value1);
	g_assert_cmpstr (member1, ==, "New Value");
	g_assert_true (member1 != value1);
	g_assert_cmpstr (value1, ==, "New Value");

	/* test PtrArray assignments */
	g_ptr_array_add (member2, g_strdup ("Item1"));
	as_assign_ptr_array_safe (member2, member2);
	g_assert_cmpstr (g_ptr_array_index (member2, 0), ==, "Item1");

	g_ptr_array_add (value2, g_strdup ("Very new item"));
	as_assign_ptr_array_safe (member2, value2);
	g_assert_cmpstr (g_ptr_array_index (member2, 0), ==, "Very new item");
}

/**
 * test_verify_int_str:
 */
static void
test_verify_int_str ()
{
	g_assert_false (as_str_verify_integer ("", G_MININT64, G_MAXINT64));
	g_assert_true (as_str_verify_integer ("64", G_MININT64, G_MAXINT64));
	g_assert_false (as_str_verify_integer ("128Kb", G_MININT64, G_MAXINT64));
	g_assert_false (as_str_verify_integer ("Hello42", G_MININT64, G_MAXINT64));
	g_assert_true (as_str_verify_integer ("-400", G_MININT64, G_MAXINT64));
	g_assert_false (as_str_verify_integer ("-400", 1, G_MAXINT64));
	g_assert_false (as_str_verify_integer ("4800", G_MININT64, 4000));
}

/**
 * test_categories:
 *
 * Test #AsCategory properties.
 */
static void
test_categories ()
{
	g_autoptr(GPtrArray) default_cats = NULL;
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
	g_autoptr(GError) error = NULL;

	str = as_markup_convert_simple ("<p>Test!</p><p>Blah.</p><ul><li>A</li><li>B</li></ul><p>End.</p>", &error);
	g_assert_no_error (error);
	g_assert_true (g_strcmp0 (str, "Test!\n\nBlah.\n • A\n • B\n\nEnd.") == 0);
	g_free (str);

	str = as_markup_convert_simple ("<p>Paragraph using all allowed markup, "
					"like an <em>emphasis</em> or <code>some code</code>.</p>"
					"<p>Second paragraph.</p>"
					"<ul>"
					"<li>List item, <em>emphasized</em></li>"
					"<li>Item with <code>a bit of code</code></li>"
					"</ul>"
					"<p>Last paragraph.</p>", &error);
	g_assert_no_error (error);
	g_assert_true (g_strcmp0 (str, "Paragraph using all allowed markup, like an emphasis or some code.\n\n"
				  "Second paragraph.\n"
				  " • List item, emphasized\n"
				  " • Item with a bit of code\n\n"
				  "Last paragraph.") == 0);
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
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsMetadata) metad = NULL;
	g_autofree gchar *str = NULL;
	g_autofree gchar *str2 = NULL;
	g_auto(GStrv) strv = NULL;

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);

	as_component_set_id (cpt, "org.example.test.desktop");
	as_component_set_name (cpt, "Test", NULL);
	as_component_set_summary (cpt, "It does things", NULL);

	strv = _get_dummy_strv ("fedex");
	as_component_set_pkgnames (cpt, strv);

	metad = as_metadata_new ();
	as_metadata_add_component (metad, cpt);
	str = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, NULL);
	str2 = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, NULL);
	g_debug ("%s", str2);

	g_assert_cmpstr (str, ==, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				  "<component type=\"desktop-application\">\n"
				  "  <id>org.example.test.desktop</id>\n"
				  "  <name>Test</name>\n"
				  "  <summary>It does things</summary>\n"
				  "  <pkgname>fedex</pkgname>\n"
				  "</component>\n");
	g_assert_cmpstr (str2, ==, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				   "<components version=\"0.14\">\n"
				   "  <component type=\"desktop-application\">\n"
				   "    <id>org.example.test.desktop</id>\n"
				   "    <name>Test</name>\n"
				   "    <summary>It does things</summary>\n"
				   "    <pkgname>fedex</pkgname>\n"
				   "  </component>\n"
				   "</components>\n");
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
 * test_locale_compat:
 */
static void
test_locale_compat (void)
{
	g_assert_true (as_utils_locale_is_compatible ("de_DE", "de_DE"));
	g_assert_true (!as_utils_locale_is_compatible ("de_DE", "en"));
	g_assert_true (as_utils_locale_is_compatible ("de_DE", "de"));
	g_assert_true (as_utils_locale_is_compatible ("ca_ES@valencia", "ca"));
	g_assert_true (as_utils_locale_is_compatible ("ca@valencia", "ca"));
	g_assert_true (!as_utils_locale_is_compatible ("ca@valencia", "de"));
	g_assert_true (!as_utils_locale_is_compatible ("de_CH", "de_DE"));
	g_assert_true (as_utils_locale_is_compatible ("de", "de_CH"));
	g_assert_true (as_utils_locale_is_compatible ("C", "C"));
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
	g_assert_true (tok == NULL);

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
	g_assert_true (tmp == NULL);

	/* leading brackets */
	tok = as_spdx_license_tokenize ("(MPLv1.1 or LGPLv3+) and LGPLv3");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "(  MPLv1.1  |  LGPLv3+  )  &  LGPLv3");
	g_strfreev (tok);
	g_free (tmp);

	/* trailing brackets */
	tok = as_spdx_license_tokenize ("MPLv1.1 and (LGPLv3 or GPLv3)");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "MPLv1.1  &  (  LGPLv3  |  GPLv3  )");
	g_strfreev (tok);
	g_free (tmp);

	/* deprecated names */
	tok = as_spdx_license_tokenize ("CC0 and (CC0 or CC0)");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@CC0-1.0  &  (  @CC0-1.0  |  @CC0-1.0  )");
	g_strfreev (tok);
	g_free (tmp);

	/* WITH operator */
	tok = as_spdx_license_tokenize ("GPL-3.0-or-later WITH GCC-exception-3.1");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@GPL-3.0+  ^  @GCC-exception-3.1");
	g_strfreev (tok);
	g_free (tmp);

	tok = as_spdx_license_tokenize ("OFL-1.1 OR (GPL-3.0-or-later WITH Font-exception-2.0)");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@OFL-1.1  |  (  @GPL-3.0+  ^  @Font-exception-2.0  )");
	g_strfreev (tok);
	g_free (tmp);

	/* SPDX strings */
	g_assert_true (as_is_spdx_license_expression ("CC0-1.0"));
	g_assert_true (as_is_spdx_license_expression ("CC0"));
	g_assert_true (as_is_spdx_license_expression ("LicenseRef-proprietary"));
	g_assert_true (as_is_spdx_license_expression ("CC0-1.0 and GFDL-1.3"));
	g_assert_true (as_is_spdx_license_expression ("CC0-1.0 AND GFDL-1.3"));
	g_assert_true (as_is_spdx_license_expression ("CC-BY-SA-3.0+"));
	g_assert_true (as_is_spdx_license_expression ("CC-BY-SA-3.0+ AND Zlib"));
	g_assert_true (as_is_spdx_license_expression ("GPL-3.0-or-later WITH GCC-exception-3.1"));
	g_assert_true (as_is_spdx_license_expression ("GPL-3.0-or-later WITH Font-exception-2.0 AND OFL-1.1"));
	g_assert_true (as_is_spdx_license_expression ("NOASSERTION"));
	g_assert_true (!as_is_spdx_license_expression ("CC0 dave"));
	g_assert_true (!as_is_spdx_license_expression (""));
	g_assert_true (!as_is_spdx_license_expression (NULL));

	/* importing non-SPDX formats */
	tmp = as_license_to_spdx_id ("CC0 and (Public Domain and GPLv3+ with exceptions)");
	g_assert_cmpstr (tmp, ==, "CC0-1.0 AND (LicenseRef-public-domain AND GPL-3.0+)");
	g_free (tmp);

	/* licenses suitable for metadata licensing */
	g_assert_true (as_license_is_metadata_license ("CC0"));
	g_assert_true (as_license_is_metadata_license ("CC0-1.0"));
	g_assert_true (as_license_is_metadata_license ("0BSD"));
	g_assert_true (as_license_is_metadata_license ("MIT AND FSFAP"));
	g_assert_true (!as_license_is_metadata_license ("GPL-2.0 AND FSFAP"));
	g_assert_true (as_license_is_metadata_license ("GPL-2.0+ OR GFDL-1.3-only"));

	/* check license URL generation */
	tmp = as_get_license_url ("CC0");
	g_assert_cmpstr (tmp, ==, "https://spdx.org/licenses/CC0-1.0.html#page");
	g_free (tmp);

	tmp = as_get_license_url ("LGPL-2.0-or-later");
	g_assert_cmpstr (tmp, ==, "https://spdx.org/licenses/LGPL-2.0-or-later.html#page");
	g_free (tmp);

	tmp = as_get_license_url ("@GPL-2.0+");
	g_assert_cmpstr (tmp, ==, "https://choosealicense.com/licenses/gpl-2.0/");
	g_free (tmp);

	tmp = as_get_license_url ("LicenseRef-proprietary");
	g_assert_cmpstr (tmp, ==, NULL);
	g_free (tmp);

	tmp = as_get_license_url ("LicenseRef-proprietary=https://example.com/mylicense.txt");
	g_assert_cmpstr (tmp, ==, "https://example.com/mylicense.txt");
	g_free (tmp);

	/* licenses are free-as-in-freedom */
	g_assert_true (as_license_is_free_license ("CC0"));
	g_assert_true (as_license_is_free_license ("GPL-2.0 AND FSFAP"));
	g_assert_true (as_license_is_free_license ("OFL-1.1 OR (GPL-3.0-or-later WITH Font-exception-2.0)"));
	g_assert_true (!as_license_is_free_license ("NOASSERTION"));
	g_assert_true (!as_license_is_free_license ("LicenseRef-proprietary=https://example.com/mylicense.txt"));
	g_assert_true (!as_license_is_free_license ("MIT AND LicenseRef-proprietary=https://example.com/lic.txt"));
	g_assert_true (!as_license_is_free_license ("ADSL"));
	g_assert_true (!as_license_is_free_license ("JSON AND GPL-3.0-or-later"));
}

/**
 * test_read_desktop_entry_simple:
 *
 * Read XDG desktop-entry file.
 */
static void
test_read_desktop_entry_simple ()
{
	static const gchar *desktop_entry_data =
		"[Desktop Entry]\n"
		"Type=Application\n"
		"Name=FooBar\n"
		"Name[de_DE]=FööBär\n"
		"Comment=A foo-ish bar.\n"
		"Keywords=Hobbes;Bentham;Locke;\n"
		"Keywords[de_DE]=Heidegger;Kant;Hegel;\n";

	gboolean ret;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;
	AsComponent *cpt;
	AsLaunchable *launch;
	GPtrArray *entries;
	gchar *tmp;

	metad = as_metadata_new ();

	ret = as_metadata_parse_desktop_data (metad, desktop_entry_data, "foobar.desktop", &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	cpt = as_metadata_get_component (metad);
	g_assert_nonnull (cpt);
	as_component_set_active_locale (cpt, "C.UTF-8");
	g_assert_cmpstr (as_component_get_id (cpt), ==, "foobar.desktop");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "FooBar");
	tmp = g_strjoinv (", ", as_component_get_keywords (cpt));
	g_assert_cmpstr (tmp, ==, "Hobbes, Bentham, Locke");
	g_free (tmp);

	as_component_set_active_locale (cpt, "de_DE");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "FööBär");
	tmp = g_strjoinv (", ", as_component_get_keywords (cpt));
	g_assert_cmpstr (tmp, ==, "Heidegger, Kant, Hegel");
	g_free (tmp);

	launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_nonnull (launch);
	g_assert_cmpint (as_launchable_get_kind (launch), ==, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	entries = as_launchable_get_entries (launch);
	g_assert_cmpint (entries->len, ==, 1);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (entries, 0), ==, "foobar.desktop");

	/* test component-id trimming */
	as_metadata_clear_components (metad);
	ret = as_metadata_parse_desktop_data (metad, desktop_entry_data, "org.example.foobar.desktop", &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	cpt = as_metadata_get_component (metad);
	g_assert_nonnull (cpt);

	as_component_set_active_locale (cpt, "C.UTF-8");
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.foobar");

	launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_nonnull (launch);
	g_assert_cmpint (as_launchable_get_kind (launch), ==, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	entries = as_launchable_get_entries (launch);
	g_assert_cmpint (entries->len, ==, 1);
	g_assert_cmpstr ((const gchar*) g_ptr_array_index (entries, 0), ==, "org.example.foobar.desktop");
}

/**
 * test_desktop_entry_convert:
 *
 * Test reading a desktop-entry.
 */
static void
test_desktop_entry_convert ()
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
	g_assert_true (as_test_compare_lines (tmp, expected_xml));
	g_free (tmp);
}

/**
 * test_version_compare:
 *
 * Test version comparisons.
 */
static void
test_version_compare ()
{
	g_assert_cmpint (as_vercmp_simple ("6", "8"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("0.6.12b-d", "0.6.12a"), >, 0);
	g_assert_cmpint (as_vercmp_simple ("7.4", "7.4"), ==, 0);
	g_assert_cmpint (as_vercmp_simple ("ab.d", "ab.f"), <, 0);

	g_assert_cmpint (as_vercmp_simple ("0.6.16", "0.6.14"), >, 0);

	g_assert_cmpint (as_vercmp_simple ("5.9.1+dfsg-5pureos1", "5.9.1+dfsg-5"), >, 0);
	g_assert_cmpint (as_vercmp_simple ("2.79", "2.79a"), <, 0);

	g_assert_cmpint (as_vercmp_simple ("3.0.rc2", "3.0.0"), >, 0);
	g_assert_cmpint (as_vercmp_simple ("3.0.0~rc2", "3.0.0"), <, 0);

	g_assert_cmpint (as_vercmp_simple (NULL, NULL), ==, 0);
	g_assert_cmpint (as_vercmp_simple (NULL, "4.0"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("4.0", NULL), >, 0);

	/* issue #288 */
	g_assert_cmpint (as_vercmp_simple ("11.0.9.1+1-0ubuntu1", "11.0.9+11-0ubuntu2"), >, 0);

	/* same */
	g_assert_cmpint (as_vercmp_simple ("1.2.3", "1.2.3"), ==, 0);
	g_assert_cmpint (as_vercmp_simple ("001.002.003", "001.002.003"), ==, 0);

	/* epochs */
	g_assert_cmpint (as_vercmp_simple ("4:5.6-2", "8.0-6"), >, 0);
	g_assert_cmpint (as_vercmp_simple ("1:1.0-4", "3:0.8-2"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("1:1.0-4", "3:0.8-2"), <, 0);
	g_assert_cmpint (as_vercmp ("1:1.0-4", "3:0.8-2", AS_VERCMP_FLAG_IGNORE_EPOCH), >, 0);

	/* upgrade and downgrade */
	g_assert_cmpint (as_vercmp_simple ("1.2.3", "1.2.4"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("001.002.000", "001.002.009"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2.3", "1.2.2"), >, 0);
	g_assert_cmpint (as_vercmp_simple ("001.002.009", "001.002.000"), >, 0);

	/* unequal depth */
	g_assert_cmpint (as_vercmp_simple ("1.2.3", "1.2.3.1"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2.3.1", "1.2.4"), <, 0);

	/* mixed-alpha-numeric */
	g_assert_cmpint (as_vercmp_simple ("1.2.3a", "1.2.3a"), ==, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2.3a", "1.2.3b"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2.3b", "1.2.3a"), >, 0);

	/* alpha version append */
	g_assert_cmpint (as_vercmp_simple ("1.2.3", "1.2.3a"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2.3a", "1.2.3"), >, 0);

	/* alpha only */
	g_assert_cmpint (as_vercmp_simple ("alpha", "alpha"), ==, 0);
	g_assert_cmpint (as_vercmp_simple ("alpha", "beta"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("beta", "alpha"), >, 0);

	/* alpha-compare */
	g_assert_cmpint (as_vercmp_simple ("1.2a.3", "1.2a.3"), ==, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2a.3", "1.2b.3"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2b.3", "1.2a.3"), >, 0);

	/* tilde is all-powerful */
	g_assert_cmpint (as_vercmp_simple ("1.2.3~rc1", "1.2.3~rc1"), ==, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2.3~rc1", "1.2.3"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2.3", "1.2.3~rc1"), >, 0);
	g_assert_cmpint (as_vercmp_simple ("1.2.3~rc2", "1.2.3~rc1"), >, 0);

	/* more complex */
	g_assert_cmpint (as_vercmp_simple ("0.9", "1"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("9", "9a"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("9a", "10"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("9+", "10"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("9half", "10"), <, 0);
	g_assert_cmpint (as_vercmp_simple ("9.5", "10"), <, 0);
}

/**
 * test_distro_details:
 *
 * Test fetching distro details.
 */
static void
test_distro_details ()
{
	g_autofree gchar *osrelease_fname = NULL;
	g_autoptr(AsDistroDetails) distro = as_distro_details_new ();

	osrelease_fname = g_build_filename (datadir, "os-release-1", NULL);

	as_distro_details_load_data (distro, osrelease_fname, NULL);

	g_assert_cmpstr (as_distro_details_get_name (distro), ==, "Debian GNU/Linux");
	g_assert_cmpstr (as_distro_details_get_version (distro), ==, "10.0");
	g_assert_cmpstr (as_distro_details_get_homepage (distro), ==, "https://www.debian.org/");

	g_assert_cmpstr (as_distro_details_get_id (distro), ==, "debian");
	g_assert_cmpstr (as_distro_details_get_cid (distro), ==, "org.debian.debian");
}

/**
 * test_rdns_convert:
 *
 * Test URL to component-ID conversion.
 */
static void
test_rdns_convert ()
{
	gchar *tmp;

	tmp = as_utils_dns_to_rdns ("https://example.com", NULL);
	g_assert_cmpstr (tmp, ==, "com.example");
	g_free (tmp);

	tmp = as_utils_dns_to_rdns ("http://www.example.org/", NULL);
	g_assert_cmpstr (tmp, ==, "org.example");
	g_free (tmp);

	tmp = as_utils_dns_to_rdns ("example.org/blah/blub", NULL);
	g_assert_cmpstr (tmp, ==, "org.example");
	g_free (tmp);

	tmp = as_utils_dns_to_rdns ("www.example..org/u//n", NULL);
	g_assert_cmpstr (tmp, ==, "org..example");
	g_free (tmp);

	tmp = as_utils_dns_to_rdns ("https://example.com", "MyApp");
	g_assert_cmpstr (tmp, ==, "com.example.MyApp");
	g_free (tmp);
}

/**
 * test_filebasename_from_uri:
 */
static void
test_filebasename_from_uri ()
{
	gchar *tmp;

	tmp = as_filebasename_from_uri ("https://example.org/test/filename.txt");
	g_assert_cmpstr (tmp, ==, "filename.txt");
	g_free (tmp);

	tmp = as_filebasename_from_uri ("https://example.org/test/video.mkv?raw=true");
	g_assert_cmpstr (tmp, ==, "video.mkv");
	g_free (tmp);

	tmp = as_filebasename_from_uri ("https://example.org/test/video.mkv#anchor");
	g_assert_cmpstr (tmp, ==, "video.mkv");
	g_free (tmp);

	tmp = as_filebasename_from_uri ("https://example.org/test/video.mkv?raw=true&aaa=bbb");
	g_assert_cmpstr (tmp, ==, "video.mkv");
	g_free (tmp);

	tmp = as_filebasename_from_uri ("");
	g_assert_cmpstr (tmp, ==, ".");
	g_free (tmp);

	tmp = as_filebasename_from_uri (NULL);
	g_assert_cmpstr (tmp, ==, NULL);
	g_free (tmp);
}

/* Test that the OARS → CSM mapping table in as_content_rating_attribute_to_csm_age()
 * is complete (contains mappings for all known IDs), and that the ages it
 * returns are non-decreasing for increasing values of #AsContentRatingValue in
 * each ID.
 *
 * Also test that unknown values of #AsContentRatingValue return an unknown age,
 * and unknown IDs do similarly. */
static void
test_content_rating_mappings (void)
{
	const AsContentRatingValue values[] = {
		AS_CONTENT_RATING_VALUE_NONE,
		AS_CONTENT_RATING_VALUE_MILD,
		AS_CONTENT_RATING_VALUE_MODERATE,
		AS_CONTENT_RATING_VALUE_INTENSE,
	};
	g_autofree const gchar **ids = as_content_rating_get_all_rating_ids ();

	for (gsize i = 0; ids[i] != NULL; i++) {
		guint max_age = 0;

		for (gsize j = 0; j < G_N_ELEMENTS (values); j++) {
			guint age = as_content_rating_attribute_to_csm_age (ids[i], values[j]);
			g_assert_cmpuint (age, >=, max_age);
			max_age = age;
		}

		g_assert_cmpuint (max_age, >, 0);
		g_assert_cmpuint (as_content_rating_attribute_to_csm_age (ids[i], AS_CONTENT_RATING_VALUE_UNKNOWN), ==, 0);
		g_assert_cmpuint (as_content_rating_attribute_to_csm_age (ids[i], AS_CONTENT_RATING_VALUE_LAST), ==, 0);
	}

	g_assert_cmpuint (as_content_rating_attribute_to_csm_age ("not-valid-id", AS_CONTENT_RATING_VALUE_INTENSE), ==, 0);
}

/* Test that gs_utils_content_rating_system_from_locale() returns the correct
 * rating system for various standard locales and various forms of locale name.
 * See `locale -a` for the list of all available locales which some of these
 * test vectors were derived from. */
static void
as_test_content_rating_from_locale (void)
{
	const struct {
		const gchar *locale;
		AsContentRatingSystem expected_system;
	} vectors[] = {
		/* Simple tests to get coverage of each rating system: */
		{ "es_AR", AS_CONTENT_RATING_SYSTEM_INCAA },
		{ "en_AU", AS_CONTENT_RATING_SYSTEM_ACB },
		{ "pt_BR", AS_CONTENT_RATING_SYSTEM_DJCTQ },
		{ "zh_TW", AS_CONTENT_RATING_SYSTEM_GSRR },
		{ "en_GB", AS_CONTENT_RATING_SYSTEM_PEGI },
		{ "hy_AM", AS_CONTENT_RATING_SYSTEM_PEGI },
		{ "bg_BG", AS_CONTENT_RATING_SYSTEM_PEGI },
		{ "fi_FI", AS_CONTENT_RATING_SYSTEM_KAVI },
		{ "de_DE", AS_CONTENT_RATING_SYSTEM_USK },
		{ "az_IR", AS_CONTENT_RATING_SYSTEM_ESRA },
		{ "jp_JP", AS_CONTENT_RATING_SYSTEM_CERO },
		{ "en_NZ", AS_CONTENT_RATING_SYSTEM_OFLCNZ },
		{ "ru_RU", AS_CONTENT_RATING_SYSTEM_RUSSIA },
		{ "en_SQ", AS_CONTENT_RATING_SYSTEM_MDA },
		{ "ko_KR", AS_CONTENT_RATING_SYSTEM_GRAC },
		{ "en_US", AS_CONTENT_RATING_SYSTEM_ESRB },
		{ "en_US", AS_CONTENT_RATING_SYSTEM_ESRB },
		{ "en_CA", AS_CONTENT_RATING_SYSTEM_ESRB },
		{ "es_MX", AS_CONTENT_RATING_SYSTEM_ESRB },
		/* Fallback (arbitrarily chosen Venezuela since it seems to use IARC): */
		{ "es_VE", AS_CONTENT_RATING_SYSTEM_IARC },
		/* Locale with a codeset: */
		{ "nl_NL.iso88591", AS_CONTENT_RATING_SYSTEM_PEGI },
		/* Locale with a codeset and modifier: */
		{ "nl_NL.iso885915@euro", AS_CONTENT_RATING_SYSTEM_PEGI },
		/* Locale with a less esoteric codeset: */
		{ "en_GB.UTF-8", AS_CONTENT_RATING_SYSTEM_PEGI },
		/* Locale with a modifier but no codeset: */
		{ "fi_FI@euro", AS_CONTENT_RATING_SYSTEM_KAVI },
		/* Invalid locale: */
		{ "_invalid", AS_CONTENT_RATING_SYSTEM_IARC },
	};

	for (gsize i = 0; i < G_N_ELEMENTS (vectors); i++) {
		g_test_message ("Test %" G_GSIZE_FORMAT ": %s", i, vectors[i].locale);
		g_assert_cmpint (as_content_rating_system_from_locale (vectors[i].locale), ==, vectors[i].expected_system);
	}
}

/**
 * test_utils_data_id_hash:
 *
 * Shows the data-id globbing functions at work
 */
void
test_utils_data_id_hash (void)
{
	AsComponent *found;
	g_autoptr(AsComponent) cpt1 = NULL;
	g_autoptr(AsComponent) cpt2 = NULL;
	g_autoptr(GHashTable) hash = NULL;

	/* create new couple of apps */
	cpt1 = as_component_new ();
	as_component_set_id (cpt1, "org.gnome.Software.desktop");
	as_component_set_branch (cpt1, "master");
	g_assert_cmpstr (as_component_get_data_id (cpt1), ==,
			 "*/*/*/org.gnome.Software.desktop/master");
	cpt2 = as_component_new ();
	as_component_set_id (cpt2, "org.gnome.Software.desktop");
	as_component_set_branch (cpt2, "stable");
	g_assert_cmpstr (as_component_get_data_id (cpt2), ==,
			 "*/*/*/org.gnome.Software.desktop/stable");

	/* add to hash table using the data ID as a key */
	hash = g_hash_table_new ((GHashFunc) as_utils_data_id_hash,
				 (GEqualFunc) as_utils_data_id_equal);
	g_hash_table_insert (hash, (gpointer) as_component_get_data_id (cpt1), cpt1);
	g_hash_table_insert (hash, (gpointer) as_component_get_data_id (cpt2), cpt2);

	/* get with exact key */
	found = g_hash_table_lookup (hash, "*/*/*/org.gnome.Software.desktop/master");
	g_assert_true (found != NULL);
	found = g_hash_table_lookup (hash, "*/*/*/org.gnome.Software.desktop/stable");
	g_assert_true (found != NULL);

	/* get with more details specified */
	found = g_hash_table_lookup (hash, "system/*/*/org.gnome.Software.desktop/master");
	g_assert_true (found != NULL);
	found = g_hash_table_lookup (hash, "system/*/*/org.gnome.Software.desktop/stable");
	g_assert_true (found != NULL);

	/* get with less details specified */
	found = g_hash_table_lookup (hash, "*/*/*/org.gnome.Software.desktop/*");
	g_assert_true (found != NULL);

	/* different key */
	found = g_hash_table_lookup (hash, "*/*/*/gimp.desktop/*");
	g_assert_true (found == NULL);

	/* different branch */
	found = g_hash_table_lookup (hash, "*/*/*/org.gnome.Software.desktop/obsolete");
	g_assert_true (found == NULL);

	/* check hash function */
	g_assert_cmpint (as_utils_data_id_hash ("*/*/*/gimp.desktop/master"), ==,
			 as_utils_data_id_hash ("system/*/*/gimp.desktop/stable"));
}

/**
 * test_utils_data_id_hash_str:
 *
 * Shows the as_utils_data_id_* functions are safe with bare text.
 */
void
test_utils_data_id_hash_str (void)
{
	AsComponent *found;
	g_autoptr(AsComponent) app = NULL;
	g_autoptr(GHashTable) hash = NULL;

	/* create new app */
	app = as_component_new ();
	as_component_set_id (app, "org.gnome.Software");

	/* add to hash table using the data ID as a key */
	hash = g_hash_table_new ((GHashFunc) as_utils_data_id_hash,
				 (GEqualFunc) as_utils_data_id_equal);
	g_hash_table_insert (hash, (gpointer) "dave", app);

	/* get with exact key */
	found = g_hash_table_lookup (hash, "dave");
	g_assert_true (found != NULL);

	/* different key */
	found = g_hash_table_lookup (hash, "frank");
	g_assert_true (found == NULL);
}

/**
 * test_utils_platform_triplet:
 */
void
test_utils_platform_triplet (void)
{
	g_assert_true (as_utils_is_platform_triplet ("x86_64-linux-gnu"));
	g_assert_true (as_utils_is_platform_triplet ("x86_64-windows-msvc"));
	g_assert_true (as_utils_is_platform_triplet ("x86_64-linux-gnu"));
	g_assert_true (as_utils_is_platform_triplet ("aarch64-linux-gnu_ilp32"));
	g_assert_true (as_utils_is_platform_triplet ("wasm64-any-any"));
	g_assert_true (as_utils_is_platform_triplet ("any-linux-gnu"));
	g_assert_true (as_utils_is_platform_triplet ("any-any-any"));
	g_assert_false (as_utils_is_platform_triplet ("aarch64-any"));
	g_assert_false (as_utils_is_platform_triplet ("---"));
	g_assert_false (as_utils_is_platform_triplet (NULL));
	g_assert_false (as_utils_is_platform_triplet ("x86_64-gnu-windows"));
	g_assert_false (as_utils_is_platform_triplet ("x86-64-linux-gnu"));
	g_assert_false (as_utils_is_platform_triplet ("x86-lunix-gna"));
}

int
main (int argc, char **argv)
{
	int ret;

	if (argc == 0) {
		g_error ("No test directory specified!");
		return 1;
	}

	g_assert_nonnull (argv[1]);
	datadir = g_build_filename (argv[1], "samples", NULL);
	g_assert_true (g_file_test (datadir, G_FILE_TEST_EXISTS));

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/AppStream/Strstrip", test_strstripnl);
	g_test_add_func ("/AppStream/Random", test_random);
	g_test_add_func ("/AppStream/SafeAssign", test_safe_assign);
	g_test_add_func ("/AppStream/VerifyIntStr", test_verify_int_str);
	g_test_add_func ("/AppStream/Categories", test_categories);
	g_test_add_func ("/AppStream/SimpleMarkupConvert", test_simplemarkup);
	g_test_add_func ("/AppStream/Component", test_component);
	g_test_add_func ("/AppStream/SPDX", test_spdx);
	g_test_add_func ("/AppStream/TranslationFallback", test_translation_fallback);
	g_test_add_func ("/AppStream/LocaleCompat", test_locale_compat);
	g_test_add_func ("/AppStream/ReadDesktopEntry", test_read_desktop_entry_simple);
	g_test_add_func ("/AppStream/ConvertDesktopEntry", test_desktop_entry_convert);
	g_test_add_func ("/AppStream/VersionCompare", test_version_compare);
	g_test_add_func ("/AppStream/DistroDetails", test_distro_details);
	g_test_add_func ("/AppStream/rDNSConvert", test_rdns_convert);
	g_test_add_func ("/AppStream/URIToBasename", test_filebasename_from_uri);
	g_test_add_func ("/AppStream/ContentRating/Mapings", test_content_rating_mappings);
	g_test_add_func ("/AppStream/ContentRating/from-locale", as_test_content_rating_from_locale);

	g_test_add_func ("/AppStream/DataID/hash", test_utils_data_id_hash);
	g_test_add_func ("/AppStream/DataID/hash-str", test_utils_data_id_hash_str);

	g_test_add_func ("/AppStream/PlatformTriplets", test_utils_platform_triplet);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
