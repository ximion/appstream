/* database-read.cpp
 *
 * Copyright (C) 2012-2013 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2009 Michael Vogt <mvo@debian.org>
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

#include "database-read.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include <glib/gstdio.h>

#include "database-common.hpp"

using namespace std;
using namespace AppStream;

DatabaseRead::DatabaseRead () :
    m_xapianDB(0)
{
	// we cache these for performance reasons
	m_systemCategories = appstream_get_system_categories ();
}

DatabaseRead::~DatabaseRead ()
{

}

bool
DatabaseRead::open (const gchar *dbPath)
{
	m_dbPath = dbPath;

	try {
		m_xapianDB = Xapian::Database (m_dbPath);
	} catch (const Xapian::Error &error) {
		g_warning ("Exception: %s", error.get_msg ().c_str ());
		return false;
	}

	return true;
}

string
DatabaseRead::getSchemaVersion ()
{
	return m_xapianDB.get_metadata ("db-schema-version");
}

AppstreamAppInfo*
DatabaseRead::docToAppInfo (Xapian::Document doc)
{
	AppstreamAppInfo *app = appstream_app_info_new ();

	// Application name
	string appName = doc.get_value (XapianValues::APPNAME);
	appstream_app_info_set_name (app, appName.c_str ());

	// Package name
	string pkgName = doc.get_value (XapianValues::PKGNAME);;
	appstream_app_info_set_pkgname (app, pkgName.c_str ());

	// Untranslated application name
	string appname_orig = doc.get_value (XapianValues::APPNAME_UNTRANSLATED);
	appstream_app_info_set_name_original (app, appname_orig.c_str ());

	// Desktop file
	string desktopFile = doc.get_value (XapianValues::DESKTOP_FILE);
	appstream_app_info_set_desktop_file (app, desktopFile.c_str ());

	// URL
	string appUrl = doc.get_value (XapianValues::URL_HOMEPAGE);
	appstream_app_info_set_url (app, appUrl.c_str ());

	// Application stock icon
	string appIconName = doc.get_value (XapianValues::ICON);
	appstream_app_info_set_icon (app, appIconName.c_str ());

	// Summary
	string appSummary = doc.get_value (XapianValues::SUMMARY);
	appstream_app_info_set_summary (app, appSummary.c_str ());

	// Long description
	string appDescription = doc.get_value (XapianValues::DESCRIPTION);
	appstream_app_info_set_description (app, appDescription.c_str ());

	// Categories
	string categories_string = doc.get_value (XapianValues::CATEGORIES);
	appstream_app_info_set_categories_from_str (app, categories_string.c_str ());

	// TODO

	return app;
}

static vector<std::string> &split(const string &s, char delim, vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

static vector<std::string> split (const string &s, char delim) {
    std::vector<std::string> elems;
    return split (s, delim, elems);
}

Xapian::QueryParser
DatabaseRead::newAppStreamParser ()
{
	Xapian::QueryParser xapian_parser = Xapian::QueryParser ();
        xapian_parser.set_database (m_xapianDB);
        xapian_parser.add_boolean_prefix ("pkg", "XP");
        xapian_parser.add_boolean_prefix ("pkg", "AP");
        xapian_parser.add_boolean_prefix ("mime", "AM");
        xapian_parser.add_boolean_prefix ("section", "XS");
        xapian_parser.add_boolean_prefix ("origin", "XOC");
        xapian_parser.add_prefix ("pkg_wildcard", "XP");
        xapian_parser.add_prefix ("pkg_wildcard", "AP");
        xapian_parser.set_default_op (Xapian::Query::OP_AND);
        return xapian_parser;
}

/**
 * Helper method that adds the current category to the query
 */
Xapian::Query
DatabaseRead::addCategoryToQuery (Xapian::Query query, Xapian::Query category_query)
{
	if (category_query.empty ())
		return query;

	return Xapian::Query (Xapian::Query::OP_AND,
				category_query,
				query);
}

/**
 * Return a Xapian query that matches exactly the list of pkgnames
 */
Xapian::Query
DatabaseRead::getQueryForPkgNames (vector<string> pkgnames)
{
	Xapian::Query query = Xapian::Query ();

	for (vector<string>::iterator it = pkgnames.begin(); it != pkgnames.end(); ++it) {
		query = Xapian::Query (Xapian::Query::OP_OR,
					query,
					Xapian::Query ("XP" + *it));
		query = Xapian::Query (Xapian::Query::OP_OR,
					query,
					Xapian::Query ("AP" + *it));
	}

	return query;
}

