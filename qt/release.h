/*
 * Copyright (C) 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
 * Copyright (C) 2018 Matthias Klumpp <matthias@tenstral.net>
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
        KindNone,
        KindSha256,
        KindSha1
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

        /**
         * \returns the internally stored AsRelease
         */
        _AsRelease *asRelease() const;

        enum Kind  {
            KindUnknown,
            KindStable,
            KindDevelopment
        };
        Q_ENUM(Kind)

        enum SizeKind {
            SizeUnknown,
            SizeDownload,
            SizeInstalled
        };
        Q_ENUM(SizeKind)

        enum UrgencyKind {
            UrgencyUnknown,
            UrgencyLow,
            UrgencyMedium,
            UrgencyHigh,
            UrgencyCritical
        };
        Q_ENUM(UrgencyKind)

        Kind kind() const;

        QString version() const;

        QDateTime timestamp() const;
        QDateTime timestampEol() const;

        QString description() const;

        QString activeLocale() const;

        UrgencyKind urgency() const;

        // DEPRECATED
        Q_DECL_DEPRECATED QHash<SizeKind, quint64> sizes() const;
	Q_DECL_DEPRECATED QList<QUrl> locations() const;
        Q_DECL_DEPRECATED Checksum checksum() const;

    private:
        QSharedDataPointer<ReleaseData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Release& release);

#endif // APPSTREAMQT_RELEASE_H
