/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2025 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include <vips/vips.h>

#include "as-macros-private.h"
#include "asw-font.h"

AS_BEGIN_PRIVATE_DECLS

#define ASW_TYPE_CANVAS (asw_canvas_get_type ())
G_DECLARE_FINAL_TYPE (AswCanvas, asw_canvas, ASW, CANVAS, GObject)

/**
 * AswCanvasError:
 * @ASW_CANVAS_ERROR_FAILED:	  Generic failure.
 * @ASW_CANVAS_ERROR_DRAWING:	  Drawing operation failed.
 * @ASW_CANVAS_ERROR_FONT:	  Issue with font or font selection.
 * @ASW_CANVAS_ERROR_UNSUPPORTED: The requested action was not supported.
 *
 * A drawing error.
 **/
typedef enum {
	ASW_CANVAS_ERROR_FAILED,
	ASW_CANVAS_ERROR_DRAWING,
	ASW_CANVAS_ERROR_FONT,
	ASW_CANVAS_ERROR_UNSUPPORTED,
	/*< private >*/
	ASW_CANVAS_ERROR_LAST
} AswCanvasError;

/**
 * AswCanvasShape:
 * @ASW_CANVAS_SHAPE_CIRCLE:		Circle
 * @ASW_CANVAS_SHAPE_HEXAGON:		Hexagon
 * @ASW_CANVAS_SHAPE_CVL_TRIANGLE:	Curvilinear Triangle
 *
 * A type of shape to draw.
 **/
typedef enum {
	ASW_CANVAS_SHAPE_CIRCLE,
	ASW_CANVAS_SHAPE_HEXAGON,
	ASW_CANVAS_SHAPE_CVL_TRIANGLE,
	/*< private >*/
	ASW_CANVAS_SHAPE_LAST
} AswCanvasShape;

#define ASW_CANVAS_ERROR asw_canvas_error_quark ()
GQuark asw_canvas_error_quark (void);

AS_INTERNAL_VISIBLE
AswCanvas *asw_canvas_new (gint width, gint height);

guint	   asw_canvas_get_width (AswCanvas *canvas);
guint	   asw_canvas_get_height (AswCanvas *canvas);

AS_INTERNAL_VISIBLE
gboolean asw_canvas_render_svg (AswCanvas *canvas, GInputStream *stream, GError **error);

AS_INTERNAL_VISIBLE
gboolean   asw_canvas_save_png (AswCanvas *canvas, const gchar *fname, GError **error);

VipsImage *asw_canvas_to_vips (AswCanvas *canvas, GError **error);

AS_INTERNAL_VISIBLE
gboolean asw_canvas_draw_text_line (AswCanvas	*canvas,
				    AswFont	*font,
				    const gchar *text,
				    gint	 border_width,
				    gint	 vertical_offset,
				    GError     **error);
AS_INTERNAL_VISIBLE
gboolean asw_canvas_draw_text (AswCanvas   *canvas,
			       AswFont	   *font,
			       const gchar *text,
			       gint	    border_width,
			       gint	    line_pad,
			       GError	  **error);

AS_INTERNAL_VISIBLE
gboolean asw_canvas_draw_font_card (AswCanvas	*canvas,
				    AswFont	*font,
				    const gchar *info_label,
				    const gchar *pangram,
				    const gchar *bg_letter,
				    gint	 border_width,
				    GError     **error);

AS_INTERNAL_VISIBLE
gboolean asw_canvas_draw_shape (AswCanvas     *canvas,
				AswCanvasShape shape,
				gint	       border_width,
				double	       red,
				double	       green,
				double	       blue,
				GError	     **error);

AS_INTERNAL_VISIBLE
gint asw_calculate_text_border_width_for_icon_shape (AswCanvasShape bg_shape,
						     gint	    canvas_size,
						     gint	    shape_border_width);

AS_END_PRIVATE_DECLS
