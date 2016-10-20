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

Q_LOGGING_CATEGORY(APPSTREAMQT_DB, "appstreamqt.pool")

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

Pool::Pool(QObject *parent) : QObject (parent),
    d(new PoolPrivate())
{

}

Pool::~Pool()
{
    // empty. needed for the scoped pointer for the private pointer
}

bool Pool::load()
{
    return as_pool_load (d->m_pool, NULL, NULL);
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

QList<Component> Pool::componentsByProvided(Provides::Kind kind, const QString& item) const
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

QList<AppStream::Component> Pool::search(const QString& term) const
{
    return cptArrayToQList(as_pool_search(d->m_pool, qPrintable(term)));
}
