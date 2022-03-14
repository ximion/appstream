/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-artifact
 * @short_description: Object describing a artifact artifact
 *
 * Describes a artifact artifact
 *
 * See also: #AsArtifact
 */

#include "config.h"
#include "as-artifact.h"
#include "as-checksum-private.h"
#include "as-utils-private.h"

typedef struct
{
	AsArtifactKind	kind;

	GPtrArray	*locations;
	GPtrArray	*checksums;
	guint64		size[AS_SIZE_KIND_LAST];
	gchar		*filename;

	GRefString	*platform;
	AsBundleKind	bundle_kind;
} AsArtifactPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsArtifact, as_artifact, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_artifact_get_instance_private (o))

/**
 * as_size_kind_to_string:
 * @size_kind: the #AsSizeKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @size_kind
 **/
const gchar*
as_size_kind_to_string (AsSizeKind size_kind)
{
	if (size_kind == AS_SIZE_KIND_INSTALLED)
		return "installed";
	if (size_kind == AS_SIZE_KIND_DOWNLOAD)
		return "download";
	return "unknown";
}

/**
 * as_size_kind_from_string:
 * @size_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: an #AsSizeKind or %AS_SIZE_KIND_UNKNOWN for unknown
 **/
AsSizeKind
as_size_kind_from_string (const gchar *size_kind)
{
	if (g_strcmp0 (size_kind, "download") == 0)
		return AS_SIZE_KIND_DOWNLOAD;
	if (g_strcmp0 (size_kind, "installed") == 0)
		return AS_SIZE_KIND_INSTALLED;
	return AS_SIZE_KIND_UNKNOWN;
}

/**
 * as_artifact_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsArtifactKind, or %AS_ARTIFACT_KIND_UNKNOWN for unknown.
 *
 **/
AsArtifactKind
as_artifact_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "source") == 0)
		return AS_ARTIFACT_KIND_SOURCE;
	if (g_strcmp0 (kind, "binary") == 0)
		return AS_ARTIFACT_KIND_BINARY;
	return AS_ARTIFACT_KIND_UNKNOWN;
}

/**
 * as_artifact_kind_to_string:
 * @kind: the #AsArtifactKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 **/
const gchar*
as_artifact_kind_to_string (AsArtifactKind kind)
{
	if (kind == AS_ARTIFACT_KIND_SOURCE)
		return "source";
	if (kind == AS_ARTIFACT_KIND_BINARY)
		return "binary";
	return NULL;
}

/**
 * as_artifact_finalize:
 **/
static void
as_artifact_finalize (GObject *object)
{
	AsArtifact *artifact = AS_ARTIFACT (object);
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);

	g_free (priv->filename);
	as_ref_string_release (priv->platform);
	g_ptr_array_unref (priv->locations);
	g_ptr_array_unref (priv->checksums);

	G_OBJECT_CLASS (as_artifact_parent_class)->finalize (object);
}

/**
 * as_artifact_init:
 **/
static void
as_artifact_init (AsArtifact *artifact)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);

	priv->locations = g_ptr_array_new_with_free_func (g_free);
	priv->checksums = g_ptr_array_new_with_free_func (g_object_unref);
	priv->bundle_kind = AS_BUNDLE_KIND_UNKNOWN;

	for (guint i = 0; i < AS_SIZE_KIND_LAST; i++)
		priv->size[i] = 0;
}

/**
 * as_artifact_set_kind:
 * @artifact: a #AsArtifact instance.
 * @kind: the #AsArtifactKind, e.g. %AS_ARTIFACT_KIND_SOURCE.
 *
 * Sets the artifact kind.
 *
 **/
void
as_artifact_set_kind (AsArtifact *artifact, AsArtifactKind kind)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	priv->kind = kind;
}

/**
 * as_artifact_get_kind:
 * @artifact: a #AsArtifact instance.
 *
 * Gets the artifact kind.
 *
 * Returns: the #AsArtifactKind
 *
 **/
AsArtifactKind
as_artifact_get_kind (AsArtifact *artifact)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	return priv->kind;
}

/**
 * as_artifact_get_locations:
 *
 * Gets the artifact locations, typically URLs.
 *
 * Returns: (transfer none) (element-type utf8): list of locations
 **/
GPtrArray*
as_artifact_get_locations (AsArtifact *artifact)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	return priv->locations;
}

/**
 * as_artifact_add_location:
 * @location: An URL of the download location
 *
 * Adds a artifact location.
 **/
