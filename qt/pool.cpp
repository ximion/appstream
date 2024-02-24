/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2023 Kai Uwe Broulik <kde@broulik.de>
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

class AppStream::PoolPrivate
{
public:
    Pool *q;
    AsPool *pool;
    QString lastError;

    PoolPrivate(Pool *q)
        : q(q)
    {
        pool = as_pool_new();
    }

    ~PoolPrivate()
    {
        g_object_unref(pool);
    }
};

static inline ComponentBox absorbResultToCBox(AsComponentBox *cbox)
{
    ComponentBox res(cbox);
    g_object_unref(cbox);
    return res;
}

static void pool_changed_cb(AsPool *cpool, AppStream::Pool *qpool)
{
    Q_EMIT qpool->changed();
}

static void pool_ready_async_cb(AsPool *cpool, GAsyncResult *result, gpointer user_data)
{
    auto *d = static_cast<PoolPrivate *>(user_data);
    g_autoptr(GError) error = NULL;

    if (as_pool_load_finish(cpool, result, &error)) {
        Q_EMIT d->q->loadFinished(true);
    } else {
        const QString errorMessage = QString::fromUtf8(error->message);
        d->lastError = errorMessage;
        Q_EMIT d->q->loadFinished(false);
    }
}

Pool::Pool(QObject *parent)
    : QObject(parent),
      d(new PoolPrivate(this))
{
    g_signal_connect(d->pool, "changed", G_CALLBACK(pool_changed_cb), this);
}

Pool::~Pool()
{
    // empty. needed for the scoped pointer for the private pointer
}

_AsPool *AppStream::Pool::cPtr() const
{
    return d->pool;
}

bool AppStream::Pool::load()
{
    g_autoptr(GError) error = nullptr;
    auto ret = as_pool_load(d->pool, NULL, &error);
    if (!ret && error)
        d->lastError = QString::fromUtf8(error->message);
    return ret;
}

void AppStream::Pool::loadAsync()
{
    as_pool_load_async(d->pool,
                       NULL, // cancellable
                       (GAsyncReadyCallback) pool_ready_async_cb,
                       d.get());
}

void Pool::clear()
{
    as_pool_clear(d->pool);
}

QString Pool::lastError() const
{
    return d->lastError;
}

bool Pool::addComponents(const ComponentBox &cbox)
{
    g_autoptr(GError) error = nullptr;

    bool ret = as_pool_add_components(d->pool, cbox.cPtr(), &error);
    if (!ret)
        d->lastError = QString::fromUtf8(error->message);

    return ret;
}

ComponentBox Pool::components() const
{
    return absorbResultToCBox(as_pool_get_components(d->pool));
}

ComponentBox Pool::componentsById(const QString &cid) const
{
    return absorbResultToCBox(as_pool_get_components_by_id(d->pool, qPrintable(cid)));
}

ComponentBox Pool::componentsByProvided(Provided::Kind kind, const QString &item) const
{
    return absorbResultToCBox(
        as_pool_get_components_by_provided_item(d->pool,
                                                static_cast<AsProvidedKind>(kind),
                                                qPrintable(item)));
}

ComponentBox Pool::componentsByKind(Component::Kind kind) const
{
    return absorbResultToCBox(
        as_pool_get_components_by_kind(d->pool, static_cast<AsComponentKind>(kind)));
}

ComponentBox Pool::componentsByCategories(const QStringList &categories) const
{
    g_autofree gchar **cats_strv = NULL;

    QVector<QByteArray> utf8Categories;
    utf8Categories.reserve(categories.size());
    for (const QString &category : categories) {
        utf8Categories += category.toUtf8();
    }

    cats_strv = g_new0(gchar *, utf8Categories.size() + 1);
    for (int i = 0; i < utf8Categories.size(); ++i)
        cats_strv[i] = (gchar *) utf8Categories[i].constData();

    return absorbResultToCBox(as_pool_get_components_by_categories(d->pool, cats_strv));
}

ComponentBox Pool::componentsByLaunchable(Launchable::Kind kind, const QString &value) const
{
    return absorbResultToCBox(
        as_pool_get_components_by_launchable(d->pool,
                                             static_cast<AsLaunchableKind>(kind),
                                             qPrintable(value)));
}

ComponentBox Pool::componentsByExtends(const QString &extendedId) const
{
    return absorbResultToCBox(as_pool_get_components_by_extends(d->pool, qPrintable(extendedId)));
}

ComponentBox
Pool::componentsByBundleId(Bundle::Kind kind, const QString &extendedId, bool matchPrefix) const
{
    return absorbResultToCBox(as_pool_get_components_by_bundle_id(d->pool,
                                                                  static_cast<AsBundleKind>(kind),
                                                                  qPrintable(extendedId),
                                                                  matchPrefix));
}

ComponentBox Pool::search(const QString &term) const
{
    return absorbResultToCBox(as_pool_search(d->pool, qPrintable(term)));
}

void Pool::setLocale(const QString &locale)
{
    as_pool_set_locale(d->pool, qPrintable(locale));
}

Pool::Flags Pool::flags() const
{
    return static_cast<Pool::Flags>(as_pool_get_flags(d->pool));
}

void Pool::setFlags(Pool::Flags flags)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    as_pool_set_flags(d->pool, (AsPoolFlags) flags.toInt());
#else
    as_pool_set_flags(d->pool, (AsPoolFlags) int(flags));
#endif
}

void Pool::addFlags(Pool::Flags flags)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    as_pool_add_flags(d->pool, (AsPoolFlags) flags.toInt());
#else
    as_pool_add_flags(d->pool, (AsPoolFlags) int(flags));
#endif
}

void Pool::removeFlags(Pool::Flags flags)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    as_pool_remove_flags(d->pool, (AsPoolFlags) flags.toInt());
#else
    as_pool_remove_flags(d->pool, (AsPoolFlags) int(flags));
#endif
}

void Pool::resetExtraDataLocations()
{
    as_pool_reset_extra_data_locations(d->pool);
}

void Pool::addExtraDataLocation(const QString &directory, Metadata::FormatStyle formatStyle)
{
    as_pool_add_extra_data_location(d->pool, qPrintable(directory), (AsFormatStyle) formatStyle);
}

void Pool::setLoadStdDataLocations(bool enabled)
{
    as_pool_set_load_std_data_locations(d->pool, enabled);
}

void Pool::overrideCacheLocations(const QString &sysDir, const QString &userDir)
{
    as_pool_override_cache_locations(d->pool,
                                     sysDir.isEmpty() ? nullptr : qPrintable(sysDir),
                                     userDir.isEmpty() ? nullptr : qPrintable(userDir));
}

bool AppStream::Pool::isEmpty() const
{
    return as_pool_is_empty(d->pool);
}
