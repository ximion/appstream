/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-macros-private.h"
#include "asc-canvas.h"
#include "asc-font.h"

AS_BEGIN_PRIVATE_DECLS

/**
 * AscCanvasShape:
 * @ASC_CANVAS_SHAPE_CIRCLE:	 Circle
 * @ASC_CANVAS_SHAPE_HEXAGON:	 Hexagon
 * @ASC_CANVAS_ERROR_PENTAGON:	 Penatgon
 *
 * A type of shape to draw.
 **/
typedef enum {
	ASC_CANVAS_SHAPE_CIRCLE,
	ASC_CANVAS_SHAPE_HEXAGON,
	ASC_CANVAS_SHAPE_PENTAGON,
	/*< private >*/
	ASC_CANVAS_SHAPE_LAST
} AscCanvasShape;

AS_INTERNAL_VISIBLE
gboolean asc_canvas_draw_text_line (AscCanvas	*canvas,
				    AscFont	*font,
				    const gchar *text,
				    gint	 border_width,
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

AS_END_PRIVATE_DECLS
