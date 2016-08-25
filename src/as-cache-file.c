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

#include "as-cache-file.h"

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-component-private.h"
#include "as-pool.h"

/**
 * SECTION:as-cache-file
 * @short_description: Load and save cache files.
 * @include: appstream.h
 */

/**
 * as_variant_mstring_new:
 *
 * Create a string wrapped in a maybe GVariant.
 */
static GVariant*
as_variant_mstring_new (const gchar *str)
{
	GVariant *res;

	if (str == NULL)
		res = g_variant_new_maybe (G_VARIANT_TYPE_STRING, NULL);
	else
		res = g_variant_new_maybe (G_VARIANT_TYPE_STRING,
					g_variant_new_string (str));
	return res;
}

/**
 * as_cache_file_save:
 * @fname: The file to save the data to.
 * @locale: The locale this cache file is for.
 * @cpts: (element-type AsComponent): The components to serialize.
 * @error: A #GError
 *
 * Serialize components to a cache file and store it on disk.
 */
void
as_cache_file_save (const gchar *fname, const gchar *locale, GPtrArray *cpts, GError **error)
{
	g_autoptr(GVariant) main_gv = NULL;
	g_autoptr(GVariantBuilder) main_builder = NULL;
	g_autoptr(GVariantBuilder) builder = NULL;
	g_autoptr(GFile) ofile = NULL;
	guint cindex;

	if (cpts->len == 0) {
		g_debug ("Skipped writing cache file: No components to serialize.");
		return;
	}

	main_builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);
	builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);

	for (cindex = 0; cindex < cpts->len; cindex++) {
		GVariantBuilder cb;
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts, cindex));

		g_variant_builder_init (&cb, G_VARIANT_TYPE_VARDICT);

		/* name */
		g_variant_builder_add (&cb, "{sv}",
					"name",
					as_variant_mstring_new (as_component_get_name (cpt)));

		/* summary */
		g_variant_builder_add (&cb, "{sv}",
					"summary",
					as_variant_mstring_new (as_component_get_summary (cpt)));

		/* add to component list */
		g_variant_builder_add_value (builder, g_variant_builder_end (&cb));
	}


	/* write basic information and add components */
	g_variant_builder_add (main_builder, "{sv}",
				"locale",
				as_variant_mstring_new (locale));

	g_variant_builder_add (main_builder, "{sv}",
				"components",
				g_variant_builder_end (builder));

	main_gv = g_variant_builder_end (main_builder);
	ofile = g_file_new_for_path (fname);
	g_file_replace_contents (ofile,
				 g_variant_get_data (main_gv),
				 g_variant_get_size (main_gv),
				 NULL, /* entity-tag */
				 FALSE, /* make backup */
				 G_FILE_CREATE_REPLACE_DESTINATION,
				 NULL, /* new entity-tag */
				 NULL, /* cancellable */
				 error);
}

/**
 * as_variant_maybe_string_new:
 *
 * Create a string wrapped in a maybe GVariant.
 */
static gchar*
as_variant_get_mstring (GVariant *variant)
{
	g_autoptr(GVariant) tmp = NULL;

	tmp = g_variant_get_maybe (variant);
	if (tmp == NULL)
		return NULL;

	return g_variant_dup_string (tmp, NULL);
}

/**
 * as_variant_get_dict_str:
 *
 * Create a string wrapped in a maybe GVariant.
 */
static gchar*
as_variant_get_dict_str (GVariantDict *dict, const gchar *key)
{
	g_autoptr(GVariant) val = NULL;

	val = g_variant_dict_lookup_value (dict,
					   key,
					   G_VARIANT_TYPE_MAYBE);
	return as_variant_get_mstring (val);
}

/**
 * as_cache_read:
 * @fname: The file to save the data to.
 * @locale: The locale this cache file is for.
 * @cpts: (element-type AsComponent): The components to serialize.
 * @error: A #GError
 *
 * Serialize components to a cache file and store it on disk.
 */
GPtrArray*
as_cache_file_read (const gchar *fname, GError **error)
{
	g_autoptr(GPtrArray) cpts = NULL;
	g_autoptr(GFile) ifile = NULL;
	g_autoptr(GVariant) main_gv = NULL;
	g_autoptr(GVariant) cptsv_array = NULL;
	g_autoptr(GVariant) cptv = NULL;
	g_autoptr(GVariant) var = NULL;
	g_autofree gchar *locale = NULL;
	GVariantIter iter;
	g_autofree gchar *data = NULL;
	gsize data_size;

	ifile = g_file_new_for_path (fname);
	g_file_load_contents (ifile,
				NULL, /* cancellable */
				&data,
				&data_size,
				NULL, /* etag output */
				error);
	if ((error != NULL) && (*error != NULL))
		return NULL;

	main_gv = g_variant_new_from_data (G_VARIANT_TYPE_VARDICT,
						data,
						data_size,
						TRUE, /* trusted */
						NULL, /* destroy notify cb */
						NULL); /* user data */
	cpts = g_ptr_array_new_with_free_func (g_object_unref);

	var = g_variant_lookup_value (main_gv,
					"locale",
					G_VARIANT_TYPE_MAYBE);
	locale = as_variant_get_mstring (var);

	cptsv_array = g_variant_lookup_value (main_gv,
					      "components",
					      G_VARIANT_TYPE_ARRAY);

	g_variant_iter_init (&iter, cptsv_array);
	while ((cptv = g_variant_iter_next_value (&iter))) {
		GVariantDict dict;
		gchar *str;
		AsComponent *cpt = as_component_new ();

		g_variant_dict_init (&dict, cptv);

		str = as_variant_get_dict_str (&dict, "name");
		as_component_set_name (cpt, str, locale);
		g_free (str);

		/* add to result list */
		g_ptr_array_add (cpts, cpt);
	}

	return cpts;
}
