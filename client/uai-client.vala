/* uai-client.vala -- Simple client for the Update-AppStream-Index DBus service
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

private class UaiClient : Object {
	// Cmdln options
	private static bool o_show_version = false;
	private static bool o_verbose_mode = false;
	private static bool o_refresh = false;

	private MainLoop loop;

	public int exit_code { get; set; }

	private const OptionEntry[] options = {
		{ "version", 'v', 0, OptionArg.NONE, ref o_show_version,
		N_("Show the application's version"), null },
		{ "verbose", 0, 0, OptionArg.NONE, ref o_verbose_mode,
			N_("Enable verbose mode"), null },
		{ "refresh", 'v', 0, OptionArg.NONE, ref o_refresh,
		N_("Refresh the AppStream application cache"), null },
		{ null }
	};

	public UaiClient (string[] args) {
		exit_code = 0;
		var opt_context = new OptionContext ("- Update-AppStream-Index client tool.");
		opt_context.set_help_enabled (true);
		opt_context.add_main_entries (options, null);
		try {
			opt_context.parse (ref args);
		} catch (Error e) {
			stdout.printf (e.message + "\n");
			stdout.printf (_("Run '%s --help' to see a full list of available command line options.\n"), args[0]);
			exit_code = 1;
			return;
		}

		loop = new MainLoop ();
	}

	private void quit_loop () {
		if (loop.is_running ())
			loop.quit ();
	}

	public void run () {
		if (exit_code > 0)
			return;

		if (o_show_version) {
			stdout.printf ("AppStream-index client tool version: %s\n", Config.VERSION);
			return;
		}

		// Just a hack, we might need proper message handling later
		if (o_verbose_mode)
			Environment.set_variable ("G_MESSAGES_DEBUG", "all", true);


		UAIService uaisv = null;
		try {
			uaisv = Bus.get_proxy_sync (BusType.SYSTEM, "org.freedesktop.AppStream",
								"/org/freedesktop/appstream");

		} catch (IOError e) {
			stderr.printf ("%s\n", e.message);
		}

		if (o_refresh) {
			try {
				/* Connecting to 'finished' signal */
				uaisv.finished.connect(() => {
					stdout.printf ("The AppStream database maintenance tool has finished current action.\n");
					quit_loop ();
				});

			stdout.printf ("Rebuilding application-info cache...\n");

			uaisv.refresh ();

			} catch (IOError e) {
				stderr.printf ("%s\n", e.message);
			}
		} else {
			stderr.printf ("No command specified.\n");
			return;
		}

		loop.run();

	}

	static int main (string[] args) {
		var main = new UaiClient (args);

		// Run the application
		main.run ();
		int code = main.exit_code;

		return code;
	}

}
