/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2021 Matthias Klumpp <matthias@tenstral.net>
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
 * @AS_CACHE_ERROR_LOCALE_MISMATCH:	Cache locale was different from the expected one.
 * @AS_CACHE_ERROR_FLOATING:		The given action can not be performed on a floating cache.
 * @AS_CACHE_ERROR_NO_FILENAME:		No filename was set to open the database.
 * @AS_CACHE_ERROR_BAD_DATA:		The data that should be added failed a sanity check
 *
 * A metadata pool error.
 **/
typedef enum {
	AS_CACHE_ERROR_FAILED,
	AS_CACHE_ERROR_NOT_OPEN,
	AS_CACHE_ERROR_WRONG_FORMAT,
	AS_CACHE_ERROR_LOCALE_MISMATCH,
	AS_CACHE_ERROR_FLOATING,
	AS_CACHE_ERROR_NO_FILENAME,
	AS_CACHE_ERROR_BAD_DATA,
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
gboolean	as_cache_open2 (AsCache *cache,
			       const gchar *locale,
			       GError **error);
gboolean	as_cache_close (AsCache *cache);

gboolean	as_cache_insert (AsCache *cache,
				 AsComponent *cpt,
				 GError **error);

gboolean	as_cache_remove_by_data_id (AsCache *cache,
					    const gchar *cdid,
					    GError **error);

GPtrArray	*as_cache_get_components_all (AsCache *cache,
						GError **error);
GPtrArray	*as_cache_get_components_by_id (AsCache *cache,
						const gchar *id,
						GError **error);
AsComponent	*as_cache_get_component_by_data_id (AsCache *cache,
						    const gchar *cdid,
						    GError **error);

GPtrArray	*as_cache_get_components_by_kind (AsCache *cache,
						  AsComponentKind kind,
						  GError **error);
GPtrArray	*as_cache_get_components_by_provided_item (AsCache *cache,
							  AsProvidedKind kind,
							  const gchar *item,
							  GError **error);
GPtrArray	*as_cache_get_components_by_categories (AsCache *cache,
							gchar **categories,
							GError **error);
GPtrArray	*as_cache_get_components_by_launchable (AsCache *cache,
						       AsLaunchableKind kind,
						       const gchar *id,
						       GError **error);

GPtrArray	*as_cache_search (AsCache *cache,
				  gchar **terms,
				  gboolean sort,
				  GError **error);

gboolean	as_cache_has_component_id (AsCache *cache,
						const gchar *id,
						GError **error);
gssize		as_cache_count_components (AsCache *cache,
					   GError **error);

time_t		as_cache_get_ctime (AsCache *cache);
gboolean	as_cache_is_open (AsCache *cache);

void		as_cache_make_floating (AsCache *cache);
guint		as_cache_unfloat (AsCache *cache,
				  GError **error);

const gchar	*as_cache_get_location (AsCache *cache);
void		as_cache_set_location (AsCache *cache,
					const gchar *location);

void		as_cache_set_refine_func (AsCache *cache,
					  GFunc func,
					  gpointer user_data);

gboolean	as_cache_get_nosync (AsCache *cache);
void		as_cache_set_nosync (AsCache *cache,
				     gboolean nosync);

gboolean	as_cache_get_readonly (AsCache *cache);
void		as_cache_set_readonly (AsCache *cache,
					gboolean ro);

G_END_DECLS

#endif /* __AS_CACHE_H */
