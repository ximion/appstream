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
#include "icon.h"

#include <QSharedData>
#include <QSize>
#include <QUrl>
#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::IconData : public QSharedData {
public:
    IconData()
    {
        m_icon = as_icon_new();
    }

    IconData(AsIcon *icon)
        : m_icon(icon)
    {
        g_object_ref(m_icon);
    }

    ~IconData()
    {
        g_object_unref(m_icon);
    }

    bool operator==(const IconData& rd) const
    {
        return rd.m_icon == m_icon;
    }

    AsIcon *icon() const {
        return m_icon;
    }

    AsIcon *m_icon;
};

Icon::Icon(const Icon& other)
    : d(other.d)
{}

Icon::Icon()
    : d(new IconData)
{}

Icon::Icon(_AsIcon *icon)
    : d(new IconData(icon))
{}

Icon::~Icon()
{}

Icon& Icon::operator=(const Icon& other)
{
    this->d = other.d;
    return *this;
}

AsIcon * AppStream::Icon::asIcon() const
{
    return d->icon();
}

Icon::Kind Icon::kind() const
{
    return Icon::Kind(as_icon_get_kind(d->m_icon));
}

void Icon::setKind(Icon::Kind kind)
{
    as_icon_set_kind(d->m_icon, (AsIconKind) kind);
}

uint Icon::height() const
{
    return as_icon_get_height(d->m_icon);
}

void Icon::setHeight(uint height) {
    as_icon_set_height(d->m_icon, height);
}

uint Icon::width() const
{
    return as_icon_get_width(d->m_icon);
}

void Icon::setWidth(uint width)
{
    as_icon_set_width(d->m_icon, width);
}

void Icon::setUrl(const QUrl& url)
{
    if (url.isLocalFile())
        as_icon_set_filename(d->m_icon, qPrintable(url.toString()));
    else
        as_icon_set_url(d->m_icon, qPrintable(url.toString()));
}

const QUrl Icon::url() const {
    if (as_icon_get_kind(d->m_icon) == AS_ICON_KIND_REMOTE)
        return QUrl(as_icon_get_url(d->m_icon));
    else
        return QUrl::fromLocalFile(as_icon_get_filename(d->m_icon));
}

const QString Icon::name() const
{
    return valueWrap(as_icon_get_name(d->m_icon));
}

void Icon::setName(const QString& name)
{
    as_icon_set_name(d->m_icon, qPrintable(name));
}

bool Icon::isEmpty() const
{
    return url().isEmpty() && as_icon_get_name(d->m_icon) == NULL;
}

QSize AppStream::Icon::size() const
{
    return QSize(width(), height());
}

QDebug operator<<(QDebug s, const AppStream::Icon& image) {
    s.nospace() << "AppStream::Icon(" << image.kind();
    if (!image.url().isEmpty())
        s.nospace() << ',' << image.url();
    if (!image.name().isEmpty())
        s.nospace() << ',' << image.name();
    s.nospace() << "[" << image.width() << "x" << image.height() << "])";
    return s;
}
