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

#include "appstream.h"
#include "screenshot.h"

#include <QSharedData>
#include <QString>
#include <QDebug>
#include "chelpers.h"
#include "image.h"
#include "video.h"

using namespace AppStream;

class AppStream::ScreenshotData : public QSharedData {
public:
    ScreenshotData()
    {
        m_scr = as_screenshot_new();
    }

    ScreenshotData(AsScreenshot *scr) : m_scr(scr)
    {
        g_object_ref(m_scr);
    }

    ~ScreenshotData()
    {
        g_object_unref(m_scr);
    }

    bool operator==(const ScreenshotData& rd) const
    {
        return rd.m_scr == m_scr;
    }

    AsScreenshot *screenshot() const
    {
        return m_scr;
    }

    AsScreenshot *m_scr;
};

Screenshot::Screenshot(const Screenshot& other)
    : d(other.d)
{}

Screenshot::Screenshot()
    : d(new ScreenshotData)
{}

Screenshot::Screenshot(_AsScreenshot *scr)
    : d(new ScreenshotData(scr))
{}

Screenshot::~Screenshot()
{}

Screenshot& Screenshot::operator=(const Screenshot& other)
{
    d = other.d;
    return *this;
}

_AsScreenshot * AppStream::Screenshot::asScreenshot() const
{
    return d->screenshot();
}

bool Screenshot::isDefault() const
{
    return as_screenshot_get_kind(d->m_scr) == AS_SCREENSHOT_KIND_DEFAULT;
}

Screenshot::MediaKind Screenshot::mediaKind() const
{
    return Screenshot::MediaKind(as_screenshot_get_media_kind(d->screenshot()));
}

QString Screenshot::caption() const
{
    return valueWrap(as_screenshot_get_caption(d->m_scr));
}

void Screenshot::setCaption(const QString& caption, const QString& lang)
{
    as_screenshot_set_caption(d->m_scr, qPrintable(caption), lang.isEmpty()? NULL : qPrintable(lang));
}

QList<Image> Screenshot::images() const
{
    QList<Image> res;

    auto images = as_screenshot_get_images(d->m_scr);
    res.reserve(images->len);
    for (uint i = 0; i < images->len; i++) {
        auto img = AS_IMAGE (g_ptr_array_index (images, i));
        res.append(Image(img));
    }
    return res;
}

QList<Video> Screenshot::videos() const
{
    QList<Video> res;

    auto videos = as_screenshot_get_videos(d->m_scr);
    res.reserve(videos->len);
    for (uint i = 0; i < videos->len; i++) {
        auto vid = AS_VIDEO (g_ptr_array_index (videos, i));
        res.append(Video(vid));
    }
    return res;
}

QDebug operator<<(QDebug s, const AppStream::Screenshot& screenshot) {
    s.nospace() << "AppStream::Screenshot(";
    if (!screenshot.caption().isEmpty())
        s.nospace() << screenshot.caption() << ":";
    s.nospace() << screenshot.images() << ')';
    return s.space();
}
