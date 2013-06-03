/* settings.vala
 *
 * Copyright (C) 2012 Matthias Klumpp <matthias@tenstral.net>
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
using Config;

namespace Appstream {

private static const string DB_SCHEMA_VERSION = "6";

internal static const string SOFTWARE_CENTER_DATABASE_PATH = "/var/cache/app-info/xapian";

private static const string APPSTREAM_BASE_PATH = DATADIR + "/app-info";

/**
 * The path where software icons (of not-installed software) are located.
 */
public static const string ICON_PATH = APPSTREAM_BASE_PATH + "/icons";

internal static const string APPSTREAM_XML_PATH = APPSTREAM_BASE_PATH + "/xmls";

} // End of namespace: Appstream
