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

#ifndef __AS_DATABASE_H
#define __AS_DATABASE_H

#include <glib-object.h>
#include "as-component.h"

G_BEGIN_DECLS

#define AS_TYPE_DATABASE (as_database_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsDatabase, as_database, AS, DATABASE, GObject)

struct _AsDatabaseClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * AsDatabaseError:
 * @AS_DATABASE_ERROR_FAILED:		Generic failure
 * @AS_DATABASE_ERROR_CLOSED:		Tried to perform action on a closed database.
 * @AS_DATABASE_ERROR_TERM_INVALID:	A query term was invalid.
 *
 * A database query error.
 **/
typedef enum {
	AS_DATABASE_ERROR_FAILED,
	AS_DATABASE_ERROR_CLOSED,
	AS_DATABASE_ERROR_TERM_INVALID,
	/*< private >*/
	AS_DATABASE_ERROR_LAST
} AsDatabaseError;

#define	AS_DATABASE_ERROR	as_database_error_quark ()

AsDatabase		*as_database_new (void);
GQuark			as_database_error_quark (void);
gboolean		as_database_open (AsDatabase *db,
						GError **error);

const gchar		*as_database_get_location (AsDatabase *db);
void			as_database_set_location (AsDatabase *db,
							const gchar *dir);

GPtrArray		*as_database_find_components (AsDatabase *db,
							const gchar *term,
							const gchar *cats_str,
							GError **error);

GPtrArray		*as_database_get_all_components (AsDatabase *db,
							 GError **error);
AsComponent		*as_database_get_component_by_id (AsDatabase *db,
								const gchar *cid,
								GError **error);
GPtrArray		*as_database_get_components_by_provided_item (AsDatabase *db,
								 AsProvidedKind kind,
								 const gchar *item,
								 GError **error);
GPtrArray		*as_database_get_components_by_kind (AsDatabase *db,
								AsComponentKind kind,
								GError **error);

G_END_DECLS

#endif /* __AS_DATABASE_H */
