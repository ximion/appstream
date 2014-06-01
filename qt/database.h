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

#ifndef DATABASE_H
#define DATABASE_H

#include <QtCore>
#include "appstream-qt_global.h"
#include "component.h"

namespace Appstream {

typedef struct _AsDatabase AsDatabase;

class ASQTSHARED_EXPORT Database : QObject
{
    Q_OBJECT
    Q_ENUMS(Kind)

public:
    Database();
    ~Database();

    bool open();

    QList<Component*> *getAllComponents();

private:
    AsDatabase *m_db;
};

} // End of namespace: Appstream

#endif // DATABASE_H
