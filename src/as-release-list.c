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
 * SECTION:as-release-list
 * @short_description: Container for component releases and their metadata.
 * @include: appstream.h
 *
 * This class contains multiple #Asrelease entries as well as information
 * affecting all releases of that grouping.
 * It can also fetch the required release information on-demand from
 * a web URL in case it is not available locally.
 *
 * See also: #AsRelease, #AsRelease
 */

#include "config.h"
#include "as-release-list-private.h"

#include <gio/gio.h>

#include "as-macros.h"
#include "as-utils-private.h"
#include "as-context-private.h"
#include "as-release-private.h"

typedef struct {
	AsReleaseListKind kind;
	gchar *url;

	AsContext *context;
} AsReleaseListPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsReleaseList, as_release_list, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_release_list_get_instance_private (o))

/**
 * as_release_list_kind_to_string:
 * @kind: the #AsReleaseKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.16.0
 **/
const gchar *
as_release_list_kind_to_string (AsReleaseListKind kind)
{
	if (kind == AS_RELEASE_LIST_KIND_EMBEDDED)
		return "embedded";
	if (kind == AS_RELEASE_LIST_KIND_EXTERNAL)
		return "external";
	return "unknown";
}

/**
 * as_release_list_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: an #AsReleaseKind or %AS_RELEASE_KIND_UNKNOWN for unknown
 *
 * Since: 0.16.0
 **/
AsReleaseListKind
as_release_list_kind_from_string (const gchar *kind_str)
{
	if (as_is_empty (kind_str))
		return AS_RELEASE_LIST_KIND_EMBEDDED;
	if (as_str_equal0 (kind_str, "embedded"))
		return AS_RELEASE_LIST_KIND_EMBEDDED;
	if (as_str_equal0 (kind_str, "external"))
		return AS_RELEASE_LIST_KIND_EXTERNAL;
	return AS_RELEASE_LIST_KIND_UNKNOWN;
}

static void
as_release_list_init (AsReleaseList *rels)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);

	rels->entries = g_ptr_array_new_with_free_func (g_object_unref);
	priv->kind = AS_RELEASE_LIST_KIND_EMBEDDED;
}

static void
as_release_list_finalize (GObject *object)
{
	AsReleaseList *rels = AS_RELEASE_LIST (object);
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);

	g_ptr_array_unref (rels->entries);
	g_free (priv->url);

	if (priv->context != NULL)
		g_object_unref (priv->context);

	G_OBJECT_CLASS (as_release_list_parent_class)->finalize (object);
}

static void
as_release_list_class_init (AsReleaseListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_release_list_finalize;
}

/**
 * as_release_list_new:
 *
 * Creates a new #AsReleaseList.
 *
 * Returns: (transfer full): an #AsReleaseList
 *
 * Since: 1.0
 **/
AsReleaseList *
as_release_list_new (void)
{
	AsReleaseList *rels;
	rels = g_object_new (AS_TYPE_RELEASE_LIST, NULL);
	return AS_RELEASE_LIST (rels);
}

/**
 * as_release_list_index:
 * @rels: a #AsReleaseList
 * @index_: the index of the #AsRelease to return
 *
 * Returns the #AsRelease at the given index of the array.
 *
 * This does not perform bounds checking on the given @index_,
 * so you are responsible for checking it against the array length.
 * Use %as_release_list_len to determine the amount of releases
 * present in the #AsReleaseList container.
 *
 * Returns: (transfer none): the #AsRelease at the given index
 */

/**
 * as_release_list_len:
 * @rels: a #AsReleaseList
 *
 * Get the amount of release entries present.
 *
 * Returns: Amount of entries in #AsReleaseList.
 */

/**
 * as_release_list_get_entries:
 * @rels: An instance of #AsReleaseList.
 *
 * Get the release entries as #GPtrArray.
 *
 * Returns: (transfer none) (element-type AsRelease): an array of #AsRelease instances.
 */
GPtrArray *
as_release_list_get_entries (AsReleaseList *rels)
{
	return rels->entries;
}

/**
 * as_release_list_get_size:
 * @rels: An instance of #AsReleaseList.
 *
 * Get the amount of components in this box.
 *
 * Returns: Amount of components.
 */
guint
as_release_list_get_size (AsReleaseList *rels)
{
	return rels->entries->len;
}

/**
 * as_release_list_is_empty:
 * @rels: An instance of #AsReleaseList.
 *
 * Check if there are any components present.
 *
 * Returns: %TRUE if this component box is empty.
 */
