/* test-database-read.vala
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
using Appstream;

private string datadir;

void msg (string s) {
	stdout.printf (s + "\n");
}

void print_apparray (Array<Appstream.AppInfo> appArray) {
	for (uint i = 0; i < appArray.length; i++) {
		stdout.printf ("  - %s\n", appArray.index (i).to_string ());
	}
}

void test_database () {
	var db = new Appstream.Database ();
	Array<Appstream.AppInfo> apps = null;

	db.open ();

	apps = db.get_all_applications ();
	assert (apps != null);

	print_apparray (apps);

	msg ("==============================");

	var query = new SearchQuery ("firefox");
	apps = db.find_applications (query);
	print_apparray (apps);
}

int main (string[] args) {
	msg ("=== Running AppStream Database (Read) Tests ===");
	datadir = args[1];
	assert (datadir != null);
	datadir = Path.build_filename (datadir, "data", null);
	assert (FileUtils.test (datadir, FileTest.EXISTS) != false);

	Environment.set_variable ("G_MESSAGES_DEBUG", "all", true);
	Test.init (ref args);

	test_database ();

	Test.run ();
	return 0;
}
