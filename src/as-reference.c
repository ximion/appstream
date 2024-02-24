/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-reference
 * @short_description: Describe external references to a component
 *
 * Contains information about external references to the
 * component this reference is associated with.
 *
 * See also: #AsComponent
 */

#include "config.h"
#include "as-reference.h"
#include "as-reference-private.h"

#include "as-utils-private.h"
#include "as-context-private.h"

typedef struct {
	AsReferenceKind kind;

	gchar *value;
	gchar *registry_name;
} AsReferencePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsReference, as_reference, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_reference_get_instance_private (o))

/**
 * as_reference_kind_to_string:
 * @kind: the %AsReferenceKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 1.0.0
 **/
const gchar *
as_reference_kind_to_string (AsReferenceKind kind)
{
	if (kind == AS_REFERENCE_KIND_DOI)
		return "doi";
	if (kind == AS_REFERENCE_KIND_CITATION_CFF)
		return "citation_cff";
	if (kind == AS_REFERENCE_KIND_REGISTRY)
		return "registry";
	return "unknown";
}

/**
 * as_reference_kind_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a AsReferenceKind or %AS_REFERENCE_KIND_UNKNOWN for unknown
 **/
AsReferenceKind
as_reference_kind_from_string (const gchar *str)
{
	if (as_str_equal0 (str, "doi"))
		return AS_REFERENCE_KIND_DOI;
	if (as_str_equal0 (str, "citation_cff"))
		return AS_REFERENCE_KIND_CITATION_CFF;
	if (as_str_equal0 (str, "registry"))
		return AS_REFERENCE_KIND_REGISTRY;

	return AS_REFERENCE_KIND_UNKNOWN;
}

/**
 * as_reference_init:
 **/
static void
as_reference_init (AsReference *reference)
{
}

/**
 * as_reference_finalize:
 **/
static void
as_reference_finalize (GObject *object)
{
	AsReference *reference = AS_REFERENCE (object);
	AsReferencePrivate *priv = GET_PRIVATE (reference);

	g_free (priv->value);
	g_free (priv->registry_name);

	G_OBJECT_CLASS (as_reference_parent_class)->finalize (object);
}

/**
 * as_reference_class_init:
 **/
static void
as_reference_class_init (AsReferenceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_reference_finalize;
}

/**
 * as_reference_new:
 *
 * Creates a new #AsReference.
 *
 * Returns: (transfer full): a #AsReference
 *
 **/
AsReference *
as_reference_new (void)
{
	AsReference *reference;
	reference = g_object_new (AS_TYPE_REFERENCE, NULL);
	return AS_REFERENCE (reference);
}

/**
 * as_reference_get_kind:
 * @reference: an #AsReference instance.
 *
 * Gets the reference kind.
 *
 * Returns: the #AsReferenceKind
 *
 * Since: 1.0.0
 */
AsReferenceKind
as_reference_get_kind (AsReference *reference)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);
	return priv->kind;
}

/**
 * as_reference_set_kind:
 * @reference: an #AsReference instance.
 * @kind: the #AsReferenceKind, e.g. %AS_REFERENCE_KIND_DOI.
 *
 * Sets the reference kind.
 *
 * Since: 1.0.0
 **/
void
as_reference_set_kind (AsReference *reference, AsReferenceKind kind)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);
	priv->kind = kind;
}

/**
 * as_reference_get_value:
 * @reference: an #AsReference instance.
 *
 * Gets the value of this reference, e.g. a DOI if the
 * reference kind is %AS_REFERENCE_KIND_DOI or an URL
 * for %AS_REFERENCE_KIND_CITATION_CFF.
 *
 * Returns: the value of this reference.
 */
const gchar *
as_reference_get_value (AsReference *reference)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);
	return priv->value;
}

/**
 * as_reference_set_value:
 * @reference: an #AsReference instance.
 * @value: a value for this reference, e.g. "10.1000/182"
 *
 * Sets a value for this reference.
 */
void
as_reference_set_value (AsReference *reference, const gchar *value)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);
	if (priv->value == value)
		return;
	g_free (priv->value);
	priv->value = g_strdup (value);
}

/**
 * as_reference_get_registry_name:
 * @reference: an #AsReference instance.
 *
 * Gets the name of the registry this reference is for,
 * if the reference is of type %AS_REFERENCE_KIND_REGISTRY.
 * Otherwise return %NULL.
 *
 * Returns: (nullable): the registry name.
 */
