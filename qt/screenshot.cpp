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

#include "appstream.h"
#include "screenshot.h"

using namespace Appstream;

class Appstream::ScreenshotPrivate
{
public:
    ScreenshotPrivate()
    {
        images = new QList<Image*> ();
    }

    ~ScreenshotPrivate() {
        delete images;
    }

    Screenshot::Kind kind;
    QString caption;
    QList<Image*> *images;
};

Screenshot::Screenshot(QObject *parent)
    : QObject(parent)
{
    priv = new ScreenshotPrivate();
}

Screenshot::~Screenshot()
{
    delete priv;
}

void
Screenshot::setKind(Screenshot::Kind kind)
{
    priv->kind = kind;
}

Screenshot::Kind
Screenshot::getKind()
{
    return priv->kind;
}

QString
Screenshot::kindToString(Screenshot::Kind kind)
{
    return QString::fromUtf8(as_screenshot_kind_to_string ((AsScreenshotKind) kind));

}

Screenshot::Kind
Screenshot::kindFromString(QString kind_str)
{
    return (Screenshot::Kind) as_screenshot_kind_from_string (qPrintable(kind_str));
}

QString
Screenshot::getCaption()
{
    return priv->caption;
}

void
Screenshot::setCaption(QString caption)
{
    priv->caption = caption;
}

void
Screenshot::addImage(Image *image)
{
    priv->images->append(image);
}

QList<Image*>*
Screenshot::getImages()
{
    return priv->images;
}
