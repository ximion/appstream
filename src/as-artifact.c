/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2019 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-variant-cache.h"
#include "as-checksum-private.h"

typedef struct
{
	AsArtifactKind	kind;

	GPtrArray	*locations;
	GPtrArray	*checksums;
	guint64		size[AS_SIZE_KIND_LAST];

	gchar		*platform;
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
	g_free (priv->platform);
	priv->platform = g_strdup (platform);
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

	g_free (priv->platform);
	priv->platform = (gchar*) xmlGetProp (node, (xmlChar*) "platform");

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
		xmlNewTextChild (n_artifact, NULL, (xmlChar*) "location", (xmlChar*) lurl);
	}

	/* add checksum node */
	for (guint j = 0; j < priv->checksums->len; j++) {
		AsChecksum *cs = AS_CHECKSUM (g_ptr_array_index (priv->checksums, j));
		as_checksum_to_xml_node (cs, ctx, n_artifact);
	}

	/* add size node */
	for (guint j = 0; j < AS_SIZE_KIND_LAST; j++) {
		if (as_artifact_get_size (artifact, j) > 0) {
			xmlNode *s_node;
			g_autofree gchar *size_str;

			size_str = g_strdup_printf ("%" G_GUINT64_FORMAT, as_artifact_get_size (artifact, j));
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
as_artifact_load_from_yaml (AsArtifact *artifact, AsContext *ctx, GNode *node, AsArtifactKind kind, GError **error)
{
	/* TODO: DEP-11 isn't defined for artifacts yet */

	return TRUE;
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
	/* TODO: DEP-11 isn't defined for artifacts yet */
}

/**
 * as_artifact_to_variant:
 * @artifact: An #AsArtifact instance.
 * @builder: A #GVariantBuilder
 *
 * Serialize the current active state of this object to a GVariant
 * for use in the on-disk binary cache.
 */
void
as_artifact_to_variant (AsArtifact *artifact, GVariantBuilder *builder)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	GVariantBuilder artifact_b;
	GVariantBuilder checksum_b;
	GVariantBuilder sizes_b;

	GVariant *locations_var;
	GVariant *checksums_var;
	GVariant *sizes_var;
	gboolean have_sizes = FALSE;

	g_variant_builder_init (&artifact_b, G_VARIANT_TYPE_ARRAY);

	g_variant_builder_add_parsed (&artifact_b, "{'type', <%u>}", priv->kind);
	g_variant_builder_add_parsed (&artifact_b, "{'platform', %v}", as_variant_mstring_new (priv->platform));
	g_variant_builder_add_parsed (&artifact_b, "{'bundle_kind', <%u>}", priv->bundle_kind);

	/* build checksum info */
	g_variant_builder_init (&checksum_b, (const GVariantType *) "a{us}");
	for (guint j = 0; j < priv->checksums->len; j++) {
		AsChecksum *cs = AS_CHECKSUM (g_ptr_array_index (priv->checksums, j));
		as_checksum_to_variant (cs, &checksum_b);
	}

	/* build size info */
	g_variant_builder_init (&sizes_b, (const GVariantType *) "a{ut}");
	for (guint j = 0; j < AS_SIZE_KIND_LAST; j++) {
		if (as_artifact_get_size (artifact, (AsSizeKind) j) > 0) {
			g_variant_builder_add (&sizes_b, "{ut}",
						(AsSizeKind) j,
						as_artifact_get_size (artifact, (AsSizeKind) j));
			have_sizes = TRUE;
		}
	}

	locations_var = as_variant_from_string_ptrarray (priv->locations);
	if (locations_var)
		g_variant_builder_add_parsed (&artifact_b, "{'locations', %v}", locations_var);

	checksums_var = priv->checksums->len > 0? g_variant_builder_end (&checksum_b) : NULL;
	if (checksums_var)
		g_variant_builder_add_parsed (&artifact_b, "{'checksums', %v}", checksums_var);

	sizes_var = have_sizes? g_variant_builder_end (&sizes_b) : NULL;
	if (sizes_var)
		g_variant_builder_add_parsed (&artifact_b, "{'sizes', %v}", sizes_var);

	g_variant_builder_add_value (builder, g_variant_builder_end (&artifact_b));
}

/**
 * as_artifact_set_from_variant:
 * @artifact: An #AsArtifact instance.
 * @variant: The #GVariant to read from.
 *
 * Read the active state of this object from a #GVariant serialization.
 * This is used by the on-disk binary cache.
 */
gboolean
as_artifact_set_from_variant (AsArtifact *artifact, GVariant *variant)
{
	AsArtifactPrivate *priv = GET_PRIVATE (artifact);
	g_auto(GVariantDict) dict;
	GVariant *tmp;

	g_variant_dict_init (&dict, variant);

	/* kind */
	priv->kind = as_variant_get_dict_uint32 (&dict, "type");

	/* platform */
	as_artifact_set_platform (artifact, as_variant_get_dict_mstr (&dict, "platform", &tmp));
	g_variant_unref (tmp);

	/* bundle kind */
	priv->bundle_kind = as_variant_get_dict_uint32 (&dict, "bundle_kind");

	/* locations */
	as_variant_to_string_ptrarray_by_dict (&dict,
						"locations",
						priv->locations);

	/* sizes */
	tmp = g_variant_dict_lookup_value (&dict, "sizes", G_VARIANT_TYPE_DICTIONARY);
	if (tmp != NULL) {
		GVariantIter iter;
		GVariant *inner_child;
		g_variant_iter_init (&iter, tmp);

		while ((inner_child = g_variant_iter_next_value (&iter))) {
			AsSizeKind kind;
			guint64 size;

			g_variant_get (inner_child, "{ut}", &kind, &size);
			as_artifact_set_size (artifact, size, kind);

			g_variant_unref (inner_child);
		}
		g_variant_unref (tmp);
	}

	/* checksums */
	tmp = g_variant_dict_lookup_value (&dict, "checksums", G_VARIANT_TYPE_DICTIONARY);
	if (tmp != NULL) {
		GVariantIter iter;
		GVariant *inner_child;
		g_variant_iter_init (&iter, tmp);

		while ((inner_child = g_variant_iter_next_value (&iter))) {
			g_autoptr(AsChecksum) cs = as_checksum_new ();
			if (as_checksum_set_from_variant (cs, inner_child))
				as_artifact_add_checksum (artifact, cs);

			g_variant_unref (inner_child);
		}
		g_variant_unref (tmp);
	}

	return TRUE;
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
