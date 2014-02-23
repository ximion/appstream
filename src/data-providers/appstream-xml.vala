/* appstream-xml.vala
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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
using Appstream;

namespace Appstream.Provider {

private class AppstreamXML : Appstream.DataProvider {
	private string locale;
	private List<Category> system_categories;

	public AppstreamXML () {
		locale = Intl.get_language_names ()[0];
		// cache this for performance reasons
		system_categories = Appstream.get_system_categories ();

		// we do this to help Vala generate proper C code - watch_files = APPSTREAM_XML_PATHS works, but
		// results in bad C code.
		string[] wfiles = {};
		foreach (string path in APPSTREAM_XML_PATHS)
			wfiles += path;
		watch_files = wfiles;
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

	private string[] get_children_as_array (Xml.Node* node, string element_name) {
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

	private void process_screenshot (Xml.Node* node, Screenshot sshot) {
		string[] list = {};
		for (Xml.Node* iter = node->children; iter != null; iter = iter->next) {
			// Discard spaces
			if (iter->type != ElementType.ELEMENT_NODE) {
				continue;
			}

			string node_name = iter->name;
			// fetch translated values by default
			string? content = parse_value (iter, true);
			switch (node_name) {
				case "image":	if (content != null) {
							string? width = iter->get_prop ("width");
							string? height = iter->get_prop ("height");
							// discard invalid elements
							if ((width == null) || (height == null))
								break;
							if (iter->get_prop ("type") == "thumbnail")
								sshot.add_thumbnail_url ("%sx%s".printf (width, height), content);
							else
								sshot.add_url ("%sx%s".printf (width, height), content);
						}
						break;
				case "caption":	if (content != null) {
							sshot.caption = content;
						}
						break;
			}
		}
	}

	private void process_screenshots_tag (Xml.Node* node, AppInfo app) {
		for (Xml.Node* iter = node->children; iter != null; iter = iter->next) {
			// Discard spaces
			if (iter->type != ElementType.ELEMENT_NODE) {
				continue;
			}

			if (iter->name == "screenshot") {
				var sshot = new Screenshot ();
				if (iter->get_prop ("type") == "default")
					sshot.set_default (true);
				process_screenshot (iter, sshot);
				if (sshot.is_valid ())
					app.add_screenshot (sshot);
			}
		}
	}

	private void parse_application_node (Xml.Node* node) {
		var app = new Appstream.AppInfo ();
		for (Xml.Node* iter = node->children; iter != null; iter = iter->next) {
			if (iter->type != ElementType.ELEMENT_NODE) {
				continue;
			}

			string node_name = iter->name;
			string? str;
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
				case "description": if (content != null) {
							app.description = content;
						} else {
							content = parse_value (iter, true);
							if (content != null)
								app.description = content;
						}
						break;
				case "icon":	if (content == null)
							break;
						str = iter->get_prop ("type");
						switch (str) {
							case "stock":
								app.icon = content;
								break;
							case "cached":
								if ((app.icon_url == "") || (app.icon_url.has_prefix ("http://")))
									app.icon_url = content;
								break;
							case "local":
								app.icon_url = content;
								break;
							case "remote":
								if (app.icon_url == "")
									app.icon_url = content;
								break;
						}
						break;
				case "url":	if (content != null)
							app.homepage = content;
						break;
				case "categories":
						string[] cat_array = get_children_as_array (iter, "category");
						app.categories = cat_array;
						break;
				/** @deprecated the appcategory tag is deprecated, handled here for backward compatibility */
				case "appcategories":
						string[] cat_array = get_children_as_array (iter, "appcategory");
						app.categories = cat_array;
						break;
				case "screenshots":
						process_screenshots_tag (iter, app);
						break;
			}
		}

		if (app.is_valid ())
			emit_application (app);
		else
			log_warning ("Invalid application found: %s". printf (app.to_string ()));
	}

	private bool process_single_document (string xmldoc) {
		bool ret = true;

		// Parse the document from path
		Xml.Doc* doc = Parser.parse_doc (xmldoc);
		if (doc == null) {
			stderr.printf ("Could not parse XML!");
			return false;
		}

		Xml.Node* root = doc->get_root_element ();
		if (root == null) {
			delete doc;
			stderr.printf ("The XML document is empty");
			return false;
		}

		if (root->name != "applications") {
			delete doc;
			stderr.printf ("XML file does not contain valid AppStream data!");
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

	public bool process_compressed_file (File infile) {
		var src_stream = infile.read ();
		var mem_os = new MemoryOutputStream (null, GLib.realloc, GLib.free);
		var conv_stream = new ConverterOutputStream (mem_os, new ZlibDecompressor (ZlibCompressorFormat.GZIP));
		// pump all data from InputStream to OutputStream
		conv_stream.splice (src_stream, 0);

		return process_single_document ((string) mem_os.get_data ());
	}

	public bool process_file (File infile) {
		string xml_doc = "";
		string line;
		var dis = new DataInputStream (infile.read ());
		// Read lines until end of file (null) is reached
		while ((line = dis.read_line (null)) != null) {
			xml_doc += line + "\n";
		}

		return process_single_document (xml_doc);
	}

	public override bool execute () {
		Array<string> xmlFiles = new Array<string> ();

		foreach (string path in APPSTREAM_XML_PATHS) {
			// Check if the folder is actually there before trying to scan it...
			if (!FileUtils.test (path, FileTest.EXISTS))
				continue;
			Array<string>? xmls = Utils.find_files_matching (path, "*.xml*");
			if (xmls != null) {
				for (uint j=0; j < xmls.length; j++)
					xmlFiles.append_val (xmls.index (j));
			}
		}

		if (xmlFiles.length == 0)
			return false;

		for (uint i=0; i < xmlFiles.length; i++) {
			string fname = xmlFiles.index (i);
			var infile = File.new_for_path (fname);
			if (!infile.query_exists ()) {
				stderr.printf ("File '%s' does not exists.", fname);
				continue;
			}

			if (fname.has_suffix (".xml")) {
				process_file (infile);
			} else if (fname.has_suffix (".xml.gz")) {
				process_compressed_file (infile);
			}
		}

		return true;
	}

}

} // End of namespace: Appstream.Provider
