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

#define CACHE_FORMAT_VERSION 1

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
 * as_variant_builder_add_kv:
 *
 * Add key/value pair with a string key and variant value.
 */
static void
as_variant_builder_add_kv (GVariantBuilder *builder, const gchar *key, GVariant *value)
{
	if (value == NULL)
		return;
	g_variant_builder_add (builder, "{sv}", key, value);
}

/**
 * as_string_ptrarray_to_variant:
 *
 * Add key/value pair with a string key and variant value.
 */
static GVariant*
as_string_ptrarray_to_variant (GPtrArray *strarray)
{
	GVariantBuilder ab;
	guint i;

	if ((strarray == NULL) || (strarray->len == 0))
		return NULL;

	g_variant_builder_init (&ab, G_VARIANT_TYPE_ARRAY);
	for (i = 0; i < strarray->len; i++) {
		const gchar *str = (const gchar*) g_ptr_array_index (strarray, i);
		g_variant_builder_add_value (&ab, g_variant_new_string (str));
	}

	return g_variant_builder_end (&ab);
}

/**
 * as_bundle_table_to_variant_cb:
 *
 * Helper function to serialize bundle data for storage in the cache.
 */
static void
as_bundle_table_to_variant_cb (gpointer bkind_ptr, AsBundle *bundle, GVariantBuilder *builder)
{
	GVariant *bundle_var;
	bundle_var = g_variant_new_parsed ("({'type', %i},{'id', %s})",
					   as_bundle_get_kind (bundle),
					   as_bundle_get_id (bundle));
	g_variant_builder_add_value (builder, bundle_var);
}

/**
 * as_url_table_to_variant_cb:
 *
 * Helper function to serialize URLs data for storage in the cache.
 */
