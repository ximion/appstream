/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsDeveloper;

namespace AppStream
{

class DeveloperData;

class APPSTREAMQT_EXPORT Developer
{
    Q_GADGET

public:
    Developer();
    Developer(_AsDeveloper *devp);
    Developer(const Developer &devp);
    ~Developer();

    Developer &operator=(const Developer &devp);
    bool operator==(const Developer &r) const;

    /**
     * \returns the internally stored AsDeveloper
     */
    _AsDeveloper *cPtr() const;

    QString id() const;
    void setId(const QString &id);

    QString name() const;
    void setName(const QString &name, const QString &lang = {});

private:
    QSharedDataPointer<DeveloperData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Developer &devp);
