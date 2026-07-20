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
#include "review.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

static_assert(static_cast<int>(Review::FlagVoted) == AS_REVIEW_FLAG_VOTED,
              "Review::Flag is out of sync with AsReviewFlags");

class AppStream::ReviewData : public QSharedData
{
public:
    ReviewData()
    {
        m_review = as_review_new();
    }

    ReviewData(AsReview *review)
        : m_review(review)
    {
        g_object_ref(m_review);
    }

    ~ReviewData()
    {
        g_object_unref(m_review);
    }

    bool operator==(const ReviewData &rd) const
    {
        return as_review_equal(rd.m_review, m_review);
    }

    AsReview *m_review;
};

Review::Review()
    : d(new ReviewData)
{
}

Review::Review(_AsReview *review)
    : d(new ReviewData(review))
{
}

Review::Review(const Review &other) = default;

Review::~Review() = default;

Review &Review::operator=(const Review &other) = default;

bool Review::operator==(const Review &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsReview *Review::cPtr() const
{
    return d->m_review;
}

int Review::priority() const
{
    return as_review_get_priority(d->m_review);
}

void Review::setPriority(int priority)
{
    as_review_set_priority(d->m_review, priority);
}

QString Review::id() const
{
    return valueWrap(as_review_get_id(d->m_review));
}

void Review::setId(const QString &id)
{
    as_review_set_id(d->m_review, qPrintable(id));
}

QString Review::summary() const
{
    return valueWrap(as_review_get_summary(d->m_review));
}

void Review::setSummary(const QString &summary)
{
    as_review_set_summary(d->m_review, qPrintable(summary));
}

QString Review::description() const
{
    return valueWrap(as_review_get_description(d->m_review));
}

void Review::setDescription(const QString &description)
{
    as_review_set_description(d->m_review, qPrintable(description));
}

QString Review::locale() const
{
    return valueWrap(as_review_get_locale(d->m_review));
}

void Review::setLocale(const QString &locale)
{
    as_review_set_locale(d->m_review, qPrintable(locale));
}

int Review::rating() const
{
    return as_review_get_rating(d->m_review);
}

void Review::setRating(int rating)
{
    as_review_set_rating(d->m_review, rating);
}

QString Review::version() const
{
    return valueWrap(as_review_get_version(d->m_review));
}

void Review::setVersion(const QString &version)
{
    as_review_set_version(d->m_review, qPrintable(version));
}

QString Review::reviewerId() const
{
    return valueWrap(as_review_get_reviewer_id(d->m_review));
}

void Review::setReviewerId(const QString &reviewerId)
{
    as_review_set_reviewer_id(d->m_review, qPrintable(reviewerId));
}

QString Review::reviewerName() const
{
    return valueWrap(as_review_get_reviewer_name(d->m_review));
}

void Review::setReviewerName(const QString &reviewerName)
{
    as_review_set_reviewer_name(d->m_review, qPrintable(reviewerName));
}

QDateTime Review::date() const
{
    GDateTime *dt = as_review_get_date(d->m_review);
    if (dt == nullptr)
        return QDateTime();
    return QDateTime::fromSecsSinceEpoch(g_date_time_to_unix(dt));
}

void Review::setDate(const QDateTime &date)
{
    if (!date.isValid()) {
        as_review_set_date(d->m_review, nullptr);
        return;
    }

    g_autoptr(GDateTime) dt = g_date_time_new_from_unix_utc(date.toSecsSinceEpoch());
    as_review_set_date(d->m_review, dt);
}

Review::Flags Review::flags() const
{
    return static_cast<Review::Flags>(static_cast<int>(as_review_get_flags(d->m_review)));
}

void Review::setFlags(Review::Flags flags)
{
    as_review_set_flags(d->m_review, static_cast<AsReviewFlags>(static_cast<int>(flags)));
}

void Review::addFlags(Review::Flags flags)
{
    as_review_add_flags(d->m_review, static_cast<AsReviewFlags>(static_cast<int>(flags)));
}

QString Review::metadataItem(const QString &key) const
{
    return valueWrap(as_review_get_metadata_item(d->m_review, qPrintable(key)));
}

void Review::addMetadata(const QString &key, const QString &value)
{
    as_review_add_metadata(d->m_review, qPrintable(key), qPrintable(value));
}

QDebug operator<<(QDebug s, const AppStream::Review &review)
{
    s.nospace() << "AppStream::Review(" << review.id() << ":" << review.summary() << ")";
    return s.space();
}
