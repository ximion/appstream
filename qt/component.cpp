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

#include "appstream.h"
#include "component.h"

#include <QSharedData>
#include <QStringList>
#include <QUrl>
#include <QMap>
#include <QMultiHash>
#include "chelpers.h"
#include "icon.h"
#include "screenshot.h"
#include "release.h"
#include "bundle.h"
#include "suggested.h"
#include "contentrating.h"
#include "launchable.h"
#include "translation.h"

using namespace AppStream;

typedef QHash<Component::Kind, QString> KindMap;
Q_GLOBAL_STATIC_WITH_ARGS(KindMap, kindMap, ( {
    { Component::KindGeneric, QLatin1String("generic") },
    { Component::KindDesktopApp, QLatin1String("desktop-application") },
    { Component::KindConsoleApp, QLatin1String("console-application") },
    { Component::KindWebApp, QLatin1String("web-application") },
    { Component::KindAddon, QLatin1String("addon") },
    { Component::KindFont, QLatin1String("font") },
    { Component::KindCodec, QLatin1String("codec") },
    { Component::KindInputmethod, QLatin1String("inputmethod") },
    { Component::KindFirmware, QLatin1String("firmware") },
    { Component::KindDriver, QLatin1String("driver") },
    { Component::KindLocalization, QLatin1String("localization") },
    { Component::KindUnknown, QLatin1String("unknown") }
    }
));

QString Component::kindToString(Component::Kind kind) {
    return kindMap->value(kind);
}

