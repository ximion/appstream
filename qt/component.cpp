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
#include "utils-private.h"

using namespace Appstream;

Component::Component(QObject *parent)
    : QObject(parent)
{
    m_cpt = as_component_new ();
}

Component::Component(AsComponent *cpt, QObject *parent)
    : QObject(parent)
{
    m_cpt = cpt;
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
Component::kindToString(Component::Kind kind)
{
    return QString::fromUtf8(as_component_kind_to_string ((AsComponentKind) kind));

}

Component::Kind
Component::kindFromString(QString kind_str)
{
    return (Component::Kind) as_component_kind_from_string (qPrintable(kind_str));

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
Component::getPkgName()
{
    return QString::fromUtf8(as_component_get_pkgname(m_cpt));
}

void
Component::setPkgName(QString pkgname)
{
    as_component_set_pkgname(m_cpt, qPrintable(pkgname));
}

QString
Component::getName()
{
    return QString::fromUtf8(as_component_get_name(m_cpt));
}

void
Component::setName(QString name)
{
    as_component_set_name(m_cpt, qPrintable(name));
}

QString
Component::getSummary()
{
    return QString::fromUtf8(as_component_get_summary(m_cpt));
}

void
Component::setSummary(QString summary)
{
    as_component_set_summary(m_cpt, qPrintable(summary));
}

QString
Component::getDescription()
{
    return QString::fromUtf8(as_component_get_description(m_cpt));
}

void
Component::setDescription(QString description)
{
    as_component_set_description(m_cpt, qPrintable(description));
}

QString
Component::getProjectLicense()
{
    return QString::fromUtf8(as_component_get_project_license(m_cpt));
}

void
Component::setProjectLicense(QString license)
{
    as_component_set_project_license(m_cpt, qPrintable(license));
}

QString
Component::getProjectGroup()
{
    return QString::fromUtf8(as_component_get_project_group(m_cpt));
}

void
Component::setProjectGroup(QString group)
{
    as_component_set_project_group(m_cpt, qPrintable(group));
}

QStringList
Component::getCompulsoryForDesktops()
{
    return strv_to_stringlist(as_component_get_compulsory_for_desktops(m_cpt));
}

void
Component::setCompulsoryForDesktops(QStringList desktops)
{
    gchar **strv;
    strv = stringlist_to_strv(desktops);
    as_component_set_compulsory_for_desktops(m_cpt, strv);
    g_strfreev(strv);
}

bool
Component::isCompulsoryForDesktop(QString desktop)
{
    return as_component_is_compulsory_for_desktop(m_cpt, qPrintable(desktop));
}

QStringList
Component::getCategories()
{
    return strv_to_stringlist(as_component_get_categories(m_cpt));
}

void
Component::setCategories(QStringList categories)
{
    gchar **strv;
    strv = stringlist_to_strv(categories);
    as_component_set_categories(m_cpt, strv);
    g_strfreev(strv);
}

bool
Component::hasCategory(QString category)
{
    return as_component_has_category(m_cpt, qPrintable(category));
}

QString
Component::getIcon()
{
    return QString::fromUtf8(as_component_get_icon(m_cpt));
}

void
Component::setIcon(QString icon)
{
    as_component_set_icon(m_cpt, qPrintable(icon));
}

QString
Component::getIconUrl()
{
    return QString::fromUtf8(as_component_get_icon_url(m_cpt));
}

void
Component::setIconUrl(QString icon_url)
{
    as_component_set_icon_url(m_cpt, qPrintable(icon_url));
}
