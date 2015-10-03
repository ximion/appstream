/* database-common.hpp -- Common specs for AppStream Xapian database
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
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

namespace AppStream {

// values used in the database
namespace XapianValues {

enum XapianValues {
	TYPE = 120,
	IDENTIFIER = 121,
	CPTNAME = 122,
	CPTNAME_UNTRANSLATED = 123,
	PKGNAME = 124,
	SOURCE_PKGNAME = 125,
	BUNDLES = 126,

	SUMMARY = 127,
	DESCRIPTION = 128,
	CATEGORIES = 129,

	ICON = 130,
	ICON_URLS = 131,

	PROVIDED_ITEMS = 140,
	SCREENSHOT_DATA = 141, // screenshot definitions, as XML
	RELEASES_DATA = 142, // releases definitions, as XML

	LICENSE = 150,
	URLS = 151,

	PROJECT_GROUP = 160,
	DEVELOPER_NAME = 161,

	COMPULSORY_FOR = 170,
	LANGUAGES = 171,

	ORIGIN = 180,


//!	GETTEXT_DOMAIN = 180,
//!	ARCHIVE_SECTION = 181,
//!	ARCHIVE_CHANNEL = 182
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

static const int AS_DB_SCHEMA_VERSION = 1;

} // End of namespace: AppStream

#endif // DATABASE_COMMON_H
