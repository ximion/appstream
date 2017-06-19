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

#ifndef APPSTREAMQT_CATEGORY_H
#define APPSTREAMQT_CATEGORY_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsCategory;

namespace AppStream {

class CategoryData;

class APPSTREAMQT_EXPORT Category {
    Q_GADGET
    public:
        Category(_AsCategory* category);
        Category(const Category& category);
        ~Category();

        Category& operator=(const Category& category);
        bool operator==(const Category& r) const;

        /**
         * \returns the internally stored AsCategory
         */
        _AsCategory *asCategory() const;

        QString id() const;
        QString name() const;
        QString summary() const;
        QString icon() const;

        QList<Category> children() const;
        QStringList desktopGroups() const;

    private:
        QSharedDataPointer<CategoryData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Category& category);

QList<AppStream::Category> getDefaultCategories(bool withSpecial);

#endif // APPSTREAMQT_CATEGORY_H
