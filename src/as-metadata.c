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

/**
 * SECTION:as-metadata
 * @short_description: Parser for AppStream metadata
 * @include: appstream.h
 *
 * This object parses AppStream metadata, including AppStream
 * upstream metadata, which is defined by upstream projects.
 * It returns an #AsComponent of the data.
 *
 * See also: #AsComponent, #AsDatabase
 */

#include <config.h>
#include <glib.h>
#include <string.h>

#include "as-metadata.h"

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-component.h"
#include "as-component-private.h"
#include "as-xmldata.h"
#include "as-yamldata.h"
#include "as-distro-details.h"

typedef struct
{
	gchar *locale;
	AsParserMode mode;
	gchar *origin;
	gchar *media_baseurl;
	gchar *arch;
	gint default_priority;

	gboolean update_existing;
	gboolean write_header;

	AsXMLData *xdt;
	AsYAMLData *ydt;

	GPtrArray *cpts;
} AsMetadataPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsMetadata, as_metadata, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_metadata_get_instance_private (o))

/**
 * as_metadata_init:
 **/
static void
as_metadata_init (AsMetadata *metad)
{
	gchar *str;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	/* set active locale without UTF-8 suffix */
	str = as_get_current_locale ();
	as_metadata_set_locale (metad, str);
	g_free (str);

	priv->mode = AS_PARSER_MODE_UPSTREAM;
	priv->default_priority = 0;
	priv->write_header = TRUE;
	priv->update_existing = FALSE;

	priv->cpts = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * as_metadata_finalize:
 **/
static void
as_metadata_finalize (GObject *object)
{
	AsMetadata *metad = AS_METADATA (object);
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	g_free (priv->locale);
	g_ptr_array_unref (priv->cpts);
	g_free (priv->origin);
	g_free (priv->media_baseurl);
	g_free (priv->arch);
	if (priv->xdt != NULL)
		g_object_unref (priv->xdt);
	if (priv->ydt != NULL)
		g_object_unref (priv->ydt);

	G_OBJECT_CLASS (as_metadata_parent_class)->finalize (object);
}

/**
 * as_metadata_init_xml:
 **/
static void
as_metadata_init_xml (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	if (priv->xdt != NULL)
		return;

	priv->xdt = as_xmldata_new ();
	as_xmldata_initialize (priv->xdt,
				priv->locale,
				priv->origin,
				priv->media_baseurl,
				priv->arch,
				priv->default_priority);
}

/**
 * as_metadata_init_yaml:
 **/
static void
as_metadata_init_yaml (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	if (priv->ydt != NULL)
		return;

	priv->ydt = as_yamldata_new ();
	as_yamldata_initialize (priv->ydt,
				priv->locale,
				priv->origin,
				priv->media_baseurl,
				priv->arch,
				priv->default_priority);
}

/**
 * as_metadata_reload_parsers:
 */
static void
as_metadata_reload_parsers (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	if (priv->xdt != NULL)
		as_xmldata_initialize (priv->xdt,
					priv->locale,
					priv->origin,
					priv->media_baseurl,
					priv->arch,
					priv->default_priority);
	if (priv->ydt != NULL)
		as_yamldata_initialize (priv->ydt,
					priv->locale,
					priv->origin,
					priv->media_baseurl,
					priv->arch,
					priv->default_priority);
}

/**
 * as_metadata_clear_components:
 **/
void
as_metadata_clear_components (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_ptr_array_unref (priv->cpts);
	priv->cpts = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * as_metadata_parse_xml:
 * @metad: A valid #AsMetadata instance
 * @data: XML data describing one or more software components.
 *
 * Parses AppStream XML metadata.
 **/
void
as_metadata_parse_xml (AsMetadata *metad, const gchar *data, GError **error)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	as_metadata_init_xml (metad);

	if (priv->mode == AS_PARSER_MODE_DISTRO) {
		guint i;
		g_autoptr(GPtrArray) new_cpts = NULL;

		new_cpts = as_xmldata_parse_distro_data (priv->xdt, data, error);
		if (new_cpts == NULL)
			return;
		for (i = 0; i < new_cpts->len; i++) {
			AsComponent *cpt;
			cpt = AS_COMPONENT (g_ptr_array_index (new_cpts, i));
			g_ptr_array_add (priv->cpts,
						g_object_ref (cpt));
		}
	} else {
		AsComponent *cpt;

		if (priv->update_existing) {
			/* we should update the existing component with new metadata */
			cpt = as_metadata_get_component (metad);
			if (cpt == NULL) {
				g_set_error_literal (error,
						AS_METADATA_ERROR,
						AS_METADATA_ERROR_NO_COMPONENT,
						"No component found that could be updated.");
				return;
			}
			as_xmldata_update_cpt_with_upstream_data (priv->xdt, data, cpt, error);
		} else {
			cpt = as_xmldata_parse_upstream_data (priv->xdt, data, error);
			if (cpt != NULL)
				g_ptr_array_add (priv->cpts, cpt);
		}
	}
}

/**
 * as_metadata_parse_yaml:
 * @metad: A valid #AsMetadata instance
 * @data: YAML data describing one or more software components.
 * @error: A #GError or %NULL.
 *
 * Parses AppStream YAML metadata.
 **/
void
as_metadata_parse_yaml (AsMetadata *metad, const gchar *data, GError **error)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	as_metadata_init_yaml (metad);

	if (priv->mode == AS_PARSER_MODE_DISTRO) {
		guint i;
		g_autoptr(GPtrArray) new_cpts = NULL;

		new_cpts = as_yamldata_parse_distro_data (priv->ydt, data, error);
		if (new_cpts == NULL)
			return;
		for (i = 0; i < new_cpts->len; i++) {
			AsComponent *cpt;
			cpt = AS_COMPONENT (g_ptr_array_index (new_cpts, i));
			g_ptr_array_add (priv->cpts,
						g_object_ref (cpt));
		}
	} else {
		g_warning ("Can not load non-distro AppStream YAML data, since their format is not specified.");
	}
}

