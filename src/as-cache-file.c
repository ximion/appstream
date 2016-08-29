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

	g_variant_builder_init (&ab, G_VARIANT_TYPE_STRING_ARRAY);
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
	bundle_var = g_variant_new_parsed ("({'type', %u},{'id', %s})",
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
 * as_images_array_to_variant_cb:
 *
 * Helper function to serialize AsImage instances for storage in the cache.
 */
static void
as_images_array_to_variant_cb (AsImage *img, GVariantBuilder *builder)
{
	GVariant *image_var;

	image_var = g_variant_new_parsed ("({'type', %u},{'url', %s},{'width', %i},{'height', %i},{'locale', %v})",
					   as_image_get_kind (img),
					   as_image_get_url (img),
					   as_image_get_width (img),
					   as_image_get_height (img),
					   as_variant_mstring_new (as_image_get_locale (img)));
	g_variant_builder_add_value (builder, image_var);
}

/**
 * as_langs_table_to_variant_cb:
 *
 * Helper function to serialize language completion information for storage in the cache.
 */
static void
as_langs_table_to_variant_cb (const gchar *key, gint value, GVariantBuilder *builder)
{
	g_variant_builder_add (builder, "{su}",
				key, value);
}

/**
 * as_token_table_to_variant_cb:
 *
 * Helper function to serialize search tokens for storage in the cache.
 */
static void
as_token_table_to_variant_cb (const gchar *term, AsTokenType *match_pval, GVariantBuilder *builder)
{
	if (match_pval == NULL)
		return;

	g_variant_builder_add (builder, "{su}",
				      term, *match_pval);
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
					   g_variant_new_strv ((const gchar* const*) as_component_get_pkgnames (cpt),
							       as_component_get_pkgnames (cpt)? -1 : 0));

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
				GVariantBuilder icon_b;


				g_variant_builder_init (&icon_b, G_VARIANT_TYPE_ARRAY);
				g_variant_builder_add_parsed (&icon_b, "{'type', <%u>}", as_icon_get_kind (icon));
				g_variant_builder_add_parsed (&icon_b, "{'width', <%i>}", as_icon_get_width (icon));
				g_variant_builder_add_parsed (&icon_b, "{'height', <%i>}", as_icon_get_height (icon));

				if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
					g_variant_builder_add_parsed (&icon_b, "{'name', <%s>}", as_icon_get_name (icon));
				} else if (as_icon_get_kind (icon) == AS_ICON_KIND_REMOTE) {
					g_variant_builder_add_parsed (&icon_b, "{'url', <%s>}", as_icon_get_url (icon));
				} else {
					/* cached or local icon */
					g_variant_builder_add_parsed (&icon_b, "{'filename', <%s>}", as_icon_get_filename (icon));
				}
				g_variant_builder_add_value (&array_b, g_variant_builder_end (&icon_b));
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

		/* provided items */
		tmp_array_ref = as_component_get_provided (cpt);
		if (tmp_array_ref->len > 0) {
			g_variant_builder_init (&array_b, G_VARIANT_TYPE_ARRAY);
			for (i = 0; i < tmp_array_ref->len; i++) {
				AsProvided *prov = AS_PROVIDED (g_ptr_array_index (tmp_array_ref, i));
				GVariant *prov_var;

				prov_var = g_variant_new ("{uv}",
							  as_provided_get_kind (prov),
							  as_string_ptrarray_to_variant (as_provided_get_items (prov)));
				g_variant_builder_add_value (&array_b, prov_var);
			}

			as_variant_builder_add_kv (&cb, "provided",
						   g_variant_builder_end (&array_b));
		}

		/* screenshots */
		tmp_array_ref = as_component_get_screenshots (cpt);
		if (tmp_array_ref->len > 0) {
			g_variant_builder_init (&array_b, G_VARIANT_TYPE_ARRAY);
			for (i = 0; i < tmp_array_ref->len; i++) {
				AsScreenshot *sshot = AS_SCREENSHOT (g_ptr_array_index (tmp_array_ref, i));
				GVariantBuilder images_b;
				GVariantBuilder scr_b;

				g_variant_builder_init (&images_b, G_VARIANT_TYPE_ARRAY);
				g_ptr_array_foreach (as_screenshot_get_images_all (sshot),
						     (GFunc) as_images_array_to_variant_cb,
						     &images_b);

				g_variant_builder_init (&scr_b, G_VARIANT_TYPE_ARRAY);
				g_variant_builder_add_parsed (&scr_b, "{'type', <%u>}", as_screenshot_get_kind (sshot));
				g_variant_builder_add_parsed (&scr_b, "{'caption', %v}", as_variant_mstring_new (as_screenshot_get_caption (sshot)));
				g_variant_builder_add_parsed (&scr_b, "{'images', %v}", g_variant_builder_end (&images_b));

				g_variant_builder_add_value (&array_b, g_variant_builder_end (&scr_b));
			}

			as_variant_builder_add_kv (&cb, "screenshots",
						   g_variant_builder_end (&array_b));
		}

		/* releases */
		tmp_array_ref = as_component_get_releases (cpt);
		if (tmp_array_ref->len > 0) {
			g_variant_builder_init (&array_b, G_VARIANT_TYPE_ARRAY);
			for (i = 0; i < tmp_array_ref->len; i++) {
				AsRelease *rel = AS_RELEASE (g_ptr_array_index (tmp_array_ref, i));
				guint j;
				GPtrArray *checksums;
				GVariantBuilder checksum_b;
				GVariantBuilder sizes_b;
				GVariantBuilder rel_b;

				GVariant *locations_var;
				GVariant *checksums_var;
				GVariant *sizes_var;
				gboolean have_sizes = FALSE;

				/* build checksum info */
				checksums = as_release_get_checksums (rel);
				g_variant_builder_init (&checksum_b, G_VARIANT_TYPE_DICTIONARY);
				for (j = 0; j < checksums->len; j++) {
					AsChecksum *cs = AS_CHECKSUM (g_ptr_array_index (checksums, j));
					g_variant_builder_add (&checksum_b, "{us}",
								as_checksum_get_kind (cs),
								as_checksum_get_value (cs));
				}

				/* build size info */
				g_variant_builder_init (&sizes_b, G_VARIANT_TYPE_DICTIONARY);
				for (j = 0; j < AS_SIZE_KIND_LAST; j++) {
					if (as_release_get_size (rel, (AsSizeKind) j) > 0) {
						g_variant_builder_add (&sizes_b, "{ut}",
								       (AsSizeKind) j,
								       as_release_get_size (rel, (AsSizeKind) j));
						have_sizes = TRUE;
					}
				}

				locations_var = g_variant_new_maybe (G_VARIANT_TYPE_STRING_ARRAY,
								     as_string_ptrarray_to_variant (as_release_get_locations (rel)));
				checksums_var = g_variant_new_maybe ((const GVariantType *) "a{us}",
								     checksums->len > 0? g_variant_builder_end (&checksum_b) : NULL);
				sizes_var = g_variant_new_maybe ((const GVariantType *) "a{ut}",
								     have_sizes? g_variant_builder_end (&sizes_b) : NULL);

				g_variant_builder_init (&rel_b, G_VARIANT_TYPE_ARRAY);
				g_variant_builder_add_parsed (&rel_b, "{'version', %v}", as_variant_mstring_new (as_release_get_version (rel)));
				g_variant_builder_add_parsed (&rel_b, "{'timestamp', <%t>}", as_release_get_timestamp (rel));
				g_variant_builder_add_parsed (&rel_b, "{'urgency', <%u>}", as_release_get_urgency (rel));
				g_variant_builder_add_parsed (&rel_b, "{'locations', %v}", locations_var);
				g_variant_builder_add_parsed (&rel_b, "{'checksums', %v}", checksums_var);
				g_variant_builder_add_parsed (&rel_b, "{'sizes', %v}", sizes_var);
				g_variant_builder_add_parsed (&rel_b, "{'description', %v}", as_variant_mstring_new (as_release_get_description (rel)));

				g_variant_builder_add_value (&array_b, g_variant_builder_end (&rel_b));
			}

			as_variant_builder_add_kv (&cb, "releases",
						   g_variant_builder_end (&array_b));
		}

		/* languages */
		tmp_table_ref = as_component_get_languages_map (cpt);
		if (g_hash_table_size (tmp_table_ref) > 0) {
			GVariantBuilder dict_b;
			g_variant_builder_init (&dict_b, G_VARIANT_TYPE_DICTIONARY);
			g_hash_table_foreach (tmp_table_ref,
						(GHFunc) as_langs_table_to_variant_cb,
						&dict_b);
			as_variant_builder_add_kv (&cb, "languages",
						   g_variant_builder_end (&dict_b));
		}

		/* search tokens */
		as_component_create_token_cache (cpt);
		tmp_table_ref = as_component_get_token_cache_table (cpt);
		if (g_hash_table_size (tmp_table_ref) > 0) {
			GVariantBuilder dict_b;
			g_variant_builder_init (&dict_b, G_VARIANT_TYPE_DICTIONARY);
			g_hash_table_foreach (tmp_table_ref,
						(GHFunc) as_token_table_to_variant_cb,
						&dict_b);
			as_variant_builder_add_kv (&cb, "tokens",
						   g_variant_builder_end (&dict_b));
		}

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
static const gchar*
as_variant_get_mstring (GVariant **var)
{
	GVariant *tmp;

	if (*var == NULL)
		return NULL;

	tmp = g_variant_get_maybe (*var);
	if (tmp == NULL)
		return NULL;
	g_variant_unref (*var);
	*var = tmp;

	return g_variant_get_string (*var, NULL);
}

