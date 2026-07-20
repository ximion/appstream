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
#include "checksum.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

static_assert(static_cast<int>(Checksum::KindBlake3) + 1 == AS_CHECKSUM_KIND_LAST,
              "Checksum::Kind is out of sync with AsChecksumKind");

class AppStream::ChecksumData : public QSharedData
{
public:
    ChecksumData()
    {
        m_cs = as_checksum_new();
    }

    ChecksumData(AsChecksum *cs)
        : m_cs(cs)
    {
        g_object_ref(m_cs);
    }

    ~ChecksumData()
    {
        g_object_unref(m_cs);
    }

    bool operator==(const ChecksumData &rd) const
    {
        return rd.m_cs == m_cs;
    }

    AsChecksum *m_cs;
};

Checksum::Kind Checksum::stringToKind(const QString &kindString)
{
    return static_cast<Checksum::Kind>(as_checksum_kind_from_string(qPrintable(kindString)));
}

QString Checksum::kindToString(Checksum::Kind kind)
{
    return valueWrap(as_checksum_kind_to_string(static_cast<AsChecksumKind>(kind)));
}

Checksum::Checksum()
    : d(new ChecksumData)
{
}

Checksum::Checksum(_AsChecksum *cs)
    : d(new ChecksumData(cs))
{
}

Checksum::Checksum(Checksum::Kind kind, const QString &value)
    : d(new ChecksumData)
{
    setKind(kind);
    setValue(value);
}

Checksum::Checksum(const Checksum &other) = default;

Checksum::~Checksum() = default;

Checksum &Checksum::operator=(const Checksum &other) = default;

bool Checksum::operator==(const Checksum &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsChecksum *Checksum::cPtr() const
{
    return d->m_cs;
}

Checksum::Kind Checksum::kind() const
{
    return static_cast<Checksum::Kind>(as_checksum_get_kind(d->m_cs));
}

void Checksum::setKind(Checksum::Kind kind)
{
    as_checksum_set_kind(d->m_cs, static_cast<AsChecksumKind>(kind));
}

QString Checksum::value() const
{
    return valueWrap(as_checksum_get_value(d->m_cs));
}

void Checksum::setValue(const QString &value)
{
    as_checksum_set_value(d->m_cs, qPrintable(value));
}

QDebug operator<<(QDebug s, const AppStream::Checksum &cs)
{
    s.nospace() << "AppStream::Checksum(" << Checksum::kindToString(cs.kind()) << ":" << cs.value()
                << ")";
    return s.space();
}
