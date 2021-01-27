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
	{ "internal-unknown-tag",
	  AS_ISSUE_SEVERITY_ERROR,
	  "The given tag was unknown. This is a bug."
	},

	{ "ancient-metadata",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The AppStream metadata should be updated to follow a more recent version of the specification.<br/>"
	  "Please consult <a href=\"http://freedesktop.org/software/appstream/docs/chap-Quickstart.html\">the XML quickstart guide</a> for "
	  "more information."
	},

	{ "metainfo-parsing-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Unable to parse AppStream MetaInfo file `{{fname}}`, the data is likely malformed.<br/>Error: {{error}}"
	},

	{ "metainfo-no-id",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Could not determine an ID for the component in '{{fname}}'. The AppStream MetaInfo file likely lacks an <code>&lt;id/&gt;</code> tag.<br/>"
          "The identifier tag is essential for AppStream metadata, and must not be missing."
	},

	{ "metainfo-license-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  "The MetaInfo file does not seem to be licensed under a permissive license that is in the allowed set for AppStream metadata. "
	  "Valid permissive licenses include FSFAP, CC0-1.0 or MIT. "
	  "Using one of the supported permissive licenses is required to allow distributors to include the metadata in mixed data collections "
	  "without the risk of license violations due to mixing incompatible licenses."
	  "We only support a limited set of licenses that went through legal review. If you think this message is an error and '{{license}}' "
	  "should actually be supported, please <a href=\"https://github.com/ximion/appstream/issues\">file a bug against AppStream</a>."
	},

	{ "metainfo-unknown-type",
	  AS_ISSUE_SEVERITY_ERROR,
	  "The component has an unknown type. Please make sure this component type is mentioned in the specification, and that the"
	  "<code>type=</code> property of the component root-node in the MetaInfo XML file does not contain a spelling mistake."
	},

	{ "x-dev-testsuite-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Dummy error hint for the testsuite. Var1: {{var1}}."
	},

	{ "x-dev-testsuite-info",
	  AS_ISSUE_SEVERITY_INFO,
	  "Dummy info hint for the testsuite. Var1: {{var1}}."
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
