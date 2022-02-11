/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-result
 * @short_description: A composer result for a single unit.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-result.h"

#include "as-utils-private.h"
#include "asc-globals-private.h"
#include "asc-utils.h"
#include "asc-hint.h"

typedef struct
{
	AsBundleKind	bundle_kind;
	gchar		*bundle_id;

	GHashTable	*cpts; /* GRefString->AsComponent */
	GHashTable	*mdata_hashes; /* AsComponent->utf8 */
	GHashTable	*hints; /* GRefString->GPtrArray */
	GHashTable	*gcids; /* GRefString->utf8 (component-id -> global component-id) */
} AscResultPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscResult, asc_result, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_result_get_instance_private (o))

static void
asc_result_init (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);

	priv->bundle_kind = AS_BUNDLE_KIND_UNKNOWN;

	priv->cpts = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    (GDestroyNotify) as_ref_string_release,
					    g_object_unref);
	priv->mdata_hashes = g_hash_table_new_full (g_direct_hash,
						    g_direct_equal,
						    NULL,
						    g_free);
	priv->hints = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     (GDestroyNotify) as_ref_string_release,
					     (GDestroyNotify) g_ptr_array_unref);
	priv->gcids = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     (GDestroyNotify) as_ref_string_release,
					     g_free);
}

static void
asc_result_finalize (GObject *object)
{
	AscResult *result = ASC_RESULT (object);
	AscResultPrivate *priv = GET_PRIVATE (result);

	g_free (priv->bundle_id);

	g_hash_table_unref (priv->cpts);
	g_hash_table_unref (priv->mdata_hashes);
	g_hash_table_unref (priv->hints);
	g_hash_table_unref (priv->gcids);

	G_OBJECT_CLASS (asc_result_parent_class)->finalize (object);
}

static void
asc_result_class_init (AscResultClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_result_finalize;
}

/**
 * asc_result_unit_ignored:
 * @result: an #AscResult instance.
 *
 * Returns: %TRUE if this result means the analyzed unit was ignored entirely.
 **/
gboolean
asc_result_unit_ignored (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return (g_hash_table_size (priv->cpts) == 0) && (g_hash_table_size (priv->hints) == 0);
}

/**
 * asc_result_components_count:
 * @result: an #AscResult instance.
 *
 * Returns: The amount of components found for this unit.
 **/
guint
asc_result_components_count (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return g_hash_table_size (priv->cpts);
}

/**
 * asc_result_hints_count:
 * @result: an #AscResult instance.
 *
 * Returns: The amount of hints emitted for this unit.
 **/
guint
asc_result_hints_count (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	GHashTableIter iter;
	gpointer value;
	guint count = 0;

	g_hash_table_iter_init (&iter, priv->hints);
	while (g_hash_table_iter_next (&iter, NULL, &value))
		count += ((GPtrArray*) value)->len;
	return count;
}

/**
 * asc_result_is_ignored:
 * @result: an #AscResult instance.
 * @cpt: the component to check for.
 *
 * Check if an #AsComponent was set to be ignored in this result
 * (usually due to errors).
 *
 * Returns: %TRUE if the component is ignored.
 **/
gboolean
asc_result_is_ignored (AscResult *result, AsComponent *cpt)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return !g_hash_table_contains (priv->cpts,
				       as_component_get_id (cpt));
}

/**
 * asc_result_get_bundle_kind:
 * @result: an #AscResult instance.
 *
 * Gets the bundle kind these results are for.
 **/
AsBundleKind
asc_result_get_bundle_kind (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return priv->bundle_kind;
}

/**
 * asc_result_set_bundle_kind:
 * @result: an #AscResult instance.
 *
 * Sets the kind of the bundle these results are for.
 **/
void
asc_result_set_bundle_kind (AscResult *result, AsBundleKind kind)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	priv->bundle_kind = kind;
}

