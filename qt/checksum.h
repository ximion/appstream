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

struct _AsChecksum;

namespace AppStream
{

class ChecksumData;

/**
 * A checksum for a file referenced by an \ref Artifact.
 */
class APPSTREAMQT_EXPORT Checksum
{
    Q_GADGET

public:
    enum Kind {
        KindNone,
        KindSha1,
        KindSha256,
        KindSha512,
        KindBlake2b,
        KindBlake3
    };
    Q_ENUM(Kind)

    static Kind stringToKind(const QString &kindString);
    static QString kindToString(Kind kind);

    Checksum();
    Checksum(_AsChecksum *cs);
    Checksum(Kind kind, const QString &value);
    Checksum(const Checksum &other);
    ~Checksum();

    Checksum &operator=(const Checksum &other);
    bool operator==(const Checksum &other) const;

    /**
     * \returns the internally stored AsChecksum
     */
    _AsChecksum *cPtr() const;

    Kind kind() const;
    void setKind(Kind kind);

    QString value() const;
    void setValue(const QString &value);

private:
    QSharedDataPointer<ChecksumData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Checksum &cs);
