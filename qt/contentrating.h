
/*
 * Copyright (C) 2017 Jan Grulich <jgrulich@redhat.com>
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

#ifndef APPSTREAMQT_CONTENT_RATING_H
#define APPSTREAMQT_CONTENT_RATING_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsContentRating;

namespace AppStream
{

class ContentRatingData;

class APPSTREAMQT_EXPORT ContentRating
{
    Q_GADGET

public:
    enum RatingValue {
        RatingValueUnknown,
        RatingValueNone,
        RatingValueMild,
        RatingValueModerate,
        RatingValueIntense
    };
    Q_ENUM(RatingValue)

    enum System {
        SystemUnknown,
        SystemIncaa,
        SystemAcb,
        SystemDjctq,
        SystemGsrr,
        SystemPegi,
        SystemKavi,
        SystemUsk,
        SystemEsra,
        SystemCero,
        SystemOflcnz,
        SystemRussia,
        SystemMda,
        SystemGrac,
        SystemEsrb,
        SystemIarc
    };
    Q_ENUM(System)

    ContentRating();
    ContentRating(_AsContentRating *category);
    ContentRating(const ContentRating &category);
    ~ContentRating();

    static RatingValue stringToRatingValue(const QString &ratingValue);
    static QString ratingValueToString(RatingValue ratingValue);

    static QString systemToString(System system);
    static System systemFromLocale(const QString &locale);
    static QString systemFormatAge(System system, uint age);
    static QStringList systemGetFormattedAges(System system);

    static QStringList allRatingIds();
    static uint attributeToCsmAge(const QString &id, RatingValue value);
    static RatingValue attributeFromCsmAge(const QString &id, uint age);

    ContentRating &operator=(const ContentRating &category);
    bool operator==(const ContentRating &r) const;

    /**
     * \returns the internally stored AsContentRating
     */
    _AsContentRating *cPtr() const;

    QString kind() const;
    void setKind(const QString &kind);

    uint minimumAge() const;

    RatingValue value(const QString &id) const;
    void setValue(const QString &id, RatingValue ratingValue);

    QStringList ratingIds() const;
    QString description(const QString &id) const;

    void addAttribute(const QString &id, RatingValue value);

private:
    QSharedDataPointer<ContentRatingData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::ContentRating &category);

#endif // APPSTREAMQT_CONTENT_RATING_H
