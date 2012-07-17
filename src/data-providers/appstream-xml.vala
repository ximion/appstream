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

namespace Uai.Provider {

private class Appstream : Uai.DataProvider {
	private string locale;

	public Appstream () {
		locale = Intl.get_language_names ()[0];
	}

	private string? parse_value (Xml.Node *node, bool translated = false) {
		string content = node->get_content ();
		if (translated) {
			// FIXME: If not-localized generic node comes _after_ the localized ones,
			//        the not-localized will override the localized. Wrong ordering should
			//        not happen. (but this code can be improved anyway :P)
			if (node->get_prop ("lang") == null)
				return content;
			if (node->get_prop ("lang") == locale)
				return content;
			if (node->get_prop ("lang") == locale.split("_")[0])
				return node->get_content ();

			// Haven't found a matching locale
			return null;
		}
		return content;
	}

	private void parse_application_node (Xml.Node* node) {
		AppInfo app = new AppInfo ();
		for (Xml.Node* iter = node->children; iter != null; iter = iter->next) {
			if (iter->type != ElementType.ELEMENT_NODE) {
				continue;
			}

			string node_name = iter->name;
			string? content = parse_value (iter);
			switch (node_name) {
				case "id":	if (content != null) {
							// Issue in AppStream documentation: AppID needs to
							// have a clear definition!
							// FIXME
							app.id = content;
							app.desktop_file = content;
						}
						break;
				case "pkgname": if (content != null) app.pkgname = content;
						break;
				case "name": 	if (content != null) {
							app.name_original = content;
						} else {
							content = parse_value (iter, true);
							if (content != null) app.name = content;
						}
						break;
				case "summary": content = parse_value (iter, true);
						if (content != null) app.summary = content;
						break;
				case "icon":	if (content != null) app.icon = content;
						break;
				case "url":	if (content != null) app.url = content;
						break;
			}
		}

		if (app.is_valid ())
			emit_application (app);
		else
			warning ("Invalid application found: %s", app.to_string ());
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
		Array<string>? xml_files = Utils.find_files_matching (APPSTREAM_XML_PATH, "*.xml");
		if ((xml_files == null) || (xml_files.length == 0))
			return false;

		for (uint i=0; i < xml_files.length; i++) {
			process_single_file (xml_files.index (i));
		}

		return true;
	}

}

} // End of namespace: Uai.Provider
