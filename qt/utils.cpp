/*
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#include "appstream.h"
#include "utils-private.h"

using namespace Appstream;

QStringList
strv_to_stringlist(gchar **strv)
{
    QStringList list;
    if (strv == NULL)
        return list;
    for(int i = 0; strv[i] != NULL; i++) {
        list.append(QString::fromUtf8 (strv[i]));
    }

    return list;
}

gchar **
stringlist_to_strv(QStringList list)
{
    gchar **strv;

    strv = new gchar*[list.size() + 1];
    for (int i = 0; i < list.size(); i++) {
        strv[i] = g_strdup (qPrintable(list.at(i)));
    }
    strv[list.size()] = ((gchar)NULL);

    return strv;
}
