/* test-basics.vala
 *
 * Copyright (C) 2012-2014 Matthias Klumpp
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
using AppStream;

private string datadir;

/* NOTE: All these tests are not really a testsuite, but quickly test at least a few things.
 * writing a good testsuite would be nice, but given the amount of code in libappstream and
 * the effort to do it sane (we need a virtual environment with PackageKit running!) I assume it's
 * not worth it.
 */

void msg (string s) {
	stdout.printf (s + "\n");
}

void test_menuparser () {
	var parser = new MenuParser ();
	List<Category> menu_dirs = parser.parse();
	assert (menu_dirs.length () > 4);

	menu_dirs.foreach ((cat) => {
		stdout.printf ("Category: %s\nExc:%s\nInc: %s\nSubcat: %s\n", cat.name,
			       Utils.string_list_to_string (cat.excluded),
			       Utils.string_list_to_string (cat.included),
			       Utils.category_list_to_string (cat.subcategories));
	});

	var query = new SearchQuery ();
	query.categories = {"science", "internet"};
}

void test_screenshotservice () {
	var screenshot_srv = new ScreenshotService ();
	string url = screenshot_srv.get_thumbnail_url ("ardour");

	msg ("Url: %s".printf (url));
	assert (url.has_prefix ("http://") == true);
	assert (url.has_suffix ("ardour") == true);
}

int main (string[] args) {
	msg ("=== Running Basic Tests ===");
	datadir = args[1];
	assert (datadir != null);
	datadir = Path.build_filename (datadir, "data", null);
	assert (FileUtils.test (datadir, FileTest.EXISTS) != false);

	Environment.set_variable ("G_MESSAGES_DEBUG", "all", true);
	Test.init (ref args);

	test_menuparser ();
	test_screenshotservice ();

	Test.run ();
	return 0;
}
