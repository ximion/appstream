/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_CACHE_BUILDER_H
#define __AS_CACHE_BUILDER_H

#include <glib-object.h>

#define AS_TYPE_CACHE_BUILDER (as_cache_builder_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsCacheBuilder, as_cache_builder, AS, CACHE_BUILDER, GObject)

G_BEGIN_DECLS

struct _AsCacheBuilderClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

#define	AS_CACHE_BUILDER_ERROR as_cache_builder_error_quark ()
GQuark as_cache_builder_error_quark (void);

/**
 * AsCacheBuilderError:
 * @AS_CACHE_BUILDER_ERROR_FAILED:		Generic failure
 * @AS_CACHE_BUILDER_ERROR_CACHE_INCOMPLETE:	The cache was built, but we had to ignore some metadata.
 * @AS_CACHE_BUILDER_ERROR_TARGET_NOT_WRITABLE:	We do not have write-access to the cache target location.
 *
 * The error type.
 **/
typedef enum {
	AS_CACHE_BUILDER_ERROR_FAILED,
	AS_CACHE_BUILDER_ERROR_CACHE_INCOMPLETE,
	AS_CACHE_BUILDER_ERROR_TARGET_NOT_WRITABLE,
	/*< private >*/
	AS_CACHE_BUILDER_ERROR_LAST
} AsCacheBuilderError;

AsCacheBuilder		*as_cache_builder_new (void);

gboolean		as_cache_builder_setup (AsCacheBuilder *builder,
						const gchar *dbpath,
						GError **error);
gboolean		as_cache_builder_refresh (AsCacheBuilder *builder,
							gboolean force,
							GError **error);

void			as_cache_builder_set_data_source_directories (AsCacheBuilder *self,
								gchar **dirs);

G_END_DECLS

#endif /* __AS_CACHE_BUILDER_H */
