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

#ifndef APPSTREAMQT_BUNDLE_H
#define APPSTREAMQT_BUNDLE_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsBundle;
namespace AppStream {

class BundleData;
class APPSTREAMQT_EXPORT Bundle {
    Q_GADGET
    public:
        Bundle();
        Bundle(_AsBundle *bundle);
        Bundle(const Bundle& bundle);
        ~Bundle();

        Bundle& operator=(const Bundle& bundle);
        bool operator==(const Bundle& r) const;

        /**
         * \returns the internally stored AsBundle
         */
        _AsBundle *asBundle() const;

        enum Kind {
            KindUnknown,
            KindPackage,
            KindLimba,
            KindFlatpak,
            KindAppImage,
            KindSnap,
            KindTarball,
            KindCabinet
        };
        Q_ENUM(Kind)

        static Kind stringToKind(const QString& kindString);
        static QString kindToString(Kind kind);

        /**
         * \return the bundle kind.
         */
        Kind kind() const;
        void setKind(Kind kind);

        /**
         * \return the bundle ID.
         */
        QString id() const;
        void setId(const QString& id);

        bool isEmpty() const;

    private:
        QSharedDataPointer<BundleData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Bundle& bundle);

#endif // APPSTREAMQT_BUNDLE_H
