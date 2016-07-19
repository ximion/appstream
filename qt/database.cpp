/*
 * Part of Appstream, a library for accessing AppStream on-disk database
 * Copyright (C) 2014  Sune Vuorela <sune@vuorela.dk>
 * Copyright (C) 2015  Matthias Klumpp <matthias@tenstral.net>
 *
 * Based upon database-read.hpp
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#define QT_NO_KEYWORDS
#include "database.h"

#include <appstream.h>
#include <QStringList>
#include <QUrl>
#include <QMultiHash>
#include <QLoggingCategory>
#include "as-component-private.h"

#include "image.h"
#include "screenshot.h"

Q_LOGGING_CATEGORY(APPSTREAMQT_DB, "appstreamqt.database")

using namespace Appstream;

class Appstream::DatabasePrivate {
    public:
        DatabasePrivate(const QString& cachePath) : m_cachePath(cachePath) {
        }

        QString m_cachePath;
        QString m_errorString;
        AsDataPool *m_dpool;

        bool open() {
            g_autoptr(GError) error = NULL;

            m_dpool = as_data_pool_new ();

            if (m_cachePath.isEmpty())
                as_data_pool_load (m_dpool, NULL, &error);
            else
                as_data_pool_load_cache_file (m_dpool, qPrintable(m_cachePath), &error);
            if (error != NULL) {
                m_errorString = QString::fromUtf8 (error->message);
                return false;
            }

            return true;
        }

        ~DatabasePrivate() {
            g_object_unref (m_dpool);
        }
};

Database::Database(const QString& cachePath) : d(new DatabasePrivate(cachePath)) {

}

bool Database::open() {
    return d->open();
}

QString Database::errorString() const {
    return d->m_errorString;
}

Database::~Database() {
    // empty. needed for the scoped pointer for the private pointer
}

QString value(const gchar *cstr) {
    return QString::fromUtf8(cstr);
}

QStringList value(gchar **strv) {
    QStringList res;
    if (strv == NULL)
        return res;
    for (uint i = 0; strv[i] != NULL; i++) {
        res.append (QString::fromUtf8(strv[i]));
    }
    return res;
}

QStringList value(GPtrArray *array) {
    QStringList res;
    res.reserve(array->len);
    for (uint i = 0; i < array->len; i++) {
        auto strval = (const gchar*) g_ptr_array_index (array, i);
	res.append (QString::fromUtf8(strval));
    }
    return res;
}

Component convertAsComponent(AsComponent *cpt) {
    Component component;
    std::string str;

    // kind
    QString kindString = value (as_component_kind_to_string (as_component_get_kind (cpt)));
    component.setKind(Component::stringToKind(kindString));

    // Identifier
    QString id = value(as_component_get_id (cpt));
    component.setId(id);

    // Component name
    QString name = value(as_component_get_name (cpt));
    component.setName(name);

    // Summary
    QString summary = value(as_component_get_summary (cpt));
    component.setSummary(summary);

    // Package name
    QStringList packageNames = value(as_component_get_pkgnames (cpt));
    component.setPackageNames(packageNames);

    // Bundles
    auto bundle_ids = as_component_get_bundles_table (cpt);
    if (g_hash_table_size (bundle_ids) > 0) {
        GHashTableIter iter;
        gpointer key, strPtr;

        QHash<Component::BundleKind, QString> bundles;
        g_hash_table_iter_init (&iter, bundle_ids);
        while (g_hash_table_iter_next (&iter, &key, &strPtr)) {
            auto bkind = (Component::BundleKind) GPOINTER_TO_INT (key);
            auto bval = QString::fromUtf8((const gchar*) strPtr);
            bundles.insertMulti(bkind, bval);
        }
        component.setBundles(bundles);
    }

    // URLs
    auto urls_table = as_component_get_urls_table (cpt);
    if (g_hash_table_size (urls_table) > 0) {
        GHashTableIter iter;
        gpointer key, cstrUrl;

        QMultiHash<Component::UrlKind, QUrl> urls;
        g_hash_table_iter_init (&iter, bundle_ids);
        while (g_hash_table_iter_next (&iter, &key, &cstrUrl)) {
            auto ukind = (Component::UrlKind) GPOINTER_TO_INT (key);
            auto url = QUrl::fromUserInput(QString::fromUtf8((const gchar*) cstrUrl));

            if (ukind != Component::UrlKindUnknown) {
                urls.insertMulti(ukind, url);
            } else {
                qCWarning(APPSTREAMQT_DB, "URL of unknown type found for '%s': %s", qPrintable(id), qPrintable(url.toString()));
            }
        }
        component.setUrls(urls);
    }

    // Provided items
    QList<Provides> provideslist;
    for (uint j = 0; j < AS_PROVIDED_KIND_LAST; j++) {
        AsProvidedKind kind = (AsProvidedKind) j;

        AsProvided *prov = as_component_get_provided_for_kind (cpt, kind);
        if (prov == NULL)
            continue;

        auto pkind = (Provides::Kind) kind;
        gchar **items = as_provided_get_items (prov);
	for (uint j = 0; items[j] != NULL; j++) {
            Provides provides;
            provides.setKind(pkind);
            provides.setValue(QString::fromUtf8(items[j]));
            provideslist << provides;
        }
    }
    component.setProvides(provideslist);

    // Icons
    auto icons = as_component_get_icons (cpt);
    for (uint i = 0; i < icons->len; i++) {
        auto icon = AS_ICON (g_ptr_array_index (icons, i));
        if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
            component.setIcon(QString::fromUtf8(as_icon_get_name (icon)));
        } else {
            // TODO: Support more icon types properly
            auto size = QSize(as_icon_get_width (icon),
                              as_icon_get_height (icon));
            QUrl url = QUrl::fromUserInput(QString::fromUtf8(as_icon_get_filename (icon)));
            component.addIconUrl(url, size);
        }
    }

    // Long description
    QString description = value(as_component_get_description (cpt));
    component.setDescription(description);

    // Categories
    QStringList categories = value(as_component_get_categories (cpt));
    component.setCategories(categories);

    // Extends
    const QStringList extends = value(as_component_get_extends (cpt));
    component.setExtends(extends);

    // Extensions
    const QStringList extensions = value(as_component_get_extensions (cpt));
    component.setExtensions(extensions);

    // Screenshots
    QList<Appstream::Screenshot> screenshots;
    auto scrs = as_component_get_screenshots (cpt);
    for (uint i = 0; i < scrs->len; i++) {
        auto cscr = AS_SCREENSHOT (g_ptr_array_index (scrs, i));
	Appstream::Screenshot scr;

        if (as_screenshot_get_kind (cscr) == AS_SCREENSHOT_KIND_DEFAULT)
            scr.setDefault(true);

        if (as_screenshot_get_caption (cscr) != NULL)
            scr.setCaption(QString::fromUtf8(as_screenshot_get_caption (cscr)));

        QList<Appstream::Image> images;
        auto images_array = as_screenshot_get_images (cscr);
        for (uint j = 0; j < images_array->len; j++) {
            auto cimg = AS_IMAGE (g_ptr_array_index (images_array, j));

            Appstream::Image image;

            image.setUrl(QUrl(QString::fromUtf8(as_image_get_url (cimg))));
            image.setWidth(as_image_get_width (cimg));
            image.setHeight(as_image_get_height (cimg));

            if (as_image_get_kind (cimg) == AS_IMAGE_KIND_THUMBNAIL)
                image.setKind(Appstream::Image::Kind::Thumbnail);
            else
                image.setKind(Appstream::Image::Kind::Plain);
            images.append(image);
        }
        scr.setImages(images);

        screenshots.append(scr);
    }
    component.setScreenshots(screenshots);

    // Compulsory-for-desktop information
    QStringList compulsory = value(as_component_get_compulsory_for_desktops (cpt));
    component.setCompulsoryForDesktops(compulsory);

    // License
    QString license = value(as_component_get_project_license (cpt));
    component.setProjectLicense(license);

    // Project group
    QString projectGroup = value(as_component_get_project_group (cpt));
    component.setProjectGroup(projectGroup);

    // Developer name
    QString developerName = value(as_component_get_developer_name (cpt));
    component.setDeveloperName(developerName);

    // Releases
      // TODO

    return component;
}

QList< Component > Database::allComponents() const
{
    g_autoptr(GPtrArray) array = NULL;
    QList<Component> components;

    // get a pointer array of all components we have
    array = as_data_pool_get_components (d->m_dpool);
    components.reserve(array->len);

    // create QList of AppStream::Component out of the AsComponents
    for (uint i = 0; i < array->len; i++) {
        auto cpt = AS_COMPONENT (g_ptr_array_index (array, i));
        components << convertAsComponent(cpt);
    }

    return components;
}

Component Database::componentById(const QString& id) const
{
    g_autoptr(AsComponent) cpt = NULL;

    cpt = as_data_pool_get_component_by_id (d->m_dpool, qPrintable(id));
    if (cpt == NULL)
        return Component();

    return convertAsComponent(cpt);
}

QList< Component > Database::componentsByKind(Component::Kind kind) const
{
    g_autoptr(GPtrArray) array = NULL;
    g_autoptr(GError) error = NULL;
    QList<Component> result;

    array = as_data_pool_get_components_by_kind (d->m_dpool, (AsComponentKind) kind, &error);
    if (error != NULL) {
        qCCritical(APPSTREAMQT_DB, "Unable to get components by kind: %s", error->message);
        return result;
    }

    result.reserve(array->len);
    for (uint i = 0; i < array->len; i++) {
        auto cpt = AS_COMPONENT (g_ptr_array_index (array, i));
        result << convertAsComponent(cpt);
    }

    return result;
}

QList< Component > Database::findComponentsByString(const QString& searchTerm, const QStringList& categories)
{
    Q_UNUSED(categories); // FIXME

    g_autoptr(GPtrArray) array = NULL;
    array = as_data_pool_search (d->m_dpool, qPrintable(searchTerm));
    QList<Component> result;
    result.reserve(array->len);

    for (uint i = 0; i < array->len; i++) {
        auto cpt = AS_COMPONENT (g_ptr_array_index (array, i));
        result << convertAsComponent(cpt);
    }

    return result;
}

QList<Component> Database::findComponentsByPackageName(const QString& packageName) const
{
    const gchar *pkgname = qPrintable(packageName);
    QList<Component> result;

    g_autoptr(GPtrArray) cpts = as_data_pool_get_components (d->m_dpool);
    for (uint i = 0; i < cpts->len; i++) {
        auto cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));
	auto pkgnames = as_component_get_pkgnames (cpt);
	if (pkgnames == NULL)
            continue;

        for (uint j = 0; pkgnames[j] != NULL; j++) {
            if (g_strcmp0 (pkgnames[j], pkgname) == 0)
                result << convertAsComponent(cpt);
	}
    }

    return result;
}


Database::Database() : d(new DatabasePrivate(QString())) {

}
