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

	m_dbPath = dbPath;

	return true;
}

bool Database::rebuild (GArray *apps)
{
	string old_path = m_dbPath + "_old";
	string rebuild_path = m_dbPath + "_rb";

	// Create the rebuild directory
	if (!uai_utils_touch_dir (rebuild_path.c_str ()))
		return false;

	// check if old unrequired version of db still exists on filesystem
	if (g_file_test (old_path.c_str (), G_FILE_TEST_EXISTS)) {
		g_warning ("Existing xapian old db was not previously cleaned: '%s'.", old_path.c_str ());
		uai_utils_delete_dir_recursive (old_path.c_str ());
	}

	Xapian::WritableDatabase db (rebuild_path, Xapian::DB_CREATE_OR_OVERWRITE);

	Xapian::TermGenerator term_generator;
	term_generator.set_database(db);
	try {
		/* this tests if we have spelling suggestions (there must be
		 * a better way?!?) - this is needed as inmemory does not have
		 * spelling corrections, but it allows setting the flag and will
		 * raise a exception much later
		 */
		db.add_spelling("test");
		db.remove_spelling("test");

		/* this enables the flag for it (we only reach this line if
		 * the db supports spelling suggestions)
		 */
		term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
	} catch (const Xapian::UnimplementedError &error) {
		// Ignore
	}

	for (guint i=0; i < apps->len; i++) {
		UaiAppInfo *app = g_array_index (apps, UaiAppInfo*, i);

		Xapian::Document doc;

		doc.set_data (uai_app_info_get_name (app));

		doc.add_value (APPNAME_UNTRANSLATED, uai_app_info_get_name_original (app));
		doc.add_value (PKGNAME, uai_app_info_get_pkgname (app));
		doc.add_value (DESKTOP_FILE, uai_app_info_get_desktop_file (app));
		doc.add_value (SUPPORT_SITE_URL, uai_app_info_get_url (app));
		doc.add_value (ICON, uai_app_info_get_icon (app));
		doc.add_value (CATEGORIES, uai_app_info_get_categories (app));
		doc.add_value (SUMMARY, uai_app_info_get_summary (app));
		doc.add_value (SC_DESCRIPTION, uai_app_info_get_description (app));
		// TODO: Register more values and add TERMs

		term_generator.set_document (doc);

		// TODO: Register terms

		db.add_document (doc);

	}

	db.set_metadata("db-schema-version", DB_SCHEMA_VERSION);
	db.flush ();

	return true;
}

bool Database::addApplication (UaiAppInfo *app)
{
	// TODO
	return false;
}
