/* database.vala -- Access the AppStream database
 *
 * Copyright (C) 2012-2013 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

using GLib;
using Appstream.Utils;

namespace Appstream {

/** TRANSLATORS: List of "grey-listed" words sperated with ";"
 * Do not translate this list directly. Instead,
 * provide a list of words in your language that people are likely
 * to include in a search but that should normally be ignored in
 * the search.
 */
private static const string SEARCH_GREYLIST_STR = _("app;application;package;program;programme;suite;tool");

public class SearchQuery : Object {
	public string search_term { get; set; }
	public Category[] categories { get; set; }

	public SearchQuery (string term = "") {
		search_term = term;
	}

	public bool get_search_all_categories () {
		return (categories.length <= 0);
	}

	public void set_search_all_categories () {
		categories = {};
	}

	public bool set_categories_from_string (string categories_str) {
		Category[]? catlist = Utils.categories_from_str (categories_str, get_system_categories ());
		if (catlist == null)
			return false;

		categories = catlist;

		return true;
	}

	internal void sanitize_search_term () {
		// check if there is a ":" in the search, if so, it means the user
		// is using a xapian prefix like "pkg:" or "mime:" and in this case
		// we do not want to alter the search term (as application is in the
		// greylist but a common mime-type prefix)
		if (search_term.index_of (":") <= 0) {
			// filter query by greylist (to avoid overly generic search terms)
			string orig_search_term = search_term;
			foreach (string term in SEARCH_GREYLIST_STR.split (";")) {
				search_term = search_term.replace (term, "");
			}

			// restore query if it was just greylist words
			if (search_term == "") {
				debug ("grey-list replaced all terms, restoring");
				search_term = orig_search_term;
			}
		}

		// we have to strip the leading and trailing whitespaces to avoid having
		// different results for e.g. 'font ' and 'font' (LP: #506419)
		search_term = search_term.strip ();
	}
}

/**
 * Class to access the AppStream
 * application database
 */
public class Database : Object {
	private ASXapian.DatabaseRead db;
	private bool opened_;

	public string database_path { get; internal set; }

	public Database () {
		db = new ASXapian.DatabaseRead ();
		opened_ = false;
		database_path = SOFTWARE_CENTER_DATABASE_PATH;
	}

	public virtual bool open () {
		bool ret = db.open (database_path);
		opened_ = ret;

		return ret;
	}

	public bool db_exists () {
		if (FileUtils.test (database_path, FileTest.IS_DIR))
			return true;
		else
			return false;
	}

	public Array<Appstream.AppInfo>? get_all_applications () {
		if (!opened_)
			return null;
		Array<Appstream.AppInfo> appArray = db.get_all_applications ();
		return appArray;
	}

	public Array<Appstream.AppInfo>? find_applications (SearchQuery query) {
		if (!opened_)
			return null;

		Array<Appstream.AppInfo> appArray = db.find_applications (query);
		return appArray;
	}

	public Array<Appstream.AppInfo>? find_applications_by_str (string search_str, string? categories_str = null) {
		var query = new SearchQuery (search_str);
		if (categories_str == null)
			query.set_search_all_categories ();
		else
			query.set_categories_from_string (categories_str);

		return find_applications (query);
	}

}

/**
 * Internal class to allow helper applications
 * to modify the AppStream application database
 */
internal class DatabaseWrite : Database {
	private ASXapian.DatabaseWrite db_w;

	public DatabaseWrite () {
		base ();
		db_w = new ASXapian.DatabaseWrite ();
		// ensure db directory exists
		touch_dir (SOFTWARE_CENTER_DATABASE_PATH);
	}

	public override bool open () {
		bool ret = db_w.init (database_path);
		ret = base.open ();

		return ret;
	}

	public bool rebuild (Array<AppInfo> appList) {
		bool ret;
		ret = db_w.rebuild (appList);
		return ret;
	}
}

} // End of namespace: Appstream
