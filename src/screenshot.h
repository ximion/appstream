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

#ifndef ASMARA_SCREENSHOT_H
#define ASMARA_SCREENSHOT_H

#include <QSharedDataPointer>

#include "asmara_export.h"

class QString;
template<class T >
class QList;
namespace Asmara {

class Image;

class ScreenShotData;

/**
 * Class to represent a reference to a screenshot
 * A screenshot might appear in various resolutions
 */

class ASMARA_EXPORT ScreenShot {
public:
    ScreenShot();
    ScreenShot(const ScreenShot& other);
    ~ScreenShot();
    ScreenShot& operator=(const ScreenShot& other);
    bool operator==(const ScreenShot& other);

    /**
     * \return true if it is the default screenshot
     * A \ref Component should in general only have one default
     */
    bool isDefault() const;
    void setDefault(bool default_);

    void setImages(const QList<Asmara::Image>& images);
    /**
     * \return the images for this screenshot
     */
    const QList<Asmara::Image> images() const;

    /**
     * \return caption for this image or a null QString if no caption
     */
    const QString& caption() const;
    void setCaption(const QString& caption);

private:
    QSharedDataPointer<ScreenShotData> d;

};
}

#endif // ASMARA_SCREENSHOT_H