/**
 * as_metadata_parse_file:
 * @metad: A valid #AsMetadata instance
 * @file: #GFile for the upstream metadata
 * @error: A #GError or %NULL.
 *
 * Parses an AppStream upstream metadata file.
 *
 **/
void
as_metadata_parse_file (AsMetadata *metad, GFile *file, GError **error)
{
	g_autofree gchar *file_basename = NULL;
	gboolean is_yaml_doc = FALSE;
	g_autoptr(GFileInfo) info = NULL;
	g_autoptr(GInputStream) file_stream = NULL;
	g_autoptr(GInputStream) stream_data = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GString) asdata = NULL;
	gssize len;
	const gsize buffer_size = 1024 * 24;
	g_autofree gchar *buffer = NULL;
	const gchar *content_type = NULL;

	info = g_file_query_info (file,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
	if (info != NULL)
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	/* check if we are dealing with a YAML document, assume XML otherwise */
	if (g_strcmp0 (content_type, "application/x-yaml") == 0)
		is_yaml_doc = TRUE;

	file_basename = g_file_get_basename (file);
	if ((g_str_has_suffix (file_basename, ".yml.gz")) ||
	    (g_str_has_suffix (file_basename, ".yaml.gz")) ||
	    (g_str_has_suffix (file_basename, ".yml")) ||
	    (g_str_has_suffix (file_basename, ".yaml"))) {
		is_yaml_doc = TRUE;
	}

	file_stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
	if (file_stream == NULL)
		return;

	if ((g_strcmp0 (content_type, "application/gzip") == 0) || (g_strcmp0 (content_type, "application/x-gzip") == 0)) {
		/* decompress the GZip stream */
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		stream_data = g_converter_input_stream_new (file_stream, conv);
	} else {
		stream_data = g_object_ref (file_stream);
	}

	/* Now read the whole file into memory to parse it.
	 * On memory-contrained systems we could adjust the code later to allow parsing
	 * a stream of data instead.
	 */

	asdata = g_string_new ("");
	buffer = g_malloc (buffer_size);
	while ((len = g_input_stream_read (stream_data, buffer, buffer_size, NULL, error)) > 0) {
		g_string_append_len (asdata, buffer, len);
	}
	/* check if there was an error */
	if (len < 0)
		return;

	/* parse metadata */
	if (is_yaml_doc)
		as_metadata_parse_yaml (metad, asdata->str, error);
	else
		as_metadata_parse_xml (metad, asdata->str, error);
}