Xapian::Query
DatabaseRead::getQueryForCategory (gchar *cat_id)
{
	string catid_lower = cat_id;
	transform (catid_lower.begin (), catid_lower.end (),
		   catid_lower.begin (), ::tolower);
	return Xapian::Query("AC" + catid_lower);
}

/**
 * Get Xapian::Query from a search term string and a limit the
 * search to the given category
 */
Xapian::Query
DatabaseRead::queryListFromSearchEntry (AppstreamSearchQuery *asQuery)
{
	// prepare search-term
	appstream_search_query_sanitize_search_term (asQuery);
	string search_term = appstream_search_query_get_search_term (asQuery);
	bool searchAll = appstream_search_query_get_search_all_categories (asQuery);

	// generate category query
	Xapian::Query category_query = Xapian::Query ();
	int length = 0;
	gchar **categories = appstream_search_query_get_categories (asQuery, &length);
	string categories_string = "";
	for (uint i=0; i < length; i++) {
		gchar *cat_id = categories[i];

		category_query = Xapian::Query (Xapian::Query::OP_OR,
						 category_query,
						 getQueryForCategory (cat_id));
	}

	// empty query returns a query that matches nothing (for performance
	// reasons)
	if ((search_term.compare ("") == 0) && (searchAll))
		return Xapian::Query ();

	// we cheat and return a match-all query for single letter searches
	if (search_term.length () < 2)
            return addCategoryToQuery (Xapian::Query (""), category_query);

	// get a pkg query
	Xapian::Query pkg_query = Xapian::Query ();
	if (search_term.find (",") != string::npos) {
		pkg_query = getQueryForPkgNames (split (search_term, ','));
	} else {
		vector<string> terms = split (search_term, '\n');
		for (vector<string>::iterator it = terms.begin(); it != terms.end(); ++it) {
			pkg_query = Xapian::Query (Xapian::Query::OP_OR,
						   Xapian::Query("XP" + *it),
						   pkg_query);
		}
	}
	pkg_query = addCategoryToQuery (pkg_query, category_query);

	// get a search query
        if (search_term.find (":") == string::npos) {  // ie, not a mimetype query
		// we need this to work around xapian oddness
		replace (search_term.begin(), search_term.end(), '-', '_');
	}

	Xapian::QueryParser parser = newAppStreamParser ();
	Xapian::Query fuzzy_query = parser.parse_query (search_term,
					  Xapian::QueryParser::FLAG_PARTIAL |
					  Xapian::QueryParser::FLAG_BOOLEAN);
	// if the query size goes out of hand, omit the FLAG_PARTIAL
	// (LP: #634449)
	if (fuzzy_query.get_length () > 1000)
		fuzzy_query = parser.parse_query(search_term,
						 Xapian::QueryParser::FLAG_BOOLEAN);
	// now add categories
	fuzzy_query = addCategoryToQuery (fuzzy_query, category_query);

        return (pkg_query, fuzzy_query);
}

GPtrArray*
DatabaseRead::findApplications (AppstreamSearchQuery *asQuery)
{
	// Create new array to store the app-info objects
	GPtrArray *appArray = g_ptr_array_new ();

	Xapian::Query query = queryListFromSearchEntry (asQuery);
	query.serialise ();

	Xapian::Enquire enquire = Xapian::Enquire (m_xapianDB);
	enquire.set_query (query);

	Xapian::MSet matches = enquire.get_mset (0, m_xapianDB.get_doccount ());
	for (Xapian::MSetIterator it = matches.begin(); it != matches.end(); ++it) {
		Xapian::Document doc = it.get_document ();

		AppstreamAppInfo *app = docToAppInfo (doc);
		g_ptr_array_add (appArray, app);
	}

	return appArray;
}

GPtrArray*
DatabaseRead::getAllApplications ()
{
	// Create new array to store the app-info objects
	GPtrArray *appArray = g_ptr_array_new ();

	// Iterate through all Xapian documents
	Xapian::PostingIterator it = m_xapianDB.postlist_begin (string());
	while (it != m_xapianDB.postlist_end(string())) {
		Xapian::docid did = *it;

		Xapian::Document doc = m_xapianDB.get_document (did);
		AppstreamAppInfo *app = docToAppInfo (doc);
		g_ptr_array_add (appArray, app);

		++it;
	}

	return appArray;
}