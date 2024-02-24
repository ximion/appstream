/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2024 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_SYSTEM_INFO_PRIVATE_H
#define __AS_SYSTEM_INFO_PRIVATE_H

#include "as-system-info.h"
#include "as-macros-private.h"

AS_BEGIN_PRIVATE_DECLS

AS_INTERNAL_VISIBLE
void as_system_info_load_os_release (AsSystemInfo *sysinfo, const gchar *os_release_fname);

AS_INTERNAL_VISIBLE
void as_system_info_set_kernel (AsSystemInfo *sysinfo, const gchar *name, const gchar *version);

AS_INTERNAL_VISIBLE
void as_system_info_set_memory_total (AsSystemInfo *sysinfo, gulong size_mib);

AS_END_PRIVATE_DECLS

#endif /* __AS_SYSTEM_INFO_PRIVATE_H */
