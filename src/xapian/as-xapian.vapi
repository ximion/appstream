/* as-xapian.vapi
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

[CCode (cheader_filename = "database-vala.h")]
namespace ASXapian {

	[Compact]
	[CCode (cname="struct DatabaseRead", free_function="xa_database_read_free", cprefix="xa_database_read_")]
	public class DatabaseRead {
		[CCode (cname="xa_database_read_new")]
		public DatabaseRead ();

		public bool open (string db_path);
		public string get_schema_version ();

		public GLib.Array<Appstream.AppInfo> get_all_applications ();
	}

	[Compact]
	[CCode (cname="struct DatabaseWrite", free_function="xa_database_write_free", cprefix="xa_database_write_")]
	public class DatabaseWrite {
		[CCode (cname="xa_database_write_new")]
		public DatabaseWrite ();

		public bool init (string db_path);
		public bool add_application (Appstream.AppInfo app);
		public bool rebuild (GLib.Array<Appstream.AppInfo> apps);
	}

}
