/* update-appstream-index.vala
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
using Config;

public class UAIMain : Object {
	// Cmdln options
	private static bool o_show_version = false;
	private static bool o_verbose_mode = false;

	private MainLoop loop;

	public int exit_code { get; set; }

	private const OptionEntry[] options = {
		{ "version", 'v', 0, OptionArg.NONE, ref o_show_version,
		N_("Show the application's version"), null },
		{ "verbose", 0, 0, OptionArg.NONE, ref o_verbose_mode,
			N_("Activate verbose mode"), null },
		{ null }
	};

	public UAIMain (string[] args) {
		exit_code = 0;
		var opt_context = new OptionContext ("- maintain AppStream application index.");
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

	void on_bus_aquired (DBusConnection conn) {
		try {
			conn.register_object ("/org/freedesktop/appstream/database", new UAIServer ());
		} catch (IOError e) {
			stderr.printf ("Could not register service\n");
			exit_code = 6;
			quit_loop ();
		}

		if (exit_code == 0)
			stdout.printf ("Update-AppStream-Index service is running.\n");
	}

	public void run () {
		bool done = false;
		if (o_show_version) {
			stdout.printf ("lipkgen tool, part of Listaller version: %s\n", Config.VERSION);
			return;
		}

		// Just a hack, we might need proper message handling later
		if (o_verbose_mode)
			Environment.set_variable ("G_MESSAGES_DEBUG", "all", true);

		Bus.own_name (BusType.SYSTEM, "org.freedesktop.AppStream", BusNameOwnerFlags.NONE,
					on_bus_aquired,
					() => {},
					() => {
						stderr.printf ("Could not aquire name\n");
						exit_code = 4;
						quit_loop ();
					});

		loop.run ();
	}

	static int main (string[] args) {
		// Bind Listaller locale
		Intl.setlocale(LocaleCategory.ALL,"");
		Intl.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
		Intl.bind_textdomain_codeset(Config.GETTEXT_PACKAGE, "UTF-8");
		Intl.textdomain(Config.GETTEXT_PACKAGE);

		var main = new UAIMain (args);

		// Run the application
		main.run ();
		int code = main.exit_code;

		return code;
	}

}
