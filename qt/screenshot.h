/*
 * Copyright (C) 2014 Sune Vuorela <sune@vuorela.dk>
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef APPSTREAMQT_SCREENSHOT_H
#define APPSTREAMQT_SCREENSHOT_H

#include <QObject>
#include <QSharedDataPointer>
#include "appstreamqt_export.h"

#include <QString>
#include <QList>


struct _AsScreenshot;
namespace AppStream {

class Image;
class Video;
class ScreenshotData;

/**
 * Class to represent a reference to a screenshot
 * A screenshot might appear in various resolutions
 */

class APPSTREAMQT_EXPORT Screenshot {
Q_GADGET
public:
    enum MediaKind {
        MediaKindUnknown,
        MediaKindImage,
        MediaKindVideo
    };
    Q_ENUM(MediaKind)

    Screenshot();
    Screenshot(_AsScreenshot *scr);
    Screenshot(const Screenshot& other);
    ~Screenshot();
    Screenshot& operator=(const Screenshot& other);

    /**
     * \returns the internally stored AsScreenshot
     */
    _AsScreenshot *asScreenshot() const;

    /**
     * \return true if it is the default screenshot
     * A \ref Component should in general only have one default
     */
    bool isDefault() const;

    /**
     * \return the kind of media (image or video) that this screenshot consists of
     */
    MediaKind mediaKind() const;

    /**
     * \return the images for this screenshot
     */
    QList<AppStream::Image> images() const;

    /**
     * \return the videos for this screenshot
     */
    QList<AppStream::Video> videos() const;

    /**
     * \return caption for this image or a null QString if no caption
     */
    QString caption() const;
    void setCaption(const QString& caption, const QString& lang = {});

private:
    QSharedDataPointer<ScreenshotData> d;

};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Screenshot& screenshot);

#endif // APPSTREAMQT_SCREENSHOT_H
