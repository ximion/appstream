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

#ifndef APPSTREAMQT_POOL_H
#define APPSTREAMQT_POOL_H

#include "appstreamqt_export.h"

#include <QString>
#include <QList>
#include <QStringList>
#include "component.h"

namespace AppStream {

/**
 * Access the AppStream metadata pool.
 *
 * See http://www.freedesktop.org/wiki/Distributions/AppStream/ for details
 */
class PoolPrivate;
class APPSTREAMQT_EXPORT Pool : QObject{
Q_OBJECT
    public:
        Pool(QObject *parent = nullptr);
        ~Pool();

        /**
         * Pool::Flags:
         * FlagNone:             No flags.
         * FlagReadCollection:   Add AppStream collection metadata to the pool.
         * FlagReadMetainfo:     Add data from AppStream metainfo files to the pool.
         * FlagReadDesktopFiles: Add metadata from .desktop files to the pool.
         *
         * Flags on how caching should be used.
         **/
        enum Flags {
            FlagNone = 0,
            FlagReadCollection   = 1 << 0,
            FlagReadMetainfo     = 1 << 1,
            FlagReadDesktopFiles = 1 << 2,
        };

        /**
         * Pool::CacheFlags:
         * None:      No flags.
         * UseUser:   Create an user-specific metadata cache.
         * UseSystem: Use and - if possible - update the global metadata cache.
         *
         * Flags on how caching should be used.
         **/
        enum CacheFlags {
            CacheFlagNone      = 0,
            CacheFlagUseUser   = 1 << 0,
            CacheFlagUseSystem = 1 << 1,
        };

        /**
         * \return true on success. False on failure
         */
        bool load();

        /**
         * \return true on success. False on failure
         *
         * In case of failure, @p error will be initialized with the error message
         */
        bool load(QString* error);

        /**
         * Remove all software component information from the pool.
         */
        void clear();

        /**
         * Add a component to the pool.
         */
        bool addComponent(const AppStream::Component& cpt);

        QList<AppStream::Component> components() const;

        QList<AppStream::Component> componentsById(const QString& cid) const;

        QList<AppStream::Component> componentsByProvided(Provided::Kind kind, const QString& item) const;

        QList<AppStream::Component> componentsByKind(Component::Kind kind) const;

        QList<AppStream::Component> componentsByCategories(const QStringList categories) const;

        QList<AppStream::Component> componentsByLaunchable(Launchable::Kind kind, const QString& value) const;

        QList<AppStream::Component> search(const QString& term) const;

        void clearMetadataLocations();
        void addMetadataLocation(const QString& directory);

        void setLocale(const QString& locale);

        uint flags() const;
        void setFlags(uint flags);

        uint cacheFlags() const;
        void setCacheFlags(uint flags);

    private:
        Q_DISABLE_COPY(Pool);
        QScopedPointer<PoolPrivate> d;
};
}

#endif // APPSTREAMQT_POOL_H
