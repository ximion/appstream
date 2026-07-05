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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "as-macros-private.h"
#include "asw-image.h"

AS_BEGIN_PRIVATE_DECLS

gboolean   asw_optimize_png (const gchar *fname, GError **error);

GdkPixbuf *asw_image_save_pixbuf (AswImage	   *image,
				  gint		    width,
				  gint		    height,
				  AswImageSaveFlags flags);

GdkPixbuf *asw_image_get_pixbuf (AswImage *image);
void	   asw_image_set_pixbuf (AswImage *image, GdkPixbuf *pixbuf);

void	   asw_pixbuf_blur (GdkPixbuf *src, gint radius, gint iterations);
void	   asw_pixbuf_sharpen (GdkPixbuf *src, gint radius, gdouble amount);

AS_END_PRIVATE_DECLS
