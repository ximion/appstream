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

#include "asc-font.h"
#include "as-settings-private.h"

#include <ft2build.h>
#include FT_FREETYPE_H

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

extern GMutex fontconfig_mutex;

AS_INTERNAL_VISIBLE
FT_Encoding	asc_font_get_charset (AscFont *font);
AS_INTERNAL_VISIBLE
FT_Face		asc_font_get_ftface (AscFont *font);

AS_INTERNAL_VISIBLE
const gchar	*asc_font_find_pangram (AscFont *font,
					const gchar *lang,
					const gchar *rand_id);

#pragma GCC visibility pop
G_END_DECLS