void
as_artifact_add_location (AsArtifact *artifact, const gchar *location)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	g_ptr_array_add (priv->locations, g_strdup (location));
}

/**
 * as_artifact_get_checksums:
 *
 * Get a list of all checksums we have for this artifact.
 *
 * Returns: (transfer none) (element-type AsChecksum): an array of #AsChecksum objects.
 **/
GPtrArray*
as_artifact_get_checksums (AsArtifact *artifact)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	return priv->checksums;
}

/**
 * as_artifact_get_checksum:
 * @artifact: a #AsArtifact instance.
 *
 * Gets the artifact checksum
 *
 * Returns: (transfer none) (nullable): an #AsChecksum, or %NULL for not set or invalid
 **/
AsChecksum*
as_artifact_get_checksum (AsArtifact *artifact, AsChecksumKind kind)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	guint i;

	for (i = 0; i < priv->checksums->len; i++) {
		AsChecksum *cs = AS_CHECKSUM (g_ptr_array_index (priv->checksums, i));
		if (as_checksum_get_kind (cs) == kind)
			return cs;
	}
	return NULL;
}

/**
 * as_artifact_add_checksum:
 * @artifact: An instance of #AsArtifact.
 * @cs: The #AsChecksum.
 *
 * Add a checksum for the file associated with this artifact.
 */
void
as_artifact_add_checksum (AsArtifact *artifact, AsChecksum *cs)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	g_ptr_array_add (priv->checksums, g_object_ref (cs));
}

/**
 * as_artifact_get_size:
 * @artifact: a #AsArtifact instance
 * @kind: a #AsSizeKind
 *
 * Gets the artifact size.
 *
 * Returns: The size of the given kind of this artifact.
 **/
guint64
as_artifact_get_size (AsArtifact *artifact, AsSizeKind kind)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	g_return_val_if_fail (kind < AS_SIZE_KIND_LAST, 0);
	return priv->size[kind];
}

/**
 * as_artifact_set_size:
 * @artifact: a #AsArtifact instance
 * @size: a size in bytes, or 0 for unknown
 * @kind: a #AsSizeKind
 *
 * Sets the artifact size for the given kind.
 **/
void
as_artifact_set_size (AsArtifact *artifact, guint64 size, AsSizeKind kind)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	g_return_if_fail (kind < AS_SIZE_KIND_LAST);
	g_return_if_fail (kind != 0);

	priv->size[kind] = size;
}

/**
 * as_artifact_get_platform:
 * @artifact: a #AsArtifact instance.
 *
 * Gets the artifact platform string (e.g. a triplet like "x86_64-linux-gnu").
 *
 * Returns: The platform triplet or identifier string.
 **/
const gchar*
as_artifact_get_platform (AsArtifact *artifact)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	return priv->platform;
}

/**
 * as_artifact_set_platform:
 * @artifact: a #AsArtifact instance.
 * @platform: the platform triplet.
 *
 * Sets the artifact platform triplet or identifier string.
 **/
void
as_artifact_set_platform (AsArtifact *artifact, const gchar *platform)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	as_ref_string_assign_safe (&priv->platform, platform);
}

/**
 * as_artifact_get_bundle_kind:
 * @artifact: an #AsArtifact instance.
 *
 * Gets the bundle kind of this artifact.
 *
 * Returns: the #AsBundleKind
 **/
AsBundleKind
as_artifact_get_bundle_kind (AsArtifact *artifact)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	return priv->bundle_kind;
}

/**
 * as_artifact_set_bundle_kind:
 * @artifact: an #AsArtifact instance.
 * @kind: the #AsBundleKind, e.g. %AS_BUNDLE_KIND_TARBALL.
 *
 * Sets the bundle kind for this release artifact.
 **/
void
as_artifact_set_bundle_kind (AsArtifact *artifact, AsBundleKind kind)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	priv->bundle_kind = kind;
}

/**
 * as_artifact_get_filename:
 * @artifact: a #AsArtifact instance.
 *
 * Gets a suggested filename for the downloaded artifact,
 * or %NULL if none is suggested.
 *
 * Returns: The platform triplet or identifier string.
 **/
const gchar*
as_artifact_get_filename (AsArtifact *artifact)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	return priv->filename;
}

/**
 * as_artifact_set_filename:
 * @artifact: a #AsArtifact instance.
 * @filename: the file name suggestion.
 *
 * Sets a suggested filename for this artifact after it has been downloaded.
 **/
