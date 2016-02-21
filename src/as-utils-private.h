/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_UTILS_PRIVATE_H
#define __AS_UTILS_PRIVATE_H

#include <glib-object.h>

G_BEGIN_DECLS

gboolean		as_utils_delete_dir_recursive (const gchar* dirname);

GPtrArray		*as_utils_categories_from_strv (gchar **categories_strv,
							GPtrArray *system_categories);
GPtrArray		*as_utils_categories_from_str (const gchar *categories_str,
							GPtrArray *system_categories);

GPtrArray		*as_utils_find_files_matching (const gchar *dir,
							const gchar *pattern,
							gboolean recursive,
							GError **error);
GPtrArray		*as_utils_find_files (const gchar *dir,
						gboolean recursive,
						GError **error);

gboolean		as_utils_is_root (void);

gboolean		as_utils_is_writable (const gchar *path);

gchar			*as_str_replace (const gchar *str,
					 const gchar *old_str,
					 const gchar *new_str);

gchar			**as_ptr_array_to_strv (GPtrArray *array);

gboolean		as_touch_location (const gchar *fname);

G_END_DECLS

#endif /* __AS_UTILS_PRIVATE_H */