/**
 * as_metadata_save_data:
 */
static void
as_metadata_save_data (AsMetadata *metad, const gchar *fname, const gchar *metadata, GError **error)
{
	g_autoptr(GFile) file = NULL;
	GError *tmp_error = NULL;

	file = g_file_new_for_path (fname);
	if (g_str_has_suffix (fname, ".gz")) {
		g_autoptr(GOutputStream) out2 = NULL;
		g_autoptr(GOutputStream) out = NULL;
		GZlibCompressor *compressor = NULL;

		/* write a gzip compressed file */
		compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
		out = g_memory_output_stream_new_resizable ();
		out2 = g_converter_output_stream_new (out, G_CONVERTER (compressor));
		g_object_unref (compressor);

		/* ensure data is not NULL */
		if (metadata == NULL)
			return;

		if (!g_output_stream_write_all (out2, metadata, strlen (metadata),
					NULL, NULL, &tmp_error)) {
			g_propagate_error (error, tmp_error);
			return;
		}

		g_output_stream_close (out2, NULL, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}

		if (!g_file_replace_contents (file,
			g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (out)),
						g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (out)),
						NULL,
						FALSE,
						G_FILE_CREATE_NONE,
						NULL,
						NULL,
						&tmp_error)) {
			g_propagate_error (error, tmp_error);
			return;
		}
	} else {
		GFileOutputStream *fos = NULL;
		GDataOutputStream *dos = NULL;

		/* write uncompressed file */
		if (g_file_query_exists (file, NULL)) {
			fos = g_file_replace (file,
							NULL,
							FALSE,
							G_FILE_CREATE_REPLACE_DESTINATION,
							NULL,
							&tmp_error);
		} else {
			fos = g_file_create (file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &tmp_error);
		}

		if (tmp_error != NULL) {
			g_object_unref (fos);
			g_propagate_error (error, tmp_error);
			return;
		}

		dos = g_data_output_stream_new (G_OUTPUT_STREAM (fos));
		g_data_output_stream_put_string (dos, metadata, NULL, &tmp_error);

		g_object_unref (dos);
		g_object_unref (fos);

		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}
	}
}

/**
 * as_metadata_save_upstream_xml:
 * @fname: The filename for the new XML file.
 *
 * Serialize #AsComponent instance to XML and save it to file.
 * An existing file at the same location will be overridden.
 */
void
as_metadata_save_upstream_xml (AsMetadata *metad, const gchar *fname, GError **error)
{
	g_autofree gchar *xml_data = NULL;

	xml_data = as_metadata_component_to_upstream_xml (metad);
	as_metadata_save_data (metad, fname, xml_data, error);
}

/**
 * as_metadata_save_distro_xml:
 * @fname: The filename for the new XML file.
 *
 * Serialize all #AsComponent instances to XML and save the data to a file.
 * An existing file at the same location will be overridden.
 */
void
as_metadata_save_distro_xml (AsMetadata *metad, const gchar *fname, GError **error)
{
	g_autofree gchar *xml_data = NULL;

	xml_data = as_metadata_components_to_distro_xml (metad);
	as_metadata_save_data (metad, fname, xml_data, error);
}

/**
 * as_metadata_components_to_distro_yaml:
 *
 * Serialize all #AsComponent instances into AppStream DEP-11
 * distro-YAML data.
 * %NULL is returned if there is nothing to serialize.
 *
 * Returns: (transfer full): A string containing the YAML markup. Free with g_free()
 */
gchar*
as_metadata_components_to_distro_yaml (AsMetadata *metad)
{
	gchar *yamlstr = NULL;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	as_metadata_init_yaml (metad);
	if (priv->cpts->len == 0)
		return NULL;

	yamlstr = as_yamldata_serialize_to_distro (priv->ydt,
							priv->cpts,
							priv->write_header,
							TRUE, /* add timestamp */
							NULL);
	return yamlstr;
}