void
as_artifact_set_filename (AsArtifact *artifact, const gchar *filename)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	as_assign_string_safe (priv->filename, filename);
}

/**
 * as_artifact_load_from_xml:
 * @artifact: a #AsArtifact instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads artifact data from an XML node.
 **/
gboolean
as_artifact_load_from_xml (AsArtifact *artifact, AsContext *ctx, xmlNode *node, GError **error)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	gchar *str;

	str = as_xml_get_prop_value (node, "platform");
	as_ref_string_assign_safe (&priv->platform, str);
	g_free (str);

	str = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	priv->kind = as_artifact_kind_from_string (str);
	g_free (str);

	str = (gchar*) xmlGetProp (node, (xmlChar*) "bundle");
	priv->bundle_kind = as_bundle_kind_from_string (str);
	g_free (str);

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		g_autofree gchar *content = NULL;
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "location") == 0) {
			content = as_xml_get_node_value (iter);
			as_artifact_add_location (artifact, content);
		} else if (g_strcmp0 ((gchar*) iter->name, "filename") == 0) {
			g_free (priv->filename);
			priv->filename = as_xml_get_node_value (iter);
		} else if (g_strcmp0 ((gchar*) iter->name, "checksum") == 0) {
			g_autoptr(AsChecksum) cs = NULL;

			cs = as_checksum_new ();
			if (as_checksum_load_from_xml (cs, ctx, iter, NULL))
				as_artifact_add_checksum (artifact, cs);
		} else if (g_strcmp0 ((gchar*) iter->name, "size") == 0) {
			AsSizeKind s_kind;
			gchar *prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");

			s_kind = as_size_kind_from_string (prop);
			if (s_kind != AS_SIZE_KIND_UNKNOWN) {
				guint64 size;

				content = as_xml_get_node_value (iter);
				size = g_ascii_strtoull (content, NULL, 10);
				if (size > 0)
					as_artifact_set_size (artifact, size, s_kind);
			}
			g_free (prop);
		}
	}

	return TRUE;
}

/**
 * as_artifact_to_xml_node:
 * @artifact: a #AsArtifact instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_artifact_to_xml_node (AsArtifact *artifact, AsContext *ctx, xmlNode *root)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	xmlNode* n_artifact = NULL;

	if (priv->kind == AS_ARTIFACT_KIND_UNKNOWN)
		return;

	n_artifact = xmlNewChild (root, NULL, (xmlChar*) "artifact", (xmlChar*) "");

	xmlNewProp (n_artifact,
		    (xmlChar*) "type",
		    (xmlChar*) as_artifact_kind_to_string (priv->kind));

	if (priv->platform != NULL) {
		xmlNewProp (n_artifact,
				(xmlChar*) "platform",
				(xmlChar*) priv->platform);
	}

	if (priv->bundle_kind != AS_BUNDLE_KIND_UNKNOWN) {
		xmlNewProp (n_artifact,
				(xmlChar*) "bundle",
				(xmlChar*) as_bundle_kind_to_string (priv->bundle_kind));
	}

	/* add location urls */
	for (guint j = 0; j < priv->locations->len; j++) {
		const gchar *lurl = (const gchar*) g_ptr_array_index (priv->locations, j);
		as_xml_add_text_node (n_artifact, "location", lurl);
	}

	/* add filename tag */
	as_xml_add_text_node (n_artifact, "filename", priv->filename);

	/* add checksum node */
	for (guint j = 0; j < priv->checksums->len; j++) {
		AsChecksum *cs = AS_CHECKSUM (g_ptr_array_index (priv->checksums, j));
		as_checksum_to_xml_node (cs, ctx, n_artifact);
	}

	/* add size node */
	for (guint j = 0; j < AS_SIZE_KIND_LAST; j++) {
		guint64 asize = as_artifact_get_size (artifact, j);
		if (asize > 0) {
			xmlNode *s_node;
			g_autofree gchar *size_str = g_strdup_printf ("%" G_GUINT64_FORMAT, asize);
			s_node = xmlNewTextChild (n_artifact,
						  NULL,
						  (xmlChar*) "size",
						  (xmlChar*) size_str);
			xmlNewProp (s_node,
				(xmlChar*) "type",
				(xmlChar*) as_size_kind_to_string (j));
		}
	}

	xmlAddChild (root, n_artifact);
}

