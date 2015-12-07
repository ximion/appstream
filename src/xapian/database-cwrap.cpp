/* database-cwrap.cpp
 *
 * Copyright (C) 2012-2014 Matthias Klumpp
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

#include "database-read.hpp"
#include "database-write.hpp"
#include "database-cwrap.hpp"

/* methods for database read access */

XADatabaseRead *xa_database_read_new () { return new DatabaseRead (); };
void xa_database_read_free (XADatabaseRead *db) { delete realDbRead (db); };

gboolean
xa_database_read_open (XADatabaseRead *db, const gchar *db_path)
{
	return realDbRead (db)->open (db_path);
};

GPtrArray*
xa_database_read_get_all_components (XADatabaseRead *db)
{
	return realDbRead (db)->getAllComponents ();
};

GPtrArray*
xa_database_read_find_components (XADatabaseRead *db, AsSearchQuery *query)
{
	return realDbRead (db)->findComponents (query);
};

AsComponent*
xa_database_read_get_component_by_id (XADatabaseRead *db, const gchar *idname)
{
	return realDbRead (db)->getComponentById (idname);
};

GPtrArray*
xa_database_read_get_components_by_provides (XADatabaseRead *db, AsProvidedKind kind, const gchar *item)
{
	return realDbRead (db)->getComponentsByProvides (kind, item);
};

GPtrArray*
xa_database_read_get_components_by_kind (XADatabaseRead *db, AsComponentKind kinds)
{
	return realDbRead (db)->getComponentsByKind (kinds);
};

/* methods for database write access */

XADatabaseWrite *xa_database_write_new () { return new DatabaseWrite (); };
void xa_database_write_free (XADatabaseWrite *db) { delete realDbWrite (db); };

gboolean
xa_database_write_initialize (XADatabaseWrite *db, const gchar *db_path)
{
	return realDbWrite (db)->initialize (db_path);
};

gboolean
xa_database_write_add_component (XADatabaseWrite *db, AsComponent *cpt)
{
	return realDbWrite (db)->addComponent (cpt);
};

gboolean
xa_database_write_rebuild (XADatabaseWrite *db, GList *cpt_list)
{
	return realDbWrite (db)->rebuild (cpt_list);
};
