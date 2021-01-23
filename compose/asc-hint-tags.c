/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:asc-hint-tags
 * @short_description: Issue hint tags definitions for appstream-compose.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-hint-tags.h"

#include "as-utils-private.h"

AscHintTagStatic asc_hint_tag_list[] =  {
	{ "x-dev-testsuite-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Dummy error hint for the testsuite. Var1: {var1}."
	},

	{ "x-dev-testsuite-info",
	  AS_ISSUE_SEVERITY_INFO,
	  "Dummy info hint for the testsuite. Var1: {var1}."
	},

	{ NULL, AS_ISSUE_SEVERITY_UNKNOWN, NULL }
};

/**
 * asc_hint_tag_new:
 *
 * Create a new #AscHintTag struct with the given values.
 */
AscHintTag*
asc_hint_tag_new (const gchar *tag, AsIssueSeverity severity, const gchar *explanation)
{
	AscHintTag *htag = g_new0 (AscHintTag, 1);
	htag->severity = severity;
	htag->tag = g_ref_string_new_intern (tag);
	htag->explanation = g_ref_string_new_intern (explanation);

	return htag;
}

/**
 * asc_hint_tag_free:
 *
 * Free a dynamically allocated hint tag struct.
 */
void
asc_hint_tag_free (AscHintTag *htag)
{
	as_ref_string_release (htag->tag);
	as_ref_string_release (htag->explanation);
	g_free (htag);
}
