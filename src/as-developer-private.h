/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-developer.h"
#include "as-xml.h"
#include "as-yaml.h"
#include "as-macros-private.h"

AS_BEGIN_PRIVATE_DECLS

AsDeveloper *as_developer_new_with_context (AsContext *ctx);

AsContext   *as_developer_get_context (AsDeveloper *devp);
void	     as_developer_set_context (AsDeveloper *devp, AsContext *context);

GHashTable  *as_developer_get_name_table (AsDeveloper *devp);

gboolean     as_developer_load_from_xml (AsDeveloper *devp,
					 AsContext   *ctx,
					 xmlNode     *node,
					 GError	    **error);
void	     as_developer_to_xml_node (AsDeveloper *devp, AsContext *ctx, xmlNode *root);

gboolean     as_developer_load_from_yaml (AsDeveloper *devp,
					  AsContext   *ctx,
					  GNode	      *node,
					  GError     **error);
void	     as_developer_emit_yaml (AsDeveloper *devp, AsContext *ctx, yaml_emitter_t *emitter);

AS_END_PRIVATE_DECLS
