/* database-write.cpp
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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
#include <sstream>
#include <iterator>
#include <glib/gstdio.h>

#include "database-common.hpp"
#include "../as-utils-private.h"
#include "../as-component-private.h"
#include "../as-settings-private.h"

using namespace std;
using namespace Appstream;

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
DatabaseWrite::initialize (const gchar *dbPath)
{
	m_dbPath = string(dbPath);

	try {
		m_rwXapianDB = new Xapian::WritableDatabase (m_dbPath,
							    Xapian::DB_CREATE_OR_OPEN);
	} catch (const Xapian::Error &error) {
		g_warning ("Exception: %s", error.get_msg ().c_str ());
		return false;
	}

	return true;
}

static
void url_hashtable_to_text (gchar *key, gchar *value, gchar **text)
{
	gchar *tmp;

	tmp = g_strdup (*text);
	g_free (*text);
    *text = g_strdup_printf ("%s\n%s\n%s", tmp, key, value);
	g_free (tmp);
}

bool
DatabaseWrite::rebuild (GList *cpt_list)
{
	string old_path = m_dbPath + "_old";
	string rebuild_path = m_dbPath + "_rb";

	// Create the rebuild directory
	if (!as_utils_touch_dir (rebuild_path.c_str ()))
		return false;

	// check if old unrequired version of db still exists on filesystem
	if (g_file_test (old_path.c_str (), G_FILE_TEST_EXISTS)) {
		g_warning ("Existing xapian old db was not previously cleaned: '%s'.", old_path.c_str ());
		as_utils_delete_dir_recursive (old_path.c_str ());
	}

	// check if old unrequired version of db still exists on filesystem
	if (g_file_test (rebuild_path.c_str (), G_FILE_TEST_EXISTS)) {
		cout << "Removing old rebuild-dir from previous database rebuild." << endl;
		as_utils_delete_dir_recursive (rebuild_path.c_str ());
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

	for (GList *list = cpt_list; list != NULL; list = list->next) {
		AsComponent *cpt = (AsComponent*) list->data;

		Xapian::Document doc;
		term_generator.set_document (doc);

		//! g_debug ("Adding component: %s", as_component_to_string (cpt));

		doc.set_data (as_component_get_name (cpt));

		// Package name
		string pkgname = as_component_get_pkgname (cpt);
		doc.add_value (XapianValues::PKGNAME, pkgname);
		doc.add_term("AP" + pkgname);
		if (pkgname.find ("-") != string::npos) {
			// we need this to work around xapian oddness
			string tmp = pkgname;
			replace (tmp.begin(), tmp.end(), '-', '_');
			doc.add_term (tmp);
		}
		// add packagename as meta-data too
		term_generator.index_text_without_positions (pkgname, WEIGHT_PKGNAME);

		// Identifier
		string idname = as_component_get_idname (cpt);
		doc.add_value (XapianValues::IDENTIFIER, idname);
		doc.add_term("AI" + idname);
		term_generator.index_text_without_positions (idname, WEIGHT_PKGNAME);

		// Untranslated component name
		string cptNameGeneric = as_component_get_name_original (cpt);
		doc.add_value (XapianValues::CPTNAME_UNTRANSLATED, cptNameGeneric);
		term_generator.index_text_without_positions (cptNameGeneric, WEIGHT_DESKTOP_GENERICNAME);

		// Component name
		string cptName = as_component_get_name (cpt);
		doc.add_value (XapianValues::CPTNAME, cptName);

		// Type identifier
		string type_str = as_component_kind_to_string (as_component_get_kind (cpt));
		doc.add_value (XapianValues::TYPE, type_str);

		// URLs
		GHashTable *urls;
		gchar *text = g_strdup ("");
		urls = as_component_get_urls (cpt);
		g_hash_table_foreach(urls, (GHFunc) url_hashtable_to_text, &text);
		doc.add_value (XapianValues::URLS, text);
		g_free (text);

		// Application icon
		doc.add_value (XapianValues::ICON, as_component_get_icon (cpt));
		doc.add_value (XapianValues::ICON_URL, as_component_get_icon_url (cpt));

		// Summary
		string cptSummary = as_component_get_summary (cpt);
		doc.add_value (XapianValues::SUMMARY, cptSummary);
		term_generator.index_text_without_positions (cptSummary, WEIGHT_DESKTOP_SUMMARY);

		// Long description
		string description = as_component_get_description (cpt);
		doc.add_value (XapianValues::DESCRIPTION, description);
		term_generator.index_text_without_positions (description, WEIGHT_DESKTOP_SUMMARY);

		// Categories
		gchar **categories = as_component_get_categories (cpt);

		string categories_str = "";
		for (uint i = 0; categories[i] != NULL; i++) {
			if (categories[i] == NULL)
				continue;

			string cat = categories[i];
			string tmp = cat;
			transform (tmp.begin (), tmp.end (),
					tmp.begin (), ::tolower);
			doc.add_term ("AC" + tmp);
			categories_str += cat + ";";
		}
		doc.add_value (XapianValues::CATEGORIES, categories_str);

		// Add our keywords (with high priority)
		gchar **keywords = as_component_get_keywords (cpt);
		if (keywords != NULL) {
			for (uint i = 0; keywords[i] != NULL; i++) {
				if (keywords[i] == NULL)
					continue;

				string kword = keywords[i];
				term_generator.index_text_without_positions (kword, WEIGHT_DESKTOP_KEYWORD);
			}
		}

		// Data of provided items
		gchar **provides_items = as_ptr_array_to_strv (as_component_get_provided_items (cpt));
		if (provides_items != NULL) {
			gchar *provides_items_str = g_strjoinv ("\n", provides_items);
			doc.add_value (XapianValues::PROVIDED_ITEMS, string(provides_items_str));
			for (uint i = 0; provides_items[i] != NULL; i++) {
				string item = provides_items[i];
				doc.add_term ("AX" + item);
			}
			g_free (provides_items_str);
		}
		g_strfreev (provides_items);

		// Add screenshot information (XML data)
		doc.add_value (XapianValues::SCREENSHOT_DATA, as_component_dump_screenshot_data_xml (cpt));

		// Add compulsory-for-desktop information
		gchar **compulsory = as_component_get_compulsory_for_desktops (cpt);
		string compulsory_str;
		if (compulsory != NULL) {
			gchar *str;
			str = g_strjoinv (";", compulsory);
			compulsory_str = string(str);
			g_free (str);
		}
		doc.add_value (XapianValues::COMPULSORY_FOR, compulsory_str);

		// Add project-license
		doc.add_value (XapianValues::LICENSE, as_component_get_project_license (cpt));

		// Add project group
		doc.add_value (XapianValues::PROJECT_GROUP, as_component_get_project_group (cpt));

		// Add releases information (XML data)
		doc.add_value (XapianValues::RELEASES_DATA, as_component_dump_releases_data_xml (cpt));

		// TODO: Look at the SC Xapian database - there are still some values and terms missing!

		// Postprocess
		string docData = doc.get_data ();
		doc.add_term ("AA" + docData);
		term_generator.index_text_without_positions (docData, WEIGHT_DESKTOP_NAME);

		db.add_document (doc);
	}

	db.set_metadata("db-schema-version", AS_DB_SCHEMA_VERSION);
	db.commit ();

	if (g_rename (m_dbPath.c_str (), old_path.c_str ()) < 0) {
		g_critical ("Error while moving old database out of the way.");
		return false;
	}
	if (g_rename (rebuild_path.c_str (), m_dbPath.c_str ()) < 0) {
		g_critical ("Error while moving rebuilt database.");
		return false;
	}
	as_utils_delete_dir_recursive (old_path.c_str ());

	return true;
}

bool
DatabaseWrite::addComponent (AsComponent *cpt)
{
	// TODO
	return false;
}