Component::Kind Component::stringToKind(const QString& kindString) {
    if(kindString.isEmpty()) {
        return KindGeneric;
    }
    if(kindString ==  QLatin1String("generic"))
        return KindGeneric;

    if (kindString == QLatin1String("desktop-application"))
        return KindDesktopApp;

    if (kindString == QLatin1String("console-application"))
        return KindConsoleApp;

    if (kindString == QLatin1String("web-application"))
        return KindWebApp;

    if (kindString == QLatin1String("addon"))
        return KindAddon;

    if (kindString == QLatin1String("font"))
        return KindFont;

    if (kindString == QLatin1String("codec"))
        return KindCodec;

    if (kindString==QLatin1String("inputmethod"))
        return KindInputmethod;

    if (kindString == QLatin1String("firmware"))
        return KindFirmware;

    if (kindString == QLatin1String("driver"))
        return KindDriver;

    if (kindString == QLatin1String("localization"))
        return KindLocalization;

    return KindUnknown;

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

typedef QHash<Component::UrlKind, QString> UrlKindMap;
Q_GLOBAL_STATIC_WITH_ARGS(UrlKindMap, urlKindMap, ({
        { Component::UrlKindBugtracker, QLatin1String("bugtracker") },
        { Component::UrlKindDonation, QLatin1String("donation") },
        { Component::UrlKindFaq, QLatin1String("faq") },
        { Component::UrlKindHelp, QLatin1String("help") },
        { Component::UrlKindHomepage, QLatin1String("homepage") },
        { Component::UrlKindUnknown, QLatin1String("unknown") },
    }));

QString Component::urlKindToString(Component::UrlKind kind) {
    return urlKindMap->value(kind);
}

Component::Component(const Component& other)
    : m_cpt(other.m_cpt)
{
    g_object_ref(m_cpt);
}

Component::Component()
{
    m_cpt = as_component_new();
}

Component::Component(_AsComponent *cpt)
    : m_cpt(cpt)
{
    g_object_ref(m_cpt);
}

Component::~Component()
{
    g_object_unref(m_cpt);
}

Component& Component::operator=(const Component& other)
{
    if (&other != this) {
        g_object_unref(m_cpt);
        m_cpt = AS_COMPONENT(g_object_ref(other.m_cpt));
    }
    return *this;
}

Component::Component(Component&& other)
    : m_cpt(other.m_cpt)
{
}

_AsComponent * AppStream::Component::asComponent() const
{
    return m_cpt;
}

uint AppStream::Component::valueFlags() const
{
    return (uint) as_component_get_value_flags(m_cpt);
}

void AppStream::Component::setValueFlags(uint flags)
{
    as_component_set_value_flags(m_cpt, (AsValueFlags) flags);
}

QString AppStream::Component::activeLocale() const
{
    return valueWrap(as_component_get_active_locale(m_cpt));
}

void AppStream::Component::setActiveLocale(const QString& locale)
{
    as_component_set_active_locale(m_cpt, qPrintable(locale));
}

Component::Kind Component::kind() const
{
    return static_cast<Component::Kind>(as_component_get_kind (m_cpt));
}

void Component::setKind(Component::Kind kind)
{
    as_component_set_kind(m_cpt, static_cast<AsComponentKind>(kind));
}

QString AppStream::Component::origin() const
{
    return valueWrap(as_component_get_origin(m_cpt));
}

void AppStream::Component::setOrigin(const QString& origin)
{
    as_component_set_origin(m_cpt, qPrintable(origin));
}

QString Component::id() const
{
    return valueWrap(as_component_get_id(m_cpt));
}

void Component::setId(const QString& id)
{
    as_component_set_id(m_cpt, qPrintable(id));
}

QString Component::dataId() const
{
    return valueWrap(as_component_get_data_id(m_cpt));
}

void Component::setDataId(const QString& cdid)
{
    as_component_set_data_id(m_cpt, qPrintable(cdid));
}

QStringList Component::packageNames() const
{
    return valueWrap(as_component_get_pkgnames(m_cpt));
}

void AppStream::Component::setPackageNames(const QStringList& list)
{
    char **packageList = stringListToCharArray(list);
    as_component_set_pkgnames(m_cpt, packageList);
    g_strfreev(packageList);
}

QString AppStream::Component::sourcePackageName() const
{
    return valueWrap(as_component_get_source_pkgname(m_cpt));
}

void AppStream::Component::setSourcePackageName(const QString& sourcePkg)
{
    as_component_set_source_pkgname(m_cpt, qPrintable(sourcePkg));
}

QString Component::name() const
{
    return valueWrap(as_component_get_name(m_cpt));
}

void Component::setName(const QString& name, const QString& lang)
{
    as_component_set_name(m_cpt, qPrintable(name), lang.isEmpty()? NULL : qPrintable(lang));
}

QString Component::summary() const
{
    return valueWrap(as_component_get_summary(m_cpt));
}

void Component::setSummary(const QString& summary, const QString& lang)
{
    as_component_set_summary(m_cpt, qPrintable(summary), lang.isEmpty()? NULL : qPrintable(lang));
}

QString Component::description() const
{
    return valueWrap(as_component_get_description(m_cpt));
}

void Component::setDescription(const QString& description, const QString& lang)
{
    as_component_set_description(m_cpt, qPrintable(description), lang.isEmpty()? NULL : qPrintable(lang));
}

AppStream::Launchable AppStream::Component::launchable(AppStream::Launchable::Kind kind) const
{
    auto launch = as_component_get_launchable(m_cpt, (AsLaunchableKind) kind);
    if (launch == NULL)
        return Launchable();
    return Launchable(launch);
}

void AppStream::Component::addLaunchable(const AppStream::Launchable& launchable)
{
    as_component_add_launchable(m_cpt, launchable.asLaunchable());
}

QString AppStream::Component::metadataLicense() const
{
    return valueWrap(as_component_get_metadata_license(m_cpt));
}

void AppStream::Component::setMetadataLicense(const QString& license)
{
    as_component_set_metadata_license(m_cpt, qPrintable(license));
}

QString Component::projectLicense() const
{
    return valueWrap(as_component_get_project_license(m_cpt));
}

void Component::setProjectLicense(const QString& license)
{
    as_component_set_project_license(m_cpt, qPrintable(license));
}

QString Component::projectGroup() const
{
    return valueWrap(as_component_get_project_group(m_cpt));
}

void Component::setProjectGroup(const QString& group)
{
    as_component_set_project_group(m_cpt, qPrintable(group));
}

QString Component::developerName() const
{
    return valueWrap(as_component_get_developer_name(m_cpt));
}

void Component::setDeveloperName(const QString& developerName, const QString& lang)
{
    as_component_set_developer_name(m_cpt, qPrintable(developerName), lang.isEmpty()? NULL : qPrintable(lang));
}

QStringList Component::compulsoryForDesktops() const
{
    return valueWrap(as_component_get_compulsory_for_desktops(m_cpt));
}

void AppStream::Component::setCompulsoryForDesktop(const QString& desktop)
{
    as_component_set_compulsory_for_desktop(m_cpt, qPrintable(desktop));
}

bool Component::isCompulsoryForDesktop(const QString& desktop) const
{
    return as_component_is_compulsory_for_desktop(m_cpt, qPrintable(desktop));
}

QStringList Component::categories() const
{
    return valueWrap(as_component_get_categories(m_cpt));
}

void AppStream::Component::addCategory(const QString& category)
{
    as_component_add_category(m_cpt, qPrintable(category));
}

bool Component::hasCategory(const QString& category) const
{
    return as_component_has_category(m_cpt, qPrintable(category));
}

QStringList Component::extends() const
{
    return valueWrap(as_component_get_extends(m_cpt));
}

void AppStream::Component::addExtends(const QString& extend)
{
    as_component_add_extends(m_cpt, qPrintable(extend));
}

QList<AppStream::Component> Component::addons() const
{
    QList<AppStream::Component> res;

    auto addons = as_component_get_addons (m_cpt);
    res.reserve(addons->len);
    for (uint i = 0; i < addons->len; i++) {
        auto cpt = AS_COMPONENT (g_ptr_array_index (addons, i));
        res.append(Component(cpt));
    }
    return res;
}

void AppStream::Component::addAddon(const AppStream::Component& addon)
{
    as_component_add_addon(m_cpt, addon.asComponent());
}

QStringList AppStream::Component::languages() const
{
    return valueWrap(as_component_get_languages(m_cpt));
}

int AppStream::Component::language(const QString& locale) const
{
    return as_component_get_language(m_cpt, qPrintable(locale));
}

void AppStream::Component::addLanguage(const QString& locale, int percentage)
{
    as_component_add_language(m_cpt, qPrintable(locale), percentage);
}

QList<AppStream::Translation> AppStream::Component::translations() const
{
    QList<Translation> res;

    auto translations = as_component_get_translations(m_cpt);
    res.reserve(translations->len);
    for (uint i = 0; i < translations->len; i++) {
        auto translation = AS_TRANSLATION (g_ptr_array_index (translations, i));
        res.append(Translation(translation));
    }
    return res;
}

void AppStream::Component::addTranslation(const AppStream::Translation& translation)
{
    as_component_add_translation(m_cpt, translation.asTranslation());
}

QUrl Component::url(Component::UrlKind kind) const
{
    auto url = as_component_get_url(m_cpt, static_cast<AsUrlKind>(kind));
    if (url == NULL)
        return QUrl();
    return QUrl(url);
}

void AppStream::Component::addUrl(AppStream::Component::UrlKind kind, const QString& url)
{
    as_component_add_url(m_cpt, (AsUrlKind) kind, qPrintable(url));
}

QList<Icon> Component::icons() const
{
    QList<Icon> res;

    auto icons = as_component_get_icons(m_cpt);
    res.reserve(icons->len);
    for (uint i = 0; i < icons->len; i++) {
        auto icon = AS_ICON (g_ptr_array_index (icons, i));
        res.append(Icon(icon));
    }
    return res;
}

Icon Component::icon(const QSize& size) const
{
    auto res = as_component_get_icon_by_size (m_cpt, size.width(), size.height());
    if (res == NULL)
        return Icon();
    return Icon(res);
}

void AppStream::Component::addIcon(const AppStream::Icon& icon)
{
    as_component_add_icon(m_cpt, icon.asIcon());
}

QList<Provided> Component::provided() const
{
    QList<Provided> res;

    auto provEntries = as_component_get_provided(m_cpt);
    res.reserve(provEntries->len);
    for (uint i = 0; i < provEntries->len; i++) {
        auto prov = AS_PROVIDED (g_ptr_array_index (provEntries, i));
        res.append(Provided(prov));
    }
    return res;
}

AppStream::Provided Component::provided(Provided::Kind kind) const
{
    auto prov = as_component_get_provided_for_kind(m_cpt, (AsProvidedKind) kind);
    if (prov == NULL)
        return Provided();
    return Provided(prov);
}

void AppStream::Component::addProvided(const AppStream::Provided& provided)
{
    as_component_add_provided(m_cpt, provided.asProvided());
}

QList<Screenshot> Component::screenshots() const
{
    QList<Screenshot> res;

    auto screenshots = as_component_get_screenshots(m_cpt);
    res.reserve(screenshots->len);
    for (uint i = 0; i < screenshots->len; i++) {
        auto scr = AS_SCREENSHOT (g_ptr_array_index (screenshots, i));
        res.append(Screenshot(scr));
    }
    return res;
}

void AppStream::Component::addScreenshot(const AppStream::Screenshot& screenshot)
{
    as_component_add_screenshot(m_cpt, screenshot.asScreenshot());
}

QList<Release> Component::releases() const
{
    QList<Release> res;

    auto rels = as_component_get_releases(m_cpt);
    res.reserve(rels->len);
    for (uint i = 0; i < rels->len; i++) {
        auto rel = AS_RELEASE (g_ptr_array_index (rels, i));
        res.append(Release(rel));
    }
    return res;
}

void AppStream::Component::addRelease(const AppStream::Release& release)
{
    as_component_add_release(m_cpt, release.asRelease());
}

bool AppStream::Component::hasBundle() const
{
    return as_component_has_bundle(m_cpt);
}

QList<Bundle> Component::bundles() const
{
    QList<Bundle> res;

    auto bdls = as_component_get_bundles(m_cpt);
    res.reserve(bdls->len);
    for (uint i = 0; i < bdls->len; i++) {
        auto bundle = AS_BUNDLE (g_ptr_array_index (bdls, i));
        res.append(Bundle(bundle));
    }
    return res;
}

Bundle Component::bundle(Bundle::Kind kind) const
{
    auto bundle = as_component_get_bundle(m_cpt, (AsBundleKind) kind);
    if (bundle == NULL)
        return Bundle();
    return Bundle(bundle);
}

void AppStream::Component::addBundle(const AppStream::Bundle& bundle) const
{
    as_component_add_bundle(m_cpt, bundle.asBundle());
}

QList<AppStream::Suggested> AppStream::Component::suggested() const
{
    QList<Suggested> res;

    auto suggestions = as_component_get_suggested(m_cpt);
    res.reserve(suggestions->len);
    for (uint i = 0; i < suggestions->len; i++) {
        auto suggestion = AS_SUGGESTED (g_ptr_array_index (suggestions, i));
        res.append(Suggested(suggestion));
    }
    return res;
}

void AppStream::Component::addSuggested(const AppStream::Suggested& suggested)
{
    as_component_add_suggested(m_cpt, suggested.suggested());
}

QStringList AppStream::Component::searchTokens() const
{
    return valueWrap(as_component_get_search_tokens(m_cpt));
}

uint AppStream::Component::searchMatches(const QString& term) const
{
    return as_component_search_matches(m_cpt, qPrintable(term));
}

uint AppStream::Component::searchMatchesAll(const QStringList& terms) const
{
    char **termList = stringListToCharArray(terms);
    const uint searchMatches = as_component_search_matches_all(m_cpt, termList);
    g_strfreev(termList);
    return searchMatches;
}

AppStream::Component::MergeKind AppStream::Component::mergeKind() const
{
    return static_cast<Component::MergeKind>(as_component_get_merge_kind(m_cpt));
}

void AppStream::Component::setMergeKind(AppStream::Component::MergeKind kind)
{
    as_component_set_merge_kind(m_cpt, (AsMergeKind) kind);
}

QHash<QString, QString> AppStream::Component::custom() const
{
    QHash<QString, QString> result;
    GHashTableIter iter;
    gpointer key, value;

    auto custom = as_component_get_custom(m_cpt);
    g_hash_table_iter_init(&iter, custom);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        result.insert(valueWrap(static_cast<char*>(key)), valueWrap(static_cast<char*>(value)));
    }
    return result;
}

