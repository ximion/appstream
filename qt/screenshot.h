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

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <QtCore>
#include "appstream-qt_global.h"
#include "image.h"

namespace Appstream {

class ScreenshotPrivate;

class ASQTSHARED_EXPORT Screenshot : QObject
{
    Q_OBJECT
    Q_ENUMS(Kind)

public:
    enum Kind  {
        KindUnknown,
        KindNormal,
        KindDefault
    };

    Screenshot(QObject *parent = 0);
    ~Screenshot();

    Kind getKind();
    void setKind(Kind kind);

    static QString kindToString(Kind kind);
    static Kind kindFromString(QString kind_str);

    QString getCaption();
    void setCaption(QString caption);

    QList<Image*> *getImages();
    void addImage(Image *image);

private:
    ScreenshotPrivate *priv;
};

} // End of namespace: Appstream

#endif // SCREENSHOT_H
