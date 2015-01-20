/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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

#include "as-enums.h"

/**
 * SECTION:as-enums
 * @short_description: Some enums used by various other modules
 * @include: appstream.h
 */

/**
 * as_url_kind_to_string:
 * @url_kind: the %AsUrlKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @url_kind
 **/
const gchar *
as_url_kind_to_string (AsUrlKind url_kind)
{
	if (url_kind == AS_URL_KIND_HOMEPAGE)
		return "homepage";
	if (url_kind == AS_URL_KIND_BUGTRACKER)
		return "bugtracker";
	if (url_kind == AS_URL_KIND_FAQ)
		return "faq";
	if (url_kind == AS_URL_KIND_HELP)
		return "help";
	if (url_kind == AS_URL_KIND_DONATION)
		return "donation";
	return "unknown";
}

/**
 * as_url_kind_from_string:
 * @url_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsUrlKind or %AS_URL_KIND_UNKNOWN for unknown
 **/
AsUrlKind
as_url_kind_from_string (const gchar *url_kind)
{
	if (g_strcmp0 (url_kind, "homepage") == 0)
		return AS_URL_KIND_HOMEPAGE;
	if (g_strcmp0 (url_kind, "bugtracker") == 0)
		return AS_URL_KIND_BUGTRACKER;
	if (g_strcmp0 (url_kind, "faq") == 0)
		return AS_URL_KIND_FAQ;
	if (g_strcmp0 (url_kind, "help") == 0)
		return AS_URL_KIND_HELP;
	if (g_strcmp0 (url_kind, "donation") == 0)
		return AS_URL_KIND_DONATION;
	return AS_URL_KIND_UNKNOWN;
}

/**
 * as_bundle_kind_to_string:
 * @bundle_kind: the %AsBundleKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @bundle_kind
 *
 * Since: 0.8.0
 **/
const gchar *
as_bundle_kind_to_string (AsBundleKind bundle_kind)
{
	if (bundle_kind == AS_BUNDLE_KIND_LIMBA)
		return "limba";
	if (bundle_kind == AS_BUNDLE_KIND_XDG_APP)
		return "xdg-app";
	return "unknown";
}

/**
 * as_bundle_kind_from_string:
 * @bundle_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsBundleKind or %AS_BUNDLE_KIND_UNKNOWN for unknown
 **/
AsBundleKind
as_bundle_kind_from_string (const gchar *bundle_kind)
{
	if (g_strcmp0 (bundle_kind, "limba") == 0)
		return AS_BUNDLE_KIND_LIMBA;
	if (g_strcmp0 (bundle_kind, "xdg-app") == 0)
		return AS_BUNDLE_KIND_LIMBA;
	return AS_BUNDLE_KIND_UNKNOWN;
}
