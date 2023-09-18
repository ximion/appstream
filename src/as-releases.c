/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2023 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-releases
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
#include "as-releases-private.h"

#include <gio/gio.h>

#include "as-macros.h"
#include "as-utils-private.h"
#include "as-context-private.h"
#include "as-release-private.h"

typedef struct {
	AsReleasesKind kind;
	gchar *url;

	AsContext *context;
} AsReleasesPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsReleases, as_releases, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_releases_get_instance_private (o))

/**
 * as_releases_kind_to_string:
 * @kind: the #AsReleaseKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.16.0
 **/
const gchar *
as_releases_kind_to_string (AsReleasesKind kind)
{
	if (kind == AS_RELEASES_KIND_EMBEDDED)
		return "embedded";
	if (kind == AS_RELEASES_KIND_EXTERNAL)
		return "external";
	return "unknown";
}

/**
 * as_releases_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: an #AsReleaseKind or %AS_RELEASE_KIND_UNKNOWN for unknown
 *
 * Since: 0.16.0
 **/
AsReleasesKind
as_releases_kind_from_string (const gchar *kind_str)
{
	if (as_is_empty (kind_str))
		return AS_RELEASES_KIND_EMBEDDED;
	if (as_str_equal0 (kind_str, "embedded"))
		return AS_RELEASES_KIND_EMBEDDED;
	if (as_str_equal0 (kind_str, "external"))
		return AS_RELEASES_KIND_EXTERNAL;
	return AS_RELEASES_KIND_UNKNOWN;
}

static void
as_releases_init (AsReleases *rels)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);

	rels->entries = g_ptr_array_new_with_free_func (g_object_unref);
	priv->kind = AS_RELEASES_KIND_EMBEDDED;
}

static void
as_releases_finalize (GObject *object)
{
	AsReleases *rels = AS_RELEASES (object);
	AsReleasesPrivate *priv = GET_PRIVATE (rels);

	g_ptr_array_unref (rels->entries);
	g_free (priv->url);

	if (priv->context != NULL)
		g_object_unref (priv->context);

	G_OBJECT_CLASS (as_releases_parent_class)->finalize (object);
}

static void
as_releases_class_init (AsReleasesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_releases_finalize;
}

/**
 * as_releases_new:
 *
 * Creates a new #AsReleases.
 *
 * Returns: (transfer full): an #AsReleases
 *
 * Since: 1.0
 **/
AsReleases *
as_releases_new (void)
{
	AsReleases *rels;
	rels = g_object_new (AS_TYPE_RELEASES, NULL);
	return AS_RELEASES (rels);
}

/**
 * as_releases_index:
 * @rels: a #AsReleases
 * @index_: the index of the #AsRelease to return
 *
 * Returns the #AsRelease at the given index of the array.
 *
 * This does not perform bounds checking on the given @index_,
 * so you are responsible for checking it against the array length.
 * Use %as_releases_len to determine the amount of releases
 * present in the #AsReleases container.
 *
 * Returns: (transfer none): the #AsRelease at the given index
 */

/**
 * as_releases_len:
 * @rels: a #AsReleases
 *
 * Get the amount of release entries present.
 *
 * Returns: Amount of entries in #AsReleases.
 */

/**
 * as_releases_get_entries:
 * @rels: An instance of #AsReleases.
 *
 * Get the release entries as #GPtrArray.
 *
 * Returns: (transfer none) (element-type AsRelease): an array of #AsRelease instances.
 */
GPtrArray *
as_releases_get_entries (AsReleases *rels)
{
	return rels->entries;
}

/**
 * as_releases_get_size:
 * @rels: An instance of #AsReleases.
 *
 * Get the amount of components in this box.
 *
 * Returns: Amount of components.
 */
guint
as_releases_get_size (AsReleases *rels)
{
	return rels->entries->len;
}

/**
 * as_releases_is_empty:
 * @rels: An instance of #AsReleases.
 *
 * Check if there are any components present.
 *
 * Returns: %TRUE if this component box is empty.
 */
