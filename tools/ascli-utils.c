/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ascli-utils.h"

#include <config.h>
#include <glib/gi18n-lib.h>

/**
 * Set to true if we don't want colored output
 */
gboolean _nocolor_output = FALSE;

/**
 * ascli_format_long_output:
 */
gchar*
ascli_format_long_output (const gchar *str)
{
	gchar *res;
	gchar *str2;
	gchar **strv;
	guint i;
	gboolean do_linebreak = FALSE;

	str2 = g_strdup (str);
	for (i = 0; str2[i] != '\0'; ++i) {
		if ((i != 0) && ((i % 80) == 0))
			do_linebreak = TRUE;
		if ((do_linebreak) && (str2[i] == ' ')) {
			do_linebreak = FALSE;
			str2[i] = '\n';
		}
	}

	strv = g_strsplit (str2, "\n", -1);
	g_free (str2);

	res = g_strjoinv ("\n  ", strv);
	g_strfreev (strv);

	return res;
}

/**
 * ascli_print_key_value:
 */
void
ascli_print_key_value (const gchar* key, const gchar* val, gboolean highlight)
{
	gchar *str;
	gchar *fmtval;
	g_return_if_fail (key != NULL);

	if (as_str_empty (val))
		return;

	if (strlen (val) > 120) {
		/* only produces slightly better output (indented).
		 * we need word-wrapping in future
		 */
		fmtval = ascli_format_long_output (val);
	} else {
		fmtval = g_strdup (val);
	}

	str = g_strdup_printf ("%s: ", key);

	if (_nocolor_output) {
		g_print ("%s%s\n", str, fmtval);
	} else {
		g_print ("%c[%dm%s%c[%dm%s\n", 0x1B, 1, str, 0x1B, 0, fmtval);
	}

	g_free (str);
	g_free (fmtval);
}

/**
 * ascli_print_separator:
 */
void
ascli_print_separator ()
{
	if (_nocolor_output) {
		g_print ("----\n");
	} else {
		g_print ("%c[%dm%s\n%c[%dm", 0x1B, 36, "----", 0x1B, 0);
	}
}

/**
 * ascli_print_stderr:
 */
void
ascli_print_stderr (const gchar *format, ...)
{
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_printerr ("%s\n", str);

	g_free (str);
}

/**
 * ascli_print_stdout:
 */
void
ascli_print_stdout (const gchar *format, ...)
{
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_print ("%s\n", str);

	g_free (str);
}

/**
 * string_hashtable_to_str:
 */
static void
string_hashtable_to_str (gchar *key, gchar *value, GString *gstr)
{
	g_string_append_printf (gstr, "%s:%s, ", key, value);
}

/**
 * ascli_set_colored_output:
 */
void
ascli_set_colored_output (gboolean colored)
{
	_nocolor_output = !colored;
}

/**
 * as_get_bundle_str:
 */
static gchar*
as_get_bundle_str (AsComponent *cpt)
{
	GHashTable *bundle_ids;
	GString *gstr;

	bundle_ids = as_component_get_bundle_ids (cpt);
	if (g_hash_table_size (bundle_ids) <= 0)
		return NULL;

	gstr = g_string_new ("");
	g_hash_table_foreach (bundle_ids, (GHFunc) string_hashtable_to_str, gstr);
	if (gstr->len > 0)
		g_string_truncate (gstr, gstr->len - 2);

	return g_string_free (gstr, FALSE);
}

/**
 * ascli_print_component:
 *
 * Print well-formatted details about a component to stdout.
 */
void
ascli_print_component (AsComponent *cpt, gboolean show_detailed)
{
	gchar *short_idline;
	gchar *pkgs_str = NULL;
	gchar *bundles_str = NULL;
	guint j;

	short_idline = g_strdup_printf ("%s [%s]",
							as_component_get_id (cpt),
							as_component_kind_to_string (as_component_get_kind (cpt)));
	if (as_component_get_pkgnames (cpt) != NULL)
		pkgs_str = g_strjoinv (", ", as_component_get_pkgnames (cpt));
	bundles_str = as_get_bundle_str (cpt);

	ascli_print_key_value (_("Identifier"), short_idline, FALSE);
	ascli_print_key_value (_("Name"), as_component_get_name (cpt), FALSE);
	ascli_print_key_value (_("Summary"), as_component_get_summary (cpt), FALSE);
	ascli_print_key_value (_("Package"), pkgs_str, FALSE);
	ascli_print_key_value (_("Bundle"), bundles_str, FALSE);
	ascli_print_key_value (_("Homepage"), as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), FALSE);
	ascli_print_key_value (_("Icon"), as_component_get_icon_url (cpt, 64, 64), FALSE);
	g_free (short_idline);
	g_free (pkgs_str);
	g_free (bundles_str);
	short_idline = NULL;

	if (show_detailed) {
		GPtrArray *sshot_array;
		GPtrArray *imgs = NULL;
		GPtrArray *provided_items;
		AsScreenshot *sshot;
		AsImage *img;
		gchar *str;
		gchar **strv;

		/* developer name */
		ascli_print_key_value (_("Developer"), as_component_get_developer_name (cpt), FALSE);

		/* long description */
		str = as_description_markup_convert_simple (as_component_get_description (cpt));
		ascli_print_key_value (_("Description"), str, FALSE);
		g_free (str);

		/* some simple screenshot information */
		sshot_array = as_component_get_screenshots (cpt);

		/* find default screenshot, if possible */
		sshot = NULL;
		for (j = 0; j < sshot_array->len; j++) {
			sshot = (AsScreenshot*) g_ptr_array_index (sshot_array, j);
			if (as_screenshot_get_kind (sshot) == AS_SCREENSHOT_KIND_DEFAULT)
				break;
		}

		if (sshot != NULL) {
			/* get the first source image and display it's url */
			imgs = as_screenshot_get_images (sshot);
			for (j = 0; j < imgs->len; j++) {
				img = (AsImage*) g_ptr_array_index (imgs, j);
				if (as_image_get_kind (img) == AS_IMAGE_KIND_SOURCE) {
					ascli_print_key_value (_("Sample Screenshot URL"), as_image_get_url (img), FALSE);
					break;
				}
			}
		}

		/* project group */
		ascli_print_key_value (_("Project Group"), as_component_get_project_group (cpt), FALSE);

		/* license */
		ascli_print_key_value (_("License"), as_component_get_project_license (cpt), FALSE);

		/* Categories */
		strv = as_component_get_categories (cpt);
		if (strv != NULL) {
			str = g_strjoinv (", ", strv);
			ascli_print_key_value (_("Categories"), str, FALSE);
			g_free (str);
		}

		/* desktop-compulsority */
		strv = as_component_get_compulsory_for_desktops (cpt);
		if (strv != NULL) {
			str = g_strjoinv (", ", strv);
			ascli_print_key_value (_("Compulsory for"), str, FALSE);
			g_free (str);
		}

		/* Provided Items */
		provided_items = as_component_get_provided_items (cpt);
		strv = as_ptr_array_to_strv (provided_items);
		if (strv != NULL) {
			str = g_strjoinv (" ", strv);
			ascli_print_key_value (_("Provided Items"), str, FALSE);
			g_free (str);
		}
		g_strfreev (strv);
	}
}
