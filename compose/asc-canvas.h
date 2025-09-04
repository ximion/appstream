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

#if !defined(__APPSTREAM_COMPOSE_H) && !defined(ASC_COMPILATION)
#error "Only <appstream-compose.h> can be included directly."
#endif
#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "as-macros-private.h"
#include "asc-font.h"

AS_BEGIN_PRIVATE_DECLS

#define ASC_TYPE_CANVAS (asc_canvas_get_type ())
G_DECLARE_FINAL_TYPE (AscCanvas, asc_canvas, ASC, CANVAS, GObject)

/**
 * AscCanvasError:
 * @ASC_CANVAS_ERROR_FAILED:	  Generic failure.
 * @ASC_CANVAS_ERROR_DRAWING:	  Drawing operation failed.
 * @ASC_CANVAS_ERROR_FONT:	  Issue with font or font selection.
 * @ASC_CANVAS_ERROR_UNSUPPORTED: The requested action was not supported.
 *
 * A drawing error.
 **/
typedef enum {
	ASC_CANVAS_ERROR_FAILED,
	ASC_CANVAS_ERROR_DRAWING,
	ASC_CANVAS_ERROR_FONT,
	ASC_CANVAS_ERROR_UNSUPPORTED,
	/*< private >*/
	ASC_CANVAS_ERROR_LAST
} AscCanvasError;

/**
 * AscCanvasShape:
 * @ASC_CANVAS_SHAPE_CIRCLE:		Circle
 * @ASC_CANVAS_SHAPE_HEXAGON:		Hexagon
 * @ASC_CANVAS_SHAPE_CVL_TRIANGLE:	Curvilinear Triangle
 *
 * A type of shape to draw.
 **/
typedef enum {
	ASC_CANVAS_SHAPE_CIRCLE,
	ASC_CANVAS_SHAPE_HEXAGON,
	ASC_CANVAS_SHAPE_CVL_TRIANGLE,
	/*< private >*/
	ASC_CANVAS_SHAPE_LAST
} AscCanvasShape;

#define ASC_CANVAS_ERROR asc_canvas_error_quark ()
GQuark asc_canvas_error_quark (void);

AS_INTERNAL_VISIBLE
AscCanvas *asc_canvas_new (gint width, gint height);

guint	   asc_canvas_get_width (AscCanvas *canvas);
guint	   asc_canvas_get_height (AscCanvas *canvas);

AS_INTERNAL_VISIBLE
gboolean asc_canvas_render_svg (AscCanvas *canvas, GInputStream *stream, GError **error);

AS_INTERNAL_VISIBLE
gboolean   asc_canvas_save_png (AscCanvas *canvas, const gchar *fname, GError **error);

GdkPixbuf *asc_canvas_to_pixbuf (AscCanvas *canvas);

AS_INTERNAL_VISIBLE
gboolean asc_canvas_draw_text_line (AscCanvas	*canvas,
				    AscFont	*font,
				    const gchar *text,
				    gint	 border_width,
				    gint	 vertical_offset,
				    GError     **error);
AS_INTERNAL_VISIBLE
gboolean asc_canvas_draw_text (AscCanvas   *canvas,
			       AscFont	   *font,
			       const gchar *text,
			       gint	    border_width,
			       gint	    line_pad,
			       GError	  **error);

AS_INTERNAL_VISIBLE
gboolean asc_canvas_draw_font_card (AscCanvas	*canvas,
				    AscFont	*font,
				    const gchar *info_label,
				    const gchar *pangram,
				    const gchar *bg_letter,
				    gint	 border_width,
				    GError     **error);

AS_INTERNAL_VISIBLE
gboolean asc_canvas_draw_shape (AscCanvas     *canvas,
				AscCanvasShape shape,
				gint	       border_width,
				double	       red,
				double	       green,
				double	       blue,
				GError	     **error);

AS_INTERNAL_VISIBLE
gint asc_calculate_text_border_width_for_icon_shape (AscCanvasShape bg_shape,
						     gint	    canvas_size,
						     gint	    shape_border_width);

AS_END_PRIVATE_DECLS
