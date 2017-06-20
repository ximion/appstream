/*
 * Copyright (C) 2017 Jan Grulich <jgrulich@redhat.com>
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

#include "appstream.h"
#include "translation.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::TranslationData : public QSharedData {
public:
    TranslationData()
    {
        m_translation = as_translation_new();
    }

    TranslationData(AsTranslation* cat) : m_translation(cat)
    {
        g_object_ref(m_translation);
    }

    ~TranslationData()
    {
        g_object_unref(m_translation);
    }

    bool operator==(const TranslationData& rd) const
    {
        return rd.m_translation == m_translation;
    }

    AsTranslation *translation() const
    {
        return m_translation;
    }

    AsTranslation* m_translation;
};

AppStream::Translation::Kind AppStream::Translation::stringToKind(const QString& kindString)
{
    if (kindString == QLatin1String("gettext")) {
        return AppStream::Translation::KindGettext;
    } else if (kindString == QLatin1String("qt")) {
        return AppStream::Translation::KindQt;
    }
    return AppStream::Translation::KindUnknown;
}

QString AppStream::Translation::kindToString(AppStream::Translation::Kind kind)
{
    if (kind == AppStream::Translation::KindGettext) {
        return QLatin1String("gettext");
    } else if (kind == AppStream::Translation::KindQt) {
        return QLatin1String("qt");
    }
    return QLatin1String("unknown");
}

Translation::Translation()
    : d(new TranslationData)
{}

Translation::Translation(_AsTranslation* translation)
    : d(new TranslationData(translation))
{}

Translation::Translation(const Translation &translation) = default;

Translation::~Translation() = default;

Translation& Translation::operator=(const Translation &translation) = default;

bool Translation::operator==(const Translation &other) const
{
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsTranslation * AppStream::Translation::asTranslation() const
{
    return d->translation();
}

AppStream::Translation::Kind AppStream::Translation::kind() const
{
    return static_cast<AppStream::Translation::Kind>(as_translation_get_kind(d->m_translation));
}

void AppStream::Translation::setKind(AppStream::Translation::Kind kind)
{
    as_translation_set_kind(d->m_translation, (AsTranslationKind) kind);
}

QString AppStream::Translation::id() const
{
    return valueWrap(as_translation_get_id(d->m_translation));
}

void AppStream::Translation::setId(const QString& id)
{
    as_translation_set_id(d->m_translation, qPrintable(id));
}

QDebug operator<<(QDebug s, const AppStream::Translation& translation)
{
    s.nospace() << "AppStream::Translation(" << translation.id() << ")";
    return s.space();
}