/**
 * as_metadata_save_distro_yaml:
 * @fname: The filename for the new YAML file.
 *
 * Serialize all #AsComponent instances to XML and save the data to a file.
 * An existing file at the same location will be overridden.
 */
void
as_metadata_save_distro_yaml (AsMetadata *metad, const gchar *fname, GError **error)
{
	g_autofree gchar *yaml_data = NULL;

	yaml_data = as_metadata_components_to_distro_yaml (metad);
	as_metadata_save_data (metad, fname, yaml_data, error);
}

/**
 * as_metadata_component_to_upstream_xml:
 *
 * Convert an #AsComponent to upstream XML.
 * (The amount of localization included in the metadata depends on how the #AsComponent
 * was initially loaded)
 *
 * The first #AsComponent added to the internal list will be transformed.
 * In case no component is present, %NULL is returned.
 *
 * Returns: (transfer full): A string containing the XML. Free with g_free()
 */
gchar*
as_metadata_component_to_upstream_xml (AsMetadata *metad)
{
	gchar *xmlstr = NULL;
	AsComponent *cpt;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	as_metadata_init_xml (metad);
	cpt = as_metadata_get_component (metad);
	if (cpt == NULL)
		return NULL;


	xmlstr = as_xmldata_serialize_to_upstream (priv->xdt, cpt);
	return xmlstr;
}

/**
 * as_metadata_components_to_distro_xml:
 *
 * Serialize all #AsComponent instances into AppStream
 * distro-XML data.
 * %NULL is returned if there is nothing to serialize.
 *
 * Returns: (transfer full): A string containing the XML. Free with g_free()
 */
gchar*
as_metadata_components_to_distro_xml (AsMetadata *metad)
{
	gchar *xmlstr = NULL;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	as_metadata_init_xml (metad);
	if (priv->cpts->len == 0)
		return NULL;

	xmlstr = as_xmldata_serialize_to_distro (priv->xdt, priv->cpts, priv->write_header);
	return xmlstr;
}

/**
 * as_metadata_add_component:
 *
 * Add an #AsComponent to the list of components.
 * This can be used to add multiple components in order to
 * produce a distro-XML AppStream metadata file.
 */
void
as_metadata_add_component (AsMetadata *metad, AsComponent *cpt)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_ptr_array_add (priv->cpts, g_object_ref (cpt));
}

/**
 * as_metadata_get_component:
 * @metad: a #AsMetadata instance.
 *
 * Gets the #AsComponent which has been parsed from the XML.
 * If the AppStream XML contained multiple components, return the first
 * component that has been parsed.
 *
 * Returns: (transfer none): An #AsComponent or %NULL
 **/
AsComponent*
as_metadata_get_component (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	if (priv->cpts->len == 0)
		return NULL;
	return AS_COMPONENT (g_ptr_array_index (priv->cpts, 0));
}

/**
 * as_metadata_get_components:
 * @metad: a #AsMetadata instance.
 *
 * Returns: (transfer none) (element-type AsComponent): A #GPtrArray of all parsed components
 **/
GPtrArray*
as_metadata_get_components (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->cpts;
}

/**
 * as_metadata_set_locale:
 * @metad: a #AsMetadata instance.
 * @locale: the locale.
 *
 * Sets the locale which should be read when processing metadata.
 * All other locales are ignored, which increases parsing speed and
 * reduces memory usage.
 * If you set the locale to "ALL", all locales will be read.
 **/
void
as_metadata_set_locale (AsMetadata *metad, const gchar *locale)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	g_free (priv->locale);
	priv->locale = g_strdup (locale);
	as_metadata_reload_parsers (metad);
}

/**
 * as_metadata_get_locale:
 * @metad: a #AsMetadata instance.
 *
 * Gets the current active locale for parsing metadata,
 * or "ALL" if all locales are read.
 *
 * Returns: Locale used for metadata parsing.
 **/
