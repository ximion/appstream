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
 * SECTION:asc-canvas
 * @short_description: Draw text and render SVG canvass.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-canvas.h"
#include "asc-canvas-private.h"

#include <cairo.h>
#include <cairo-ft.h>
#include <librsvg/rsvg.h>

#include "asc-font-private.h"
#include "asc-image.h"

struct _AscCanvas
{
	GObject parent_instance;
};

typedef struct
{
	cairo_t *cr;
	cairo_surface_t *srf;

	gint width;
	gint height;
} AscCanvasPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscCanvas, asc_canvas, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_canvas_get_instance_private (o))

/**
 * asc_canvas_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
asc_canvas_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AscCanvasError");
	return quark;
}

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

/**
 * asc_canvas_render_svg:
 * @canvas: an #AscCanvas instance.
 * @stream: SVG data input stream.
 * @error: A #GError or %NULL
 *
 * Render an SVG graphic from the SVG data provided.
 **/
gboolean
asc_canvas_render_svg (AscCanvas *canvas, GInputStream *stream, GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	RsvgHandle *handle = NULL;
	gboolean ret = FALSE;
	gdouble srf_width, srf_height;
#if LIBRSVG_CHECK_VERSION(2, 52, 0)
	RsvgRectangle viewport;
#else
	RsvgDimensionData dims;
#endif

	/* NOTE: unfortunately, Cairo/RSvg may use Fontconfig internally, so
	 * we need to lock this down since a parallel-processed font
	 * might need to access this too. */
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);

	handle = rsvg_handle_new_from_stream_sync (stream,
						   NULL,
						   RSVG_HANDLE_FLAGS_NONE,
						   NULL,
						   error);
	if (handle == NULL)
		goto out;
	rsvg_handle_set_dpi (handle, 100);

	srf_width = (gdouble) cairo_image_surface_get_width (priv->srf);
	srf_height = (gdouble) cairo_image_surface_get_height (priv->srf);

	cairo_save (priv->cr);

#if LIBRSVG_CHECK_VERSION(2, 52, 0)
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = srf_width;
	viewport.height = srf_height;

	ret = rsvg_handle_render_document (handle,
					   priv->cr,
					   &viewport,
					   error);
	if (!ret) {
		cairo_restore (priv->cr);
		g_prefix_error (error, "SVG graphic rendering failed:");
		goto out;
	}
#else
	rsvg_handle_get_dimensions (handle, &dims);

	/* cairo_translate (cr, (srf_width - dims.width) / 2, (srf_height - dims.height) / 2); */
	cairo_scale (priv->cr,
		     srf_width / dims.width,
		     srf_height / dims.height);

	ret = rsvg_handle_render_cairo (handle, priv->cr);
	if (!ret) {
		cairo_restore (priv->cr);
		g_set_error_literal (error,
				     ASC_CANVAS_ERROR,
				     ASC_CANVAS_ERROR_DRAWING,
				     "SVG graphic rendering failed.");
		goto out;
	}
#endif

	ret = TRUE;
out:
	if (handle != NULL)
		g_object_unref (handle);
	return ret;
}

/**
 * asc_canvas_draw_text_line:
 * @canvas: an #AscCanvas instance.
 * @font: an #AscFont to use for drawing the text.
 * @border_width: Border with around the text, set to -1 to use defaults.
 * @error: A #GError or %NULL
 *
 * Draw a simple line of text without linebreaks to fill the canvas.
 **/
gboolean
asc_canvas_draw_text_line (AscCanvas *canvas, AscFont *font, const gchar *text, gint border_width, GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	cairo_font_face_t *cff = NULL;
	cairo_status_t status;
	cairo_text_extents_t te;
	gint text_size;
	gboolean ret = FALSE;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);

	/* set default value */
	if (border_width < 0)
		border_width = 4;

	if (text == NULL) {
		g_set_error_literal (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FAILED,
			     "Can not draw NULL string.");
		return FALSE;
	}

	cff = cairo_ft_font_face_create_for_ft_face (asc_font_get_ftface (font), FT_LOAD_DEFAULT);

	/* set font face for Cairo surface */
	status = cairo_font_face_status (cff);
	if (status != CAIRO_STATUS_SUCCESS) {
		g_set_error (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FONT,
			     "Could not set font face for Cairo: %i", status);
		goto out;
	}
	cairo_set_font_face (priv->cr, cff);

	text_size = 128;
	while (text_size-- > 0) {
		cairo_set_font_size (priv->cr, text_size);
		cairo_text_extents (priv->cr, text, &te);
		if (te.width <= 0.01f || te.height <= 0.01f)
			continue;
		if (te.width < priv->width - (border_width * 2) &&
		    te.height < priv->height - (border_width * 2))
			break;
	}

	/* draw text */
	cairo_move_to (priv->cr,
    		       (priv->width / 2) - te.width / 2 - te.x_bearing,
    		       (priv->height / 2) - te.height / 2 - te.y_bearing);
    	cairo_set_source_rgb (priv->cr, 0.0, 0.0, 0.0);
    	cairo_show_text (priv->cr, text);

        cairo_save (priv->cr);
	ret = TRUE;

