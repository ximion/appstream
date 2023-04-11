/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include "as-component.h"

G_BEGIN_DECLS

GPtrArray		*as_pool_get_components_gi (AsPool *pool);
GPtrArray		*as_pool_get_components_by_id_gi (AsPool *pool,
							   const gchar *cid);
GPtrArray		*as_pool_get_components_by_provided_item_gi (AsPool *pool,
									AsProvidedKind kind,
									const gchar *item);
GPtrArray		*as_pool_get_components_by_kind_gi (AsPool *pool, AsComponentKind kind);
GPtrArray		*as_pool_get_components_by_categories_gi (AsPool *pool, gchar **categories);
GPtrArray		*as_pool_get_components_by_launchable_gi (AsPool *pool,
								   AsLaunchableKind kind,
								   const gchar *id);
GPtrArray		*as_pool_get_components_by_extends_gi (AsPool *pool,
								const gchar *extended_id);
GPtrArray		*as_pool_get_components_by_bundle_id_gi (AsPool *pool,
								  AsBundleKind kind,
								  const gchar *bundle_id,
								  gboolean match_prefix);
GPtrArray		*as_pool_search_gi (AsPool *pool, const gchar *search);

G_END_DECLS
