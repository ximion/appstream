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

#pragma once

#include <glib-object.h>
#include <appstream.h>

#include "as-settings-private.h"
#include "asc-result.h"
#include "asc-compose.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

AsComponent		*asc_parse_metainfo_data (AscResult *cres,
						  AsMetadata *mdata,
						  GBytes *bytes,
						  const gchar *mi_basename);
AsComponent		*asc_parse_metainfo_data_simple (AscResult *cres,
							 GBytes *bytes,
							 const gchar *mi_basename);

void			asc_validate_metainfo_data_for_component (AscResult *cres,
								  AsValidator *validator,
								  AsComponent *cpt,
								  GBytes *bytes,
								  const gchar *mi_basename);

AS_INTERNAL_VISIBLE
AsComponent		*asc_parse_desktop_entry_data (AscResult *cres,
							AsComponent *cpt,
							GBytes *bytes,
							const gchar *de_basename,
							gboolean ignore_nodisplay,
							AsFormatVersion fversion,
							AscTranslateDesktopTextFn de_l10n_fn,
							gpointer user_data);

#pragma GCC visibility pop
G_END_DECLS
