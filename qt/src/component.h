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
 *
 */

#ifndef APPSTREAMQT_COMPONENT_H
#define APPSTREAMQT_COMPONENT_H

#include <QSharedDataPointer>
#include <QObject>
#include <QUrl>
#include <QStringList>
#include <QSize>
#include "appstreamqt_export.h"
#include "provides.h"


namespace Appstream {

class Screenshot;

class ComponentData;

/**
 * Describes a Component (package) in appstream
 */
class APPSTREAMQT_EXPORT Component {
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

        QString id() const;
        void setId(const QString& id);

        QStringList packageNames() const;
        void setPackageNames(const QStringList& packageName);

        QString name() const;
        void setName(const QString& name);

        QString summary() const;
        void setSummary(const QString& summary);

        QString description() const;
        void setDescription(const QString& description);

        QString projectLicense() const;
        void setProjectLicense(const QString& license);

        QString projectGroup() const;
        void setProjectGroup(const QString& group);

        QString developerName() const;
        void setDeveloperName(const QString& developerName);

        QStringList compulsoryForDesktops() const;
        void setCompulsoryForDesktops(const QStringList& desktops);
        bool isCompulsoryForDesktop(const QString& desktop) const;

        QStringList categories() const;
        void setCategories(const QStringList& categories);
        bool hasCategory(const QString& category) const;

        /**
         * \return generic (stock) icon name
         */
        QString icon() const;
        void setIcon(const QString& icon);

        /**
         * \return absolute path to an icon of the given size
         */
        QUrl iconUrl() const;
        QUrl iconUrl(const QSize& size) const;
        void setIconUrl(const QUrl& iconUrl);
        void addIconUrl(const QUrl& iconUrl, const QSize& size);


        void setUrls(const QMultiHash<UrlKind , QUrl >& urls);
        QMultiHash<UrlKind, QUrl> urls() const;
        QList<QUrl> urls(UrlKind kind) const;


        /**
         * \param kind for provides
         * \return a list of all provides for this \param kind
         */
        QList<Appstream::Provides> provides(Provides::Kind kind) const;
        void setProvides(const QList<Appstream::Provides>& provides);
        /**
         * \return the full list of provides for all kinds.
         * Note that it might be ordered differently than the list given with
         * \ref setProvides, but it will have the same entries.
         */
        QList<Appstream::Provides> provides() const;

        QList<Appstream::Screenshot> screenshots() const;
        void setScreenshots(const QList<Appstream::Screenshot>& screenshots);

        /**
         * \returns whether the component is fully initialized
         */
        bool isValid() const;

        static Kind stringToKind(const QString& kindString);
        static QString kindToString(Kind kind);

        static UrlKind stringToUrlKind(const QString& urlKindString);
        static QString urlKindToString(Appstream::Component::UrlKind kind);

    private:
        QSharedDataPointer<ComponentData> d;
};
}

#endif // APPSTREAMQT_COMPONENT_H
