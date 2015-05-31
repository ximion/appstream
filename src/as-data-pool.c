/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:as-data-pool
 * @short_description: Collect and temporarily store metadata from different sources
 *
 * This class contains a temporary pool of metadata which has been collected from different
 * sources on the system.
 * It can directly be used, but usually it is accessed through a #AsDatabase instance.
 * This class is used by internally by the cache builder, but might be useful for others.
 *
 * See also: #AsDatabase
 */

#include "config.h"
#include "as-data-pool.h"

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-component-private.h"
#include "as-distro-details.h"
#include "as-settings-private.h"

#include "as-metadata.h"
#include "as-metadata-private.h"
#ifdef DEBIAN_DEP11
#include "as-dep11.h"
#endif

const gchar *AS_APPSTREAM_XML_PATHS[4] = {AS_APPSTREAM_BASE_PATH "/xmls",
										"/var/cache/app-info/xmls",
										"/var/lib/app-info/xmls",
										NULL};
const gchar *AS_APPSTREAM_DEP11_PATHS[4] = {AS_APPSTREAM_BASE_PATH "/yaml",
										"/var/cache/app-info/yaml",
										"/var/lib/app-info/yaml",
										NULL};

typedef struct _AsDataPoolPrivate	AsDataPoolPrivate;
struct _AsDataPoolPrivate
{
	GHashTable* cpt_table;
	GPtrArray* providers;
	gchar *scr_base_url;
	gchar *locale;

	gchar **asxml_paths;
	gchar **dep11_paths;

	gchar **icon_paths;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsDataPool, as_data_pool, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_data_pool_get_instance_private (o))

/**
 * as_data_pool_finalize:
 **/
static void
as_data_pool_finalize (GObject *object)
{
	AsDataPool *dpool = AS_DATA_POOL (object);
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	g_ptr_array_unref (priv->providers);
	g_free (priv->scr_base_url);
	g_hash_table_unref (priv->cpt_table);

	g_strfreev (priv->asxml_paths);
	g_strfreev (priv->dep11_paths);

	g_strfreev (priv->icon_paths);

	G_OBJECT_CLASS (as_data_pool_parent_class)->finalize (object);
}

/**
 * as_data_pool_init:
 **/
static void
as_data_pool_init (AsDataPool *dpool)
{
}

/**
 * as_data_pool_class_init:
 **/
static void
as_data_pool_class_init (AsDataPoolClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_data_pool_finalize;
}

static void
as_data_pool_add_new_component (AsDataPool *dpool, AsComponent *cpt)
{
	const gchar *cpt_id;
	AsComponent *existing_cpt;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_return_if_fail (cpt != NULL);

	cpt_id = as_component_get_id (cpt);
	existing_cpt = g_hash_table_lookup (priv->cpt_table, cpt_id);

	/* add additional data to the component, e.g. external screenshots. Also refines
	 * the component's icon paths */
	as_component_complete (cpt, priv->scr_base_url, priv->icon_paths);

	if (existing_cpt) {
		int priority;
		priority = as_component_get_priority (existing_cpt);
		if (priority < as_component_get_priority (cpt)) {
			g_hash_table_replace (priv->cpt_table,
								  g_strdup (cpt_id),
								  g_object_ref (cpt));
		} else {
			g_debug ("Detected colliding ids: %s was already added.", cpt_id);
		}
	} else {
		g_hash_table_insert (priv->cpt_table,
							g_strdup (cpt_id),
							g_object_ref (cpt));
	}
}

/**
 * as_data_pool_get_watched_locations:
 * @dpool: a valid #AsDataPool instance
 *
 * Return a list of all locations which are searched for metadata.
 *
 * Returns: (transfer full): A string-list of watched (absolute) filepaths
 **/
gchar**
as_data_pool_get_watched_locations (AsDataPool *dpool)
{
	guint i;
	GPtrArray *res_array;
	gchar **res;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_return_val_if_fail (dpool != NULL, NULL);

	res_array = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; priv->asxml_paths[i] != NULL; i++) {
		g_ptr_array_add (res_array, g_strdup (priv->asxml_paths[i]));
	}
