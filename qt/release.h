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

#ifndef APPSTREAMQT_RELEASE_H
#define APPSTREAMQT_RELEASE_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include <QCryptographicHash>
#include "appstreamqt_export.h"

struct _AsRelease;

namespace AppStream {

class ReleaseData;

struct Checksum {
    enum ChecksumKind {
        NoneChecksum,
        Sha256Checksum,
        Sha1Checksum,
        LastChecksum
    };
    const ChecksumKind kind;
    const QByteArray data;
};

class APPSTREAMQT_EXPORT Release {
    Q_GADGET
    public:
        Release(_AsRelease* release);
        Release(const Release& release);
        ~Release();

        Release& operator=(const Release& release);
        bool operator==(const Release& r) const;

        enum SizeKind {
            UnknownSize,
            DownloadSize,
            InstalledSize,
            LastSize
        };
        Q_ENUM(SizeKind)

        enum UrgencyKind {
            UnknownUrgency,
            LowUrgency,
            MediumUrgency,
            HighUrgency,
            CriticalUrgency,
            LastUrgency
        };
        Q_ENUM(UrgencyKind)

        QString version() const;

        QDateTime timestamp() const;

        QString description() const;

        QString activeLocale() const;

        QList<QUrl> locations() const;

        Checksum checksum() const;

        QHash<SizeKind, quint64> sizes() const;

        UrgencyKind urgency() const;

    private:
        QSharedDataPointer<ReleaseData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Release& release);

#endif // APPSTREAMQT_RELEASE_H
