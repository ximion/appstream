/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_LAUNCH_H
#define __AS_LAUNCH_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_LAUNCH (as_launch_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsLaunch, as_launch, AS, LAUNCH, GObject)

struct _AsLaunchClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * AsLaunchKind:
 * @AS_LAUNCH_KIND_UNKNOWN:		Unknown kind
 * @AS_LAUNCH_KIND_DESKTOP_ID:		Launch by desktop-id
 *
 * Type of launch system the entries belong to.
 **/
typedef enum  {
	AS_LAUNCH_KIND_UNKNOWN,
	AS_LAUNCH_KIND_DESKTOP_ID,
	/*< private >*/
	AS_LAUNCH_KIND_LAST
} AsLaunchKind;

const gchar		*as_launch_kind_to_string (AsLaunchKind kind);
AsLaunchKind		as_launch_kind_from_string (const gchar *kind_str);

AsLaunch		*as_launch_new (void);

AsLaunchKind		as_launch_get_kind (AsLaunch *launch);
void			as_launch_set_kind (AsLaunch *launch,
						AsLaunchKind kind);

GPtrArray		*as_launch_get_entries (AsLaunch *launch);
void			as_launch_add_entry (AsLaunch *launch,
						const gchar *entry);

G_END_DECLS

#endif /* __AS_LAUNCH_H */
