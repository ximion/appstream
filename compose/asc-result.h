/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (__APPSTREAM_COMPOSE_H) && !defined (ASC_COMPILATION)
#error "Only <appstream-compose.h> can be included directly."
#endif
#pragma once

#include <glib-object.h>
#include <appstream.h>

G_BEGIN_DECLS

#define ASC_TYPE_RESULT (asc_result_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscResult, asc_result, ASC, RESULT, GObject)

struct _AscResultClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
};

AscResult		*asc_result_new (void);

gboolean		asc_result_unit_ignored (AscResult *result);
guint			asc_result_components_count (AscResult *result);
guint			asc_result_hints_count (AscResult *result);

AsBundleKind		asc_result_get_bundle_kind (AscResult *result);
void			asc_result_set_bundle_kind (AscResult *result,
						    AsBundleKind kind);

const gchar		*asc_result_get_bundle_id (AscResult *result);
void			asc_result_set_bundle_id (AscResult *result,
						  const gchar *id);

AsComponent		*asc_result_get_component (AscResult *result,
						   const gchar *cid);
GPtrArray		*asc_result_fetch_components (AscResult *result);
GPtrArray		*asc_result_get_hints (AscResult *result,
					       const gchar *cid);

gboolean		asc_result_update_component_gcid (AscResult *result,
							  AsComponent *cpt,
							  const gchar *data);

gboolean		asc_result_add_component (AscResult *result,
						  AsComponent *cpt,
						  const gchar *data,
						  GError **error);
gboolean		asc_result_remove_component (AscResult *result,
						     AsComponent *cpt);
gboolean		asc_result_remove_component_by_id (AscResult *result,
							   const gchar *cid);

gboolean		asc_result_add_hint_by_cid (AscResult *result,
						    const gchar *component_id,
						    const gchar *tag,
						    const gchar *key1,
						    ...) G_GNUC_NULL_TERMINATED;
gboolean		asc_result_add_hint_by_cid_v (AscResult *result,
						      const gchar *component_id,
						      const gchar *tag,
						      gchar **kv);

gboolean		asc_result_add_hint (AscResult *result,
					     AsComponent *cpt,
					     const gchar *tag,
					     const gchar *key1,
					     ...) G_GNUC_NULL_TERMINATED;
gboolean		asc_result_add_hint_v (AscResult *result,
					       AsComponent *cpt,
					       const gchar *tag,
					       gchar **kv);

G_END_DECLS
