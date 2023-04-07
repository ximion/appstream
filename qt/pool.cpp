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

#include <appstream.h>
#include "as-pool-private.h"
#include "pool.h"

#include <QStringList>
#include <QUrl>
#include <QLoggingCategory>
#include "chelpers.h"

Q_DECLARE_LOGGING_CATEGORY(APPSTREAMQT_POOL)
Q_LOGGING_CATEGORY(APPSTREAMQT_POOL, "appstreamqt.pool")

using namespace AppStream;

class AppStream::PoolPrivate {
    public:
        AsPool *pool;
        QString lastError;

        PoolPrivate()
        {
            pool = as_pool_new();
        }

        ~PoolPrivate() {
            g_object_unref(pool);
        }
};

static QList<Component> cptArrayToQList(GPtrArray *cpts)
{
    QList<Component> res;
    res.reserve(cpts->len);
    for (uint i = 0; i < cpts->len; i++) {
        auto ccpt = AS_COMPONENT(g_ptr_array_index(cpts, i));
        Component cpt(ccpt);
        res.append(cpt);
    }
    g_ptr_array_unref (cpts);
    return res;
}

static void
pool_changed_cb (AsPool *cpool, AppStream::Pool *qpool)
{
    qpool->changed();
}

Pool::Pool(QObject *parent)
    : QObject (parent),
      d(new PoolPrivate())
{
    g_signal_connect (d->pool, "changed",
                      G_CALLBACK (pool_changed_cb), this);
}

Pool::~Pool()
{
    // empty. needed for the scoped pointer for the private pointer
}

_AsPool *AppStream::Pool::asPool() const
{
    return d->pool;
}

bool AppStream::Pool::load()
{
    g_autoptr(GError) error = nullptr;
    auto ret = as_pool_load (d->pool, NULL, &error);
    if (!ret && error)
        d->lastError = QString::fromUtf8(error->message);
    return ret;
}

void Pool::clear()
{
    as_pool_clear (d->pool);
}

QString Pool::lastError() const
{
    return d->lastError;
}

bool Pool::addComponents(const QList<AppStream::Component>& cpts)
{
    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) array = nullptr;

    array = g_ptr_array_sized_new (cpts.length());
    for (const auto& cpt : cpts)
        g_ptr_array_add(array, cpt.asComponent());

    bool ret = as_pool_add_components (d->pool, array, &error);
    if (!ret)
        d->lastError = QString::fromUtf8(error->message);

    return ret;
}

QList<Component> Pool::components() const
{
    return cptArrayToQList(as_pool_get_components(d->pool));
}

QList<Component> Pool::componentsById(QAnyStringView cid) const
{
    return cptArrayToQList(as_pool_get_components_by_id(d->pool, stringViewToChar(cid)));
}

QList<Component> Pool::componentsByProvided(Provided::Kind kind, QAnyStringView item) const
{
    return cptArrayToQList(as_pool_get_components_by_provided_item(d->pool,
                                                                   static_cast<AsProvidedKind>(kind),
                                                                   stringViewToChar(item)));
}

QList<AppStream::Component> Pool::componentsByKind(Component::Kind kind) const
{
    return cptArrayToQList(as_pool_get_components_by_kind(d->pool, static_cast<AsComponentKind>(kind)));
}

QList<AppStream::Component> Pool::componentsByCategories(QList<QAnyStringView> categories) const
{
    QList<AppStream::Component> res;
    g_autofree gchar **cats_strv = NULL;

    QVector<QByteArray> utf8Categories;
    utf8Categories.reserve(categories.size());
    for (QAnyStringView category : categories) {
        QByteArray bytes = QByteArray();
        bytes.reserve(category.size_bytes() + 1);
        bytes.append((char*)category.data(), category.size_bytes());
        bytes.append('\0');
        utf8Categories += bytes;
    }

    cats_strv = g_new0(gchar *, utf8Categories.size() + 1);
    for (int i = 0; i < utf8Categories.size(); ++i)
        cats_strv[i] = (gchar*) utf8Categories[i].constData();

    return cptArrayToQList(as_pool_get_components_by_categories (d->pool, cats_strv));
}

QList<Component> Pool::componentsByLaunchable(Launchable::Kind kind, QAnyStringView value) const
{
    return cptArrayToQList(as_pool_get_components_by_launchable(d->pool,
                                                                static_cast<AsLaunchableKind>(kind),
                                                                stringViewToChar(value)));
}

QList<Component> Pool::componentsByExtends(QAnyStringView extendedId) const
{
    return cptArrayToQList(as_pool_get_components_by_extends(d->pool,
                                                             stringViewToChar(extendedId)));
}

QList<Component> Pool::componentsByBundleId(Bundle::Kind kind, QAnyStringView extendedId, bool matchPrefix) const
{
    return cptArrayToQList(as_pool_get_components_by_bundle_id(d->pool,
                                                               static_cast<AsBundleKind>(kind),
                                                               stringViewToChar(extendedId),
                                                               matchPrefix));
}

QList<AppStream::Component> Pool::search(QAnyStringView term) const
{
    return cptArrayToQList(as_pool_search(d->pool, stringViewToChar(term)));
}

void Pool::setLocale(QAnyStringView locale)
{
    as_pool_set_locale (d->pool, stringViewToChar(locale));
}

uint Pool::flags() const
{
    return as_pool_get_flags(d->pool);
}

void Pool::setFlags(uint flags)
{
    as_pool_set_flags (d->pool, (AsPoolFlags) flags);
}

void Pool::addFlags(uint flags)
{
    as_pool_add_flags(d->pool, (AsPoolFlags) flags);
}

void Pool::removeFlags(uint flags)
{
    as_pool_remove_flags(d->pool, (AsPoolFlags) flags);
}

void Pool::resetExtraDataLocations()
{
    as_pool_reset_extra_data_locations (d->pool);
}

void Pool::addExtraDataLocation(QAnyStringView directory, Metadata::FormatStyle formatStyle)
{
    as_pool_add_extra_data_location (d->pool,
                                     stringViewToChar(directory),
                                     (AsFormatStyle) formatStyle);
}

void Pool::setLoadStdDataLocations(bool enabled)
{
    as_pool_set_load_std_data_locations(d->pool, enabled);
}

void Pool::overrideCacheLocations(QAnyStringView sysDir, QAnyStringView userDir)
{
    as_pool_override_cache_locations (d->pool,
                                      sysDir.isEmpty()? nullptr : stringViewToChar(sysDir),
                                      userDir.isEmpty()? nullptr : stringViewToChar(userDir));
}

bool AppStream::Pool::isEmpty() const
{
    return as_pool_is_empty(d->pool);
}
