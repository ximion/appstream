/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright (C) 2014  Sune Vuorela <sune@vuorela.dk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef APPSTREAMQT_PROVIDES_H
#define APPSTREAMQT_PROVIDES_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

namespace AppStream {

class ProvidesData;

class APPSTREAMQT_EXPORT Provides {
    Q_GADGET
    public:
        Provides();
        Provides(const Provides& other);
        ~Provides();
        Provides& operator=(const Provides& other);
        bool operator==(const Provides& other) const;

        enum Kind {
            KindUnknown,
            KindLibrary,
            KindBinary,
            KindMimetype,
            KindFont,
            KindModAlias,
            KindPython2Module,
            KindPython3Module,
            KindDBusSystemService,
            KindDBusUserService,
            KindFirmwareRuntime,
            KindFirmwareFlashed
        };
        Q_ENUM(Kind)

        void setKind(Kind kind);
        Kind kind() const;

        static Kind stringToKind(const QString& kind);
        static QString kindToString(Kind kind);

        void setValue(const QString& string);
        QString value() const;

        void setExtraData(const QString& string);
        QString extraData() const;

    private:
        QSharedDataPointer<ProvidesData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Provides& provides);

#endif // APPSTREAMQT_PROVIDES_H
