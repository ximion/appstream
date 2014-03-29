/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

gboolean		as_utils_str_empty (const gchar* str);
gboolean		as_utils_touch_dir (const gchar* dirname);
gboolean		as_utils_delete_dir_recursive (const gchar* dirname);
gchar*			as_string_strip (const gchar* str);
gchar**			as_ptr_array_to_strv (GPtrArray *array);
GPtrArray*		as_utils_categories_from_strv (gchar** categories_strv, GList* system_categories);
GPtrArray*		as_utils_categories_from_str (const gchar* categories_str, GList* system_categories);
GPtrArray*		as_utils_find_files_matching (const gchar* dir, const gchar* pattern, gboolean recursive);
GPtrArray*		as_utils_find_files (const gchar* dir, gboolean recursive);
gboolean		as_utils_is_root (void);
gchar*			as_string_strip (const gchar* str);
gchar**			as_ptr_array_to_strv (GPtrArray *array);
gchar**			as_strv_dup (gchar** strv);

G_END_DECLS

#endif /* __AS_UTILS_H */
