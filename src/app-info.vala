/* app-info.vala
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
using Appstream.Utils;

[CCode (lower_case_cprefix = "as_", cprefix = "As")]
namespace Appstream {

/**
 * Class to store data describing an application in AppStream
 */
[CCode (lower_case_cprefix = "as_appinfo_")]
public class AppInfo : Component {
	public string desktop_file { get; set; }

	public AppInfo () {
		base ();
		desktop_file = "";
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

}

} // End of namespace: Appstream
