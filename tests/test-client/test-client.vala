/* test-client.vala -- Example client for the Update-AppStream-Index DBus service
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

[DBus (name = "org.freedesktop.AppStream")]
interface UAIService : Object {
	public abstract bool refresh () throws IOError;
	public signal void finished ();
}

void main () {
	var loop = new MainLoop();

	UAIService uai = null;

	try {
		uai = Bus.get_proxy_sync (BusType.SYSTEM, "org.freedesktop.AppStream",
							"/org/freedesktop/appstream");

		/* Connecting to signal pong! */
		uai.finished.connect(() => {
			stdout.printf ("The AppStream database maintenance tool has finished current action.\n");
			loop.quit ();
		});

		stdout.printf ("Running Refresh() action now...\n");

	uai.refresh ();

	} catch (IOError e) {
		stderr.printf ("%s\n", e.message);
	}

	loop.run();
}
