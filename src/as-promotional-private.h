/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2024 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_PROMOTIONAL_PRIVATE_H
#define __AS_PROMOTIONAL_PRIVATE_H

#include "as-macros-private.h"
#include "as-promotional.h"
#include "as-xml.h"
#include "as-yaml.h"

AS_BEGIN_PRIVATE_DECLS

AS_INTERNAL_VISIBLE
void	 as_promotional_set_context_locale (AsPromotional *promotional, const gchar *locale);

gboolean as_promotional_load_from_xml (AsPromotional *promotional,
				       AsContext      *ctx,
				       xmlNode	      *node,
				       GError	     **error);
void	 as_promotional_to_xml_node (AsPromotional *promotional, AsContext *ctx, xmlNode *root);

gboolean as_promotional_load_from_yaml (AsPromotional  *promotional,
					AsContext      *ctx,
					struct fy_node *node,
					GError	      **error);
void	 as_promotional_emit_yaml (AsPromotional *promotional,
				   AsContext      *ctx,
				   struct fy_emitter *emitter);

gint	 as_promotional_get_position (AsPromotional *promotional);
void	 as_promotional_set_position (AsPromotional *promotional, gint pos);

AS_END_PRIVATE_DECLS

#endif /* __AS_PROMOTIONAL_PRIVATE_H */
