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
#include <QDebug>
#include "image.h"

using namespace AppStream;

QDebug operator<<(QDebug s, const AppStream::Screenshot& screenshot) {
    s.nospace() << "AppStream::Screenshot(";
    if (!screenshot.caption().isEmpty())
        s.nospace() << screenshot.caption() << ":";
    s.nospace() << screenshot.images() << ')';
    return s.space();
}

class AppStream::ScreenshotData : public QSharedData {
    public:
        ScreenshotData() : m_default(false) {
        }
        bool m_default;
        QString m_caption;
        QList<AppStream::Image> m_images;
        bool operator==(const ScreenshotData& other) {
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

QString Screenshot::caption() const {
    return d->m_caption;
}

QList< Image > Screenshot::images() const {
    return d->m_images;
}

bool Screenshot::isDefault() const {
    return d->m_default;
}

Screenshot& Screenshot::operator=(const Screenshot& other) {
    d = other.d;
    return *this;
}

bool Screenshot::operator==(const Screenshot& other) {
    if(d == other.d) {
        return true;
    }
    if(d && other.d) {
        return *d == *other.d;
    }
    return false;
}

Screenshot::Screenshot(const Screenshot& other) : d(other.d) {

}

Screenshot::Screenshot() : d(new ScreenshotData) {

}

void Screenshot::setCaption(const QString& caption) {
    d->m_caption = caption;
}

void Screenshot::setDefault(bool default_) {
    d->m_default = default_;
}

void Screenshot::setImages(const QList< Image >& images) {
    d->m_images = images;
}

Screenshot::~Screenshot() {

}
