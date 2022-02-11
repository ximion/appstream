/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_CONTEXT_H
#define __AS_CONTEXT_H

#include <glib-object.h>
#include "as-enums.h"

G_BEGIN_DECLS

#define AS_TYPE_CONTEXT (as_context_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsContext, as_context, AS, CONTEXT, GObject)

struct _AsContextClass
{
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
 * @AS_FORMAT_VERSION_UNKNOWN:	Unknown
 * @AS_FORMAT_VERSION_V0_6:	0.6
 * @AS_FORMAT_VERSION_V0_7:	0.7
 * @AS_FORMAT_VERSION_V0_8:	0.8
 * @AS_FORMAT_VERSION_V0_9:	0.9
 * @AS_FORMAT_VERSION_V0_10:	0.10
 * @AS_FORMAT_VERSION_V0_11:	0.11
 * @AS_FORMAT_VERSION_V0_12:	0.12
 * @AS_FORMAT_VERSION_V0_13:	0.13
 * @AS_FORMAT_VERSION_V0_14:	0.14
 *
 * Format version / API level of the AppStream metadata.
 **/
typedef enum {
	AS_FORMAT_VERSION_V0_6,
	AS_FORMAT_VERSION_V0_7,
	AS_FORMAT_VERSION_V0_8,
	AS_FORMAT_VERSION_V0_9,
	AS_FORMAT_VERSION_V0_10,
	AS_FORMAT_VERSION_V0_11,
	AS_FORMAT_VERSION_V0_12,
	AS_FORMAT_VERSION_V0_13,
	AS_FORMAT_VERSION_V0_14,
	AS_FORMAT_VERSION_UNKNOWN, /* added to work around GIR inconsistencies */
	/*< private >*/
	AS_FORMAT_VERSION_LAST
} AsFormatVersion;

#define AS_FORMAT_VERSION_CURRENT AS_FORMAT_VERSION_V0_14

const gchar		*as_format_version_to_string (AsFormatVersion version);
AsFormatVersion		 as_format_version_from_string (const gchar *version_str);

AsContext		*as_context_new (void);

AsFormatVersion		as_context_get_format_version (AsContext *ctx);
void			as_context_set_format_version (AsContext *ctx,
						       AsFormatVersion ver);

AsFormatStyle		as_context_get_style (AsContext *ctx);
void			as_context_set_style (AsContext *ctx,
						AsFormatStyle style);

gint			as_context_get_priority (AsContext *ctx);
void			as_context_set_priority (AsContext *ctx,
						 gint priority);

const gchar		*as_context_get_origin (AsContext *ctx);
void			as_context_set_origin (AsContext *ctx,
					       const gchar *value);

const gchar		*as_context_get_locale (AsContext *ctx);
void			as_context_set_locale (AsContext *ctx,
					       const gchar *value);

gboolean		as_context_has_media_baseurl (AsContext *ctx);
const gchar		*as_context_get_media_baseurl (AsContext *ctx);
void			as_context_set_media_baseurl (AsContext *ctx,
						      const gchar *value);

gboolean		as_context_get_locale_all_enabled (AsContext *ctx);

const gchar		*as_context_get_filename (AsContext *ctx);
void			as_context_set_filename (AsContext *ctx,
					       const gchar *fname);

G_END_DECLS

#endif /* __AS_CONTEXT_H */
