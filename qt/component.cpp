/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "developer.h"
#include "relation.h"
#include "bundle.h"
#include "suggested.h"
#include "contentrating.h"
#include "launchable.h"
#include "translation.h"
#include "systeminfo.h"
#include "pool.h"

using namespace AppStream;

QString Component::kindToString(Component::Kind kind)
{
    return QString::fromUtf8(as_component_kind_to_string(static_cast<AsComponentKind>(kind)));
}

Component::Kind Component::stringToKind(const QString &kindString)
{
    if (kindString.isEmpty()) {
        return KindGeneric;
    }
    return static_cast<Component::Kind>(as_component_kind_from_string(qPrintable(kindString)));
}

QString Component::urlKindToString(Component::UrlKind kind)
{
    return QString::fromUtf8(as_url_kind_to_string(static_cast<AsUrlKind>(kind)));
}

Component::UrlKind Component::stringToUrlKind(const QString &urlKindString)
{
    return static_cast<Component::UrlKind>(as_url_kind_from_string(qPrintable(urlKindString)));
}

QString Component::scopeToString(Component::Scope scope)
{
    return QString::fromUtf8(as_component_scope_to_string(static_cast<AsComponentScope>(scope)));
}

Component::Scope Component::stringToScope(const QString &scopeString)
{
    return static_cast<Component::Scope>(as_component_scope_from_string(qPrintable(scopeString)));
}

class AppStream::ComponentData : public QSharedData
{
public:
    ComponentData(AsComponent *ncpt)
        : cpt(ncpt)
    {
        if (ncpt == nullptr)
            cpt = as_component_new();
        else
            g_object_ref(cpt);
    }

    ~ComponentData()
    {
        g_object_unref(cpt);
    }

    bool operator==(const ComponentData &cd) const
    {
        return cd.cpt == cpt;
    }

    AsComponent *component() const
    {
        return cpt;
    }

    AsComponent *cpt;
    QString lastError;
};

Component::Component()
    : d(new ComponentData(nullptr))
{
}

Component::Component(_AsComponent *cpt)
    : d(new ComponentData(cpt))
{
}

Component::Component(const Component &cpt) = default;

Component::~Component() = default;

Component &Component::operator=(const Component &cpt) = default;

