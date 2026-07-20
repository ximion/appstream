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

G_BEGIN_DECLS

#define ASW_TYPE_FONT (asw_font_get_type ())
G_DECLARE_FINAL_TYPE (AswFont, asw_font, ASW, FONT, GObject)

/**
 * AswFontError:
 * @ASW_FONT_ERROR_FAILED:	Generic failure.
 *
 * A metadata processing error.
 **/
typedef enum {
	ASW_FONT_ERROR_FAILED,
	/*< private >*/
	ASW_FONT_ERROR_LAST
} AswFontError;

#define ASW_FONT_ERROR asw_font_error_quark ()
GQuark	     asw_font_error_quark (void);

AswFont	    *asw_font_new_from_file (const gchar *fname, GError **error);
AswFont	    *asw_font_new_from_data (const void	 *data,
				     gssize	  len,
				     const gchar *file_basename,
				     GError	**error);
AswFont	    *asw_font_new_from_fd (gint fd, const gchar *file_basename, GError **error);

const gchar *asw_font_get_family (AswFont *font);
const gchar *asw_font_get_style (AswFont *font);
const gchar *asw_font_get_fullname (AswFont *font);
const gchar *asw_font_get_id (AswFont *font);

GList	    *asw_font_get_language_list (AswFont *font);
void	     asw_font_add_language (AswFont *font, const gchar *lang);

const gchar *asw_font_get_preferred_language (AswFont *font);
void	     asw_font_set_preferred_language (AswFont *font, const gchar *lang);

const gchar *asw_font_get_description (AswFont *font);
const gchar *asw_font_get_homepage (AswFont *font);

const gchar *asw_font_get_sample_text (AswFont *font);
void	     asw_font_set_sample_text (AswFont *font, const gchar *text);

const gchar *asw_font_get_sample_icon_text (AswFont *font);
void	     asw_font_set_sample_icon_text (AswFont *font, const gchar *text);

gboolean     asw_font_render_card_to_file (AswFont     *font,
					   const gchar *png_fname,
					   gint		width,
					   gint		height,
					   const gchar *info_label,
					   gint	       *actual_width,
					   gint	       *actual_height,
					   GError     **error);
gboolean     asw_font_render_icon_to_file (AswFont     *font,
					   const gchar *png_fname,
					   guint	size,
					   gint	       *actual_width,
					   gint	       *actual_height,
					   GError     **error);

G_END_DECLS
