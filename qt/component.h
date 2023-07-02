/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include <QAnyStringView>
#include <QList>
#include <QSize>
#include "appstreamqt_export.h"
#include "provided.h"
#include "bundle.h"
#include "category.h"
#include "contentrating.h"
#include "launchable.h"
#include "translation.h"

struct _AsComponent;
namespace AppStream {

class Icon;
class Screenshot;
class Release;
class Relation;
class Suggested;

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
            KindLocalization,
            KindService,
            KindRepository,
            KindOperatingSystem,
            KindIconTheme,
            KindRuntime
        };
        Q_ENUM(Kind)

        enum MergeKind {
            MergeKindNone,
            MergeKindReplace,
            MergeKindAppend
        };
        Q_ENUM(MergeKind)

        enum UrlKind {
            UrlKindUnknown,
            UrlKindHomepage,
            UrlKindBugtracker,
            UrlKindFaq,
            UrlKindHelp,
            UrlKindDonation,
            UrlKindTranslate,
            UrlKindContact,
            UrlKindVcsBrowser,
            UrlKindContribute,
        };
        Q_ENUM(UrlKind)

        enum Scope {
            ScopeUnknown,
            ScopeSystem,
            ScopeUser
        };
        Q_ENUM(Scope)

        enum ValueFlags {
            FlagNone = 0,
            FlagDuplicateCheck = 1 << 0,
            FlagNoTranslationFallback = 1 << 1
        };
        Q_ENUM(ValueFlags)

        static Kind stringToKind(QAnyStringView kindString);
        static QAnyStringView kindToString(Kind kind);

        static UrlKind stringToUrlKind(QAnyStringView urlKindString);
        static QAnyStringView urlKindToString(AppStream::Component::UrlKind kind);

        static Scope stringToScope(QAnyStringView scopeString);
        static QAnyStringView scopeToString(AppStream::Component::Scope scope);

        Component(_AsComponent *cpt);
        Component();
        Component(const Component& other);
        Component(Component&& other);
        ~Component();

        Component& operator=(const Component& c);

        _AsComponent *asComponent() const;

        uint valueFlags() const;
        void setValueFlags(uint flags);

        QAnyStringView activeLocale() const;
        void setActiveLocale(QAnyStringView locale);

        Kind kind () const;
        void setKind (Component::Kind kind);

        QAnyStringView origin() const;
        void setOrigin(QAnyStringView origin);

        QAnyStringView id() const;
        void setId(QAnyStringView id);

        QAnyStringView dataId() const;
        void setDataId(QAnyStringView cdid);

        Scope scope () const;
        void setScope (Component::Scope scope);

        QList<QAnyStringView> packageNames() const;
        void setPackageNames(QList<QAnyStringView> list);

        QAnyStringView sourcePackageName() const;
        void setSourcePackageName(QAnyStringView sourcePkg);

        QAnyStringView name() const;
        void setName(QAnyStringView name, QAnyStringView lang = {});

        QAnyStringView summary() const;
        void setSummary(QAnyStringView summary, QAnyStringView lang = {});

        QAnyStringView description() const;
        void setDescription(QAnyStringView description, QAnyStringView lang = {});

        AppStream::Launchable launchable(AppStream::Launchable::Kind kind) const;
        void addLaunchable(const AppStream::Launchable& launchable);

        QAnyStringView metadataLicense() const;
        void setMetadataLicense(QAnyStringView license);

        QAnyStringView projectLicense() const;
        void setProjectLicense(QAnyStringView license);

        QAnyStringView projectGroup() const;
        void setProjectGroup(QAnyStringView group);

        QAnyStringView developerName() const;
        void setDeveloperName(QAnyStringView developerName, QAnyStringView lang = {});

        QList<QAnyStringView> compulsoryForDesktops() const;
        void setCompulsoryForDesktop(QAnyStringView desktop);
        bool isCompulsoryForDesktop(QAnyStringView desktop) const;

        QList<QAnyStringView> categories() const;
        void addCategory(QAnyStringView category);
        bool hasCategory(QAnyStringView category) const;
        bool isMemberOfCategory(const AppStream::Category& category) const;

        QList<QAnyStringView> extends() const;
        void setExtends(QList<QAnyStringView> extends);
        void addExtends(QAnyStringView extend);

        QList<AppStream::Component> addons() const;
        void addAddon(const AppStream::Component& addon);

        QList<QAnyStringView> replaces() const;
        void addReplaces(QAnyStringView cid);

        QList<AppStream::Relation> requirements() const;
        QList<AppStream::Relation> recommends() const;
        QList<AppStream::Relation> supports() const;
        void addRelation(const AppStream::Relation &relation);

        QList<QAnyStringView> languages() const;
        int language(QAnyStringView locale) const;
        void addLanguage(QAnyStringView locale, int percentage);

        QList<AppStream::Translation> translations() const;
        void addTranslation(const AppStream::Translation& translation);

        QUrl url(UrlKind kind) const;
        void addUrl(UrlKind kind, QAnyStringView url);

        QList<AppStream::Icon> icons() const;
        AppStream::Icon icon(const QSize& size) const;
        void addIcon(const AppStream::Icon& icon);

        /**
         * \return the full list of provided entries for all kinds.
         */
        QList<AppStream::Provided> provided() const;

        /**
         * \param kind for provides
         * \return provided items for this \param kind
         */
        AppStream::Provided provided(Provided::Kind kind) const;
        void addProvided(const AppStream::Provided& provided);

        QList<AppStream::Screenshot> screenshots() const;
        void addScreenshot(const AppStream::Screenshot& screenshot);

        QList<AppStream::Release> releases() const;
        void addRelease(const AppStream::Release& release);

        bool hasBundle() const;
        QList<AppStream::Bundle> bundles() const;
        AppStream::Bundle bundle(Bundle::Kind kind) const;
        void addBundle(const AppStream::Bundle& bundle) const;

        QList<AppStream::Suggested> suggested() const;
        void addSuggested(const AppStream::Suggested& suggested);

        QList<QAnyStringView> searchTokens() const;
        uint searchMatches(QAnyStringView term) const;
        uint searchMatchesAll(QList<QAnyStringView> terms) const;

        uint sortScore() const;
        void setSortScore(uint score);

        MergeKind mergeKind() const;
        void setMergeKind(MergeKind kind);

        QHash<QString,QString> custom() const;
        QAnyStringView customValue(QAnyStringView key);
        bool insertCustomValue(QAnyStringView key, QAnyStringView value);

        QList<AppStream::ContentRating> contentRatings() const;
        AppStream::ContentRating contentRating(QAnyStringView kind) const;
        void addContentRating(const AppStream::ContentRating& contentRating);

        QAnyStringView nameVariantSuffix() const;
        void setNameVariantSuffix(QAnyStringView variantSuffix, QAnyStringView lang = {});

        bool hasTag(QAnyStringView ns, QAnyStringView tagName);
        bool addTag(QAnyStringView ns, QAnyStringView tagName);
        void removeTag(QAnyStringView ns, QAnyStringView tagName);
        void clearTags();

        bool isFree() const;
        bool isIgnored() const;
        bool isValid() const;

        QAnyStringView toString() const;

    private:
        _AsComponent *m_cpt;
};
}

#endif // APPSTREAMQT_COMPONENT_H
