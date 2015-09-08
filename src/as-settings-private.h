/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_SETTINGSPRIVATE_H
#define __AS_SETTINGSPRIVATE_H

#include <glib-object.h>
#include "config.h"

G_BEGIN_DECLS

#define AS_DB_SCHEMA_VERSION "1"
#define AS_CONFIG_NAME "/etc/appstream.conf"
#define AS_APPSTREAM_CACHE_PATH "/var/cache/app-info"

#define AS_APPSTREAM_BASE_PATH "/usr/share/app-info"

G_END_DECLS

#endif /* __AS_SETTINGSPRIVATE_H */
