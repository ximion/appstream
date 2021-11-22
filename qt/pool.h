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
#include "metadata.h"

namespace AppStream {

/**
 * Access the AppStream metadata pool.
 *
 * See http://www.freedesktop.org/wiki/Distributions/AppStream/ for details
 */
class PoolPrivate;
class APPSTREAMQT_EXPORT Pool : public QObject
{
    Q_OBJECT

public:
    Pool(QObject *parent = nullptr);
    ~Pool();

    /**
     * Pool::Flags:
     * FlagNone:             No flags.
     * FlagReadCollection:   Add AppStream collection metadata to the pool.
     * FlagReadMetainfo:     Add data from AppStream metainfo files to the pool.
     * FlagReadDesktopFiles: Add metadata from desktop-entry files to the pool.
     * FlagLoadFlatpak:      Add AppStream metadata from Flatpak to the pool.
     * FlagIgnoreCacheAge:   Ignore cache age and always load data from scratch.
     * FlagResolveAddons:    Always resolve addons for returned components
     * FlagPreferOsMetainfo: Prefer local metainfo data over the system-provided collection data. Useful for debugging.
     * FlagMonitor:          Monitor registered directories for changes, and auto-reload metadata if necessary.
     *
     * Flags controlling the metadata pool behavior.
     **/
    enum Flags {
        FlagNone = 0,
        FlagLoadOsCollection   = 1 << 0,
        FlagLoadOsMetainfo     = 1 << 1,
        FlagLoadOsDesktopFiles = 1 << 2,
        FlagLoadFlatpak        = 1 << 3,
        FlagIgnoreCacheAge     = 1 << 4,
        FlagResolveAddons      = 1 << 5,
        FlagPreferOsMetainfo   = 1 << 6,
        FlagMonitor            = 1 << 7,

        // deprecated
        FlagReadCollection   [[deprecated]] = FlagLoadOsCollection,
        FlagReadMetainfo     [[deprecated]] = FlagLoadOsMetainfo,
        FlagReadDesktopFiles [[deprecated]] = FlagLoadOsDesktopFiles,
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
        CacheFlagNone      [[deprecated]] = 0,
        CacheFlagUseUser   [[deprecated]] = 1 << 0,
        CacheFlagUseSystem [[deprecated]] = 1 << 1,
    };

    /**
     * \return true on success. False on failure
     *
     * Loads all available metadata and opens the cache. In case of an error,
     * \sa lastError() will contain the error message.
     */
    bool load();

    /**
     * \return true on success. False on failure
     *
     * In case of failure, @p error will be initialized with the error message
     */
    Q_DECL_DEPRECATED bool load(QString* error);

    /**
     * Remove all software component information from the pool.
     */
    void clear();

    /**
     * \return The last error message received.
     */
    QString lastError() const;

    /**
     * Add a component to the pool.
     */
    bool addComponents(const QList<AppStream::Component>& cpts);

    QList<AppStream::Component> components() const;

    QList<AppStream::Component> componentsById(const QString& cid) const;

    QList<AppStream::Component> componentsByProvided(Provided::Kind kind, const QString& item) const;

    QList<AppStream::Component> componentsByKind(Component::Kind kind) const;

    QList<AppStream::Component> componentsByCategories(const QStringList& categories) const;

    QList<AppStream::Component> componentsByLaunchable(Launchable::Kind kind, const QString& value) const;

    QList<AppStream::Component> componentsByExtends(const QString& extendedId) const;

    QList<AppStream::Component> search(const QString& term) const;


    void setLocale(const QString& locale);

    uint flags() const;
    void setFlags(uint flags);
    void addFlags(uint flags);
    void removeFlags(uint flags);

    void resetExtraDataLocations();
    void addExtraDataLocation(const QString &directory, Metadata::FormatStyle formatStyle);

    void setLoadStdDataLocations(bool enabled);
    void overrideCacheLocations(const QString &sysDir,
                                const QString &userDir);

    Q_DECL_DEPRECATED bool addComponent(const AppStream::Component& cpt);

    Q_DECL_DEPRECATED uint cacheFlags() const;
    Q_DECL_DEPRECATED void setCacheFlags(uint flags);

    Q_DECL_DEPRECATED void setCacheLocation(const QString &path);
    Q_DECL_DEPRECATED QString cacheLocation() const;

    Q_DECL_DEPRECATED void clearMetadataLocations();
    Q_DECL_DEPRECATED void addMetadataLocation(const QString& directory);

Q_SIGNALS:
    void changed();

private:
    Q_DISABLE_COPY(Pool);
    QScopedPointer<PoolPrivate> d;
};

}

#endif // APPSTREAMQT_POOL_H
