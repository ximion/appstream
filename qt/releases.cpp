/*
 * Copyright (C) 2019-2023 Matthias Klumpp <matthias@tenstral.net>
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
#include "releases.h"

#include <QSharedData>
#include <QDebug>

using namespace AppStream;

class AppStream::ReleasesData : public QSharedData
{
public:
    ReleasesData()
    {
        m_rels = as_releases_new();
    }

    ReleasesData(AsReleases *rels)
        : m_rels(rels)
    {
        g_object_ref(m_rels);
    }

    ~ReleasesData()
    {
        g_object_unref(m_rels);
    }

    bool operator==(const ReleasesData &rd) const
    {
        return rd.m_rels == m_rels;
    }

    AsReleases *Releases() const
    {
        return m_rels;
    }

    AsReleases *m_rels;
};

Releases::Releases(const Releases &other)
    : d(other.d)
{
}

Releases::Releases()
    : d(new ReleasesData())
{
}

Releases::Releases(_AsReleases *rels)
    : d(new ReleasesData(rels))
{
}

Releases::~Releases() { }

Releases &Releases::operator=(const Releases &other)
{
    this->d = other.d;
    return *this;
}

_AsReleases *AppStream::Releases::asReleases() const
{
    return d->Releases();
}

QList<Release> Releases::entries() const
{
    QList<Release> res;
    res.reserve(as_releases_len(d->m_rels));
    for (uint i = 0; i < as_releases_len(d->m_rels); i++) {
        Release release(as_releases_index(d->m_rels, i));
        res.append(release);
    }

    return res;
}

uint Releases::size() const
{
    return as_releases_get_size(d->m_rels);
}

bool Releases::isEmpty() const
{
    return as_releases_is_empty(d->m_rels);
}

void Releases::clear()
{
    as_releases_clear(d->m_rels);
}

std::optional<Release> Releases::indexSafe(uint index) const
{
    std::optional<Release> result;
    AsRelease *release = as_releases_index_safe(d->m_rels, index);
    if (release != nullptr)
        result = Release(release);
    return result;
}

void Releases::add(Release &release)
{
    as_releases_add(d->m_rels, release.asRelease());
}

void Releases::sort()
{
    as_releases_sort(d->m_rels);
}

Releases::Kind Releases::kind() const
{
    return static_cast<Releases::Kind>(as_releases_get_kind(d->m_rels));
}

void Releases::setKind(Kind kind)
{
    as_releases_set_kind(d->m_rels, static_cast<AsReleasesKind>(kind));
}

QString Releases::url() const
{
    return QString::fromUtf8(as_releases_get_url(d->m_rels));
}

void Releases::setUrl(const QString &url)
{
    as_releases_set_url(d->m_rels, qPrintable(url));
}
