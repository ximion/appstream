/* appstream-index.vala -- Simple client for the Update-AppStream-Index DBus service
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
using Appstream;

private class ASClient : Object {
	// Cmdln options
	private static bool o_show_version = false;
	private static bool o_verbose_mode = false;
	private static bool o_no_color = false;
	private static bool o_refresh = false;
	private static bool o_force = false;
	private static string? o_search = null;

	private MainLoop loop;

	public int exit_code { get; set; }

	private const OptionEntry[] options = {
		{ "version", 'v', 0, OptionArg.NONE, ref o_show_version,
		N_("Show the application's version"), null },
		{ "verbose", 0, 0, OptionArg.NONE, ref o_verbose_mode,
		N_("Enable verbose mode"), null },
		{ "no-color", 0, 0, OptionArg.NONE, ref o_no_color,
		N_("Don't show colored output"), null },
		{ "refresh", 0, 0, OptionArg.NONE, ref o_refresh,
		N_("Rebuild the application information cache"), null },
		{ "force", 0, 0, OptionArg.NONE, ref o_force,
		N_("Enforce a cache refresh"), null },
		{ "search", 's', 0, OptionArg.STRING, ref o_search,
		N_("Search the application database"), null },
		{ null }
	};

	public ASClient (string[] args) {
		exit_code = 0;
		var opt_context = new OptionContext ("- Appstream-Index client tool.");
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

	private void print_key_value (string key, string val, bool highlight = false) {
		if ((val == null) || (val == ""))
			return;
		stdout.printf ("%c[%dm%s%c[%dm%s\n", 0x1B, 1, "%s: ".printf (key), 0x1B, 0, val);
	}

	private void print_separator () {
		stdout.printf ("%c[%dm%s\n%c[%dm", 0x1B, 36, "----", 0x1B, 0);
	}


	public void run () {
		if (exit_code > 0)
			return;

		if (o_show_version) {
			stdout.printf ("Appstream-Index client tool version: %s\n", Config.VERSION);
			return;
		}

		// Just a hack, we might need proper message handling later
		if (o_verbose_mode)
			Environment.set_variable ("G_MESSAGES_DEBUG", "all", true);


		// Prepare the AppStream database connection
		var db = new Appstream.Database ();

		if (o_search != null) {
			db.open ();
			PtrArray? app_list;
			app_list = db.find_applications_by_str (o_search);
			if (app_list == null) {
				// this might be an error
				stdout.printf ("Unable to find application matching %s!\n", o_search);
				exit_code = 4;
				return;
			}
			if (app_list.len == 0) {
				stdout.printf ("No application matching '%s' found.\n", o_search);
				return;
			}
			for (uint i = 0; i < app_list.len; i++) {
				var app = (Appstream.AppInfo) app_list.index (i);
				print_key_value ("Application", app.name);
				print_key_value ("Summary", app.summary);
				print_key_value ("Package", app.pkgname);
				print_key_value ("Homepage", app.homepage);
				print_key_value ("Desktop-File", app.desktop_file);
				print_key_value ("Icon", app.icon_url);
				for (uint j = 0; j < app.screenshots.len; j++) {
					Screenshot sshot = (Screenshot) app.screenshots.index (j);
					if (sshot.is_default ()) {
						// just return some size right now
						// FIXME: return the larges size?
						string[] sizes = sshot.get_available_sizes ();
						if (sizes[0] == null)
							continue;
						string sshot_url = sshot.get_url_for_size (sizes[0]);
						string caption = sshot.caption;
						if (caption == "")
							print_key_value ("Screenshot", sshot_url);
						else
							print_key_value ("Screenshot", "%s (%s)".printf (sshot_url, caption));
						break;
					}
				}

				print_separator ();
			}

		} else if (o_refresh) {
			if (Posix.getuid () != 0) {
				stdout.printf ("You need to run this command with superuser permissions!\n");
				exit_code = 2;
				return;
			}
			var builder = new Appstream.Builder ();
			builder.initialize ();
			builder.refresh_cache (o_force);
		} else {
			stderr.printf ("No command specified.\n");
			return;
		}
	}

	static int main (string[] args) {
		// Bind locale
		Intl.setlocale(LocaleCategory.ALL,"");
		Intl.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
		Intl.bind_textdomain_codeset(Config.GETTEXT_PACKAGE, "UTF-8");
		Intl.textdomain(Config.GETTEXT_PACKAGE);

		var main = new ASClient (args);

		// Run the application
		main.run ();
		int code = main.exit_code;

		return code;
	}
}
