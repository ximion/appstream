/*
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef IMAGE_H
#define IMAGE_H

#include <QtCore>
#include "appstream-qt_global.h"

namespace Appstream {

class ImagePrivate;

class ASQTSHARED_EXPORT Image : public QObject
{
    Q_OBJECT
    Q_ENUMS(Kind)

public:
    enum Kind  {
        KindUnknown,
        KindSource,
        KindThumbnail
    };

    Image(QObject *parent = 0);
    ~Image();

    Kind getKind();
    void setKind(Kind kind);

    static QString kindToString(Kind kind);
    static Kind kindFromString(QString kind_str);

    QUrl getUrl();
    void setUrl(QUrl url);

    int getWidth();
    void setWidth(int width);

    int getHeight();
    void setHeight(int height);

private:
    ImagePrivate *priv;
};

} // End of namespace: Appstream

#endif // IMAGE_H
