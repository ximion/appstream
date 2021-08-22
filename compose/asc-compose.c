/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2021 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-compose
 * @short_description: Compose collection metadata easily.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-compose.h"

#include "as-utils-private.h"
#include "asc-globals-private.h"
#include "asc-utils.h"
#include "asc-hint.h"

typedef struct
{
	GPtrArray	*units;
	GPtrArray	*results;

	GHashTable	*allowed_cids;
	GRefString	*prefix;
	GRefString	*origin;
	gchar		*media_baseurl;
	AsFormatKind	format;
	guint		min_l10n_percentage;
} AscComposePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscCompose, asc_compose, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_compose_get_instance_private (o))

static void
asc_compose_init (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);

	priv->units = g_ptr_array_new_with_free_func (g_object_unref);
	priv->results = g_ptr_array_new_with_free_func (g_object_unref);
	priv->allowed_cids = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    NULL);

	/* defaults */
	priv->format = AS_FORMAT_KIND_XML;
	as_ref_string_assign_safe (&priv->prefix, "/usr");
	priv->min_l10n_percentage = 25;
}

static void
asc_compose_finalize (GObject *object)
{
	AscCompose *compose = ASC_COMPOSE (object);
	AscComposePrivate *priv = GET_PRIVATE (compose);

	g_ptr_array_unref (priv->units);
	g_ptr_array_unref (priv->results);

	g_hash_table_unref (priv->allowed_cids);
	as_ref_string_release (priv->prefix);
	as_ref_string_release (priv->origin);
	g_free (priv->media_baseurl);

	G_OBJECT_CLASS (asc_compose_parent_class)->finalize (object);
}

static void
asc_compose_class_init (AscComposeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_compose_finalize;
}

/**
 * asc_compose_reset:
 * @compose: an #AscCompose instance.
 *
 * Reset the results, units and run-specific settings so the
 * instance can be reused for another metadata generation run.
 **/
void
asc_compose_reset (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_hash_table_remove_all (priv->allowed_cids);
	g_ptr_array_set_size (priv->units, 0);
	g_ptr_array_set_size (priv->results, 0);
}

/**
 * asc_compose_add_unit:
 * @compose: an #AscCompose instance.
 * @unit: The #AscUnit to add
 *
 * Add an #AscUnit as data source for metadata processing.
 **/
void
asc_compose_add_unit (AscCompose *compose, AscUnit *unit)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_ptr_array_add (priv->units,
			 g_object_ref (unit));
}

/**
 * asc_compose_add_allowed_cid:
 * @compose: an #AscCompose instance.
 * @component_id: The component-id to whitelist
 *
 * Adds a component ID to the allowlist. If the list is not empty, only
 * components in the list will be added to the metadata output.
 **/
void
asc_compose_add_allowed_cid (AscCompose *compose, const gchar *component_id)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_hash_table_add (priv->allowed_cids,
			  g_strdup (component_id));
}

/**
 * asc_compose_get_prefix:
 * @compose: an #AscCompose instance.
 *
 * Get the directory prefix used for processing.
 */
const gchar*
asc_compose_get_prefix (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->prefix;
}

/**
 * asc_compose_set_prefix:
 * @compose: an #AscCompose instance.
 * @prefix: a directory prefix, e.g. "/usr"
 *
 * Set the directory prefix the to-be-processed units are using.
 */
void
asc_compose_set_prefix (AscCompose *compose, const gchar *prefix)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	as_ref_string_assign_safe (&priv->prefix, prefix);
}

/**
 * asc_compose_get_origin:
 * @compose: an #AscCompose instance.
 *
 * Get the metadata origin field.
 */
const gchar*
asc_compose_get_origin (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->origin;
}

/**
 * asc_compose_set_origin:
 * @compose: an #AscCompose instance.
 * @origin: the origin.
 *
 * Set the metadata origin field (e.g. "debian" or "flathub")
 */
void
asc_compose_set_origin (AscCompose *compose, const gchar *origin)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	as_ref_string_assign_safe (&priv->origin, origin);
}

/**
 * asc_compose_get_format:
 * @compose: an #AscCompose instance.
 *
 * get the format type we are generating.
 */
AsFormatKind
asc_compose_get_format (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->format;
}

/**
 * asc_compose_set_format:
 * @compose: an #AscCompose instance.
 * @kind: The format, e.g. %AS_FORMAT_KIND_XML
 *
 * Set the format kind of the collection metadata that we should generate.
 */
void
asc_compose_set_format (AscCompose *compose, AsFormatKind kind)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	priv->format = kind;
}

/**
 * asc_compose_get_media_baseurl:
 * @compose: an #AscCompose instance.
 *
 * Get the media base URL to be used for the generated data,
 * or %NULL if this feature is not used.
 */
const gchar*
asc_compose_get_media_baseurl (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->media_baseurl;
}

/**
 * asc_compose_set_media_baseurl:
 * @compose: an #AscCompose instance.
 * @url: (nullable): the media base URL.
 *
 * Set the media base URL for the generated metadata. Can be %NULL.
 */
void
asc_compose_set_media_baseurl (AscCompose *compose, const gchar *url)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	as_assign_string_safe (priv->media_baseurl, url);
}

/**
 * asc_compose_new:
 *
 * Creates a new #AscCompose.
 **/
AscCompose*
asc_compose_new (void)
{
	AscCompose *compose;
	compose = g_object_new (ASC_TYPE_COMPOSE, NULL);
	return ASC_COMPOSE (compose);
}