/**
 * asc_result_get_bundle_id:
 * @result: an #AscResult instance.
 *
 * Gets the ID name of the bundle (a package / Flatpak / any entity containing metadata)
 * that these these results are generated for.
 **/
const gchar*
asc_result_get_bundle_id (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return priv->bundle_id;
}

/**
 * asc_result_set_bundle_id:
 * @result: an #AscResult instance.
 * @id: The new ID.
 *
 * Sets the name of the bundle these results are for.
 **/
void
asc_result_set_bundle_id (AscResult *result, const gchar *id)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	g_free (priv->bundle_id);
	priv->bundle_id = g_strdup (id);
}

/**
 * asc_result_get_component:
 * @result: an #AscResult instance.
 * @cid: Component ID to look for.
 *
 * Gets the component by its component-id-
 *
 * returns: (transfer none): An #AsComponent
 **/
AsComponent*
asc_result_get_component (AscResult *result, const gchar *cid)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return g_hash_table_lookup (priv->cpts, cid);
}

/**
 * asc_result_fetch_components:
 * @result: an #AscResult instance.
 *
 * Gets all components this #AsResult instance contains.
 *
 * Returns: (transfer container) (element-type AsComponent) : An array of #AsComponent
 **/
GPtrArray*
asc_result_fetch_components (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	GPtrArray *res;
	GHashTableIter iter;
	gpointer value;

	res = g_ptr_array_new_full (g_hash_table_size (priv->cpts),
				    g_object_unref);

	g_hash_table_iter_init (&iter, priv->cpts);
	while (g_hash_table_iter_next (&iter, NULL, &value))
		g_ptr_array_add (res, g_object_ref (AS_COMPONENT (value)));
	return res;
}

/**
 * asc_result_get_hints:
 * @result: an #AscResult instance.
 * @cid: Component ID to look for.
 *
 * Gets hints for a component with the given component-id.
 *
 * Returns: (transfer none) (element-type AscHint): An array of #AscHint or %NULL
 **/
GPtrArray*
asc_result_get_hints (AscResult *result, const gchar *cid)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return g_hash_table_lookup (priv->hints, cid);
}

/**
 * asc_result_fetch_hints_all:
 * @result: an #AscResult instance.
 *
 * Get a list of all hints for all components that are registered with this result.
 *
 * Returns: (transfer container) (element-type AscHint) : An array of #AscHint
 **/
GPtrArray*
asc_result_fetch_hints_all (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	GPtrArray *res;
	GHashTableIter iter;
	gpointer value;

	res = g_ptr_array_new_full (g_hash_table_size (priv->hints),
				    g_object_unref);

	g_hash_table_iter_init (&iter, priv->hints);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		GPtrArray *hints = value;
		for (guint i = 0; i < hints->len; i++)
			g_ptr_array_add (res,
					 g_object_ref (ASC_HINT (g_ptr_array_index (hints, i))));
	}
	return res;
}

/**
 * asc_result_get_component_ids_with_hints:
 * @result: an #AscResult instance.
 *
 * Gets list of component-IDs which do have issue hints associated with them.
 *
 * Returns: (transfer container): An array of component-IDs. Free container with %g_free
 **/
const gchar**
asc_result_get_component_ids_with_hints (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return (const gchar**) g_hash_table_get_keys_as_array (priv->hints, NULL);
}

/**
 * asc_result_update_component_gcid:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to edit.
 * @bytes: (nullable): The data to include in the global component ID, or %NULL
 *
 * Update the global component ID for the given component.
 *
 * Returns: %TRUE if the component existed and was updated.
 **/
