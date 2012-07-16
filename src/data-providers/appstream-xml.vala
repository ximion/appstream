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

namespace Uai.Provider {

private class Appstream : Uai.DataProvider {

	public Appstream () {
	}

	private string? parse_value (Xml.Node *key, bool translated = false) {
		return "";
	}

	private void parse_application_node (Xml.Node* node) {
		AppInfo app = new AppInfo ();
		for (Xml.Node* iter = node->children; iter != null; iter = iter->next) {
			if (iter->type != ElementType.ELEMENT_NODE) {
				continue;
			}

			string node_name = iter->name;
			string node_content = iter->get_content ();
			switch (node_name) {
				case "id": app.id = node_content;
						break;
				case "name": app.name = node_content;
						break;
			}
		}
	}

	private bool process_single_file (string fname) {
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
		return false;
	}

}

} // End of namespace: Uai
