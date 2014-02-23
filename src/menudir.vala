/* menudir.vala
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

using GLib;

[CCode (lower_case_cprefix = "appstream_")]
namespace AppStream {

/**
 * Description of an XDG Menu category
 */
public class Category : Object {
	public string name { get; internal set; }
	public string summary { get; private set; }
	public string icon { get; internal set; }
	public string directory { get; internal set; }

	private List<string> _included;
	private List<string> _excluded;

	public List<string> included { get { return _included; } }
	public List<string> excluded { get { return _excluded; } }

	public int level { get; internal set; }

	private List<Category> subcats;
	public List<Category> subcategories { get { return subcats; } }

	public Category () {
		_included = new List<string> ();
		_excluded = new List<string> ();

		subcats = new List<Category> ();
	}

	internal void complete () {
		if (directory == null) {
			debug ("no directory set for category %s", name);
			return;
		}
		var file = new KeyFile ();

		summary = "";
		icon = "applications-other";
		try {
			file.load_from_file ("/usr/share/desktop-directories/%s".printf (directory), 0);
			name = file.get_string ("Desktop Entry", "Name");
			if (file.has_key ("Desktop Entry", "Comment"))
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

	public void add_subcategory (Category cat) {
		subcats.append (cat);
	}

	public void remove_subcategory (Category cat) {
		subcats.remove (cat);
	}

	/**
	 * @return TRUE if this category has any subcategory
	 */
	public bool has_subcategory () {
		return subcats.length () > 0;
	}
}

/**
 * Parser for XDG Menu files
 */
public class MenuParser {
	private string menu_file;
	public bool update_category_data { get; set; }

	/**
	 * Create a new MenuParser for the generic AppStream categories list
	 */
	public MenuParser () {
		this.from_file (Config.DATADIR + "/app-info/categories.xml");
	}

	/**
	 * Create a new MenuParser for an arbitrary menu file
	 */
	public MenuParser.from_file (string menu_file) {
		update_category_data = true;
		this.menu_file = menu_file;
	}

	/**
	 * Parse the menu file
	 *
	 * @return GList of Category objects found in the Menu, or NULL if there was an error
	 */
	public List<Category>? parse () {
		var category_list = new List<Category> ();

		// Parse the document from path
		Xml.Doc* xdoc;
		xdoc = Xml.Parser.parse_file (menu_file);
		if (xdoc == null) {
			warning (_("File %s not found or permission denied!"), menu_file);
			return null;
		}

		// Get the root node
		Xml.Node* root = xdoc->get_root_element ();
		if ((root == null) || (root->name != "Menu")) {
			warning (_("XDG Menu XML file '%s' is damaged."), menu_file);
			return null;
		}

		for (Xml.Node* iter = root->children; iter != null; iter = iter->next) {
			// Spaces between tags are also nodes, discard them
			if (iter->type != Xml.ElementType.ELEMENT_NODE)
				continue;
			if (iter->name == "Menu") {
				// parse menu entry
				category_list.append (parse_menu_enry (iter));
			}
		}

		if (update_category_data) {
			// complete the missing information from desktop-directories folder
			category_list.foreach ((cat) => {
				cat.complete ();
			});
		}

		return category_list;
	}

	private void extend_category_name_list (Xml.Node *nd, List<string> list) {
		for (Xml.Node* iter = nd->children; iter != null; iter = iter->next) {
			if (iter->type != Xml.ElementType.ELEMENT_NODE)
				continue;
			if (iter->name == "Category")
				list.append (iter->get_content ());
		}
	}

	private void parse_category_entry (Xml.Node *nd, Category cat) {
		for (Xml.Node* iter = nd->children; iter != null; iter = iter->next) {
			if (iter->type != Xml.ElementType.ELEMENT_NODE)
				continue;
			if (iter->name == "And") {
				extend_category_name_list (iter, cat.included);
				// check for "Not" elements
				for (Xml.Node* not_iter = iter->children; not_iter != null; not_iter = not_iter->next) {
					if (not_iter->name == "Not")
						extend_category_name_list (not_iter, cat.excluded);
				}
			} else if (iter->name == "Or") {
				extend_category_name_list (iter, cat.included);
			}
		}
	}

	private Category parse_menu_enry (Xml.Node* nd) {
		var cat = new Category ();

		for (Xml.Node* iter = nd->children; iter != null; iter = iter->next) {
			// Spaces between tags are also nodes, discard them
			if (iter->type != Xml.ElementType.ELEMENT_NODE) {
				continue;
			}
			switch (iter->name) {
				case "Name":
					// we don't want a localized name (indicated through a language property)
					if (iter->properties == null)
						cat.name = iter->get_content ();
					break;
				case "Directory": cat.directory = iter->get_content ();
					break;
				case "Icon": cat.icon = iter->get_content ();
					break;
				case "Categories":
					parse_category_entry (iter, cat);
					break;
				case "Menu":
					// we have a submenu!
					cat.add_subcategory (parse_menu_enry (iter));
					break;
				default: break;
			}
		}

		return cat;
	}

}

/**
 * Get a GList of the default AppStream categories
 */
public static List<Category> get_system_categories () {
	var parser = new MenuParser ();
	List<Category> system_cats = parser.parse ();

	return system_cats;
}

} // End of namespace: Appstream
