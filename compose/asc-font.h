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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define ASC_TYPE_FONT (asc_font_get_type ())
G_DECLARE_FINAL_TYPE (AscFont, asc_font, ASC, FONT, GObject)

/**
 * AscFontError:
 * @ASC_FONT_ERROR_FAILED:	Generic failure.
 *
 * A metadata processing error.
 **/
typedef enum {
	ASC_FONT_ERROR_FAILED,
	/*< private >*/
	ASC_FONT_ERROR_LAST
} AscFontError;

#define	ASC_FONT_ERROR	asc_font_error_quark ()
GQuark			asc_font_error_quark (void);

AscFont	*asc_font_new_from_file (const gchar* fname,
				 GError **error);
AscFont	*asc_font_new_from_data (const void *data,
				 gssize len,
				 const gchar *file_basename,
				 GError **error);

const gchar	*asc_font_get_family (AscFont *font);
const gchar	*asc_font_get_style (AscFont *font);
const gchar	*asc_font_get_fullname (AscFont *font);
const gchar	*asc_font_get_id (AscFont *font);

GList		*asc_font_get_language_list (AscFont *font);
void		asc_font_add_language (AscFont *font,
				       const gchar *lang);

const gchar*	asc_font_get_preferred_language (AscFont *font);
void		asc_font_set_preferred_language (AscFont *font,
						 const gchar *lang);

const gchar	*asc_font_get_description (AscFont *font);
const gchar	*asc_font_get_homepage (AscFont *font);

const gchar	*asc_font_get_sample_text (AscFont *font);
void		asc_font_set_sample_text (AscFont *font,
					  const gchar *text);

const gchar	*asc_font_get_sample_icon_text (AscFont *font);
void		asc_font_set_sample_icon_text (AscFont *font,
					       const gchar *text);

G_END_DECLS
