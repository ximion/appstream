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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_NEWS_CONVERT_H
#define __AS_NEWS_CONVERT_H

#include <glib-object.h>
#include "as-release.h"

G_BEGIN_DECLS

/**
 * AsNewsFormatKind:
 * @AS_NEWS_FORMAT_KIND_UNKNOWN:	Unknown release info format.
 * @AS_NEWS_FORMAT_KIND_YAML:		YAML release information.
 * @AS_NEWS_FORMAT_KIND_TEXT:		Pure text release information.
 *
 * Format of a NEWS file.
 **/
typedef enum {
	AS_NEWS_FORMAT_KIND_UNKNOWN,
	AS_NEWS_FORMAT_KIND_YAML,
	AS_NEWS_FORMAT_KIND_TEXT,
	/*< private >*/
	AS_NEWS_FORMAT_KIND_LAST
} AsNewsFormatKind;

const gchar		*as_news_format_kind_to_string (AsNewsFormatKind kind);
AsNewsFormatKind	as_news_format_kind_from_string (const gchar *kind_str);

GPtrArray		*as_news_yaml_to_releases (const gchar *yaml_data, GError **error);

gboolean		as_news_releases_to_yaml (GPtrArray *releases, gchar **yaml_data);

G_END_DECLS

#endif /* __AS_NEWS_CONVERT_H */
