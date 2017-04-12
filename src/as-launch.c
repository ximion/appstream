/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-launch.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <glib.h>

/**
 * SECTION:as-launch
 * @short_description: Description of launchable entries for a software component
 * @include: appstream.h
 *
 * Components can provide multiple launch-entries to launch the software they belong to.
 * This class describes them.
 *
 * See also: #AsComponent
 */

typedef struct
{
	AsLaunchKind	kind;
	GPtrArray	*entries;
} AsLaunchPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsLaunch, as_launch, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_launch_get_instance_private (o))

/**
 * as_launch_kind_to_string:
 * @kind: the #AsLaunchKind.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.11.0
 **/
const gchar*
as_launch_kind_to_string (AsLaunchKind kind)
{
	if (kind == AS_LAUNCH_KIND_DESKTOP_ID)
		return "desktop-id";
	return "unknown";
}

/**
 * as_launch_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsLaunchKind or %AS_LAUNCH_KIND_UNKNOWN for unknown
 *
 * Since: 0.11.0
 **/
AsLaunchKind
as_launch_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "desktop-id") == 0)
		return AS_LAUNCH_KIND_DESKTOP_ID;
	return AS_LAUNCH_KIND_UNKNOWN;
}

/**
 * as_launch_finalize:
 **/
static void
as_launch_finalize (GObject *object)
{
	AsLaunch *launch = AS_LAUNCH (object);
	AsLaunchPrivate *priv = GET_PRIVATE (launch);

	g_ptr_array_unref (priv->entries);

	G_OBJECT_CLASS (as_launch_parent_class)->finalize (object);
}

/**
 * as_launch_init:
 **/
static void
as_launch_init (AsLaunch *launch)
{
	AsLaunchPrivate *priv = GET_PRIVATE (launch);

	priv->kind = AS_LAUNCH_KIND_UNKNOWN;
	priv->entries = g_ptr_array_new_with_free_func (g_free);
}

/**
 * as_launch_class_init:
 **/
static void
as_launch_class_init (AsLaunchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_launch_finalize;
}

/**
 * as_launch_get_kind:
 * @launch: a #AsLaunch instance.
 *
 * The launch system for the entries this #AsLaunch
 * object stores.
 *
 * Returns: an enum of type #AsLaunchKind
 *
 * Since: 0.11.0
 */
AsLaunchKind
as_launch_get_kind (AsLaunch *launch)
{
	AsLaunchPrivate *priv = GET_PRIVATE (launch);
	return priv->kind;
}

/**
 * as_launch_set_kind:
 * @launch: a #AsLaunch instance.
 * @kind: the new #AsLaunchKind
 *
 * Set the launch system for the entries this #AsLaunch
 * object stores.
 *
 * Since: 0.11.0
 */
void
as_launch_set_kind (AsLaunch *launch, AsLaunchKind kind)
{
	AsLaunchPrivate *priv = GET_PRIVATE (launch);
	priv->kind = kind;
}

/**
 * as_launch_get_entries:
 * @launch: a #AsLaunch instance.
 *
 * Get an array of launchable entries.
 *
 * Returns: (transfer none) (element-type utf8): An string list of launch entries.
 *
 * Since: 0.11.0
 */
GPtrArray*
as_launch_get_entries (AsLaunch *launch)
{
	AsLaunchPrivate *priv = GET_PRIVATE (launch);
	return priv->entries;
}

/**
 * as_launch_add_entry:
 * @launch: a #AsLaunch instance.
 *
 * Add a new launchable entry.
 *
 * Since: 0.11.0
 */
void
as_launch_add_entry (AsLaunch *launch, const gchar *entry)
{
	AsLaunchPrivate *priv = GET_PRIVATE (launch);
	g_ptr_array_add (priv->entries, g_strdup (entry));
}

/**
 * as_launch_new:
 *
 * Creates a new #AsLaunch.
 *
 * Returns: (transfer full): a #AsLaunch
 *
 * Since: 0.11.0
 **/
AsLaunch*
as_launch_new (void)
{
	AsLaunch *launch;
	launch = g_object_new (AS_TYPE_LAUNCH, NULL);
	return AS_LAUNCH (launch);
}
