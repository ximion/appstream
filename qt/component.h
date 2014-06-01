/*
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef COMPONENT_H
#define COMPONENT_H

#include <QtCore>
#include "appstream-qt_global.h"

typedef struct _AsComponent AsComponent;

namespace Appstream {

class ComponentPrivate;

class ASQTSHARED_EXPORT Component : QObject
{
    Q_OBJECT
    Q_ENUMS(Kind)

public:
    enum Kind  {
        KindUnknown,
        KindGeneric,
        KindDesktop,
        KindFont,
        KindCodec,
        KindInputmethod
    };

    Component(QObject *parent = 0);
    ~Component();

    QString toString();

    Kind getKind ();
    void setKind (Component::Kind kind);
    static QString kindToString(Component::Kind kind);
    static Kind kindFromString(QString kind_str);

    QString getId();
    void setId(QString id);

    QString getPkgName();
    void setPkgName(QString pkgname);

    QString getName();
    void setName(QString name);

    QString getSummary();
    void setSummary(QString summary);

    QString getDescription();
    void setDescription(QString description);

    QString getProjectLicense();
    void setProjectLicense(QString license);

    QString getProjectGroup();
    void setProjectGroup(QString group);

    QStringList getCompulsoryForDesktops();
    void setCompulsoryForDesktops(QStringList desktops);
    bool isCompulsoryForDesktop(QString desktop);

    QStringList getCategories();
    void setCategories(QStringList categories);
    bool hasCategory(QString category);

    QString getIcon();
    void setIcon(QString icon);

    QString getIconUrl();
    void setIconUrl(QString iconUrl);

    /** internal */
    Component(AsComponent *cpt, QObject *parent = 0);

private:
    ComponentPrivate *priv;
};

} // End of namespace: Appstream

#endif // COMPONENT_H
