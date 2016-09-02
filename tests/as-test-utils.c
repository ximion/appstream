/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * as_test_compare_lines:
 **/
gboolean
as_test_compare_lines (const gchar *txt1, const gchar *txt2)
{
	g_autoptr(GError) error = NULL;
	g_autofree gchar *output = NULL;

	/* exactly the same */
	if (g_strcmp0 (txt1, txt2) == 0)
		return TRUE;

	/* save temp files and diff them */
	if (!g_file_set_contents ("/tmp/as-utest_a", txt1, -1, &error))
		return FALSE;
	if (!g_file_set_contents ("/tmp/as-utest_b", txt2, -1, &error))
		return FALSE;
	if (!g_spawn_command_line_sync ("diff -urNp /tmp/as-utest_b /tmp/as-utest_a",
					&output, NULL, NULL, &error))
		return FALSE;

	g_assert_no_error (error);
	g_print ("%s\n", output);
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
