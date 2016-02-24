/* database-read.cpp
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2009 Michael Vogt <mvo@debian.org>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
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

#include "database-schema.hpp"
#include "../as-menu-parser.h"
#include "../as-component-private.h"

using namespace std;
using namespace ASCache;

DatabaseRead::DatabaseRead () :
    m_dbPath("")
{

}

DatabaseRead::~DatabaseRead ()
{
	m_xapianDB.close ();
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

	m_dbLocale = m_xapianDB.get_metadata ("db-locale");
	if (m_dbLocale.empty ())
		m_dbLocale = "C";

	try {
		m_schemaVersion = stoi (m_xapianDB.get_metadata ("db-schema-version"));
	} catch (...) {
		g_warning ("Unable to read database schema version, assuming 0.");
		m_schemaVersion = 0;
	}

	if (m_schemaVersion != AS_DB_SCHEMA_VERSION) {
		g_warning ("Attempted to open an old version of the AppStream cache. Please refresh the cache and try again!");
		return false;
	}

	return true;
}

int
DatabaseRead::getSchemaVersion ()
{
	return m_schemaVersion;
}

string
DatabaseRead::getLocale ()
{
	return m_dbLocale;
}

AsComponent*
DatabaseRead::docToComponent (Xapian::Document doc)
{
	AsComponent *cpt = as_component_new ();
	string str;

	/* set component active languge (which is the locale the database was built for) */
	as_component_set_active_locale (cpt, m_dbLocale.c_str ());

	// Component type/kind
	string type_str = doc.get_value (XapianValues::TYPE);
	as_component_set_kind (cpt, as_component_kind_from_string (type_str.c_str ()));

	// Identifier
	string id_str = doc.get_value (XapianValues::IDENTIFIER);
	as_component_set_id (cpt, id_str.c_str ());

	// Component name
	string cptName = doc.get_value (XapianValues::CPTNAME);
	as_component_set_name (cpt, cptName.c_str (), NULL);
	cptName = doc.get_value (XapianValues::CPTNAME_UNTRANSLATED);
	as_component_set_name (cpt, cptName.c_str (), "C");

	// Package name
	string pkgNamesStr = doc.get_value (XapianValues::PKGNAMES);
	if (!pkgNamesStr.empty ()) {
		g_auto(GStrv) pkgs = g_strsplit (pkgNamesStr.c_str (), ";", -1);
		as_component_set_pkgnames (cpt, pkgs);
	}

	// Source package name
	string cptSPkg = doc.get_value (XapianValues::SOURCE_PKGNAME);
	as_component_set_source_pkgname (cpt, cptSPkg.c_str ());

	// Origin
	string cptOrigin = doc.get_value (XapianValues::ORIGIN);
	as_component_set_origin (cpt, cptOrigin.c_str ());

	// Bundles
	Bundles bundles;
	str = doc.get_value (XapianValues::BUNDLES);
	bundles.ParseFromString (str);
	for (int i = 0; i < bundles.bundle_size (); i++) {
		const Bundles_Bundle& bdl = bundles.bundle (i);
		AsBundleKind bkind = (AsBundleKind) bdl.type ();
		if (bkind != AS_BUNDLE_KIND_UNKNOWN)
			as_component_add_bundle_id (cpt, bkind, bdl.id ().c_str ());
	}

	// Extends
	string extendsStr = doc.get_value (XapianValues::EXTENDS);
	if (!extendsStr.empty ()) {
		g_auto(GStrv) extends = g_strsplit (extendsStr.c_str (), ";", -1);
		for (uint i = 0; extends[i] != NULL; i++) {
			as_component_add_extends (cpt, extends[i]);
		}
	}

	// Extensions
	string extsStr = doc.get_value (XapianValues::EXTENSIONS);
	if (!extsStr.empty ()) {
		g_auto(GStrv) exts = g_strsplit (extsStr.c_str (), ";", -1);
		for (uint i = 0; exts[i] != NULL; i++) {
			as_component_add_extension (cpt, exts[i]);
		}
	}

	// URLs
	Urls urls;
	str = doc.get_value (XapianValues::URLS);
	urls.ParseFromString (str);
	for (int i = 0; i < urls.url_size (); i++) {
		const Urls_Url& url = urls.url (i);
		AsUrlKind ukind = (AsUrlKind) url.type ();
		if (ukind != AS_URL_KIND_UNKNOWN)
			as_component_add_url (cpt, ukind, url.url ().c_str ());
	}

	// Icons
	Icons icons;
	str = doc.get_value (XapianValues::ICONS);
	icons.ParseFromString (str);
	for (int i = 0; i < icons.icon_size (); i++) {
		const Icons_Icon& pbIcon = icons.icon (i);

		AsIcon *icon = as_icon_new ();
		as_icon_set_width (icon, pbIcon.width ());
		as_icon_set_height (icon, pbIcon.height ());

		if (pbIcon.type () == Icons_IconType_REMOTE) {
			as_icon_set_kind (icon, AS_ICON_KIND_REMOTE);
			as_icon_set_url (icon, pbIcon.url ().c_str ());
		} else {
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
			as_icon_set_filename (icon, pbIcon.url ().c_str ());
		}
		as_component_add_icon (cpt, icon);
		g_object_unref (icon);
	}

	// Summary
	string appSummary = doc.get_value (XapianValues::SUMMARY);
	as_component_set_summary (cpt, appSummary.c_str (), NULL);

	// Long description
	string appDescription = doc.get_value (XapianValues::DESCRIPTION);
	as_component_set_description (cpt, appDescription.c_str (), NULL);

	// Categories
	string categories_str = doc.get_value (XapianValues::CATEGORIES);
	as_component_set_categories_from_str (cpt, categories_str.c_str ());

	// Provided items
	ASCache::ProvidedItems pbPI;
	str = doc.get_value (XapianValues::PROVIDED_ITEMS);
	pbPI.ParseFromString (str);
	for (int i = 0; i < pbPI.provided_size (); i++) {
		const ProvidedItems_Provided& pbProv = pbPI.provided (i);

		AsProvided *prov = as_provided_new ();
		as_provided_set_kind (prov, (AsProvidedKind) pbProv.type ());

		for (int j = 0; j < pbProv.item_size (); j++) {
			const string& item = pbProv.item (j);
			as_provided_add_item (prov, item.c_str ());
		}

		as_component_add_provided (cpt, prov);
	}

	// Screenshot data
	Screenshots screenshots;
	str = doc.get_value (XapianValues::SCREENSHOTS);
	screenshots.ParseFromString (str);
	for (int i = 0; i < screenshots.screenshot_size (); i++) {
		const Screenshots_Screenshot& pb_scr = screenshots.screenshot (i);
		AsScreenshot *scr = as_screenshot_new ();
		as_screenshot_set_active_locale (scr, m_dbLocale.c_str ());

		if (pb_scr.primary ())
			as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_DEFAULT);
		else
			as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_EXTRA);

		if (pb_scr.has_caption ())
			as_screenshot_set_caption (scr, pb_scr.caption ().c_str (), NULL);

		for (int j = 0; j < pb_scr.image_size (); j++) {
			const Screenshots_Image& pb_img = pb_scr.image (j);
			AsImage *img = as_image_new ();

			if (pb_img.source ()) {
				as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
			} else {
				as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
			}

			as_image_set_width (img, pb_img.width ());
			as_image_set_height (img, pb_img.height ());
			as_image_set_url (img, pb_img.url ().c_str ());

			as_screenshot_add_image (scr, img);
			g_object_unref (img);
		}

		as_component_add_screenshot (cpt, scr);
		g_object_unref (scr);
	}

	// Compulsory-for-desktop information
	string compulsory_str = doc.get_value (XapianValues::COMPULSORY_FOR);
	gchar **strv = g_strsplit (compulsory_str.c_str (), ";", -1);
	as_component_set_compulsory_for_desktops (cpt, strv);
	g_strfreev (strv);

	// License
	string license = doc.get_value (XapianValues::LICENSE);
	as_component_set_project_license (cpt, license.c_str ());

	// Project group
	string project_group = doc.get_value (XapianValues::PROJECT_GROUP);
	as_component_set_project_group (cpt, project_group.c_str ());

	// Source package name
	string developerName = doc.get_value (XapianValues::DEVELOPER_NAME);
	as_component_set_developer_name (cpt, developerName.c_str (), NULL);

	// Releases data
	Releases pb_rels;
	str = doc.get_value (XapianValues::RELEASES);
	pb_rels.ParseFromString (str);
	for (int i = 0; i < pb_rels.release_size (); i++) {
		const Releases_Release& pb_rel = pb_rels.release (i);
		AsRelease *rel = as_release_new ();
		as_release_set_active_locale (rel, m_dbLocale.c_str ());

		as_release_set_version (rel, pb_rel.version ().c_str ());
		as_release_set_timestamp (rel, pb_rel.unix_timestamp ());
		if (pb_rel.has_urgency ())
			as_release_set_urgency (rel, (AsUrgencyKind) pb_rel.urgency ());

		if (pb_rel.has_description ())
			as_release_set_description (rel, pb_rel.description ().c_str (), NULL);

		// load locations
		for (int j = 0; j < pb_rel.location_size (); j++)
			as_release_add_location (rel, pb_rel.location (j).c_str ());

		// load checksums
		for (int j = 0; j < pb_rel.checksum_size (); j++) {
			const Releases_Checksum& pb_cs = pb_rel.checksum (j);
			AsChecksumKind cskind = (AsChecksumKind) pb_cs.type ();

			if (cskind >= AS_CHECKSUM_KIND_LAST) {
				g_warning ("Found invalid release-checksum type in database for component '%s'", id_str.c_str ());
				continue;
			}
			as_release_set_checksum (rel, pb_cs.value ().c_str (), cskind);
		}

		// load sizes
		for (int j = 0; j < pb_rel.size_size (); j++) {
			const Releases_Size& pb_s = pb_rel.size (j);
			AsSizeKind skind = (AsSizeKind) pb_s.type ();

			if (skind >= AS_SIZE_KIND_LAST) {
				g_warning ("Found invalid release-size type in database for component '%s'", id_str.c_str ());
				continue;
			}
			as_release_set_size (rel, pb_s.value (), skind);
		}

		as_component_add_release (cpt, rel);
		g_object_unref (rel);
	}

	// Languages
	Languages langs;
	str = doc.get_value (XapianValues::LANGUAGES);
	langs.ParseFromString (str);
	for (int i = 0; i < langs.language_size (); i++) {
		const Languages_Language& lang = langs.language (i);

		as_component_add_language (cpt,
					   lang.locale ().c_str (),
					   lang.percentage ());
	}

	// TODO: Read out keywords? - actually not necessary, since they're already in the database and used by the search engine
	//       or do we really need to know what the defined keywords have been for each component?

	return cpt;
}

