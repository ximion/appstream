/* appstream-xml.vala
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

using GLib;
using Xml;
using Uai;
using Appstream;

namespace Uai.Provider {

private class AppstreamXML : Uai.DataProvider {
	private string locale;
	private Category[] system_categories;

	public AppstreamXML () {
		locale = Intl.get_language_names ()[0];
		// cache this for performance reasons
		system_categories = Appstream.get_system_categories ();
	}

	private string? parse_value (Xml.Node *node, bool translated = false) {
		string content = node->get_content ();
		string? lang = node->get_prop ("lang");
		if (translated) {
			// FIXME: If not-localized generic node comes _after_ the localized ones,
			//        the not-localized will override the localized. Wrong ordering should
			//        not happen. (but this code can be improved anyway :P)
			if (lang == null)
				return content;
			if (lang == locale)
				return content;
			if (lang == locale.split("_")[0])
				return node->get_content ();

			// Haven't found a matching locale
			return null;
		}
		// If we have locale here, but want the untranslated item
		if (lang != null)
			return null;

		return content;
	}

	private string[] get_childs_as_array (Xml.Node* node, string element_name) {
		string[] list = {};
		for (Xml.Node* iter = node->children; iter != null; iter = iter->next) {
			// Discard spaces
			if (iter->type != ElementType.ELEMENT_NODE) {
				continue;
			}

			if (iter->name == element_name) {
				string? content = iter->get_content ();
				if (content != null)
					list += content.strip ();
			}
		}

		return list;
	}

	private void parse_application_node (Xml.Node* node) {
		var app = new Appstream.AppInfo ();
		for (Xml.Node* iter = node->children; iter != null; iter = iter->next) {
			if (iter->type != ElementType.ELEMENT_NODE) {
				continue;
			}

			string node_name = iter->name;
			string? content = parse_value (iter);
			switch (node_name) {
				case "id":	if (content != null) {
							// in this case, ID == desktop-file
							app.desktop_file = content;
						}
						break;
				case "pkgname": if (content != null)
							app.pkgname = content;
						break;
				case "name": 	if (content != null) {
							app.name_original = content;
						} else {
							content = parse_value (iter, true);
							if (content != null)
								app.name = content;
						}
						break;
				case "summary": if (content != null) {
							app.summary = content;
						} else {
							content = parse_value (iter, true);
							if (content != null)
								app.summary = content;
						}
						break;
				case "icon":	if (node->get_prop ("type") == "stock")
							if (content != null)
								app.icon = content;
						break;
				case "url":	if (content != null)
							app.url = content;
						break;
				case "appcategories":
						string[] cat_array = get_childs_as_array (iter, "appcategory");
						app.categories = Appstream.Utils.categories_from_strv (cat_array, system_categories);
						break;
			}
		}

		if (app.is_valid ())
			emit_application (app);
		else
			log_warning ("Invalid application found: %s". printf (app.to_string ()));
	}

	public bool process_single_file (string fname) {
		bool ret = true;

		// Parse the document from path
		Xml.Doc* doc = Parser.parse_file (fname);
		if (doc == null) {
			stderr.printf ("File %s not found or permissions missing", fname);
			return false;
		}

		Xml.Node* root = doc->get_root_element ();
		if (root == null) {
			delete doc;
			stderr.printf ("The XML file '%s' is empty", fname);
			return false;
		}

		if (root->name != "applications") {
			delete doc;
			stderr.printf ("The XML file '%s' does not contain valid AppStream data!", fname);
			return false;
		}

		for (Xml.Node* iter = root->children; iter != null; iter = iter->next) {
			// Discard spaces
			if (iter->type != ElementType.ELEMENT_NODE) {
				continue;
			}

			if (iter->name == "application")
				parse_application_node (iter);
		}

		delete doc;

		return ret;
	}

	public override bool execute () {
		Array<string>? xmlFiles = Utils.find_files_matching (APPSTREAM_XML_PATH, "*.xml");
		if ((xmlFiles == null) || (xmlFiles.length == 0))
			return false;

		for (uint i=0; i < xmlFiles.length; i++) {
			process_single_file (xmlFiles.index (i));
		}

		return true;
	}

}

} // End of namespace: Uai.Provider
