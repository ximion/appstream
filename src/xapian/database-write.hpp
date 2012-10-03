/* database-write.hpp
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

#endif // DATABASE_WRITE_H
