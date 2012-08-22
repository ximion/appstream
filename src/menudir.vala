/* menudir.vala
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

namespace Appstream {

public class MenuDir : Object {
	public string id { get; set; }
	public string name { get; set; }
	public string summary { get; set; }
	public string icon { get; set; }
	public string directory { get; set; }
	public string[] included;
	public string[] excluded;
	public int level;

	public MenuDir () {
		included = {};
		excluded = {};
	}

	internal void complete (KeyFile file) {
		summary = "";
		icon = "applications-other";
		try {
			file.load_from_file ("/usr/share/desktop-directories/%s".printf (directory), 0);
			name = file.get_string ("Desktop Entry", "Name");
			summary = file.get_string ("Desktop Entry", "Comment");
			icon = file.get_string ("Desktop Entry", "Icon");
			if (summary == null) {
				summary = "";
			}
			if (icon == null) {
				icon = "";
			}
		} catch (Error e) {
			debug ("error retrieving data for %s: %s\n", directory, e.message);
		}
	}
}

public class MenuParser {

	private const MarkupParser parser = {
		opening_item, // when an element opens
		closing_item,  // when an element closes
		get_text, // when text is found
		null, // when comments are found
		null  // when errors occur
	};

	private string menu_file;
	private MarkupParseContext context;
	private MenuDir[] dirlist;
	private MenuDir[] dirs_level;
	private int level = 0;
	private string last_item;
	private bool include = true;
	private KeyFile file;

	public MenuParser () {
		this.from_file ("/etc/xdg/menus/applications.menu");
	}

	public MenuParser.from_file (string menu_file) {
		context = new MarkupParseContext(
			parser, // the structure with the callbacks
			0,	// MarkupParseFlags
			this,   // extra argument for the callbacks, methods in this case
			null   // when the parsing ends
		);

		dirlist = {};
		dirs_level = {};
		this.menu_file = menu_file;
		file = new KeyFile();
	}

	public MenuDir[] parse () {
		string file;
		FileUtils.get_contents (menu_file, out file, null);
		context.parse (file, file.length);

		return dirlist;
	}

	private void opening_item (MarkupParseContext context, string name, string[] attr, string[] vals) throws MarkupError {
		last_item = name;
		switch (name) {
			case "Menu":
				MenuDir tmp = new MenuDir();
				dirs_level[level] = tmp;
				dirlist += tmp;
				dirs_level[level].level = level;
				level ++;
				break;
			case "Not":
				include = false;
				break;
		}
	}

	private void closing_item (MarkupParseContext context, string name) throws MarkupError {
		switch (name) {
			case "Menu":
				level --;
				dirs_level[level].complete (file);
				break;
			case "Not":
				include = true;
				break;
		}
	}

	private bool check_whitespaces (string str) {
		return (str.strip() == "" ? true : false);
	}

	private void get_text (MarkupParseContext context, string text, size_t text_len) throws MarkupError {
		if (check_whitespaces (text)) { return; }
		switch (last_item) {
			case "Name":
				dirs_level[level-1].name = text;
				dirs_level[level-1].id = text;
				break;
			case "Directory":
				dirs_level[level-1].directory = text;
				break;
			case "Category":
				MenuDir mdir = dirs_level[level-1];
				if (include) {
					string[] tmp = mdir.included;
					tmp += text;
					mdir.included = tmp;
				} else {
					string[] tmp = mdir.excluded;
					tmp += text;
					mdir.excluded = tmp;
				}
				break;
		}
	}
}

} // End of namespace: Appstream
