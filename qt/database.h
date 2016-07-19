/*
 * Part of Appstream, a library for accessing AppStream on-disk database
 *
 * Copyright 2014  Sune Vuorela <sune@vuorela.dk>
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

#ifndef APPSTREAMQT_DATABASE_H
#define APPSTREAMQT_DATABASE_H

#include "appstreamqt_export.h"

#include <QString>
#include <QList>
#include <QStringList>
#include "component.h"

namespace Appstream {

/**
 * Represents a handle to an Appstream database.
 *
 * See http://www.freedesktop.org/wiki/Distributions/AppStream/ for details
 */
class DatabasePrivate;
class APPSTREAMQT_EXPORT Database {
    public:
        /**
         * Constructs a database object
         */
        Database(const QString& dbPath);
        /**
         * Constructs a database object pointing to the default system path
         * ( /var/cache/app-info/xapian )
         */
        Database();
        /**
         * \return true on success. False on failure
         */
        bool open();
        /**
         * \return an error string describing what went wrong with open
         */
        QString errorString() const;
        /**
         * \param id
         * \return component with the given id
         */
        Component componentById(const QString& id) const;
        /**
         * \return all components
         */
        QList<Component> allComponents() const;
        /**
         * \return all components with a given kind
         */
        QList<Component> componentsByKind(Component::Kind kind) const;
        /**
         * \return all components matching \param searchTerm in \param category
         */
        QList<Component> findComponentsByString(const QString& searchTerm, const QStringList& categories = QStringList());
        /**
         * \return all components that have \param packageName assigned as the package
         */
	Q_DECL_DEPRECATED
        QList<Component> findComponentsByPackageName(const QString& packageName) const;
        ~Database();
    private:
        Q_DISABLE_COPY(Database);
        QScopedPointer<DatabasePrivate> d;
};
}

#endif // APPSTREAMQT_DATABASE_H
