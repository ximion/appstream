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

#ifndef APPSTREAMQT_CATEGORY_H
#define APPSTREAMQT_CATEGORY_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsCategory;

namespace AppStream
{

class CategoryData;
class Component;

class APPSTREAMQT_EXPORT Category
{
    Q_GADGET

public:
    Category();
    Category(_AsCategory *category);
    Category(const Category &category);
    ~Category();

    Category &operator=(const Category &category);
    bool operator==(const Category &r) const;

    /**
     * \returns the internally stored AsCategory
     */
    _AsCategory *cPtr() const;

    QString id() const;
    void setId(const QString &id);

    QString name() const;
    void setName(const QString &name);

    QString summary() const;
    void setSummary(const QString &summary);

    QString icon() const;
    void setIcon(const QString &icon);

    QList<Category> children() const;
    bool hasChildren() const;
    void addChild(const Category &subcat);
    void removeChild(const Category &subcat);

    QStringList desktopGroups() const;
    void addDesktopGroup(const QString &groupName);

    QList<AppStream::Component> components() const;
    void addComponent(const AppStream::Component &cpt);
    bool hasComponent(const AppStream::Component &cpt) const;

private:
    QSharedDataPointer<CategoryData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Category &category);

QList<AppStream::Category> getDefaultCategories(bool withSpecial);

#endif // APPSTREAMQT_CATEGORY_H
