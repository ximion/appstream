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

class ASQTSHARED_EXPORT Component : public QObject
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

    Q_PROPERTY(QString id READ getId WRITE setId)
    QString getId();
    void setId(QString id);

    Q_PROPERTY(QString pkgName READ getPkgName WRITE setPkgName)
    QString getPkgName();
    void setPkgName(QString pkgname);

    Q_PROPERTY(QString name READ getName WRITE setName)
    QString getName();
    void setName(QString name);

    Q_PROPERTY(QString summary READ getSummary WRITE setSummary)
    QString getSummary();
    void setSummary(QString summary);

    Q_PROPERTY(QString description READ getDescription WRITE setDescription)
    QString getDescription();
    void setDescription(QString description);

    Q_PROPERTY(QString projectLicense READ getProjectLicense WRITE setProjectLicense)
    QString getProjectLicense();
    void setProjectLicense(QString license);

    Q_PROPERTY(QString projectGroup READ getProjectGroup WRITE setProjectGroup)
    QString getProjectGroup();
    void setProjectGroup(QString group);

    Q_PROPERTY(QStringList compulsoryForDesktops READ getCompulsoryForDesktops WRITE setCompulsoryForDesktops)
    QStringList getCompulsoryForDesktops();
    void setCompulsoryForDesktops(QStringList desktops);
    bool isCompulsoryForDesktop(QString desktop);

    Q_PROPERTY(QStringList categories READ getCategories WRITE setCategories)
    QStringList getCategories();
    void setCategories(QStringList categories);
    bool hasCategory(QString category);

    Q_PROPERTY(QString icon READ getIcon WRITE setIcon)
    QString getIcon();
    void setIcon(QString icon);

    Q_PROPERTY(QUrl iconUrl READ getIconUrl WRITE setIconUrl)
    QUrl getIconUrl();
    void setIconUrl(QUrl iconUrl);

    /** internal */
    Component(AsComponent *cpt, QObject *parent = 0);

private:
    ComponentPrivate *priv;
};

} // End of namespace: Appstream

#endif // COMPONENT_H
