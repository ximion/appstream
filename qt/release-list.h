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

#pragma once

#include <QSharedDataPointer>
#include <QObject>
#include <optional>
#include "appstreamqt_export.h"

#include "release.h"

class QString;
struct _AsReleaseList;
namespace AppStream
{

class ReleaseListData;

/**
 * Container for component ReleaseList and their metadata.
 */
class APPSTREAMQT_EXPORT ReleaseList
{
    Q_GADGET

public:
    ReleaseList();
    ReleaseList(_AsReleaseList *cbox);
    ReleaseList(const ReleaseList &other);
    ~ReleaseList();

    ReleaseList &operator=(const ReleaseList &other);

    /**
     * \returns the internally stored AsReleaseList
     */
    _AsReleaseList *cPtr() const;

    enum Kind {
        KindUnknown,
        KindEmbedded,
        KindExternal
    };
    Q_ENUM(Kind)

    /**
     * \returns release entries as list.
     */
    QList<Release> entries() const;

    uint size() const;
    bool isEmpty() const;
    void clear();

    std::optional<Release> indexSafe(uint index) const;
    void add(Release &release);

    void sort();

    Kind kind() const;
    void setKind(Kind kind);

    QString url() const;
    void setUrl(const QString &url);

private:
    QSharedDataPointer<ReleaseListData> d;
};
}
