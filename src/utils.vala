/* utils.vala
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

namespace Appstream.Utils {

private bool str_empty (string? str) {
	if ((str == "") || (str == null))
		return true;
	return false;
}

private bool touch_dir (string dirname) {
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
internal List<Category> categories_from_strv (string[] categories_strv, List<Category> system_categories) {
	// This needs to be done way smarter...

	var cat_list = new List<Category> ();
	foreach (string idstr in categories_strv) {
		for(int i = 0; i < system_categories.length (); i++) {
			Category sys_cat = system_categories.nth_data (i);
			if (sys_cat.name == null)
				continue;
			if (sys_cat.name.down () == idstr.down ()) {
				cat_list.append (sys_cat);
				break;
			}
		}
	}

	return cat_list;
}

/**
 * Create a list of categories from semicolon-separated string
 */
private List<Category>? categories_from_str (string categories_str, List<Category> system_categories) {
	string[] cats = categories_str.split (";");
	if (cats.length == 0)
		return null;

	List<Category> cat_list = categories_from_strv (cats, system_categories);

	return cat_list;

}

private string category_list_to_string (List<Category> list) {
	string res = "";

	list.foreach ((entry) => {
		res = "%s%s\n".printf (res, entry.name);
	});

	return res;
}

private string string_list_to_string (List<string> list) {
	string res = "";

	list.foreach ((entry) => {
		res = "%s%s\n".printf (res, entry);
	});

	return res;
}

private Array<string>? find_files_matching (string dir, string pattern, bool recursive = false) {
	var list = new Array<string> ();
	try {
		var directory = File.new_for_path (dir);

		var enumerator = directory.enumerate_children (FileAttribute.STANDARD_NAME, 0);

		FileInfo file_info;
		while ((file_info = enumerator.next_file ()) != null) {
			string path = Path.build_filename (dir, file_info.get_name (), null);

			if (file_info.get_is_hidden ())
				continue;
			if ((!FileUtils.test (path, FileTest.IS_REGULAR)) && (recursive)) {
				Array<string> subdir_list = find_files_matching (path, pattern, recursive);
				// There was an error, exit
				if (subdir_list == null)
					return null;
				for (uint i=0; i < subdir_list.length; i++) {
					list.append_val (subdir_list.index (i));
				}
			} else {
				if (pattern != "") {
					string fname = file_info.get_name ();
					if (!PatternSpec.match_simple (pattern, fname))
						continue;
				}
				list.append_val (path);
			}
		}

	} catch (Error e) {
		stderr.printf (_("Error while finding files in directory %s: %s") + "\n", dir, e.message);
		return null;
	}
	return list;
}

private Array<string>? find_files (string dir, bool recursive = false) {
	return find_files_matching (dir, "", recursive);
}

private bool is_root () {
	if (Posix.getuid () == 0) {
		return true;
	} else {
		return false;
	}
}

private string? load_file_to_string (string fname) throws IOError {
	var file = File.new_for_path (fname);
	if (!file.query_exists ()) {
		return null;
	}

	string res = "";
	try {
		string line;
		var dis = new DataInputStream (file.read ());
		// Read lines until end of file (null) is reached
		while ((line = dis.read_line (null)) != null) {
			res += line + "\n";
		}

	} catch (IOError e) {
		throw e;
	}
	return res;
}

} // End of namespace: Appstream.Utils