/**
 * as_artifact_load_from_yaml:
 * @artifact: a #AsArtifact instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_artifact_load_from_yaml (AsArtifact *artifact, AsContext *ctx, GNode *node, GError **error)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);

	for (GNode *n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);

		if (g_strcmp0 (key, "type") == 0) {
			priv->kind = as_artifact_kind_from_string (as_yaml_node_get_value (n));

		} else if (g_strcmp0 (key, "platform") == 0) {
			as_ref_string_assign_safe (&priv->platform,
						   as_yaml_node_get_value (n));

		} else if (g_strcmp0 (key, "bundle") == 0) {
			priv->bundle_kind = as_bundle_kind_from_string (as_yaml_node_get_value (n));

		} else if (g_strcmp0 (key, "locations") == 0) {
			as_yaml_list_to_str_array (n, priv->locations);

		} else if (g_strcmp0 (key, "filename") == 0) {
			g_free (priv->filename);
			priv->filename = g_strdup (as_yaml_node_get_value (n));

		} else if (g_strcmp0 (key, "checksum") == 0) {
			for (GNode *sn = n->children; sn != NULL; sn = sn->next) {
				g_autoptr(AsChecksum) cs = as_checksum_new ();
				if (as_checksum_load_from_yaml (cs, ctx, sn, NULL))
					as_artifact_add_checksum (artifact, cs);
			}

		} else if (g_strcmp0 (key, "size") == 0) {
			for (GNode *sn = n->children; sn != NULL; sn = sn->next) {
				AsSizeKind size_kind = as_size_kind_from_string (as_yaml_node_get_key (sn));
				guint64 asize = g_ascii_strtoull (as_yaml_node_get_value (sn), NULL, 10);
				if (size_kind == AS_SIZE_KIND_UNKNOWN)
					continue;
				if (asize <= 0)
					continue;
				as_artifact_set_size (artifact, asize, size_kind);
			}

		} else {
			as_yaml_print_unknown ("artifact", key);
		}
	}

	return priv->kind != AS_ARTIFACT_KIND_UNKNOWN;
}

/**
 * as_artifact_emit_yaml:
 * @artifact: a #AsArtifact instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_artifact_emit_yaml (AsArtifact *artifact, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);

	if (priv->kind == AS_ARTIFACT_KIND_UNKNOWN)
		return;

	as_yaml_mapping_start (emitter);

	as_yaml_emit_entry (emitter,
			    "type",
			    as_artifact_kind_to_string (priv->kind));

	as_yaml_emit_entry (emitter,
			    "platform",
			    priv->platform);

	if (priv->bundle_kind != AS_BUNDLE_KIND_UNKNOWN) {
		as_yaml_emit_entry (emitter,
				    "bundle",
				    as_bundle_kind_to_string (priv->bundle_kind));
	}

	/* location URLs */
	as_yaml_emit_sequence_from_str_array (emitter, "locations", priv->locations);

	/* filename suggestion */
	as_yaml_emit_entry (emitter,
			    "filename",
			    priv->filename);

	/* checksums */
	if (priv->locations->len > 0) {
		as_yaml_emit_scalar (emitter, "checksum");
		as_yaml_mapping_start (emitter);

		for (guint j = 0; j < priv->checksums->len; j++) {
			AsChecksum *cs = AS_CHECKSUM (g_ptr_array_index (priv->checksums, j));
			as_checksum_emit_yaml (cs, ctx, emitter);
		}

		as_yaml_mapping_end (emitter);
	}

	/* sizes */
	as_yaml_emit_scalar (emitter, "size");
	as_yaml_mapping_start (emitter);
	for (guint j = 0; j < AS_SIZE_KIND_LAST; j++) {
		guint64 asize = as_artifact_get_size (artifact, j);
		if (asize > 0)
			as_yaml_emit_entry_uint64 (emitter, as_size_kind_to_string (j), asize);
	}
	as_yaml_mapping_end (emitter);

	as_yaml_mapping_end (emitter);
}

/**
 * as_artifact_class_init:
 **/
static void
as_artifact_class_init (AsArtifactClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_artifact_finalize;
}

/**
 * as_artifact_new:
 *
 * Creates a new #AsArtifact.
 *
 * Returns: (transfer full): a #AsArtifact
 *
 **/
AsArtifact*
as_artifact_new (void)
{
	AsArtifact *artifact;
	artifact = g_object_new (AS_TYPE_ARTIFACT, NULL);
	return AS_ARTIFACT (artifact);
}
