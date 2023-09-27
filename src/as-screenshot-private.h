/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_SCREENSHOT_PRIVATE_H
#define __AS_SCREENSHOT_PRIVATE_H

#include "as-macros-private.h"
#include "as-screenshot.h"
#include "as-xml.h"
#include "as-yaml.h"

AS_BEGIN_PRIVATE_DECLS

AS_INTERNAL_VISIBLE
void	 as_screenshot_set_context_locale (AsScreenshot *screenshot, const gchar *locale);

gboolean as_screenshot_load_from_xml (AsScreenshot *screenshot,
				      AsContext	   *ctx,
				      xmlNode	   *node,
				      GError	  **error);
void	 as_screenshot_to_xml_node (AsScreenshot *screenshot, AsContext *ctx, xmlNode *root);

gboolean as_screenshot_load_from_yaml (AsScreenshot *screenshot,
				       AsContext    *ctx,
				       GNode	    *node,
				       GError	   **error);
void as_screenshot_emit_yaml (AsScreenshot *screenshot, AsContext *ctx, yaml_emitter_t *emitter);

gint as_screenshot_get_position (AsScreenshot *screenshot);
void as_screenshot_set_position (AsScreenshot *screenshot, gint pos);

AS_END_PRIVATE_DECLS

#endif /* __AS_SCREENSHOT_PRIVATE_H */
