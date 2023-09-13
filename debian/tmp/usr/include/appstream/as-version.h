/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2021-2022 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#if !defined (__APPSTREAM_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

/* compile time version
 */
#define AS_MAJOR_VERSION				1
#define AS_MINOR_VERSION				0
#define AS_MICRO_VERSION				0

/* check whether a As version equal to or greater than
 * major.minor.micro.
 */
#define AS_CHECK_VERSION(major,minor,micro)    \
    (AS_MAJOR_VERSION > (major) || \
     (AS_MAJOR_VERSION == (major) && AS_MINOR_VERSION > (minor)) || \
     (AS_MAJOR_VERSION == (major) && AS_MINOR_VERSION == (minor) && \
      AS_MICRO_VERSION >= (micro)))

const gchar	*as_version_string (void);

G_END_DECLS
