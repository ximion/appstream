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

namespace Appstream {

// values used in the database
namespace XapianValues {

enum XapianValues {
	TYPE = 140,
	IDENTIFIER = 141,
	CPTNAME = 142,
	CPTNAME_UNTRANSLATED = 143,
	PKGNAME = 144,

	SUMMARY = 145,
	DESCRIPTION = 146,
	CATEGORIES = 147,

	ICON = 148,
	ICON_URLS = 149,

	PROVIDED_ITEMS = 150,
	SCREENSHOT_DATA = 152, // screenshot definitions, as XML
	RELEASES_DATA = 153, // releases definitions, as XML

	LICENSE = 154,
	URLS = 155,

	PROJECT_GROUP = 160,

	COMPULSORY_FOR = 170,
	LANGUAGES = 171,

	ORIGIN = 180,
	BUNDLES = 181

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

} // End of namespace: AppStream

#endif // DATABASE_COMMON_H
