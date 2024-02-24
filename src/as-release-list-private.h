/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include "as-release-list.h"
#include "as-release-private.h"
#include "as-macros-private.h"
#include "as-tag.h"
#include "as-xml.h"
#include "as-yaml.h"

AS_BEGIN_PRIVATE_DECLS

gboolean as_release_list_load_from_xml (AsReleaseList *rels,
					AsContext     *ctx,
					xmlNode	      *node,
					GError	     **error);
void	 as_release_list_to_xml_node (AsReleaseList *rels, AsContext *ctx, xmlNode *root);

gboolean as_release_list_load_from_yaml (AsReleaseList *rels,
					 AsContext     *ctx,
					 GNode	       *root,
					 GError	      **error);
void	 as_release_list_emit_yaml (AsReleaseList *rels, AsContext *ctx, yaml_emitter_t *emitter);

gint	 as_release_compare (gconstpointer a, gconstpointer b);

gboolean as_release_list_load (AsReleaseList *rels,
			       AsComponent   *cpt,
			       gboolean	      reload,
			       gboolean	      allow_net,
			       GError	    **error);

AS_END_PRIVATE_DECLS
