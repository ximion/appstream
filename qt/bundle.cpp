/*
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
#include "bundle.h"

#include <QDateTime>
#include <QDebug>
#include <QUrl>
#include "chelpers.h"

using namespace AppStream;

QString Bundle::kindToString(Bundle::Kind kind)
{
    return as_bundle_kind_to_string((AsBundleKind) kind);
}

Bundle::Kind Bundle::stringToKind(const QString& kindString)
{
    return Bundle::Kind(as_bundle_kind_from_string(qPrintable(kindString)));
}

class AppStream::BundleData : public QSharedData {
public:
    BundleData()
    {
        m_bundle = as_bundle_new();
    }

    BundleData(AsBundle *bundle)
        : m_bundle(bundle)
    {
        g_object_ref(m_bundle);
    }

    ~BundleData()
    {
        g_object_unref(m_bundle);
    }

    bool operator==(const BundleData& rd) const
    {
        return rd.m_bundle == m_bundle;
    }

    AsBundle *bundle() const
    {
        return m_bundle;
    }

    AsBundle* m_bundle;
};

Bundle::Bundle()
    : d(new BundleData())
{}

Bundle::Bundle(_AsBundle* bundle)
    : d(new BundleData(bundle))
{}

Bundle::Bundle(const Bundle &bundle) = default;

Bundle::~Bundle() = default;

Bundle& Bundle::operator=(const Bundle &bundle) = default;

bool Bundle::operator==(const Bundle &other) const
{
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsBundle * AppStream::Bundle::asBundle() const
{
    return d->bundle();
}

Bundle::Kind Bundle::kind() const
{
    return Bundle::Kind(as_bundle_get_kind(d->m_bundle));
}

void Bundle::setKind(Kind kind)
{
    as_bundle_set_kind(d->m_bundle, (AsBundleKind) kind);
}

QString Bundle::id() const
{
    return valueWrap(as_bundle_get_id(d->m_bundle));
}

void Bundle::setId(const QString& id)
{
    as_bundle_set_id(d->m_bundle, qPrintable(id));
}

bool Bundle::isEmpty() const
{
    return as_bundle_get_id(d->m_bundle) == NULL;
}

QDebug operator<<(QDebug s, const AppStream::Bundle& bundle)
{
    s.nospace() << "AppStream::Bundle(" << bundle.id() << ")";
    return s.space();
}
