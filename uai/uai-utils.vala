/* uai-utils.vala -- Helper methods for update-appstream-index
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

namespace Uai.Utils {

private ulong now_sec () {
	TimeVal time_val = TimeVal ();

	return time_val.tv_sec;
}

private bool is_root () {
	if (Posix.getuid () == 0) {
		return true;
	} else {
		return false;
	}
}

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

} // End of namespace: Uai.Utils