static vector<std::string> &split(const string &s, char delim, vector<std::string> &elems)
{
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
	xapian_parser.add_boolean_prefix ("id", "AI");
	xapian_parser.add_boolean_prefix ("pkg", "AP");
	xapian_parser.add_boolean_prefix ("provides", "AE");
	xapian_parser.add_boolean_prefix ("section", "XS");
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
vector<Xapian::Query>
DatabaseRead::queryListForTermCats (const gchar *term, gchar **categories)
{
	// prepare search-term
	bool globalSearch = false;
	string search_term = term == NULL ? "" : term;
	if (categories == NULL)
		globalSearch = true;

	// generate category query (if we are not searching globally in all categories)
	Xapian::Query category_query = Xapian::Query ();
	if (not globalSearch) {
		for (uint i = 0; categories[i] != NULL; i++) {
			gchar *cat_id = categories[i];

			category_query = Xapian::Query (Xapian::Query::OP_OR,
						 category_query,
						 getQueryForCategory (cat_id));
		}
	}

	// empty query returns a query that matches nothing (for performance
	// reasons)
	// We caught the "both criteria zero" case way earlier, so this is just additional safety.
	if ((search_term.compare ("") == 0) && (globalSearch)) {
		Xapian::Query vv[2] = { Xapian::Query(), Xapian::Query () };
		vector<Xapian::Query> res(&vv[0], &vv[0]+2);
		return res;
	}

	// we cheat and return a match-all query for single letter searches
	if (search_term.length () < 2) {
		Xapian::Query allQuery = addCategoryToQuery (Xapian::Query (""), category_query);
		// NOTE: I want C++11!
		Xapian::Query vv[2] = { allQuery, allQuery };
		vector<Xapian::Query> res(&vv[0], &vv[0]+2);
		return res;
	}

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

	Xapian::Query vv[2] = { pkg_query, fuzzy_query };
	vector<Xapian::Query> res(&vv[0], &vv[0]+2);
	return res;
}

void
DatabaseRead::appendSearchResults (Xapian::Enquire enquire, GPtrArray *cptArray)
{
	Xapian::MSet matches = enquire.get_mset (0, m_xapianDB.get_doccount ());
	for (Xapian::MSetIterator it = matches.begin(); it != matches.end(); ++it) {
		Xapian::Document doc = it.get_document ();

		AsComponent *cpt = docToComponent (doc);
		g_ptr_array_add (cptArray, g_object_ref (cpt));
	}
}

GPtrArray*
DatabaseRead::findComponents (const gchar *term, gchar **cats)
{
	// Create new array to store the AsComponent objects
	GPtrArray *cptArray = g_ptr_array_new_with_free_func (g_object_unref);

	// "normal" query
	auto qlist = queryListForTermCats (term, cats);
	Xapian::Query query = qlist[0];
	query.serialise ();

	Xapian::Enquire enquire = Xapian::Enquire (m_xapianDB);
	enquire.set_query (query);
	appendSearchResults (enquire, cptArray);

	// do fuzzy query if we got no results
	if (cptArray->len == 0) {
		query = qlist[1];
		query.serialise ();

		enquire = Xapian::Enquire (m_xapianDB);
		enquire.set_query (query);
		appendSearchResults (enquire, cptArray);
	}

	return cptArray;
}

GPtrArray*
DatabaseRead::getAllComponents ()
{
	// Create new array to store the app-info objects
	GPtrArray *appArray = g_ptr_array_new_with_free_func (g_object_unref);

	// Iterate through all Xapian documents
	Xapian::PostingIterator it = m_xapianDB.postlist_begin (string());
	while (it != m_xapianDB.postlist_end(string())) {
		Xapian::docid did = *it;

		Xapian::Document doc = m_xapianDB.get_document (did);
		AsComponent *app = docToComponent (doc);
		g_ptr_array_add (appArray, g_object_ref (app));

		++it;
	}

	return appArray;
}

AsComponent*
DatabaseRead::getComponentById (const gchar *idname)
{
	Xapian::Query id_query = Xapian::Query (Xapian::Query::OP_OR,
						   Xapian::Query("AI" + string(idname)),
						   Xapian::Query ());
	id_query.serialise ();

	Xapian::Enquire enquire = Xapian::Enquire (m_xapianDB);
	enquire.set_query (id_query);

	Xapian::MSet matches = enquire.get_mset (0, m_xapianDB.get_doccount ());
	if (matches.size () > 1) {
		g_warning ("Found more than one component with id '%s'! Returning the first one.", idname);
	}
	if (matches.size () <= 0)
		return NULL;

	Xapian::Document doc = matches[matches.get_firstitem ()].get_document ();
	AsComponent *cpt = docToComponent (doc);

	return cpt;
}

GPtrArray*
DatabaseRead::getComponentsByProvides (AsProvidedKind kind, const gchar *item)
{
	/* Create new array to store the AsComponent objects */
	GPtrArray *cptArray = g_ptr_array_new_with_free_func (g_object_unref);

	Xapian::Query item_query;
	gchar *element_id;
	element_id = g_strdup_printf ("%s;%s",
					 as_provided_kind_to_string (kind),
					 item);
	item_query = Xapian::Query (Xapian::Query::OP_OR,
					   Xapian::Query("AE" + string(element_id)),
					   Xapian::Query ());
	g_free (element_id);

	item_query.serialise ();

	Xapian::Enquire enquire = Xapian::Enquire (m_xapianDB);
	enquire.set_query (item_query);
	appendSearchResults (enquire, cptArray);

	return cptArray;
}

GPtrArray*
DatabaseRead::getComponentsByKind (AsComponentKind kind)
{
	GPtrArray *cptArray = g_ptr_array_new_with_free_func (g_object_unref);

	auto kind_query = Xapian::Query (Xapian::Query::OP_OR,
			   Xapian::Query("AT" + string(as_component_kind_to_string (kind))),
			   Xapian::Query());

	kind_query.serialise ();

	Xapian::Enquire enquire = Xapian::Enquire (m_xapianDB);
	enquire.set_query (kind_query);
	appendSearchResults (enquire, cptArray);

	return cptArray;
}
