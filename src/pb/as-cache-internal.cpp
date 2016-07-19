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

#include "as-cache-internal.h"

#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ascache.pb.h>
#include <glib/gstdio.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "../as-utils.h"
#include "../as-utils-private.h"
#include "../as-component-private.h"

using namespace std;

/**
 * Helper function to serialize language completion information for storage in the cache.
 */
static void
langs_hashtable_to_langentry_cb (gchar *key, gint value, ASCache::Component *pb_cpt)
{
	auto pb_lang = pb_cpt->add_language ();
	pb_lang->set_locale (key);
	pb_lang->set_percentage (value);
}

/**
 * Helper function to serialize bundle data for storage in the cache.
 */
static void
bundles_hashtable_to_bundleentry_cb (gpointer bkind_ptr, gchar *value, ASCache::Component *pb_cpt)
{
	AsBundleKind bkind = (AsBundleKind) GPOINTER_TO_INT (bkind_ptr);

	auto pb_bundle = pb_cpt->add_bundle ();
	pb_bundle->set_type ((ASCache::Bundle_Type) bkind);
	pb_bundle->set_id (value);
}

/**
 * Helper function to serialize urls for storage in the cache.
 */
static void
urls_hashtable_to_urlentry_cb (gpointer ukind_ptr, gchar *value, ASCache::Component *pb_cpt)
{
	AsUrlKind ukind = (AsUrlKind) GPOINTER_TO_INT (ukind_ptr);

	auto pb_url = pb_cpt->add_url ();
	pb_url->set_type ((ASCache::Url_Type) ukind);
	pb_url->set_url (value);
}

/**
 * Helper function to serialize AsImage instances for storage in the cache.
 */
static void
images_array_to_imageentry_cb (AsImage *img, ASCache::Screenshot *pb_sshot)
{
	const gchar *locale;

	auto pb_img = pb_sshot->add_image ();
	pb_img->set_url (as_image_get_url (img));

	if (as_image_get_kind (img) == AS_IMAGE_KIND_THUMBNAIL)
		pb_img->set_source (false);
	else
		pb_img->set_source (true);

	pb_img->set_width (as_image_get_width (img));
	pb_img->set_height (as_image_get_height (img));

	locale = as_image_get_locale (img);
	if (locale != NULL)
		pb_img->set_locale (locale);
}

/**
 * Rebuild an AppStream cache file.
 */