gboolean
as_releases_is_empty (AsReleases *rels)
{
	return rels->entries->len == 0;
}

/**
 * as_releases_index_safe:
 * @rels: An instance of #AsReleases.
 * @index: The release entry index.
 *
 * Retrieve a release entry at the respective index from the
 * release entry list.
 *
 * Returns: (transfer none): An #AsRelease or %NULL
 */
AsRelease *
as_releases_index_safe (AsReleases *rels, guint index)
{
	if (index >= rels->entries->len)
		return NULL;
	return as_releases_index (rels, index);
}

/**
 * as_releases_add:
 * @rels: An instance of #AsReleases.
 *
 * Append a release entry to this #AsReleases container.
 */
void
as_releases_add (AsReleases *rels, AsRelease *release)
{
	g_ptr_array_add (rels->entries, g_object_ref (release));
}

/**
 * as_releases_get_context:
 * @rels: a #AsReleases instance.
 *
 * Get the #AsContext associated with these releases.
 * This function may return %NULL if no context is set
 *
 * Returns: (transfer none) (nullable): the associated #AsContext or %NULL
 */
AsContext *
as_releases_get_context (AsReleases *rels)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
	return priv->context;
}

/**
 * as_releases_set_context:
 * @rels: a #AsReleases instance.
 * @context: the #AsContext.
 *
 * Sets the document context these releases are associated with.
 */
void
as_releases_set_context (AsReleases *rels, AsContext *context)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
	if (priv->context != NULL)
		g_object_unref (priv->context);

	if (context == NULL) {
		priv->context = NULL;
		return;
	}

	priv->context = g_object_ref (context);
	for (guint i = 0; i < rels->entries->len; i++) {
		AsRelease *release = as_releases_index (rels, i);
		as_release_set_context (release, priv->context);
	}
}

/**
 * as_releases_get_kind:
 * @rels: a #AsReleases instance.
 *
 * Returns the #AsReleasesKind of the release metadata
 * associated with this component.
 *
 * Returns: The kind.
 *
 * Since: 0.16.0
 */
AsReleasesKind
as_releases_get_kind (AsReleases *rels)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
	return priv->kind;
}

/**
 * as_releases_set_kind:
 * @rels: a #AsReleases instance.
 * @kind: the #AsComponentKind.
 *
 * Sets the #AsReleasesKind of the release metadata
 * associated with this component.
 *
 * Since: 0.16.0
 */
void
as_releases_set_kind (AsReleases *rels, AsReleasesKind kind)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
	priv->kind = kind;
}

/**
 * as_releases_get_url:
 * @rels: a #AsReleases instance.
 *
 * Get the remote URL to obtain release information from.
 *
 * Returns: (nullable): The URL of external release data, or %NULL
 *
 * Since: 0.16.0
 **/
const gchar *
as_releases_get_url (AsReleases *rels)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
	return priv->url;
}

/**
 * as_releases_set_url:
 * @rels: a #AsReleases instance.
 * @url: the web URL where release data is found.
 *
 * Set a remote URL pointing to an AppStream release info file.
 *
 * Since: 0.16.0
 **/
void
as_releases_set_url (AsReleases *rels, const gchar *url)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
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
 * as_releases_sort:
 * @rels: a #AsReleases instance.
 *
 * Sort releases by their release version,
 * starting with the most recent release.
 */
void
as_releases_sort (AsReleases *rels)
{
	g_ptr_array_sort (rels->entries, as_release_compare);
}

/**
 * as_releases_clear:
 * @rels: a #AsReleases instance.
 *
 * Remove all release entries from this releases object.
 */
void
as_releases_clear (AsReleases *rels)
{
	g_ptr_array_set_size (rels->entries, 0);
}

/**
 * as_releases_set_size:
 * @rels: a #AsReleases instance.
 *
 * Set the amount of release entries stored.
 */
void
as_releases_set_size (AsReleases *rels, guint size)
{
	g_ptr_array_set_size (rels->entries, size);
}

