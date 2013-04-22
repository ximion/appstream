/* ubuntu-appinstall.vala
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
using Uai;
using Appstream;

namespace Uai.Provider {

private static const string UBUNTU_APPINSTALL_DIR = "/usr/share/app-install";

private class UbuntuAppinstall : Uai.DataProvider {
	private Category[] system_categories;

	public UbuntuAppinstall () {
		// cache this for performance reasons
		system_categories = Appstream.get_system_categories ();
	}

	private string desktop_file_get_str (KeyFile key_file, string key) {
		string str = "";
		try {
			str = key_file.get_string ("Desktop Entry", key);
		} catch (Error e) {}

		return str;
	}

	private void process_desktop_file (string fname) {
		KeyFile desktopFile = new KeyFile ();

		try {
			desktopFile.load_from_file (fname, KeyFileFlags.NONE);
		} catch (Error e) {
			log_error ("Error while loading file %s: %s".printf (fname, e.message));
			return;
		}

		var app = new Appstream.AppInfo ();

		string[] lines = fname.split (":", 2);
		string desktop_file_name = lines[1];
		if (Utils.str_empty (desktop_file_name)) {
			desktop_file_name = Path.get_basename (fname);
		}

		if (desktop_file_get_str (desktopFile, "X-AppInstall-Ignore").down () == "true")
			return;

		app.desktop_file = desktop_file_name;
		app.pkgname = desktop_file_get_str (desktopFile, "X-AppInstall-Package");
		app.name = desktop_file_get_str (desktopFile, "Name");
		app.name_original = desktop_file_get_str (desktopFile, "Name");
		app.summary = desktop_file_get_str (desktopFile, "Comment");
		app.icon = desktop_file_get_str (desktopFile, "Icon");

		string categories = desktop_file_get_str (desktopFile, "Categories");
		string[] cats_strv = categories.split (";");
		app.categories = Appstream.Utils.categories_from_strv (cats_strv, system_categories);

		string mimetypes = desktop_file_get_str (desktopFile, "MimeType");
		if (!Utils.str_empty (mimetypes)) {
			app.mimetypes = mimetypes.split (";");
		}

		// TODO: Add remaining items, e.g. keywords, ...

		if (app.is_valid ()) {
			// stdout.printf ("Found: %s\n", app.to_string ());
			emit_application (app);
		} else {
			log_warning ("Invalid application found: %s\n". printf (app.to_string ()));
		}

	}

	public override bool execute () {
		Array<string>? desktopFiles = Utils.find_files_matching (Path.build_filename (UBUNTU_APPINSTALL_DIR, "desktop", null),
								   "*.desktop");
		if (desktopFiles == null)
			return false;

		for (uint i=0; i < desktopFiles.length; i++) {
			process_desktop_file (desktopFiles.index (i));
		}

		return true;
	}

}

} // End of namespace: Uai.Provider
