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
#include "relation.h"
#include "bundle.h"
#include "suggested.h"
#include "contentrating.h"
#include "launchable.h"
#include "translation.h"

using namespace AppStream;

QAnyStringView Component::kindToString(Component::Kind kind)
{
    return valueWrap(as_component_kind_to_string(static_cast<AsComponentKind>(kind)));
}

Component::Kind Component::stringToKind(QAnyStringView kindString)
{
    if(kindString.isEmpty()) {
        return KindGeneric;
    }
    return static_cast<Component::Kind>(as_component_kind_from_string(stringViewToChar(kindString)));
}

QAnyStringView Component::urlKindToString(Component::UrlKind kind)
{
    return valueWrap(as_url_kind_to_string(static_cast<AsUrlKind>(kind)));
}

Component::UrlKind Component::stringToUrlKind(QAnyStringView urlKindString)
{
    return static_cast<Component::UrlKind>(as_url_kind_from_string(stringViewToChar(urlKindString)));
}

QAnyStringView Component::scopeToString(Component::Scope scope)
{
    return valueWrap(as_component_scope_to_string(static_cast<AsComponentScope>(scope)));
}

Component::Scope Component::stringToScope(QAnyStringView scopeString)
{
    return static_cast<Component::Scope>(as_component_scope_from_string(stringViewToChar(scopeString)));
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
    g_object_ref(other.m_cpt);
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

QAnyStringView AppStream::Component::activeLocale() const
{
    return valueWrap(as_component_get_active_locale(m_cpt));
}

void AppStream::Component::setActiveLocale(QAnyStringView locale)
{
    as_component_set_active_locale(m_cpt, stringViewToChar(locale));
}

Component::Kind Component::kind() const
{
    return static_cast<Component::Kind>(as_component_get_kind (m_cpt));
}

void Component::setKind(Component::Kind kind)
{
    as_component_set_kind(m_cpt, static_cast<AsComponentKind>(kind));
}

QAnyStringView AppStream::Component::origin() const
{
    return valueWrap(as_component_get_origin(m_cpt));
}

void AppStream::Component::setOrigin(QAnyStringView origin)
{
    as_component_set_origin(m_cpt, stringViewToChar(origin));
}

QAnyStringView Component::id() const
{
    return valueWrap(as_component_get_id(m_cpt));
}

void Component::setId(QAnyStringView id)
{
    as_component_set_id(m_cpt, stringViewToChar(id));
}

QAnyStringView Component::dataId() const
{
    return valueWrap(as_component_get_data_id(m_cpt));
}

void Component::setDataId(QAnyStringView cdid)
{
    as_component_set_data_id(m_cpt, stringViewToChar(cdid));
}

Component::Scope Component::scope() const
{
    return static_cast<Component::Scope>(as_component_get_scope (m_cpt));
}

void Component::setScope(Component::Scope scope)
{
    as_component_set_scope(m_cpt, static_cast<AsComponentScope>(scope));
}

QList<QAnyStringView> Component::packageNames() const
{
    return valueWrap(as_component_get_pkgnames(m_cpt));
}

void AppStream::Component::setPackageNames(QList<QAnyStringView> list)
{
    char **packageList = stringListToCharArray(list);
    as_component_set_pkgnames(m_cpt, packageList);
    g_strfreev(packageList);
}

QAnyStringView AppStream::Component::sourcePackageName() const
{
    return valueWrap(as_component_get_source_pkgname(m_cpt));
}

void AppStream::Component::setSourcePackageName(QAnyStringView sourcePkg)
{
    as_component_set_source_pkgname(m_cpt, stringViewToChar(sourcePkg));
}

QAnyStringView Component::name() const
{
    return valueWrap(as_component_get_name(m_cpt));
}

void Component::setName(QAnyStringView name, QAnyStringView lang)
{
    as_component_set_name(m_cpt, stringViewToChar(name), lang.isEmpty()? NULL : stringViewToChar(lang));
}

QAnyStringView Component::summary() const
{
    return valueWrap(as_component_get_summary(m_cpt));
}

void Component::setSummary(QAnyStringView summary, QAnyStringView lang)
{
    as_component_set_summary(m_cpt, stringViewToChar(summary), lang.isEmpty()? NULL : stringViewToChar(lang));
}

QAnyStringView Component::description() const
{
    return valueWrap(as_component_get_description(m_cpt));
}

void Component::setDescription(QAnyStringView description, QAnyStringView lang)
{
    as_component_set_description(m_cpt, stringViewToChar(description), lang.isEmpty()? NULL : stringViewToChar(lang));
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

QAnyStringView AppStream::Component::metadataLicense() const
{
    return valueWrap(as_component_get_metadata_license(m_cpt));
}

void AppStream::Component::setMetadataLicense(QAnyStringView license)
{
    as_component_set_metadata_license(m_cpt, stringViewToChar(license));
}

QAnyStringView Component::projectLicense() const
{
    return valueWrap(as_component_get_project_license(m_cpt));
}

void Component::setProjectLicense(QAnyStringView license)
{
    as_component_set_project_license(m_cpt, stringViewToChar(license));
}

QAnyStringView Component::projectGroup() const
{
    return valueWrap(as_component_get_project_group(m_cpt));
}

void Component::setProjectGroup(QAnyStringView group)
{
    as_component_set_project_group(m_cpt, stringViewToChar(group));
}

QAnyStringView Component::developerName() const
{
    return valueWrap(as_component_get_developer_name(m_cpt));
}

void Component::setDeveloperName(QAnyStringView developerName, QAnyStringView lang)
{
    as_component_set_developer_name(m_cpt, stringViewToChar(developerName), lang.isEmpty()? NULL : stringViewToChar(lang));
}

QList<QAnyStringView> Component::compulsoryForDesktops() const
{
    return valueWrap(as_component_get_compulsory_for_desktops(m_cpt));
}

void AppStream::Component::setCompulsoryForDesktop(QAnyStringView desktop)
{
    as_component_set_compulsory_for_desktop(m_cpt, stringViewToChar(desktop));
}

bool Component::isCompulsoryForDesktop(QAnyStringView desktop) const
{
    return as_component_is_compulsory_for_desktop(m_cpt, stringViewToChar(desktop));
}

QList<QAnyStringView> Component::categories() const
{
    return valueWrap(as_component_get_categories(m_cpt));
}

void AppStream::Component::addCategory(QAnyStringView category)
{
    as_component_add_category(m_cpt, stringViewToChar(category));
}

bool Component::hasCategory(QAnyStringView category) const
{
    return as_component_has_category(m_cpt, stringViewToChar(category));
}

bool AppStream::Component::isMemberOfCategory(const AppStream::Category& category) const
{
    return as_component_is_member_of_category(m_cpt, category.asCategory());
}

QList<QAnyStringView> Component::extends() const
{
    return valueWrap(as_component_get_extends(m_cpt));
}

void AppStream::Component::addExtends(QAnyStringView extend)
{
    as_component_add_extends(m_cpt, stringViewToChar(extend));
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

QList<QAnyStringView> Component::replaces() const
{
    return valueWrap(as_component_get_replaces(m_cpt));
}

void Component::addReplaces(QAnyStringView cid)
{
    as_component_add_replaces(m_cpt, stringViewToChar(cid));
}

QList<Relation> Component::requirements() const
{
    QList<AppStream::Relation> res;

    auto reqs = as_component_get_requires(m_cpt);
    res.reserve(reqs->len);
    for (uint i = 0; i < reqs->len; i++) {
        auto rel = AS_RELATION (g_ptr_array_index(reqs, i));
        res.append(Relation(rel));
    }
    return res;
}

QList<Relation> Component::recommends() const
{
    QList<AppStream::Relation> res;

    auto recommends = as_component_get_recommends(m_cpt);
    res.reserve(recommends->len);
    for (uint i = 0; i < recommends->len; i++) {
        auto rel = AS_RELATION (g_ptr_array_index(recommends, i));
        res.append(Relation(rel));
    }
    return res;
}

QList<Relation> Component::supports() const
{
    QList<AppStream::Relation> res;

    auto supports = as_component_get_supports(m_cpt);
    res.reserve(supports->len);
    for (uint i = 0; i < supports->len; i++) {
        auto rel = AS_RELATION (g_ptr_array_index(supports, i));
        res.append(Relation(rel));
    }
    return res;
}

void Component::addRelation(const Relation &relation)
{
    as_component_add_relation(m_cpt, relation.asRelation());
}

QList<QAnyStringView> AppStream::Component::languages() const
{
    return valueWrap(as_component_get_languages(m_cpt));
}

int AppStream::Component::language(QAnyStringView locale) const
{
    return as_component_get_language(m_cpt, stringViewToChar(locale));
}

void AppStream::Component::addLanguage(QAnyStringView locale, int percentage)
{
    as_component_add_language(m_cpt, stringViewToChar(locale), percentage);
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

void AppStream::Component::addUrl(AppStream::Component::UrlKind kind, QAnyStringView url)
{
    as_component_add_url(m_cpt, (AsUrlKind) kind, stringViewToChar(url));
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

QList<QAnyStringView> AppStream::Component::searchTokens() const
{
    return valueWrap(as_component_get_search_tokens(m_cpt));
}

uint AppStream::Component::searchMatches(QAnyStringView term) const
{
    return as_component_search_matches(m_cpt, stringViewToChar(term));
}

uint AppStream::Component::searchMatchesAll(QList<QAnyStringView> terms) const
{
    char **termList = stringListToCharArray(terms);
    const uint searchMatches = as_component_search_matches_all(m_cpt, termList);
    g_strfreev(termList);
    return searchMatches;
}

uint AppStream::Component::sortScore() const
{
    return as_component_get_sort_score(m_cpt);
}

void AppStream::Component::setSortScore(uint score)
{
    as_component_set_sort_score(m_cpt, score);
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
        result.insert(QString::fromUtf8(static_cast<char*>(key)), QString::fromUtf8(static_cast<char*>(value)));
    }
    return result;
}

QAnyStringView AppStream::Component::customValue(QAnyStringView key)
{
    return valueWrap(as_component_get_custom_value(m_cpt, stringViewToChar(key)));
}

bool AppStream::Component::insertCustomValue(QAnyStringView key, QAnyStringView value)
{
    return as_component_insert_custom_value(m_cpt, stringViewToChar(key), stringViewToChar(value));
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

AppStream::ContentRating AppStream::Component::contentRating(QAnyStringView kind) const
{
    auto rating = as_component_get_content_rating(m_cpt, stringViewToChar(kind));
    if (rating == NULL)
        return ContentRating();
    return ContentRating(rating);
}

void AppStream::Component::addContentRating(const AppStream::ContentRating& contentRating)
{
    as_component_add_content_rating(m_cpt, contentRating.asContentRating());
}

QAnyStringView Component::nameVariantSuffix() const
{
    return valueWrap(as_component_get_name_variant_suffix(m_cpt));
}

void Component::setNameVariantSuffix(QAnyStringView variantSuffix, QAnyStringView lang)
{
    as_component_set_name_variant_suffix(m_cpt, stringViewToChar(variantSuffix), lang.isEmpty()? NULL : stringViewToChar(lang));
}

bool Component::hasTag(QAnyStringView ns, QAnyStringView tagName)
{
    return as_component_has_tag(m_cpt, stringViewToChar(ns), stringViewToChar(tagName));
}

bool Component::addTag(QAnyStringView ns, QAnyStringView tagName)
{
    return as_component_add_tag(m_cpt, stringViewToChar(ns), stringViewToChar(tagName));
}

void Component::removeTag(QAnyStringView ns, QAnyStringView tagName)
{
    as_component_remove_tag(m_cpt, stringViewToChar(ns), stringViewToChar(tagName));
}

void Component::clearTags()
{
    as_component_clear_tags(m_cpt);
}

bool Component::isFree() const
{
    return as_component_is_free(m_cpt);
}

bool AppStream::Component::isIgnored() const
{
    return as_component_is_ignored(m_cpt);
}

bool Component::isValid() const
{
    return as_component_is_valid(m_cpt);
}

QAnyStringView AppStream::Component::toString() const
{
    return valueWrap(as_component_to_string(m_cpt));
}

