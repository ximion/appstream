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

#ifndef APPSTREAMQT_CHELPERS_H
#define APPSTREAMQT_CHELPERS_H

#include "qanystringview.h"
#include "qglobal.h"
#include <glib.h>
#include <QStringList>
#include <QAnyStringView>

namespace AppStream {

inline QAnyStringView valueWrap(const gchar *cstr)
{
    return QAnyStringView(cstr);
}

inline QList<QAnyStringView> valueWrap(gchar **strv)
{
    QList<QAnyStringView> res;
    if (strv == NULL)
        return res;
    for (uint i = 0; strv[i] != NULL; i++) {
        res.append (QAnyStringView(strv[i]));
    }
    return res;
}

inline QList<QAnyStringView> valueWrap(const gchar **strv)
{
    QList<QAnyStringView> res;
    if (strv == NULL)
        return res;
    for (uint i = 0; strv[i] != NULL; i++) {
        res.append (QAnyStringView(strv[i]));
    }
    return res;
}

inline QList<QAnyStringView> valueWrap(GPtrArray *array)
{
    QList<QAnyStringView> res;
    res.reserve(array->len);
    for (uint i = 0; i < array->len; i++) {
        auto strval = (const gchar*) g_ptr_array_index (array, i);
        res.append (QAnyStringView(strval));
    }
    return res;
}

inline QList<QAnyStringView> valueWrap(GList *list)
{
    GList *l;
    QList<QAnyStringView> res;
    res.reserve(g_list_length(list));
    for (l = list; l != NULL; l = l->next) {
        auto strval = (const gchar*) l->data;
        res.append (QAnyStringView(strval));
    }
    return res;
}

inline const char* stringViewToChar(QAnyStringView str) {
    QByteArray bytes = QByteArray();
    bytes.resize(str.size_bytes() + 1);
    bytes.append((char*)str.data(), str.size_bytes());
    bytes.append('\0');
    return bytes.constData();
}

inline char ** stringListToCharArray(QList<QAnyStringView> list)
{
    char **array = (char**) g_malloc(sizeof(char*) * list.size() + 1);
    for (int i = 0; i < list.size(); ++i) {
        array[i] = (char*) g_malloc(sizeof(char) * (list[i].size_bytes() + 1));
        strcpy(array[i], stringViewToChar(list[i]));
    }
    array[list.size()] = nullptr;
    return array;
}

}

#endif // APPSTREAMQT_CHELPERS_H
