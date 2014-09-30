/*
 * Part of Appstream, a library for accessing AppStream on-disk database
 * Copyright 2014  Sune Vuorela <sune@vuorela.dk>
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
 */

#include "component.h"
#include "screenshot.h"
#include <QSharedData>
#include <QStringList>
#include <QUrl>
#include <QMultiHash>

using namespace Appstream;

class Appstream::ComponentData : public QSharedData {
    public:
        QStringList m_categories;
        QStringList m_compulsoryForDesktops;
        QString m_description;
        QString m_developerName;
        QStringList m_extends;
        QString m_icon;
        QUrl m_iconUrl;
        QString m_id;
        Component::Kind m_kind;
        QString m_name;
        QStringList m_packageNames;
        QString m_projectGroup;
        QString m_projectLicense;
        QString m_summary;
        QMultiHash<Component::UrlKind, QUrl> m_urls;
        QList<Appstream::Screenshot> m_screenshots;
        QMultiHash<Provides::Kind, Provides> m_provides;
        bool operator==(const ComponentData& other) const {
            if(m_categories != other.m_categories) {
                return false;
            }
            if(m_compulsoryForDesktops != other.m_compulsoryForDesktops) {
                return false;
            }
            if(m_description != other.m_description) {
                return false;
            }
            if(m_developerName != other.m_developerName) {
                return false;
            }
            if(m_extends != other.m_extends) {
                return false;
            }
            if(m_icon != other.m_icon) {
                return false;
            }
            if(m_iconUrl != other.m_iconUrl) {
                return false;
            }
            if(m_id != other.m_id) {
                return false;
            }
            if(m_kind != other.m_kind) {
                return false;
            }
            if(m_name != other.m_name) {
                return false;
            }
            if(m_packageNames != other.m_packageNames) {
                return false;
            }
            if(m_projectGroup != other.m_projectGroup) {
                return false;
            }
            if(m_projectLicense != other.m_projectLicense) {
                return false;
            }
            if(m_summary != other.m_summary) {
                return false;
            }
            if(m_urls != other.m_urls) {
                return false;
            }
            if(m_screenshots != other.m_screenshots) {
                return false;
            }
            if(m_provides != other.m_provides) {
                return false;
            }
            return true;
        }
};


QStringList Component::categories() const {
    return d->m_categories;
}

QStringList Component::compulsoryForDesktops() const {
    return d->m_compulsoryForDesktops;
}

QString Component::description() const {
    return d->m_description;
}

QString Component::developerName() const {
    return d->m_developerName;
}

bool Component::hasCategory(const QString& category) const {
    return d->m_categories.contains(category);
}

QString Component::icon() const {
    return d->m_icon;
}

QUrl Component::iconUrl() const {
    return d->m_iconUrl;
}

QString Component::id() const {
    return d->m_id;
}

bool Component::isCompulsoryForDesktop(const QString& desktop) const {
    return d->m_compulsoryForDesktops.contains(desktop);
}

Component::Kind Component::kind() const {
    return d->m_kind;
}

QString Component::name() const {
    return d->m_name;
}

QStringList Component::packageNames() const {
    return d->m_packageNames;
}

QString Component::projectGroup() const {
    return d->m_projectGroup;
}

QString Component::projectLicense() const {
    return d->m_projectLicense;
}

void Component::setCategories(const QStringList& categories) {
    d->m_categories = categories;
}

void Component::setCompulsoryForDesktops(const QStringList& desktops) {
    d->m_compulsoryForDesktops = desktops;
}

void Component::setDescription(const QString& description) {
    d->m_description = description;
}

void Component::setDeveloperName(const QString& developerName) {
    d->m_developerName = developerName;
}

void Component::setIcon(const QString& icon) {
    d->m_icon = icon;
}

void Component::setIconUrl(const QUrl& iconUrl) {
    d->m_iconUrl = iconUrl;
}

void Component::setId(const QString& id) {
    d->m_id = id;
}

void Component::setKind(Component::Kind kind) {
    d->m_kind = kind;
}

void Component::setName(const QString& name) {
    d->m_name = name;
}

void Component::setPackageNames(const QStringList& packageNames) {
    d->m_packageNames = packageNames;
}

void Component::setProjectGroup(const QString& group) {
    d->m_projectGroup = group;
}

