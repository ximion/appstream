/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <glib.h>
#include "as-validator-issue.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

typedef struct {
	GRefString	*tag;
	AsIssueSeverity	severity;
	GRefString	*explanation;
} AscHintTag;

AscHintTag	*asc_hint_tag_new (const gchar *tag,
				   AsIssueSeverity severity,
				   const gchar *explanation);
void		asc_hint_tag_free (AscHintTag *htag);

typedef struct {
	const gchar	*tag;
	AsIssueSeverity	severity;
	const gchar	*explanation;
} AscHintTagStatic;

extern AscHintTagStatic asc_hint_tag_list[];

#pragma GCC visibility pop
G_END_DECLS
