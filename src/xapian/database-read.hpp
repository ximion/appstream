/* database-read.hpp
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

#ifndef DATABASE_READ_H
#define DATABASE_READ_H

#include <iostream>
#include <string>
#include <list>
#include <xapian.h>
#include <glib.h>
#include "appstream_internal.h"

using namespace std;

class DatabaseRead
{
public:
	explicit DatabaseRead ();
	~DatabaseRead ();

	bool open (const gchar *dbPath);

	string getSchemaVersion ();

	GArray *getAllApplications ();
	GArray *findApplications (AppstreamSearchQuery *asQuery);

private:
	Xapian::Database m_xapianDB;
	string m_dbPath;
	AppstreamCategory **m_systemCategories;
	int m_systemCategories_len;

	AppstreamAppInfo *docToAppInfo (Xapian::Document);

	Xapian::QueryParser newAppStreamParser ();
	Xapian::Query addCategoryToQuery (Xapian::Query query, Xapian::Query category_query);
	Xapian::Query getQueryForPkgNames (vector<string> pkgnames);
	Xapian::Query getQueryForCategory (AppstreamCategory *cat);
	Xapian::Query queryListFromSearchEntry (AppstreamSearchQuery *asQuery);
};

extern "C" {

DatabaseRead *xa_database_read_new ()
	{ return new DatabaseRead (); };
void xa_database_read_free (DatabaseRead *db)
	{ delete db; };
gboolean xa_database_read_open (DatabaseRead *db, const gchar *db_path)
	{ return db->open (db_path); };
const gchar *xa_database_read_get_schema_version (DatabaseRead *db)
	{ return db->getSchemaVersion ().c_str (); };
GArray *xa_database_read_get_all_applications (DatabaseRead *db)
	{ return db->getAllApplications (); };
GArray *xa_database_read_find_applications (DatabaseRead *db, AppstreamSearchQuery *query)
	{ return db->findApplications (query); };

}

#endif // DATABASE_READ_H
