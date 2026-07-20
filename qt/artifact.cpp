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

#include "appstream.h"
#include "artifact.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

static_assert(static_cast<int>(Artifact::KindBinary) + 1 == AS_ARTIFACT_KIND_LAST,
              "Artifact::Kind is out of sync with AsArtifactKind");
static_assert(static_cast<int>(Artifact::SizeKindInstalled) + 1 == AS_SIZE_KIND_LAST,
              "Artifact::SizeKind is out of sync with AsSizeKind");

class AppStream::ArtifactData : public QSharedData
{
public:
    ArtifactData()
    {
        m_artifact = as_artifact_new();
    }

    ArtifactData(AsArtifact *artifact)
        : m_artifact(artifact)
    {
        g_object_ref(m_artifact);
    }

    ~ArtifactData()
    {
        g_object_unref(m_artifact);
    }

    bool operator==(const ArtifactData &rd) const
    {
        return rd.m_artifact == m_artifact;
    }

    AsArtifact *m_artifact;
};

Artifact::Kind Artifact::stringToKind(const QString &kindString)
{
    return static_cast<Artifact::Kind>(as_artifact_kind_from_string(qPrintable(kindString)));
}

QString Artifact::kindToString(Artifact::Kind kind)
{
    return valueWrap(as_artifact_kind_to_string(static_cast<AsArtifactKind>(kind)));
}

Artifact::SizeKind Artifact::stringToSizeKind(const QString &sizeKindString)
{
    return static_cast<Artifact::SizeKind>(as_size_kind_from_string(qPrintable(sizeKindString)));
}

QString Artifact::sizeKindToString(Artifact::SizeKind kind)
{
    return valueWrap(as_size_kind_to_string(static_cast<AsSizeKind>(kind)));
}

Artifact::Artifact()
    : d(new ArtifactData)
{
}

Artifact::Artifact(_AsArtifact *artifact)
    : d(new ArtifactData(artifact))
{
}

Artifact::Artifact(const Artifact &other) = default;

Artifact::~Artifact() = default;

Artifact &Artifact::operator=(const Artifact &other) = default;

bool Artifact::operator==(const Artifact &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsArtifact *Artifact::cPtr() const
{
    return d->m_artifact;
}

Artifact::Kind Artifact::kind() const
{
    return static_cast<Artifact::Kind>(as_artifact_get_kind(d->m_artifact));
}

void Artifact::setKind(Artifact::Kind kind)
{
    as_artifact_set_kind(d->m_artifact, static_cast<AsArtifactKind>(kind));
}

QStringList Artifact::locations() const
{
    return valueWrap(as_artifact_get_locations(d->m_artifact));
}

void Artifact::addLocation(const QString &location)
{
    as_artifact_add_location(d->m_artifact, qPrintable(location));
}

QList<Checksum> Artifact::checksums() const
{
    QList<Checksum> res;

    auto checksums = as_artifact_get_checksums(d->m_artifact);
    res.reserve(checksums->len);
    for (uint i = 0; i < checksums->len; i++) {
        auto cs = AS_CHECKSUM(g_ptr_array_index(checksums, i));
        res.append(Checksum(cs));
    }
    return res;
}

std::optional<Checksum> Artifact::checksum(Checksum::Kind kind) const
{
    auto cs = as_artifact_get_checksum(d->m_artifact, static_cast<AsChecksumKind>(kind));
    if (cs == nullptr)
        return std::nullopt;
    return Checksum(cs);
}

void Artifact::addChecksum(const AppStream::Checksum &cs)
{
    as_artifact_add_checksum(d->m_artifact, cs.cPtr());
}

quint64 Artifact::size(Artifact::SizeKind kind) const
{
    return as_artifact_get_size(d->m_artifact, static_cast<AsSizeKind>(kind));
}

void Artifact::setSize(quint64 size, Artifact::SizeKind kind)
{
    as_artifact_set_size(d->m_artifact, size, static_cast<AsSizeKind>(kind));
}

QString Artifact::platform() const
{
    return valueWrap(as_artifact_get_platform(d->m_artifact));
}

void Artifact::setPlatform(const QString &platform)
{
    as_artifact_set_platform(d->m_artifact, qPrintable(platform));
}

Bundle::Kind Artifact::bundleKind() const
{
    return static_cast<Bundle::Kind>(as_artifact_get_bundle_kind(d->m_artifact));
}

void Artifact::setBundleKind(Bundle::Kind kind)
{
    as_artifact_set_bundle_kind(d->m_artifact, static_cast<AsBundleKind>(kind));
}

QString Artifact::filename() const
{
    return valueWrap(as_artifact_get_filename(d->m_artifact));
}

void Artifact::setFilename(const QString &filename)
{
    as_artifact_set_filename(d->m_artifact, qPrintable(filename));
}

QDebug operator<<(QDebug s, const AppStream::Artifact &artifact)
{
    s.nospace() << "AppStream::Artifact(" << Artifact::kindToString(artifact.kind()) << ":"
                << artifact.locations() << ")";
    return s.space();
}
