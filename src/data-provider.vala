/* data-provider.vala
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

namespace Appstream {

private abstract class DataProvider : Object {
	public signal void application (Appstream.AppInfo app);
	public string[] watch_files { get; protected set; }

	public DataProvider () {
	}

	protected void emit_application (Appstream.AppInfo app) {
		application (app);
	}

	public abstract bool execute ();

	protected void log_error (string msg) {
		debug (msg);
	}

	protected void log_warning (string msg) {
		warning (msg);
	}

}

} // End of namespace: AppstreamBuilder
