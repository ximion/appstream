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

#ifndef APPSTREAMQT_RELEASE_H
#define APPSTREAMQT_RELEASE_H

#include <QSharedDataPointer>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QObject>
#include "appstreamqt_export.h"
#include "artifact.h"

struct _AsRelease;

namespace AppStream
{

class ReleaseData;

class APPSTREAMQT_EXPORT Release
{
    Q_GADGET
public:
    Release();
    Release(_AsRelease *release);
    Release(const Release &release);
    ~Release();

    Release &operator=(const Release &release);
    bool operator==(const Release &r) const;

    /**
     * \returns the internally stored AsRelease
     */
    _AsRelease *cPtr() const;

    enum Kind {
        KindUnknown,
        KindStable,
        KindDevelopment,
        KindSnapshot
    };
    Q_ENUM(Kind)

    enum UrgencyKind {
        UrgencyUnknown,
        UrgencyLow,
        UrgencyMedium,
        UrgencyHigh,
        UrgencyCritical
    };
    Q_ENUM(UrgencyKind)

    enum UrlKind {
        UrlKindUnknown,
        UrlKindDetails
    };
    Q_ENUM(UrlKind)

    static Kind stringToKind(const QString &kindString);
    static QString kindToString(Kind kind);

    static UrgencyKind stringToUrgencyKind(const QString &urgencyString);
    static QString urgencyKindToString(UrgencyKind urgency);

    static UrlKind stringToUrlKind(const QString &kindString);
    static QString urlKindToString(UrlKind kind);

    Kind kind() const;
    void setKind(Kind kind);

    QString version() const;
    void setVersion(const QString &version);

    QDateTime timestamp() const;
    void setTimestamp(const QDateTime &timestamp);

    QDateTime timestampEol() const;
    void setTimestampEol(const QDateTime &timestamp);

    QString description() const;
    void setDescription(const QString &description, const QString &lang = {});

    UrgencyKind urgency() const;
    void setUrgency(UrgencyKind urgency);

    QList<AppStream::Artifact> artifacts() const;
    void addArtifact(const AppStream::Artifact &artifact);

    QString url(UrlKind kind) const;
    void setUrl(UrlKind kind, const QString &url);

    /**
     * \return -1, 0 or 1 if this release is older, equal to, or newer than \a other
     */
    int vercmp(const Release &other) const;

    bool hasTag(const QString &ns, const QString &tag) const;
    bool addTag(const QString &ns, const QString &tag);
    bool removeTag(const QString &ns, const QString &tag);
    void clearTags();

private:
    QSharedDataPointer<ReleaseData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Release &release);

#endif // APPSTREAMQT_RELEASE_H
