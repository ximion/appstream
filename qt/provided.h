/*
 * Copyright (C) 2014 Sune Vuorela <sune@vuorela.dk>
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

#ifndef APPSTREAMQT_PROVIDED_H
#define APPSTREAMQT_PROVIDED_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsProvided;

namespace AppStream {

class ProvidedData;

class APPSTREAMQT_EXPORT Provided {
    Q_GADGET
    public:
        Provided();
        Provided(_AsProvided *prov);
        Provided(const Provided& other);
        ~Provided();
        Provided& operator=(const Provided& other);
        bool operator==(const Provided& other) const;

        /**
         * \returns the internally stored AsProvided
         */
        _AsProvided *asProvided() const;

        enum Kind {
            KindUnknown,
            KindLibrary,
            KindBinary,
            KindMimetype,
            KindFont,
            KindModalias,
            KindPython2Module,
            KindPython3Module,
            KindDBusSystemService,
            KindDBusUserService,
            KindFirmwareRuntime,
            KindFirmwareFlashed,
            KindId,
        };
        Q_ENUM(Kind)

        static Kind stringToKind(const QString& kind);
        static QString kindToString(Kind kind);

        Kind kind() const;

        QStringList items() const;
        bool hasItem(const QString &item) const;

        bool isEmpty() const;

    private:
        QSharedDataPointer<ProvidedData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Provided& provides);

#endif // APPSTREAMQT_PROVIDED_H
