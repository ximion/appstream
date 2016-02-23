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

#ifndef __AS_YAMLDATA_H
#define __AS_YAMLDATA_H

#include <glib.h>
#include <gio/gio.h>

#include "as-component.h"

G_BEGIN_DECLS

#define AS_TYPE_YAMLDATA (as_yamldata_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsYAMLData, as_yamldata, AS, YAMLDATA, GObject)

struct _AsYAMLDataClass
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

AsYAMLData		*as_yamldata_new (void);

void			as_yamldata_initialize (AsYAMLData *ydt,
						const gchar *locale,
						const gchar *origin,
						const gchar *media_baseurl,
						gint priority);

GPtrArray		*as_yamldata_parse_distro_data (AsYAMLData *ydt,
							const gchar *data,
							GError **error);

const gchar		*as_yamldata_get_locale (AsYAMLData *ydt);
void			as_yamldata_set_locale (AsYAMLData *ydt,
						const gchar *locale);

G_END_DECLS

#endif /* __AS_YAMLDATA_H */