#ifdef DEBIAN_DEP11
	for (i = 0; priv->dep11_paths[i] != NULL; i++) {
		g_ptr_array_add (res_array, g_strdup (priv->dep11_paths[i]));
	}
#endif

	res = as_ptr_array_to_strv (res_array);
	g_ptr_array_unref (res_array);
	return res;
}

/**
 * as_data_pool_read_compressed_file:
 */
static gchar*
as_data_pool_read_compressed_file (AsDataPool *dpool, GFile* infile)
{
	GFileInputStream* src_stream;
	GMemoryOutputStream* mem_os;
	GInputStream *conv_stream;
	GZlibDecompressor* zdecomp;
	guint8* data;
	gchar *res;

	g_return_val_if_fail (infile != NULL, FALSE);

	src_stream = g_file_read (infile, NULL, NULL);
	mem_os = (GMemoryOutputStream*) g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
	zdecomp = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
	conv_stream = g_converter_input_stream_new (G_INPUT_STREAM (src_stream), G_CONVERTER (zdecomp));
	g_object_unref (zdecomp);

	g_output_stream_splice ((GOutputStream*) mem_os, conv_stream, 0, NULL, NULL);
	data = g_memory_output_stream_get_data (mem_os);
	res = g_strdup ((const gchar*) data);

	g_object_unref (conv_stream);
	g_object_unref (mem_os);
	g_object_unref (src_stream);
	return res;
}

/**
 * as_data_pool_read_file:
 */
static gchar*
as_data_pool_read_file (AsDataPool *dpool, GFile* infile)
{
	gchar* data;
	gchar* line = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;
	GString *str = NULL;

	g_return_val_if_fail (infile != NULL, FALSE);

	ir = g_file_read (infile, NULL, NULL);
	dis = g_data_input_stream_new ((GInputStream*) ir);
	g_object_unref (ir);

	str = g_string_new ("");
	while (TRUE) {
		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		if (str->len > 0)
			g_string_append (str, "\n");
		g_string_append_printf (str, "%s\n", line);
		g_free (line);
	}

	data = g_string_free (str, FALSE);
	g_object_unref (dis);

	return data;
}

/**
 * as_data_pool_process_asxml_document:
 */
static gboolean
as_data_pool_process_asxml_document (AsDataPool *dpool, const gchar* xmldoc_str)
{
	xmlDoc* doc;
	xmlNode* root;
	xmlNode* iter;
	AsMetadata *metad;
	AsComponent *cpt;
	gchar *origin;
	GError *error = NULL;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	g_return_val_if_fail (xmldoc_str != NULL, FALSE);

	doc = xmlParseDoc ((xmlChar*) xmldoc_str);
	if (doc == NULL) {
		fprintf (stderr, "%s\n", "Could not parse XML!");
		return FALSE;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL) {
		fprintf (stderr, "%s\n", "The XML document is empty.");
		return FALSE;
	}

	if (g_strcmp0 ((gchar*) root->name, "components") != 0) {
		fprintf (stderr, "%s\n", "XML file does not contain valid AppStream data!");
		return FALSE;
	}

	metad = as_metadata_new ();
	as_metadata_set_parser_mode (metad, AS_PARSER_MODE_DISTRO);
	as_metadata_set_locale (metad, priv->locale);

	/* set the proper origin of this data */
	origin = (gchar*) xmlGetProp (root, (xmlChar*) "origin");
	as_metadata_set_origin (metad, origin);
	g_free (origin);

	for (iter = root->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "component") == 0) {
			cpt = as_metadata_parse_component_node (metad, iter, FALSE, &error);
			if (error != NULL) {
				g_debug ("WARNING: %s", error->message);
				g_error_free (error);
				error = NULL;
			} else if (cpt != NULL) {
				as_data_pool_add_new_component (dpool, cpt);
				g_object_unref (cpt);
			}
		}
	}
	xmlFreeDoc (doc);
	g_object_unref (metad);

	return TRUE;
}

