/* database-read.hpp
 *
 * Copyright (C) 2012 Matthias Klumpp
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
	GList *m_systemCategories;

	AppstreamAppInfo *docToAppInfo (Xapian::Document);

	Xapian::QueryParser newAppStreamParser ();
	Xapian::Query addCategoryToQuery (Xapian::Query query, Xapian::Query category_query);
	Xapian::Query getQueryForPkgNames (vector<string> pkgnames);
	Xapian::Query getQueryForCategory (gchar *cat_id);
	Xapian::Query queryListFromSearchEntry (AppstreamSearchQuery *asQuery);
};

#endif // DATABASE_READ_H
