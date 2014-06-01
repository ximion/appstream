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
#include "database.h"

using namespace Appstream;

Database::Database()
{
    m_db = as_database_new ();
}

Database::~Database()
{
    g_object_unref (m_db);
}

bool
Database::open()
{
    return as_database_open(m_db);
}

QList<Component*>*
Database::getAllComponents()
{
    QList<Component*> *cpts;
    GPtrArray *array;

    cpts = new QList<Component*> ();
    array = as_database_get_all_components(m_db);
    if (array->len == 0) {
        goto out;
    }

    for (unsigned int i = 0; i < array->len; i++) {
        AsComponent *as_cpt;
        as_cpt = (AsComponent*) g_ptr_array_index (array, i);
        g_object_ref(as_cpt);
        Component *cpt = new Component(as_cpt);
        cpts->append(cpt);
    }

out:
    g_ptr_array_unref (array);
    return cpts;
}
