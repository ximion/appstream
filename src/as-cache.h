/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_CACHE_H
#define __AS_CACHE_H

#include <glib-object.h>
#include "as-component.h"

G_BEGIN_DECLS

#define AS_TYPE_CACHE (as_cache_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsCache, as_cache, AS, CACHE, GObject)

struct _AsCacheClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * AsCacheError:
 * @AS_CACHE_ERROR_FAILED:		Generic failure
 * @AS_CACHE_ERROR_NOT_OPEN:		Cache was not open.
 * @AS_CACHE_ERROR_WRONG_FORMAT:	Cache has an unsupported format version.
 *
 * A metadata pool error.
 **/
typedef enum {
	AS_CACHE_ERROR_FAILED,
	AS_CACHE_ERROR_NOT_OPEN,
	AS_CACHE_ERROR_WRONG_FORMAT,
	/*< private >*/
	AS_CACHE_ERROR_LAST
} AsCacheError;

#define AS_CACHE_ERROR	as_cache_error_quark ()
GQuark			as_cache_error_quark (void);

AsCache		*as_cache_new (void);

gboolean	as_cache_open (AsCache *cache,
			       const gchar *fname,
			       const gchar *locale,
			       GError **error);
gboolean	as_cache_close (AsCache *cache);

gboolean	as_cache_insert (AsCache *cache,
				 AsComponent *cpt,
				 gboolean replace,
				 GError **error);

GPtrArray	*as_cache_get_components_by_id (AsCache *cache,
						const gchar *id,
						GError **error);

G_END_DECLS

#endif /* __AS_CACHE_H */