void Component::setProjectLicense(const QString& license){
    d->m_projectLicense = license;
}

void Component::setSummary(const QString& summary){
    d->m_summary = summary;
}

QString Component::summary() const {
    return d->m_summary;
}

Component::Component(const Component& other) : d(other.d) {

}

Component::Component() : d(new ComponentData) {

}

Component& Component::operator=(const Component& other) {
    this->d = other.d;
    return *this;
}

void Component::setUrls(const QMultiHash< Component::UrlKind, QUrl >& urls) {
    d->m_urls = urls;
}
QList< QUrl > Component::urls(Component::UrlKind kind) const {
    return d->m_urls.values(kind);
}

QMultiHash< Component::UrlKind, QUrl > Component::urls() const {
    return d->m_urls;
}





bool Component::operator==(const Component& other) {
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

Component::~Component() {

}

QHash<Component::Kind, QString> buildKindMap() {
    QHash<Component::Kind,QString> map;
    map.insert(Component::KindAddon, QLatin1String("addon"));
    map.insert(Component::KindCodec, QLatin1String("codec"));
    map.insert(Component::KindDesktop, QLatin1String("desktop"));
    map.insert(Component::KindFont, QLatin1String("font"));
    map.insert(Component::KindGeneric, QLatin1String("generic"));
    map.insert(Component::KindInputmethod, QLatin1String("inputmethod"));
    map.insert(Component::KindUnknown, QLatin1String("unknown"));
    return map;
}

QString Component::kindToString(Component::Kind kind) {
    static QHash<Kind, QString> kindMap = buildKindMap();
    return kindMap.value(kind);
}

Component::Kind Component::stringToKind(const QString& kindString) {
    if(kindString ==  QLatin1String("generic")) {
        return KindGeneric;
    }
    if (kindString == QLatin1String("desktop")) {
        return KindDesktop;
    }
    if (kindString == QLatin1String("font")) {
        return KindFont;
    }
    if (kindString == QLatin1String("codec")) {
        return KindCodec;
    }
    if (kindString==QLatin1String("inputmethod")) {
        return KindInputmethod;
    }
    if (kindString == QLatin1String("addon")) {
        return KindAddon;
    }
    return KindUnknown;

}

void Component::setScreenshots(const QList< Screenshot >& screenshots) {
    d->m_screenshots = screenshots;
}

QList< Screenshot > Component::screenshots() const {
    return d->m_screenshots;
}




Component::UrlKind Component::stringToUrlKind(const QString& urlKindString) {
    if (urlKindString == QLatin1String("homepage")) {
        return UrlKindHomepage;
    }
    if (urlKindString == QLatin1String("bugtracker")) {
        return UrlKindBugtracker;
    }
    if (urlKindString == QLatin1String("faq")) {
        return UrlKindFaq;
    }
    if (urlKindString == QLatin1String("help")) {
        return UrlKindHelp;
    }
    if (urlKindString == QLatin1String("donation")) {
        return UrlKindDonation;
    }
    return UrlKindUnknown;
}

static QHash<Component::UrlKind,QString> buildUrlKindMap() {
    QHash<Component::UrlKind, QString> map;
    map.insert(Component::UrlKindBugtracker,QLatin1String("bugtracker"));
    map.insert(Component::UrlKindDonation,QLatin1String("donation"));
    map.insert(Component::UrlKindFaq,QLatin1String("faq"));
    map.insert(Component::UrlKindHelp,QLatin1String("help"));
    map.insert(Component::UrlKindHomepage, QLatin1String("homepage"));
    map.insert(Component::UrlKindUnknown, QLatin1String("unknown"));
    return map;
}

QString Component::urlKindToString(Component::UrlKind kind) {
    static const QHash<UrlKind, QString> kindMap = buildUrlKindMap();
    return kindMap.value(kind);
}

QList< Appstream::Provides > Component::provides() const {
    return d->m_provides.values();
}

QList<Appstream::Provides> Component::provides(Provides::Kind kind) const {
    return d->m_provides.values(kind);
}


void Component::setProvides(const QList<Appstream::Provides>& provides) {
    Q_FOREACH(const Appstream::Provides& provide, provides) {
        d->m_provides.insertMulti(provide.kind(), provide);
    }
}

bool Component::isValid() const
{
    return !d->m_name.isEmpty();
}

