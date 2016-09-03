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

#ifndef APPSTREAMQT_IMAGE_H
#define APPSTREAMQT_IMAGE_H

#include <QSharedDataPointer>
#include <QObject>
#include "appstreamqt_export.h"

class QUrl;
class QString;
namespace AppStream {

class ImageData;

/**
 * A reference to a image that can be accessed thru a URL
 *
 * This class doesn't contain any image data, but only a reference to
 * a url and the expected size for the image.
 *
 * "expected size" means that the data is read out from the database, and
 * there is somehow a chance that the information might not fit with the
 * image at the end of the url.
 */
class APPSTREAMQT_EXPORT Image {
    Q_GADGET
    public:
        enum Kind {
            Unknown,
            Thumbnail,
            Plain
        };
        Q_ENUM(Kind)

        Image();
        Image(const Image& other);
        ~Image();
        Image& operator=(const Image& other);
        bool operator==(const Image& other);
        /**
         * \return the url for this image
         */
        const QUrl& url() const;
        void setUrl(const QUrl& url);

        /**
         * \return the expected width of this image
         */
        int width() const;
        void setWidth(int width);
        /**
         * \return the expected height of this image
         */
        int height() const;
        void setHeight(int height);


        /**
         * \return the kind of image
         */
        Kind kind() const;
        void setKind(Kind stringToKind);

        static Kind stringToKind(const QString& string);
    private:
        QSharedDataPointer<ImageData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Image& image);

#endif // APPSTREAMQT_IMAGE_H
