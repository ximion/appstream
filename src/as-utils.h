/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_UTILS_H
#define __AS_UTILS_H

#include <glib-object.h>

G_BEGIN_DECLS

gchar		*as_description_markup_convert_simple (const gchar *markup);
gchar		*as_get_current_locale (void);
gboolean	as_str_empty (const gchar* str);
GDateTime	*as_iso8601_to_datetime (const gchar *iso_date);
gboolean	as_utils_locale_is_compatible (const gchar *locale1, const gchar *locale2);
gboolean	as_utils_is_category_name (const gchar *category_name);

G_END_DECLS

#endif /* __AS_UTILS_H */
