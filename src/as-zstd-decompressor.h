/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>

#include "as-macros-private.h"

AS_BEGIN_PRIVATE_DECLS

#define AS_TYPE_ZSTD_DECOMPRESSOR (as_zstd_decompressor_get_type ())
G_DECLARE_FINAL_TYPE (AsZstdDecompressor, as_zstd_decompressor, AS, ZSTD_DECOMPRESSOR, GObject)

AsZstdDecompressor *as_zstd_decompressor_new (void);

AS_END_PRIVATE_DECLS
