/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_COMPONENT_PRIVATE_H
#define __AS_COMPONENT_PRIVATE_H

#include <glib-object.h>
#include "as-component.h"
#include "as-utils-private.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

typedef guint16		AsTokenType; /* big enough for both bitshifts */

gint			as_component_get_priority (AsComponent *cpt);
void			as_component_set_priority (AsComponent *cpt,
							gint priority);

void			as_component_complete (AsComponent *cpt,
						gchar *scr_base_url,
						gchar **icon_paths);

GHashTable		*as_component_get_name_table (AsComponent *cpt);
GHashTable		*as_component_get_summary_table (AsComponent *cpt);
GHashTable		*as_component_get_description_table (AsComponent *cpt);
GHashTable		*as_component_get_developer_name_table (AsComponent *cpt);
GHashTable		*as_component_get_keywords_table (AsComponent *cpt);
GHashTable		*as_component_get_languages_table (AsComponent *cpt);

void			as_component_set_bundles_array (AsComponent *cpt,
							GPtrArray *bundles);

AS_INTERNAL_VISIBLE
GHashTable		*as_component_get_urls_table (AsComponent *cpt);

gboolean		as_component_has_package (AsComponent *cpt);
gboolean		as_component_has_install_candidate (AsComponent *cpt);

void			as_component_add_provided_item (AsComponent *cpt,
							AsProvidedKind kind,
							const gchar *item);

const gchar		*as_component_get_architecture (AsComponent *cpt);
void			 as_component_set_architecture (AsComponent *cpt,
							const gchar *arch);

void			 as_component_create_token_cache (AsComponent *cpt);
GHashTable		*as_component_get_token_cache_table (AsComponent *cpt);
void			 as_component_set_token_cache_valid (AsComponent *cpt,
							     gboolean valid);

guint			as_component_get_sort_score (AsComponent *cpt);
void			as_component_set_sort_score (AsComponent *cpt,
							guint score);

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_COMPONENT_PRIVATE_H */