void
as_cache_write (const gchar *fname, const gchar *locale, GPtrArray *cpts, GError **error)
{
	// check if old unrequired version of db still exists on filesystem
	if (g_file_test (fname, G_FILE_TEST_EXISTS)) {
		g_debug ("Removing existing cache file: %s", fname);
		g_remove (fname);
	}

	ASCache::Cache cache;

	cache.set_cache_version (1);
	if (locale == NULL)
		locale = "C";
	cache.set_locale (locale);

	for (uint cindex = 0; cindex < cpts->len; cindex++) {
		auto cpt = AS_COMPONENT (g_ptr_array_index (cpts, cindex));

		// Sanity check
		if (!as_component_is_valid (cpt)) {
			// we should *never* get here, all invalid stuff should be filtered out at this point
			g_critical ("Skipped component '%s' from inclusion into the cache: The component is invalid.",
					   as_component_get_id (cpt));
			continue;
		}

		// new Protobuf cache component
		auto pb_cpt = cache.add_component ();

		// ID
		pb_cpt->set_id (as_component_get_id (cpt));

		// Type
		pb_cpt->set_type (as_component_get_kind (cpt));

		// Name
		pb_cpt->set_name (as_component_get_name (cpt));

		// Summary
		const gchar *tmp;
		tmp = as_component_get_summary (cpt);
		if (tmp != NULL)
			pb_cpt->set_summary (tmp);

		// Source package name
		tmp = as_component_get_source_pkgname (cpt);
		if (tmp != NULL)
			pb_cpt->set_source_pkgname (tmp);

		// Package name
		auto pkgs = as_component_get_pkgnames (cpt);
		if (pkgs != NULL) {
			for (uint i = 0; pkgs[i] != NULL; i++)
				pb_cpt->add_pkgname (pkgs[i]);
		}

		// Origin
		tmp = as_component_get_origin (cpt);
		if (tmp != NULL)
			pb_cpt->set_origin (tmp);

		// Bundles
		GHashTable *bundle_ids = as_component_get_bundles_table (cpt);
		if (g_hash_table_size (bundle_ids) > 0) {
			g_hash_table_foreach (bundle_ids,
						(GHFunc) bundles_hashtable_to_bundleentry_cb,
						pb_cpt);
		}

		// Extends
		auto extends = as_component_get_extends (cpt);
		for (uint i = 0; i < extends->len; i++) {
			auto strval = (const gchar*) g_ptr_array_index (extends, i);
			pb_cpt->add_extends (strval);
		}

		// Extensions
		auto addons = as_component_get_extensions (cpt);
		for (uint i = 0; i < addons->len; i++) {
			auto strval = (const gchar*) g_ptr_array_index (addons, i);
			pb_cpt->add_addon (strval);
		}

		// URLs
		auto urls_table = as_component_get_urls_table (cpt);
		if (g_hash_table_size (urls_table) > 0) {
			g_hash_table_foreach (urls_table,
						(GHFunc) urls_hashtable_to_urlentry_cb,
						pb_cpt);
		}

		// Icons
		GPtrArray *icons = as_component_get_icons (cpt);
		for (uint i = 0; i < icons->len; i++) {
			AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

			auto pb_icon = pb_cpt->add_icon ();
			pb_icon->set_width (as_icon_get_width (icon));
			pb_icon->set_height (as_icon_get_height (icon));

			if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
				pb_icon->set_type (ASCache::Icon_Type_STOCK);
				pb_icon->set_value (as_icon_get_name (icon));
			} else if (as_icon_get_kind (icon) == AS_ICON_KIND_REMOTE) {
				pb_icon->set_type (ASCache::Icon_Type_REMOTE);
				pb_icon->set_value (as_icon_get_url (icon));
			} else if (as_icon_get_kind (icon) == AS_ICON_KIND_CACHED) {
				pb_icon->set_type (ASCache::Icon_Type_CACHED);
				pb_icon->set_value (as_icon_get_filename (icon));
			} else {
				pb_icon->set_type (ASCache::Icon_Type_LOCAL);
				pb_icon->set_value (as_icon_get_filename (icon));
			}
		}

		// Long description
		tmp = as_component_get_description (cpt);
		if (tmp != NULL)
			pb_cpt->set_description (tmp);

		// Categories
		auto categories = as_component_get_categories (cpt);
		if (categories != NULL) {
			for (uint i = 0; categories[i] != NULL; i++) {
				if (as_str_empty (categories[i]))
					continue;

				pb_cpt->add_category (categories[i]);
			}
		}

		// Provided Items
		for (uint j = 0; j < AS_PROVIDED_KIND_LAST; j++) {
			AsProvidedKind kind = (AsProvidedKind) j;
			string kind_str;
			AsProvided *prov = as_component_get_provided_for_kind (cpt, kind);
			if (prov == NULL)
				continue;

			auto pb_prov = pb_cpt->add_provided ();
			pb_prov->set_type ((ASCache::Provided_Type) kind);

			kind_str = as_provided_kind_to_string (kind);
			gchar **items = as_provided_get_items (prov);
			for (uint j = 0; items[j] != NULL; j++) {
				string item = items[j];
				pb_prov->add_item (item);
			}
			g_free (items);
		}

		// Screenshots
		GPtrArray *scrs = as_component_get_screenshots (cpt);
		for (uint i = 0; i < scrs->len; i++) {
			AsScreenshot *sshot = AS_SCREENSHOT (g_ptr_array_index (scrs, i));
			auto pb_sshot = pb_cpt->add_screenshot ();

			pb_sshot->set_primary (false);
			if (as_screenshot_get_kind (sshot) == AS_SCREENSHOT_KIND_DEFAULT)
				pb_sshot->set_primary (true);

			if (as_screenshot_get_caption (sshot) != NULL)
				pb_sshot->set_caption (as_screenshot_get_caption (sshot));

			g_ptr_array_foreach (as_screenshot_get_images (sshot),
						(GFunc) images_array_to_imageentry_cb,
						pb_sshot);
		}

		// Compulsory-for-desktop
		gchar **strv;
		strv = as_component_get_compulsory_for_desktops (cpt);
		if (strv != NULL) {
			for (uint i = 0; strv[i] != NULL; i++) {
				pb_cpt->add_compulsory_for (strv[i]);
			}
		}

		// Add project-license
		tmp = as_component_get_project_license (cpt);
		if (tmp != NULL)
			pb_cpt->set_license (tmp);

		// Add project group
		tmp = as_component_get_project_group (cpt);
		if (tmp != NULL)
			pb_cpt->set_project_group (tmp);

		// Add developer name
		tmp = as_component_get_developer_name (cpt);
		if (tmp != NULL)
			pb_cpt->set_developer_name (tmp);

		// Add releases information
		GPtrArray *releases = as_component_get_releases (cpt);
		for (uint i = 0; i < releases->len; i++) {
			AsRelease *rel = AS_RELEASE (g_ptr_array_index (releases, i));
			auto pb_rel = pb_cpt->add_release ();

			// version
			pb_rel->set_version (as_release_get_version (rel));
			// UNIX timestamp
			pb_rel->set_unix_timestamp (as_release_get_timestamp (rel));
			// release urgency (if set)
			if (as_release_get_urgency (rel) != AS_URGENCY_KIND_UNKNOWN)
				pb_rel->set_urgency ((ASCache::Release_UrgencyType) as_release_get_urgency (rel));

			// add location urls
			GPtrArray *locations = as_release_get_locations (rel);
			for (uint j = 0; j < locations->len; j++) {
				pb_rel->add_location ((gchar*) g_ptr_array_index (locations, j));
			}

			// add checksum info
			for (uint j = 0; j < AS_CHECKSUM_KIND_LAST; j++) {
				if (as_release_get_checksum (rel, (AsChecksumKind) j) != NULL) {
					auto pb_cs = pb_rel->add_checksum ();
					pb_cs->set_type ((ASCache::Release_ChecksumType) j);
					pb_cs->set_value (as_release_get_checksum (rel, (AsChecksumKind) j));
				}
			}

			// add size info
			for (uint j = 0; j < AS_SIZE_KIND_LAST; j++) {
				if (as_release_get_size (rel, (AsSizeKind) j) > 0) {
					auto pb_s = pb_rel->add_size ();
					pb_s->set_type ((ASCache::Release_SizeType) j);
					pb_s->set_value (as_release_get_size (rel, (AsSizeKind) j));
				}
			}

			// add description
			if (as_release_get_description (rel) != NULL)
				pb_rel->set_description (as_release_get_description (rel));
		}

		// Languages
		auto langs_table = as_component_get_languages_map (cpt);
		if (g_hash_table_size (langs_table) > 0) {
			g_hash_table_foreach (langs_table,
						(GHFunc) langs_hashtable_to_langentry_cb,
						pb_cpt);
		}
	}

	// Save the cache object to disk
	int fd = open (fname, O_WRONLY | O_CREAT, 0755);
	// TODO: Handle error

	google::protobuf::io::FileOutputStream ostream (fd);
	if (cache.SerializeToZeroCopyStream (&ostream)) {
		ostream.Close ();
		return;
	} else {
		// TODO: Set error
		return;
	}
}