gboolean
asc_result_update_component_gcid (AscResult *result, AsComponent *cpt, GBytes *bytes)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	g_autofree gchar *gcid = NULL;
	gchar *hash = NULL;
	const gchar *data;
	gsize data_len;
	const gchar *old_hash;
	const gchar *cid = as_component_get_id (cpt);

	if (bytes == NULL) {
		data = "";
		data_len = strlen (data);
	} else {
		data = g_bytes_get_data (bytes, &data_len);
	}

	if (as_is_empty (cid)) {
		gcid = asc_build_component_global_id (cid, NULL);
		g_hash_table_insert (priv->gcids,
				     g_ref_string_new_intern (cid),
				     g_steal_pointer (&gcid));
		return TRUE;
	}
	if (!g_hash_table_contains (priv->cpts, cid))
		return FALSE;

	old_hash = g_hash_table_lookup (priv->mdata_hashes, cpt);
	if (old_hash == NULL) {
		hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, data, data_len);
	} else {
		gsize old_hash_len;
		g_autofree gchar *tmp = NULL;

		old_hash_len = strlen (old_hash);
		tmp = g_malloc (old_hash_len + data_len);
		memcpy (tmp, old_hash, old_hash_len);
		memcpy (tmp + old_hash_len, data, data_len);

		hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, tmp, old_hash_len + data_len);
	}

	g_hash_table_insert (priv->mdata_hashes, cpt, hash);
	gcid = asc_build_component_global_id (cid, hash);
	g_hash_table_insert (priv->gcids,
			     g_ref_string_new_intern (cid),
			     g_steal_pointer (&gcid));

	return TRUE;
}

/**
 * asc_result_update_component_gcid_with_string:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to edit.
 * @data: (nullable): The data as string to include in the global component ID, or %NULL
 *
 * Update the global component ID for the given component.
 * This is a convenience method for %asc_result_update_component_gcid
 *
 * Returns: %TRUE if the component existed and was updated.
 **/
gboolean
asc_result_update_component_gcid_with_string (AscResult *result, AsComponent *cpt, const gchar *data)
{
	g_autoptr(GBytes) bytes = g_bytes_new_static (data? data : "", strlen (data? data : ""));
	return asc_result_update_component_gcid (result, cpt, bytes);
}

/**
 * asc_result_gcid_for_cid:
 * @result: an #AscResult instance.
 * @cid: Component ID to look for.
 *
 * Retrieve the global component-ID string for the given component-ID,
 * as long as component with the given ID is registered with this #AscResult.
 * Otherwise, %NULL is returned.
 */
const gchar*
asc_result_gcid_for_cid (AscResult *result, const gchar *cid)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return (const gchar*) g_hash_table_lookup (priv->gcids, cid);
}

/**
 * asc_result_component_gcid_for_cid:
 * @result: an #AscResult instance.
 * @cpt: Component to look for.
 *
 * Retrieve the global component-ID string for the given #AsComponent,
 * as long as component with the given ID is registered with this #AscResult.
 * Otherwise, %NULL is returned.
 */
const gchar*
asc_result_gcid_for_component (AscResult *result, AsComponent *cpt)
{
	return asc_result_gcid_for_cid (result, as_component_get_id (cpt));
}

/**
 * asc_result_get_component_gcids:
 * @result: an #AscResult instance.
 *
 * Retrieve a list of all global component-IDs that this result knows of.
 *
 * Returns: (transfer container): An array of global component IDs. Free with %g_free
 */
const gchar**
asc_result_get_component_gcids (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	const gchar **strv;
	GHashTableIter iter;
	gpointer value;
	guint i = 0;

	strv = g_new0 (const gchar *, g_hash_table_size (priv->gcids) + 1);
	g_hash_table_iter_init (&iter, priv->gcids);
	while (g_hash_table_iter_next (&iter, NULL, &value))
		strv[i++] = (const gchar *) value;
	return strv;
}

/**
 * asc_result_add_component:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to add.
 * @bytes: Source data used to generate the GCID hash, or %NULL if nonexistent.
 * @error: A #GError or %NULL
 *
 * Add component to the results set.
 *
 * Returns: %TRUE on success.
 **/