/**
 * as_data_pool_read_asxml:
 */
static gboolean
as_data_pool_read_asxml (AsDataPool* dpool)
{
	GPtrArray* xml_files;
	guint i;
	GFile *infile;
	gboolean ret = TRUE;
	const gchar *content_type;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	xml_files = g_ptr_array_new_with_free_func (g_free);

	if ((priv->asxml_paths == NULL) || (priv->asxml_paths[0] == NULL))
		return TRUE;

	for (i = 0; priv->asxml_paths[i] != NULL; i++) {
		gchar *path;
		GPtrArray *xmls;
		guint j;
		path = priv->asxml_paths[i];

		if (!g_file_test (path, G_FILE_TEST_EXISTS))
			continue;

		xmls = as_utils_find_files_matching (path, "*.xml*", FALSE);
		if (xmls == NULL)
			continue;
		for (j = 0; j < xmls->len; j++) {
			const gchar *val;
			val = (const gchar *) g_ptr_array_index (xmls, j);
			g_ptr_array_add (xml_files, g_strdup (val));
		}

		g_ptr_array_unref (xmls);
	}

	/* check if we have XML data */
	if (xml_files->len == 0) {
		g_ptr_array_unref (xml_files);
		return TRUE;
	}

	for (i = 0; i < xml_files->len; i++) {
		gchar *fname;
		GFileInfo *info = NULL;
		gchar *data = NULL;

		fname = (gchar*) g_ptr_array_index (xml_files, i);
		infile = g_file_new_for_path (fname);
		if (!g_file_query_exists (infile, NULL)) {
			g_warning ("File '%s' does not exist.", fname);
			g_object_unref (infile);
			continue;
		}

		info = g_file_query_info (infile,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
		if (info == NULL) {
			g_debug ("No info for file '%s' found, file was skipped.", fname);
			g_object_unref (infile);
			continue;
		}
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
		if ((g_strcmp0 (content_type, "application/xml") == 0) || (g_strcmp0 (content_type, "text/plain") == 0)) {
			data = as_data_pool_read_file (dpool, infile);
		} else if (g_strcmp0 (content_type, "application/gzip") == 0 ||
				g_strcmp0 (content_type, "application/x-gzip") == 0) {
			data = as_data_pool_read_compressed_file (dpool, infile);
		} else {
			g_warning ("Invalid file of type '%s' found. File '%s' was skipped.", content_type, fname);
		}
		g_object_unref (info);
		g_object_unref (infile);

		if (data != NULL) {
			ret = as_data_pool_process_asxml_document (dpool, data);
			g_free (data);
		}

		if (!ret)
			break;
	}
	g_ptr_array_unref (xml_files);

	return ret;
}

#ifdef DEBIAN_DEP11
/**
 * as_data_pool_read_dep11:
 */
static gboolean
as_data_pool_read_dep11 (AsDataPool *dpool)
{
	AsDEP11 *dep11;
	GPtrArray* yaml_files;
	guint i;
	GFile *infile;
	gboolean ret = TRUE;
	const gchar *content_type;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	dep11 = as_dep11_new ();
	yaml_files = g_ptr_array_new_with_free_func (g_free);

	if ((priv->dep11_paths == NULL) || (priv->dep11_paths[0] == NULL))
		return TRUE;

	for (i = 0; priv->dep11_paths[i] != NULL; i++) {
		gchar *path;
		GPtrArray *yamls;
		guint j;
		path = priv->dep11_paths[i];

		if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
			continue;
		}

		yamls = as_utils_find_files_matching (path, "*.yml*", FALSE);
		if (yamls == NULL)
			continue;
		for (j = 0; j < yamls->len; j++) {
			const gchar *val;
			val = (const gchar *) g_ptr_array_index (yamls, j);
			g_ptr_array_add (yaml_files, g_strdup (val));
		}

		g_ptr_array_unref (yamls);
	}

	/* do we have metadata at all? */
	if (yaml_files->len == 0) {
		g_ptr_array_unref (yaml_files);
		return TRUE;
	}

	ret = TRUE;
	for (i = 0; i < yaml_files->len; i++) {
		gchar *fname;
		GFileInfo *info = NULL;
		gchar *data = NULL;
		GError *tmp_error = NULL;

		fname = (gchar*) g_ptr_array_index (yaml_files, i);
		infile = g_file_new_for_path (fname);
		if (!g_file_query_exists (infile, NULL)) {
			g_warning ("File '%s' does not exist.", fname);
			g_object_unref (infile);
			continue;
		}

		info = g_file_query_info (infile,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
		if (info == NULL) {
			g_debug ("No info for file '%s' found, file was skipped.", fname);
			g_object_unref (infile);
			continue;
		}
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
		if ((g_strcmp0 (content_type, "application/x-yaml") == 0) || (g_strcmp0 (content_type, "text/plain") == 0)) {
			data = as_data_pool_read_file (dpool, infile);
		} else if (g_strcmp0 (content_type, "application/gzip") == 0 ||
				g_strcmp0 (content_type, "application/x-gzip") == 0) {
			data = as_data_pool_read_compressed_file (dpool, infile);
		} else {
			g_warning ("Invalid file of type '%s' found. File '%s' was skipped.", content_type, fname);
		}
		g_object_unref (info);
		g_object_unref (infile);

		if (data != NULL) {
			as_dep11_parse_data (dep11, data, &tmp_error);
			g_free (data);
			if (tmp_error != NULL) {
				g_warning ("DEP11: %s", tmp_error->message);
				g_error_free (tmp_error);
			}
		}

		if (!ret)
			break;
	}
	g_ptr_array_unref (yaml_files);

	return ret;
}
#endif




/**
 * as_data_pool_update:
 *
 * Builds an index of all found components in the watched locations.
 * The function will try to get as much data into the pool as possible, so even if
 * the updates completes with %FALSE, it might still add components to the pool.
 *
 * Returns: %TRUE if update completed without error.
 **/
gboolean
as_data_pool_update (AsDataPool *dpool)
{
	gboolean ret = TRUE;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	/* just in case, clear the components table */
	g_hash_table_unref (priv->cpt_table);
	priv->cpt_table = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						(GDestroyNotify) g_object_unref);

	/* read all AppStream metadata that we can find */
	ret = as_data_pool_read_asxml (dpool);
#ifdef DEBIAN_DEP11
	if (!as_data_pool_read_dep11 (dpool))
		ret = FALSE;
#endif

	return ret;
}

