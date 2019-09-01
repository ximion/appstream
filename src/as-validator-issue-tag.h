/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_VALIDATOR_ISSUE_TAG_H
#define __AS_VALIDATOR_ISSUE_TAG_H

#include <glib.h>
#include <glib/gi18n.h>

#include "as-validator-issue.h"

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

typedef struct {
	const gchar	*tag;
	AsIssueSeverity	severity;
	const gchar	*explanation;
} AsValidatorIssueTag;

AsValidatorIssueTag as_validator_issue_tag_list[] =  {
	{ "type-property-required",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("This tag requires a type property.")
	},

	{ "invalid-child-tag-name",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Tags of this name are not permitted in this section.")
	},

	{ "metainfo-localized-description-tag",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("A <description/> tag must not be localized in metainfo files (upstream metadata). "
	     "Localize the individual paragraphs instead.")
	},

	{ "collection-localized-description-section",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("This element (paragraph, list, etc.) of a <description/> tag must not be localized individually in collection metadata. "
	     "Localize the whole <description/> tag instead. The AppStream collection metadata generator (e.g. `appstream-generator`) will already do the right thing when compiling the data.")
	},

	{ "description-markup-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("AppStream descriptions support only a limited set of tags to format text: Paragraphs (<p/>) and lists (<ul/>, <ol/>). "
	     "This description markup contains an invalid XML tag that would not be rendered correctly in applications supporting the metainfo specification.")
	},

	{ "description-para-markup-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("This description paragraph contains invalid markup. Currently, only <em/> and <code/> are permitted.")
	},

	{ "description-enum-item-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("Enumerations must only have list items (<li/>) as children.")
	},

	{ "description-first-para-too-short",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The first 'description/p' paragraph of this component might be too short (< 80 characters). "
	     "Please consider starting with a longer paragraph to improve how the description looks like in software centers "
	     "and to provide more detailed information on this component immediately in the first paragraph.")
	},

	{ "description-has-plaintext-url",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The description contains a web URL in plain text. This is not allowed, please use the <url/> tag instead to share links.")
	},

	{ "tag-duplicated",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("As per AppStream specification, the mentioned tag must only appear once in this context. Having multiple tags of this kind is not valid.")
	},

	{ "tag-empty",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The mentioned tag is empty, which is highly likely not intended as it should have content.")
	},

	{ "cid-is-not-rdns",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component ID is required to follow a reverse domain-name scheme for its name. See the AppStream specification for details.")
	},

	{ "cid-desktopapp-is-not-rdns",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The component ID is not a reverse domain-name. Please update the ID to avoid future issues and be compatible with all AppStream implementations.\n"
	     "You may also consider to update the name of the accompanying .desktop file to follow the latest version of the Desktop-Entry specification and use "
	     "a rDNS name for it as well. In any case, do not forget to mention the new desktop-entry in a <launchable/> tag for this component to keep the application "
	     "launchable from software centers and the .desktop file data associated with the metainfo data.")
	},

	{ "cid-maybe-not-rdns",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The component ID might not follow the reverse domain-name schema (the TLD used by it is not known to the validator).")
	},

	{ "cid-invalid-character",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component ID contains an invalid character. Only ASCII characters, dots and numbers are permitted.")
	},

	{ "cid-contains-hyphen",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The component ID contains a hyphen/minus. Using a hyphen is strongly discouraged, to improve interoperability with other tools such as D-Bus. "
	     "A good option is to replace any hyphens with underscores ('_').")
	},

	{ "cid-has-number-prefix",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The component ID contains a segment starting with a number. Starting a segment of the reverse-DNS ID with a number is strongly discouraged, "
             "to keep interoperability with other tools such as D-Bus. Ideally, prefix these segments with an underscore.")
	},

	{ "cid-missing-affiliation-freedesktop",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The component is part of the Freedesktop project, but its ID does not start with fd.o's reverse-DNS name (\"org.freedesktop\").")
	},

	{ "cid-missing-affiliation-kde",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The component is part of the KDE project, but its ID does not start with KDEs reverse-DNS name (\"org.kde\").")
	},

	{ "cid-missing-affiliation-gnome",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The component is part of the GNOME project, but its ID does not start with GNOMEs reverse-DNS name (\"org.gnome\").")
	},

	{ "spdx-expression-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The SPDX license expression is invalid and could not be parsed.")
	},

	{ "spdx-license-unknown",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The license ID is not found in the SPDX database. "
	     "Please check that the license ID is written in an SPDX-conformant way and is a valid free software license.")
	},

	{ "metadata-license-too-complex",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The metadata itself seems to be licensed under a complex collection of licenses. Please license the data under a simple permissive license, like FSFAP, MIT or CC0-1.0 "
	     "to allow distributors to include it in mixed data collections without the risk of license violations due to mutually incompatible licenses.")
	},

	{ "metadata-license-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The metadata itself does not seem to be licensed under a permissive license. Please license the data under a permissive license, like FSFAP, CC0-1.0 or 0BSD "
	     "to allow distributors to include it in mixed data collections without the risk of license violations due to mutually incompatible licenses.")
	},

	{ "update-contact-no-mail",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The update-contact does not appear to be a valid email address (escaping of '@' is only allowed as '_at_' or '_AT_').")
	},

	{ "screenshot-image-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Unable to reach the screenshot image on its remote location - does the image exist?")
	},

	{ "screenshot-video-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Unable to reach the screenshot video on its remote location - does the video file exist?")
	},

	{ "screenshot-media-url-insecure",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Consider using a secure (HTTPS) URL to reference this screenshot image or video.")
	},

	{ "screenshot-no-media",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("A screenshot must contain at least one image or video in order to be useful. Please add an <image/> to it.")
	},

	{ "screenshot-mixed-images-videos",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("A screenshot must contain either images or videos, but not both at the same time. Please use this screenshot exclusively "
	     "for either static images or for videos.")
	},

	{ "screenshot-no-caption",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("The screenshot does not have a caption text. Consider adding one.")
	},

	{ "screenshot-video-codec-missing",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("The screenshot video does not specify which video codec was used in a 'codec' property.")
	},

	{ "screenshot-video-container-missing",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("The screenshot video does not specify which container format was used in a 'container' property.")
	},

	{ "screenshot-video-codec-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("The selected video codec is not supported by AppStream and software centers may not be able to play the video. "
	     "Only the AV1 and VP9 codecs are currently supported, using 'av1' and 'vp9' as values for the 'codec' property.")
	},

	{ "screenshot-video-container-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("The selected video container format is not supported by AppStream and software centers may not be able to play the video. "
	     "Only the WebM and Matroska video containers are currently supported, using 'webm' and 'mkv' as values for the 'container' property.")
	},

	{ "screenshot-video-file-wrong-container",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("For videos, only the WebM and Matroska (.mkv) container formats are currently supported. The file extension of the referenced "
	     "video does not belong to either of these formats.")
	},

	{ "screenshot-default-contains-video",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The default screenshot of a software component must not be a video. Use a static image as default screenshot and set the video as a secondary screenshot.")
	},

	{ "relation-invalid-tag",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Found an unknown tag in a requires/recommends group. This is likely an error, because a component relation of this type is unknown.")
	},

	{ "relation-item-no-value",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("A 'requires' or 'recommends' item requires a value to denote a valid relation.")
	},

	{ "relation-item-has-version",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: 'version' is an AppStream XML property. Please do not translate it. */
	  N_("Found 'version' property on required/recommended item of a type that should not have or require a version.")
	},

	{ "relation-item-missing-compare",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: 'version' and 'compare' are AppStream XML properties. Please do not translate them. */
	  N_("Found 'version' property on this required/recommended item, but not 'compare' property. It is recommended to explicitly define a comparison operation.")
	},

	{ "relation-item-invalid-vercmp",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("Invalid version comparison operation on relation item. Only eq/ne/lt/gt/le/ge are permitted.")
	},

	{ "relation-memory-in-requires",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Found a memory size dependency in a 'requires' tag. This means users will not be able to even install the component without having enough RAM. "
	     "This is usually not intended and you want to use 'memory' in the 'recommends' tag instead.")
	},

	{ "component-type-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The set component type is not a recognized, valid AppStream component type.")
	},

	{ "component-priority-in-metainfo",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The component has a priority value set. This is not allowed in metainfo files.")
	},

	{ "component-merge-in-metainfo",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The component has a 'merge' method defined. This is not allowed in metainfo files.")
	},

	{ "component-id-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component is missing an ID (<id/> tag).")
	},

	{ "component-name-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component is missing a name (<name/> tag).")
	},

	{ "component-summary-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component is missing a summary (<summary/> tag).")
	},

	{ "id-tag-has-type",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The <id/> tag still contains a 'type' property, probably from an old conversion to the recent metainfo format.")
	},

	{ "multiple-pkgname",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("The 'pkgname' tag appears multiple times. You should evaluate creating a metapackage containing the metainfo and .desktop files in order to avoid defining multiple package names per component.")
	},

	{ "name-has-dot-suffix",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("The component name should (likely) not end with a dot ('.').")
	},

	{ "summary-has-dot-suffix",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The component summary should not end with a dot ('.').")
	},

	{ "summary-has-tabs-or-linebreaks",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component summary must not contain tabs or linebreaks.")
	},

	{ "summary-has-url",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The summary must not contain any URL. Use the <url/> tags for links.")
	},

	{ "icon-stock-cached-has-url",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Icons of type 'stock' or 'cached' must not contain an URL, a full or an relative path to the icon. "
	     "Only file basenames or stock names are allowed."),
	},

	{ "icon-remote-no-url",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Icons of type 'remote' must contain an URL to the referenced icon."),
	},

	{ "icon-remote-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Unable to reach remote icon at the given web location - does it exist?"),
	},

	{ "icon-remote-not-secure",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Consider using a secure (HTTPS) URL for the remote icon link."),
	},

	{ "metainfo-invalid-icon-type",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Metainfo files may only contain icons of type 'stock' or 'remote', the set type is not allowed."),
	},

	{ "url-invalid-type",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Invalid 'type' property for this 'url' tag. URLs of this type are not known in the AppStream specification."),
	},

	{ "url-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Unable to reach remote location that this URL references - does it exist?"),
	},

	{ "url-not-secure",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Consider using a secure (HTTPS) URL for this web link."),
	},

	{ "developer-name-has-url",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The <developer_name/> can not contain a hyperlink."),
	},

	{ "unknown-desktop-id",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The given values is not an identifier for a desktop environment as registered with Freedesktop.org."),
	},

	{ "launchable-unknown-type",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("This 'launchable' tag has an unknown type and can not be used."),
	},

	{ "bundle-unknown-type",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("This 'bundle' tag has an unknown type and can not be used."),
	},

	{ "update-contact-in-collection-data",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("The 'update_contact' tag should not be included in collection AppStream XML."),
	},

	{ "nonstandard-gnome-extension",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("This tag is a GNOME-specific extension to AppStream and not part of the official specification. "
	     "Do not expect it to work in all implementations and in all software centers.")
	},

	{ "unknown-tag",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Found invalid tag. Non-standard tags should be prefixed with 'x-'. "
	     "AppStream also provides the <custom/> tag to add arbitrary custom data to metainfo files. This tag is read by AppStream libraries and may be useful "
	     "instead of defining new custom toplevel or 'x-'-prefixed tags if you just want to add custom data to a metainfo file.")
	},

	{ "metadata-license-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("The essential tag 'metadata_license' is missing. A license for the metadata itself always has to be defined."),
	},

	{ "app-description-required",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component is missing a long description. Components of this type must have a long description."),
	},

	{ "font-description-missing",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("It would be useful to add a long description to this font to present it better to users."),
	},

	{ "driver-firmware-description-missing",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("It is recommended to add a long description to this component to present it better to users."),
	},

	{ "generic-description-missing",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("This generic component is missing a long description. It may be useful to add one."),
	},

	{ "console-app-no-binary",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Type 'console-application' component, but no information about binaries in $PATH was provided via a 'provides/binary' tag."),
	},

	{ "web-app-no-url-launchable",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("This 'web-application' component is missing a 'launchable' tag of type 'url'."),
	},

	{ "web-app-no-icon",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("This 'web-application' component is missing a 'icon' tag to specify a valid icon."),
	},

	{ "web-app-no-category",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("This 'web-application' component is missing categorizations. A 'categories' block is likely missing."),
	},

	{ "font-no-font-data",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Type 'font' component, but no font information was provided via a 'provides/font' tag."),
	},

	{ "driver-no-modalias",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Type 'driver' component, but no modalias information was provided via a provides/modalias tag."),
	},

	{ "extends-not-allowed",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("An 'extends' tag is specified, but the component is not of type 'addon', 'localization' or 'repository'."),
	},

	{ "addon-extends-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("The component is an addon, but no 'extends' tag was specified."),
	},

	{ "localization-extends-missing",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("This 'localization' component is missing an 'extends' tag, to specify the components it adds localization to."),
	},

	{ "localization-no-languages",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("This 'localization' component does not define any languages this localization is for."),
	},

	{ "service-no-service-launchable",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("This 'service' component is missing a 'launchable' tag of type 'service'."),
	},

	{ "metainfo-suggestion-type-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Suggestions of any type other than 'upstream' are not allowed in metainfo files."),
	},

	{ "category-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The category name is not valid. Refer to the XDG Menu Specification for a list of valid category names."),
	},

	{ "screenshot-caption-too-long",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("The screenshot caption is too long (should be <= 80 characters)"),
	},

	{ "file-read-failed",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("Unable to read file."),
	},

	{ "xml-markup-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The XML of this file is malformed."),
	},

	{ "curl-not-found",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Unable to find the curl binary. remote URLs can not be checked for validity!"),
	},

	{ "component-collection-tag-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("Invalid tag found in collection metadata. Only 'component' tags are permitted."),
	},

	{ "metainfo-ancient",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names. */
	  N_("The metainfo file uses an ancient version of the AppStream specification, which can not be validated. Please migrate it to version 0.6 (or higher)."),
	},

	{ "root-tag-unknown",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("This XML document has an unknown root tag. Maybe this file is not a metainfo document?"),
	},

	{ "metainfo-filename-cid-mismatch",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The metainfo filename does not match the component ID."),
	},

	{ "desktop-file-read-failed",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("Unable to read the .desktop file associated with this component."),
	},

	{ "desktop-file-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("This component metadata refers to a non-existing .desktop file."),
	},

	{ "desktop-file-category-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The category defined in the .desktop file is not valid. Refer to the XDG Menu Specification for a list of valid categories."),
	},

	{ "dir-no-metadata.found",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("No AppStream metadata was found in this directory or directory tree."),
	},

	{ "dir-applications-not.found",
	  AS_ISSUE_SEVERITY_PEDANTIC, /* pedantic because not everything which has metadata is an application */
	  N_("No XDG applications directory found."),
	},

	{ "metainfo-legacy-path",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The metainfo file is stored in a legacy path. Please place it in '/usr/share/metainfo'."),
	},

	{ "metainfo-multiple-components",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The metainfo file specifies multiple components. This is not allowed."),
	},

	{ "releases-not-in-order",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The releases are not sorted in a latest to oldest version order. "
	     "This is required as some tools will assume that the latest version is always at the top. "
	     "Sorting releases also increases overall readability of the metainfo file."),
	},

	{ "invalid-iso8601-date",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The AppStream specification requires a complete, ISO 8601 date string with at least day-granularity to denote dates. "
	     "Please ensure the date string is valid."),
	},

	{ "circular-component-relation",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("This component extends, provides, requires or recommends itself, which is certainly not intended and may confuse users or machines dealing "
	     "with this metadata."),
	},

	{ "unknown-provides-item-type",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The type of the item that the component provides is not known to AppStream."),
	},

	{ NULL, AS_ISSUE_SEVERITY_UNKNOWN, NULL }
};

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_VALIDATOR_ISSUE_TAG_H */
