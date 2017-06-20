/*
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

#ifndef APPSTREAMQT_ICON_H
#define APPSTREAMQT_ICON_H

#include <QSharedDataPointer>
#include <QObject>
#include "appstreamqt_export.h"

class QUrl;
class QString;
struct _AsIcon;
namespace AppStream {

class IconData;

/**
 * A reference to n icon which can be loaded from a locale file
 * or remote URI.
 */
class APPSTREAMQT_EXPORT Icon {
    Q_GADGET
    public:
        enum Kind {
            KindUnknown,
            KindCached,
            KindStock,
            KindLocal,
            KindRemote
        };
        Q_ENUM(Kind)

        Icon();
        Icon(_AsIcon *icon);
        Icon(const Icon& other);
        ~Icon();

        Icon& operator=(const Icon& other);

        /**
         * \returns the internally stored AsIcon
         */
        _AsIcon *asIcon() const;

        /**
         * \return the kind of icon
         */
        Kind kind() const;
        void setKind(Kind kind);

        /**
         * \return the local or remote url for this image
         */
        const QUrl url() const;
        void setUrl(const QUrl& url);

        /**
         * \return the icon (stock) name
         */
        const QString name() const;
        void setName(const QString& name);

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

        bool isEmpty() const;

    private:
        QSharedDataPointer<IconData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Icon& image);

#endif // APPSTREAMQT_ICON_H
