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
	APPNAME = 170,
	APPNAME_UNTRANSLATED = 171,
	DESKTOP_FILE = 172,
	PKGNAME = 173,
	ICON = 174,
	ICON_URL = 175,
	SUMMARY = 176,
	DESCRIPTION = 177,
	SCREENSHOT_DATA = 178,	// screenshot definitions, as XML
	CATEGORIES = 179,
	LICENSE = 180,
	URL_HOMEPAGE = 181,

	GETTEXT_DOMAIN = 190,
	ARCHIVE_SECTION = 191,
	ARCHIVE_CHANNEL = 192
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