const gchar *
as_metadata_get_locale (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->locale;
}

/**
 * as_metadata_set_origin:
 * @metad: an #AsMetadata instance.
 * @origin: the origin of AppStream distro metadata.
 *
 * Set the origin of AppStream distro metadata
 **/
void
as_metadata_set_origin (AsMetadata *metad, const gchar *origin)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_free (priv->origin);
	priv->origin = g_strdup (origin);
	as_metadata_reload_parsers (metad);
}

/**
 * as_metadata_get_origin:
 * @metad: an #AsMetadata instance.
 *
 * Returns: The origin of AppStream distro metadata
 **/
const gchar*
as_metadata_get_origin (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->origin;
}

/**
 * as_metadata_set_architecture:
 * @metad: an #AsMetadata instance.
 * @arch: an architecture string.
 *
 * Set the architecture the components in this metadata belong to.
 **/
void
as_metadata_set_architecture (AsMetadata *metad, const gchar *arch)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_free (priv->arch);
	priv->arch = g_strdup (arch);
	as_metadata_reload_parsers (metad);
}

/**
 * as_metadata_get_architecture:
 * @metad: an #AsMetadata instance.
 *
 * Returns: The architecture of AppStream distro metadata
 **/
const gchar*
as_metadata_get_architecture (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->arch;
}

/**
 * as_metadata_set_parser_mode:
 * @metad: a #AsMetadata instance.
 * @mode: the #AsParserMode.
 *
 * Sets the current metadata parsing mode.
 **/
void
as_metadata_set_parser_mode (AsMetadata *metad, AsParserMode mode)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	priv->mode = mode;
}

/**
 * as_metadata_set_update_existing:
 * @metad: an #AsMetadata instance.
 * @update: A boolean value.
 *
 * If set to %TRUE, the parser will not create new components but
 * instead update existing components in the pool with new metadata.
 *
 * NOTE: Right now, this feature is only implemented for metainfo XML parsing!
 **/
void
as_metadata_set_update_existing (AsMetadata *metad, gboolean update)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	priv->update_existing = update;
}

/**
 * as_metadata_get_update_existing:
 * @metad: an #AsMetadata instance.
 *
 * Returns: Whether existing components should be updates with the parsed data,
 *          instead of creating new ones.
 **/
gboolean
as_metadata_get_update_existing (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->update_existing;
}

/**
 * as_metadata_set_write_header:
 * @metad: an #AsMetadata instance.
 * @wheader: A boolean value.
 *
 * If set to %TRUE, tehe metadata writer will omit writing a DEP-11
 * header document when in YAML mode, and will not write a root components node
 * when writing XML data.
 * Please keep in mind that this will create an invalid DEP-11 YAML AppStream
 * distro metadata file, and an invalid XML file.
 * This parameter should only be changed e.g. by the appstream-generator tool.
 *
 * NOTE: Right now, this feature is only implemented for YAML!
 **/
void
as_metadata_set_write_header (AsMetadata *metad, gboolean wheader)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	priv->write_header = wheader;
}

/**
 * as_metadata_get_write_header:
 * @metad: an #AsMetadata instance.
 *
 * Returns: Whether we will write a header/root node in distro metadata.
 **/
gboolean
as_metadata_get_write_header (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->write_header;
}

/**
 * as_metadata_get_parser_mode:
 * @metad: a #AsMetadata instance.
 *
 * Gets the current parser mode
 *
 * Returns: an #AsParserMode
 **/
AsParserMode
as_metadata_get_parser_mode (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->mode;
}

/**
 * as_metadata_class_init:
 **/
static void
as_metadata_class_init (AsMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_metadata_finalize;
}

/**
 * as_metadata_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_metadata_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsMetadataError");
	return quark;
}

/**
 * as_metadata_new:
 *
 * Creates a new #AsMetadata.
 *
 * Returns: (transfer full): a #AsMetadata
 **/
AsMetadata*
as_metadata_new (void)
{
	AsMetadata *metad;
	metad = g_object_new (AS_TYPE_METADATA, NULL);
	return AS_METADATA (metad);
}
