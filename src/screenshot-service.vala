/* screenshot-service.vala
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
using Config;

[CCode (lower_case_cprefix = "as_", cprefix = "As")]
namespace Appstream {

/**
 * Get access to a package screenshot service which matches
 * the current distribution.
 */
public class ScreenshotService : Object {
	public string base_url { get; private set; }

	public ScreenshotService () {
		var distro = new DistroDetails ();
		base_url = distro.config_distro_get_str ("ScreenshotUrl");
		if (base_url == null) {
			warning ("Unable to determine screenshot service for distribution '%s'. Using the Debian services.", distro.distro_name);
			base_url = "http://screenshots.debian.net";
		}
	}

	/**
	 * Get the url of a screenshot thumbnail for the package.
	 *
	 * @param package_name The name of the package which the screenshot belongs to
	 */
	public string get_thumbnail_url (string package_name) {
		return Path.build_filename (base_url, "thumbnail", package_name);
	}

	/**
	 * Get the url of a screenshot for the package.
	 *
	 * @param package_name The name of the package which the screenshot belongs to
	 */
	public string get_screenshot_url (string package_name) {
		return Path.build_filename (base_url, "screenshot", package_name);
	}

}

} // End of namespace: Appstream