gboolean
asc_result_add_component (AscResult *result, AsComponent *cpt, GBytes *bytes, GError **error)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	AsComponentKind ckind;
	const gchar *cid = as_component_get_id (cpt);

	if (as_is_empty (cid)) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     "Can not add component with empty ID to results set.");
		return FALSE;
	}


	/* web applications, operating systems, repositories
	 * and component-removal merges don't (need to) have a package/bundle name set */
	ckind = as_component_get_kind (cpt);
	if ((ckind != AS_COMPONENT_KIND_WEB_APP) &&
	    (ckind != AS_COMPONENT_KIND_OPERATING_SYSTEM) &&
	    (as_component_get_merge_kind (cpt) != AS_MERGE_KIND_REMOVE_COMPONENT)) {
		if (priv->bundle_kind == AS_BUNDLE_KIND_PACKAGE) {
			gchar *pkgnames[2] = {priv->bundle_id, NULL};
			as_component_set_pkgnames (cpt, pkgnames);
		} else if ((priv->bundle_kind != AS_BUNDLE_KIND_UNKNOWN) && (priv->bundle_kind >= AS_BUNDLE_KIND_LAST)) {
			g_autoptr(AsBundle) bundle = as_bundle_new ();
			as_bundle_set_kind (bundle, priv->bundle_kind);
			as_bundle_set_id (bundle, priv->bundle_id);
			as_component_add_bundle (cpt, bundle);
		}
        }

        g_hash_table_insert (priv->cpts,
			     g_ref_string_new_intern (cid),
			     g_object_ref (cpt));
	asc_result_update_component_gcid (result, cpt, bytes);
	return TRUE;
}

/**
 * asc_result_add_component_with_string:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to add.
 * @data: Source data used to generate the GCID hash, or %NULL if nonexistent.
 * @error: A #GError or %NULL
 *
 * Add component to the results set, using string data.
 *
 * Returns: %TRUE on success.
 **/
gboolean
asc_result_add_component_with_string (AscResult *result, AsComponent *cpt, const gchar *data, GError **error)
{
	g_autoptr(GBytes) bytes = g_bytes_new_static (data? data : "", strlen (data? data : ""));
	return asc_result_add_component (result, cpt, bytes, error);
}

/**
 * asc_result_remove_component_full:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to remove.
 * @remove_gcid: %TRUE if global component ID should be unregistered as well.
 *
 * Remove a component from the results set.
 *
 * Returns: %TRUE if the component was found and removed.
 **/
gboolean
asc_result_remove_component_full (AscResult *result, AsComponent *cpt, gboolean remove_gcid)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	gboolean ret;

	ret = g_hash_table_remove (priv->cpts,
				   as_component_get_id (cpt));
	if (remove_gcid)
		g_hash_table_remove (priv->gcids, as_component_get_id (cpt));
	g_hash_table_remove (priv->mdata_hashes, cpt);

	return ret;
}

/**
 * asc_result_remove_component:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to remove.
 *
 * Remove a component from the results set.
 *
 * Returns: %TRUE if the component was found and removed.
 **/
gboolean
asc_result_remove_component (AscResult *result, AsComponent *cpt)
{
	return asc_result_remove_component_full (result, cpt, TRUE);
}

/**
 * asc_result_remove_hints_for_cid:
 * @result: an #AscResult instance.
 * @cid: The component ID
 *
 * Remove all hints that we have associated with the selected component-ID.
 */
void
asc_result_remove_hints_for_cid (AscResult *result, const gchar *cid)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	g_hash_table_remove (priv->hints, cid);
}

/**
 * asc_result_has_hint:
 * @result: an #AscResult instance.
 * @cpt: the #AsComponent to check
 * @tag: the hint tag to check for
 *
 * Test if a hint tag is associated with a given component in this result.
 *
 * Returns: %TRUE if a hint with this tag exists for the selected component.
 */
