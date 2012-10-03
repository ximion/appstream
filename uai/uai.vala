/* uai.vala -- Main file for update-appstream-index
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

namespace Uai {

private static string CURRENT_DB_PATH;

private class Main : Object {
	// Cmdln options
	private static bool o_show_version = false;
	private static bool o_verbose_mode = false;
	private static string o_database_path;

	private MainLoop loop;
	private Uai.Engine engine;
	private uint exit_idle_time;

	public int exit_code { get; set; }

	private const OptionEntry[] options = {
		{ "version", 'v', 0, OptionArg.NONE, ref o_show_version,
		N_("Show the application's version"), null },
		{ "verbose", 0, 0, OptionArg.NONE, ref o_verbose_mode,
			N_("Enable verbose mode"), null },
		{ "cachepath", 0, 0, OptionArg.FILENAME, ref o_database_path,
			N_("Path to AppStream cache directory"), N_("DIRECTORY") },
		{ null }
	};

	public Main (string[] args) {
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

	private void on_bus_aquired (DBusConnection conn) {
		try {
			conn.register_object ("/org/freedesktop/appstream", engine);
		} catch (IOError e) {
			stderr.printf ("Could not register service\n");
			exit_code = 6;
			quit_loop ();
		}
	}

	private bool main_timeout_check_cb () {
		uint idle;
		idle = engine.get_idle_time_seconds ();
		debug ("idle is %u", idle);
		if (idle > exit_idle_time) {
			warning ("exit!!");
			quit_loop ();
			return false;
		}

		return true;
	}

	public void run () {
		if (exit_code > 0)
			return;

		if (o_show_version) {
			stdout.printf ("update-appstream-index version: %s\n", Config.VERSION);
			return;
		}

		// Just a hack, we might need proper message handling later
		if (o_verbose_mode)
			Environment.set_variable ("G_MESSAGES_DEBUG", "all", true);

		if (Utils.str_empty (o_database_path))
			CURRENT_DB_PATH = "";
		else
			CURRENT_DB_PATH = o_database_path;

		engine = new Uai.Engine ();
		engine.init ();

		Bus.own_name (BusType.SYSTEM, "org.freedesktop.AppStream", BusNameOwnerFlags.NONE,
					on_bus_aquired,
					() => {},
					() => {
						stderr.printf ("Could not aquire name\n");
						exit_code = 4;
						quit_loop ();
					});

		if (exit_code == 0)
			stdout.printf ("Running Update-AppStream-Index service...\n");

		// TODO
		// Hardcode it for now, make it a setting later
		exit_idle_time = 24;

		// only poll when we are alive
		uint timer_id;
		if ((exit_idle_time != 0) && (exit_code == 0)) {
			timer_id = Timeout.add_seconds (5, main_timeout_check_cb);
			// FIXME: Vala bug - not present in Vapi, instead broken MainContext.set_name_by_id()
			// Source.set_name_by_id (timer_id, "[UaiMain] main poll");
		}

		// run main loop until quit
		loop.run ();
	}

	static int main (string[] args) {
		// Bind UAI locale
		Intl.setlocale(LocaleCategory.ALL,"");
		Intl.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
		Intl.bind_textdomain_codeset(Config.GETTEXT_PACKAGE, "UTF-8");
		Intl.textdomain(Config.GETTEXT_PACKAGE);

		var main = new Uai.Main (args);

		// Run the application
		main.run ();
		int code = main.exit_code;

		return code;
	}

}

} // End of namespace: Uai
