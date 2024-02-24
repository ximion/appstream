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

#include "appstream.h"
#include "developer.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::DeveloperData : public QSharedData
{
public:
    DeveloperData()
    {
        devp = as_developer_new();
    }

    DeveloperData(AsDeveloper *developer)
        : devp(developer)
    {
        g_object_ref(devp);
    }

    ~DeveloperData()
    {
        g_object_unref(devp);
    }

    bool operator==(const DeveloperData &rd) const
    {
        return rd.devp == devp;
    }

    AsDeveloper *devp;
};

Developer::Developer()
    : d(new DeveloperData)
{
}

Developer::Developer(_AsDeveloper *devp)
    : d(new DeveloperData(devp))
{
}

Developer::Developer(const Developer &devp) = default;

Developer::~Developer() = default;

Developer &Developer::operator=(const Developer &devp) = default;

bool Developer::operator==(const Developer &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsDeveloper *AppStream::Developer::cPtr() const
{
    return d->devp;
}

QString AppStream::Developer::id() const
{
    return valueWrap(as_developer_get_id(d->devp));
}

void AppStream::Developer::setId(const QString &id)
{
    as_developer_set_id(d->devp, qPrintable(id));
}

QString Developer::name() const
{
    return valueWrap(as_developer_get_name(d->devp));
}

void Developer::setName(const QString &name, const QString &lang)
{
    as_developer_set_name(d->devp, qPrintable(name), lang.isEmpty() ? NULL : qPrintable(lang));
}

QDebug operator<<(QDebug s, const AppStream::Developer &devp)
{
    s.nospace() << "AppStream::Developer(" << devp.id() << ":" << devp.name() << ")";
    return s.space();
}