static void
as_url_table_to_variant_cb (gpointer ukind_ptr, const gchar *value, GVariantBuilder *builder)
{
	g_variant_builder_add (builder, "{uv}",
				(AsUrlKind) GPOINTER_TO_INT (ukind_ptr),
				g_variant_new_string (value));
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
	g_autoptr(GFileOutputStream) file_out = NULL;
	g_autoptr(GOutputStream) zout = NULL;
	g_autoptr(GZlibCompressor) compressor = NULL;
	GError *tmp_error = NULL;
	guint cindex;

	if (cpts->len == 0) {
		g_debug ("Skipped writing cache file: No components to serialize.");
		return;
	}

	main_builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);
	builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);

	for (cindex = 0; cindex < cpts->len; cindex++) {
		GVariantBuilder cb;
		GVariantBuilder array_b;
		GHashTable *tmp_table_ref;
		GPtrArray *tmp_array_ref;
		guint i;
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts, cindex));

		/* sanity checks */
		if (!as_component_is_valid (cpt)) {
			/* we should *never* get here, all invalid stuff should be filtered out at this point */
			g_critical ("Skipped component '%s' from inclusion into the cache: The component is invalid.",
					   as_component_get_id (cpt));
			continue;
		}

		if (as_component_get_merge_kind (cpt) != AS_MERGE_KIND_NONE) {
			g_debug ("Skipping '%s' from cache inclusion, it is a merge component.",
				 as_component_get_id (cpt));
			continue;
		}

		/* start serializing our component */
		g_variant_builder_init (&cb, G_VARIANT_TYPE_VARDICT);


		/* type */
		as_variant_builder_add_kv (&cb, "type",
					g_variant_new_uint32 (as_component_get_kind (cpt)));

		/* id */
		as_variant_builder_add_kv (&cb, "id",
					as_variant_mstring_new (as_component_get_id (cpt)));

		/* name */
		as_variant_builder_add_kv (&cb, "name",
					as_variant_mstring_new (as_component_get_name (cpt)));

		/* summary */
		as_variant_builder_add_kv (&cb, "summary",
					as_variant_mstring_new (as_component_get_summary (cpt)));

		/* source package name */
		as_variant_builder_add_kv (&cb, "source_pkgname",
					as_variant_mstring_new (as_component_get_source_pkgname (cpt)));

		/* package name */
		as_variant_builder_add_kv (&cb, "pkgnames",
					   g_variant_new_strv ((const gchar* const*) as_component_get_pkgnames (cpt), -1));

		/* origin */
		as_variant_builder_add_kv (&cb, "origin",
					as_variant_mstring_new (as_component_get_origin (cpt)));

		/* bundles */
		tmp_table_ref = as_component_get_bundles_table (cpt);
		if (g_hash_table_size (tmp_table_ref) > 0) {
			g_variant_builder_init (&array_b, G_VARIANT_TYPE_ARRAY);
			g_hash_table_foreach (tmp_table_ref,
						(GHFunc) as_bundle_table_to_variant_cb,
						&array_b);
			as_variant_builder_add_kv (&cb, "bundles",
						   g_variant_builder_end (&array_b));
		}

		/* extends */
		as_variant_builder_add_kv (&cb, "extends",
					   as_string_ptrarray_to_variant (as_component_get_extends (cpt)));

		/* extensions */
		as_variant_builder_add_kv (&cb, "extensions",
					   as_string_ptrarray_to_variant (as_component_get_extensions (cpt)));

		/* URLs */
		tmp_table_ref = as_component_get_urls_table (cpt);
		if (g_hash_table_size (tmp_table_ref) > 0) {
			GVariantBuilder dict_b;
			g_variant_builder_init (&dict_b, G_VARIANT_TYPE_DICTIONARY);
			g_hash_table_foreach (tmp_table_ref,
						(GHFunc) as_url_table_to_variant_cb,
						&dict_b);
			as_variant_builder_add_kv (&cb, "urls",
						   g_variant_builder_end (&dict_b));
		}

		/* icons */
		tmp_array_ref = as_component_get_icons (cpt);
		if (tmp_array_ref->len > 0) {
			g_variant_builder_init (&array_b, G_VARIANT_TYPE_ARRAY);
			for (i = 0; i < tmp_array_ref->len; i++) {
				AsIcon *icon = AS_ICON (g_ptr_array_index (tmp_array_ref, i));
				GVariant *icon_var;

				if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
					icon_var = g_variant_new_parsed ("({'type', %i},{'name', %s},{'width', %i},{'height', %i})",
									 AS_ICON_KIND_STOCK,
									 as_icon_get_name (icon),
									 as_icon_get_width (icon),
									 as_icon_get_height (icon));
				} else if (as_icon_get_kind (icon) == AS_ICON_KIND_REMOTE) {
					icon_var = g_variant_new_parsed ("({'type', %i},{'url', %s},{'width', %i},{'height', %i})",
									 AS_ICON_KIND_REMOTE,
									 as_icon_get_url (icon),
									 as_icon_get_width (icon),
									 as_icon_get_height (icon));
				} else {
					/* cached or local icon */
					icon_var = g_variant_new_parsed ("({'type', %i},{'filename', %s},{'width', %i},{'height', %i})",
									 as_icon_get_kind (icon),
									 as_icon_get_filename (icon),
									 as_icon_get_width (icon),
									 as_icon_get_height (icon));
				}
				g_variant_builder_add_value (&array_b, icon_var);
			}

			as_variant_builder_add_kv (&cb, "icons",
						   g_variant_builder_end (&array_b));
		}

		/* long description */
		as_variant_builder_add_kv (&cb, "description",
					as_variant_mstring_new (as_component_get_description (cpt)));

		/* categories */
		as_variant_builder_add_kv (&cb, "categories",
					   as_string_ptrarray_to_variant (as_component_get_categories (cpt)));


		/* compulsory-for-desktop */
		as_variant_builder_add_kv (&cb, "compulsory_for",
					   as_string_ptrarray_to_variant (as_component_get_compulsory_for_desktops (cpt)));

		/* project license */
		as_variant_builder_add_kv (&cb, "project_license",
					as_variant_mstring_new (as_component_get_project_license (cpt)));

		/* project group */
		as_variant_builder_add_kv (&cb, "project_group",
					as_variant_mstring_new (as_component_get_project_group (cpt)));

		/* developer name */
		as_variant_builder_add_kv (&cb, "developer_name",
					as_variant_mstring_new (as_component_get_developer_name (cpt)));

		/* add to component list */
		g_variant_builder_add_value (builder, g_variant_builder_end (&cb));
	}


	/* write basic information and add components */
	g_variant_builder_add (main_builder, "{sv}",
				"format_version",
				g_variant_new_uint32 (CACHE_FORMAT_VERSION));
	g_variant_builder_add (main_builder, "{sv}",
				"locale",
				as_variant_mstring_new (locale));

	g_variant_builder_add (main_builder, "{sv}",
				"components",
				g_variant_builder_end (builder));
	main_gv = g_variant_builder_end (main_builder);

	ofile = g_file_new_for_path (fname);
	compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
	file_out = g_file_replace (ofile,
				   NULL, /* entity-tag */
				   FALSE, /* make backup */
				   G_FILE_CREATE_REPLACE_DESTINATION,
				   NULL, /* cancellable */
				   error);
	if ((error != NULL) && (*error != NULL))
		return;

	zout = g_converter_output_stream_new (G_OUTPUT_STREAM (file_out), G_CONVERTER (compressor));
	if (!g_output_stream_write_all (zout,
					g_variant_get_data (main_gv),
					g_variant_get_size (main_gv),
					NULL, NULL, &tmp_error)) {
		g_set_error (error,
			     AS_POOL_ERROR,
			     AS_POOL_ERROR_FAILED,
			     "Failed to write stream: %s",
			     tmp_error->message);
		g_error_free (tmp_error);
		return;
	}
	if (!g_output_stream_close (zout, NULL, &tmp_error)) {
		g_set_error (error,
			     AS_POOL_ERROR,
			     AS_POOL_ERROR_FAILED,
			     "Failed to close stream: %s",
			     tmp_error->message);
		g_error_free (tmp_error);
		return;
	}
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
 * Get a string wrapped in a maybe GVariant from a dictionary.
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
 * as_variant_get_dict_uint32:
 *
 * Get a an uint32 from a dictionary.
 */
