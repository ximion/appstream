/* database-write.cpp
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

#include "database-write.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <glib/gstdio.h>

#include "database-common.hpp"

using namespace std;

DatabaseWrite::DatabaseWrite () :
    m_rwXapianDB(0)
{

}

DatabaseWrite::~DatabaseWrite ()
{
	if (m_rwXapianDB) {
		delete m_rwXapianDB;
	}
}

bool
DatabaseWrite::init (const gchar *dbPath)
{
	m_dbPath = dbPath;

	try {
		m_rwXapianDB = new Xapian::WritableDatabase (m_dbPath,
							    Xapian::DB_CREATE_OR_OPEN);
	} catch (const Xapian::Error &error) {
		cout << "ERROR!" << endl;
		g_warning ("Exception: %s", error.get_msg ().c_str ());
		return false;
	}

	return true;
}

bool
DatabaseWrite::rebuild (GArray *apps)
{
	string old_path = m_dbPath + "_old";
	string rebuild_path = m_dbPath + "_rb";

	// Create the rebuild directory
	if (!appstream_utils_touch_dir (rebuild_path.c_str ()))
		return false;

	// check if old unrequired version of db still exists on filesystem
	if (g_file_test (old_path.c_str (), G_FILE_TEST_EXISTS)) {
		g_warning ("Existing xapian old db was not previously cleaned: '%s'.", old_path.c_str ());
		appstream_utils_delete_dir_recursive (old_path.c_str ());
	}

	// check if old unrequired version of db still exists on filesystem
	if (g_file_test (rebuild_path.c_str (), G_FILE_TEST_EXISTS)) {
		cout << "Removing old rebuild-dir from previous database rebuild." << endl;
		appstream_utils_delete_dir_recursive (rebuild_path.c_str ());
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
		AppstreamAppInfo *app = g_array_index (apps, AppstreamAppInfo*, i);

		Xapian::Document doc;

		cout << "Adding application: " << appstream_app_info_to_string (app) << endl;

		doc.set_data (appstream_app_info_get_name (app));

		// Package name
		string pkgname = appstream_app_info_get_pkgname (app);
		doc.add_value (PKGNAME, pkgname);
		doc.add_term("AP" + pkgname);
		if (pkgname.find ("-") != string::npos) {
			// we need this to work around xapian oddness
			string tmp = pkgname;
			replace (tmp.begin(), tmp.end(), '-', '_');
			doc.add_term (tmp);
		}
		term_generator.index_text_without_positions (pkgname, WEIGHT_PKGNAME);

		// Untranslated application name
		doc.add_value (APPNAME_UNTRANSLATED, appstream_app_info_get_name_original (app));

		// Application name
		string appName = appstream_app_info_get_name (app);
		doc.add_value (APPNAME, appName);
		doc.add_term ("AA" + appName);
		term_generator.index_text_without_positions (appName, WEIGHT_DESKTOP_NAME);

		// Desktop file
		doc.add_value (DESKTOP_FILE, appstream_app_info_get_desktop_file (app));

		// URL
		doc.add_value (SUPPORT_SITE_URL, appstream_app_info_get_url (app));

		// Application stock icon
		doc.add_value (ICON, appstream_app_info_get_icon (app));

		// Summary
		string appSummary = appstream_app_info_get_summary (app);
		doc.add_value (SUMMARY, appSummary);
		term_generator.index_text_without_positions (appSummary, WEIGHT_SUMMARY);

		// Long description
		doc.add_value (SC_DESCRIPTION, appstream_app_info_get_description (app));

		// Categories
		int length = 0;
		AppstreamCategory **categories = appstream_app_info_get_categories (app, &length);
		string categories_string = "";
		for (uint i=0; i < length; i++) {
			if (categories[i] == NULL)
				continue;

			string cat = appstream_category_get_id (categories[i]);
			string tmp = cat;
			transform (tmp.begin (), tmp.end (),
					tmp.begin (), ::tolower);
			doc.add_term ("AC" + tmp);
			categories_string += cat + ";";
		}
		doc.add_value (CATEGORIES, categories_string);

		// Add our keywords (with high priority)
		length = 0;
		gchar **keywords = appstream_app_info_get_keywords (app, &length);
		for (uint i=0; i < length; i++) {
			if (keywords[i] == NULL)
				continue;

			string kword = keywords[i];
			term_generator.index_text_without_positions (kword, WEIGHT_DESKTOP_KEYWORD);
		}

		// TODO: Look at the SC Xapian database - there are still some values and terms missing!

		term_generator.set_document (doc);

		db.add_document (doc);
	}

	db.set_metadata("db-schema-version", APPSTREAM_DB_SCHEMA_VERSION);
	db.flush ();

	if (g_rename (m_dbPath.c_str (), old_path.c_str ()) < 0) {
		g_critical ("Error while moving old database out of the way.");
		return false;
	}
	if (g_rename (rebuild_path.c_str (), m_dbPath.c_str ()) < 0) {
		g_critical ("Error while moving rebuilt database.");
		return false;
	}
	appstream_utils_delete_dir_recursive (old_path.c_str ());

	return true;
}

bool
DatabaseWrite::addApplication (AppstreamAppInfo *app)
{
	// TODO
	return false;
}
