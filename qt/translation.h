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

#ifndef APPSTREAMQT_TRANSLATION_H
#define APPSTREAMQT_TRANSLATION_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsTranslation;

namespace AppStream {

class TranslationData;

class APPSTREAMQT_EXPORT Translation {
    Q_GADGET
    public:
        enum Kind {
            KindUnknown,
            KindGettext,
            KindQt
        };
        Q_ENUM(Kind)

        Translation();
        Translation(_AsTranslation* category);
        Translation(const Translation& category);
        ~Translation();

        static Kind stringToKind(const QString& kindString);
        static QString kindToString(Kind kind);

        Translation& operator=(const Translation& category);
        bool operator==(const Translation& r) const;

        /**
         * \returns the internally stored AsTranslation
         */
        _AsTranslation *asTranslation() const;

        Kind kind() const;
        void setKind(Kind kind);

        QString id() const;
        void setId(const QString& id);

    private:
        QSharedDataPointer<TranslationData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Translation& category);

#endif // APPSTREAMQT_TRANSLATION_H


