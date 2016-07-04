/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Lucas Moura <lucas.moura128@gmail.com>
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
 * SECTION: as-suggested
 * @short_description: Suggestions for other software suggested by a component.
 * @include: appstream.h
 *
 * This class provides a list of other component-ids suggested by a software component, as well
 * as an origin of the suggestion (manually suggested by the upstream project, or
 * automatically determined by heuristics).
 */

#include "config.h"

#include "as-suggested.h"

typedef struct
{
	AsSuggestedKind kind;
	GPtrArray *cpt_ids; /* of utf8 */
} AsSuggestedPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsSuggested, as_suggested, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_suggested_get_instance_private (o))

/**
 * as_suggested_kind_to_string:
 * @kind: the %AsSuggestedKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar*
as_suggested_kind_to_string (AsSuggestedKind kind)
{
	if (kind == AS_SUGGESTED_KIND_UPSTREAM)
		return "upstream";
	if (kind == AS_SUGGESTED_KIND_HEURISTIC)
		return "heuristic";

	return "unknown";
}

/**
 * as_suggested_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsSuggestedKind or %AS_SUGGESTED_KIND_UNKNOWN for unknown
 **/
AsSuggestedKind
as_suggested_kind_from_string (const gchar *kind_str)
{
	/* if the kind is not set, we assume upstream */
	if (kind_str == NULL)
		return AS_SUGGESTED_KIND_UPSTREAM;

	if (g_strcmp0 (kind_str, "upstream") == 0)
		return AS_SUGGESTED_KIND_UPSTREAM;
	if (g_strcmp0 (kind_str, "heuristic") == 0)
		return AS_SUGGESTED_KIND_HEURISTIC;

	return AS_SUGGESTED_KIND_UNKNOWN;
}

/**
 * as_suggested_finalize:
 **/
static void
as_suggested_finalize (GObject *object)
{
	AsSuggested *suggested = AS_SUGGESTED (object);
	AsSuggestedPrivate *priv = GET_PRIVATE (suggested);

	g_ptr_array_unref (priv->cpt_ids);

	G_OBJECT_CLASS (as_suggested_parent_class)->finalize (object);
}

/**
 * as_suggested_init:
 **/
static void
as_suggested_init (AsSuggested *suggested)
{
	AsSuggestedPrivate *priv = GET_PRIVATE (suggested);

	priv->cpt_ids = g_ptr_array_new_with_free_func (g_free);
}

/**
 * as_suggested_get_kind:
 * @suggested: a #AsSuggested instance.
 *
 * Gets the suggested kind.
 *
 * Returns: the #AssuggestedKind
 **/
AsSuggestedKind
as_suggested_get_kind (AsSuggested *suggested)
{
	AsSuggestedPrivate *priv = GET_PRIVATE (suggested);
	return priv->kind;
}

/**
 * as_suggested_set_kind:
 * @suggested: a #AsSuggested instance.
 * @kind: the #AsSuggestedKind, e.g. %AS_SUGGESTED_KIND_HEURISTIC.
 *
 * Sets the suggested kind.
 **/
void
as_suggested_set_kind (AsSuggested *suggested, AsSuggestedKind kind)
{
	AsSuggestedPrivate *priv = GET_PRIVATE (suggested);
	priv->kind = kind;
}

/**
 * as_suggested_get_ids:
 * @suggested: a #AsSuggested instance.
 *
 * Get a list of components id that generated the suggestion
 *
 * Returns: (transfer none) (element-type utf8): an array of components id
 */
GPtrArray*
as_suggested_get_ids (AsSuggested *suggested)
{
	AsSuggestedPrivate *priv = GET_PRIVATE (suggested);
	return priv->cpt_ids;
}


/**
 * as_suggested_add_id:
 * @suggested: a #AsSuggested instance.
 * @cid: The component id to add
 *
 * Add a component id to this suggested object.
 **/
void
as_suggested_add_id (AsSuggested *suggested, const gchar *cid)
{
	AsSuggestedPrivate *priv = GET_PRIVATE (suggested);
	g_ptr_array_add (priv->cpt_ids, g_strdup (cid));
}

/**
 * as_suggested_is_valid:
 * @suggested: a #AsSuggested instance.
 *
 * Check if the essential properties of this suggestion are
 * populated with useful data.
 *
 * Returns: %TRUE if we have useful data.
 */
gboolean
as_suggested_is_valid (AsSuggested *suggested)
{
	AsSuggestedPrivate *priv = GET_PRIVATE (suggested);

	if (priv->kind == AS_SUGGESTED_KIND_UNKNOWN)
		return FALSE;
	if (priv->cpt_ids->len == 0)
		return FALSE;

	return TRUE;
}

/**
 * as_suggested_new:
 *
 * Creates a new #AsSuggested.
 *
 * Returns: (transfer full): a new #AsSuggested
 **/
AsSuggested*
as_suggested_new (void)
{
	AsSuggested *suggested;
	suggested = g_object_new (AS_TYPE_SUGGESTED, NULL);
	return AS_SUGGESTED (suggested);
}

/**
 * as_suggested_class_init:
 **/
static void
as_suggested_class_init (AsSuggestedClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_suggested_finalize;
}
