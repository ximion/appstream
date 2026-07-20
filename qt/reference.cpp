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
#include "reference.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

static_assert(static_cast<int>(Reference::KindRegistry) + 1 == AS_REFERENCE_KIND_LAST,
              "Reference::Kind is out of sync with AsReferenceKind");

class AppStream::ReferenceData : public QSharedData
{
public:
    ReferenceData()
    {
        m_reference = as_reference_new();
    }

    ReferenceData(AsReference *reference)
        : m_reference(reference)
    {
        g_object_ref(m_reference);
    }

    ~ReferenceData()
    {
        g_object_unref(m_reference);
    }

    bool operator==(const ReferenceData &rd) const
    {
        return rd.m_reference == m_reference;
    }

    AsReference *m_reference;
};

Reference::Kind Reference::stringToKind(const QString &kindString)
{
    return static_cast<Reference::Kind>(as_reference_kind_from_string(qPrintable(kindString)));
}

QString Reference::kindToString(Reference::Kind kind)
{
    return valueWrap(as_reference_kind_to_string(static_cast<AsReferenceKind>(kind)));
}

Reference::Reference()
    : d(new ReferenceData)
{
}

Reference::Reference(_AsReference *reference)
    : d(new ReferenceData(reference))
{
}

Reference::Reference(const Reference &other) = default;

Reference::~Reference() = default;

Reference &Reference::operator=(const Reference &other) = default;

bool Reference::operator==(const Reference &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsReference *Reference::cPtr() const
{
    return d->m_reference;
}

Reference::Kind Reference::kind() const
{
    return static_cast<Reference::Kind>(as_reference_get_kind(d->m_reference));
}

void Reference::setKind(Reference::Kind kind)
{
    as_reference_set_kind(d->m_reference, static_cast<AsReferenceKind>(kind));
}

QString Reference::value() const
{
    return valueWrap(as_reference_get_value(d->m_reference));
}

void Reference::setValue(const QString &value)
{
    as_reference_set_value(d->m_reference, qPrintable(value));
}

QString Reference::registryName() const
{
    return valueWrap(as_reference_get_registry_name(d->m_reference));
}

void Reference::setRegistryName(const QString &name)
{
    as_reference_set_registry_name(d->m_reference, qPrintable(name));
}

QDebug operator<<(QDebug s, const AppStream::Reference &reference)
{
    s.nospace() << "AppStream::Reference(" << Reference::kindToString(reference.kind()) << ":"
                << reference.value() << ")";
    return s.space();
}