out:
	if (cff != NULL)
		cairo_font_face_destroy (cff);
	return ret;
}

/**
 * asc_canvas_draw_text:
 * @canvas: an #AscCanvas instance.
 * @font: an #AscFont to use for drawing the text.
 * @border_width: Border with around the text (set to -1 to use defaults).
 * @line_pad: Padding between lines (set to -1 to use defaults).
 * @error: A #GError or %NULL
 *
 * Draw a longer text with linebreaks.
 **/
gboolean
asc_canvas_draw_text (AscCanvas *canvas, AscFont *font, const gchar *text, gint border_width, gint line_pad, GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	gboolean ret = FALSE;
	cairo_font_face_t *cff = NULL;
	cairo_status_t status;
	g_auto(GStrv) lines = NULL;
	guint lines_len;
	guint line_padding;
	const gchar *longest_line;
	gint text_size;
	double x_pos, y_pos, te_height;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);

	/* set default values */
	if (border_width < 0)
		border_width = 4;
	if (line_pad < 0)
		line_pad = 2;

	if (text == NULL) {
		g_set_error_literal (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FAILED,
			     "Can not draw NULL string.");
		return FALSE;
	}

	cff = cairo_ft_font_face_create_for_ft_face (asc_font_get_ftface (font), FT_LOAD_DEFAULT);
        /* set font face for Cairo surface */
	status = cairo_font_face_status (cff);
	if (status != CAIRO_STATUS_SUCCESS) {
		g_set_error (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FONT,
			     "Could not set font face for Cairo: %i", status);
		goto out;
	}
	cairo_set_font_face (priv->cr, cff);

	/* calculate best font size */
        line_padding = line_pad;
        lines = g_strsplit (text, "\n", -1);
	lines_len = g_strv_length (lines);
        if (lines_len <= 1) {
            line_padding = 0;
            longest_line = text;
        } else {
		guint ll = 0;
		longest_line = lines[0];
		for (guint i = 0; lines[i] != NULL; i++) {
			guint l_len = strlen (lines[i]);
			if (l_len > ll) {
				longest_line = lines[i];
				ll = l_len;
			}
		}
        }

        cairo_text_extents_t te;
	text_size = 128;
	while (text_size-- > 0) {
		cairo_set_font_size (priv->cr, text_size);
		cairo_text_extents (priv->cr, longest_line, &te);
		if (te.width <= 0.01f || te.height <= 0.01f)
			continue;
		if (te.width < priv->width - (border_width * 2) &&
		   (te.height * lines_len + line_padding) < priv->height - (border_width * 2))
			break;
	}

	/* center text and draw it */
        x_pos = (priv->width / 2.0) - te.width / 2 - te.x_bearing;
        te_height = (double) te.height * lines_len + line_padding * ((double) lines_len - 1);
        y_pos = (te_height / 2) - te_height / 2 - te.y_bearing + border_width;
        cairo_move_to (priv->cr, x_pos, y_pos);
        cairo_set_source_rgb (priv->cr, 0.0, 0.0, 0.0);

	for (guint i = 0; lines[i] != NULL; i++) {
		cairo_show_text (priv->cr, lines[i]);
		y_pos += te.height + line_padding;
		cairo_move_to (priv->cr, x_pos, y_pos);
	}

        cairo_save (priv->cr);
	ret = TRUE;

out:
	if (cff != NULL)
		cairo_font_face_destroy (cff);
	return ret;
}

/**
 * asc_canvas_save_png:
 * @canvas: an #AscCanvas instance.
 * @fname: Filename to save to.
 * @error: A #GError or %NULL
 *
 * Save canvas to PNG file.
 **/
gboolean
asc_canvas_save_png (AscCanvas *canvas, const gchar *fname, GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	cairo_status_t status;

	status = cairo_surface_write_to_png (priv->srf, fname);
        if (status != CAIRO_STATUS_SUCCESS) {
		g_set_error (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FONT,
			     "Could not save canvas to PNG: %s",
			     cairo_status_to_string (status));
		return FALSE;
	}

	return asc_optimize_png (fname, error);
}
