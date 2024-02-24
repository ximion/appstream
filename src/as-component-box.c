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
 * SECTION:as-component-box
 * @short_description: A collection of components that are managed together.
 * @include: appstream.h
 *
 * This class is a container for #AsComponent objects which usually share the same #AsContext
 * and are manipulated together.
 * It also provides binding-safe accessor functions to manipulate an array of
 * components.
 *
 * See also: #AsComponent
 */

#include "config.h"
#include "as-component-box-private.h"

#include <gio/gio.h>

#include "as-macros.h"
#include "as-utils-private.h"

typedef struct {
	AsComponentBoxFlags flags;
	GHashTable *cpt_map;
} AsComponentBoxPrivate;

enum {
	PROP_0,
	PROP_FLAGS,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = {
	NULL,
};

G_DEFINE_TYPE_WITH_PRIVATE (AsComponentBox, as_component_box, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_component_box_get_instance_private (o))

static void
as_component_box_init (AsComponentBox *cbox)
{
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);

	cbox->cpts = g_ptr_array_new_with_free_func (g_object_unref);
	priv->cpt_map = NULL;
}

static void
as_component_box_constructed (GObject *object)
{
	AsComponentBox *cbox = AS_COMPONENT_BOX (object);
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);

	if (!as_flags_contains (priv->flags, AS_COMPONENT_BOX_FLAG_NO_CHECKS))
		priv->cpt_map = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	G_OBJECT_CLASS (as_component_box_parent_class)->constructed (object);
}

static void
as_component_box_finalize (GObject *object)
{
	AsComponentBox *cbox = AS_COMPONENT_BOX (object);
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);

	g_ptr_array_unref (cbox->cpts);
	if (priv->cpt_map != NULL)
		g_hash_table_unref (priv->cpt_map);

	G_OBJECT_CLASS (as_component_box_parent_class)->finalize (object);
}

static void
as_component_box_set_property (GObject *object,
			       guint property_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	AsComponentBox *cbox = AS_COMPONENT_BOX (object);
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);

	switch (property_id) {
	case PROP_FLAGS:
		priv->flags = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
as_component_box_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	AsComponentBox *cbox = AS_COMPONENT_BOX (object);
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);

	switch (property_id) {
	case PROP_FLAGS:
		g_value_set_flags (value, priv->flags);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
as_component_box_class_init (AsComponentBoxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_component_box_finalize;

	object_class->constructed = as_component_box_constructed;
	object_class->set_property = as_component_box_set_property;
	object_class->get_property = as_component_box_get_property;

	obj_properties[PROP_FLAGS] = g_param_spec_uint ("flags",
							"Flags",
							"Component box flags",
							AS_COMPONENT_BOX_FLAG_NONE,
							G_MAXUINT,
							AS_COMPONENT_BOX_FLAG_NONE,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

/**
 * as_component_box_new:
 *
 * Creates a new #AsComponentBox.
 *
 * Returns: (transfer full): an #AsComponentBox
 *
 * Since: 1.0
 **/
AsComponentBox *
as_component_box_new (AsComponentBoxFlags flags)
{
	AsComponentBox *cbox;
	cbox = g_object_new (AS_TYPE_COMPONENT_BOX, "flags", flags, NULL);
	return AS_COMPONENT_BOX (cbox);
}

/**
 * as_component_box_new_simple:
 *
 * Creates a new #AsComponentBox with the simplest parameters,
 * so it is basically an array storage without overhead.
 *
 * Only the most basic checks on inserted components will be performed,
 * and it is assumed that the inserted components have been checked
 * already prior to insertion.
 *
 * Returns: (transfer full): an #AsComponentBox
 **/
AsComponentBox *
as_component_box_new_simple (void)
{
	return as_component_box_new (AS_COMPONENT_BOX_FLAG_NO_CHECKS);
}

/**
 * as_component_box_index:
 * @cbox: a #AsComponentBox
 * @index_: the index of the #AsComponent to return
 *
 * Returns the #AsComponent at the given index of the array.
 *
 * This does not perform bounds checking on the given @index_,
 * so you are responsible for checking it against the array length.
 * Use %as_component_box_len to determine the amount of components
 * present in the #AsComponentBox.
 *
 * Returns: (transfer none): the #AsComponent at the given index
 */

/**
 * as_component_box_len:
 * @cbox: a #AsComponentBox
 *
 * Get the amount of components in its box array.
 *
 * Returns: Size of components in #AsComponentBox.
 */

/**
 * as_component_box_as_array:
 * @cbox: An instance of #AsComponentBox.
 *
 * Get the contents of this component box as #GPtrArray.
 *
 * Returns: (transfer none) (element-type AsComponent): an array of #AsComponent instances.
 */
GPtrArray *
as_component_box_as_array (AsComponentBox *cbox)
{
	return cbox->cpts;
}

/**
 * as_component_box_get_flags:
 * @cbox: An instance of #AsComponentBox.
 *
 * Get the flags this component box was constructed with.
 *
 * Returns: The #AsComponentBoxFlags that are in effect.
 */
AsComponentBoxFlags
as_component_box_get_flags (AsComponentBox *cbox)
{
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);
	return priv->flags;
}

/**
 * as_component_box_get_size:
 * @cbox: An instance of #AsComponentBox.
 *
 * Get the amount of components in this box.
 *
 * Returns: Amount of components.
 */
guint
as_component_box_get_size (AsComponentBox *cbox)
{
	return cbox->cpts->len;
}

/**
 * as_component_box_is_empty:
 * @cbox: An instance of #AsComponentBox.
 *
 * Check if there are any components present.
 *
 * Returns: %TRUE if this component box is empty.
 */
gboolean
as_component_box_is_empty (AsComponentBox *cbox)
{
	return cbox->cpts->len == 0;
}

/**
 * as_component_box_index_safe:
 * @cbox: An instance of #AsComponentBox.
 * @index: The component index.
 *
 * Retrieve a component at the respective index from the internal
 * component array.
 *
 * Returns: (transfer none): An #AsComponent or %NULL
 */
AsComponent *
as_component_box_index_safe (AsComponentBox *cbox, guint index)
{
	if (index >= cbox->cpts->len)
		return NULL;
	return as_component_box_index (cbox, index);
}

/**
 * as_component_box_add:
 * @cbox: An instance of #AsComponentBox.
 *
 * Add a component to the box. Returns an error if we could not add it
 * (most likely due to component box constraints).
 *
 * Returns: %TRUE on success.
 */
gboolean
as_component_box_add (AsComponentBox *cbox, AsComponent *cpt, GError **error)
{
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);

	if (!as_flags_contains (priv->flags, AS_COMPONENT_BOX_FLAG_NO_CHECKS)) {
		const gchar *data_id = as_component_get_data_id (cpt);

		if (g_hash_table_lookup (priv->cpt_map, data_id) != NULL) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_EXISTS,
				     "Tried to insert component that already exists: %s",
				     data_id);
			return FALSE;
		}

		g_hash_table_insert (priv->cpt_map, (gchar *) data_id, cpt);
	}

	g_ptr_array_add (cbox->cpts, g_object_ref (cpt));
	return TRUE;
}

