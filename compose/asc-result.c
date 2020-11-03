/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

typedef struct
{
	gchar		*unit_name;

	GHashTable	*cpts; /* utf8->AsComponent */
	GHashTable	*mdata_hashes; /* AsComponent->utf8 */
	GHashTable	*hints; /* utf8->GPtrArray */
} AscResultPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscResult, asc_result, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_result_get_instance_private (o))

static void
asc_result_finalize (GObject *object)
{
	AscResult *result = ASC_RESULT (object);
	AscResultPrivate *priv = GET_PRIVATE (result);

	priv->cpts = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    g_free,
					    g_object_unref);
	priv->mdata_hashes = g_hash_table_new_full (g_direct_hash,
						    g_direct_equal,
						    NULL,
						    g_free);
	priv->hints = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     g_free,
					     (GDestroyNotify) g_ptr_array_unref);


	G_OBJECT_CLASS (asc_result_parent_class)->finalize (object);
}

static void
asc_result_init (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);

	g_free (priv->unit_name);

	g_hash_table_unref (priv->cpts);
	g_hash_table_unref (priv->mdata_hashes);
	g_hash_table_unref (priv->hints);
}

static void
asc_result_class_init (AscResultClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_result_finalize;
}

/**
 * asc_result_is_ignored:
 * @result: an #AscResult instance.
 *
 * Returns: %TRUE if this result means the analyzed unit was ignored entirely..
 **/
gboolean
asc_result_is_ignored (AscResult *result)
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
	return g_hash_table_size (priv->hints);
}

/**
 * asc_result_get_unit_name:
 * @result: an #AscResult instance.
 *
 * Gets the name of the unit (a directory / package / any entity containing metadata)
 * these results are for.
 **/
const gchar*
asc_result_get_unit_name (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return priv->unit_name;
}

/**
 * asc_result_set_tag:
 * @result: an #AscResult instance.
 *
 * Sets the name of the unit these results are for.
 **/
void
asc_result_set_tag (AscResult *result, const gchar *name)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	g_free (priv->unit_name);
	priv->unit_name = g_strdup (name);
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