static AsComponent*
buffer_to_component (const ASCache::Component& pb_cpt, const gchar *locale)
{
	AsComponent *cpt = as_component_new ();
	string str;

	/* set component active languge (which is the locale the cache was built for) */
	as_component_set_active_locale (cpt, locale);

	// Type
	as_component_set_kind (cpt, (AsComponentKind) pb_cpt.type ());

	// Identifier
	as_component_set_id (cpt, pb_cpt.id ().c_str ());

	// Name
	as_component_set_name (cpt, pb_cpt.name ().c_str (), NULL);

	// Summary
	if (pb_cpt.has_summary ()) {
		as_component_set_summary (cpt, pb_cpt.summary ().c_str (), NULL);
	}

	// Source package name
	if (pb_cpt.has_source_pkgname ()) {
		as_component_set_source_pkgname (cpt, pb_cpt.source_pkgname ().c_str ());
	}

	// Package name
	auto size = pb_cpt.pkgname_size ();
	if (size > 0) {
		g_auto(GStrv) strv = g_new0 (gchar*, size + 1);
		for (int i = 0; i < size; i++) {
			strv[i] = g_strdup (pb_cpt.pkgname (i).c_str ());
		}
		as_component_set_pkgnames (cpt, strv);
	}

	// Source package name
	if (pb_cpt.has_source_pkgname ()) {
		as_component_set_source_pkgname (cpt, pb_cpt.source_pkgname ().c_str ());
	}

	// Origin
	if (pb_cpt.has_origin ()) {
		as_component_set_origin (cpt, pb_cpt.origin ().c_str ());
	}

	// Bundles
	for (int i = 0; i < pb_cpt.bundle_size (); i++) {
		auto bdl = pb_cpt.bundle (i);
		AsBundleKind bkind = (AsBundleKind) bdl.type ();
		if (bkind != AS_BUNDLE_KIND_UNKNOWN)
			as_component_add_bundle_id (cpt, bkind, bdl.id ().c_str ());
	}

	// Extends
	size = pb_cpt.extends_size ();
	if (size > 0) {
		for (int i = 0; i < size; i++)
			as_component_add_extends (cpt, pb_cpt.extends (i).c_str ());
	}

	// Extensions
	size = pb_cpt.addon_size ();
	if (size > 0) {
		for (int i = 0; i < size; i++)
			as_component_add_extension (cpt, pb_cpt.addon (i).c_str ());
	}

	// URLs
	for (int i = 0; i < pb_cpt.url_size (); i++) {
		auto url = pb_cpt.url (i);
		AsUrlKind ukind = (AsUrlKind) url.type ();
		if (ukind != AS_URL_KIND_UNKNOWN)
			as_component_add_url (cpt, ukind, url.url ().c_str ());
	}

	// Icons
	for (int i = 0; i < pb_cpt.icon_size (); i++) {
		auto pb_icon = pb_cpt.icon (i);

		g_autoptr(AsIcon) icon = as_icon_new ();
		as_icon_set_width (icon, pb_icon.width ());
		as_icon_set_height (icon, pb_icon.height ());

		if (pb_icon.type () == ASCache::Icon_Type_STOCK) {
			as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
			as_icon_set_url (icon, pb_icon.value ().c_str ());
		} else if (pb_icon.type () == ASCache::Icon_Type_REMOTE) {
			as_icon_set_kind (icon, AS_ICON_KIND_REMOTE);
			as_icon_set_url (icon, pb_icon.value ().c_str ());
		} else if (pb_icon.type () == ASCache::Icon_Type_CACHED) {
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
			as_icon_set_filename (icon, pb_icon.value ().c_str ());
		} else {
			as_icon_set_kind (icon, AS_ICON_KIND_LOCAL);
			as_icon_set_filename (icon, pb_icon.value ().c_str ());
		}
		as_component_add_icon (cpt, icon);
	}

	// Long description
	if (pb_cpt.has_description ()) {
		as_component_set_description (cpt, pb_cpt.description ().c_str (), NULL);
	}

	// Categories
	size = pb_cpt.category_size ();
	if (size > 0) {
		g_auto(GStrv) strv = g_new0 (gchar*, size + 1);
		for (int i = 0; i < size; i++) {
			strv[i] = g_strdup (pb_cpt.category (i).c_str ());
		}
		as_component_set_categories (cpt, strv);
	}

	// Provided items
	for (int i = 0; i < pb_cpt.provided_size (); i++) {
		auto pb_prov = pb_cpt.provided (i);
		g_autoptr(AsProvided) prov = as_provided_new ();

		as_provided_set_kind (prov, (AsProvidedKind) pb_prov.type ());
		for (int j = 0; j < pb_prov.item_size (); j++) {
			auto item = pb_prov.item (j);
			as_provided_add_item (prov, item.c_str ());
		}
		as_component_add_provided (cpt, prov);
	}

	// Screenshots
	for (int i = 0; i < pb_cpt.screenshot_size (); i++) {
		auto pb_scr = pb_cpt.screenshot (i);
		g_autoptr(AsScreenshot) scr = as_screenshot_new ();
		as_screenshot_set_active_locale (scr, locale);

		if (pb_scr.primary ())
			as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_DEFAULT);
		else
			as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_EXTRA);

		if (pb_scr.has_caption ())
			as_screenshot_set_caption (scr, pb_scr.caption ().c_str (), NULL);

		for (int j = 0; j < pb_scr.image_size (); j++) {
			auto pb_img = pb_scr.image (j);
			g_autoptr(AsImage) img = as_image_new ();

			if (pb_img.source ()) {
				as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
			} else {
				as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
			}

			as_image_set_width (img, pb_img.width ());
			as_image_set_height (img, pb_img.height ());
			as_image_set_url (img, pb_img.url ().c_str ());
			if (pb_img.has_locale ())
				as_image_set_locale (img, pb_img.locale ().c_str ());

			as_screenshot_add_image (scr, img);
		}

		as_component_add_screenshot (cpt, scr);
	}

	// Compulsory-for-desktop information
	size = pb_cpt.compulsory_for_size ();
	if (size > 0) {
		g_auto(GStrv) strv = g_new0 (gchar*, size + 1);
		for (int i = 0; i < size; i++) {
			strv[i] = g_strdup (pb_cpt.compulsory_for (i).c_str ());
		}
		as_component_set_compulsory_for_desktops (cpt, strv);
	}

	// License
	if (pb_cpt.has_license ()) {
		as_component_set_project_license (cpt, pb_cpt.license ().c_str ());
	}

	// Project group
	if (pb_cpt.has_project_group ()) {
		as_component_set_project_group (cpt, pb_cpt.project_group ().c_str ());
	}

	// Releases data
	for (int i = 0; i < pb_cpt.release_size (); i++) {
		auto pb_rel = pb_cpt.release (i);
		g_autoptr(AsRelease) rel = as_release_new ();
		as_release_set_active_locale (rel, locale);

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
			auto pb_cs = pb_rel.checksum (j);
			AsChecksumKind cskind = (AsChecksumKind) pb_cs.type ();

			if (cskind >= AS_CHECKSUM_KIND_LAST) {
				g_warning ("Found invalid release-checksum type in database for component '%s'", as_component_get_id (cpt));
				continue;
			}
			as_release_set_checksum (rel, pb_cs.value ().c_str (), cskind);
		}

		// load sizes
		for (int j = 0; j < pb_rel.size_size (); j++) {
			auto pb_s = pb_rel.size (j);
			AsSizeKind skind = (AsSizeKind) pb_s.type ();

			if (skind >= AS_SIZE_KIND_LAST) {
				g_warning ("Found invalid release-size type in database for component '%s'", as_component_get_id (cpt));
				continue;
			}
			as_release_set_size (rel, pb_s.value (), skind);
		}

		as_component_add_release (cpt, rel);
	}

	// Languages
	for (int i = 0; i < pb_cpt.language_size (); i++) {
		auto pb_lang = pb_cpt.language (i);

		as_component_add_language (cpt,
					   pb_lang.locale ().c_str (),
					   pb_lang.percentage ());
	}

	// TODO:
	//   * Read out keywords
	//   * Read out search terms

	return cpt;
}

/**
 * Read the whole cache into memory and create #AsComponent instances for all found
 * components.
 */
GPtrArray*
as_cache_read (const gchar *fname, GError **error)
{
	int fd = open (fname, O_RDONLY);
	// TODO: Handle error

	google::protobuf::io::FileInputStream istream (fd);

	ASCache::Cache cache;
	auto ret = cache.ParseFromZeroCopyStream (&istream);
	if (!ret) {
		// TODO: Emit error
		return NULL;
	}

	auto locale = cache.locale ().c_str ();
	auto entries = g_ptr_array_new_with_free_func (g_object_unref);
	for (int i = 0; i < cache.component_size (); i++) {
		auto pb_cpt = cache.component (i);

		auto cpt = buffer_to_component (pb_cpt, locale);
		if (cpt != NULL)
			g_ptr_array_add (entries, cpt);
	}

	return entries;
}
