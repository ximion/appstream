/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2021 Matthias Klumpp <matthias@tenstral.net>
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

#define ASC_TYPE_UNIT (asc_unit_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscUnit, asc_unit, ASC, UNIT, GObject)

struct _AscUnitClass
{
	GObjectClass parent_class;

	gboolean (*open) (AscUnit *unit,
			  GError **error);
	void (*close) (AscUnit *unit);
	GBytes *(*read_data) (AscUnit *unit,
			      const gchar *filename,
			      GError **error);

	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
};

AscUnit			*asc_unit_new (void);

AsBundleKind		asc_unit_get_bundle_kind (AscUnit *unit);
void			asc_unit_set_bundle_kind (AscUnit *unit,
						    AsBundleKind kind);

const gchar		*asc_unit_get_bundle_id (AscUnit *unit);
void			asc_unit_set_bundle_id (AscUnit *unit,
						  const gchar *id);

GPtrArray		*asc_unit_get_contents (AscUnit *unit);
void			asc_unit_set_contents (AscUnit *unit,
					       GPtrArray *contents);

gboolean		asc_unit_open (AscUnit *unit,
					GError **error);
void			asc_unit_close (AscUnit *unit);

GBytes			*asc_unit_read_data (AscUnit *unit,
					     const gchar *filename,
					     GError **error);

G_END_DECLS
