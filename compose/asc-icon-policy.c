/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-icon-policy
 * @short_description: Set policy on how to deal with different icon types.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-icon-policy.h"

#include "as-utils-private.h"
#include "asc-globals-private.h"

typedef struct {
	GPtrArray *entries;
} AscIconPolicyPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscIconPolicy, asc_icon_policy, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_icon_policy_get_instance_private (o))

/**
 * AscIconPolicyIter:
 *
 * A #AscIconPolicyIter structure represents an iterator that can be used
 * to iterate over the icon sizes / policy entries of an #AscIconPolicy.
 * #AscIconPolicyIter structures are typically allocated on the stack and
 * then initialized with asc_icon_policy_iter_init().
 */

typedef struct {
	AscIconPolicy *ipolicy;
	guint pos;
	gpointer dummy3;
	gpointer dummy4;
	gpointer dummy5;
	gpointer dummy6;
} RealIconPolicyIter;

G_STATIC_ASSERT (sizeof (AscIconPolicyIter) == sizeof (RealIconPolicyIter));

typedef struct {
	guint size;
	guint scale;
	AscIconState state;
} AscIconPolicyEntry;

static AscIconPolicyEntry *
asc_icon_policy_entry_new (guint size, guint scale)
{
	AscIconPolicyEntry *entry;
	entry = g_slice_new0 (AscIconPolicyEntry);
	entry->size = size;
	entry->scale = scale;
	entry->state = ASC_ICON_STATE_IGNORED;
	return entry;
}

static void
asc_icon_policy_entry_free (AscIconPolicyEntry *entry)
{
	g_slice_free (AscIconPolicyEntry, entry);
}

static void
asc_icon_policy_init (AscIconPolicy *ipolicy)
{
	AscIconPolicyPrivate *priv = GET_PRIVATE (ipolicy);
	priv->entries = g_ptr_array_new_with_free_func (
	    (GDestroyNotify) asc_icon_policy_entry_free);

	/* set our default policy */
	asc_icon_policy_set_policy (ipolicy, 48, 1, ASC_ICON_STATE_CACHED_ONLY);
	asc_icon_policy_set_policy (ipolicy, 48, 2, ASC_ICON_STATE_CACHED_ONLY);
	asc_icon_policy_set_policy (ipolicy, 64, 1, ASC_ICON_STATE_CACHED_ONLY);
	asc_icon_policy_set_policy (ipolicy, 64, 2, ASC_ICON_STATE_CACHED_ONLY);
	asc_icon_policy_set_policy (ipolicy, 128, 1, ASC_ICON_STATE_CACHED_REMOTE);
	asc_icon_policy_set_policy (ipolicy, 128, 2, ASC_ICON_STATE_CACHED_REMOTE);
}

static void
asc_icon_policy_finalize (GObject *object)
{
	AscIconPolicy *ipolicy = ASC_ICON_POLICY (object);
	AscIconPolicyPrivate *priv = GET_PRIVATE (ipolicy);

	g_ptr_array_unref (priv->entries);

	G_OBJECT_CLASS (asc_icon_policy_parent_class)->finalize (object);
}

static void
asc_icon_policy_class_init (AscIconPolicyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_icon_policy_finalize;
}

/**
 * asc_icon_state_to_string:
 * @istate: the #AscIconState.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @istate
 **/
const gchar *
asc_icon_state_to_string (AscIconState istate)
{
	if (istate == ASC_ICON_STATE_CACHED_REMOTE)
		return "cached-remote";
	if (istate == ASC_ICON_STATE_CACHED_ONLY)
		return "cached";
	if (istate == ASC_ICON_STATE_REMOTE_ONLY)
		return "remote";

	return "ignored";
}

/**
 * asc_icon_state_from_string:
 * @state_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AscIconState
 **/
AscIconState
asc_icon_state_from_string (const gchar *state_str)
{
	if (as_str_equal0 (state_str, "cached-remote"))
		return ASC_ICON_STATE_CACHED_REMOTE;
	if (as_str_equal0 (state_str, "cached"))
		return ASC_ICON_STATE_CACHED_ONLY;
	if (as_str_equal0 (state_str, "remote"))
		return ASC_ICON_STATE_REMOTE_ONLY;

	return ASC_ICON_STATE_IGNORED;
}

