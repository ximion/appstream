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

#ifndef __AS_POOL_PRIVATE_H
#define __AS_POOL_PRIVATE_H

#include "as-pool.h"
#include "as-settings-private.h"
#include "as-file-monitor.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

time_t			as_pool_get_os_metadata_cache_age (AsPool *pool);

AS_INTERNAL_VISIBLE
gboolean		as_pool_refresh_system_cache (AsPool *pool,
							gboolean force,
							gboolean *caches_updated,
							GError **error);

AS_INTERNAL_VISIBLE
void			as_pool_override_cache_locations (AsPool *pool,
							  const gchar *dir_sys,
							  const gchar *dir_user);

AS_INTERNAL_VISIBLE
gboolean		as_pool_print_std_data_locations_info_private (AsPool *pool,
									gboolean print_os_data,
									gboolean print_extra_data);

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_POOL_PRIVATE_H */
