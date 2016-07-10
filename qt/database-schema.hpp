/* database-schema.hpp -- Common specs for AppStream Xapian database
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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

#include "asxentries.pb.h"

#ifndef DATABASE_SCHEMA_H
#define DATABASE_SCHEMA_H

namespace ASCache {

// database schema version
static const int AS_DB_SCHEMA_VERSION = 2;

// values used in the database
namespace XapianValues {

enum XapianValues {
	TYPE = 100,
	IDENTIFIER = 101,
	CPTNAME = 120,
	CPTNAME_UNTRANSLATED = 121,
	PKGNAMES = 122,       // semicolon-separated list of strings
	SOURCE_PKGNAME = 123,
	BUNDLES = 124,        // protobuf serialization: Bundles
	EXTENDS = 125,        // semicolon-separated list of strings
	EXTENSIONS = 126,     // semicolon-separated list of strings

	SUMMARY = 130,
	DESCRIPTION = 131,
	CATEGORIES = 132,     // semicolon-separated list of strings

	ICONS = 133,          // protobuf serialization: Icons

	PROVIDED_ITEMS = 140, // protobuf serialization: ProvidedItems
	SCREENSHOTS = 141,    // protobuf serialization: Screenshots
	RELEASES = 142,       // protobuf serialization: Releases

	LICENSE = 150,
	URLS = 151,           // protobuf serialization: Urls

	PROJECT_GROUP = 160,
	DEVELOPER_NAME = 161,

	COMPULSORY_FOR = 170, // semicolon-separated list of strings
	LANGUAGES = 171,      // protobuf serialization: Languages

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

} // End of namespace: ASCache

#endif // DATABASE_SCHEMA_H