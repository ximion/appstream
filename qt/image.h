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

#ifndef APPSTREAMQT_IMAGE_H
#define APPSTREAMQT_IMAGE_H

#include <QSharedDataPointer>
#include <QObject>
#include "appstreamqt_export.h"

class QUrl;
class QString;
struct _AsImage;
namespace AppStream {

class ImageData;

/**
 * A reference to an image that can be accessed via a URL
 *
 * This class doesn't contain any image data, but only a reference to
 * an url and the expected size for the image.
 *
 * "expected size" means that the data is read out from the AppStream metadata.
 * Discrepancies between the data and the actual data are very rare, but might happen.
 * image at the end of the url.
 */
class APPSTREAMQT_EXPORT Image {
    Q_GADGET
    public:
        enum Kind {
            KindUnknown,
            KindSource,
            KindThumbnail
        };
        Q_ENUM(Kind)

        Image();
        Image(_AsImage *img);
        Image(const Image& other);
        ~Image();

        Image& operator=(const Image& other);

        /**
         * \returns the internally stored AsImage
         */
        _AsImage *asImage() const;

        /**
         * \return the kind of image
         */
        Kind kind() const;
        void setKind(Kind kind);

        /**
         * \return the url for this image
         */
        const QUrl url() const;
        void setUrl(const QUrl& url);

        /**
         * \return the expected width of this image
         */
        uint width() const;
        void setWidth(uint width);

        /**
         * \return the expected height of this image
         */
        uint height() const;
        void setHeight(uint height);

        /**
         * \returns the expected size of the image
         */
        QSize size() const;

    private:
        QSharedDataPointer<ImageData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Image& image);

#endif // APPSTREAMQT_IMAGE_H
