/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_METADATA_H
#define __AS_METADATA_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "as-component.h"

G_BEGIN_DECLS

#define AS_TYPE_METADATA (as_metadata_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsMetadata, as_metadata, AS, METADATA, GObject)

struct _AsMetadataClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * AsParserMode:
 * @AS_PARSER_MODE_UPSTREAM:	Parse Appstream upstream metadata
 * @AS_PARSER_MODE_DISTRO:	Parse Appstream distribution metadata
 *
 * There are a few differences between Appstream's upstream metadata
 * and the distribution metadata.
 * The parser mode indicates which style we should process.
 * Usually you don't want to change this.
 **/
typedef enum {
	AS_PARSER_MODE_UPSTREAM,
	AS_PARSER_MODE_DISTRO,
	/*< private >*/
	AS_PARSER_MODE_LAST
} AsParserMode;

/**
 * AsMetadataError:
 * @AS_METADATA_ERROR_FAILED:			Generic failure
 * @AS_METADATA_ERROR_UNEXPECTED_FORMAT_KIND:	Expected upstream metadata but got distro metadata, or vice versa.
 *
 * A metadata processing error.
 **/
typedef enum {
	AS_METADATA_ERROR_FAILED,
	AS_METADATA_ERROR_UNEXPECTED_FORMAT_KIND,
	/*< private >*/
	AS_METADATA_ERROR_LAST
} AsMetadataError;

#define	AS_METADATA_ERROR	as_metadata_error_quark ()

AsMetadata		*as_metadata_new (void);
GQuark			as_metadata_error_quark (void);

void			as_metadata_parse_file (AsMetadata *metad,
							GFile *file,
							GError **error);
void			as_metadata_parse_data (AsMetadata *metad,
							const gchar *data,
							GError **error);

AsComponent		*as_metadata_get_component (AsMetadata *metad);
GPtrArray		*as_metadata_get_components (AsMetadata *metad);

void			as_metadata_clear_components (AsMetadata *metad);
void			as_metadata_add_component (AsMetadata *metad,
							AsComponent *cpt);

gchar			*as_metadata_component_to_upstream_xml (AsMetadata *metad);
gchar			*as_metadata_components_to_distro_xml (AsMetadata *metad);

void			as_metadata_save_upstream_xml (AsMetadata *metad,
							const gchar *fname,
							GError **error);
void			as_metadata_save_distro_xml (AsMetadata *metad,
							const gchar *fname,
							GError **error);

void			as_metadata_set_locale (AsMetadata *metad,
							const gchar *locale);
const gchar		*as_metadata_get_locale (AsMetadata *metad);

const gchar		*as_metadata_get_origin (AsMetadata *metad);
void			as_metadata_set_origin (AsMetadata *metad,
							const gchar *origin);

void			as_metadata_set_parser_mode (AsMetadata *metad,
							AsParserMode mode);
AsParserMode		as_metadata_get_parser_mode (AsMetadata *metad);

G_END_DECLS

#endif /* __AS_METADATA_H */