gboolean
as_release_list_is_empty (AsReleaseList *rels)
{
	return rels->entries->len == 0;
}

/**
 * as_release_list_index_safe:
 * @rels: An instance of #AsReleaseList.
 * @index: The release entry index.
 *
 * Retrieve a release entry at the respective index from the
 * release entry list.
 *
 * Returns: (transfer none): An #AsRelease or %NULL
 */
AsRelease *
as_release_list_index_safe (AsReleaseList *rels, guint index)
{
	if (index >= rels->entries->len)
		return NULL;
	return as_release_list_index (rels, index);
}

/**
 * as_release_list_add:
 * @rels: An instance of #AsReleaseList.
 *
 * Append a release entry to this #AsReleaseList container.
 */
void
as_release_list_add (AsReleaseList *rels, AsRelease *release)
{
	g_ptr_array_add (rels->entries, g_object_ref (release));
}

/**
 * as_release_list_get_context:
 * @rels: a #AsReleaseList instance.
 *
 * Get the #AsContext associated with these releases.
 * This function may return %NULL if no context is set
 *
 * Returns: (transfer none) (nullable): the associated #AsContext or %NULL
 */
AsContext *
as_release_list_get_context (AsReleaseList *rels)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	return priv->context;
}

/**
 * as_release_list_set_context:
 * @rels: a #AsReleaseList instance.
 * @context: the #AsContext.
 *
 * Sets the document context these releases are associated with.
 */
void
as_release_list_set_context (AsReleaseList *rels, AsContext *context)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	if (priv->context != NULL)
		g_object_unref (priv->context);

	if (context == NULL) {
		priv->context = NULL;
		return;
	}

	priv->context = g_object_ref (context);
	for (guint i = 0; i < rels->entries->len; i++) {
		AsRelease *release = as_release_list_index (rels, i);
		as_release_set_context (release, priv->context);
	}
}

/**
 * as_release_list_get_kind:
 * @rels: a #AsReleaseList instance.
 *
 * Returns the #AsReleaseListKind of the release metadata
 * associated with this component.
 *
 * Returns: The kind.
 *
 * Since: 0.16.0
 */
AsReleaseListKind
as_release_list_get_kind (AsReleaseList *rels)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	return priv->kind;
}

/**
 * as_release_list_set_kind:
 * @rels: a #AsReleaseList instance.
 * @kind: the #AsComponentKind.
 *
 * Sets the #AsReleaseListKind of the release metadata
 * associated with this component.
 *
 * Since: 0.16.0
 */
void
as_release_list_set_kind (AsReleaseList *rels, AsReleaseListKind kind)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	priv->kind = kind;
}

/**
 * as_release_list_get_url:
 * @rels: a #AsReleaseList instance.
 *
 * Get the remote URL to obtain release information from.
 *
 * Returns: (nullable): The URL of external release data, or %NULL
 *
 * Since: 0.16.0
 **/
const gchar *
as_release_list_get_url (AsReleaseList *rels)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	return priv->url;
}

/**
 * as_release_list_set_url:
 * @rels: a #AsReleaseList instance.
 * @url: the web URL where release data is found.
 *
 * Set a remote URL pointing to an AppStream release info file.
 *
 * Since: 0.16.0
 **/
void
as_release_list_set_url (AsReleaseList *rels, const gchar *url)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	as_assign_string_safe (priv->url, url);
}

/**
 * as_release_compare:
 *
 * Callback for releases #GPtrArray sorting.
 *
 * NOTE: We sort in descending order here, so the most recent
 * release ends up at the top of the list.
 */
gint
as_release_compare (gconstpointer a, gconstpointer b)
{
	AsRelease **rel1 = (AsRelease **) a;
	AsRelease **rel2 = (AsRelease **) b;
	gint ret;

	/* compare version strings */
	ret = as_release_vercmp (*rel1, *rel2);
	if (ret == 0)
		return 0;
	return (ret >= 1) ? -1 : 1;
}

/**
 * as_release_list_sort:
 * @rels: a #AsReleaseList instance.
 *
 * Sort releases by their release version,
 * starting with the most recent release.
 */
void
as_release_list_sort (AsReleaseList *rels)
{
	g_ptr_array_sort (rels->entries, as_release_compare);
}

/**
 * as_release_list_clear:
 * @rels: a #AsReleaseList instance.
 *
 * Remove all release entries from this releases object.
 */
void
as_release_list_clear (AsReleaseList *rels)
{
	g_ptr_array_set_size (rels->entries, 0);
}