/**
 * as_variant_get_dict_mstr:
 *
 * Get a string wrapped in a maybe GVariant from a dictionary.
 */
static const gchar*
as_variant_get_dict_mstr (GVariantDict *dict, const gchar *key, GVariant **var)
{
	*var = g_variant_dict_lookup_value (dict,
					   key,
					   G_VARIANT_TYPE_MAYBE);
	return as_variant_get_mstring (var);
}

/**
 * as_variant_get_dict_str:
 *
 * Get a string from a GVariant dictionary.
 */
static const gchar*
as_variant_get_dict_str (GVariantDict *dict, const gchar *key, GVariant **var)
{
	*var = g_variant_dict_lookup_value (dict,
					   key,
					   G_VARIANT_TYPE_STRING);
	return g_variant_get_string (*var, NULL);
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
 * as_variant_get_dict_int32:
 *
 * Get a an uint32 from a dictionary.
 */
static gint
as_variant_get_dict_int32 (GVariantDict *dict, const gchar *key)
{
	g_autoptr(GVariant) val = NULL;
	val = g_variant_dict_lookup_value (dict,
					   key,
					   G_VARIANT_TYPE_INT32);
	return g_variant_get_int32 (val);
}

/**
 * as_variant_get_dict_strv:
 *
 * Get a strv from a dictionary.
 *
 * returns: (transfer container): A gchar**
 */
static const gchar**
as_variant_get_dict_strv (GVariantDict *dict, const gchar *key, GVariant **var)
{
	*var = g_variant_dict_lookup_value (dict,
					   key,
					   G_VARIANT_TYPE_STRING_ARRAY);
	return g_variant_get_strv (*var, NULL);
}

/**
 * as_variant_to_string_ptrarray:
 *
 * Add contents of array-type variant to string list.
 */
static void
as_variant_to_string_ptrarray (GVariant *var, GPtrArray *dest)
{
	GVariant *child;
	GVariantIter iter;

	g_variant_iter_init (&iter, var);
	while ((child = g_variant_iter_next_value (&iter))) {
		g_ptr_array_add (dest, g_variant_dup_string (child, NULL));
		g_variant_unref (child);
	}
}

/**
 * as_variant_to_string_ptrarray_by_dict:
 *
 * Add contents of array-type variant to string list using a dictionary key
 * to get the source variant.
 */
static void
as_variant_to_string_ptrarray_by_dict (GVariantDict *dict, const gchar *key, GPtrArray *dest)
{
	g_autoptr(GVariant) var = NULL;

	var = g_variant_dict_lookup_value (dict, key, G_VARIANT_TYPE_STRING_ARRAY);
	if (var != NULL)
		as_variant_to_string_ptrarray (var, dest);
}

/**
 * as_variant_to_image:
 *
 * Read image data from a GVariant and return an #AsImage.
 * The variant is dereferenced in after conversion.
 */
static AsImage*
as_variant_to_image (GVariant *variant)
{
	GVariantDict dict;
	GVariant *tmp;
	AsImage *img = as_image_new ();

	g_variant_dict_init (&dict, variant);

	/* kind */
	as_image_set_kind (img, as_variant_get_dict_uint32 (&dict, "type"));

	/* locale */
	as_image_set_locale (img, as_variant_get_dict_mstr (&dict, "locale", &tmp));
	g_variant_unref (tmp);

	/* url */
	as_image_set_url (img, as_variant_get_dict_str (&dict, "url", &tmp));
	g_variant_unref (tmp);

	/* sizes */
	as_image_set_width (img, as_variant_get_dict_int32 (&dict, "width"));
	as_image_set_height (img, as_variant_get_dict_int32 (&dict, "height"));

	g_variant_unref (variant);
	return img;
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
	GPtrArray *cpts = NULL;
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
	const gchar *locale = NULL;
	GVariantIter main_iter;

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
	locale = as_variant_get_mstring (&gmvar);

	cptsv_array = g_variant_lookup_value (main_gv,
					      "components",
					      G_VARIANT_TYPE_ARRAY);

	g_variant_iter_init (&main_iter, cptsv_array);
	while ((cptv = g_variant_iter_next_value (&main_iter))) {
		GVariantDict dict;
		const gchar **strv;
		GVariant *var;
		GVariantIter gvi;

		g_autoptr(AsComponent) cpt = as_component_new ();
		g_variant_dict_init (&dict, cptv);

		/* type */
		as_component_set_kind (cpt,
				       as_variant_get_dict_uint32 (&dict, "type"));

		/* active locale */
		as_component_set_active_locale (cpt, locale);

		/* id */
		as_component_set_id (cpt,
				     as_variant_get_dict_mstr (&dict, "id", &var));
		g_variant_unref (var);

		/* name */
		as_component_set_name (cpt,
				       as_variant_get_dict_mstr (&dict, "name", &var),
				       locale);
		g_variant_unref (var);

		/* summary */
		as_component_set_summary (cpt,
				       as_variant_get_dict_mstr (&dict, "summary", &var),
				       locale);
		g_variant_unref (var);

		/* source package name */
		as_component_set_source_pkgname (cpt,
						as_variant_get_dict_mstr (&dict, "source_pkgname", &var));
		g_variant_unref (var);

		/* package names */
		strv = as_variant_get_dict_strv (&dict, "pkgnames", &var);
		as_component_set_pkgnames (cpt, (gchar **) strv);
		g_free (strv);
		g_variant_unref (var);

		/* origin */
		as_component_set_origin (cpt,
					 as_variant_get_dict_mstr (&dict, "origin", &var));
		g_variant_unref (var);

		/* bundles */
		var = g_variant_dict_lookup_value (&dict, "bundles", G_VARIANT_TYPE_STRING_ARRAY);
		if (var != NULL) {
			GVariant *child;

			g_variant_iter_init (&gvi, var);
			while ((child = g_variant_iter_next_value (&gvi))) {
				GVariantDict tmp_dict;
				GVariant *var2;
				AsBundle *bundle = as_bundle_new ();

				g_variant_dict_init (&tmp_dict, child);
				as_bundle_set_kind (bundle,
						    as_variant_get_dict_uint32 (&tmp_dict, "type"));
				as_bundle_set_id (bundle,
						    as_variant_get_dict_str (&tmp_dict, "id", &var2));
				g_variant_unref (var2);

				g_variant_unref (child);
			}
			g_variant_unref (var);
		}

		/* extends */
		as_variant_to_string_ptrarray_by_dict (&dict,
							"extends",
							as_component_get_extends (cpt));

		/* extensions */
		as_variant_to_string_ptrarray_by_dict (&dict,
							"extensions",
							as_component_get_extensions (cpt));

		/* URLs */
		var = g_variant_dict_lookup_value (&dict,
						   "urls",
						   G_VARIANT_TYPE_DICTIONARY);
		if (var != NULL) {
			GVariant *child;

			g_variant_iter_init (&gvi, var);
			while ((child = g_variant_iter_next_value (&gvi))) {
				AsUrlKind kind;
				g_autoptr(GVariant) url_var = NULL;

				g_variant_get (child, "{uv}", &kind, &url_var);
				as_component_add_url (cpt,
						      kind,
						      g_variant_get_string (url_var, NULL));

				g_variant_unref (child);
			}
			g_variant_unref (var);
		}

		/* icons */
		var = g_variant_dict_lookup_value (&dict,
						   "icons",
						   G_VARIANT_TYPE_ARRAY);
		if (var != NULL) {
			GVariant *child;

			g_variant_iter_init (&gvi, var);
			while ((child = g_variant_iter_next_value (&gvi))) {
				GVariantDict idict;
				AsIconKind kind;
				g_autoptr(GVariant) ival_var = NULL;
				g_autoptr(AsIcon) icon = as_icon_new ();

				g_variant_dict_init (&idict, child);

				kind = as_variant_get_dict_uint32 (&idict, "type");
				as_icon_set_kind (icon, kind);

				as_icon_set_width (icon,
						   as_variant_get_dict_int32 (&idict, "width"));
				as_icon_set_height (icon,
						   as_variant_get_dict_int32 (&idict, "height"));

				if (kind == AS_ICON_KIND_STOCK) {
					as_icon_set_name (icon,
							  as_variant_get_dict_str (&idict, "name", &ival_var));
				} else if (kind == AS_ICON_KIND_REMOTE) {
					as_icon_set_url (icon,
							  as_variant_get_dict_str (&idict, "url", &ival_var));
				} else {
					/* cached or local icon */
					as_icon_set_filename (icon,
								as_variant_get_dict_str (&idict, "filename", &ival_var));
				}

				as_component_add_icon (cpt, icon);
				g_print ("I : %s\n", as_icon_get_filename (icon));

				g_variant_unref (child);
			}
			g_variant_unref (var);
		}

		/* long description */
		as_component_set_description (cpt,
						as_variant_get_dict_mstr (&dict, "description", &var),
						locale);
		g_variant_unref (var);

		/* categories */
		as_variant_to_string_ptrarray_by_dict (&dict,
							"categories",
							as_component_get_categories (cpt));

		/* compulsory-for-desktop */
		as_variant_to_string_ptrarray_by_dict (&dict,
							"compulsory_for",
							as_component_get_compulsory_for_desktops (cpt));

		/* project license */
		as_component_set_project_license (cpt, as_variant_get_dict_mstr (&dict, "project_license", &var));
		g_variant_unref (var);

		/* project group */
		as_component_set_project_group (cpt, as_variant_get_dict_mstr (&dict, "project_group", &var));
		g_variant_unref (var);

		/* developer name */
		as_component_set_developer_name (cpt,
						 as_variant_get_dict_mstr (&dict, "developer_name", &var),
						 locale);
		g_variant_unref (var);


		/* provided items */
		var = g_variant_dict_lookup_value (&dict,
						   "provided",
						   G_VARIANT_TYPE_ARRAY);
		if (var != NULL) {
			GVariant *child;

			g_variant_iter_init (&gvi, var);
			while ((child = g_variant_iter_next_value (&gvi))) {
				AsProvidedKind kind;
				GVariantIter inner_iter;
				GVariant *item_child;
				g_autoptr(GVariant) items_var = NULL;
				g_autoptr(AsProvided) prov = as_provided_new ();

				g_variant_get (child, "{uv}", &kind, &items_var);
				as_provided_set_kind (prov, kind);

				g_variant_iter_init (&inner_iter, items_var);
				while ((item_child = g_variant_iter_next_value (&inner_iter))) {
					as_provided_add_item (prov, g_variant_get_string (item_child, NULL));
					g_variant_unref (item_child);
				}

				as_component_add_provided (cpt, prov);
				g_variant_unref (child);
			}
			g_variant_unref (var);
		}

		/* screenshots */
		var = g_variant_dict_lookup_value (&dict,
						   "screenshots",
						   G_VARIANT_TYPE_ARRAY);
		if (var != NULL) {
			GVariant *child;

			g_variant_iter_init (&gvi, var);
			while ((child = g_variant_iter_next_value (&gvi))) {
				GVariantIter inner_iter;
				GVariantDict idict;
				GVariant *tmp;
				g_autoptr(GVariant) images_var = NULL;
				g_autoptr(AsScreenshot) scr = as_screenshot_new ();

				as_screenshot_set_active_locale (scr, locale);
				g_variant_dict_init (&idict, child);

				as_screenshot_set_kind (scr, as_variant_get_dict_uint32 (&idict, "type"));
				as_screenshot_set_caption (scr,
							   as_variant_get_dict_mstr (&idict, "caption", &tmp),
							   locale);
				g_variant_unref (tmp);

				images_var = g_variant_dict_lookup_value (&idict, "images", G_VARIANT_TYPE_VARIANT);
				if (images_var != NULL) {
					GVariant *img_child;
					g_variant_iter_init (&inner_iter, images_var);

					while ((img_child = g_variant_iter_next_value (&inner_iter))) {
						g_autoptr(AsImage) img = as_variant_to_image (img_child);
						as_screenshot_add_image (scr, img);
					}

					as_component_add_screenshot (cpt, scr);
				}

				g_variant_unref (child);
			}
			g_variant_unref (var);
		}

		/* releases */
		var = g_variant_dict_lookup_value (&dict,
						   "releases",
						   G_VARIANT_TYPE_ARRAY);
		if (var != NULL) {
			GVariant *child;

			g_variant_iter_init (&gvi, var);
			while ((child = g_variant_iter_next_value (&gvi))) {
				GVariant *tmp;
				GVariantDict rdict;
				GVariantIter riter;
				GVariant *inner_child;;
				g_autoptr(AsRelease) rel = as_release_new ();

				as_release_set_active_locale (rel, locale);
				g_variant_dict_init (&rdict, child);

				as_release_set_version (rel, as_variant_get_dict_mstr (&rdict, "version", &tmp));
				g_variant_unref (tmp);

				tmp = g_variant_dict_lookup_value (&rdict, "timestamp", G_VARIANT_TYPE_UINT64);
				as_release_set_timestamp (rel, g_variant_get_uint64 (tmp));
				g_variant_unref (tmp);

				as_release_set_urgency (rel, as_variant_get_dict_uint32 (&rdict, "urgency"));

				as_release_set_description (rel,
							    as_variant_get_dict_mstr (&rdict, "description", &tmp),
							    locale);
				g_variant_unref (tmp);

				/* sizes */
				tmp = g_variant_dict_lookup_value (&rdict, "sizes", G_VARIANT_TYPE_VARIANT);
				if (tmp != NULL) {
					g_variant_iter_init (&riter, tmp);
					while ((inner_child = g_variant_iter_next_value (&riter))) {
						AsSizeKind kind;
						guint64 size;

						g_variant_get (inner_child, "{ut}", &kind, &size);
						as_release_set_size (rel, kind, size);

						g_variant_unref (inner_child);
					}
					g_variant_unref (tmp);
				}

				/* checksums */
				tmp = g_variant_dict_lookup_value (&rdict, "checksums", G_VARIANT_TYPE_VARIANT);
				if (tmp != NULL) {
					g_variant_iter_init (&riter, var);
					while ((inner_child = g_variant_iter_next_value (&riter))) {
						AsChecksumKind kind;
						g_autofree gchar *value;
						g_autoptr(AsChecksum) cs = as_checksum_new ();

						g_variant_get (inner_child, "{us}", &kind, &value);
						as_checksum_set_kind (cs, kind);
						as_checksum_set_value (cs, value);
						as_release_add_checksum (rel, cs);

						g_variant_unref (inner_child);
					}
					g_variant_unref (tmp);
				}

				as_component_add_release (cpt, rel);
				g_variant_unref (child);
			}
			g_variant_unref (var);
		}

		/* languages */
		var = g_variant_dict_lookup_value (&dict,
						   "languages",
						   G_VARIANT_TYPE_DICTIONARY);
		if (var != NULL) {
			GVariant *child;

			g_variant_iter_init (&gvi, var);
			while ((child = g_variant_iter_next_value (&gvi))) {
				guint percentage;
				g_autofree gchar *lang;

				g_variant_get (child, "{su}", &lang, &percentage);
				as_component_add_language (cpt, lang, percentage);

				g_variant_unref (child);
			}
			g_variant_unref (var);
		}

		/* search tokens */
		var = g_variant_dict_lookup_value (&dict,
						   "tokens",
						   G_VARIANT_TYPE_DICTIONARY);
		if (var != NULL) {
			GVariant *child;
			GHashTable *token_cache;
			gboolean tokens_added = FALSE;

			token_cache = as_component_get_token_cache_table (cpt);
			g_variant_iter_init (&gvi, var);
			while ((child = g_variant_iter_next_value (&gvi))) {
				guint score;
				gchar *token;
				AsTokenType *match_pval;

				g_variant_get (child, "{su}", &token, &score);

				match_pval = g_new0 (AsTokenType, 1);
				*match_pval = score;

				g_hash_table_insert (token_cache,
							token,
							match_pval);
				tokens_added = TRUE;

				g_variant_unref (child);
			}

			/* we added things to the token cache, so we just assume it's valid */
			if (tokens_added)
				as_component_set_token_cache_valid (cpt, TRUE);

			g_variant_unref (var);
		}

		/* add to result list */
		if (as_component_is_valid (cpt)) {
			g_ptr_array_add (cpts, g_object_ref (cpt));
		} else {
			g_autofree gchar *str = as_component_to_string (cpt);
			g_warning ("Ignored serialized component: %s", str);
		}
	}

	return cpts;
}