bool Component::operator==(const Component &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsComponent *AppStream::Component::cPtr() const
{
    return d->cpt;
}

Component::Kind Component::kind() const
{
    return static_cast<Component::Kind>(as_component_get_kind(d->cpt));
}

void Component::setKind(Component::Kind kind)
{
    as_component_set_kind(d->cpt, static_cast<AsComponentKind>(kind));
}

QString AppStream::Component::origin() const
{
    return valueWrap(as_component_get_origin(d->cpt));
}

void AppStream::Component::setOrigin(const QString &origin)
{
    as_component_set_origin(d->cpt, qPrintable(origin));
}

QString Component::id() const
{
    return valueWrap(as_component_get_id(d->cpt));
}

void Component::setId(const QString &id)
{
    as_component_set_id(d->cpt, qPrintable(id));
}

QString Component::dataId() const
{
    return valueWrap(as_component_get_data_id(d->cpt));
}

void Component::setDataId(const QString &cdid)
{
    as_component_set_data_id(d->cpt, qPrintable(cdid));
}

Component::Scope Component::scope() const
{
    return static_cast<Component::Scope>(as_component_get_scope(d->cpt));
}

void Component::setScope(Component::Scope scope)
{
    as_component_set_scope(d->cpt, static_cast<AsComponentScope>(scope));
}

QStringList Component::packageNames() const
{
    return valueWrap(as_component_get_pkgnames(d->cpt));
}

void AppStream::Component::setPackageNames(const QStringList &list)
{
    char **packageList = stringListToCharArray(list);
    as_component_set_pkgnames(d->cpt, packageList);
    g_strfreev(packageList);
}

QString AppStream::Component::sourcePackageName() const
{
    return valueWrap(as_component_get_source_pkgname(d->cpt));
}

void AppStream::Component::setSourcePackageName(const QString &sourcePkg)
{
    as_component_set_source_pkgname(d->cpt, qPrintable(sourcePkg));
}

QString Component::name() const
{
    return valueWrap(as_component_get_name(d->cpt));
}

void Component::setName(const QString &name, const QString &lang)
{
    as_component_set_name(d->cpt, qPrintable(name), lang.isEmpty() ? NULL : qPrintable(lang));
}

QString Component::summary() const
{
    return valueWrap(as_component_get_summary(d->cpt));
}

void Component::setSummary(const QString &summary, const QString &lang)
{
    as_component_set_summary(d->cpt, qPrintable(summary), lang.isEmpty() ? NULL : qPrintable(lang));
}

QString Component::description() const
{
    return valueWrap(as_component_get_description(d->cpt));
}

void Component::setDescription(const QString &description, const QString &lang)
{
    as_component_set_description(d->cpt,
                                 qPrintable(description),
                                 lang.isEmpty() ? NULL : qPrintable(lang));
}

AppStream::Launchable AppStream::Component::launchable(AppStream::Launchable::Kind kind) const
{
    auto launch = as_component_get_launchable(d->cpt, (AsLaunchableKind) kind);
    if (launch == NULL)
        return Launchable();
    return Launchable(launch);
}

void AppStream::Component::addLaunchable(const AppStream::Launchable &launchable)
{
    as_component_add_launchable(d->cpt, launchable.cPtr());
}

QString AppStream::Component::metadataLicense() const
{
    return valueWrap(as_component_get_metadata_license(d->cpt));
}

void AppStream::Component::setMetadataLicense(const QString &license)
{
    as_component_set_metadata_license(d->cpt, qPrintable(license));
}

QString Component::projectLicense() const
{
    return valueWrap(as_component_get_project_license(d->cpt));
}

void Component::setProjectLicense(const QString &license)
{
    as_component_set_project_license(d->cpt, qPrintable(license));
}

QString Component::projectGroup() const
{
    return valueWrap(as_component_get_project_group(d->cpt));
}

void Component::setProjectGroup(const QString &group)
{
    as_component_set_project_group(d->cpt, qPrintable(group));
}

Developer Component::developer() const
{
    return Developer(as_component_get_developer(d->cpt));
}

void Component::setDeveloper(const Developer &developer)
{
    as_component_set_developer(d->cpt, developer.cPtr());
}

QStringList Component::compulsoryForDesktops() const
{
    return valueWrap(as_component_get_compulsory_for_desktops(d->cpt));
}

void AppStream::Component::setCompulsoryForDesktop(const QString &desktop)
{
    as_component_set_compulsory_for_desktop(d->cpt, qPrintable(desktop));
}

bool Component::isCompulsoryForDesktop(const QString &desktop) const
{
    return as_component_is_compulsory_for_desktop(d->cpt, qPrintable(desktop));
}

QStringList Component::categories() const
{
    return valueWrap(as_component_get_categories(d->cpt));
}

void AppStream::Component::addCategory(const QString &category)
{
    as_component_add_category(d->cpt, qPrintable(category));
}

bool Component::hasCategory(const QString &category) const
{
    return as_component_has_category(d->cpt, qPrintable(category));
}

bool AppStream::Component::isMemberOfCategory(const AppStream::Category &category) const
{
    return as_component_is_member_of_category(d->cpt, category.cPtr());
}

QStringList Component::extends() const
{
    return valueWrap(as_component_get_extends(d->cpt));
}

void AppStream::Component::addExtends(const QString &extend)
{
    as_component_add_extends(d->cpt, qPrintable(extend));
}

QList<AppStream::Component> Component::addons() const
{
    QList<AppStream::Component> res;

    auto addons = as_component_get_addons(d->cpt);
    res.reserve(addons->len);
    for (uint i = 0; i < addons->len; i++) {
        auto cpt = AS_COMPONENT(g_ptr_array_index(addons, i));
        res.append(Component(cpt));
    }
    return res;
}

void AppStream::Component::addAddon(const AppStream::Component &addon)
{
    as_component_add_addon(d->cpt, addon.cPtr());
}

QStringList Component::replaces() const
{
    return valueWrap(as_component_get_replaces(d->cpt));
}

void Component::addReplaces(const QString &cid)
{
    as_component_add_replaces(d->cpt, qPrintable(cid));
}

QList<Relation> Component::requirements() const
{
    QList<AppStream::Relation> res;

    auto reqs = as_component_get_requires(d->cpt);
    res.reserve(reqs->len);
    for (uint i = 0; i < reqs->len; i++) {
        auto rel = AS_RELATION(g_ptr_array_index(reqs, i));
        res.append(Relation(rel));
    }
    return res;
}

QList<Relation> Component::recommends() const
{
    QList<AppStream::Relation> res;

    auto recommends = as_component_get_recommends(d->cpt);
    res.reserve(recommends->len);
    for (uint i = 0; i < recommends->len; i++) {
        auto rel = AS_RELATION(g_ptr_array_index(recommends, i));
        res.append(Relation(rel));
    }
    return res;
}

QList<Relation> Component::supports() const
{
    QList<AppStream::Relation> res;

    auto supports = as_component_get_supports(d->cpt);
    res.reserve(supports->len);
    for (uint i = 0; i < supports->len; i++) {
        auto rel = AS_RELATION(g_ptr_array_index(supports, i));
        res.append(Relation(rel));
    }
    return res;
}

void Component::addRelation(const Relation &relation)
{
    as_component_add_relation(d->cpt, relation.cPtr());
}

QList<RelationCheckResult>
Component::checkRelations(SystemInfo *sysinfo, Pool *pool, Relation::Kind relKind)
{
    g_autoptr(GPtrArray) cresult = NULL;
    cresult = as_component_check_relations(d->cpt,
                                           sysinfo == nullptr ? nullptr : sysinfo->cPtr(),
                                           pool == nullptr ? nullptr : pool->cPtr(),
                                           static_cast<AsRelationKind>(relKind));

    QList<RelationCheckResult> res;
    res.reserve(cresult->len);
    for (guint i = 0; i < cresult->len; i++)
        res.append(RelationCheckResult(AS_RELATION_CHECK_RESULT(g_ptr_array_index(cresult, i))));

    return res;
}

int Component::calculateSystemCompatibilityScore(SystemInfo *sysinfo,
                                                 bool isTemplate,
                                                 QList<RelationCheckResult> &results)
{
    g_autoptr(GPtrArray) cres = NULL;
    int score;

    score = as_component_get_system_compatibility_score(d->cpt, sysinfo->cPtr(), isTemplate, &cres);

    results.reserve(cres->len);
    for (guint i = 0; i < cres->len; i++)
        results.append(RelationCheckResult(AS_RELATION_CHECK_RESULT(g_ptr_array_index(cres, i))));

    return score;
}

int Component::calculateSystemCompatibilityScore(SystemInfo *sysinfo, bool isTemplate)
{
    return as_component_get_system_compatibility_score(d->cpt,
                                                       sysinfo->cPtr(),
                                                       isTemplate,
                                                       nullptr);
}

QStringList AppStream::Component::languages() const
{
    return valueWrap(as_component_get_languages(d->cpt));
}

int AppStream::Component::language(const QString &locale) const
{
    return as_component_get_language(d->cpt, qPrintable(locale));
}

void AppStream::Component::addLanguage(const QString &locale, int percentage)
{
    as_component_add_language(d->cpt, qPrintable(locale), percentage);
}

QList<AppStream::Translation> AppStream::Component::translations() const
{
    QList<Translation> res;

    auto translations = as_component_get_translations(d->cpt);
    res.reserve(translations->len);
    for (uint i = 0; i < translations->len; i++) {
        auto translation = AS_TRANSLATION(g_ptr_array_index(translations, i));
        res.append(Translation(translation));
    }
    return res;
}

void AppStream::Component::addTranslation(const AppStream::Translation &translation)
{
    as_component_add_translation(d->cpt, translation.cPtr());
}

QUrl Component::url(Component::UrlKind kind) const
{
    auto url = as_component_get_url(d->cpt, static_cast<AsUrlKind>(kind));
    if (url == NULL)
        return QUrl();
    return QUrl(url);
}

void AppStream::Component::addUrl(AppStream::Component::UrlKind kind, const QString &url)
{
    as_component_add_url(d->cpt, (AsUrlKind) kind, qPrintable(url));
}

QList<Icon> Component::icons() const
{
    QList<Icon> res;

    auto icons = as_component_get_icons(d->cpt);
    res.reserve(icons->len);
    for (uint i = 0; i < icons->len; i++) {
        auto icon = AS_ICON(g_ptr_array_index(icons, i));
        res.append(Icon(icon));
    }
    return res;
}

Icon Component::icon(const QSize &size) const
{
    auto res = as_component_get_icon_by_size(d->cpt, size.width(), size.height());
    if (res == NULL)
        return Icon();
    return Icon(res);
}

void AppStream::Component::addIcon(const AppStream::Icon &icon)
{
    as_component_add_icon(d->cpt, icon.cPtr());
}

QList<Provided> Component::provided() const
{
    QList<Provided> res;

    auto provEntries = as_component_get_provided(d->cpt);
    res.reserve(provEntries->len);
    for (uint i = 0; i < provEntries->len; i++) {
        auto prov = AS_PROVIDED(g_ptr_array_index(provEntries, i));
        res.append(Provided(prov));
    }
    return res;
}

AppStream::Provided Component::provided(Provided::Kind kind) const
{
    auto prov = as_component_get_provided_for_kind(d->cpt, (AsProvidedKind) kind);
    if (prov == NULL)
        return Provided();
    return Provided(prov);
}

void AppStream::Component::addProvided(const AppStream::Provided &provided)
{
    as_component_add_provided(d->cpt, provided.cPtr());
}

void Component::sortScreenshots(const QString &environment,
                                const QString &style,
                                bool prioritizeStyle)
{
    as_component_sort_screenshots(d->cpt,
                                  qPrintable(environment),
                                  qPrintable(style),
                                  prioritizeStyle);
}

QList<Screenshot> Component::screenshotsAll() const
{
    QList<Screenshot> res;

    auto screenshots = as_component_get_screenshots_all(d->cpt);
    res.reserve(screenshots->len);
    for (uint i = 0; i < screenshots->len; i++) {
        auto scr = AS_SCREENSHOT(g_ptr_array_index(screenshots, i));
        res.append(Screenshot(scr));
    }
    return res;
}

void AppStream::Component::addScreenshot(const AppStream::Screenshot &screenshot)
{
    as_component_add_screenshot(d->cpt, screenshot.cPtr());
}

ReleaseList Component::releasesPlain() const
{
    return ReleaseList(as_component_get_releases_plain(d->cpt));
}

std::optional<ReleaseList> Component::loadReleases(bool allowNet)
{
    g_autoptr(GError) error = nullptr;
    std::optional<ReleaseList> result;

    auto rels = as_component_load_releases(d->cpt, allowNet, &error);
    if (rels == nullptr) {
        d->lastError = QString::fromUtf8(error->message);
        return result;
    }
    result = ReleaseList(rels);
    return result;
}

void Component::setReleases(const ReleaseList &releases)
{
    as_component_set_releases(d->cpt, releases.cPtr());
}

void AppStream::Component::addRelease(const AppStream::Release &release)
{
    as_component_add_release(d->cpt, release.cPtr());
}

bool AppStream::Component::hasBundle() const
{
    return as_component_has_bundle(d->cpt);
}

QList<Bundle> Component::bundles() const
{
    QList<Bundle> res;

    auto bdls = as_component_get_bundles(d->cpt);
    res.reserve(bdls->len);
    for (uint i = 0; i < bdls->len; i++) {
        auto bundle = AS_BUNDLE(g_ptr_array_index(bdls, i));
        res.append(Bundle(bundle));
    }
    return res;
}

Bundle Component::bundle(Bundle::Kind kind) const
{
    auto bundle = as_component_get_bundle(d->cpt, (AsBundleKind) kind);
    if (bundle == NULL)
        return Bundle();
    return Bundle(bundle);
}

void AppStream::Component::addBundle(const AppStream::Bundle &bundle) const
{
    as_component_add_bundle(d->cpt, bundle.cPtr());
}

QList<AppStream::Suggested> AppStream::Component::suggested() const
{
    QList<Suggested> res;

    auto suggestions = as_component_get_suggested(d->cpt);
    res.reserve(suggestions->len);
    for (uint i = 0; i < suggestions->len; i++) {
        auto suggestion = AS_SUGGESTED(g_ptr_array_index(suggestions, i));
        res.append(Suggested(suggestion));
    }
    return res;
}

void AppStream::Component::addSuggested(const AppStream::Suggested &suggested)
{
    as_component_add_suggested(d->cpt, suggested.cPtr());
}

QStringList AppStream::Component::searchTokens() const
{
    return valueWrap(as_component_get_search_tokens(d->cpt));
}

uint AppStream::Component::searchMatches(const QString &term) const
{
    return as_component_search_matches(d->cpt, qPrintable(term));
}

uint AppStream::Component::searchMatchesAll(const QStringList &terms) const
{
    char **termList = stringListToCharArray(terms);
    const uint searchMatches = as_component_search_matches_all(d->cpt, termList);
    g_strfreev(termList);
    return searchMatches;
}

uint AppStream::Component::sortScore() const
{
    return as_component_get_sort_score(d->cpt);
}

void AppStream::Component::setSortScore(uint score)
{
    as_component_set_sort_score(d->cpt, score);
}

AppStream::Component::MergeKind AppStream::Component::mergeKind() const
{
    return static_cast<Component::MergeKind>(as_component_get_merge_kind(d->cpt));
}

void AppStream::Component::setMergeKind(AppStream::Component::MergeKind kind)
{
    as_component_set_merge_kind(d->cpt, (AsMergeKind) kind);
}

QHash<QString, QString> AppStream::Component::custom() const
{
    QHash<QString, QString> result;
    GHashTableIter iter;
    gpointer key, value;

    auto custom = as_component_get_custom(d->cpt);
    g_hash_table_iter_init(&iter, custom);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        result.insert(valueWrap(static_cast<char *>(key)), valueWrap(static_cast<char *>(value)));
    }
    return result;
}

