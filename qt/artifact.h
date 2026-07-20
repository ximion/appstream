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
#include <QStringList>
#include <QList>
#include <QObject>
#include <optional>
#include "appstreamqt_export.h"
#include "bundle.h"
#include "checksum.h"

struct _AsArtifact;

namespace AppStream
{

class ArtifactData;

/**
 * A downloadable artifact of a \ref Release, e.g. a binary or source tarball.
 */
class APPSTREAMQT_EXPORT Artifact
{
    Q_GADGET

public:
    enum Kind {
        KindUnknown,
        KindSource,
        KindBinary
    };
    Q_ENUM(Kind)

    enum SizeKind {
        SizeKindUnknown,
        SizeKindDownload,
        SizeKindInstalled
    };
    Q_ENUM(SizeKind)

    static Kind stringToKind(const QString &kindString);
    static QString kindToString(Kind kind);

    static SizeKind stringToSizeKind(const QString &sizeKindString);
    static QString sizeKindToString(SizeKind kind);

    Artifact();
    Artifact(_AsArtifact *artifact);
    Artifact(const Artifact &other);
    ~Artifact();

    Artifact &operator=(const Artifact &other);
    bool operator==(const Artifact &other) const;

    /**
     * \returns the internally stored AsArtifact
     */
    _AsArtifact *cPtr() const;

    Kind kind() const;
    void setKind(Kind kind);

    QStringList locations() const;
    void addLocation(const QString &location);

    QList<AppStream::Checksum> checksums() const;
    std::optional<AppStream::Checksum> checksum(Checksum::Kind kind) const;
    void addChecksum(const AppStream::Checksum &cs);

    quint64 size(SizeKind kind) const;
    void setSize(quint64 size, SizeKind kind);

    QString platform() const;
    void setPlatform(const QString &platform);

    Bundle::Kind bundleKind() const;
    void setBundleKind(Bundle::Kind kind);

    QString filename() const;
    void setFilename(const QString &filename);

private:
    QSharedDataPointer<ArtifactData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Artifact &artifact);
