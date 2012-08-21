/* database.vala -- Access the AppStream database
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
using Appstream.Utils;

namespace Appstream {

/**
 * Class to access the AppStream
 * application database
 */
public class Database : Object {
	private ASXapian.DatabaseRead db;
	private bool opened_;

	public string database_path { get; set; }

	public Database () {
		db = new ASXapian.DatabaseRead ();
		opened_ = false;
		database_path = SOFTWARE_CENTER_DATABASE_PATH;
	}

	public virtual void open () {
		db.open (database_path);
		opened_ = true;
	}

	public Array<Appstream.AppInfo>? get_all_applications () {
		if (!opened_)
			return null;
		Array<Appstream.AppInfo> appArray = db.get_all_applications ();
		return appArray;
	}

	public bool db_exists () {
		if (FileUtils.test (database_path, FileTest.IS_DIR))
			return true;
		else
			return false;
	}
}

/**
 * Internal class to allow helper applications
 * to modify the AppStream application database
 */
internal class DatabaseWrite : Database {
	private ASXapian.DatabaseWrite db_w;

	public DatabaseWrite () {
		base ();
		db_w = new ASXapian.DatabaseWrite ();
	}

	public override void open () {
		base.open ();
		db_w.init (database_path);
	}

	public bool rebuild (Array<AppInfo> appList) {
		bool ret;
		ret = db_w.rebuild (appList);
		return ret;
	}
}

} // End of namespace: Appstream
