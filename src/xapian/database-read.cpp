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

#include "database-common.hpp"

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

bool
DatabaseRead::open (const gchar *dbPath)
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

string
DatabaseRead::getSchemaVersion ()
{
	return m_xapianDB->get_metadata ("db-schema-version");
}

AppstreamAppInfo*
DatabaseRead::docToAppInfo (Xapian::Document doc)
{
	AppstreamAppInfo *app = appstream_app_info_new ();

	// Application name
	string appName = doc.get_value (APPNAME);
	appstream_app_info_set_name (app, appName.c_str ());

	// Package name
	string pkgName = doc.get_value (PKGNAME);;
	appstream_app_info_set_pkgname (app, pkgName.c_str ());

	// Untranslated application name
	string appname_orig = doc.get_value (APPNAME_UNTRANSLATED);
	appstream_app_info_set_name_original (app, appname_orig.c_str ());

	// Desktop file
	string desktopFile = doc.get_value (DESKTOP_FILE);
	appstream_app_info_set_desktop_file (app, desktopFile.c_str ());

	// URL
	string appUrl = doc.get_value (SUPPORT_SITE_URL);
	appstream_app_info_set_url (app, appUrl.c_str ());

	// Application stock icon
	string appIconName = doc.get_value (ICON);
	appstream_app_info_set_icon (app, appIconName.c_str ());

	// Summary
	string appSummary = doc.get_value (SUMMARY);
	appstream_app_info_set_summary (app, appSummary.c_str ());

	// Long description
	string appDescription = doc.get_value (SC_DESCRIPTION);
	appstream_app_info_set_description (app, appDescription.c_str ());

	// Categories
	string categories_string = doc.get_value (CATEGORIES);
	appstream_app_info_set_categories_from_str (app, categories_string.c_str ());

	// TODO

	return app;
}

GArray*
DatabaseRead::getAllApplications ()
{
	// Create new array to store the app-info objects
	GArray *appArray = g_array_new (true, true, sizeof (AppstreamAppInfo*));

	// Iterate through all Xapian documents
	Xapian::PostingIterator it = m_xapianDB->postlist_begin (string());
	while (it != m_xapianDB->postlist_end(string())) {
		Xapian::docid did = *it;

		Xapian::Document doc = m_xapianDB->get_document (did);
		AppstreamAppInfo *app = docToAppInfo (doc);
		g_array_append_val (appArray, app);

		++it;
	}

	return appArray;
}