/**
 * as_data_pool_get_components:
 *
 * Get a list of found components.
 *
 * Returns: (element-type AsComponent) (transfer container): a list of #AsComponent instances, free with g_list_free()
 */
GList*
as_data_pool_get_components (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	return g_hash_table_get_values (priv->cpt_table);
}

/**
 * as_data_pool_set_locale:
 * @dpool: a #AsDataPool instance.
 * @locale: the locale.
 *
 * Sets the current locale which should be used when parsing metadata.
 **/
void
as_data_pool_set_locale (AsDataPool *dpool, const gchar *locale)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	g_free (priv->locale);
	priv->locale = g_strdup (locale);
}

/**
 * as_data_pool_get_locale:
 * @dpool: a #AsDataPool instance.
 *
 * Gets the currently used locale.
 *
 * Returns: Locale used for metadata parsing.
 **/
const gchar *
as_data_pool_get_locale (AsDataPool *dpool)
{
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);
	return priv->locale;
}

/**
 * as_data_pool_set_data_source_directories:
 * @dpool a valid #AsBuilder instance
 * @dirs: (array zero-terminated=1): a zero-terminated array of data input directories.
 *
 * Set locations for the data pool to read it's data from.
 * This is mainly used for testing purposes. Each location should have an
 * "xmls" and/or "yaml" subdirectory with the actual data as (compressed)
 * AppStream XML or DEP-11 YAML in it.
 */
