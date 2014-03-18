/* database-vala.hpp
 *
 * Copyright (C) 2012-2013 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef DATABASE_VALA_H
#define DATABASE_VALA_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XADatabaseRead XADatabaseRead;
typedef struct XADatabaseWrite XADatabaseWrite;

typedef struct _AsSearchQuery AsSearchQuery;
typedef struct _AsAppInfo AsAppInfo;

/* methods for database read access */

XADatabaseRead *xa_database_read_new ();
void xa_database_read_free (XADatabaseRead *db);

gboolean xa_database_read_open (XADatabaseRead *db, const gchar *db_path);
const gchar *xa_database_read_get_schema_version (XADatabaseRead *db);
GPtrArray *xa_database_read_get_all_applications (XADatabaseRead *db);
GPtrArray *xa_database_read_find_applications (XADatabaseRead *db, AsSearchQuery *query);

/* methods for database write access */

XADatabaseWrite *xa_database_write_new ();
void xa_database_write_free (XADatabaseWrite *db);

gboolean xa_database_write_initialize (XADatabaseWrite *db, const gchar *db_path);
gboolean xa_database_write_add_application (XADatabaseWrite *db, AsAppInfo *app);
gboolean xa_database_write_rebuild (XADatabaseWrite *db, GArray *apps);

#ifdef __cplusplus
};
#endif

#endif /* DATABASE_VALA_H */
