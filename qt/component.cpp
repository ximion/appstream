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
#include "component.h"

using namespace Appstream;

Component::Component()
{
    m_cpt = as_component_new ();
}

Component::~Component()
{
    g_object_unref (m_cpt);
}

QString
Component::toString()
{
    gchar *str;
    QString qstr;

    str = as_component_to_string (m_cpt);
    qstr = QString::fromUtf8 (str);
    return qstr;
}

Component::Kind
Component::getKind()
{
    return (Component::Kind) as_component_get_kind (m_cpt);
}

void
Component::setKind(Component::Kind kind)
{
    as_component_set_kind (m_cpt, (AsComponentKind) kind);
}

QString
Component::getId()
{
    return QString::fromUtf8(as_component_get_id(m_cpt));
}

void
Component::setId(QString id)
{
    as_component_set_id(m_cpt, qPrintable(id));
}

QString
Component::getPkgname()
{
    return QString::fromUtf8(as_component_get_pkgname(m_cpt));
}

void
Component::setPkgname(QString pkgname)
{
    as_component_set_pkgname(m_cpt, qPrintable(pkgname));
}
