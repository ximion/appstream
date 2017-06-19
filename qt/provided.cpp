/*
 * Copyright (C) 2014  Sune Vuorela <sune@vuorela.dk>
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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
#include "provided.h"

#include <QSharedData>
#include <QString>
#include <QDebug>
#include <QHash>

#include "chelpers.h"

using namespace AppStream;

class AppStream::ProvidedData : public QSharedData {
public:
    ProvidedData()
    {
        m_prov = as_provided_new();
    }

    ProvidedData(AsProvided *prov)
        : m_prov(prov)
    {
        g_object_ref(m_prov);
    }

    ~ProvidedData()
    {
        g_object_unref(m_prov);
    }

    bool operator==(const ProvidedData& rd) const
    {
        return rd.m_prov == m_prov;
    }

    AsProvided *provided() const
    {
        return m_prov;
    }

    AsProvided *m_prov;
};

QString Provided::kindToString(Provided::Kind kind)
{
    return valueWrap(as_provided_kind_to_string((AsProvidedKind) kind));
}

Provided::Kind Provided::stringToKind(const QString& kindString)
{
    return Provided::Kind(as_provided_kind_from_string(qPrintable(kindString)));
}

Provided::Provided(const Provided& other)
    : d(other.d)
{}

Provided::Provided(_AsProvided *prov)
    : d(new ProvidedData(prov))
{}

Provided::Provided()
    : d(new ProvidedData)
{}

Provided::~Provided()
{}

Provided& Provided::operator=(const Provided& other)
{
    this->d = other.d;
    return *this;
}

bool Provided::operator==(const Provided& other) const
{
    if(d == other.d) {
        return true;
    }
    if(d && other.d) {
        return *d == *other.d;
    }
    return false;
}

_AsProvided * AppStream::Provided::asProvided() const
{
    return d->provided();
}

Provided::Kind Provided::kind() const
{
    return Provided::Kind(as_provided_get_kind(d->m_prov));
}

QStringList Provided::items() const
{
    return valueWrap(as_provided_get_items(d->m_prov));
}

bool Provided::hasItem(const QString& item) const
{
    return as_provided_has_item (d->m_prov, qPrintable(item));
}

bool Provided::isEmpty() const
{
    auto array = as_provided_get_items(d->m_prov);
    if (!array)
        return true;
    return array->len == 0;
}

QDebug operator<<(QDebug s, const AppStream::Provided& Provided) {
    s.nospace() << "AppStream::Provided(" << Provided.kind() << ',' << Provided.items() << "])";
    return s.space();
}
