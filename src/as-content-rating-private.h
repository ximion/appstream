/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017-2024 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_CONTENT_RATING_PRIVATE_H
#define __AS_CONTENT_RATING_PRIVATE_H

#include <glib-object.h>
#include "as-content-rating.h"
#include "as-xml.h"
#include "as-yaml.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

typedef enum {
	AS_OARS_VERSION_UNKNOWN,
	AS_OARS_VERSION_1_0,
	AS_OARS_VERSION_1_1
} AsOarsVersion;

gboolean as_content_rating_load_from_xml (AsContentRating *content_rating,
					  AsContext	  *ctx,
					  xmlNode	  *node,
					  GError	 **error);
void as_content_rating_to_xml_node (AsContentRating *content_rating, AsContext *ctx, xmlNode *root);

gboolean	       as_content_rating_load_from_yaml (AsContentRating *content_rating,
							 AsContext	 *ctx,
							 GNode		 *node,
							 GError		**error);
void		       as_content_rating_emit_yaml (AsContentRating *content_rating,
						    AsContext	    *ctx,
						    yaml_emitter_t  *emitter);

gboolean	       as_is_oars_key (const gchar *id, AsOarsVersion version);

AsOarsVersion	       as_oars_version_from_string (const gchar *value);

gboolean	       as_content_rating_id_is_valid (const gchar *id, AsContentRatingValue value);

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_CONTENT_RATING_PRIVATE_H */
