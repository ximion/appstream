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

#ifndef __AS_METADATAPRIVATE_H
#define __AS_METADATAPRIVATE_H

#include <glib-object.h>
#include <libxml/tree.h>

#include "as-metadata.h"

G_BEGIN_DECLS

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

AsComponent	*as_metadata_parse_component_node (AsMetadata* metad,
							xmlNode* node,
							gboolean allow_invalid,
							GError **error);
void		as_metadata_set_parser_mode (AsMetadata *metad,
						AsParserMode mode);
AsParserMode	as_metadata_get_parser_mode (AsMetadata *metad);

G_END_DECLS

#endif /* __AS_METADATAPRIVATE_H */
