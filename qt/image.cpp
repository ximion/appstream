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
#include "image.h"

using namespace Appstream;

class Appstream::ImagePrivate
{
public:
    ImagePrivate()
    {
    }

    ~ImagePrivate() {
    }

    QString url;
    int width;
    int height;
    Image::Kind kind;
};

Image::Image(QObject *parent)
    : QObject(parent)
{
    priv = new ImagePrivate();
}

Image::~Image()
{
    delete priv;
}

Image::Kind
Image::getKind()
{
    return priv->kind;
}

void
Image::setKind(Image::Kind kind)
{
    priv->kind = kind;
}

QString
Image::kindToString(Image::Kind kind)
{
    return QString::fromUtf8(as_image_kind_to_string ((AsImageKind) kind));

}

Image::Kind
Image::kindFromString(QString kind_str)
{
    return (Image::Kind) as_image_kind_from_string (qPrintable(kind_str));
}

QString
Image::getUrl()
{
    return priv->url;
}

void
Image::setUrl(QString url)
{
    priv->url = url;
}

int
Image::getWidth()
{
    return priv->width;
}

void
Image::setWidth(int width)
{
    priv->width = width;
}

int
Image::getHeight()
{
    return priv->height;
}

void
Image::setHeight(int height)
{
    priv->height = height;
}
