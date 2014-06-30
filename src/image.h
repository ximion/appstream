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

#ifndef ASMARA_IMAGE_H
#define ASMARA_IMAGE_H

#include <QSharedDataPointer>
#include <QObject>
#include "asmara_export.h"

class QUrl;
class QString;
namespace Asmara {

class ImageData;

/**
 * A reference to a image that can be accessed thru a URL
 */
class ASMARA_EXPORT Image {
    Q_GADGET
    Q_ENUMS(Kind)
    public:
        enum Kind {
            Unknown,
            Thumbnail,
            Plain
        };
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

#endif // ASMARA_IMAGE_H

class QUrl;
