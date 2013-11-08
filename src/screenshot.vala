/* screenshot.vala
 *
 * Copyright (C) 2012-2013 Matthias Klumpp <matthias@tenstral.net>
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
 * Class to store data describing a screenshot
 */
public class Screenshot : Object {
	public string caption { get; set; }
	public HashTable<string, string> urls;
	public HashTable<string, string> thumbnail_urls;

	private bool default_screenshot;

	public Screenshot () {
		caption = "";
		default_screenshot = false;
		urls = new HashTable<string, string> (str_hash, str_equal);
		thumbnail_urls = new HashTable<string, string> (str_hash, str_equal);
	}

	/**
	 * Returns a screenshot url for the given size. Returns NULL if no
	 * url exists for the given size.
	 *
	 * @param size_id a screenshot size, like "800x600", "1400x1600" etc.
	 *
	 * @return url of the screenshot as string
	 */
	public string? get_url_for_size (string size_id) {
		unowned string val;
		val = urls.get (size_id);
		return val;
	}

	/**
	 * Returns a thumbnail url for the given size. Returns NULL if no
	 * url exists for the given size.
	 *
	 * @param size_id a thumbnail size, like "800x600", "1400x1600" etc.
	 *
	 * @return url of the thumbnail image as string
	 */
	public string? get_thumbnail_url_for_size (string size_id) {
		unowned string val;
		val = thumbnail_urls.get (size_id);
		return val;
	}

	public void add_url (string size_id, string url) {
		urls.insert (size_id, url);
	}

	public void add_thumbnail_url (string size_id, string url) {
		thumbnail_urls.insert (size_id, url);
	}

	/**
	 * Returns TRUE if the screenshot is the default screenshot for this application
	 */
	public bool is_default () {
		return default_screenshot;
	}

	public void set_default (bool b) {
		default_screenshot = b;
	}

}

} // End of namespace: Appstream
