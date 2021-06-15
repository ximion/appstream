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
#include "pool.h"

#include <QStringList>
#include <QUrl>
#include <QLoggingCategory>

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

Pool::Pool(QObject *parent)
    : QObject (parent),
      d(new PoolPrivate())
{}

Pool::~Pool()
{
    // empty. needed for the scoped pointer for the private pointer
}

bool AppStream::Pool::load()
{
    g_autoptr(GError) error = nullptr;
    auto ret = as_pool_load (d->pool, NULL, &error);
    if (!ret && error)
        d->lastError = QString::fromUtf8(error->message);
    return ret;
}

bool Pool::load(QString* strerror)
{
    auto ret = load();
    if (!ret && strerror) {
        *strerror = d->lastError;
    }
    return ret;
}

void Pool::clear()
{
    g_autoptr(GError) error = nullptr;
    auto ret = as_pool_clear2 (d->pool, &error);
    if (!ret && error)
        d->lastError = QString::fromUtf8(error->message);
}

QString Pool::lastError() const
{
    return d->lastError;
}

bool Pool::addComponent(const AppStream::Component& cpt)
{
    // FIXME: We ignore errors for now.
    return as_pool_add_component (d->pool, cpt.m_cpt, NULL);
}

QList<Component> Pool::components() const
{
    return cptArrayToQList(as_pool_get_components(d->pool));
}

QList<Component> Pool::componentsById(const QString& cid) const
{
    return cptArrayToQList(as_pool_get_components_by_id(d->pool, qPrintable(cid)));
}

QList<Component> Pool::componentsByProvided(Provided::Kind kind, const QString& item) const
{
    return cptArrayToQList(as_pool_get_components_by_provided_item(d->pool,
                                                                   static_cast<AsProvidedKind>(kind),
                                                                   qPrintable(item)));
}

QList<AppStream::Component> Pool::componentsByKind(Component::Kind kind) const
{
    return cptArrayToQList(as_pool_get_components_by_kind(d->pool, static_cast<AsComponentKind>(kind)));
}

QList<AppStream::Component> Pool::componentsByCategories(const QStringList& categories) const
{
    QList<AppStream::Component> res;
    g_autofree gchar **cats_strv = NULL;

    cats_strv = g_new0(gchar *, categories.size() + 1);
    for (int i = 0; i < categories.size(); ++i)
        cats_strv[i] = (gchar*) qPrintable(categories.at(i));

    return cptArrayToQList(as_pool_get_components_by_categories (d->pool, cats_strv));
}

QList<Component> Pool::componentsByLaunchable(Launchable::Kind kind, const QString& value) const
{
    return cptArrayToQList(as_pool_get_components_by_launchable(d->pool,
                                                                   static_cast<AsLaunchableKind>(kind),
                                                                   qPrintable(value)));
}

QList<AppStream::Component> Pool::search(const QString& term) const
{
    return cptArrayToQList(as_pool_search(d->pool, qPrintable(term)));
}

void Pool::clearMetadataLocations()
{
    as_pool_clear_metadata_locations(d->pool);
}

void Pool::addMetadataLocation(const QString& directory)
{
    as_pool_add_metadata_location (d->pool, qPrintable(directory));
}

void Pool::setLocale(const QString& locale)
{
    as_pool_set_locale (d->pool, qPrintable(locale));
}

uint Pool::flags() const
{
    return (uint) as_pool_get_flags(d->pool);
}

void Pool::setFlags(uint flags)
{
    as_pool_set_flags (d->pool, (AsPoolFlags) flags);
}

uint Pool::cacheFlags() const
{
    return (uint) as_pool_get_cache_flags(d->pool);
}

void Pool::setCacheFlags(uint flags)
{
    as_pool_set_cache_flags (d->pool, (AsCacheFlags) flags);
}

QString AppStream::Pool::cacheLocation() const
{
    return QString::fromUtf8(as_pool_get_cache_location(d->pool));
}

void Pool::setCacheLocation(const QString &location)
{
    as_pool_set_cache_location(d->pool, qPrintable(location));
}
