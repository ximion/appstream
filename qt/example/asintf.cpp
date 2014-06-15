/* Copyright (c) 2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#include "asintf.h"

using namespace Appstream;

AsIntf::AsIntf(QObject *parent) :
    QObject(parent)
{
    asdb = new Appstream::Database(this);
    asdb->open();
}

QQmlListProperty<Appstream::Component>
AsIntf::getAllComponents()
{
    m_cpts = asdb->getAllComponents();

    QQmlListProperty<Appstream::Component> list(this, m_cpts);
    return list;
}

QQmlListProperty<Appstream::Component>
AsIntf::getAllDesktopApps()
{
    m_cpts = asdb->getComponentsByKind(Component::KindDesktop);

    QQmlListProperty<Appstream::Component> list(this, m_cpts);
    return list;
}

