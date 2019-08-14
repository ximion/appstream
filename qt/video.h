/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef APPSTREAMQT_VIDEO_H
#define APPSTREAMQT_VIDEO_H

#include <QSharedDataPointer>
#include <QObject>
#include "appstreamqt_export.h"

class QUrl;
class QString;
struct _AsVideo;
namespace AppStream {

class VideoData;

/**
 * A reference to a video that can be accessed via a URL
 *
 * This class doesn't contain any video data, but only a reference to
 * an url and a bit of useful metadata about the video.
 */
class APPSTREAMQT_EXPORT Video {
    Q_GADGET
    public:
        enum CodecKind {
            CodecKindUnknown,
            CodecKindVP9,
            CodecKindAV1
        };
        Q_ENUM(CodecKind)

        enum ContainerKind {
            ContainerKindUnknown,
            ContainerKindMKV,
            ContainerKindWebM
        };
        Q_ENUM(ContainerKind)

        Video();
        Video(_AsVideo *vid);
        Video(const Video& other);
        ~Video();

        Video& operator=(const Video& other);

        /**
         * \returns the internally stored AsVideo
         */
        _AsVideo *asVideo() const;

        /**
         * \return the codec of this video, if known
         */
        CodecKind codec() const;
        void setCodec(CodecKind codec);

	/**
         * \return the container format of this video, if known
         */
        ContainerKind container() const;
        void setContainer(ContainerKind container);

        /**
         * \return the url for this video
         */
        const QUrl url() const;
        void setUrl(const QUrl& url);

        /**
         * \return the expected width of this video
         */
        uint width() const;
        void setWidth(uint width);

        /**
         * \return the expected height of this video
         */
        uint height() const;
        void setHeight(uint height);

        /**
         * \returns the expected size of the video
         */
        QSize size() const;

    private:
        QSharedDataPointer<VideoData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Video& video);

#endif // APPSTREAMQT_VIDEO_H
