/*
 * Copyright (C) 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
 * Copyright (C) 2018-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "release.h"

#include <QDateTime>
#include <QDebug>
#include <QUrl>

using namespace AppStream;

class AppStream::ReleaseData : public QSharedData
{
public:
    ReleaseData(AsRelease *rel)
        : m_release(rel)
    {
        g_object_ref(m_release);
    }

    ~ReleaseData()
    {
        g_object_unref(m_release);
    }

    bool operator==(const ReleaseData &rd) const
    {
        return rd.m_release == m_release;
    }

    AsRelease *release() const
    {
        return m_release;
    }

    AsRelease *m_release;
};

Release::Release(_AsRelease *release)
    : d(new ReleaseData(release))
{
}

Release::Release(const Release &release) = default;

Release::~Release() = default;

Release &Release::operator=(const Release &release) = default;

bool Release::operator==(const Release &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsRelease *AppStream::Release::cPtr() const
{
    return d->release();
}

Release::Kind Release::kind() const
{
    return Release::Kind(as_release_get_kind(d->m_release));
}

QString Release::version() const
{
    return QString::fromUtf8(as_release_get_version(d->m_release));
}

QDateTime Release::timestamp() const
{
    const guint64 timestamp = as_release_get_timestamp(d->m_release);
    return timestamp > 0 ? QDateTime::fromSecsSinceEpoch(timestamp) : QDateTime();
}

QDateTime Release::timestampEol() const
{
    const guint64 timestamp = as_release_get_timestamp_eol(d->m_release);
    return timestamp > 0 ? QDateTime::fromSecsSinceEpoch(timestamp) : QDateTime();
}

QString Release::description() const
{
    return QString::fromUtf8(as_release_get_description(d->m_release));
}

Release::UrgencyKind Release::urgency() const
{
    return Release::UrgencyKind(as_release_get_urgency(d->m_release));
}

QDebug operator<<(QDebug s, const AppStream::Release &release)
{
    s.nospace() << "AppStream::Release(" << release.version() << ": " << release.description()
                << ")";
    return s.space();
}