void
as_data_pool_set_data_source_directories (AsDataPool *dpool, gchar **dirs)
{
	guint i;
	GPtrArray *xmldirs;
	GPtrArray *yamldirs;
	GPtrArray *icondirs;
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	xmldirs = g_ptr_array_new_with_free_func (g_free);
	yamldirs = g_ptr_array_new_with_free_func (g_free);

	icondirs = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; priv->icon_paths[i] != NULL; i++) {
		g_ptr_array_add (icondirs, g_strdup (priv->icon_paths[i]));
	}

	for (i = 0; dirs[i] != NULL; i++) {
		gchar *path;
		path = g_build_filename (dirs[i], "xmls", NULL);
		if (g_file_test (path, G_FILE_TEST_EXISTS))
			g_ptr_array_add (xmldirs, g_strdup (path));
		g_free (path);

		path = g_build_filename (dirs[i], "yaml", NULL);
		if (g_file_test (path, G_FILE_TEST_EXISTS))
			g_ptr_array_add (yamldirs, g_strdup (path));
		g_free (path);

		path = g_build_filename (dirs[i], "icons", NULL);
		if (g_file_test (path, G_FILE_TEST_EXISTS))
			g_ptr_array_add (icondirs, g_strdup (path));
		g_free (path);
	}

	/* add new metadata directories */
	g_strfreev (priv->asxml_paths);
	priv->asxml_paths = as_ptr_array_to_strv (xmldirs);

	g_strfreev (priv->dep11_paths);
	priv->dep11_paths = as_ptr_array_to_strv (yamldirs);

	/* add new icon search locations */
	g_strfreev (priv->icon_paths);
	priv->icon_paths = as_ptr_array_to_strv (icondirs);

	g_ptr_array_unref (xmldirs);
	g_ptr_array_unref (yamldirs);
}

/**
 * as_data_pool_new:
 *
 * Creates a new #AsDataPool.
 *
 * Returns: (transfer full): a #AsDataPool
 *
 **/
AsDataPool *
as_data_pool_new (void)
{
	AsDataPool *dpool;
	AsDataPoolPrivate *priv;
	AsDistroDetails *distro;
	guint len;
	guint i;

	dpool = g_object_new (AS_TYPE_DATA_POOL, NULL);
	priv = GET_PRIVATE (dpool);

	/* set active locale */
	priv->locale = as_get_locale ();

	priv->cpt_table = g_hash_table_new_full (g_str_hash,
								g_str_equal,
								g_free,
								(GDestroyNotify) g_object_unref);
	priv->providers = g_ptr_array_new_with_free_func (g_object_unref);

	distro = as_distro_details_new ();
	priv->scr_base_url = as_distro_details_config_distro_get_str (distro, "ScreenshotUrl");
	if (priv->scr_base_url == NULL) {
		g_debug ("Unable to determine screenshot service for distribution '%s'. Using the Debian services.", as_distro_details_get_distro_name (distro));
		priv->scr_base_url = g_strdup ("http://screenshots.debian.net");
	}
	g_object_unref (distro);

	/* set watched default directories for AppStream XML */
	len = G_N_ELEMENTS (AS_APPSTREAM_XML_PATHS);
	priv->asxml_paths = g_new0 (gchar *, len + 1);
	for (i = 0; i < len+1; i++) {
		if (i < len)
			priv->asxml_paths[i] = g_strdup (AS_APPSTREAM_XML_PATHS[i]);
		else
			priv->asxml_paths[i] = NULL;
	}

	/* set watched default directories for Debian DEP11 AppStream data */
	len = G_N_ELEMENTS (AS_APPSTREAM_DEP11_PATHS);
	priv->dep11_paths = g_new0 (gchar *, len + 1);
	for (i = 0; i < len+1; i++) {
		if (i < len)
			priv->dep11_paths[i] = g_strdup (AS_APPSTREAM_DEP11_PATHS[i]);
		else
			priv->dep11_paths[i] = NULL;
	}

	/* set default icon search locations */
	priv->icon_paths = as_distro_details_get_icon_repository_paths ();

	return AS_DATA_POOL (dpool);
}