QString AppStream::Component::customValue(const QString &key) const
{
    return valueWrap(as_component_get_custom_value(d->cpt, qPrintable(key)));
}

QString AppStream::Component::customValue(const QString &key)
{
    return valueWrap(as_component_get_custom_value(d->cpt, qPrintable(key)));
}

bool AppStream::Component::insertCustomValue(const QString &key, const QString &value)
{
    return as_component_insert_custom_value(d->cpt, qPrintable(key), qPrintable(value));
}

QList<AppStream::ContentRating> AppStream::Component::contentRatings() const
{
    QList<ContentRating> res;

    auto ratings = as_component_get_content_ratings(d->cpt);
    res.reserve(ratings->len);
    for (uint i = 0; i < ratings->len; i++) {
        auto rating = AS_CONTENT_RATING(g_ptr_array_index(ratings, i));
        res.append(ContentRating(rating));
    }
    return res;
}

AppStream::ContentRating AppStream::Component::contentRating(const QString &kind) const
{
    auto rating = as_component_get_content_rating(d->cpt, qPrintable(kind));
    if (rating == NULL)
        return ContentRating();
    return ContentRating(rating);
}

void AppStream::Component::addContentRating(const AppStream::ContentRating &contentRating)
{
    as_component_add_content_rating(d->cpt, contentRating.cPtr());
}