gboolean
asc_result_has_hint (AscResult *result, AsComponent *cpt, const gchar *tag)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	GPtrArray *hints;
	const gchar *cid = as_component_get_id (cpt);

	hints = g_hash_table_lookup (priv->hints, cid);
	if (hints == NULL)
		return FALSE;
	for (guint i = 0; i < hints->len; i++) {
		AscHint *hint = ASC_HINT (g_ptr_array_index (hints, i));
		if (g_strcmp0 (asc_hint_get_tag (hint), tag) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * asc_result_remove_component_by_id:
 * @result: an #AscResult instance.
 * @cid: a component-ID
 *
 * Remove a component from the results set.
 *
 * Returns: %TRUE if the component was found and removed.
 **/
gboolean
asc_result_remove_component_by_id (AscResult *result, const gchar *cid)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	AsComponent *cpt;

	cpt = g_hash_table_lookup (priv->cpts, cid);
	if (cpt == NULL)
		return FALSE;
	return asc_result_remove_component (result, cpt);
}

static gboolean
asc_result_add_hint_va (AscResult *result, AsComponent *cpt, const gchar *component_id, const gchar *tag, const gchar *key1, va_list *args, gchar **args_v)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	const gchar *cur_key;
	const gchar *cur_val;
	g_autoptr(AscHint) hint = NULL;
	GPtrArray *hints = NULL;
	guint i;
	gboolean ret = TRUE;

	g_assert ((cpt == NULL) != (component_id == NULL));
	if (component_id == NULL) {
		g_assert (cpt != NULL);
		component_id = as_component_get_id (cpt);
	}

	hint = asc_hint_new_for_tag (tag, NULL);
	if (hint == NULL) {
		g_error ("Unable to find description of issue hint '%s' - this is a bug!", tag);
		return ret;
	}

	i = 1;
	cur_key = key1;
	while (cur_key != NULL) {
		if (args_v == NULL)
			cur_val = va_arg (*args, gchar*);
		else
			cur_val = args_v[i++];
		if (cur_val == NULL)
			cur_val = "";
		asc_hint_add_explanation_var (hint, cur_key, cur_val);

		if (args_v == NULL)
			cur_key = va_arg (*args, gchar*);
		else
			cur_key = args_v[i++];
	}

	hints = g_hash_table_lookup (priv->hints, component_id);
	if (hints == NULL) {
		hints = g_ptr_array_new_with_free_func (g_object_unref);
		g_hash_table_insert (priv->hints,
				     g_ref_string_new_intern (component_id),
				     hints);
	}

	/* we stop dealing with this component as soon as we encounter a fatal error. */
	if (asc_hint_is_error (hint)) {
		if (cpt == NULL)
			asc_result_remove_component_by_id (result, component_id);
		else
			asc_result_remove_component (result, cpt);
		ret = FALSE;
	}

	g_ptr_array_add (hints, g_steal_pointer (&hint));
	return ret;
}

/**
 * asc_result_add_hint_by_cid: (skip)
 * @result: an #AscResult instance.
 * @component_id: The component-ID of the affected #AsComponent
 * @tag: AppStream Compose Issue hint tag
 * @key1: First key to add a value for, or %NULL
 * @...: replacement keys and values for the issue explanation, terminated by %NULL
 *
 * Add an issue hint for a component.
 *
 * Returns: %TRUE if the added hint did not cause the component to be invalidated.
 **/
gboolean
asc_result_add_hint_by_cid (AscResult *result, const gchar *component_id, const gchar *tag, const gchar *key1, ...)
{
	va_list args;
	gboolean ret;

	va_start (args, key1);
	ret = asc_result_add_hint_va (result,
				      NULL,
				      component_id,
				      tag,
				      key1,
				      &args,
				      NULL);
	va_end (args);
	return ret;
}

