/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "release-list.h"

#include <QSharedData>
#include <QDebug>

using namespace AppStream;

class AppStream::ReleaseListData : public QSharedData
{
public:
    ReleaseListData()
    {
        m_rels = as_release_list_new();
    }

    ReleaseListData(AsReleaseList *rels)
        : m_rels(rels)
    {
        g_object_ref(m_rels);
    }

    ~ReleaseListData()
    {
        g_object_unref(m_rels);
    }

    bool operator==(const ReleaseListData &rd) const
    {
        return rd.m_rels == m_rels;
    }

    AsReleaseList *ReleaseList() const
    {
        return m_rels;
    }

    AsReleaseList *m_rels;
};

ReleaseList::ReleaseList(const ReleaseList &other)
    : d(other.d)
{
}

ReleaseList::ReleaseList()
    : d(new ReleaseListData())
{
}

ReleaseList::ReleaseList(_AsReleaseList *rels)
    : d(new ReleaseListData(rels))
{
}

ReleaseList::~ReleaseList() { }

ReleaseList &ReleaseList::operator=(const ReleaseList &other)
{
    this->d = other.d;
    return *this;
}

_AsReleaseList *AppStream::ReleaseList::cPtr() const
{
    return d->ReleaseList();
}

QList<Release> ReleaseList::entries() const
{
    QList<Release> res;
    res.reserve(as_release_list_len(d->m_rels));
    for (uint i = 0; i < as_release_list_len(d->m_rels); i++) {
        Release release(as_release_list_index(d->m_rels, i));
        res.append(release);
    }

    return res;
}

uint ReleaseList::size() const
{
    return as_release_list_get_size(d->m_rels);
}

bool ReleaseList::isEmpty() const
{
    return as_release_list_is_empty(d->m_rels);
}

void ReleaseList::clear()
{
    as_release_list_clear(d->m_rels);
}

std::optional<Release> ReleaseList::indexSafe(uint index) const
{
    std::optional<Release> result;
    AsRelease *release = as_release_list_index_safe(d->m_rels, index);
    if (release != nullptr)
        result = Release(release);
    return result;
}

void ReleaseList::add(Release &release)
{
    as_release_list_add(d->m_rels, release.cPtr());
}

void ReleaseList::sort()
{
    as_release_list_sort(d->m_rels);
}

ReleaseList::Kind ReleaseList::kind() const
{
    return static_cast<ReleaseList::Kind>(as_release_list_get_kind(d->m_rels));
}

void ReleaseList::setKind(Kind kind)
{
    as_release_list_set_kind(d->m_rels, static_cast<AsReleaseListKind>(kind));
}

QString ReleaseList::url() const
{
    return QString::fromUtf8(as_release_list_get_url(d->m_rels));
}

void ReleaseList::setUrl(const QString &url)
{
    as_release_list_set_url(d->m_rels, qPrintable(url));
}
