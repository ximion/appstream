/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2021 Matthias Klumpp <matthias@tenstral.net>
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

	{ "x-dev-testsuite-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Dummy error hint for the testsuite. Var1: {{var1}}."
	},

	{ "x-dev-testsuite-info",
	  AS_ISSUE_SEVERITY_INFO,
	  "Dummy info hint for the testsuite. Var1: {{var1}}."
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
	  "Using one of the vetted permissive licenses is required to allow distributors to include the metadata in mixed data collections "
	  "without the risk of license violations due to mixing incompatible licenses."
	  "We only support a limited set of licenses that went through legal review. Refer to "
	  "<a href=\"https://www.freedesktop.org/software/appstream/docs/chap-Metadata.html#tag-metadata_license\">the specification documentation</a> "
	  "for information on how to make '{{license}}' a valid expression, or consider replacing the license with one of the recognized licenses directly."
	},

	{ "metainfo-unknown-type",
	  AS_ISSUE_SEVERITY_ERROR,
	  "The component has an unknown type. Please make sure this component type is mentioned in the specification, and that the"
	  "<code>type=</code> property of the component root-node in the MetaInfo XML file does not contain a spelling mistake."
	},

	{ "file-read-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Unable to read data from file {{fname}}: {{msg}}",
	},

	{ "desktop-file-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Unable to read data from .desktop file: {{msg}}",
	},

	{ "desktop-entry-bad-data",
	  AS_ISSUE_SEVERITY_WARNING,
	  "Error while reading some data from the desktop-entry file: {{hint}}",
	},

	{ "desktop-entry-category-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The category name {{hint}} is invalid. The software can not be shown in this category.",
	},

	{ "desktop-entry-value-invalid-chars",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The desktop-entry field value for {{hint}} contains invalid or non-printable UTF-8 characters, which can not be properly displayed.",
	},

	{ "desktop-file-hidden-set",
	  AS_ISSUE_SEVERITY_WARNING,
	  "This .desktop file has the 'Hidden' property set. This is wrong for vendor-installed .desktop files, and "
	  "nullifies all effects this .desktop file has (including MIME associations), which most certainly is not intentional. "
	  "See <a href=\"https://standards.freedesktop.org/desktop-entry-spec/latest/ar01s06.html\">the specification</a> for details."
	},

	{ "desktop-entry-value-quoted",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The desktop entry field value {{hint}} is quoted, which is likely unintentional."
	},

	{ "desktop-entry-empty-onlyshowin",
	  AS_ISSUE_SEVERITY_WARNING,
	  "This .desktop file has the 'OnlyShowIn' property set with an empty value. This might not be intended, as this will hide "
	  "the application from all desktops. If you do want to hide the application from all desktops, using 'NoDisplay=true' is more explicit. "
	  "See <a href=\"https://standards.freedesktop.org/desktop-entry-spec/latest/ar01s06.html\">the specification</a> for details."
	},

	{ "missing-launchable-desktop-file",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The MetaInfo file references a .desktop file with ID '{{desktop_id}}' in its <code>launchable</code> tag, but the file "
	  "was not found in the same source tree. In order to be able to launch the software once it was installed, please place the "
	  "MetaInfo file and its .desktop files in the same package."
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
