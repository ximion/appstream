/* database-write.hpp
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

#ifndef AS_CACHE_INTERNAL_H
#define AS_CACHE_INTERNAL_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

G_GNUC_INTERNAL
void as_cache_write (const gchar *path, const gchar *locale, GPtrArray *cpts, GError **error);

G_GNUC_INTERNAL
GPtrArray *as_cache_read (const gchar *fname, GError **error);

#ifdef __cplusplus
}
#endif

#endif // AS_CACHE_INTERNAL_H
