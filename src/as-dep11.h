/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_DEP11_H
#define __AS_DEP11_H

#include <glib.h>
#include <gio/gio.h>

#include "as-component.h"

#define AS_TYPE_DEP11			(as_dep11_get_type())
#define AS_DEP11(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_DEP11, AsDEP11))
#define AS_DEP11_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_DEP11, AsDEP11Class))
#define AS_IS_DEP11(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_DEP11))
#define AS_IS_DEP11_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_DEP11))
#define AS_DEP11_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_DEP11, AsDEP11Class))

G_BEGIN_DECLS

typedef struct _AsDEP11	AsDEP11;
typedef struct _AsDEP11Class	AsDEP11Class;

struct _AsDEP11
{
	GObject			parent;
};

struct _AsDEP11Class
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

GType			as_dep11_get_type (void) G_GNUC_CONST;

AsDEP11*		as_dep11_new (void);

const gchar		*as_dep11_get_locale (AsDEP11 *dep11);
void			as_dep11_set_locale (AsDEP11 *dep11,
									const gchar *locale);

void			as_dep11_parse_data (AsDEP11 *dep11,
									const gchar *data,
									GError **error);
void			as_dep11_parse_file (AsDEP11 *dep11,
									GFile* file,
									GError **error);

GPtrArray		*as_dep11_get_components (AsDEP11 *dep11);
void			as_dep11_clear_components (AsDEP11 *dep11);

G_END_DECLS

#endif /* __AS_DEP11_H */