/**
 * as_component_box_clear:
 * @cbox: An instance of #AsComponentBox.
 *
 * Remove all contents of this component box.
 */
void
as_component_box_clear (AsComponentBox *cbox)
{
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);

	g_ptr_array_set_size (cbox->cpts, 0);
	if (priv->cpt_map != NULL)
		g_hash_table_remove_all (priv->cpt_map);
}

/**
 * as_component_box_remove_at:
 * @cbox: An instance of #AsComponentBox.
 * @index: the index of the component to remove.
 *
 * Remove a component at the specified index.
 * Please ensure that the index is not larger than
 * %as_component_box_get_size() - 1
 */
void
as_component_box_remove_at (AsComponentBox *cbox, guint index)
{
	AsComponentBoxPrivate *priv = GET_PRIVATE (cbox);
	AsComponent *cpt;

	g_return_if_fail (index < cbox->cpts->len);

	cpt = AS_COMPONENT (g_ptr_array_index (cbox->cpts, index));
	if (!as_flags_contains (priv->flags, AS_COMPONENT_BOX_FLAG_NO_CHECKS)) {
		AsComponent *ht_cpt = NULL;
		const gchar *data_id = as_component_get_data_id (cpt);

		if (!g_hash_table_remove (priv->cpt_map, data_id)) {
			/* we did not find the component reference, let's try a deep search */
			GHashTableIter ht_iter;
			gpointer ht_key, ht_value;

			g_hash_table_iter_init (&ht_iter, priv->cpt_map);
			while (g_hash_table_iter_next (&ht_iter, &ht_key, &ht_value)) {
				ht_cpt = AS_COMPONENT (ht_value);
				if (ht_cpt == cpt) {
					g_hash_table_remove (priv->cpt_map, ht_key);
					break;
				}
			}
		}
	}

	g_ptr_array_remove_index (cbox->cpts, index);
}

/**
 * as_sort_components_cb:
 *
 * Helper method to sort lists of #AsComponent
 */
static gint
as_sort_components_cb (gconstpointer a, gconstpointer b)
{
	AsComponent *cpt1 = *((AsComponent **) a);
	AsComponent *cpt2 = *((AsComponent **) b);

	return g_strcmp0 (as_component_get_id (cpt1), as_component_get_id (cpt2));
}

/**
 * as_component_box_sort:
 * @cbox: An instance of #AsComponentBox.
 *
 * Sort components to bring them into a deterministic order.
 */
void
as_component_box_sort (AsComponentBox *cbox)
{
	g_ptr_array_sort (cbox->cpts, as_sort_components_cb);
}

/**
 * as_component_box_sort_by_score:
 * @cbox: An instance of #AsComponentBox.
 *
 * Sort components by their (search) match score.
 */
void
as_component_box_sort_by_score (AsComponentBox *cbox)
{
	as_sort_components_by_score (cbox->cpts);
}
