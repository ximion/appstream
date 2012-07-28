/* uai-engine.vala
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
public class Engine : Object {
	private ASXapian.Database db;
	private Array<AppInfo> appList;
	private Timer timer;

	public signal void finished ();
	public signal void rebuild_finished ();

	public Engine () {
		db = new ASXapian.Database ();
		// Make sure directory exists
		Utils.touch_dir (SOFTWARE_CENTER_DATABASE_PATH);

		timer = new Timer ();
		appList = new Array<AppInfo> ();
	}

	public void init () {
		db.init (SOFTWARE_CENTER_DATABASE_PATH);
		timer.start ();
	}

	private bool run_provider (DataProvider dprov) {
		dprov.application.connect (new_application);
		return dprov.execute ();
	}

	private void new_application (AppInfo app) {
		appList.append_val (app);
	}

	public uint get_idle_time_seconds () {
		return (uint) timer.elapsed ();
	}

	public bool refresh (GLib.BusName sender) {
		bool ret;
		timer.stop ();
		timer.reset ();
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

		rebuild_finished ();
		timer.start ();

		return ret;
	}
}

} // End of namespace: Uai
