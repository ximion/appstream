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

#ifndef __AS_MACROS_PRIVATE_H
#define __AS_MACROS_PRIVATE_H

#include <glib-object.h>

#include "as-macros.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

#define AS_BEGIN_PRIVATE_DECLS \
	G_BEGIN_DECLS          \
	_Pragma ("GCC visibility push(hidden)")

#define AS_END_PRIVATE_DECLS _Pragma ("GCC visibility pop") G_END_DECLS

#define AS_INTERNAL_VISIBLE __attribute__((visibility("default")))

/**
 * as_str_equal0:
 * Returns TRUE if strings are equal, ignoring NULL strings.
 * This is a convenience wrapper around g_strcmp0
 */
#define as_str_equal0(str1, str2) (g_strcmp0 ((gchar *) str1, (gchar *) str2) == 0)

/**
 * as_assign_string_safe:
 * @target: target variable variable to assign string to
 * @new_val: the value to set the target variable to
 *
 * Assigns @new_val to @target, freeing the previous content of
 * @target, unless both variables have been identical.
 *
 * This is useful in setter functions for class members, to ensure
 * we do not accidentally free a memory region that is still in use.
 */
#define as_assign_string_safe(target, new_val)                     \
	G_STMT_START                                               \
	{                                                          \
		if (G_LIKELY (!as_str_equal0 (target, new_val))) { \
			g_free (target);                           \
			target = g_strdup (new_val);               \
		}                                                  \
	}                                                          \
	G_STMT_END

/**
 * as_assign_ptr_array_safe:
 * @target: target variable variable to assign #GPtrArray to
 * @new_ptrarray: the value to set the target variable to
 *
 * Assigns @new_ptrarray to @target, decreasing the reference count of
 * @target, unless both variables are already identical.
 *
 * This is useful in setter functions for class members, to ensure
 * we do not accidentally free a memory region that is still in use.
 */
#define as_assign_ptr_array_safe(target, new_ptrarray)           \
	G_STMT_START                                             \
	{                                                        \
		if (G_LIKELY ((target) != (new_ptrarray))) {     \
			g_ptr_array_unref (target);              \
			target = g_ptr_array_ref (new_ptrarray); \
		}                                                \
	}                                                        \
	G_STMT_END

#define AS_PTR_ARRAY_SET_FREE_FUNC(array, func)                                       \
	G_STMT_START                                                                  \
	{                                                                             \
		if ((array) != NULL)                                                  \
			g_ptr_array_set_free_func ((array), (GDestroyNotify) (func)); \
	}                                                                             \
	G_STMT_END
#define AS_PTR_ARRAY_CLEAR_FREE_FUNC(array) AS_PTR_ARRAY_SET_FREE_FUNC (array, NULL)
#define AS_PTR_ARRAY_STEAL_FULL(arrptr)                   \
	({                                                \
		AS_PTR_ARRAY_CLEAR_FREE_FUNC (*(arrptr)); \
		g_steal_pointer ((arrptr));               \
	})
#define AS_PTR_ARRAY_RETURN_CLEAR_FREE_FUNC(array)         \
	G_STMT_START                                       \
	{                                                  \
		GPtrArray *_tmp_array = (array);           \
		AS_PTR_ARRAY_CLEAR_FREE_FUNC (_tmp_array); \
		return _tmp_array;                         \
	}                                                  \
	G_STMT_END

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_MACROS_PRIVATE_H */