const gchar *
as_reference_get_registry_name (AsReference *reference)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);
	return priv->registry_name;
}

/**
 * as_reference_set_registry_name:
 * @reference: an #AsReference instance.
 * @name: name of an external registry.
 *
 * Sets a name of a registry if this reference is of
 * type %AS_REFERENCE_KIND_REGISTRY.
 */
void
as_reference_set_registry_name (AsReference *reference, const gchar *name)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);
	if (priv->registry_name == name)
		return;
	g_free (priv->registry_name);
	priv->registry_name = g_strdup (name);
}

/**
 * as_reference_load_from_xml:
 * @reference: a #AsReference instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads reference data from an XML node.
 **/
gboolean
as_reference_load_from_xml (AsReference *reference, AsContext *ctx, xmlNode *node, GError **error)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);

	if (as_str_equal0 (node->name, "doi")) {
		priv->kind = AS_REFERENCE_KIND_DOI;
		g_free (priv->value);
		priv->value = as_xml_get_node_value (node);
	} else if (as_str_equal0 (node->name, "citation_cff")) {
		priv->kind = AS_REFERENCE_KIND_CITATION_CFF;
		g_free (priv->value);
		priv->value = as_xml_get_node_value (node);
	} else if (as_str_equal0 (node->name, "registry")) {
		priv->kind = AS_REFERENCE_KIND_REGISTRY;

		g_free (priv->registry_name);
		priv->registry_name = as_xml_get_prop_value (node, "name");
		if (priv->registry_name == NULL)
			return FALSE;

		g_free (priv->value);
		priv->value = as_xml_get_node_value (node);
	}

	return TRUE;
}

/**
 * as_reference_to_xml_node:
 * @reference: a #AsReference instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_reference_to_xml_node (AsReference *reference, AsContext *ctx, xmlNode *root)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);
	xmlNode *n_reference = NULL;

	if (priv->kind == AS_REFERENCE_KIND_UNKNOWN)
		return;
	if (priv->kind == AS_REFERENCE_KIND_REGISTRY && priv->registry_name == NULL)
		return;
	if (priv->value == NULL)
		return;

	n_reference = as_xml_add_text_node (root,
					    as_reference_kind_to_string (priv->kind),
					    priv->value);

	if (priv->kind == AS_REFERENCE_KIND_REGISTRY)
		as_xml_add_text_prop (n_reference, "name", priv->registry_name);

	xmlAddChild (root, n_reference);
}

/**
 * as_reference_load_from_yaml:
 * @reference: a #AsReference instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_reference_load_from_yaml (AsReference *reference, AsContext *ctx, GNode *node, GError **error)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);

	for (GNode *n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);

		if (as_str_equal0 (key, "type")) {
			priv->kind = as_reference_kind_from_string (as_yaml_node_get_value (n));

		} else if (as_str_equal0 (key, "value")) {
			as_reference_set_value (reference, as_yaml_node_get_value (n));

		} else if (as_str_equal0 (key, "registry")) {
			as_reference_set_registry_name (reference, as_yaml_node_get_value (n));

		} else {
			as_yaml_print_unknown ("reference", key);
		}
	}

	if (priv->kind == AS_REFERENCE_KIND_UNKNOWN)
		return FALSE;
	if (priv->kind == AS_REFERENCE_KIND_REGISTRY && priv->registry_name == NULL)
		return FALSE;
	if (priv->value == NULL)
		return FALSE;

	return TRUE;
}

/**
 * as_reference_emit_yaml:
 * @reference: a #AsReference instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_reference_emit_yaml (AsReference *reference, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsReferencePrivate *priv = GET_PRIVATE (reference);

	if (priv->kind == AS_REFERENCE_KIND_UNKNOWN)
		return;
	if (priv->kind == AS_REFERENCE_KIND_REGISTRY && priv->registry_name == NULL)
		return;
	if (priv->value == NULL)
		return;

	as_yaml_mapping_start (emitter);

	as_yaml_emit_entry (emitter, "type", as_reference_kind_to_string (priv->kind));
	as_yaml_emit_entry (emitter, "value", priv->value);
	if (priv->kind == AS_REFERENCE_KIND_REGISTRY)
		as_yaml_emit_entry (emitter, "registry", priv->registry_name);

	as_yaml_mapping_end (emitter);
}
