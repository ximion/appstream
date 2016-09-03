/*
 * Copyright (C) 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "appstream.h"

#include "release.h"

#include <QDateTime>
#include <QDebug>
#include <QUrl>

using namespace AppStream;

class AppStream::ReleaseData : public QSharedData {
public:
    ReleaseData(AsRelease* rel) : m_release(rel)
    {
        g_object_ref(m_release);
    }

    ~ReleaseData()
    {
        g_object_unref(m_release);
    }

    bool operator==(const ReleaseData& rd) const
    {
        return rd.m_release == m_release;
    }

    AsRelease* m_release;
};

Release::Release(_AsRelease* release)
    : d(new ReleaseData(release))
{}

Release::Release(const Release &release) = default;

Release::~Release() = default;

Release& Release::operator=(const Release &release) = default;

bool Release::operator==(const Release &other) const
{
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

QString Release::version() const
{
    return QString::fromUtf8(as_release_get_version(d->m_release));
}

QDateTime Release::timestamp() const
{
    return QDateTime::fromTime_t(as_release_get_timestamp(d->m_release));
}

QString Release::description() const
{
    return QString::fromUtf8(as_release_get_description(d->m_release));
}

QString Release::activeLocale() const
{
    return QString::fromUtf8(as_release_get_active_locale(d->m_release));
}

QList<QUrl> Release::locations() const
{
    auto urls = as_release_get_locations(d->m_release);
    QList<QUrl> ret;
    ret.reserve(urls->len);
    for(uint i = 0; i<urls->len; ++i) {
        auto strval = (const gchar*) g_ptr_array_index (urls, i);
        ret << QUrl(QString::fromUtf8(strval));
    }
    return ret;
}

Checksum Release::checksum() const
{
    {
        auto cs = as_release_get_checksum(d->m_release, AS_CHECKSUM_KIND_SHA256);
        if (cs)
            return Checksum { Checksum::Sha256Checksum, QByteArray(as_checksum_get_value (cs)) };
    }

    {
        auto cs = as_release_get_checksum(d->m_release, AS_CHECKSUM_KIND_SHA1);
        if (cs)
            return Checksum { Checksum::Sha1Checksum, QByteArray(as_checksum_get_value (cs)) };
    }
    return Checksum { Checksum::NoneChecksum, "" };
}

QHash<Release::SizeKind, quint64> Release::sizes() const
{
    return {
        { InstalledSize, as_release_get_size(d->m_release, AS_SIZE_KIND_INSTALLED) },
        { DownloadSize, as_release_get_size(d->m_release, AS_SIZE_KIND_DOWNLOAD) }
    };
}

Release::UrgencyKind Release::urgency() const
{
    return Release::UrgencyKind(as_release_get_urgency(d->m_release));
}

QDebug operator<<(QDebug s, const AppStream::Release& release)
{
    s.nospace() << "AppStream::Release(" << release.version() << ": " << release.description() << ")";
    return s.space();
}
