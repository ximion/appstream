/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_RELEASE_H
#define __AS_RELEASE_H

#include <glib-object.h>

#define AS_TYPE_RELEASE			(as_release_get_type())
#define AS_RELEASE(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_RELEASE, AsRelease))
#define AS_RELEASE_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_RELEASE, AsReleaseClass))
#define AS_IS_RELEASE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_RELEASE))
#define AS_IS_RELEASE_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_RELEASE))
#define AS_RELEASE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_RELEASE, AsReleaseClass))

G_BEGIN_DECLS

typedef struct _AsRelease	AsRelease;
typedef struct _AsReleaseClass	AsReleaseClass;

struct _AsRelease
{
	GObject			parent;
};

struct _AsReleaseClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

GType		 as_release_get_type (void);
AsRelease	*as_release_new (void);

const gchar	*as_release_get_version (AsRelease *release);
void		 as_release_set_version (AsRelease *release,
								const gchar *version);

guint64		 as_release_get_timestamp (AsRelease *release);
void		 as_release_set_timestamp (AsRelease *release,
								guint64 timestamp);

const gchar	*as_release_get_description (AsRelease *release);
void		 as_release_set_description (AsRelease *release,
								const gchar *description,
								const gchar *locale);

gchar		*as_release_get_active_locale (AsRelease *release);
void		as_release_set_active_locale (AsRelease	*release,
								const gchar *locale);

G_END_DECLS

#endif /* __AS_RELEASE_H */
