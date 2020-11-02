/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2020 Matthias Klumpp <matthias@tenstral.net>
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

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

typedef struct {
	const gchar	*tag;
	AsIssueSeverity	severity;
	const gchar	*explanation;
} AsValidatorIssueTag;

AscHintTag as_validator_issue_tag_list[] =  {
	{ "dev-test",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Placeholder."
	},

	{ NULL, AS_ISSUE_SEVERITY_UNKNOWN, NULL }
};

#pragma GCC visibility pop
G_END_DECLS
