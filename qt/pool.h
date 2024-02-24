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

#ifndef APPSTREAMQT_POOL_H
#define APPSTREAMQT_POOL_H

#include "appstreamqt_export.h"

#include <QString>
#include <QList>
#include <QStringList>
#include "component-box.h"
#include "metadata.h"

struct _AsPool;
namespace AppStream
{

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
     * \returns the internally stored AsPool
     */
    _AsPool *cPtr() const;

    /**
     * Pool::Flag:
     * FlagNone:               No flags.
     * FlagLoadOsCatalog:      Add AppStream catalog metadata to the pool.
     * FlagLoadOsMetainfo:     Add data from AppStream metainfo files to the pool.
     * FlagLoadOsDesktopFiles: Add metadata from desktop-entry files to the pool.
     * FlagLoadFlatpak:        Add AppStream metadata from Flatpak to the pool.
     * FlagIgnoreCacheAge:     Ignore cache age and always load data from scratch.
     * FlagResolveAddons:      Always resolve addons for returned components
     * FlagPreferOsMetainfo:   Prefer local metainfo data over the system-provided catalog data. Useful for debugging.
     * FlagMonitor:            Monitor registered directories for changes, and auto-reload metadata if necessary.
     *
     * Flags controlling the metadata pool behavior.
     **/
    enum Flag {
        FlagNone = 0,
        FlagLoadOsCatalog = 1 << 0,
        FlagLoadOsMetainfo = 1 << 1,
        FlagLoadOsDesktopFiles = 1 << 2,
        FlagLoadFlatpak = 1 << 3,
        FlagIgnoreCacheAge = 1 << 4,
        FlagResolveAddons = 1 << 5,
        FlagPreferOsMetainfo = 1 << 6,
        FlagMonitor = 1 << 7,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * \return true on success. False on failure
     *
     * Loads all available metadata and opens the cache. In case of an error,
     * \sa lastError() will contain the error message.
     */
    bool load();

    /**
     * Loads all available metadata and opens the cache asynchronously.
     * Once finished, the \sa loaded() signal will be emitted, with
     * its bool argument indicating whether it was successful.
     */
    void loadAsync();

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
    bool addComponents(const ComponentBox &cbox);

    ComponentBox components() const;

    ComponentBox componentsById(const QString &cid) const;

    ComponentBox componentsByProvided(Provided::Kind kind, const QString &item) const;

    ComponentBox componentsByKind(Component::Kind kind) const;

    ComponentBox componentsByCategories(const QStringList &categories) const;

    ComponentBox componentsByLaunchable(Launchable::Kind kind, const QString &value) const;

    ComponentBox componentsByExtends(const QString &extendedId) const;

    ComponentBox
    componentsByBundleId(Bundle::Kind kind, const QString &bundleId, bool matchPrefix) const;

    ComponentBox search(const QString &term) const;

    void setLocale(const QString &locale);

    Pool::Flags flags() const;
    void setFlags(Pool::Flags flags);
    void addFlags(Pool::Flags flags);
    void removeFlags(Pool::Flags flags);

    void resetExtraDataLocations();
    void addExtraDataLocation(const QString &directory, Metadata::FormatStyle formatStyle);

    void setLoadStdDataLocations(bool enabled);
    void overrideCacheLocations(const QString &sysDir, const QString &userDir);

    bool isEmpty() const;

Q_SIGNALS:
    void changed();

    /**
     * Emitted when the pool has been loaded asynchronously.
     * \param success Whether loading was successful.
     * \sa lastError() will contain the error message.
     * \sa loadAsync()
     */
    void loadFinished(bool success);

private:
    Q_DISABLE_COPY(Pool);
    QScopedPointer<PoolPrivate> d;
};

}

#endif // APPSTREAMQT_POOL_H
