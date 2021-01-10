/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef APPSTREAMQT_UTILS_H
#define APPSTREAMQT_UTILS_H

#include <QStringList>
#include "appstreamqt_export.h"

namespace AppStream {

namespace Utils {

APPSTREAMQT_EXPORT QString currentDistroComponentId();

APPSTREAMQT_EXPORT QString currentAppStreamVersion();

APPSTREAMQT_EXPORT int vercmpSimple(const QString &a, const QString &b);

/* DEPRECATED */

Q_DECL_DEPRECATED
APPSTREAMQT_EXPORT int compareVersions(const QString &a, const QString &b);

}

}

#endif
