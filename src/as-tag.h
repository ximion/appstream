/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2018 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_TAG_H
#define __AS_TAG_H

#include <glib.h>

G_BEGIN_DECLS
#pragma GCC visibility push(hidden)

/**
 * AsTag:
 * @AS_TAG_UNKNOWN:			Type invalid or not known
 * @AS_TAG_COMPONENTS:			`components`
 * @AS_TAG_COMPONENT:			`component`
 * @AS_TAG_ID:				`id`
 * @AS_TAG_PKGNAME:			`pkgname`
 * @AS_TAG_NAME:			`name`
 * @AS_TAG_SUMMARY:			`summary`
 * @AS_TAG_DESCRIPTION:			`description`
 * @AS_TAG_URL:				`url`
 * @AS_TAG_ICON:			`icon`
 * @AS_TAG_CATEGORIES:			`categories`
 * @AS_TAG_CATEGORY:			`category`
 * @AS_TAG_KEYWORDS:			`keywords`
 * @AS_TAG_KEYWORD:			`keyword`
 * @AS_TAG_MIMETYPES:			`mimetypes`
 * @AS_TAG_MIMETYPE:			`mimetype`
 * @AS_TAG_PROJECT_GROUP:		`project_group`
 * @AS_TAG_PROJECT_LICENSE:		`project_license`
 * @AS_TAG_SCREENSHOT:			`screenshot`
 * @AS_TAG_SCREENSHOTS:			`screenshots`
 * @AS_TAG_UPDATE_CONTACT:		`update_contact`
 * @AS_TAG_IMAGE:			`image`
 * @AS_TAG_COMPULSORY_FOR_DESKTOP:	`compulsory_for_desktop`
 *
 * The tag type.
 **/
typedef enum {
	AS_TAG_UNKNOWN,
	AS_TAG_COMPONENTS,
	AS_TAG_COMPONENT,
	AS_TAG_ID,
	AS_TAG_PKGNAME,
	AS_TAG_NAME,
	AS_TAG_SUMMARY,
	AS_TAG_DESCRIPTION,
	AS_TAG_URL,
	AS_TAG_ICON,
	AS_TAG_CATEGORIES,
	AS_TAG_CATEGORY,
	AS_TAG_KEYWORDS,
	AS_TAG_KEYWORD,
	AS_TAG_MIMETYPES,
	AS_TAG_MIMETYPE,
	AS_TAG_PROJECT_GROUP,
	AS_TAG_PROJECT_LICENSE,
	AS_TAG_SCREENSHOT,
	AS_TAG_SCREENSHOTS,
	AS_TAG_UPDATE_CONTACT,
	AS_TAG_IMAGE,
	AS_TAG_COMPULSORY_FOR_DESKTOP,

	/*< private >*/
	AS_TAG_LAST
} AsTag;

AsTag			as_xml_tag_from_string (const gchar *tag);
const gchar		*as_xml_tag_to_string (AsTag tag);

#pragma GCC visibility pop
G_END_DECLS

#endif /* __AS_TAG_H */
