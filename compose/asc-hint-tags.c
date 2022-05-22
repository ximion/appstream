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
	  "The given tag was unknown. Please file an issue against AppStream."
	},

	{ "internal-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "A fatal problem appeared in appstream-compose. Please file an issue against AppStream.<br/>Error: {{msg}}"
	},

	{ "x-dev-testsuite-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Dummy error hint for the testsuite. Var1: {{var1}}."
	},

	{ "x-dev-testsuite-info",
	  AS_ISSUE_SEVERITY_INFO,
	  "Dummy info hint for the testsuite. Var1: {{var1}}."
	},

	{ "unit-read-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Error while reading data from unit <code>{{name}}</code>: {{msg}}",
	},

	{ "ancient-metadata",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The AppStream metadata should be updated to follow a more recent version of the specification.<br/>"
	  "Please consult <a href=\"http://freedesktop.org/software/appstream/docs/chap-Quickstart.html\">the XML quickstart guide</a> for "
	  "more information."
	},

	{ "metainfo-parsing-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Unable to parse AppStream MetaInfo file <code>{{fname}}</code>, the data is likely malformed.<br/>Error: {{error}}"
	},

	{ "metainfo-no-id",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Could not determine an ID for the component in <code>{{fname}}</code>. The AppStream MetaInfo file likely lacks an <code>&lt;id/&gt;</code> tag.<br/>"
          "The identifier tag is essential for AppStream metadata, and must not be missing."
	},

	{ "metainfo-no-name",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Component has no name specified. Ensure that the AppStream MetaInfo file or the .desktop file (if there is any) specify a component name."
	},

	{ "metainfo-no-summary",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Component does not contain a short summary. Ensure that the components MetaInfo file has a <code>summary</code> tag, or that its .desktop file "
	  "has a <code>Comment=</code> field set.<br/>"
	  "More information can be found in the <a href=\"http://standards.freedesktop.org/desktop-entry-spec/latest/ar01s05.html\">Desktop Entry specification</a> "
	  "and the <a href=\"https://www.freedesktop.org/software/appstream/docs/sect-Metadata-Application.html#tag-dapp-summary\">MetaInfo specification</a>."
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
	  "Unable to read data from file <code>{{fname}}</code>: {{msg}}",
	},

	{ "desktop-file-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Unable to read data from .desktop file: {{msg}}",
	},

	{ "desktop-entry-hidden-set",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The desktop-entry file `{{location}}` has the 'Hidden' property set. This is wrong for vendor-installed .desktop files, and "
	  "nullifies all effects this .desktop file has (including MIME associations), which most certainly is not intentional. "
	  "See <a href=\"https://standards.freedesktop.org/desktop-entry-spec/latest/ar01s06.html\">the specification</a> for details."
	},

	{ "desktop-entry-empty-onlyshowin",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The desktop-entry file `{{location}}` has the 'OnlyShowIn' property set with an empty value. This might not be intended, as this will hide "
	  "the application from all desktops. If you do want to hide the application from all desktops, using 'NoDisplay=true' is more explicit. "
	  "See <a href=\"https://standards.freedesktop.org/desktop-entry-spec/latest/ar01s06.html\">the specification</a> for details."
	},

	{ "missing-launchable-desktop-file",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The MetaInfo file references a .desktop file with ID '{{desktop_id}}' in its <code>launchable</code> tag, but the file "
	  "was not found in the same source tree. In order to be able to launch the software once it was installed, please place the "
	  "MetaInfo file and its .desktop files in the same package."
	},

	{ "translation-status-error",
	  AS_ISSUE_SEVERITY_WARNING,
	  "Unable to read translation status data: {{msg}}",
	},

	{ "translations-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  "Unable to add languages information, even though a <code>translation</code> tag was present in the MetaInfo file. "
	  "Please check that its value is set correctly, and all locale files are placed in the right directories "
	  "(e.g. <code>/usr/share/locale/*/LC_MESSAGES/</code> for Gettext .mo files)."
	},

	{ "icon-not-found",
	  AS_ISSUE_SEVERITY_ERROR,
	  "The icon <em>{{icon_fname}}</em> was not found in the archive. This issue can have multiple reasons, "
	  "like the icon being in a wrong directory or not being available in a suitable size (at least 64x64px). "
	  "To make the icon easier to find, place it in <code>/usr/share/icons/hicolor/&lt;size&gt;/apps</code> and ensure the <code>Icon=</code> value"
	  "of the desktop-entry file is set correctly."
	},

	{ "no-stock-icon",
	  AS_ISSUE_SEVERITY_ERROR,
	  "The component has no stock icon set, even though it requires one (or a `local` icon) to be valid."
	},

	{ "icon-write-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Unable to store icon <code>{{fname}}</code>: {{msg}}"
	},

	{ "duplicate-component",
	  AS_ISSUE_SEVERITY_ERROR,
	  "A component with this ID already exists. AppStream IDs must be unique, any subsequent components "
	  "have been ignored. Please resolve the ID conflict!"
	},

	{ "metainfo-screenshot-but-no-media",
	  AS_ISSUE_SEVERITY_WARNING,
	  "A screenshot has been found for this component, but apparently it does not have any images or videos defined. "
	  "The screenshot entry has been ignored."
	},

	{ "screenshot-download-error",
	  AS_ISSUE_SEVERITY_WARNING,
	  "Error while downloading screenshot from '{{url}}': {{error}}<br/>"
          "This might be a temporary server issue, or the screenshot is no longer available."
	},

	{ "screenshot-save-error",
	  AS_ISSUE_SEVERITY_WARNING,
	  "Unable to store screenshot for '{{url}}': {{error}}"
	},

	{ "screenshot-no-thumbnails",
	  AS_ISSUE_SEVERITY_INFO,
	  "No thumbnails have been generated for screenshot '{{url}}'.<br/>"
	  "This could mean that the original provided screenshot is too small to generate thumbnails from."
	},

	{ "screenshot-video-check-failed",
	  AS_ISSUE_SEVERITY_WARNING,
	  "Unable to inspect video file '{{fname}}'. This may have been caused by a configuration or network issue, or the supplied video file was faulty. "
	  "The error message was: {{msg}}"
	},

	{ "screenshot-video-has-audio",
	  AS_ISSUE_SEVERITY_INFO,
	  "The video '{{fname}}' contains an audio track. The audio may not be played by software centers, so ideally you should avoid using audio, "
	  "or at least make the audio non-essential for understanding the screencast."
	},

	{ "screenshot-video-audio-codec-unsupported",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The video '{{fname}}' contains an audio track using the '{{codec}}' codec. The only permitted audio codec is <a href=\"https://opus-codec.org/\">Opus</a>."
	},

	{ "screenshot-video-format-unsupported",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The video codec '{{codec}}' or container '{{container}}' of '{{fname}}' are not supported. Please encode the video "
	  "as VP9 or AV1 using the WebM or Matroska container."
	},

	{ "screenshot-video-too-big",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The video '{{fname}}' exceeds the maximum allowed file size of {{max_size}} (its size is {{size}}). Please try to make a shorter screencast."
	},

	{ "screenshot-image-too-big",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The image '{{fname}}' exceeds the maximum allowed file size of {{max_size}} (its size is {{size}}). Please create a smaller screenshot image."
	},

	{ "font-load-error",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Unable to load font '{{fname}}' from unit '{{unit_name}}: {{error}}"
	},

	{ "font-metainfo-but-no-font",
	  AS_ISSUE_SEVERITY_ERROR,
	  "A MetaInfo file with component-type <code>font</code> was found, but we could not find any matching font file (TrueType or OpenType) in the package.<br/> "
	  "This can mean that the <code>&lt;provides&gt; - &lt;font&gt;</code> tags contain wrong values that we could not map to the actual fonts, or that the package simply contained no fonts at all.<br/> "
	  "Fonts in this package: <em>{{font_names}}</em>"
	},

	{ "font-render-error",
	  AS_ISSUE_SEVERITY_WARNING,
	  "Unable to render image for font '{{name}}': {{error}}"
	},

	{ "gui-app-without-icon",
	  AS_ISSUE_SEVERITY_ERROR,
	  "The component is a GUI application (application which has a .desktop file for the XDG menu and <code>Type=Application</code>), "
	  "but we could not find a matching icon for this application."
	},

	{ "web-app-without-icon",
	  AS_ISSUE_SEVERITY_ERROR,
	  "The component is a GUI web application, but it either has no icon set in its MetaInfo file, "
	  "or we could not find a matching icon for this application."
	},

	{ "font-without-icon",
	  AS_ISSUE_SEVERITY_WARNING,
	  "The component is a font, but somehow we failed to automatically generate an icon for it, and no custom icon was set explicitly. "
	  "Is there a font file in the analyzed package, and does the MetaInfo file set the right font name to look for?"
	},

	{ "os-without-icon",
	  AS_ISSUE_SEVERITY_INFO,
	  "The component is an operating system, but no icon was found for it. Setting an icon would improve the look of this component in GUIs."
	},

	{ "no-valid-category",
	  AS_ISSUE_SEVERITY_ERROR,
	  "This software component is no member of any valid category."
	},

	{ "description-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Software components of type '{{kind}}' require a long description, and we were unable to find one. Please add one via a MetaInfo file."
	},

	{ "no-metainfo",
	  AS_ISSUE_SEVERITY_WARNING,
	  "This software component is missing a <a href=\"https://freedesktop.org/software/appstream/docs/chap-Metadata.html#sect-Metadata-GenericComponent\">MetaInfo file</a> "
	  "as metadata source.<br/>"
	  "To synthesize suitable metadata anyway, we took some data from its desktop-entry file.<br/>"
	  "This has many disadvantages, like low-quality and incomplete metadata. Therefore clients may ignore this component entirely due to poor metadata.<br/>"
	  "Additionally, a lot of software from desktop-entry files should either not be installable and searchable via the software catalog "
	  "(like desktop-specific settings applications) or be tagged accordingly via MetaInfo files.<br/>"
	  "Please consider to either hide this .desktop file from AppStream by adding a <code>X-AppStream-Ignore=true</code> field to it, or to write a MetaInfo file for this component.<br/>"
	  "You can consult the <a href=\"http://freedesktop.org/software/appstream/docs/chap-Quickstart.html\">MetaInfo quickstart guides</a> for more information "
	  "on how to write a MetaInfo file, or file a bug with the upstream author of this software component."
	},

	{ "filters-but-no-output",
	  AS_ISSUE_SEVERITY_ERROR,
	  "Component filters were set, but no output was generated at all. Likely none of the filtered components were found, "
	  "try to relax the filters and ensure the input data is valid."
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
