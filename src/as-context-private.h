/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_CONTEXT_PRIVATE_H
#define __AS_CONTEXT_PRIVATE_H

#include "as-context.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

const gchar		*as_context_get_architecture (AsContext *ctx);
void			as_context_set_architecture (AsContext *ctx,
						     const gchar *value);

gboolean		as_context_get_internal_mode (AsContext *ctx);
void			as_context_set_internal_mode (AsContext *ctx,
						      gboolean enabled);

const gchar		*as_context_localized_ht_get (AsContext *ctx,
						      GHashTable *lht,
						      const gchar *locale_override,
						      AsValueFlags value_flags);
void			as_context_localized_ht_set (AsContext *ctx,
						     GHashTable *lht,
						     const gchar *value,
						     const gchar *locale);

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_CONTEXT_PRIVATE_H */
