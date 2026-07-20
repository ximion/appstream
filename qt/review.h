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
#include <QString>
#include <QDateTime>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsReview;

namespace AppStream
{

class ReviewData;

/**
 * A user review of a \ref Component.
 */
class APPSTREAMQT_EXPORT Review
{
    Q_GADGET

public:
    enum Flag {
        FlagNone = 0,
        FlagSelf = 1 << 0,
        FlagVoted = 1 << 1,
    };
    Q_FLAG(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    Review();
    Review(_AsReview *review);
    Review(const Review &other);
    ~Review();

    Review &operator=(const Review &other);
    bool operator==(const Review &other) const;

    /**
     * \returns the internally stored AsReview
     */
    _AsReview *cPtr() const;

    int priority() const;
    void setPriority(int priority);

    QString id() const;
    void setId(const QString &id);

    QString summary() const;
    void setSummary(const QString &summary);

    QString description() const;
    void setDescription(const QString &description);

    QString locale() const;
    void setLocale(const QString &locale);

    int rating() const;
    void setRating(int rating);

    QString version() const;
    void setVersion(const QString &version);

    QString reviewerId() const;
    void setReviewerId(const QString &reviewerId);

    QString reviewerName() const;
    void setReviewerName(const QString &reviewerName);

    QDateTime date() const;
    void setDate(const QDateTime &date);

    Flags flags() const;
    void setFlags(Flags flags);
    void addFlags(Flags flags);

    QString metadataItem(const QString &key) const;
    void addMetadata(const QString &key, const QString &value);

private:
    QSharedDataPointer<ReviewData> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Review::Flags)
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Review &review);
