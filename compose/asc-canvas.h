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

#if !defined (__APPSTREAM_COMPOSE_H) && !defined (ASC_COMPILATION)
#error "Only <appstream-compose.h> can be included directly."
#endif
#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#include "asc-font.h"

G_BEGIN_DECLS

#define ASC_TYPE_CANVAS (asc_canvas_get_type ())
G_DECLARE_FINAL_TYPE (AscCanvas, asc_canvas, ASC, CANVAS, GObject)

/**
 * AscCanvasError:
 * @ASC_CANVAS_ERROR_FAILED:	Generic failure.
 * @ASC_CANVAS_ERROR_DRAWING:	Drawing operation failed.
 * @ASC_CANVAS_ERROR_FONT:	Issue with font or font selection.
 *
 * A drawing error.
 **/
typedef enum {
	ASC_CANVAS_ERROR_FAILED,
	ASC_CANVAS_ERROR_DRAWING,
	ASC_CANVAS_ERROR_FONT,
	/*< private >*/
	ASC_CANVAS_ERROR_LAST
} AscCanvasError;

#define	ASC_CANVAS_ERROR	asc_canvas_error_quark ()
GQuark				asc_canvas_error_quark (void);

AscCanvas	*asc_canvas_new (gint width,
				 gint height);

guint		asc_canvas_get_width (AscCanvas *canvas);
guint		asc_canvas_get_height (AscCanvas *canvas);

gboolean	asc_canvas_render_svg (AscCanvas* canvas,
					GInputStream *stream,
					GError** error);

gboolean	asc_canvas_save_png (AscCanvas *canvas,
					const gchar *fname,
					GError **error);

G_END_DECLS
