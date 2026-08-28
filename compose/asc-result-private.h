/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-macros-private.h"
#include "asc-result.h"

AS_BEGIN_PRIVATE_DECLS

gboolean asc_result_update_component_gcid (AscResult   *result,
					   AsComponent *cpt,
					   GBytes      *bytes,
					   GError     **error);
AS_INTERNAL_VISIBLE
gboolean asc_result_update_component_gcid_with_string (AscResult   *result,
						       AsComponent *cpt,
						       const gchar *data,
						       GError	  **error);

AS_END_PRIVATE_DECLS
