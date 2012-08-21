/* database-read.cpp
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

#include "database-read.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <glib/gstdio.h>

using namespace std;

DatabaseRead::DatabaseRead () :
    m_xapianDB(0)
{

}

DatabaseRead::~DatabaseRead ()
{
	if (m_xapianDB) {
		delete m_xapianDB;
	}
}

bool DatabaseRead::open (const gchar *dbPath)
{
	m_dbPath = dbPath;

	try {
		m_xapianDB = new Xapian::Database (m_dbPath);
	} catch (const Xapian::Error &error) {
		cout << "ERROR!" << endl;
		g_warning ("Exception: %s", error.get_msg ().c_str ());
		return false;
	}

	return true;
}
