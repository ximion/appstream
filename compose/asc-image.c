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
 * SECTION:asc-image
 * @short_description: Basic image rendering functions.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-image.h"

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct
{
	GdkPixbuf	*pix;
} AscImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscImage, asc_image, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_image_get_instance_private (o))

static void
asc_image_finalize (GObject *object)
{
	AscImage *image = ASC_IMAGE (object);
	AscImagePrivate *priv = GET_PRIVATE (image);

	if (priv->pix != NULL)
		g_object_unref (priv->pix);

	G_OBJECT_CLASS (asc_image_parent_class)->finalize (object);
}

static void
asc_image_init (AscImage *image)
{
}

static void
asc_image_class_init (AscImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_image_finalize;
}

static AscImage*
asc_image_new (void)
{
	AscImage *image;
	image = g_object_new (ASC_TYPE_IMAGE, NULL);
	return ASC_IMAGE (image);
}

/**
 * asc_image_new_from_file:
 * @fname: Name of the file to load.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscImage from a file on the filesystem.
 **/
AscImage*
asc_image_new_from_file (const gchar* fname, GError **error)
{
	g_autoptr(AscImage) image = asc_image_new();
	AscImagePrivate *priv = GET_PRIVATE (image);

	priv->pix = gdk_pixbuf_new_from_file (fname, error);
	if (priv->pix == NULL)
		return NULL;
	return g_steal_pointer (&image);
}

/**
 * asc_image_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscImage from a file on the filesystem.
 **/
AscImage*
asc_image_new_from_data (const void *data, gssize len, GError **error)
{
	g_autoptr(GInputStream) istream = NULL;
	g_autoptr(AscImage) image = asc_image_new();
	AscImagePrivate *priv = GET_PRIVATE (image);

	istream = g_memory_input_stream_new_from_data (data, len, NULL);
        priv->pix = gdk_pixbuf_new_from_stream (istream, NULL, error);
	if (priv->pix == NULL)
		return NULL;
	return g_steal_pointer (&image);
}
