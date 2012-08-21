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
	//private ASXapian.DatabaseRead db;

	public Database () {
		//db = new ASXapian.DatabaseRead ();
	}

	public virtual void open () {
		//db.init (SOFTWARE_CENTER_DATABASE_PATH);
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
		db_w.init (SOFTWARE_CENTER_DATABASE_PATH);
	}

	public bool rebuild (Array<AppInfo> appList) {
		bool ret;
		ret = db_w.rebuild (appList);
		return ret;
	}

	public string get_db_path () {
		return SOFTWARE_CENTER_DATABASE_PATH;
	}
}

} // End of namespace: Appstream
