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

class Appstream::DatabasePrivate
{
public:
    DatabasePrivate()
    {
        db = as_database_new ();
    }

    ~DatabasePrivate() {
        g_object_unref (db);
    }

    AsDatabase *db;
};

Database::Database(QObject *parent)
    : QObject(parent)
{
    priv = new DatabasePrivate();
}

Database::~Database()
{
    delete priv;
}

bool
Database::open()
{
    return as_database_open(priv->db);
}

static Component*
asqt_wrap_component (AsComponent *cpt) {
    g_object_ref(cpt);
    Component *qcpt = new Component(cpt);
    return qcpt;
}

QList<Component*>
Database::getAllComponents()
{
	QList<Component*> cpts;
    GPtrArray *array;

    array = as_database_get_all_components(priv->db);
    if (array == NULL)
        return cpts;
    if (array->len == 0) {
        goto out;
    }

    for (unsigned int i = 0; i < array->len; i++) {
        AsComponent *cpt;
        cpt = (AsComponent*) g_ptr_array_index (array, i);
        cpts.append(asqt_wrap_component (cpt));
    }

out:
    g_ptr_array_unref (array);
    return cpts;
}
