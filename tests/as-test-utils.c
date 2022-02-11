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

#include "as-test-utils.h"

#include <glib/gstdio.h>

/**
 * as_test_compare_lines:
 **/
gboolean
as_test_compare_lines (const gchar *txt1, const gchar *txt2)
{
	g_autoptr(GError) error = NULL;
	g_autofree gchar *output = NULL;
	g_autofree gchar *tmp_fname1 = NULL;
	g_autofree gchar *tmp_fname2 = NULL;
	g_autofree gchar *diff_cmd = NULL;

	/* check if data is identical */
	if (g_strcmp0 (txt1, txt2) == 0)
		return TRUE;

	/* data is different, print diff and exit with FALSE */

	tmp_fname1 = g_strdup_printf ("/tmp/as-diff-%i_a", g_random_int ());
	tmp_fname2 = g_strdup_printf ("/tmp/as-diff-%i_b", g_random_int ());
	diff_cmd = g_strdup_printf ("diff -urNp %s %s", tmp_fname2, tmp_fname1);

	/* save temp files and diff them */
	if (!g_file_set_contents (tmp_fname1, txt1, -1, &error))
		goto out;
	if (!g_file_set_contents (tmp_fname2, txt2, -1, &error))
		goto out;
	if (!g_spawn_command_line_sync (diff_cmd, &output, NULL, NULL, &error))
		goto out;

	g_assert_no_error (error);
	g_print ("%s\n", output);

out:
	g_remove (tmp_fname1);
	g_remove (tmp_fname2);
	return FALSE;
}

/**
 * as_sort_strings_cb:
 *
 * Helper method to sort lists of strings
 */
static gint
as_sort_strings_cb (gconstpointer a, gconstpointer b)
{
	const gchar *astr = *((const gchar **) a);
	const gchar *bstr = *((const gchar **) b);

	return g_strcmp0 (astr, bstr);
}

/**
 * as_sort_strings:
 */
void
as_sort_strings (GPtrArray *utf8)
{
	g_ptr_array_sort (utf8, as_sort_strings_cb);
}

/**
 * as_component_sort_values:
 */
void
as_component_sort_values (AsComponent *cpt)
{
	GPtrArray *provideds;
	guint i;

	provideds = as_component_get_provided (cpt);
	for (i = 0; i < provideds->len; i++) {
		AsProvided *prov = AS_PROVIDED (g_ptr_array_index (provideds, i));

		g_ptr_array_sort (as_provided_get_items (prov),
				  as_sort_strings_cb);
	}
}

/**
 * as_sort_components_cb:
 *
 * Helper method to sort lists of #AsComponent
 */
static gint
as_sort_components_cb (gconstpointer a, gconstpointer b)
{
	AsComponent *cpt1 = *((AsComponent **) a);
	AsComponent *cpt2 = *((AsComponent **) b);

	return g_strcmp0 (as_component_get_id (cpt1),
			  as_component_get_id (cpt2));
}

/**
 * as_sort_components:
 */
void
as_sort_components (GPtrArray *cpts)
{
	g_ptr_array_sort (cpts, as_sort_components_cb);
}

/**
 * as_gbytes_from_literal:
 */
GBytes*
as_gbytes_from_literal (const gchar *string)
{
	return g_bytes_new_static (string, strlen (string));
}
