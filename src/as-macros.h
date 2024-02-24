/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_MACROS_H
#define __AS_MACROS_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * SECTION:as-macros
 * @short_description: Generic helper macros.
 * @include: appstream.h
 */

/* convenience functions as it's easy to forget the bitwise operators */
#define as_flags_add(bitfield, flag)    \
	do {                            \
		((bitfield) |= (flag)); \
	} while (0)
#define as_flags_remove(bitfield, flag)  \
	do {                             \
		((bitfield) &= ~(flag)); \
	} while (0)
#define as_flags_invert(bitfield, flag) \
	do {                            \
		((bitfield) ^= flag);   \
	} while (0)
#define as_flags_contains(bitfield, flag) (((bitfield) &flag) > 0)

G_END_DECLS

#endif /* __AS_MACROS_H */
