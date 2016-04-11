/* database-write.cpp
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#include "database-write.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iterator>
#include <glib/gstdio.h>

#include "database-schema.hpp"
#include "../as-utils.h"
#include "../as-utils-private.h"
#include "../as-component-private.h"
#include "../as-settings-private.h"

using namespace std;
using namespace ASCache;

DatabaseWrite::DatabaseWrite () :
    m_rwXapianDB(nullptr)
{
}

DatabaseWrite::~DatabaseWrite ()
{
	if (m_rwXapianDB) {
		m_rwXapianDB->close ();
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

/**
 * Helper function to serialize language completion information for storage in the database
 */
static void
langs_hashtable_to_langentry (gchar *key, gint value, Languages *pb_langs)
{
	Languages_Language *pb_lang = pb_langs->add_language ();
	pb_lang->set_locale (key);
	pb_lang->set_percentage (value);
}

/**
 * Helper function to serialize bundle data for storage in the database
 */
static void
bundles_hashtable_to_bundleentry (gpointer bkind_ptr, gchar *value, Bundles *bundles)
{
	AsBundleKind bkind = (AsBundleKind) GPOINTER_TO_INT (bkind_ptr);

	Bundles_Bundle *bdl = bundles->add_bundle ();
	bdl->set_type ((Bundles_BundleType) bkind);
	bdl->set_id (value);
}

/**
 * Helper function to serialize urls for storage in the database
 */
static void
urls_hashtable_to_urlentry (gpointer ukind_ptr, gchar *value, Urls *urls)
{
	AsUrlKind ukind = (AsUrlKind) GPOINTER_TO_INT (ukind_ptr);

	Urls_Url *url = urls->add_url ();
	url->set_type ((Urls_UrlType) ukind);
	url->set_url (value);
}

/**
 * Helper function to serialize AsImage instances for storage in the database
 */
static void
images_array_to_imageentry (AsImage *img, Screenshots_Screenshot *pb_sshot)
{
	Screenshots_Image *pb_img;

	pb_img = pb_sshot->add_image ();
	pb_img->set_url (as_image_get_url (img));

	if (as_image_get_kind (img) == AS_IMAGE_KIND_THUMBNAIL)
		pb_img->set_source (false);
	else
		pb_img->set_source (true);

	if ((as_image_get_width (img) > 0) && (as_image_get_height (img) > 0)) {
		pb_img->set_width (as_image_get_width (img));
		pb_img->set_height (as_image_get_height (img));
	}
}

/**
 * Turn a GPtrArray (containing C strings) into a semicolon-separated list
 * of strings as sdt__string
 */
static string
ptrarray_to_semicolon_str (GPtrArray *array)
{
	if (array == NULL)
		return string();
	if (array->len == 0)
		return string();

	ostringstream res_sstr;
	for (uint i = 0; i < array->len; i++) {
		const gchar *acstr = (const gchar*) g_ptr_array_index (array, i);
		if (i == array->len-1)
			res_sstr << acstr;
		else
			res_sstr << acstr << ";";
	}

	return res_sstr.str ();
}

/**
 * Recreate the database from a given component list.
 */
