/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2020 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_UTILS_H
#define __AS_UTILS_H

#include <glib-object.h>
#include "as-component.h"

G_BEGIN_DECLS

gchar			*as_markup_convert_simple (const gchar *markup,
						   GError **error);

gboolean		as_utils_locale_is_compatible (const gchar *locale1,
						       const gchar *locale2);
gboolean		as_utils_is_category_name (const gchar *category_name);
gboolean		as_utils_is_tld (const gchar *tld);
gboolean		as_utils_is_desktop_environment (const gchar *desktop);

void			as_utils_sort_components_into_categories (GPtrArray *cpts,
								  GPtrArray *categories,
								  gboolean check_duplicates);

gint			as_utils_compare_versions (const gchar* a,
						   const gchar *b);

gchar			*as_utils_build_data_id (AsComponentScope scope,
						 const gchar *origin,
						 AsBundleKind bundle_kind,
						 const gchar *cid);
gchar			*as_utils_data_id_get_cid (const gchar *data_id);

const gchar		*as_get_appstream_version (void);

G_END_DECLS

#endif /* __AS_UTILS_H */
