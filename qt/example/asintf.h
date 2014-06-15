/* Copyright (c) 2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#ifndef ASINTF_H
#define ASINTF_H

#include <QObject>
#include <appstream-qt.h>
#include <QQmlListProperty>

class AsIntf : public QObject
{
    Q_OBJECT
public:
    explicit AsIntf(QObject *parent = 0);

    Q_PROPERTY(QQmlListProperty<Appstream::Component> allComponents READ getAllComponents CONSTANT)
    QQmlListProperty<Appstream::Component> getAllComponents();

    Q_PROPERTY(QQmlListProperty<Appstream::Component> desktopApps READ getAllDesktopApps CONSTANT)
    QQmlListProperty<Appstream::Component> getAllDesktopApps();

signals:

public slots:

private:
    Appstream::Database *asdb;
    QList<Appstream::Component*> m_cpts;

};

#endif // ASINTF_H
