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
#include "as-component.h"

G_BEGIN_DECLS

/**
 * AsVercmpFlags:
 * @AS_VERCMP_FLAG_NONE:		No flags set
 * @AS_VERCMP_FLAG_IGNORE_EPOCH:	Ignore epoch part of a version string.
 *
 * The flags used when matching unique IDs.
 **/
typedef enum {
	AS_VERCMP_FLAG_NONE		= 0,
	AS_VERCMP_FLAG_IGNORE_EPOCH	= 1 << 0,
	/*< private >*/
	AS_VERCMP_FLAG_LAST
} AsVercmpFlags;


gint		as_vercmp (const gchar* a,
			   const gchar *b,
			   AsVercmpFlags flags);
gint		as_vercmp_simple (const gchar* a,
				  const gchar *b);


/* DEPRECATED */

G_DEPRECATED_FOR(as_vercmp_simple)
gint		as_utils_compare_versions (const gchar* a,
					   const gchar *b);

G_END_DECLS
