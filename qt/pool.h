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
         * \return true on success. False on failure
         */
        bool load();

        /**
         * Remove all software component information from the pool.
         */
        void clear();

        /**
         * Add a component to the pool.
         */
        bool addComponent(AppStream::Component cpt);

        QList<AppStream::Component> components() const;

        QList<AppStream::Component> componentsByProvided(Provides::Kind kind, const QString& item) const;

        QList<AppStream::Component> componentsByKind(Component::Kind kind) const;

        QList<AppStream::Component> componentsByCategories(const QStringList categories) const;

        QList<AppStream::Component> search(const QString& term) const;

    private:
        Q_DISABLE_COPY(Pool);
        QScopedPointer<PoolPrivate> d;
};
}

#endif // APPSTREAMQT_POOL_H
