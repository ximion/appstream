/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined(__APPSTREAM_H) && !defined(AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_CONTEXT_H
#define __AS_CONTEXT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_CONTEXT (as_context_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsContext, as_context, AS, CONTEXT, GObject)

struct _AsContextClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

/**
 * AsFormatVersion:
 * @AS_FORMAT_VERSION_UNKNOWN:	Unknown format version
 * @AS_FORMAT_VERSION_V1_0:	1.0
 *
 * Format version / API level of the AppStream metadata.
 **/
typedef enum {
	AS_FORMAT_VERSION_UNKNOWN,
	AS_FORMAT_VERSION_V1_0,
	/*< private >*/
	AS_FORMAT_VERSION_LAST
} AsFormatVersion;

#define AS_FORMAT_VERSION_LATEST AS_FORMAT_VERSION_V1_0

const gchar    *as_format_version_to_string (AsFormatVersion version);
AsFormatVersion as_format_version_from_string (const gchar *version_str);

/**
 * AsFormatStyle:
 * @AS_FORMAT_STYLE_UNKNOWN:	The format style is unknown.
 * @AS_FORMAT_STYLE_METAINFO:	Parse AppStream upstream metadata (metainfo files)
 * @AS_FORMAT_STYLE_CATALOG:	Parse AppStream metadata catalog (shipped by software distributors)
 *
 * There are a few differences between AppStream's metainfo files (shipped by upstream projects)
 * and the catalog metadata (shipped by distributors).
 * The data source kind indicates which style we should process.
 * Usually you do not want to set this explicitly.
 **/
typedef enum {
	AS_FORMAT_STYLE_UNKNOWN,
	AS_FORMAT_STYLE_METAINFO,
	AS_FORMAT_STYLE_CATALOG,
	/*< private >*/
	AS_FORMAT_STYLE_LAST
} AsFormatStyle;

/**
 * AsFormatKind:
 * @AS_FORMAT_KIND_UNKNOWN:		Unknown metadata format.
 * @AS_FORMAT_KIND_XML:			AppStream XML metadata.
 * @AS_FORMAT_KIND_YAML:		AppStream YAML (DEP-11) metadata.
 * @AS_FORMAT_KIND_DESKTOP_ENTRY:	XDG Desktop Entry data.
 *
 * Format of the AppStream metadata.
 **/
typedef enum {
	AS_FORMAT_KIND_UNKNOWN,
	AS_FORMAT_KIND_XML,
	AS_FORMAT_KIND_YAML,
	AS_FORMAT_KIND_DESKTOP_ENTRY,
	/*< private >*/
	AS_FORMAT_KIND_LAST
} AsFormatKind;

const gchar *as_format_kind_to_string (AsFormatKind kind);
AsFormatKind as_format_kind_from_string (const gchar *kind_str);

/**
 * AsValueFlags:
 * @AS_VALUE_FLAG_NONE:				No flags.
 * @AS_VALUE_FLAG_DUPLICATE_CHECK:		Check for duplicates when adding items to list values.
 * @AS_VALUE_FLAG_NO_TRANSLATION_FALLBACK:	Don't fall back to C when retrieving translated values.
 *
 * Set how values assigned to an #AsComponent should be treated when
 * they are set or retrieved.
 */
typedef enum {
	AS_VALUE_FLAG_NONE		      = 0,
	AS_VALUE_FLAG_DUPLICATE_CHECK	      = 1 << 0,
	AS_VALUE_FLAG_NO_TRANSLATION_FALLBACK = 1 << 1
} AsValueFlags;

AsContext      *as_context_new (void);

AsFormatVersion as_context_get_format_version (AsContext *ctx);
void		as_context_set_format_version (AsContext *ctx, AsFormatVersion ver);

AsFormatStyle	as_context_get_style (AsContext *ctx);
void		as_context_set_style (AsContext *ctx, AsFormatStyle style);

gint		as_context_get_priority (AsContext *ctx);
void		as_context_set_priority (AsContext *ctx, gint priority);

const gchar    *as_context_get_origin (AsContext *ctx);
void		as_context_set_origin (AsContext *ctx, const gchar *value);

const gchar    *as_context_get_locale (AsContext *ctx);
void		as_context_set_locale (AsContext *ctx, const gchar *locale);

gboolean	as_context_has_media_baseurl (AsContext *ctx);
const gchar    *as_context_get_media_baseurl (AsContext *ctx);
void		as_context_set_media_baseurl (AsContext *ctx, const gchar *value);

gboolean	as_context_get_locale_use_all (AsContext *ctx);

const gchar    *as_context_get_filename (AsContext *ctx);
void		as_context_set_filename (AsContext *ctx, const gchar *fname);

AsValueFlags	as_context_get_value_flags (AsContext *ctx);
void		as_context_set_value_flags (AsContext *ctx, AsValueFlags flags);

G_END_DECLS

#endif /* __AS_CONTEXT_H */
