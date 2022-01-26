/*
 * Copyright (C) 2017 Jan Grulich <jgrulich@redhat.com>
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
#include "contentrating.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::ContentRatingData : public QSharedData {
public:
    ContentRatingData()
    {
        m_contentRating = as_content_rating_new();
    }

    ContentRatingData(AsContentRating* cat) : m_contentRating(cat)
    {
        g_object_ref(m_contentRating);
    }

    ~ContentRatingData()
    {
        g_object_unref(m_contentRating);
    }

    bool operator==(const ContentRatingData& rd) const
    {
        return rd.m_contentRating == m_contentRating;
    }

    AsContentRating *contentRating() const
    {
        return m_contentRating;
    }

    AsContentRating* m_contentRating;
};

AppStream::ContentRating::RatingValue AppStream::ContentRating::stringToRatingValue(const QString& ratingValue)
{
    return static_cast<ContentRating::RatingValue>(as_content_rating_value_from_string(qPrintable(ratingValue)));
}

QString AppStream::ContentRating::ratingValueToString(AppStream::ContentRating::RatingValue ratingValue)
{
    return QString::fromUtf8(as_content_rating_value_to_string(static_cast<AsContentRatingValue>(ratingValue)));
}

ContentRating::ContentRating()
    : d(new ContentRatingData)
{}

ContentRating::ContentRating(_AsContentRating* contentRating)
    : d(new ContentRatingData(contentRating))
{}

ContentRating::ContentRating(const ContentRating &contentRating) = default;

ContentRating::~ContentRating() = default;

ContentRating& ContentRating::operator=(const ContentRating &contentRating) = default;

bool ContentRating::operator==(const ContentRating &other) const
{
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsContentRating * AppStream::ContentRating::asContentRating() const
{
    return d->contentRating();
}

QString AppStream::ContentRating::kind() const
{
    return valueWrap(as_content_rating_get_kind(d->m_contentRating));
}

void AppStream::ContentRating::setKind(const QString& kind)
{
    as_content_rating_set_kind(d->m_contentRating, qPrintable(kind));
}

uint AppStream::ContentRating::minimumAge() const
{
    return as_content_rating_get_minimum_age(d->m_contentRating);
}

AppStream::ContentRating::RatingValue AppStream::ContentRating::value(const QString& id) const
{
    return static_cast<AppStream::ContentRating::RatingValue>(as_content_rating_get_value(d->m_contentRating, qPrintable(id)));
}

void AppStream::ContentRating::setValue(const QString& id, AppStream::ContentRating::RatingValue ratingValue)
{
    as_content_rating_set_value(d->m_contentRating, qPrintable(id), (AsContentRatingValue) ratingValue);
}

QDebug operator<<(QDebug s, const AppStream::ContentRating& contentRating)
{
    s.nospace() << "AppStream::ContentRating(" << contentRating.kind() << contentRating.minimumAge() << ")";
    return s.space();
}

