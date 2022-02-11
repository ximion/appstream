/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2021-2022 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AS_TYPE_FILE_MONITOR (as_file_monitor_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsFileMonitor, as_file_monitor, AS, FILE_MONITOR, GObject)

struct _AsFileMonitorClass
{
	GObjectClass		parent_class;
	void			(*added)	(AsFileMonitor	*monitor,
						 const gchar	*filename);
	void			(*removed)	(AsFileMonitor	*monitor,
						 const gchar	*filename);
	void			(*changed)	(AsFileMonitor	*monitor,
						 const gchar	*filename);
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
};

/**
 * AsFileMonitorError:
 * @AS_FILE_MONITOR_ERROR_FAILED:			Generic failure
 *
 * The error type.
 **/
typedef enum {
	AS_FILE_MONITOR_ERROR_FAILED,
	/*< private >*/
	AS_FILE_MONITOR_ERROR_LAST
} AsFileMonitorError;

#define	AS_FILE_MONITOR_ERROR			as_file_monitor_error_quark ()

AsFileMonitor	*as_file_monitor_new			(void);
GQuark		 as_file_monitor_error_quark		(void);

gboolean	 as_file_monitor_add_directory	(AsFileMonitor	*monitor,
						 const gchar	*filename,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 as_file_monitor_add_file	(AsFileMonitor	*monitor,
						 const gchar	*filename,
						 GCancellable	*cancellable,
						 GError		**error);

G_END_DECLS
