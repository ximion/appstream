/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include "asc-unit.h"

G_BEGIN_DECLS

#define ASC_TYPE_DIRECTORY_UNIT (asc_directory_unit_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscDirectoryUnit, asc_directory_unit, ASC, DIRECTORY_UNIT, AscUnit)

struct _AscDirectoryUnitClass
{
	AscUnitClass parent_class;

	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
};

AscDirectoryUnit	*asc_directory_unit_new (const gchar *root_dir);

const gchar		*asc_directory_unit_get_root (AscDirectoryUnit *dirunit);
void			asc_directory_unit_set_root (AscDirectoryUnit *dirunit,
						     const gchar *root_dir);

G_END_DECLS
