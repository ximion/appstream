/* database-builder.vala
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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
using Appstream;

[CCode (lower_case_cprefix = "as_", cprefix = "As")]
namespace Appstream {

internal class Builder : Object {
	private Appstream.DatabaseWrite db_rw;
	private Array<Component> appList;
	private string CURRENT_DB_PATH;

	private DataProvider[] providers;

	public Builder () {
		db_rw = new Appstream.DatabaseWrite ();

		// Update db path if necessary
		if (Utils.str_empty (CURRENT_DB_PATH))
			CURRENT_DB_PATH = db_rw.database_path;

		appList = new Array<Component> ();

		providers = {};
		providers += new Provider.AppStreamXML ();
#if DEBIAN_DEP11
		providers += new Provider.DEP11 ();
#endif
#if UBUNTU_APPINSTALL
		providers += new Provider.UbuntuAppinstall ();
#endif
		foreach (DataProvider dprov in providers)
			dprov.application.connect (new_application);
	}

	public Builder.path (string dbpath) {
		base ();
		CURRENT_DB_PATH = dbpath;
	}

	public void initialize () {
		db_rw.database_path = CURRENT_DB_PATH;

		// Make sure directory exists
		Utils.touch_dir (CURRENT_DB_PATH);

		db_rw.open ();
	}

	private void new_application (Component app) {
		appList.append_val (app);
	}

	private string[] get_watched_files () {
		string[] wfiles = {};
		foreach (DataProvider dprov in providers) {
			foreach (string s in dprov.watch_files)
				wfiles += s;
		}

		return wfiles;
	}

	private bool appstream_data_changed () {
		var file = File.new_for_path (Path.build_filename (APPSTREAM_CACHE_PATH, "cache.watch", null));
		string[] watchfile = {};
		string[] watchfile_new = {};

		if (file.query_exists ()) {
			try {
				var dis = new DataInputStream (file.read ());
				string line;
				while ((line = dis.read_line (null)) != null) {
					watchfile += line;
				}
			} catch (Error e) {
				return true;
			}
		}

		string[] files = get_watched_files ();
		bool ret = false;
		foreach (string fname in files) {
			Posix.Stat? sbuf;
			Posix.stat(fname, out sbuf);
			if (sbuf == null)
				continue;
			string ctime_str = "%ld".printf (sbuf.st_ctime);

			watchfile_new += "%s %s".printf (fname, ctime_str);
			if (watchfile.length == 0) {
				ret = true;
				continue;
			}

			foreach (string wentry in watchfile) {
				if (wentry.has_prefix (fname)) {
					string[] wparts = wentry.split (" ", 2);
					if (wparts[1] != ctime_str)
						ret = true;
					break;
				}
			}
		}

		// write our watchfile
		try {
			if (file.query_exists ())
				file.delete ();
			var dos = new DataOutputStream (file.create (FileCreateFlags.REPLACE_DESTINATION));
			foreach (string line in watchfile_new)
				dos.put_string ("%s\n".printf (line));
		} catch (Error e) {
			return ret;
		}

		return ret;
	}

	public bool refresh_cache (bool force = false) {
		bool ret = false;

		if (!force) {
			// check if we need to refresh the cache
			// (which is only necessary if the AppStream data has changed)
			if (!appstream_data_changed ()) {
				debug ("Data did not change, no cache refresh done.");
				return true;
			}
		}

		debug ("Refreshing AppStream cache");

		// call all AppStream data providers to return applications they find
		foreach (DataProvider dprov in providers)
			dprov.execute ();

		ret = db_rw.rebuild (appList);

		if (ret)
			debug ("Cache refresh completed successfully.");
		else
			debug ("Unable to refresh AppStream cache");

		return ret;
	}
}

} // End of namespace: Uai
