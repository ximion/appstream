/*
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef UTILS_PRIVATE_H
#define UTILS_PRIVATE_H

#include <QtCore>
#include <glib.h>

namespace Appstream {

QStringList strv_to_stringlist(gchar **strv);
gchar **stringlist_to_strv(QStringList list);

QStringList strarray_to_stringlist(GPtrArray *array);

} // End of namespace: Appstream

#endif // UTILS_PRIVATE_H
