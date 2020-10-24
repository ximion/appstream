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
 * SECTION:asc-canvas
 * @short_description: Draw text and render SVG canvass.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-canvas.h"

#include <cairo.h>

typedef struct
{
	cairo_t *cr;
	cairo_surface_t *srf;

	gint width;
	gint height;
} AscCanvasPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscCanvas, asc_canvas, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_canvas_get_instance_private (o))

static void
asc_canvas_finalize (GObject *object)
{
	AscCanvas *canvas = ASC_CANVAS (object);
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);

	if (priv->cr != NULL)
            cairo_destroy (priv->cr);
        if (priv->srf != NULL)
            cairo_surface_destroy (priv->srf);

	G_OBJECT_CLASS (asc_canvas_parent_class)->finalize (object);
}

static void
asc_canvas_init (AscCanvas *canvas)
{
}

static void
asc_canvas_class_init (AscCanvasClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_canvas_finalize;
}

/**
 * asc_canvas_new:
 *
 * Creates a new #AscFont.
 *
 * Returns: (transfer full): an #AscCanvas
 **/
AscCanvas*
asc_canvas_new (gint width, gint height)
{
	AscCanvasPrivate *priv;
	AscCanvas *canvas = ASC_CANVAS (g_object_new (ASC_TYPE_CANVAS, NULL));
	priv = GET_PRIVATE (canvas);

	priv->srf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
	priv->cr = cairo_create (priv->srf);

	priv->width = width;
	priv->height = height;

	return canvas;
}

/**
 * asc_canvas_get_width:
 * @canvas: an #AscCanvas instance.
 *
 * Gets the canvas width.
 **/
guint
asc_canvas_get_width (AscCanvas *canvas)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	return priv->width;
}

/**
 * asc_canvas_get_height:
 * @canvas: an #AscCanvas instance.
 *
 * Gets the canvas height.
 **/
guint
asc_canvas_get_height (AscCanvas *canvas)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	return priv->height;
}
