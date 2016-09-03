/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright (C) 2014  Sune Vuorela <sune@vuorela.dk>
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

#include "image.h"
#include <QSharedData>
#include <QUrl>
#include <QDebug>

using namespace AppStream;

class AppStream::ImageData : public QSharedData {
    public:
        ImageData() : m_height(0), m_width(0), m_kind(Image::Unknown) {
        }
        int m_height;
        int m_width;
        Image::Kind m_kind;
        QUrl m_url;
        bool operator==(const ImageData& other) {
            if(m_height != other.m_height) {
                return false;
            }
            if(m_width != other.m_width) {
                return false;
            }
            if(m_kind != other.m_kind) {
                return false;
            }
            if(m_url != other.m_url) {
                return false;
            }
            return true;
        }
};

int Image::height() const {
    return d->m_height;
}

Image::Image(const Image& other) : d(other.d) {

}

Image::Image() : d(new ImageData) {

}

Image::~Image() {

}

Image::Kind Image::kind() const {
    return d->m_kind;
}



Image& Image::operator=(const Image& other) {
    this->d = other.d;
    return *this;
}

bool Image::operator==(const Image& other) {
    if(d == other.d) {
        return true;
    }
    if(d && other.d) {
        return *d == *other.d;
    }
    return false;
}

void Image::setHeight(int height) {
    d->m_height = height;
}

void Image::setKind(Image::Kind kind) {
    d->m_kind = kind;
}

void Image::setUrl(const QUrl& url) {
    d->m_url = url;
}

void Image::setWidth(int width) {
    d->m_width = width;
}

Image::Kind Image::stringToKind(const QString& string) {
    if(string == QLatin1String("thumbnail")) {
        return Thumbnail;
    }
    if(string == QLatin1String("source")) {
        return Plain;
    }
    return Unknown;
}

const QUrl& Image::url() const {
    return d->m_url;
}

int Image::width() const {
    return d->m_width;
}

QDebug operator<<(QDebug s, const AppStream::Image& image) {
    s.nospace() << "AppStream::Image(" << image.url() << ',' << image.kind() << "[" << image.width() << "x" << image.height() << "])";
    return s.space();
}