/**
 * asc_icon_policy_set_policy:
 * @ipolicy: an #AscIconPolicy instance.
 * @icon_size: the size of the icon to set policy for (e.g. 64 for 64x64px icons)
 * @icon_scale: the icon scale factor, e.g. 1
 * @state: the designated #AscIconState
 *
 * Sets a designated state for an icon of the given size.
 */
void
asc_icon_policy_set_policy (AscIconPolicy *ipolicy,
			    guint icon_size,
			    guint icon_scale,
			    AscIconState state)
{
	AscIconPolicyPrivate *priv = GET_PRIVATE (ipolicy);
	AscIconPolicyEntry *entry = NULL;

	if (icon_scale < 1) {
		g_warning ("An icon scale of 0 is invalid, resetting to 1.");
		icon_scale = 1;
	}

	for (guint i = 0; i < priv->entries->len; i++) {
		AscIconPolicyEntry *e = g_ptr_array_index (priv->entries, i);
		if (e->size == icon_size && e->scale == icon_scale) {
			entry = e;
			break;
		}
	}

	if (entry != NULL) {
		/* found an existing entry, just modify that */
		entry->state = state;
		return;
	}

	entry = asc_icon_policy_entry_new (icon_size, icon_scale);
	entry->state = state;
	g_ptr_array_add (priv->entries, entry);
}

/**
 * asc_icon_policy_iter_init:
 * @iter: an uninitialized #AscIconPolicyIter
 * @ipolicy: an #AscIconPolicy
 *
 * Initializes a policy iterator for the policy entry list and associates it
 * it with @ipolicy.
 * The #AscIconPolicyIter structure is typically allocated on the stack
 * and does not need to be freed explicitly.
 */
void
asc_icon_policy_iter_init (AscIconPolicyIter *iter, AscIconPolicy *ipolicy)
{
	RealIconPolicyIter *ri = (RealIconPolicyIter *) iter;

	g_return_if_fail (iter != NULL);
	g_return_if_fail (ASC_IS_ICON_POLICY (ipolicy));

	ri->ipolicy = ipolicy;
	ri->pos = 0;
}

/**
 * asc_icon_policy_iter_next:
 * @iter: an initialized #AscIconPolicyIter
 * @size: (out) (optional) (not nullable): Destination of the returned icon size
 * @scale: (out) (optional) (not nullable): Destination of the returned icon scale factor
 * @state: (out) (optional) (nullable): Destination of the returned designated icon state.
 *
 * Returns the current icon policy entry and advances the iterator.
 * Example:
 * |[<!-- language="C" -->
 * AscIconPolicyIter iter;
 * guint icon_size;
 * guint icon_scale;
 * AscIconState istate;
 *
 * asc_icon_policy_iter_init (&iter, ipolicy);
 * while (asc_icon_policy_iter_next (&iter, &icon_size, &icon_scale, &istate)) {
 *     // do something with the icon entry data
 * }
 * ]|
 *
 * Returns: %FALSE if the last entry has been reached.
 */
gboolean
asc_icon_policy_iter_next (AscIconPolicyIter *iter, guint *size, guint *scale, AscIconState *state)
{
	AscIconPolicyPrivate *priv;
	AscIconPolicyEntry *entry = NULL;
	RealIconPolicyIter *ri = (RealIconPolicyIter *) iter;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (size != NULL, FALSE);
	g_return_val_if_fail (scale != NULL, FALSE);
	priv = GET_PRIVATE (ri->ipolicy);

	/* check if the iteration was finished */
	if (ri->pos >= priv->entries->len) {
		*size = 0;
		*scale = 0;
		return FALSE;
	}

	entry = g_ptr_array_index (priv->entries, ri->pos);
	ri->pos++;
	*size = entry->size;
	*scale = entry->scale;
	if (state != NULL)
		*state = entry->state;

	return TRUE;
}

/**
 * asc_icon_policy_to_string:
 * @ipolicy: an #AscIconPolicy instance.
 *
 * Converts the current icon policy into a textual representation.
 *
 * Returns: The icon policy serialized into a string. Free with g_free()
 **/
