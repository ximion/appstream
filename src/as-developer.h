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

#if !defined(__APPSTREAM_H) && !defined(AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_DEVELOPER (as_developer_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsDeveloper, as_developer, AS, DEVELOPER, GObject)

struct _AsDeveloperClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

AsDeveloper *as_developer_new (void);

const gchar *as_developer_get_id (AsDeveloper *devp);
void	     as_developer_set_id (AsDeveloper *devp, const gchar *id);

const gchar *as_developer_get_name (AsDeveloper *devp);
void	     as_developer_set_name (AsDeveloper *devp, const gchar *value, const gchar *locale);

G_END_DECLS
