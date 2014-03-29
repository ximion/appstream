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

#ifndef __AS_SETTINGSPRIVATE_H
#define __AS_SETTINGSPRIVATE_H

#include <glib-object.h>
#include "config.h"

G_BEGIN_DECLS

#define AS_DB_SCHEMA_VERSION "1"
#define AS_APPSTREAM_BASE_PATH DATADIR "/app-info"
#define AS_CONFIG_NAME "/etc/appstream.conf"
#define AS_APPSTREAM_CACHE_PATH "/var/cache/app-info"
#define AS_APPSTREAM_DATABASE_PATH AS_APPSTREAM_CACHE_PATH "/xapian"

const gchar* AS_APPSTREAM_XML_PATHS[2] = {AS_APPSTREAM_BASE_PATH "/xmls", "/var/cache/app-info/xmls", NULL};

#define AS_APPSTREAM_BASE_PATH DATADIR "/app-info"

/**
 * The path where software icons (of not-installed software) are located.
 */
const gchar* AS_ICON_PATHS[2] = {AS_APPSTREAM_BASE_PATH "/icons", "/var/cache/app-info/icons"};

G_END_DECLS

#endif /* __AS_SETTINGSPRIVATE_H */