/**
 * asc_result_add_hint_by_cid_v: (rename-to asc_result_add_hint_by_cid)
 * @result: an #AscResult instance.
 * @component_id: The component-ID of the affected #AsComponent
 * @tag: AppStream Compose Issue hint tag
 * @kv: List of key-value pairs for replacement variables.
 *
 * Add an issue hint for a component.
 *
 * Returns: %TRUE if the added hint did not cause the component to be invalidated.
 **/
gboolean
asc_result_add_hint_by_cid_v (AscResult *result, const gchar *component_id, const gchar *tag, gchar **kv)
{
	gboolean ret;
	const gchar *first_key = NULL;

	if (kv != NULL)
		first_key = kv[0];
	ret = asc_result_add_hint_va (result,
				      NULL,
				      component_id,
				      tag,
				      first_key,
				      NULL,
				      kv);
	return ret;
}

/**
 * asc_result_add_hint: (skip)
 * @result: an #AscResult instance.
 * @cpt: The affected #AsComponent
 * @tag: AppStream Compose Issue hint tag
 * @key1: First key to add a value for, or %NULL
 * @...: replacement keys and values for the issue explanation, terminated by %NULL
 *
 * Add an issue hint for a component.
 *
 * Returns: %TRUE if the added hint did not cause the component to be invalidated.
 **/
gboolean
asc_result_add_hint (AscResult *result, AsComponent *cpt, const gchar *tag, const gchar *key1, ...)
{
	va_list args;
	gboolean ret;

	if (cpt != NULL) {
		va_start (args, key1);
		ret = asc_result_add_hint_va (result,
					cpt,
					NULL,
					tag,
					key1,
					&args,
					NULL);
		va_end (args);
	} else {
		va_start (args, key1);
		ret = asc_result_add_hint_va (result,
					NULL,
					"general",
					tag,
					key1,
					&args,
					NULL);
		va_end (args);
	}

	return ret;
}

/**
 * asc_result_add_hint_simple: (skip)
 * @result: an #AscResult instance.
 * @cpt: The affected #AsComponent
 * @tag: AppStream Compose Issue hint tag
 *
 * Add an issue hint which does not have any variables to replace in its
 * explanation text for a component.
 *
 * Returns: %TRUE if the added hint did not cause the component to be invalidated.
 **/
gboolean
asc_result_add_hint_simple (AscResult *result, AsComponent *cpt, const gchar *tag)
{
	if (cpt != NULL) {
		return asc_result_add_hint_va (result,
						cpt,
						NULL,
						tag,
						NULL,
						NULL,
						NULL);
	} else {
		return asc_result_add_hint_va (result,
						NULL,
						"general",
						tag,
						NULL,
						NULL,
						NULL);
	}
}

/**
 * asc_result_add_hint_v: (rename-to asc_result_add_hint)
 * @result: an #AscResult instance.
 * @cpt: The affected #AsComponent
 * @tag: AppStream Compose Issue hint tag
 * @kv: List of key-value pairs for replacement variables.
 *
 * Add an issue hint for a component.
 *
 * Returns: %TRUE if the added hint did not cause the component to be invalidated.
 **/
gboolean
asc_result_add_hint_v (AscResult *result, AsComponent *cpt, const gchar *tag, gchar **kv)
{
	gboolean ret;
	const gchar *first_key = NULL;

	if (kv != NULL)
		first_key = kv[0];

	if (cpt != NULL) {
		ret = asc_result_add_hint_va (result,
					cpt,
					NULL,
					tag,
					first_key,
					NULL,
					kv);
	} else {
		ret = asc_result_add_hint_va (result,
						NULL,
						"general",
						tag,
						first_key,
						NULL,
						kv);
	}

	return ret;
}

/**
 * asc_result_new:
 *
 * Creates a new #AscResult.
 **/
AscResult*
asc_result_new (void)
{
	AscResult *result;
	result = g_object_new (ASC_TYPE_RESULT, NULL);
	return ASC_RESULT (result);
}