QString AppStream::Component::customValue(const QString& key)
{
    return valueWrap(as_component_get_custom_value(m_cpt, qPrintable(key)));
}

bool AppStream::Component::insertCustomValue(const QString& key, const QString& value)
{
    return as_component_insert_custom_value(m_cpt, qPrintable(key), qPrintable(value));
}

QList<AppStream::ContentRating> AppStream::Component::contentRatings() const
{
    QList<ContentRating> res;

    auto ratings = as_component_get_content_ratings(m_cpt);
    res.reserve(ratings->len);
    for (uint i = 0; i < ratings->len; i++) {
        auto rating = AS_CONTENT_RATING (g_ptr_array_index (ratings, i));
        res.append(ContentRating(rating));
    }
    return res;
}

AppStream::ContentRating AppStream::Component::contentRating(const QString& kind) const
{
    auto rating = as_component_get_content_rating(m_cpt, qPrintable(kind));
    if (rating == NULL)
        return ContentRating();
    return ContentRating(rating);
}

void AppStream::Component::addContentRating(const AppStream::ContentRating& contentRating)
{
    as_component_add_content_rating(m_cpt, contentRating.asContentRating());
}

bool AppStream::Component::isMemberOfCategory(const AppStream::Category& category) const
{
    return as_component_is_member_of_category(m_cpt, category.asCategory());
}

bool AppStream::Component::isIgnored() const
{
    return as_component_is_ignored(m_cpt);
}

bool Component::isValid() const
{
    return as_component_is_valid(m_cpt);
}

QString AppStream::Component::toString() const
{
    return valueWrap(as_component_to_string(m_cpt));
}

QString Component::desktopId() const
{
    auto de_launchable = as_component_get_launchable (m_cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
    if (de_launchable == NULL)
        return QString();

    auto entries = as_launchable_get_entries (de_launchable);
    if (entries->len <= 0)
        return QString();

    auto desktop_id = (const gchar*) g_ptr_array_index (entries, 0);
    return QString::fromUtf8(desktop_id);
}