QString Component::nameVariantSuffix() const
{
    return valueWrap(as_component_get_name_variant_suffix(d->cpt));
}

void Component::setNameVariantSuffix(const QString &variantSuffix, const QString &lang)
{
    as_component_set_name_variant_suffix(d->cpt,
                                         qPrintable(variantSuffix),
                                         lang.isEmpty() ? NULL : qPrintable(lang));
}

bool Component::hasTag(const QString &ns, const QString &tagName)
{
    return as_component_has_tag(d->cpt, qPrintable(ns), qPrintable(tagName));
}

bool Component::addTag(const QString &ns, const QString &tagName)
{
    return as_component_add_tag(d->cpt, qPrintable(ns), qPrintable(tagName));
}

void Component::removeTag(const QString &ns, const QString &tagName)
{
    as_component_remove_tag(d->cpt, qPrintable(ns), qPrintable(tagName));
}

void Component::clearTags()
{
    as_component_clear_tags(d->cpt);
}

bool Component::isFloss() const
{
    return as_component_is_floss(d->cpt);
}

bool AppStream::Component::isIgnored() const
{
    return as_component_is_ignored(d->cpt);
}

bool Component::isValid() const
{
    return as_component_is_valid(d->cpt);
}

QString AppStream::Component::toString() const
{
    return valueWrap(as_component_to_string(d->cpt));
}

QString Component::lastError() const
{
    return d->lastError;
}