/**
 * as_release_list_set_size:
 * @rels: a #AsReleaseList instance.
 *
 * Set the amount of release entries stored.
 */
void
as_release_list_set_size (AsReleaseList *rels, guint size)
{
	g_ptr_array_set_size (rels->entries, size);
}

/**
 * as_release_list_load_from_bytes:
 * @rels: a #AsReleaseList instance.
 * @context: (nullable): the attached #AsContext or %NULL to use the current context
 * @bytes: the release XML data as #GBytes
 * @error: a #GError.
 *
 * Load release information from XML bytes.
 *
 * Returns: %TRUE on success.
 *
 * Since: 0.16.0
 **/
gboolean
as_release_list_load_from_bytes (AsReleaseList *rels,
				 AsContext *context,
				 GBytes *bytes,
				 GError **error)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	const gchar *rel_data = NULL;
	gsize rel_data_len;
	xmlDoc *xdoc;
	xmlNode *xroot;
	GError *tmp_error = NULL;

	if (context != NULL)
		as_release_list_set_context (rels, context);

	rel_data = g_bytes_get_data (bytes, &rel_data_len);
	xdoc = as_xml_parse_document (rel_data,
				      rel_data_len,
				      FALSE, /* pedantic */
				      &tmp_error);
	if (xdoc == NULL) {
		g_propagate_prefixed_error (error,
					    tmp_error,
					    "Unable to parse external release data: ");
		return FALSE;
	}

	/* load releases */
	xroot = xmlDocGetRootElement (xdoc);
	for (xmlNode *iter = xroot->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (as_str_equal0 (iter->name, "release")) {
			g_autoptr(AsRelease) release = as_release_new ();
			if (as_release_load_from_xml (release, priv->context, iter, NULL))
				g_ptr_array_add (rels->entries, g_steal_pointer (&release));
		}
	}
	xmlFreeDoc (xdoc);

	return TRUE;
}

/**
 * as_release_list_load:
 * @rels: a #AsReleaseList instance.
 * @cpt: the component to load the data for.
 * @reload: set to %TRUE to discard existing data and reload.
 * @allow_net: allow fetching release data from the internet.
 * @error: a #GError.
 *
 * Load data from an external source, possibly a local file
 * or a network resource.
 *
 * Returns: %TRUE on success.
 *
 * Since: 0.16.0
 **/
gboolean
as_release_list_load (AsReleaseList *rels,
		      AsComponent *cpt,
		      gboolean reload,
		      gboolean allow_net,
		      GError **error)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	g_autoptr(GBytes) reldata_bytes = NULL;
	GError *tmp_error = NULL;

	if (priv->kind != AS_RELEASE_LIST_KIND_EXTERNAL)
		return TRUE;
	if (rels->entries->len != 0 && !reload)
		return TRUE;

	/* we need context data for this to work properly */
	if (priv->context == NULL) {
		g_set_error_literal (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Unable to read external release information from a component "
				     "without metadata context.");
		return FALSE;
	}

	if (reload)
		g_ptr_array_set_size (rels->entries, 0);

	if (allow_net && priv->url != NULL) {
		/* grab release data from a remote source */
		g_autoptr(AsCurl) curl = NULL;

		curl = as_context_get_curl (priv->context, error);
		if (curl == NULL)
			return FALSE;

		reldata_bytes = as_curl_download_bytes (curl, priv->url, &tmp_error);
		if (reldata_bytes == NULL) {
			g_propagate_prefixed_error (
			    error,
			    tmp_error,
			    "Unable to obtain remote external release data: ");
			return FALSE;
		}
	} else {
		/* read release data from a local source */
		g_autofree gchar *relfile_path = NULL;
		g_autofree gchar *relfile_name = NULL;
		g_autofree gchar *tmp = NULL;
		gchar *rel_data = NULL;
		gsize rel_data_len;
		const gchar *mi_fname = NULL;

		mi_fname = as_context_get_filename (priv->context);
		if (mi_fname == NULL) {
			g_set_error_literal (error,
					     AS_UTILS_ERROR,
					     AS_UTILS_ERROR_FAILED,
					     "Unable to read external release information: "
					     "Component has no known metainfo filename.");
			return FALSE;
		}
		relfile_name = g_strconcat (as_component_get_id (cpt), ".releases.xml", NULL);
		tmp = g_path_get_dirname (mi_fname);
		relfile_path = g_build_filename (tmp, "releases", relfile_name, NULL);

		if (!g_file_get_contents (relfile_path, &rel_data, &rel_data_len, &tmp_error)) {
			g_propagate_prefixed_error (error,
						    tmp_error,
						    "Unable to read local external release data: ");
			return FALSE;
		}

		reldata_bytes = g_bytes_new_take (rel_data, rel_data_len);
	}

	return as_release_list_load_from_bytes (rels, NULL, reldata_bytes, error);
}

