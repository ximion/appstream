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

AsDatabase		*as_database_new (void);
gboolean		as_database_open (AsDatabase *db);

const gchar		*as_database_get_database_path (AsDatabase *db);
void			as_database_set_database_path (AsDatabase *db,
						const gchar *dir);

GPtrArray		*as_database_get_all_components (AsDatabase *db);

GPtrArray		*as_database_find_components (AsDatabase *db,
							const gchar *term,
							const gchar *cats_str);

AsComponent		*as_database_get_component_by_id (AsDatabase *db,
								const gchar *idname);
GPtrArray		*as_database_get_components_by_provides (AsDatabase* db,
								 AsProvidedKind kind,
								 const gchar *item);
GPtrArray		*as_database_get_components_by_kind (AsDatabase *db,
								AsComponentKind kinds);

G_END_DECLS

#endif /* __AS_DATABASE_H */
