/* uai-server.vala
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

namespace Uai {

[DBus (name = "org.freedesktop.AppStream")]
public class Server : Object {
	private ASXapian.Database db;
	private Array<AppInfo> appList;

	public signal void finished ();

	public Server () {
		db = new ASXapian.Database ();
		db.init (SOFTWARE_CENTER_DATABASE_PATH);
		appList = new Array<AppInfo> ();
	}

	private bool run_provider (DataProvider dprov) {
		dprov.application.connect (new_application);
		return dprov.execute ();
	}

	private void new_application (AppInfo app) {
		appList.append_val (app);
	}

	public bool refresh (GLib.BusName sender) {
		bool ret;
#if APPSTREAM
		run_provider (new Provider.Appstream ());
#endif
#if DEBIAN_DEP11
		run_provider (new Provider.DEP11 ());
#endif
#if UBUNTU_APPINSTALL
		run_provider (new Provider.UbuntuAppinstall ());
#endif

		ret = db.rebuild (appList);

		finished ();
		return ret;
	}
}

} // End of namespace: Uai
