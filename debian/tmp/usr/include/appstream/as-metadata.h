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

#if !defined(__APPSTREAM_H) && !defined(AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_METADATA_H
#define __AS_METADATA_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "as-component.h"
#include "as-context.h"

G_BEGIN_DECLS

#define AS_TYPE_METADATA (as_metadata_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsMetadata, as_metadata, AS, METADATA, GObject)

struct _AsMetadataClass {
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
 * AsParseFlags:
 * @AS_PARSE_FLAG_NONE:				No flags.
 * @AS_PARSE_FLAG_IGNORE_MEDIABASEURL:		Do not process the media_baseurl document property.
 *
 * Influence certain aspects of how AppStream metadata is parsed.
 */
typedef enum {
	AS_PARSE_FLAG_NONE		  = 0,
	AS_PARSE_FLAG_IGNORE_MEDIABASEURL = 1 << 0
} AsParseFlags;

/**
 * AsMetadataError:
 * @AS_METADATA_ERROR_FAILED:			Generic failure.
 * @AS_METADATA_ERROR_PARSE:			Unable to parse the metadata file.
 * @AS_METADATA_ERROR_FORMAT_UNEXPECTED:	Expected catalog metadata but got metainfo metadata, or vice versa.
 * @AS_METADATA_ERROR_NO_COMPONENT:		We expected a component in the pool, but couldn't find one.
 * @AS_METADATA_ERROR_VALUE_MISSING:		A essential value is missing in the source document.
 *
 * A metadata processing error.
 **/
typedef enum {
	AS_METADATA_ERROR_FAILED,
	AS_METADATA_ERROR_PARSE,
	AS_METADATA_ERROR_FORMAT_UNEXPECTED,
	AS_METADATA_ERROR_NO_COMPONENT,
	AS_METADATA_ERROR_VALUE_MISSING,
	/*< private >*/
	AS_METADATA_ERROR_LAST
} AsMetadataError;

#define AS_METADATA_ERROR as_metadata_error_quark ()

AsFormatStyle as_metadata_file_guess_style (const gchar *filename);

AsMetadata   *as_metadata_new (void);
GQuark	      as_metadata_error_quark (void);

gboolean      as_metadata_parse_file (AsMetadata  *metad,
				      GFile	  *file,
				      AsFormatKind format,
				      GError	 **error);

gboolean      as_metadata_parse_data (AsMetadata  *metad,
				      const gchar *data,
				      gssize	   data_len,
				      AsFormatKind format,
				      GError	 **error);
gboolean      as_metadata_parse_bytes (AsMetadata  *metad,
				       GBytes	   *bytes,
				       AsFormatKind format,
				       GError	  **error);

gboolean      as_metadata_parse_desktop_data (AsMetadata  *metad,
					      const gchar *cid,
					      const gchar *data,
					      gssize	   data_len,
					      GError	 **error);

GPtrArray    *as_metadata_parse_releases_bytes (AsMetadata *metad, GBytes *bytes, GError **error);
GPtrArray    *as_metadata_parse_releases_file (AsMetadata *metad, GFile *file, GError **error);
gchar	     *as_metadata_releases_to_data (AsMetadata *metad, GPtrArray *releases, GError **error);

AsComponent  *as_metadata_get_component (AsMetadata *metad);
GPtrArray    *as_metadata_get_components (AsMetadata *metad);

void	      as_metadata_clear_components (AsMetadata *metad);
void	      as_metadata_add_component (AsMetadata *metad, AsComponent *cpt);

gchar	*as_metadata_component_to_metainfo (AsMetadata *metad, AsFormatKind format, GError **error);
gboolean as_metadata_save_metainfo (AsMetadata	*metad,
				    const gchar *fname,
				    AsFormatKind format,
				    GError     **error);

gchar	*as_metadata_components_to_catalog (AsMetadata *metad, AsFormatKind format, GError **error);
gboolean as_metadata_save_catalog (AsMetadata  *metad,
				   const gchar *fname,
				   AsFormatKind format,
				   GError     **error);

AsFormatVersion as_metadata_get_format_version (AsMetadata *metad);
void		as_metadata_set_format_version (AsMetadata *metad, AsFormatVersion version);

AsFormatStyle	as_metadata_get_format_style (AsMetadata *metad);
void		as_metadata_set_format_style (AsMetadata *metad, AsFormatStyle mode);

void		as_metadata_set_locale (AsMetadata *metad, const gchar *locale);
const gchar    *as_metadata_get_locale (AsMetadata *metad);

const gchar    *as_metadata_get_origin (AsMetadata *metad);
void		as_metadata_set_origin (AsMetadata *metad, const gchar *origin);

gboolean	as_metadata_get_update_existing (AsMetadata *metad);
void		as_metadata_set_update_existing (AsMetadata *metad, gboolean update);

gboolean	as_metadata_get_write_header (AsMetadata *metad);
void		as_metadata_set_write_header (AsMetadata *metad, gboolean wheader);

const gchar    *as_metadata_get_media_baseurl (AsMetadata *metad);
void		as_metadata_set_media_baseurl (AsMetadata *metad, const gchar *url);

const gchar    *as_metadata_get_architecture (AsMetadata *metad);
void		as_metadata_set_architecture (AsMetadata *metad, const gchar *arch);

AsParseFlags	as_metadata_get_parse_flags (AsMetadata *metad);
void		as_metadata_set_parse_flags (AsMetadata *metad, AsParseFlags flags);

G_END_DECLS

#endif /* __AS_METADATA_H */
