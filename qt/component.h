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

#ifndef APPSTREAMQT_COMPONENT_H
#define APPSTREAMQT_COMPONENT_H

#include <QSharedDataPointer>
#include <QObject>
#include <QUrl>
#include <QStringList>
#include <QSize>
#include "appstreamqt_export.h"
#include "provided.h"
#include "bundle.h"

struct _AsComponent;
namespace AppStream {

class Screenshot;
class Release;

class ComponentData;

/**
 * Describes a software component (application, driver, font, ...)
 */
class APPSTREAMQT_EXPORT Component {
Q_GADGET
    friend class Pool;
    public:
        enum Kind  {
            KindUnknown,
            KindGeneric,
            KindDesktopApp,
            KindConsoleApp,
            KindWebApp,
            KindAddon,
            KindFont,
            KindCodec,
            KindInputmethod,
            KindFirmware,
            KindDriver,
            KindLocalization
        };
        Q_ENUM(Kind)

        enum UrlKind {
            UrlKindUnknown,
            UrlKindHomepage,
            UrlKindBugtracker,
            UrlKindFaq,
            UrlKindHelp,
            UrlKindDonation,
            UrlTranslate
        };
        Q_ENUM(UrlKind)

        static Kind stringToKind(const QString& kindString);
        static QString kindToString(Kind kind);

        static UrlKind stringToUrlKind(const QString& urlKindString);
        static QString urlKindToString(AppStream::Component::UrlKind kind);

        Component(_AsComponent *cpt);
        Component();
        Component(const Component& other);
        ~Component();

        Kind kind () const;
        void setKind (Component::Kind kind);

        QString id() const;
        void setId(const QString& id);

        QString dataId() const;
        void setDataId(const QString& cdid);

        QString desktopId() const;

        QStringList packageNames() const;

        QString name() const;
        void setName(const QString& name, const QString& lang = {});

        QString summary() const;
        void setSummary(const QString& summary, const QString& lang = {});

        QString description() const;
        void setDescription(const QString& description, const QString& lang = {});

        QString projectLicense() const;
        void setProjectLicense(const QString& license);

        QString projectGroup() const;
        void setProjectGroup(const QString& group);

        QString developerName() const;
        void setDeveloperName(const QString& developerName, const QString& lang = {});

        QStringList compulsoryForDesktops() const;
        bool isCompulsoryForDesktop(const QString& desktop) const;

        QStringList categories() const;
        bool hasCategory(const QString& category) const;

        QStringList extends() const;
        void setExtends(const QStringList& extends);
        QList<AppStream::Component> addons() const;

        QUrl url(UrlKind kind) const;

        /**
         * \return the full list of provided entries for all kinds.
         */
        QList<AppStream::Provided> provided() const;

        /**
         * \param kind for provides
         * \return provided items for this \param kind
         */
        AppStream::Provided provided(Provided::Kind kind) const;

        QList<AppStream::Screenshot> screenshots() const;

        QList<AppStream::Release> releases() const;

        QList<AppStream::Bundle> bundles() const;
        AppStream::Bundle bundle(Bundle::Kind kind) const;

        /**
         * \returns an icon that represents the component
         */
        QIcon icon() const;

    private:
        _AsComponent *m_cpt;
};
}

#endif // APPSTREAMQT_COMPONENT_H
