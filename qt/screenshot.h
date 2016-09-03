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

#ifndef APPSTREAMQT_SCREENSHOT_H
#define APPSTREAMQT_SCREENSHOT_H

#include <QSharedDataPointer>

#include "appstreamqt_export.h"

#include <QString>
#include <QList>
namespace AppStream {

class Image;

class ScreenshotData;

/**
 * Class to represent a reference to a screenshot
 * A screenshot might appear in various resolutions
 */

class APPSTREAMQT_EXPORT Screenshot {
public:
    Screenshot();
    Screenshot(const Screenshot& other);
    ~Screenshot();
    Screenshot& operator=(const Screenshot& other);
    bool operator==(const Screenshot& other);

    /**
     * \return true if it is the default screenshot
     * A \ref Component should in general only have one default
     */
    bool isDefault() const;
    void setDefault(bool default_);

    void setImages(const QList<AppStream::Image>& images);
    /**
     * \return the images for this screenshot
     */
    QList<AppStream::Image> images() const;

    /**
     * \return caption for this image or a null QString if no caption
     */
    QString caption() const;
    void setCaption(const QString& caption);

private:
    QSharedDataPointer<ScreenshotData> d;

};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Screenshot& screenshot);

#endif // APPSTREAMQT_SCREENSHOT_H
