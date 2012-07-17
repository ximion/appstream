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
#include <string>
#include <xapian.h>
#include <glib.h>
#include "uai_internal.h"

using namespace std;

class Database
{
public:
	explicit Database ();
	~Database ();

	bool init (const gchar *dbPath);

	bool addApplication (UaiAppInfo *app);
	bool rebuild (GArray *apps);

private:
	Xapian::WritableDatabase *m_rwXapianDB;
	string m_dbPath;

};

// values used in the database
enum XapianValues {
	APPNAME = 170,
	PKGNAME = 171,
	ICON = 172,
	GETTEXT_DOMAIN = 173,
	ARCHIVE_SECTION = 174,
	ARCHIVE_ARCH = 175,
	POPCON = 176,
	SUMMARY = 177,
	ARCHIVE_CHANNEL = 178,
	DESKTOP_FILE = 179,
	PRICE = 180,
	ARCHIVE_PPA = 181,
	ARCHIVE_DEB_LINE = 182,
	ARCHIVE_SIGNING_KEY_ID = 183,
	PURCHASED_DATE = 184,
	SCREENSHOT_URLS = 185,			 // multiple urls, comma seperated
	SC_DESCRIPTION = 188,
	APPNAME_UNTRANSLATED = 189,
	ICON_URL = 190,
	CATEGORIES = 191,
	LICENSE_KEY = 192,
	LICENSE = 194,
	VIDEO_URL = 195,
	DATE_PUBLISHED = 196,
	SUPPORT_SITE_URL = 197,
	VERSION_INFO = 198,
	SC_SUPPORTED_DISTROS = 199
};

extern "C" {

Database *xa_database_new () { return new Database (); };
void xa_database_free (Database *db) { delete db; };
gboolean xa_database_init (Database *db, const gchar *db_path) { return db->init (db_path); };
gboolean xa_database_add_application (Database *db, UaiAppInfo *app) { return db->addApplication (app); };
gboolean xa_database_rebuild (Database *db, GArray *apps) { return db->rebuild (apps); };

}

#endif // DATABASE_H
