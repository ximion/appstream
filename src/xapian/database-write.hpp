/* database-write.hpp
 *
 * Copyright (C) 2012 Matthias Klumpp
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DATABASE_WRITE_H
#define DATABASE_WRITE_H

#include <iostream>
#include <string>
#include <xapian.h>
#include <glib.h>
#include "appstream_internal.h"

using namespace std;

class DatabaseWrite
{
public:
	explicit DatabaseWrite ();
	~DatabaseWrite ();

	bool init (const gchar *dbPath);

	bool addApplication (AppstreamAppInfo *app);
	bool rebuild (GArray *apps);

private:
	Xapian::WritableDatabase *m_rwXapianDB;
	string m_dbPath;

};

extern "C" {

DatabaseWrite *xa_database_write_new ()
	{ return new DatabaseWrite (); };
void xa_database_write_free (DatabaseWrite *db)
	{ delete db; };
gboolean xa_database_write_init (DatabaseWrite *db, const gchar *db_path)
	{ return db->init (db_path); };
gboolean xa_database_write_add_application (DatabaseWrite *db, AppstreamAppInfo *app)
	{ return db->addApplication (app); };
gboolean xa_database_write_rebuild (DatabaseWrite *db, GArray *apps)
	{ return db->rebuild (apps); };

}

#endif // DATABASE_WRITE_H
