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
	  N_("The first `description/p` paragraph of this component might be too short (< 80 characters). "
	     "Please consider starting with a longer paragraph to improve how the description looks like in software centers "
	     "and to provide more detailed information on this component immediately in the first paragraph.")
	},

	{ "description-first-word-not-capitalized",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The description line does not start with a capitalized word, project name or number.")
	},

	{ "description-has-plaintext-url",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The description contains a web URL in plain text. This is not allowed, please use the <url/> tag instead to share links.")
	},

	{ "tag-not-translatable",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("This tag is not translatable.")
	},

	{ "tag-duplicated",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("This tag must only appear once in this context. Having multiple tags of this kind is not valid.")
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

	{ "cid-punctuation-prefix",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component ID starts with punctuation. This is not allowed.")
	},

	{ "cid-contains-hyphen",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The component ID contains a hyphen/minus. Using a hyphen is strongly discouraged, to improve interoperability with other tools such as D-Bus. "
	     "A good option is to replace any hyphens with underscores (`_`).")
	},

	{ "cid-has-number-prefix",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The component ID contains a segment starting with a number. Starting a segment of the reverse-DNS ID with a number is strongly discouraged, "
             "to keep interoperability with other tools such as D-Bus. Ideally, prefix these segments with an underscore.")
	},

	{ "cid-contains-uppercase-letter",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("The component ID should only contain lowercase characters.")
	},

	{ "cid-domain-not-lowercase",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The domain part of the rDNS component ID (first two parts) must only contain lowercase characters.")
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
	  N_("The license ID was not found in the SPDX database. "
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
	  N_("The update-contact does not appear to be a valid email address (escaping of `@` is only allowed as `_at_` or `_AT_`).")
	},

	{ "screenshot-image-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Unable to reach the screenshot image on its remote location - does the image exist?")
	},

	{ "screenshot-video-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Unable to reach the screenshot video on its remote location - does the video file exist?")
	},

	{ "screenshot-media-url-not-secure",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Consider using a secure (HTTPS) URL to reference this screenshot image or video.")
	},

	{ "screenshot-no-media",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
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
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The screenshot video does not specify which video codec was used in a `codec` property.")
	},

	{ "screenshot-video-container-missing",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The screenshot video does not specify which container format was used in a `container` property.")
	},

	{ "screenshot-video-codec-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The selected video codec is not supported by AppStream and software centers may not be able to play the video. "
	     "Only the AV1 and VP9 codecs are currently supported, using `av1` and `vp9` as values for the `codec` property.")
	},

	{ "screenshot-video-container-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The selected video container format is not supported by AppStream and software centers may not be able to play the video. "
	     "Only the WebM and Matroska video containers are currently supported, using `webm` and `mkv` as values for the `container` property.")
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
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("A `requires` or `recommends` item requires a value to denote a valid relation.")
	},

	{ "relation-item-has-version",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: `version` is an AppStream XML property. Please do not translate it. */
	  N_("Found `version` property on required/recommended item of a type that should not have or require a version.")
	},

	{ "relation-item-missing-compare",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: `version` and `compare` are AppStream XML properties. Please do not translate them. */
	  N_("Found `version` property on this required/recommended item, but not `compare` property. It is recommended to explicitly define a comparison operation.")
	},

	{ "relation-item-invalid-vercmp",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: `eq/ne/lt/gt/le/ge` are AppStream XML values. Please do not translate them. */
	  N_("Invalid comparison operation on relation item. Only one of `eq/ne/lt/gt/le/ge` is permitted.")
	},

	{ "relation-item-has-vercmp",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The relation item has a comparison operation set, but does not support any comparisons.")
	},

	{ "relation-item-redefined",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("This relation item has already been defined once for this or a different relation type. Please do not redefine relations.")
	},

	{ "relation-memory-in-requires",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Found a memory size relation in a `requires` tag. This means users will not be able to even install the component without having enough RAM. "
	     "This is usually not intended and you want to use `memory` in the `recommends` tag instead.")
	},

	{ "relation-control-in-requires",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Found a user input control relation in a `requires` tag. This means users will not be able to even install the component without having the "
	     "defined input control available on the system. This is usually not intended and you want to use `control` in the `recommends` tag instead.")
	},

	{ "relation-control-value-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `control` item defines an unknown input method and is invalid. Check the specification for a list of permitted values.")
	},

	{ "relation-display-length-value-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `display_length` item contains an invalid display length. Its value must either be a shorthand string, or positive integer value denoting logical pixels. "
	     "Please refer to the AppStream specification for more information on this tag.")
	},

	{ "relation-display-length-side-property-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `side` property of this `display_length` item contains an invalid value. It must either be `shortest` or `longest`, or unset to imply `shortest` to "
	     "make the item value refer to either the shortest or longest side of the display.")
	},

	{ "relation-hardware-value-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `hardware` item contains an invalid value. It should be a Computer Hardware ID (CHID) UUID without braces.")
	},

	{ "relation-memory-value-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("A `memory` item must only contain a non-zero integer value, depicting a system memory size in mebibyte (MiB)")
	},

	{ "relation-internet-value-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The set tag value is not valid for an `internet` relation.")
	},

	{ "relation-internet-bandwidth-value-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The value of this property must be a positive integer value, describing the minimum required bandwidth in mbit/s.")
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
	  N_("The component has a `merge` method defined. This is not allowed in metainfo files.")
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
	  N_("The <id/> tag still contains a `type` property, probably from an old conversion to the recent metainfo format.")
	},

	{ "multiple-pkgname",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("The `pkgname` tag appears multiple times. You should evaluate creating a metapackage containing the metainfo and .desktop files in order to avoid defining multiple package names per component.")
	},

	{ "name-has-dot-suffix",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("The component name should (likely) not end with a dot (`.`).")
	},

	{ "summary-has-dot-suffix",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The component summary should not end with a dot (`.`).")
	},

	{ "summary-has-tabs-or-linebreaks",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The component summary must not contain tabs or linebreaks.")
	},

	{ "summary-has-url",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The summary must not contain any URL. Use the `<url/>` tags for links.")
	},

	{ "summary-first-word-not-capitalized",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The summary text does not start with a capitalized word, project name or number.")
	},

	{ "icon-stock-cached-has-url",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Icons of type `stock` or `cached` must not contain an URL, a full or an relative path to the icon. "
	     "Only file basenames or stock names are allowed."),
	},

	{ "icon-remote-no-url",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Icons of type `remote` must contain an URL to the referenced icon."),
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
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Metainfo files may only contain icons of type `stock` or `remote`, the set type is not allowed."),
	},

	{ "url-invalid-type",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Invalid `type` property for this `url` tag. URLs of this type are not known in the AppStream specification."),
	},

	{ "url-not-reachable",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Unable to reach remote location that this URL references - does it exist?"),
	},

	{ "url-not-secure",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Consider using a secure (HTTPS) URL for this web link."),
	},

	{ "web-url-expected",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("A web URL was expected for this value."),
	},

	{ "url-uses-ftp",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("This web link uses the FTP protocol. Consider switching to HTTP(S) instead."),
	},

	{ "url-redefined",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("An URL of this type has already been defined."),
	},

	{ "developer-name-has-url",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The <developer_name/> can not contain a hyperlink."),
	},

	{ "unknown-desktop-id",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The set value is not an identifier for a desktop environment as registered with Freedesktop.org."),
	},

	{ "launchable-unknown-type",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `launchable` tag has an unknown type and can not be used."),
	},

	{ "bundle-unknown-type",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `bundle` tag has an unknown type and can not be used."),
	},

	{ "update-contact-in-collection-data",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The `update_contact` tag should not be included in collection AppStream XML."),
	},

	{ "nonstandard-gnome-extension",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("This tag is a GNOME-specific extension to AppStream and not part of the official specification. "
	     "Do not expect it to work in all implementations and in all software centers.")
	},

	{ "unknown-tag",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("Found invalid tag. Non-standard tags should be prefixed with `x-`. "
	     "AppStream also provides the <custom/> tag to add arbitrary custom data to metainfo files. This tag is read by AppStream libraries and may be useful "
	     "instead of defining new custom toplevel or `x-`-prefixed tags if you just want to add custom data to a metainfo file.")
	},

	{ "metadata-license-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The essential tag `metadata_license` is missing. A license for the metadata itself always has to be defined."),
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

	{ "desktop-app-launchable-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `desktop-application` component is missing a `desktop-id` launchable tag. "
	     "This means that this application can not be launched and has no association with its desktop-entry file. "
	     "It also means no icon data or category information from the desktop-entry file will be available, which "
	     "will result in this application being ignored entirely."),
	},

	{ "desktop-app-launchable-omitted",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `desktop-application` component has no `desktop-id` launchable tag, "
	     "however it contains all the necessary information to display the application. "
	     "The omission of the launchable entry means that this application can not be launched directly from "
	     "installers or software centers. If this is intended, this information can be ignored, otherwise "
	     "it is strongly recommended to add a launchable tag as well."),
	},

	{ "console-app-no-binary",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Type `console-application` component, but no information about binaries in $PATH was provided via a `provides/binary` tag."),
	},

	{ "web-app-no-url-launchable",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `web-application` component is missing a `launchable` tag of type `url`."),
	},

	{ "web-app-no-icon",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `web-application` component is missing a `icon` tag to specify a valid icon."),
	},

	{ "web-app-no-category",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `web-application` component is missing categorizations. A `categories` block is likely missing."),
	},

	{ "font-no-font-data",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Type `font` component, but no font information was provided via a `provides/font` tag."),
	},

	{ "driver-no-modalias",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Type `driver` component, but no modalias information was provided via a `provides/modalias` tag."),
	},

	{ "extends-not-allowed",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("An `extends` tag is specified, but the component is not of type `addon`, `localization` or `repository`."),
	},

	{ "addon-extends-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The component is an addon, but no `extends` tag was specified."),
	},

	{ "localization-extends-missing",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `localization` component is missing an `extends` tag, to specify the components it adds localization to."),
	},

	{ "localization-no-languages",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `localization` component does not define any languages this localization is for."),
	},

	{ "service-no-service-launchable",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This `service` component is missing a `launchable` tag of type `service`."),
	},

	{ "metainfo-suggestion-type-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Suggestions of any type other than `upstream` are not allowed in metainfo files."),
	},

	{ "category-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The category name is not valid. Refer to the XDG Menu Specification for a list of valid category names."),
	},

	{ "app-categories-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("This component is in no valid categories, even though it should be. Please check its metainfo file and desktop-entry file."),
	},

	{ "screenshot-caption-too-long",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("The screenshot caption is too long (should be <= 100 characters)"),
	},

	{ "file-read-failed",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("Unable to read file."),
	},

	{ "xml-markup-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The XML of this file is malformed."),
	},

	{ "component-collection-tag-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Invalid tag found in collection metadata. Only `component` tags are permitted."),
	},

	{ "metainfo-ancient",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
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

	{ "desktop-file-load-failed",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("Unable to load the desktop-entry file associated with this component."),
	},

	{ "desktop-file-not-found",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("This component metadata refers to a non-existing .desktop file."),
	},

	{ "desktop-entry-category-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("A category defined in the desktop-entry file is not valid. Refer to the XDG Menu Specification for a list of valid categories."),
	},

	{ "desktop-entry-bad-data",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("Error while reading some data from the desktop-entry file."),
	},

	{ "desktop-entry-value-invalid-chars",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The value of this desktop-entry field contains invalid or non-printable UTF-8 characters, which can not be displayed properly."),
	},

	{ "desktop-entry-value-quoted",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("This desktop-entry field value is quoted, which is likely unintentional."),
	},

	{ "desktop-entry-hidden-set",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("This desktop-entry file has the 'Hidden' property set. This is wrong for vendor-installed .desktop files, and "
	     "nullifies all effects this .desktop file has (including MIME associations), which most certainly is not intentional."),
	},

	{ "desktop-entry-empty-onlyshowin",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("This desktop-entry file has the 'OnlyShowIn' property set with an empty value. This might not be intended, as this will hide "
	     "the application from all desktops. If you do want to hide the application from all desktops, using 'NoDisplay=true' is more explicit."),
	},

	{ "dir-no-metadata-found",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("No AppStream metadata was found in this directory or directory tree."),
	},

	{ "dir-applications-not-found",
	  AS_ISSUE_SEVERITY_PEDANTIC, /* pedantic because not everything which has metadata is an application */
	  N_("No XDG applications directory found."),
	},

	{ "metainfo-legacy-path",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The metainfo file is stored in a legacy path. Please place it in `/usr/share/metainfo/`."),
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

	{ "release-urgency-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The value set as release urgency is not a known urgency value."),
	},

	{ "release-type-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The value set as release type is invalid."),
	},

	{ "release-version-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The release is missing the `version` property."),
	},

	{ "release-time-missing",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The release is missing either the `date` (preferred) or the `timestamp` property."),
	},

	{ "release-timestamp-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The release timestamp is invalid."),
	},

	{ "artifact-type-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The value set as artifact type is invalid. Must be either `source` or `binary`."),
	},

	{ "artifact-bundle-type-invalid",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The value set as artifact bundle type is invalid."),
	},

	{ "artifact-invalid-platform-triplet",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The platform triplet for this release is invalid. It must be in the form of "
	     "`architecture-oskernel-osenv` - refer to the AppStream documentation or information "
	     "on normalized GNU triplets for more information and valid fields."),
	},

	{ "artifact-checksum-type-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The selected checksumming algorithm is unsupported or unknown."),
	},

	{ "artifact-size-type-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The size type is unknown. Must be `download` or `installed`."),
	},

	{ "artifact-filename-not-basename",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The artifact filename must be a file basename, not a (relative or absolute) path."),
	},

	{ "release-issue-type-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The value set as release issue type is invalid."),
	},

	{ "release-issue-is-cve-but-no-cve-id",
	  AS_ISSUE_SEVERITY_WARNING,
	  N_("The issue is tagged at security vulnerability with a CVE number, but its value does not look like a valid CVE identifier."),
	},

	{ "releases-info-missing",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("This component is missing information about releases. Consider adding a `releases` tag to describe releases and their changes."),
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

	{ "runtime-project-license-no-ref",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("Licenses for `runtime` components are usually too complex to reflect them in a simple SPDX expression. Consider using a `LicenseRef` and a web URL "
	     "as value for this component's `project_license`. E.g. `LicenseRef-free=https://example.com/licenses.html`")
	},

	{ "runtime-no-provides",
	  AS_ISSUE_SEVERITY_PEDANTIC,
	  N_("Since a `runtime` component is comprised of multiple other software components, their component-IDs may be listed in a `<provides/>` section for this runtime.")
	},

	{ "unknown-provides-item-type",
	  AS_ISSUE_SEVERITY_INFO,
	  N_("The type of the item that the component provides is not known to AppStream."),
	},

	{ "mimetypes-tag-deprecated",
	  AS_ISSUE_SEVERITY_WARNING,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks). */
	  N_("The toplevel `mimetypes` tag is deprecated. Please use `mediatype` tags in a `provides` block instead "
	     "to indicate that your software provides a media handler for the given types."),
	},

	{ "content-rating-missing",
	  AS_ISSUE_SEVERITY_INFO,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks) and keep the URL intact. */
	  N_("This component has no `content_rating` tag to provide age rating information. "
	     "You can generate the tag data online by answering a few questions at https://hughsie.github.io/oars/"),
	},

	{ "component-tag-missing-namespace",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks) and keep the URL intact. */
	  N_("This `tag` is missing a `namespace` attribute."),
	},

	{ "component-tag-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  /* TRANSLATORS: Please do not translate AppStream tag and property names (in backticks) and keep the URL intact. */
	  N_("This tag or its namespace contains invalid characters. Only lower-cased ASCII letters, numbers, dots, hyphens and underscores are permitted."),
	},

	{ "branding-color-type-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The type of this color is not valid."),
	},

	{ "branding-color-scheme-type-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("The value of this color scheme preference is not valid."),
	},

	{ "branding-color-invalid",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("This color is not a valid HTML color code."),
	},

	{ "metainfo-localized-keywords-tag",
	  AS_ISSUE_SEVERITY_ERROR,
	  N_("A <keywords/> tag must not be localized in metainfo files (upstream metadata). "
	     "Localize the individual keyword entries instead.")
	},

	{ NULL, AS_ISSUE_SEVERITY_UNKNOWN, NULL }
};

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_VALIDATOR_ISSUE_TAG_H */
