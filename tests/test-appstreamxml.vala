/* test-appstreamxml.vala
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
using Appstream;

private string datadir;

void msg (string s) {
	stdout.printf (s + "\n");
}

void test_appstream_parser () {
	var asxml = new Provider.AppStreamXML ();

	asxml.process_file (File.new_for_path (Path.build_filename (datadir, "appdata.xml", null)));
	asxml.process_compressed_file (File.new_for_path (Path.build_filename (datadir, "appdata.xml.gz", null)));
}

void test_screenshot_handling () {
	var asxml = new Provider.AppStreamXML ();
	Component? app = null;
	asxml.application.connect ( (newApp) => {
		app = newApp;
	});

	asxml.process_file (File.new_for_path (Path.build_filename (datadir, "appstream-test2.xml", null)));
	assert (app != null);

	string xml_data = app.dump_screenshot_data_xml ();
	debug (xml_data);

	// dirty...
	app.screenshots.remove_range (0, app.screenshots.len);

	app.load_screenshots_from_internal_xml (xml_data);
	for (uint i = 0; i < app.screenshots.len; i++) {
			Screenshot sshot = (Screenshot) app.screenshots.index (i);
			assert (sshot.urls.size () == 1);
			assert (sshot.thumbnail_urls.size () == 1);
			debug (sshot.caption);
	}
	assert (app.screenshots.len > 0);
}

int main (string[] args) {
	msg ("=== Running Appstream-XML Tests ===");
	datadir = args[1];
	assert (datadir != null);
	datadir = Path.build_filename (datadir, "data", null);
	assert (FileUtils.test (datadir, FileTest.EXISTS) != false);

	Environment.set_variable ("G_MESSAGES_DEBUG", "all", true);
	Test.init (ref args);

	test_appstream_parser ();
	test_screenshot_handling ();

	Test.run ();
	return 0;
}
