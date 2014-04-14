/* database-common.hpp -- Common specs for AppStream Xapian database
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef DATABASE_COMMON_H
#define DATABASE_COMMON_H

namespace Appstream {

// values used in the database
namespace XapianValues {

enum XapianValues {
	TYPE = 140,
	CPTNAME = 141,
	CPTNAME_UNTRANSLATED = 142,
	IDENTIFIER = 143,
	PKGNAME = 144,
	ICON = 145,
	ICON_URL = 146,
	SUMMARY = 147,
	DESCRIPTION = 148,
	CATEGORIES = 149,
	PROVIDED_ITEMS = 150,
	SCREENSHOT_DATA = 151,	// screenshot definitions, as XML
	RELEASES_DATA = 152,	// releases definitions, as XML

	LICENSE = 153,
	URL_HOMEPAGE = 154,
	URL_BUGTRACKER = 155,
	URL_FAQ = 156,
	URL_DONATION = 157,

	PROJECT_GROUP = 160,

	COMPULSORY_FOR = 170,

	GETTEXT_DOMAIN = 180,
	ARCHIVE_SECTION = 181,
	ARCHIVE_CHANNEL = 182
};

};

// weights for the different fields
static const int WEIGHT_DESKTOP_NAME = 10;
static const int WEIGHT_DESKTOP_KEYWORD = 5;
static const int WEIGHT_DESKTOP_GENERICNAME = 3;
static const int WEIGHT_DESKTOP_SUMMARY = 1;

static const int WEIGHT_PKGNAME = 8;
static const int WEIGHT_SUMMARY = 5;
static const int WEIGHT_PK_DESCRIPTION = 1;

} // End of namespace: AppStream

#endif // DATABASE_COMMON_H
