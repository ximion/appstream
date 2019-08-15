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

#include "appstream.h"
#include "video.h"

#include <QSharedData>
#include <QSize>
#include <QUrl>
#include <QDebug>

using namespace AppStream;

class AppStream::VideoData : public QSharedData {
public:
    VideoData()
    {
        m_vid = as_video_new();
    }

    VideoData(AsVideo *vid) : m_vid(vid)
    {
        g_object_ref(m_vid);
    }

    ~VideoData()
    {
        g_object_unref(m_vid);
    }

    bool operator==(const VideoData& rd) const
    {
        return rd.m_vid == m_vid;
    }

    AsVideo *video() const
    {
        return m_vid;
    }

    AsVideo *m_vid;
};

Video::Video(const Video& other)
    : d(other.d)
{}

Video::Video()
    : d(new VideoData)
{}

Video::Video(_AsVideo *vid)
    : d(new VideoData(vid))
{}

Video::~Video()
{}

Video& Video::operator=(const Video& other)
{
    this->d = other.d;
    return *this;
}

_AsVideo * AppStream::Video::asVideo() const
{
    return d->video();
}

Video::CodecKind Video::codec() const
{
    return Video::CodecKind(as_video_get_codec_kind(d->m_vid));
}

void Video::setCodec(Video::CodecKind codec)
{
    as_video_set_codec_kind(d->m_vid, (AsVideoCodecKind) codec);
}

Video::ContainerKind Video::container() const
{
    return Video::ContainerKind(as_video_get_container_kind(d->m_vid));
}

void Video::setContainer(Video::ContainerKind container)
{
    as_video_set_container_kind(d->m_vid, (AsVideoContainerKind) container);
}

uint Video::height() const
{
    return as_video_get_height(d->m_vid);
}

void Video::setHeight(uint height) {
    as_video_set_height(d->m_vid, height);
}

uint Video::width() const
{
    return as_video_get_width(d->m_vid);
}

void Video::setWidth(uint width)
{
    as_video_set_width(d->m_vid, width);
}

void Video::setUrl(const QUrl& url)
{
    as_video_set_url(d->m_vid, qPrintable(url.toString()));
}

const QUrl Video::url() const {
    return QUrl(as_video_get_url(d->m_vid));
}

QSize AppStream::Video::size() const
{
    return QSize(width(), height());
}

QDebug operator<<(QDebug s, const AppStream::Video& video) {
    s.nospace() << "AppStream::Video(" << video.url() << ',' << video.container() << ':' << video.codec() << "[" << video.width() << "x" << video.height() << "])";
    return s.space();
}
