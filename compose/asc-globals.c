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
 * SECTION:asc-globals
 * @short_description: Set and read a bunch of global settings used across appstream-compose
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-globals.h"

#define ASC_TYPE_GLOBALS (asc_globals_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscGlobals, asc_globals, ASC, GLOBALS, GObject)

struct _AscGlobalsClass
{
	GObjectClass parent_class;
};

typedef struct
{
	gboolean	use_optipng;
	gchar		*optipng_bin;
} AscGlobalsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscGlobals, asc_globals, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_globals_get_instance_private (o))

static AscGlobals *g_globals = NULL;

static GObject*
asc_globals_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
	if (g_globals)
		return g_object_ref (G_OBJECT (g_globals));
	else
		return G_OBJECT_CLASS (asc_globals_parent_class)->constructor (type, n_construct_properties, construct_properties);
}

static void
asc_globals_finalize (GObject *object)
{
	AscGlobals *globals = ASC_GLOBALS (object);
	AscGlobalsPrivate *priv = GET_PRIVATE (globals);

	g_free (priv->optipng_bin);

	G_OBJECT_CLASS (asc_globals_parent_class)->finalize (object);
}

static void
asc_globals_init (AscGlobals *globals)
{
	AscGlobalsPrivate *priv = GET_PRIVATE (globals);
	g_assert (g_globals == NULL);
	g_globals = globals;

	priv->optipng_bin = g_find_program_in_path ("optipng");
	if (priv->optipng_bin != NULL)
		priv->use_optipng = TRUE;
}

static void
asc_globals_class_init (AscGlobalsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->constructor = asc_globals_constructor;
	object_class->finalize = asc_globals_finalize;
}

static AscGlobalsPrivate*
asc_globals_get_priv (void)
{
	return GET_PRIVATE (g_object_new (ASC_TYPE_GLOBALS, NULL));
}

/**
 * asc_globals_get_use_optipng:
 *
 * Get whether images should be optimized using optipng.
 **/
gboolean
asc_globals_get_use_optipng (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	return priv->use_optipng;
}

/**
 * asc_globals_set_use_optipng:
 *
 * Set whether images should be optimized using optipng.
 **/
void
asc_globals_set_use_optipng (gboolean enabled)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	priv->use_optipng = enabled;
}

/**
 * asc_globals_get_optipng_binary:
 *
 * Get path to the "optipng" binary we should use.
 **/
const gchar*
asc_globals_get_optipng_binary (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	return priv->optipng_bin;
}

/**
 * asc_globals_set_optipng_binary:
 *
 * Set path to the "optipng" binary we should use.
 **/
void
asc_globals_set_optipng_binary (const gchar *path)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	g_free (priv->optipng_bin);
	priv->optipng_bin = g_strdup (path);
}
