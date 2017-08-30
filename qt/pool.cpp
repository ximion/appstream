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
        AsPool *m_pool;

        PoolPrivate()
        {
            m_pool = as_pool_new();
        }

        ~PoolPrivate() {
            g_object_unref(m_pool);
        }
};

static QList<Component> cptArrayToQList(GPtrArray *cpts)
{
    QList<Component> res;
    res.reserve(cpts->len);
    for (uint i = 0; i < cpts->len; i++) {
        auto cpt = AS_COMPONENT(g_ptr_array_index(cpts, i));
        Component x(cpt);
        res.append(x);
    }
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
    return load(nullptr);
}

bool Pool::load(QString* strerror)
{
    g_autoptr(GError) error = nullptr;
    bool ret = as_pool_load (d->m_pool, NULL, &error);
    if (!ret && error && strerror) {
        *strerror = QString::fromUtf8(error->message);
    }
    return ret;
}

void Pool::clear()
{
    return as_pool_clear (d->m_pool);
}

bool Pool::addComponent(const AppStream::Component& cpt)
{
    // FIXME: We ignore errors for now.
    return as_pool_add_component (d->m_pool, cpt.m_cpt, NULL);
}

QList<Component> Pool::components() const
{
    return cptArrayToQList(as_pool_get_components(d->m_pool));
}

QList<Component> Pool::componentsById(const QString& cid) const
{
    return cptArrayToQList(as_pool_get_components_by_id(d->m_pool, qPrintable(cid)));
}

QList<Component> Pool::componentsByProvided(Provided::Kind kind, const QString& item) const
{
    return cptArrayToQList(as_pool_get_components_by_provided_item(d->m_pool,
                                                                   static_cast<AsProvidedKind>(kind),
                                                                   qPrintable(item)));
}

QList<AppStream::Component> Pool::componentsByKind(Component::Kind kind) const
{
    return cptArrayToQList(as_pool_get_components_by_kind(d->m_pool, static_cast<AsComponentKind>(kind)));
}

QList<AppStream::Component> Pool::componentsByCategories(const QStringList categories) const
{
    // FIXME: Todo
    QList<AppStream::Component> res;
    //! return cptArrayToQList(as_pool_get_components_by_categories (d->m_pool, );
    return res;
}

QList<Component> Pool::componentsByLaunchable(Launchable::Kind kind, const QString& value) const
{
    return cptArrayToQList(as_pool_get_components_by_launchable(d->m_pool,
                                                                   static_cast<AsLaunchableKind>(kind),
                                                                   qPrintable(value)));
}

QList<AppStream::Component> Pool::search(const QString& term) const
{
    return cptArrayToQList(as_pool_search(d->m_pool, qPrintable(term)));
}

void Pool::clearMetadataLocations()
{
    as_pool_clear_metadata_locations(d->m_pool);
}

void Pool::addMetadataLocation(const QString& directory)
{
    as_pool_add_metadata_location (d->m_pool, qPrintable(directory));
}

void Pool::setLocale(const QString& locale)
{
    as_pool_set_locale (d->m_pool, qPrintable(locale));
}

uint Pool::flags() const
{
    return (uint) as_pool_get_flags(d->m_pool);
}

void Pool::setFlags(uint flags)
{
    as_pool_set_flags (d->m_pool, (AsPoolFlags) flags);
}

uint Pool::cacheFlags() const
{
    return (uint) as_pool_get_cache_flags(d->m_pool);
}

void Pool::setCacheFlags(uint flags)
{
    as_pool_set_cache_flags (d->m_pool, (AsCacheFlags) flags);
}
