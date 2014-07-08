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

#include "screenshot.h"
#include <QSharedData>
#include <QString>
#include "image.h"

using namespace Appstream;

class Appstream::ScreenShotData : public QSharedData {
    public:
        ScreenShotData() : m_default(false) {
        }
        bool m_default;
        QString m_caption;
        QList<Appstream::Image> m_images;
        bool operator==(const ScreenShotData& other) {
            if(m_default != other.m_default) {
                return false;
            }
            if(m_caption != other.m_caption) {
                return false;
            }
            if(m_images != other.m_images) {
                return false;
            }
            return true;
        }
};

const QString& ScreenShot::caption() const {
    return d->m_caption;
}

const QList< Image > ScreenShot::images() const {
    return d->m_images;
}

bool ScreenShot::isDefault() const {
    return d->m_default;
}

ScreenShot& ScreenShot::operator=(const ScreenShot& other) {
    d = other.d;
    return *this;
}

bool ScreenShot::operator==(const ScreenShot& other) {
    if(d == other.d) {
        return true;
    }
    if(d && other.d) {
        return *d == *other.d;
    }
    return false;
}

ScreenShot::ScreenShot(const ScreenShot& other) : d(other.d) {

}

ScreenShot::ScreenShot() : d(new ScreenShotData) {

}

void ScreenShot::setCaption(const QString& caption) {
    d->m_caption = caption;
}

void ScreenShot::setDefault(bool default_) {
    d->m_default = default_;
}

void ScreenShot::setImages(const QList< Image >& images) {
    d->m_images = images;
}

ScreenShot::~ScreenShot() {

}
