/* utils.vala
 *
 * Copyright (C) 2012 Matthias Klumpp
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

namespace Appstream.Utils {

private bool str_empty (string? str) {
	if ((str == "") || (str == null))
		return true;
	return false;
}

internal bool touch_dir (string dirname) {
	File d = File.new_for_path (dirname);
	try {
		if (!d.query_exists ()) {
			d.make_directory_with_parents ();
		}
	} catch (Error e) {
		error ("Unable to create directories! Error: %s".printf (e.message));
	}

	return true;
}

/*
 * Remove folder like rm -r does
 */
internal bool delete_dir_recursive (string dirname) {
	try {
		if (!FileUtils.test (dirname, FileTest.IS_DIR))
			return true;
		File dir = File.new_for_path (dirname);
		FileEnumerator enr = dir.enumerate_children ("standard::name", FileQueryInfoFlags.NOFOLLOW_SYMLINKS);
		if (enr != null) {
			FileInfo info = enr.next_file ();
			while (info != null) {
				string path = Path.build_filename (dirname, info.get_name ());
				if (FileUtils.test (path, FileTest.IS_DIR)) {
					delete_dir_recursive (path);
				} else {
					FileUtils.remove (path);
				}
				info = enr.next_file ();
			}
			if (FileUtils.test (dirname, FileTest.EXISTS))
				DirUtils.remove (dirname);
		}
	} catch (Error e) {
		critical ("Could not remove directory: %s", e.message);
		return false;
	}
	return true;
}

/**
 * Create a list of categories from string array
 */
internal Category[] categories_from_strv (string[] categories_strv, Category[] system_categories) {
	// This needs to be done way smarter...

	Category[] cat_list = {};
	foreach (string idstr in categories_strv) {
		foreach (Category sys_cat in system_categories) {
			if (sys_cat.id.down () == idstr.down ()) {
				cat_list += sys_cat;
				break;
			}
		}
	}

	return cat_list;
}

/**
 * Create a list of categories from semicolon-separated string
 */
private Category[]? categories_from_str (string categories_str, Category[] system_categories) {
	string[] cats = categories_str.split (";");
	if (cats.length == 0)
		return null;

	Category[] cat_list = categories_from_strv (cats, system_categories);

	return cat_list;

}

} // End of namespace: Appstream.Utils
