/*
 * Copyright (C) 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
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
#include "suggested.h"

#include <QSharedData>
#include <QSize>
#include <QUrl>
#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::SuggestedData : public QSharedData {
public:
    SuggestedData()
    {
        m_suggested = as_suggested_new();
    }

    SuggestedData(AsSuggested *suggested)
        : m_suggested(suggested)
    {
        g_object_ref(m_suggested);
    }

    ~SuggestedData()
    {
        g_object_unref(m_suggested);
    }

    bool operator==(const SuggestedData& rd) const
    {
        return rd.m_suggested == m_suggested;
    }

    AsSuggested *suggested() const
    {
        return m_suggested;
    }

    AsSuggested *m_suggested;
};

Suggested::Suggested(const Suggested& other)
    : d(other.d)
{}

Suggested::Suggested()
    : d(new SuggestedData)
{}

Suggested::Suggested(_AsSuggested *suggested)
    : d(new SuggestedData(suggested))
{}

Suggested::~Suggested()
{}

Suggested& Suggested::operator=(const Suggested& other)
{
    this->d = other.d;
    return *this;
}

_AsSuggested * AppStream::Suggested::suggested() const
{
    return d->suggested();
}

Suggested::Kind Suggested::kind() const
{
    return Suggested::Kind(as_suggested_get_kind(d->m_suggested));
}

void Suggested::setKind(Suggested::Kind kind)
{
    as_suggested_set_kind(d->m_suggested, (AsSuggestedKind) kind);
}

const QStringList AppStream::Suggested::ids() const
{
    return valueWrap(as_suggested_get_ids(d->m_suggested));
}

void AppStream::Suggested::addSuggested(const QString& id)
{
    as_suggested_add_id(d->m_suggested, qPrintable(id));
}

QDebug operator<<(QDebug s, const AppStream::Suggested& suggested)
{
    s.nospace() << "AppStream::Suggested(" << suggested.ids() << ")";
    return s.space();
}