/**
 * as_release_list_load_from_xml:
 * @rels: an #AsReleaseList instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads artifact data from an XML node.
 **/
gboolean
as_release_list_load_from_xml (AsReleaseList *rels, AsContext *ctx, xmlNode *node, GError **error)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);
	g_autofree gchar *releases_kind_str = NULL;

	/* clear any existing entries */
	as_release_list_clear (rels);

	/* set new context */
	as_release_list_set_context (rels, ctx);

	/* load new releases */
	releases_kind_str = as_xml_get_prop_value (node, "type");
	priv->kind = as_release_list_kind_from_string (releases_kind_str);
	if (priv->kind == AS_RELEASE_LIST_KIND_EXTERNAL) {
		g_autofree gchar *release_url_prop = as_xml_get_prop_value (node, "url");
		if (release_url_prop != NULL) {
			g_free (priv->url);
			/* handle the media baseurl */
			if (as_context_has_media_baseurl (ctx))
				priv->url = g_strconcat (as_context_get_media_baseurl (ctx),
							 "/",
							 release_url_prop,
							 NULL);
			else
				priv->url = g_steal_pointer (&release_url_prop);
		}
	}

	/* only read release data if it is not external */
	if (priv->kind != AS_RELEASE_LIST_KIND_EXTERNAL) {
		for (xmlNode *iter2 = node->children; iter2 != NULL; iter2 = iter2->next) {
			if (iter2->type != XML_ELEMENT_NODE)
				continue;
			if (g_strcmp0 ((const gchar *) iter2->name, "release") == 0) {
				g_autoptr(AsRelease) release = as_release_new ();
				if (as_release_load_from_xml (release, ctx, iter2, NULL))
					g_ptr_array_add (rels->entries, g_steal_pointer (&release));
			}
		}
	}

	return TRUE;
}

/**
 * as_release_list_to_xml_node:
 * @rels: an #AsReleaseList instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_release_list_to_xml_node (AsReleaseList *rels, AsContext *ctx, xmlNode *root)
{
	AsReleaseListPrivate *priv = GET_PRIVATE (rels);

	if (priv->kind == AS_RELEASE_LIST_KIND_EXTERNAL &&
	    as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) {
		xmlNode *rnode = as_xml_add_node (root, "releases");
		as_xml_add_text_prop (rnode, "type", "external");
		if (priv->url != NULL)
			as_xml_add_text_prop (rnode, "url", priv->url);
	} else if (rels->entries->len > 0) {
		xmlNode *rnode = as_xml_add_node (root, "releases");

		/* ensure releases are sorted, then emit XML nodes */
		as_release_list_sort (rels);
		for (guint i = 0; i < rels->entries->len; i++) {
			AsRelease *rel = AS_RELEASE (g_ptr_array_index (rels->entries, i));
			as_release_to_xml_node (rel, ctx, rnode);
		}
	}
}

/**
 * as_release_list_load_from_yaml:
 * @rels: an #AsReleaseList instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_release_list_load_from_yaml (AsReleaseList *rels, AsContext *ctx, GNode *node, GError **error)
{
	/* set new context */
	as_release_list_set_context (rels, ctx);

	for (GNode *n = node->children; n != NULL; n = n->next) {
		g_autoptr(AsRelease) release = as_release_new ();
		if (as_release_load_from_yaml (release, ctx, n, NULL))
			g_ptr_array_add (rels->entries, g_steal_pointer (&release));
	}

	return TRUE;
}

/**
 * as_release_list_emit_yaml:
 * @rels: an #AsReleaseList instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_release_list_emit_yaml (AsReleaseList *rels, AsContext *ctx, yaml_emitter_t *emitter)
{
	if (rels->entries->len == 0)
		return;

	/* ensure releases are sorted */
	as_release_list_sort (rels);

	as_yaml_emit_scalar (emitter, "Releases");
	as_yaml_sequence_start (emitter);

	for (guint i = 0; i < rels->entries->len; i++) {
		AsRelease *release = AS_RELEASE (g_ptr_array_index (rels->entries, i));
		as_release_emit_yaml (release, ctx, emitter);
	}

	as_yaml_sequence_end (emitter);
}
