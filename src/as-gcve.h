/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_GCVE_H
#define __AS_GCVE_H

#include <glib.h>
#include "as-macros-private.h"

AS_BEGIN_PRIVATE_DECLS

AS_INTERNAL_VISIBLE
gboolean      as_is_gcve_id (const gchar *gcve_id);
const GRegex *as_gcve_id_word_regex (void);

AS_INTERNAL_VISIBLE
gchar *as_gcve_id_from_cve (const gchar *cve_id);
AS_INTERNAL_VISIBLE
gchar *as_gcve_id_to_cve (const gchar *gcve_id);

AS_INTERNAL_VISIBLE
gchar *as_get_gcve_url (const gchar *id);
AS_INTERNAL_VISIBLE
gchar *as_get_gcve_json_url (const gchar *id);

AS_END_PRIVATE_DECLS

#endif /* __AS_GCVE_H */
