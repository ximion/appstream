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

#ifndef __AS_COMPONENTPRIVATE_H
#define __AS_COMPONENTPRIVATE_H

#include <glib-object.h>
#include "as-component.h"

G_BEGIN_DECLS

int			as_component_get_priority (AsComponent *cpt);
void			as_component_set_priority (AsComponent *cpt,
							int priority);

GHashTable		*as_component_get_languages_map (AsComponent *cpt);

void			as_component_complete (AsComponent *cpt,
						gchar *scr_base_url,
						gchar **icon_paths);

GHashTable		*as_component_get_name_table (AsComponent *cpt);
GHashTable		*as_component_get_summary_table (AsComponent *cpt);
GHashTable		*as_component_get_description_table (AsComponent *cpt);
GHashTable		*as_component_get_developer_name_table (AsComponent *cpt);
GHashTable		*as_component_get_keywords_table (AsComponent *cpt);
void			as_component_set_bundles_table (AsComponent *cpt,
							GHashTable *bundles);

gboolean		as_component_has_bundle (AsComponent *cpt);
gboolean		as_component_has_package (AsComponent *cpt);
gboolean		as_component_has_install_candidate (AsComponent *cpt);

G_END_DECLS

#endif /* __AS_COMPONENTPRIVATE_H */
