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

#include "utils.h"

#include "appstream.h"
#include "chelpers.h"

QString AppStream::Utils::currentDistroComponentId()
{
    return QString::fromUtf8(as_get_current_distro_component_id());
}


QString AppStream::Utils::currentAppStreamVersion()
{
    return QString::fromUtf8(as_version_string());
}

int AppStream::Utils::vercmpSimple(const QString &a, const QString &b)
{
    return as_vercmp (qPrintable(a), qPrintable(b), AS_VERCMP_FLAG_NONE);
}

int AppStream::Utils::compareVersions(const QString &a, const QString &b)
{
    gint r;
    r = as_vercmp (qPrintable(a), qPrintable(b), AS_VERCMP_FLAG_NONE);
    if (r == 0)
        return 0;
    return (r < 0)? -1 : 1;
}
