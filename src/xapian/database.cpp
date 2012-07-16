/* database.cpp
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

#include "database.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

Database::Database () :
    m_rwXapianDB(0)
{

}

Database::~Database ()
{
	if (m_rwXapianDB) {
		delete m_rwXapianDB;
	}
}

bool Database::init (const gchar *dbPath)
{
	try {
		m_rwXapianDB = new Xapian::WritableDatabase (dbPath,
							    Xapian::DB_CREATE_OR_OPEN);
	} catch (const Xapian::Error &error) {
		g_warning ("Exception: %s", error.get_msg ().c_str ());
		return false;
	}

	return true;
}

bool Database::addApplication (UaiAppInfo *app)
{
	return false;
}