/**
 * as_releases_load_from_bytes:
 * @rels: a #AsReleases instance.
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
as_releases_load_from_bytes (AsReleases *rels, GBytes *bytes, GError **error)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
	const gchar *rel_data = NULL;
	gsize rel_data_len;
	xmlDoc *xdoc;
	xmlNode *xroot;
	GError *tmp_error = NULL;

	rel_data = g_bytes_get_data (bytes, &rel_data_len);
	xdoc = as_xml_parse_document (rel_data, rel_data_len, &tmp_error);
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
 * as_releases_load:
 * @rels: a #AsReleases instance.
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
as_releases_load (AsReleases *rels,
		  AsComponent *cpt,
		  gboolean reload,
		  gboolean allow_net,
		  GError **error)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
	g_autoptr(GBytes) reldata_bytes = NULL;
	GError *tmp_error = NULL;

	if (priv->kind != AS_RELEASES_KIND_EXTERNAL)
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

	return as_releases_load_from_bytes (rels, reldata_bytes, error);
}

/**
 * as_releases_load_from_xml:
 * @rels: an #AsReleases instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads artifact data from an XML node.
 **/
gboolean
as_releases_load_from_xml (AsReleases *rels, AsContext *ctx, xmlNode *node, GError **error)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);
	g_autofree gchar *releases_kind_str = NULL;

	/* clear any existing entries */
	as_releases_clear (rels);

	/* set new context */
	as_releases_set_context (rels, ctx);

	/* load new releases */
	releases_kind_str = as_xml_get_prop_value (node, "type");
	priv->kind = as_releases_kind_from_string (releases_kind_str);
	if (priv->kind == AS_RELEASES_KIND_EXTERNAL) {
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
	if (priv->kind != AS_RELEASES_KIND_EXTERNAL) {
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
 * as_releases_to_xml_node:
 * @rels: an #AsReleases instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_releases_to_xml_node (AsReleases *rels, AsContext *ctx, xmlNode *root)
{
	AsReleasesPrivate *priv = GET_PRIVATE (rels);

	if (priv->kind == AS_RELEASES_KIND_EXTERNAL &&
	    as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) {
		xmlNode *rnode = as_xml_add_node (root, "releases");
		as_xml_add_text_prop (rnode, "type", "external");
		if (priv->url != NULL)
			as_xml_add_text_prop (rnode, "url", priv->url);
	} else if (rels->entries->len > 0) {
		xmlNode *rnode = as_xml_add_node (root, "releases");

		/* ensure releases are sorted, then emit XML nodes */
		as_releases_sort (rels);
		for (guint i = 0; i < rels->entries->len; i++) {
			AsRelease *rel = AS_RELEASE (g_ptr_array_index (rels->entries, i));
			as_release_to_xml_node (rel, ctx, rnode);
		}
	}
}

/**
 * as_releases_load_from_yaml:
 * @rels: an #AsReleases instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_releases_load_from_yaml (AsReleases *rels, AsContext *ctx, GNode *node, GError **error)
{
	/* set new context */
	as_releases_set_context (rels, ctx);

	for (GNode *n = node->children; n != NULL; n = n->next) {
		g_autoptr(AsRelease) release = as_release_new ();
		if (as_release_load_from_yaml (release, ctx, n, NULL))
			g_ptr_array_add (rels->entries, g_steal_pointer (&release));
	}

	return TRUE;
}

/**
 * as_releases_emit_yaml:
 * @rels: an #AsReleases instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_releases_emit_yaml (AsReleases *rels, AsContext *ctx, yaml_emitter_t *emitter)
{
	if (rels->entries->len == 0)
		return;

	/* ensure releases are sorted */
	as_releases_sort (rels);

	as_yaml_emit_scalar (emitter, "Releases");
	as_yaml_sequence_start (emitter);

	for (guint i = 0; i < rels->entries->len; i++) {
		AsRelease *release = AS_RELEASE (g_ptr_array_index (rels->entries, i));
		as_release_emit_yaml (release, ctx, emitter);
	}

	as_yaml_sequence_end (emitter);
}
