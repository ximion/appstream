/* database.hpp
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

#ifndef DATABASE_H
#define DATABASE_H

#include <stdio.h>
#include <vector>
#include <xapian.h>
#include <glib.h>
#include "uai_internal.h"

class Database
{
public:
	explicit Database ();
	~Database ();

	bool init (const gchar *dbPath);

	bool addApplication (UaiAppInfo *app);

private:
	Xapian::WritableDatabase *m_rwXapianDB;

};

extern "C" {

Database *xa_database_new () { return new Database (); };
void xa_database_free (Database *db) { delete db; };
gboolean xa_database_init (Database *db, const gchar *db_path) { return db->init (db_path); };
gboolean xa_database_add_application (Database *db, UaiAppInfo *app) { return db->addApplication (app); };

}

#endif // DATABASE_H
