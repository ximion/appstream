/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2019 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:as-news-convert
 * @short_description: Read and write NEWS/Changelog files from metainfo
 * @include: appstream.h
 *
 * Read NEWS and other types of release information files and convert them
 * to AppStream metainfo data.
 * Also, write NEWS files from #AsRelease release information.
 *
 * This is a private/internal class.
 */

#include "config.h"
#include "as-news-convert.h"
#include "as-yaml.h"

/**
 * as_news_format_kind_to_string:
 * @kind: the #AsNewsFormatKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.12.9
 **/
const gchar*
as_news_format_kind_to_string (AsNewsFormatKind kind)
{
	if (kind == AS_NEWS_FORMAT_KIND_YAML)
		return "yaml";
	if (kind == AS_NEWS_FORMAT_KIND_TEXT)
		return "text";
	return "unknown";
}

/**
 * as_news_format_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsNewsFormatKind or %AS_NEWS_FORMAT_KIND_UNKNOWN for unknown
 *
 * Since: 0.12.9
 **/
AsNewsFormatKind
as_news_format_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "yaml") == 0)
		return AS_NEWS_FORMAT_KIND_YAML;
	if (g_strcmp0 (kind_str, "text") == 0)
		return AS_NEWS_FORMAT_KIND_TEXT;
	return AS_NEWS_FORMAT_KIND_UNKNOWN;
}