gchar *
asc_icon_policy_to_string (AscIconPolicy *ipolicy)
{
	AscIconPolicyPrivate *priv = GET_PRIVATE (ipolicy);
	GString *result = g_string_new ("");

	for (guint i = 0; i < priv->entries->len; i++) {
		AscIconPolicyEntry *e = g_ptr_array_index (priv->entries, i);
		if (e->scale > 1)
			g_string_append_printf (result,
						"%dx%d@%d=%s,",
						e->size,
						e->size,
						e->scale,
						asc_icon_state_to_string (e->state));
		else
			g_string_append_printf (result,
						"%dx%d=%s,",
						e->size,
						e->size,
						asc_icon_state_to_string (e->state));
	}

	/* trim trailing comma */
	if (result->len > 0)
		g_string_truncate (result, result->len - 1);

	return g_string_free (result, FALSE);
}

/**
 * asc_icon_policy_from_string:
 * @ipolicy: an #AscIconPolicy instance.
 * @serialized_policy: A policy string as returned by %asc_icon_policy_to_string
 * @error: A #GError
 *
 * Loads the icon policy from a textual representation.
 *
 **/
gboolean
asc_icon_policy_from_string (AscIconPolicy *ipolicy, const gchar *serialized_policy, GError **error)
{
	AscIconPolicyPrivate *priv = GET_PRIVATE (ipolicy);
	g_auto(GStrv) policy_blocks = NULL;
	gboolean success = TRUE;
	gboolean have_64x64_cached = FALSE;
	g_return_val_if_fail (serialized_policy != NULL, FALSE);

	policy_blocks = g_strsplit (serialized_policy, ",", -1);
	if (policy_blocks == NULL) {
		g_set_error_literal (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Unable to parse icon policy string representation.");
		return FALSE;
	}

	/* delete existing entries */
	g_ptr_array_set_size (priv->entries, 0);

	/* parse data */
	for (guint i = 0; policy_blocks[i] != NULL; i++) {
		guint size = 0;
		guint scale = 1;
		gchar *scale_ptr;
		g_auto(GStrv) size_scale = NULL;
		g_auto(GStrv) parts = g_strsplit (policy_blocks[i], "=", 2);

		if (parts == NULL || g_strv_length (parts) != 2) {
			success = FALSE;
			continue;
		}
		size_scale = g_strsplit (parts[0], "x", 2);
		if (size_scale == NULL || g_strv_length (size_scale) != 2) {
			success = FALSE;
			continue;
		}

		size = g_ascii_strtoull (size_scale[0], NULL, 10);
		scale_ptr = g_strrstr (size_scale[1], "@");
		if (scale_ptr != NULL)
			scale = g_ascii_strtoull (scale_ptr + 1, NULL, 10);

		if (size == 0 || scale == 0) {
			success = FALSE;
			continue;
		}

		asc_icon_policy_set_policy (ipolicy,
					    size,
					    scale,
					    asc_icon_state_from_string (parts[1]));
	}

	/* we must have 64x64px icons cached, to satisfy policy */
	for (guint i = 0; i < priv->entries->len; i++) {
		AscIconPolicyEntry *e = g_ptr_array_index (priv->entries, i);
		if (e->size == 64 && e->scale == 1) {
			if (e->state == ASC_ICON_STATE_CACHED_REMOTE ||
			    e->state == ASC_ICON_STATE_CACHED_ONLY)
				have_64x64_cached = TRUE;
			break;
		}
	}

	if (!have_64x64_cached) {
		g_set_error_literal (
		    error,
		    AS_UTILS_ERROR,
		    AS_UTILS_ERROR_FAILED,
		    "64x64@1 icons were not selected for being cached, which is not permitted.");
		asc_icon_policy_set_policy (ipolicy, 64, 1, ASC_ICON_STATE_CACHED_ONLY);
		return FALSE;
	}

	if (!success)
		g_set_error_literal (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Unable to parse icon policy string representation.");

	return success;
}

/**
 * asc_icon_policy_new:
 *
 * Creates a new #AscIconPolicy.
 **/
AscIconPolicy *
asc_icon_policy_new (void)
{
	AscIconPolicy *ipolicy;
	ipolicy = g_object_new (ASC_TYPE_ICON_POLICY, NULL);
	return ASC_ICON_POLICY (ipolicy);
}
