/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2024 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-developer
 * @short_description: Brief information about a component's developer
 *
 * Describes the developer of a component.
 *
 * See also: #AsComponent
 */

#include "config.h"
#include "as-developer.h"
#include "as-developer-private.h"

#include "as-utils-private.h"
#include "as-context-private.h"

typedef struct {
	gchar *id;
	GHashTable *name; /* localized entry */

	AsContext *context;
} AsDeveloperPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsDeveloper, as_developer, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_developer_get_instance_private (o))

/**
 * as_developer_init:
 **/
static void
as_developer_init (AsDeveloper *devp)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);

	priv->name = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    (GDestroyNotify) as_ref_string_release,
					    g_free);
}

/**
 * as_developer_finalize:
 **/
static void
as_developer_finalize (GObject *object)
{
	AsDeveloper *devp = AS_DEVELOPER (object);
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);

	g_free (priv->id);
	g_hash_table_unref (priv->name);

	if (priv->context != NULL)
		g_object_unref (priv->context);

	G_OBJECT_CLASS (as_developer_parent_class)->finalize (object);
}

/**
 * as_developer_class_init:
 **/
static void
as_developer_class_init (AsDeveloperClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_developer_finalize;
}

/**
 * as_developer_new:
 *
 * Creates a new #AsDeveloper.
 *
 * Returns: (transfer full): a #AsDeveloper
 *
 **/
AsDeveloper *
as_developer_new (void)
{
	AsDeveloper *devp;
	devp = g_object_new (AS_TYPE_DEVELOPER, NULL);
	return AS_DEVELOPER (devp);
}

/**
 * as_developer_new_with_context:
 *
 * Creates a new #AsDeveloper that is associated with
 * an #AsContext.
 *
 * Returns: (transfer full): a #AsDeveloper
 *
 **/
AsDeveloper *
as_developer_new_with_context (AsContext *ctx)
{
	AsDeveloper *devp = as_developer_new ();
	as_developer_set_context (devp, ctx);
	return devp;
}

/**
 * as_developer_get_context:
 * @devp: an #AsDeveloper instance.
 *
 * Returns: the #AsContext associated with this developer.
 * This function may return %NULL if no context is set.
 */
AsContext *
as_developer_get_context (AsDeveloper *devp)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	return priv->context;
}

/**
 * as_developer_set_context:
 * @devp: an #AsDeveloper instance.
 * @context: the #AsContext.
 *
 * Sets the document context this developer is associated
 * with.
 */
void
as_developer_set_context (AsDeveloper *devp, AsContext *context)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	if (priv->context != NULL)
		g_object_unref (priv->context);
	priv->context = g_object_ref (context);
}

/**
 * as_developer_get_id:
 * @devp: an #AsDeveloper instance.
 *
 * Gets a unique ID for this particular developer, e.g. "gnome" or "mozilla.org"
 *
 * Returns: the unique developer ID, or %NULL if none was set.
 */
const gchar *
as_developer_get_id (AsDeveloper *devp)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	return priv->id;
}

/**
 * as_developer_set_id:
 * @devp: an #AsDeveloper instance.
 * @id: a developer ID, e.g. "mozilla.org"
 *
 * Sets the unique ID of this developer.
 */
void
as_developer_set_id (AsDeveloper *devp, const gchar *id)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	if (priv->id == id)
		return;
	g_free (priv->id);
	priv->id = g_strdup (id);
}

/**
 * as_developer_get_name:
 * @devp: an #AsDeveloper instance.
 *
 * Get a localized developer, or development team name.
 *
 * Returns: the developer name.
 */
const gchar *
as_developer_get_name (AsDeveloper *devp)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	return as_context_localized_ht_get (priv->context, priv->name, NULL /* locale override */);
}

/**
 * as_developer_set_name:
 * @devp: an #AsDeveloper instance.
 * @value: the developer or developer team name
 * @locale: (nullable): the BCP47 locale, or %NULL. e.g. "en-GB"
 *
 * Set the the developer or development team name.
 */
void
as_developer_set_name (AsDeveloper *devp, const gchar *value, const gchar *locale)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	as_context_localized_ht_set (priv->context, priv->name, value, locale);
}

/**
 * as_developer_get_name_table:
 * @devp: an #AsDeveloper instance.
 */
GHashTable *
as_developer_get_name_table (AsDeveloper *devp)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	return priv->name;
}

/**
 * as_developer_load_from_xml:
 * @devp: a #AsDeveloper instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads devp data from an XML node.
 **/
gboolean
as_developer_load_from_xml (AsDeveloper *devp, AsContext *ctx, xmlNode *node, GError **error)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	g_autofree gchar *str = NULL;

	str = as_xml_get_prop_value (node, "id");
	if (priv->id != NULL)
		g_free (priv->id);
	priv->id = g_steal_pointer (&str);

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (as_str_equal0 (iter->name, "name")) {
			g_autofree gchar *lang = NULL;
			lang = as_xml_get_node_locale_match (ctx, iter);
			if (lang != NULL) {
				g_autofree gchar *content = as_xml_get_node_value (iter);
				as_developer_set_name (devp, content, lang);
			}
		}
	}

	as_developer_set_context (devp, ctx);
	return TRUE;
}

/**
 * as_developer_to_xml_node:
 * @devp: a #AsDeveloper instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_developer_to_xml_node (AsDeveloper *devp, AsContext *ctx, xmlNode *root)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);
	xmlNode *n_devp = NULL;

	if (g_hash_table_size (priv->name) == 0)
		return;

	n_devp = as_xml_add_node (root, "developer");

	if (priv->id != NULL)
		as_xml_add_text_prop (n_devp, "id", priv->id);

	as_xml_add_localized_text_node (n_devp, "name", priv->name);

	xmlAddChild (root, n_devp);
}

/**
 * as_developer_load_from_yaml:
 * @devp: a #AsDeveloper instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_developer_load_from_yaml (AsDeveloper *devp, AsContext *ctx, GNode *node, GError **error)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);

	for (GNode *n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);

		if (g_strcmp0 (key, "id") == 0) {
			as_developer_set_id (devp, as_yaml_node_get_value (n));

		} else if (g_strcmp0 (key, "name") == 0) {
			as_yaml_set_localized_table (ctx, n, priv->name);

		} else {
			as_yaml_print_unknown ("developer", key);
		}
	}

	as_developer_set_context (devp, ctx);
	return TRUE;
}

/**
 * as_developer_emit_yaml:
 * @devp: a #AsDeveloper instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_developer_emit_yaml (AsDeveloper *devp, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsDeveloperPrivate *priv = GET_PRIVATE (devp);

	if (g_hash_table_size (priv->name) == 0)
		return;

	as_yaml_emit_scalar (emitter, "Developer");
	as_yaml_mapping_start (emitter);

	as_yaml_emit_entry (emitter, "id", priv->id);
	as_yaml_emit_localized_entry (emitter, "name", priv->name);

	as_yaml_mapping_end (emitter);
}