bool
DatabaseWrite::rebuild (GList *cpt_list)
{
	string old_path = m_dbPath + "_old";
	string rebuild_path = m_dbPath + "_rb";
	string db_locale;

	// Create the rebuild directory
	if (g_mkdir_with_parents (rebuild_path.c_str (), 0755) != 0) {
		g_warning ("Unable to create database rebuild directory.");
		return false;
	}

	// check if old unrequired version of db still exists on filesystem
	if (g_file_test (old_path.c_str (), G_FILE_TEST_EXISTS)) {
		g_warning ("Existing xapian old db was not cleaned previously: '%s'.", old_path.c_str ());
		as_utils_delete_dir_recursive (old_path.c_str ());
	}

	// check if old unrequired version of db still exists on filesystem
	if (g_file_test (rebuild_path.c_str (), G_FILE_TEST_EXISTS)) {
		g_debug ("Removing old rebuild-dir from previous database rebuild.");
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

		doc.set_data (as_component_get_name (cpt));

		// Sanity check
		if (!as_component_is_valid (cpt)) {
			g_warning ("Skipped component '%s' from inclusion into database: The component is invalid.",
					   as_component_get_id (cpt));
			continue;
		}

		// Package name
		gchar **pkgs = as_component_get_pkgnames (cpt);
		if (pkgs != NULL) {
			gchar *pkgs_cstr = g_strjoinv (";", pkgs);
			string pkgs_str = pkgs_cstr;
			doc.add_value (XapianValues::PKGNAMES, pkgs_str);
			g_free (pkgs_cstr);

			for (uint i = 0; pkgs[i] != NULL; i++) {
				string pkgname = pkgs[i];
				doc.add_term("AP" + pkgname);
				if (pkgname.find ("-") != string::npos) {
					// we need this to work around xapian oddness
					string tmp = pkgname;
					replace (tmp.begin (), tmp.end (), '-', '_');
					doc.add_term (tmp);
				}
				// add packagename as meta-data too
				term_generator.index_text_without_positions (pkgname, WEIGHT_PKGNAME);
			}
		}

		// Source package name
		const gchar *spkgname_cstr = as_component_get_source_pkgname (cpt);
		if (spkgname_cstr != NULL) {
			string spkgname = spkgname_cstr;
			doc.add_value (XapianValues::SOURCE_PKGNAME, spkgname);
			if (!spkgname.empty()) {
				doc.add_term("AP" + spkgname);
				if (spkgname.find ("-") != string::npos) {
					// we need this to work around xapian oddness
					string tmp = spkgname;
					replace (tmp.begin (), tmp.end (), '-', '_');
					doc.add_term (tmp);
				}
				// add packagename as meta-data too
				term_generator.index_text_without_positions (spkgname, WEIGHT_PKGNAME);
			}
		}

		// Type identifier
		string type_str = as_component_kind_to_string (as_component_get_kind (cpt));
		doc.add_value (XapianValues::TYPE, type_str);
		doc.add_term ("AT" + type_str);

		// Identifier
		string idname = as_component_get_id (cpt);
		doc.add_value (XapianValues::IDENTIFIER, idname);
		doc.add_term("AI" + idname);
		term_generator.index_text_without_positions (idname, WEIGHT_PKGNAME);

		// Origin
		const gchar *cptOrigin = as_component_get_origin (cpt);
		if (cptOrigin != NULL)
			doc.add_value (XapianValues::ORIGIN, cptOrigin);

		// Bundles
		Bundles bundles;
		GHashTable *bundle_ids = as_component_get_bundles_table (cpt);
		if (g_hash_table_size (bundle_ids) > 0) {
			string ostr;
			g_hash_table_foreach (bundle_ids,
						(GHFunc) bundles_hashtable_to_bundleentry,
						&bundles);
			if (bundles.SerializeToString (&ostr))
				doc.add_value (XapianValues::BUNDLES, ostr);
		}

		// Component name
		const gchar *cptName = as_component_get_name (cpt);
		if (cptName != NULL)
			doc.add_value (XapianValues::CPTNAME, cptName);

		// Untranslated component name
		string clocale = as_component_get_active_locale (cpt)? as_component_get_active_locale (cpt) : "";
		as_component_set_active_locale (cpt, "C");

		const gchar *cptNameGeneric = as_component_get_name (cpt);
		if (cptNameGeneric != NULL) {
			doc.add_value (XapianValues::CPTNAME_UNTRANSLATED, cptNameGeneric);
			term_generator.index_text_without_positions (cptNameGeneric, WEIGHT_DESKTOP_GENERICNAME);
		}

		if (clocale.empty ())
			as_component_set_active_locale (cpt, NULL);
		else
			as_component_set_active_locale (cpt, clocale.c_str());

		// Extends
		GPtrArray *extends = as_component_get_extends (cpt);
		doc.add_value (XapianValues::EXTENDS, ptrarray_to_semicolon_str (extends));

		// Extensions
		GPtrArray *exts = as_component_get_extensions (cpt);
		doc.add_value (XapianValues::EXTENSIONS, ptrarray_to_semicolon_str (exts));

		// URLs
		GHashTable *urls_table;
		urls_table = as_component_get_urls_table (cpt);
		if (g_hash_table_size (urls_table) > 0) {
			Urls urls;
			string ostr;

			g_hash_table_foreach (urls_table,
						(GHFunc) urls_hashtable_to_urlentry,
						&urls);
			if (urls.SerializeToString (&ostr))
				doc.add_value (XapianValues::URLS, ostr);
		}

		// Icons
		GPtrArray *icons = as_component_get_icons (cpt);
		Icons pbIcons;
		for (uint i = 0; i < icons->len; i++) {
			AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

			Icons_Icon *pbIcon = pbIcons.add_icon ();
			pbIcon->set_width (as_icon_get_width (icon));
			pbIcon->set_height (as_icon_get_height (icon));

			if (as_icon_get_kind (icon) == AS_ICON_KIND_REMOTE) {
				pbIcon->set_type (Icons_IconType_REMOTE);
				pbIcon->set_url (as_icon_get_url (icon));
			} else {
				/* TODO: Properly support STOCK and LOCAL icons */
				pbIcon->set_type (Icons_IconType_CACHED);
				pbIcon->set_url (as_icon_get_filename (icon));
			}
		}
		string icons_ostr;
		if (pbIcons.SerializeToString (&icons_ostr))
			doc.add_value (XapianValues::ICONS, icons_ostr);


		// Summary
		const gchar *cptSummary = as_component_get_summary (cpt);
		if (cptSummary != NULL) {
			doc.add_value (XapianValues::SUMMARY, cptSummary);
			term_generator.index_text_without_positions (cptSummary, WEIGHT_DESKTOP_SUMMARY);
		}

		// Long description
		const gchar *description = as_component_get_description (cpt);
		if (description != NULL) {
			doc.add_value (XapianValues::DESCRIPTION, description);
			term_generator.index_text_without_positions (description, WEIGHT_DESKTOP_SUMMARY);
		}

		// Categories
		gchar **categories = as_component_get_categories (cpt);
		if (categories != NULL) {
			ostringstream categories_sstr;
			for (uint i = 0; categories[i] != NULL; i++) {
				if (as_str_empty (categories[i]))
					continue;

				string cat = categories[i];
				string tmp = cat;
				transform (tmp.begin (), tmp.end (),
						tmp.begin (), ::tolower);
				doc.add_term ("AC" + tmp);
				categories_sstr << cat << ";";
			}
			doc.add_value (XapianValues::CATEGORIES, categories_sstr.str ());
		}

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
		ASCache::ProvidedItems pbPI;
		for (uint j = 0; j < AS_PROVIDED_KIND_LAST; j++) {
			AsProvidedKind kind = (AsProvidedKind) j;
			string kind_str;
			AsProvided *prov = as_component_get_provided_for_kind (cpt, kind);
			if (prov == NULL)
				continue;

			auto *pbProv = pbPI.add_provided ();
			pbProv->set_type ((ProvidedItems_ItemType) kind);

			kind_str = as_provided_kind_to_string (kind);
			gchar **items = as_provided_get_items (prov);
			for (uint j = 0; items[j] != NULL; j++) {
				string item = items[j];
				pbProv->add_item (item);
				doc.add_term ("AE" + kind_str + ";" + item);
			}
			g_free (items);
		}
		string pitems_ostr;
		if (pbPI.SerializeToString (&pitems_ostr))
				doc.add_value (XapianValues::PROVIDED_ITEMS, pitems_ostr);

		// Add screenshot information
		Screenshots screenshots;
		GPtrArray *sslist = as_component_get_screenshots (cpt);
		for (uint i = 0; i < sslist->len; i++) {
			AsScreenshot *sshot = (AsScreenshot*) g_ptr_array_index (sslist, i);
			Screenshots_Screenshot *pb_sshot = screenshots.add_screenshot ();

			pb_sshot->set_primary (false);
			if (as_screenshot_get_kind (sshot) == AS_SCREENSHOT_KIND_DEFAULT)
				pb_sshot->set_primary (true);

			if (as_screenshot_get_caption (sshot) != NULL)
				pb_sshot->set_caption (as_screenshot_get_caption (sshot));

			g_ptr_array_foreach (as_screenshot_get_images (sshot),
						(GFunc) images_array_to_imageentry,
						pb_sshot);
		}
		string scr_ostr;
		if (screenshots.SerializeToString (&scr_ostr))
			doc.add_value (XapianValues::SCREENSHOTS, scr_ostr);

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
		const gchar *project_license = as_component_get_project_license (cpt);
		if (project_license != NULL)
			doc.add_value (XapianValues::LICENSE, project_license);

		// Add project group
		const gchar *project_group = as_component_get_project_group (cpt);
		if (project_group != NULL)
			doc.add_value (XapianValues::PROJECT_GROUP, project_group);

		// Add developer name
		const gchar *developer_name = as_component_get_developer_name (cpt);
		if (developer_name != NULL)
			doc.add_value (XapianValues::DEVELOPER_NAME, developer_name);

		// Add releases information
		Releases pb_rels;
		GPtrArray *releases = as_component_get_releases (cpt);
		for (uint i = 0; i < releases->len; i++) {
			AsRelease *rel = (AsRelease*) g_ptr_array_index (releases, i);
			Releases_Release *pb_rel = pb_rels.add_release ();

			// version
			pb_rel->set_version (as_release_get_version (rel));
			// UNIX timestamp
			pb_rel->set_unix_timestamp (as_release_get_timestamp (rel));
			// release urgency (if set)
			if (as_release_get_urgency (rel) != AS_URGENCY_KIND_UNKNOWN)
				pb_rel->set_urgency ((Releases_UrgencyType) as_release_get_urgency (rel));

			// add location urls
			GPtrArray *locations = as_release_get_locations (rel);
			for (uint j = 0; j < locations->len; j++) {
				pb_rel->add_location ((gchar*) g_ptr_array_index (locations, j));
			}

			// add checksum info
			for (uint j = 0; j < AS_CHECKSUM_KIND_LAST; j++) {
				if (as_release_get_checksum (rel, (AsChecksumKind) j) != NULL) {
					Releases_Checksum *pb_cs = pb_rel->add_checksum ();
					pb_cs->set_type ((Releases_ChecksumType) j);
					pb_cs->set_value (as_release_get_checksum (rel, (AsChecksumKind) j));
				}
			}

			// add size info
			for (uint j = 0; j < AS_SIZE_KIND_LAST; j++) {
				if (as_release_get_size (rel, (AsSizeKind) j) > 0) {
					Releases_Size *pb_s = pb_rel->add_size ();
					pb_s->set_type ((Releases_SizeType) j);
					pb_s->set_value (as_release_get_size (rel, (AsSizeKind) j));
				}
			}

			// add description
			if (as_release_get_description (rel) != NULL)
				pb_rel->set_description (as_release_get_description (rel));
		}
		string rel_ostr;
		if (pb_rels.SerializeToString (&rel_ostr))
			doc.add_value (XapianValues::RELEASES, rel_ostr);

		// Languages
		GHashTable *langs_table;
		langs_table = as_component_get_languages_map (cpt);
		if (g_hash_table_size (langs_table) > 0) {
			Languages pb_langs;
			string ostr;

			g_hash_table_foreach (langs_table,
						(GHFunc) langs_hashtable_to_langentry,
						&pb_langs);

			if (pb_rels.SerializeToString (&ostr))
				doc.add_value (XapianValues::LANGUAGES, ostr);
		}

		// Postprocess
		string docData = doc.get_data ();
		doc.add_term ("AA" + docData);
		term_generator.index_text_without_positions (docData, WEIGHT_DESKTOP_NAME);

		//! g_debug ("Adding component: %s", as_component_to_string (cpt));
		db.add_document (doc);

		// infer database locale from single component
		// TODO: Do that in a smarter way, if we support multiple databases later.
		if (db_locale.empty ())
			db_locale = as_component_get_active_locale (cpt);
	}

	db.set_metadata ("db-schema-version", to_string (AS_DB_SCHEMA_VERSION));
	db.set_metadata ("db-locale", db_locale);
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
