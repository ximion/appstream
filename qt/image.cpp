/*
 * Copyright (C) 2014 Sune Vuorela <sune@vuorela.dk>
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

#include "appstream.h"
#include "image.h"

#include <QSharedData>
#include <QSize>
#include <QUrl>
#include <QDebug>

using namespace AppStream;

class AppStream::ImageData : public QSharedData {
public:
    ImageData()
    {
        m_img = as_image_new();
    }

    ImageData(AsImage *img) : m_img(img)
    {
        g_object_ref(m_img);
    }

    ~ImageData()
    {
        g_object_unref(m_img);
    }

    bool operator==(const ImageData& rd) const
    {
        return rd.m_img == m_img;
    }

    AsImage *image() const
    {
        return m_img;
    }

    AsImage *m_img;
};

Image::Image(const Image& other)
    : d(other.d)
{}

Image::Image()
    : d(new ImageData)
{}

Image::Image(_AsImage *img)
    : d(new ImageData(img))
{}

Image::~Image()
{}

Image& Image::operator=(const Image& other)
{
    this->d = other.d;
    return *this;
}

_AsImage * AppStream::Image::asImage() const
{
    return d->image();
}

Image::Kind Image::kind() const
{
    return Image::Kind(as_image_get_kind(d->m_img));
}

void Image::setKind(Image::Kind kind)
{
    as_image_set_kind(d->m_img, (AsImageKind) kind);
}

uint Image::height() const
{
    return as_image_get_height(d->m_img);
}

void Image::setHeight(uint height) {
    as_image_set_height(d->m_img, height);
}

uint Image::width() const
{
    return as_image_get_width(d->m_img);
}

void Image::setWidth(uint width)
{
    as_image_set_width(d->m_img, width);
}

void Image::setUrl(const QUrl& url)
{
    as_image_set_url(d->m_img, qPrintable(url.toString()));
}

const QUrl Image::url() const {
    return QUrl(as_image_get_url(d->m_img));
}

QSize AppStream::Image::size() const
{
    return QSize(width(), height());
}

QDebug operator<<(QDebug s, const AppStream::Image& image) {
    s.nospace() << "AppStream::Image(" << image.url() << ',' << image.kind() << "[" << image.width() << "x" << image.height() << "])";
    return s.space();
}
