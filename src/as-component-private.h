/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_COMPONENTPRIVATE_H
#define __AS_COMPONENTPRIVATE_H

#include <glib-object.h>
#include "as-component.h"
#include <libxml/tree.h>

G_BEGIN_DECLS

gchar				*as_component_dump_screenshot_data_xml (AsComponent *cpt);
void				as_component_load_screenshots_from_internal_xml (AsComponent *cpt,
																	 const gchar* xmldata);

gchar				*as_component_dump_releases_data_xml (AsComponent *cpt);
void				as_component_load_releases_from_internal_xml (AsComponent *cpt,
																  const gchar* xmldata);

int					as_component_get_priority (AsComponent *cpt);
void				as_component_set_priority (AsComponent *cpt,
											   int priority);

GHashTable			*as_component_get_languages_map (AsComponent *self);

void				as_component_complete (AsComponent *cpt,
										   gchar *scr_base_url,
										   gchar **icon_paths);

void				as_component_xml_add_screenshot_subnodes (AsComponent *cpt,
														xmlNode *root);
void				as_component_xml_add_release_subnodes (AsComponent *cpt,
														xmlNode *root);

G_END_DECLS

#endif /* __AS_COMPONENTPRIVATE_H */
