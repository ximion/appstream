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
#include <QStringList>
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

            // deprecated
            UrlTranslate   [[deprecated]] = UrlKindTranslate,
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

        static Kind stringToKind(const QString& kindString);
        static QString kindToString(Kind kind);

        static UrlKind stringToUrlKind(const QString& urlKindString);
        static QString urlKindToString(AppStream::Component::UrlKind kind);

        static Scope stringToScope(const QString& scopeString);
        static QString scopeToString(AppStream::Component::Scope scope);

        Component(_AsComponent *cpt);
        Component();
        Component(const Component& other);
        Component(Component&& other);
        ~Component();

        Component& operator=(const Component& c);

        _AsComponent *asComponent() const;

        uint valueFlags() const;
        void setValueFlags(uint flags);

        QString activeLocale() const;
        void setActiveLocale(const QString& locale);

        Kind kind () const;
        void setKind (Component::Kind kind);

        QString origin() const;
        void setOrigin(const QString& origin);

        QString id() const;
        void setId(const QString& id);

        QString dataId() const;
        void setDataId(const QString& cdid);

        Scope scope () const;
        void setScope (Component::Scope scope);

        QStringList packageNames() const;
        void setPackageNames(const QStringList& list);

        QString sourcePackageName() const;
        void setSourcePackageName(const QString& sourcePkg);

        QString name() const;
        void setName(const QString& name, const QString& lang = {});

        QString summary() const;
        void setSummary(const QString& summary, const QString& lang = {});

        QString description() const;
        void setDescription(const QString& description, const QString& lang = {});

        AppStream::Launchable launchable(AppStream::Launchable::Kind kind) const;
        void addLaunchable(const AppStream::Launchable& launchable);

        QString metadataLicense() const;
        void setMetadataLicense(const QString& license);

        QString projectLicense() const;
        void setProjectLicense(const QString& license);

        QString projectGroup() const;
        void setProjectGroup(const QString& group);

        QString developerName() const;
        void setDeveloperName(const QString& developerName, const QString& lang = {});

        QStringList compulsoryForDesktops() const;
        void setCompulsoryForDesktop(const QString& desktop);
        bool isCompulsoryForDesktop(const QString& desktop) const;

        QStringList categories() const;
        void addCategory(const QString& category);
        bool hasCategory(const QString& category) const;
        bool isMemberOfCategory(const AppStream::Category& category) const;

        QStringList extends() const;
        void setExtends(const QStringList& extends);
        void addExtends(const QString& extend);

        QList<AppStream::Component> addons() const;
        void addAddon(const AppStream::Component& addon);

        QStringList replaces() const;
        void addReplaces(const QString& cid);

        QList<AppStream::Relation> requires() const;
        QList<AppStream::Relation> recommends() const;
        QList<AppStream::Relation> supports() const;
        void addRelation(const AppStream::Relation &relation);

        QStringList languages() const;
        int language(const QString& locale) const;
        void addLanguage(const QString& locale, int percentage);

        QList<AppStream::Translation> translations() const;
        void addTranslation(const AppStream::Translation& translation);

        QUrl url(UrlKind kind) const;
        void addUrl(UrlKind kind, const QString& url);

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

        QStringList searchTokens() const;
        uint searchMatches(const QString& term) const;
        uint searchMatchesAll(const QStringList& terms) const;

        uint sortScore() const;
        void setSortScore(uint score);

        MergeKind mergeKind() const;
        void setMergeKind(MergeKind kind);

        QHash<QString,QString> custom() const;
        QString customValue(const QString& key);
        bool insertCustomValue(const QString& key, const QString& value);

        QList<AppStream::ContentRating> contentRatings() const;
        AppStream::ContentRating contentRating(const QString& kind) const;
        void addContentRating(const AppStream::ContentRating& contentRating);

        QString nameVariantSuffix() const;
        void setNameVariantSuffix(const QString& variantSuffix, const QString& lang = {});

        bool hasTag(const QString &ns, const QString &tagName);
        bool addTag(const QString &ns, const QString &tagName);
        void removeTag(const QString &ns, const QString &tagName);
        void clearTags();

        bool isFree() const;
        bool isIgnored() const;
        bool isValid() const;

        QString toString() const;

        // DEPRECATED
        Q_DECL_DEPRECATED QString desktopId() const;

    private:
        _AsComponent *m_cpt;
};
}

#endif // APPSTREAMQT_COMPONENT_H