static guint32
as_variant_get_dict_uint32 (GVariantDict *dict, const gchar *key)
{
	g_autoptr(GVariant) val = NULL;

	val = g_variant_dict_lookup_value (dict,
					   key,
					   G_VARIANT_TYPE_UINT32);
	return g_variant_get_uint32 (val);
}

/**
 * as_variant_get_dict_strv:
 *
 * Get a strv from a dictionary.
 *
 * returns: (transfer container): A gchar**
 */
static const gchar**
as_variant_get_dict_strv (GVariantDict *dict, const gchar *key)
{
	g_autoptr(GVariant) val = NULL;

	val = g_variant_dict_lookup_value (dict,
					   key,
					   G_VARIANT_TYPE_STRING_ARRAY);
	return g_variant_get_strv (val, NULL);
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
	g_autoptr(GInputStream) file_stream = NULL;
	g_autoptr(GInputStream) stream_data = NULL;
	g_autoptr(GConverter) conv = NULL;

	GByteArray *byte_array;
	g_autoptr(GBytes) bytes = NULL;
	gssize len;
	const gsize buffer_size = 1024 * 32;
	g_autofree guint8 *buffer = NULL;

	g_autoptr(GVariant) main_gv = NULL;
	g_autoptr(GVariant) cptsv_array = NULL;
	g_autoptr(GVariant) cptv = NULL;
	g_autoptr(GVariant) gmvar = NULL;
	g_autofree gchar *locale = NULL;
	GVariantIter iter;

	ifile = g_file_new_for_path (fname);

	file_stream = G_INPUT_STREAM (g_file_read (ifile, NULL, error));
	if (file_stream == NULL)
		return NULL;

	/* decompress the GZip stream */
	conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
	stream_data = g_converter_input_stream_new (file_stream, conv);

	buffer = g_malloc (buffer_size);
	byte_array = g_byte_array_new ();
	while ((len = g_input_stream_read (stream_data, buffer, buffer_size, NULL, error)) > 0) {
		g_byte_array_append (byte_array, buffer, len);
	}
	bytes = g_byte_array_free_to_bytes (byte_array);

	/* check if there was an error */
	if (len < 0)
		return NULL;
	if ((error != NULL) && (*error != NULL))
		return NULL;

	main_gv = g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, bytes, TRUE);
	cpts = g_ptr_array_new_with_free_func (g_object_unref);

	gmvar = g_variant_lookup_value (main_gv,
					"format_version",
					G_VARIANT_TYPE_UINT32);
	if ((gmvar == NULL) || (g_variant_get_uint32 (gmvar) != CACHE_FORMAT_VERSION)) {
		/* don't try to load incompatible cache versions */
		if (gmvar == NULL)
			g_warning ("Skipped loading of broken cache file '%s'.", fname);
		else
			g_warning ("Skipped loading of incompatible or broken cache file '%s': Format is %i (expected %i)",
					fname, g_variant_get_uint32 (gmvar), CACHE_FORMAT_VERSION);

		/* TODO: Maybe emit a proper GError? */
		return NULL;
	}

	g_variant_unref (gmvar);
	gmvar = g_variant_lookup_value (main_gv,
					"locale",
					G_VARIANT_TYPE_MAYBE);
	locale = as_variant_get_mstring (gmvar);

	cptsv_array = g_variant_lookup_value (main_gv,
					      "components",
					      G_VARIANT_TYPE_ARRAY);

	g_variant_iter_init (&iter, cptsv_array);
	while ((cptv = g_variant_iter_next_value (&iter))) {
		GVariantDict dict;
		gchar *str;
		const gchar **strv;
		AsComponent *cpt = as_component_new ();

		g_variant_dict_init (&dict, cptv);

		/* type */
		as_component_set_kind (cpt,
				       as_variant_get_dict_uint32 (&dict, "type"));

		/* id */
		str = as_variant_get_dict_str (&dict, "id");
		as_component_set_id (cpt, str);
		g_free (str);

		/* name */
		str = as_variant_get_dict_str (&dict, "name");
		as_component_set_name (cpt, str, locale);
		g_free (str);

		/* summary */
		str = as_variant_get_dict_str (&dict, "summary");
		as_component_set_name (cpt, str, locale);
		g_free (str);

		/* source package name */
		str = as_variant_get_dict_str (&dict, "source_pkgname");
		as_component_set_source_pkgname (cpt, str);
		g_free (str);

		/* package names */
		strv = as_variant_get_dict_strv (&dict, "pkgnames");
		as_component_set_pkgnames (cpt, (gchar **) strv);
		g_free (strv);

		/* origin */
		str = as_variant_get_dict_str (&dict, "origin");
		as_component_set_origin (cpt, str);
		g_free (str);

		/* add to result list */
		g_ptr_array_add (cpts, cpt);
	}

	return cpts;
}
