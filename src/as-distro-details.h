/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_DISTRODETAILS_H
#define __AS_DISTRODETAILS_H

#include <glib-object.h>

#define AS_TYPE_DISTRO_DETAILS			(as_distro_details_get_type ())
#define AS_DISTRO_DETAILS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_DISTRO_DETAILS, AsDistroDetails))
#define AS_DISTRO_DETAILS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_DISTRO_DETAILS, AsDistroDetailsClass))
#define AS_IS_DISTRO_DETAILS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_DISTRO_DETAILS))
#define AS_IS_DISTRO_DETAILS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_DISTRO_DETAILS))
#define AS_DISTRO_DETAILS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_DISTRO_DETAILS, AsDistroDetailsClass))

G_BEGIN_DECLS

typedef struct _AsDistroDetails AsDistroDetails;
typedef struct _AsDistroDetailsClass AsDistroDetailsClass;
typedef struct _AsDistroDetailsPrivate AsDistroDetailsPrivate;

struct _AsDistroDetails
{
	GObject parent_instance;
	AsDistroDetailsPrivate * priv;
};

struct _AsDistroDetailsClass
{
	GObjectClass parent_class;
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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsDistroDetails, g_object_unref)

GType			as_distro_details_get_type (void) G_GNUC_CONST;

AsDistroDetails		*as_distro_details_new (void);
AsDistroDetails		*as_distro_details_construct (GType object_type);
gchar			*as_distro_details_config_distro_get_str (AsDistroDetails *self,
									const gchar *key);
const gchar		*as_distro_details_get_distro_id (AsDistroDetails *self);
gboolean		as_distro_details_config_distro_get_bool (AsDistroDetails *self,
									const gchar *key);
const gchar		*as_distro_details_get_distro_name (AsDistroDetails *self);
const gchar		*as_distro_details_get_distro_version (AsDistroDetails *self);

gchar			**as_distro_details_get_icon_repository_paths (void);

G_END_DECLS

#endif /* __AS_DISTRODETAILS_H */
