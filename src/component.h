/*
 * Part of libAsmara, a library for accessing AppStream on-disk database
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
 *
 */

#ifndef ASMARA_COMPONENT_H
#define ASMARA_COMPONENT_H

#include <QSharedDataPointer>
#include <QObject>
#include <QUrl>
#include "asmara_export.h"


namespace Asmara {

class ScreenShot;

class ComponentData;

/**
 * Describes a Component (package) in appstream
 */
class ASMARA_EXPORT Component {
    Q_GADGET
    Q_ENUMS(Kind UrlKind)
    public:
        enum Kind  {
            KindUnknown,
            KindGeneric,
            KindDesktop,
            KindFont,
            KindCodec,
            KindInputmethod,
            KindAddon
        };
        enum UrlKind {
            UrlKindUnknown,
            UrlKindHomepage,
            UrlKindBugtracker,
            UrlKindFaq,
            UrlKindHelp,
            UrlKindDonation
        };

        Component();
        Component(const Component& other);
        ~Component();
        Component& operator=(const Component& other);
        bool operator==(const Component& other);

        Kind kind () const;
        void setKind (Component::Kind kind);

        const QString& id() const;
        void setId(QString id);

        const QString& packageName() const;
        void setPackageName(const QString& packageName);

        const QString& name() const;
        void setName(const QString& name);

        const QString& summary() const;
        void setSummary(const QString& summary);

        const QString& description();
        void setDescription(const QString& description);

        const QString& projectLicense() const;
        void setProjectLicense(const QString& license);

        const QString& projectGroup() const;
        void setProjectGroup(const QString& group);

        const QString& developerName() const;
        void setDeveloperName(const QString& developerName);

        const QStringList& compulsoryForDesktops() const;
        void setCompulsoryForDesktops(const QStringList& desktops);
        bool isCompulsoryForDesktop(const QString& desktop) const;

        const QStringList& categories() const;
        void setCategories(const QStringList& categories);
        bool hasCategory(const QString& category) const;

        /**
         * \return generic icon name
         */
        const QString& icon() const;
        void setIcon(const QString& icon);

        const QUrl& iconUrl() const;
        void setIconUrl(const QUrl& iconUrl);

        const QStringList& extends() const;

        void setUrls(const QMultiHash<UrlKind , QUrl >& urls);
        const QMultiHash<UrlKind, QUrl>& urls() const;
        QList<QUrl> urls(UrlKind kind) const;


        static Kind stringToKind(const QString& kindString);
        static QString kindToString(Kind kind);

        static UrlKind stringToUrlKind(const QString& urlKindString);
        static QString urlKindToString(Asmara::Component::UrlKind kind);

        const QList<Asmara::ScreenShot>& screenShots() const;
        void setScreenShots(const QList<Asmara::ScreenShot>& screenshots);

    private:
        QSharedDataPointer<ComponentData> d;
};
}

#endif // ASMARA_COMPONENT_H
