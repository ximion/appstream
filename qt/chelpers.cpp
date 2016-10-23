/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#include <appstream.h>
#include "chelpers.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(APPSTREAMQT_CHELPERS, "appstreamqt.chelpers")

using namespace AppStream;

QString value(const gchar *cstr)
{
    return QString::fromUtf8(cstr);
}

QStringList value(gchar **strv)
{
    QStringList res;
    if (strv == NULL)
        return res;
    for (uint i = 0; strv[i] != NULL; i++) {
        res.append (QString::fromUtf8(strv[i]));
    }
    return res;
}

QStringList value(GPtrArray *array)
{
    QStringList res;
    res.reserve(array->len);
    for (uint i = 0; i < array->len; i++) {
        auto strval = (const gchar*) g_ptr_array_index (array, i);
        res.append (QString::fromUtf8(strval));
    }
    return res;
}
