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
#include <appstream.h>

#include "asc-media.h"
#include "asw-canvas.h"

G_BEGIN_DECLS

#define ASW_TYPE_IMAGE (asw_image_get_type ())
G_DECLARE_FINAL_TYPE (AswImage, asw_image, ASW, IMAGE, GObject)

GHashTable *asw_image_supported_format_names (void);
AswImage   *asw_image_new (void);
AswImage   *asw_image_new_from_file (const gchar      *fname,
				     gint	       dest_width,
				     gint	       dest_height,
				     AscImageLoadFlags flags,
				     GError	     **error);
AswImage   *asw_image_new_from_data (const void	      *data,
				     gssize	       len,
				     gint	       dest_width,
				     gint	       dest_height,
				     AscImageLoadFlags flags,
				     GError	     **error);

gboolean    asw_image_load_filename (AswImage	      *image,
				     const gchar      *filename,
				     gint	       dest_width,
				     gint	       dest_height,
				     gint	       src_size_min,
				     AscImageLoadFlags flags,
				     GError	     **error);

gboolean    asw_image_save_filename (AswImage	      *image,
				     const gchar      *filename,
				     gint	       width,
				     gint	       height,
				     AscImageSaveFlags flags,
				     GError	     **error);

gint	    asw_image_get_width (AswImage *image);
gint	    asw_image_get_height (AswImage *image);

void	    asw_image_scale (AswImage *image, gint new_width, gint new_height);

void	    asw_image_scale_to_width (AswImage *image, gint new_width);
void	    asw_image_scale_to_height (AswImage *image, gint new_height);
void	    asw_image_scale_to_fit (AswImage *image, gint size);

gboolean    asw_canvas_save_to_file (AswCanvas	   *canvas,
				     const gchar   *filename,
				     AscImageFormat format,
				     gboolean	    lossless,
				     GError	  **error);

G_END_DECLS
