/* app-info.vala
 *
 * Copyright (C) 2012 Matthias Klumpp <matthias@tenstral.net>
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
using Appstream.Utils;

namespace Appstream {

/**
 * Class to store data describing an application in AppStream
 */
public class AppInfo : Object {
	public string pkgname { get; set; }
	public string desktop_file { get; set; }

	private string _name;
	public string name {
			get {
				if (str_empty (_name))
					return name_original;
				else
					return _name;
			}
			set {
				_name = value;
			}
		} // Localized!
	public string name_original { get; set; } // Not localized!

	public string summary { get; set; } // Localized!
	public string description { get; set; } // Localized!
	public string[] keywords { get; set; } // Localized!

	public string icon { get; set; } // stock icon
	public string icon_url { get; set; } // cached, local or remote icon

	public string homepage { get; set; } // app homepage

	public string[] categories { get; set; }
	public string[] mimetypes { get; set; }


	public AppInfo () {
		pkgname = "";
		desktop_file = "";
		_name = "";
		name_original = "";
		summary = "";
		description = "";
		homepage = "";
		icon = "";
		icon_url = "";
		categories = {null};
	}

	/**
	 * Check if the essential properties of this AppInfo instance are
	 * populated with useful data.
	 */
	public bool is_valid () {
		if ((pkgname != "") && (desktop_file != "") && (name != "") && (name_original != ""))
			return true;
		return false;
	}

	public string to_string () {
		string res;
		res = "[AppInfo::%s]> name: %s | desktop: %s | summary: %s".printf (pkgname, name, desktop_file, summary);
		return res;
	}

	/**
	 * Set the categories list from a string
	 *
	 * @param categories_str Comma-separated list of category-names
	 */
	public void set_categories_from_str (string categories_str) {
		string[] cats = categories_str.split (",");
		categories = cats;
	}

}

} // End of namespace: Appstream
