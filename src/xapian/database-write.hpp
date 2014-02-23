/* database-write.hpp
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

#ifndef DATABASE_WRITE_H
#define DATABASE_WRITE_H

#include <iostream>
#include <string>
#include <xapian.h>
#include <glib.h>
#include "appstream_internal.h"

using namespace std;

/* _VERY_ ugly hack to make C++ and Vala work together */
struct XADatabaseWrite {};

class DatabaseWrite : public XADatabaseWrite
{
public:
	explicit DatabaseWrite ();
	~DatabaseWrite ();

	bool initialize (const gchar *dbPath);

	bool addApplication (AppStreamAppInfo *app);
	bool rebuild (GArray *apps);

private:
	Xapian::WritableDatabase *m_rwXapianDB;
	string m_dbPath;

};

inline DatabaseWrite* realDbWrite (XADatabaseWrite* d) { return static_cast<DatabaseWrite*>(d); }

#endif // DATABASE_WRITE_H
