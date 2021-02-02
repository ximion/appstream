/*
 * Copyright (C) 2017 Jan Grulich <jgrulich@redhat.com>
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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
#include "launchable.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::LaunchableData : public QSharedData {
public:
    LaunchableData()
    {
        m_launchable = as_launchable_new();
    }

    LaunchableData(AsLaunchable* cat) : m_launchable(cat)
    {
        g_object_ref(m_launchable);
    }

    ~LaunchableData()
    {
        g_object_unref(m_launchable);
    }

    bool operator==(const LaunchableData& rd) const
    {
        return rd.m_launchable == m_launchable;
    }

    AsLaunchable *launchable() const
    {
        return m_launchable;
    }

    AsLaunchable* m_launchable;
};


AppStream::Launchable::Kind AppStream::Launchable::stringToKind(const QString& kindString)
{
    if (kindString == QLatin1String("desktop-id")) {
        return AppStream::Launchable::KindDesktopId;
    }
    return AppStream::Launchable::KindUnknown;
}

QString AppStream::Launchable::kindToString(AppStream::Launchable::Kind kind)
{
    if (kind == AppStream::Launchable::KindDesktopId) {
        return QLatin1String("desktop-id");
    }
    return QLatin1String("unknown");
}

Launchable::Launchable()
    : d(new LaunchableData)
{}

Launchable::Launchable(_AsLaunchable* launchable)
    : d(new LaunchableData(launchable))
{}

Launchable::Launchable(const Launchable &launchable) = default;

Launchable::~Launchable() = default;

Launchable& Launchable::operator=(const Launchable &launchable) = default;

bool Launchable::operator==(const Launchable &other) const
{
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsLaunchable * AppStream::Launchable::asLaunchable() const
{
    return d->launchable();
}

AppStream::Launchable::Kind AppStream::Launchable::kind() const
{
    return static_cast<AppStream::Launchable::Kind>(as_launchable_get_kind(d->m_launchable));
}

void AppStream::Launchable::setKind(AppStream::Launchable::Kind kind)
{
    as_launchable_set_kind(d->m_launchable, (AsLaunchableKind) kind);
}

QStringList AppStream::Launchable::entries() const
{
    return valueWrap(as_launchable_get_entries(d->m_launchable));
}

void AppStream::Launchable::addEntry(const QString& entry)
{
    as_launchable_add_entry(d->m_launchable, qPrintable(entry));
}

QDebug operator<<(QDebug s, const AppStream::Launchable& launchable)
{
    s.nospace() << "AppStream::Launchable("
                << Launchable::kindToString(launchable.kind()) << ":"
                << launchable.entries() << ")";
    return s.space();
}
