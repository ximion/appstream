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

#ifndef APPSTREAMQT_LAUNCHABLE_H
#define APPSTREAMQT_LAUNCHABLE_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsLaunchable;

namespace AppStream {

class LaunchableData;

class APPSTREAMQT_EXPORT Launchable {
    Q_GADGET
    public:
        enum Kind {
            KindUnknown,
            KindDesktopId,
            KindService,
            KindCockpitManifest
        };
        Q_ENUM(Kind)

        Launchable();
        Launchable(_AsLaunchable* launchable);
        Launchable(const Launchable& launchable);
        ~Launchable();

        static Kind stringToKind(const QString& kindString);
        static QString kindToString(Kind kind);

        Launchable& operator=(const Launchable& launchable);
        bool operator==(const Launchable& r) const;

        /**
         * \returns the internally stored AsLaunchable
         */
        _AsLaunchable *asLaunchable() const;

        Kind kind() const;
        void setKind(Kind kind);

        QStringList entries() const;
        void addEntry(const QString& entry);

    private:
        QSharedDataPointer<LaunchableData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Launchable& launchable);

#endif // APPSTREAMQT_LAUNCHABLE_H

