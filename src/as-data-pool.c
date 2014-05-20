/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

/**
 * SECTION:as-data-pool
 * @short_description: Collect and temporarily store metadata from different sources
 *
 * This class contains a temporary pool of metadata which has been collected from different
 * sources on the system.
 * It can directly be used, but usually it is accessed through a #AsDatabase instance.
 *
 * See also: #AsDatabase
 */

#include "config.h"
#include "as-data-pool.h"

typedef struct _AsDataPoolPrivate	AsDataPoolPrivate;
struct _AsDataPoolPrivate
{
	gchar *url;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsDataPool, as_data_pool, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_data_pool_get_instance_private (o))

/**
 * as_data_pool_finalize:
 **/
static void
as_data_pool_finalize (GObject *object)
{
	AsDataPool *dpool = AS_DATA_POOL (object);
	AsDataPoolPrivate *priv = GET_PRIVATE (dpool);

	g_free (priv->url);

	G_OBJECT_CLASS (as_data_pool_parent_class)->finalize (object);
}

/**
 * as_data_pool_init:
 **/
static void
as_data_pool_init (AsDataPool *dpool)
{
}

/**
 * as_data_pool_class_init:
 **/
static void
as_data_pool_class_init (AsDataPoolClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_data_pool_finalize;
}

/**
 * as_data_pool_new:
 *
 * Creates a new #AsDataPool.
 *
 * Returns: (transfer full): a #AsDataPool
 *
 **/
AsDataPool *
as_data_pool_new (void)
{
	AsDataPool *dpool;
	dpool = g_object_new (AS_TYPE_DATA_POOL, NULL);
	return AS_DATA_POOL (dpool);
}
