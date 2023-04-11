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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_ENUMS_H
#define __AS_ENUMS_H

#include <glib-object.h>

G_BEGIN_DECLS

/* convenience functions as it's easy to forget the bitwise operators */
#define as_flags_add(bitfield,enum)		do { ((bitfield) |= (enum)); } while (0)
#define as_flags_remove(bitfield,enum)		do { ((bitfield) &= ~(enum)); } while (0)
#define as_flags_invert(bitfield,enum)		do { ((bitfield) ^= enum); } while (0)
#define as_flags_contains(bitfield,enum)	(((bitfield) & enum) > 0)

/**
 * AsFormatStyle:
 * @AS_FORMAT_STYLE_UNKNOWN:	The format style is unknown.
 * @AS_FORMAT_STYLE_METAINFO:	Parse AppStream upstream metadata (metainfo files)
 * @AS_FORMAT_STYLE_CATALOG:	Parse AppStream metadata catalog (shipped by software distributors)
 *
 * There are a few differences between AppStream's metainfo files (shipped by upstream projects)
 * and the catalog metadata (shipped by distributors).
 * The data source kind indicates which style we should process.
 * Usually you do not want to set this explicitly.
 **/
typedef enum {
	AS_FORMAT_STYLE_UNKNOWN,
	AS_FORMAT_STYLE_METAINFO,
	AS_FORMAT_STYLE_CATALOG,
	/* DEPRECATED */
	AS_FORMAT_STYLE_COLLECTION = AS_FORMAT_STYLE_CATALOG,
	/*< private >*/
	AS_FORMAT_STYLE_LAST
} AsFormatStyle;

/**
 * AsFormatKind:
 * @AS_FORMAT_KIND_UNKNOWN:		Unknown metadata format.
 * @AS_FORMAT_KIND_XML:			AppStream XML metadata.
 * @AS_FORMAT_KIND_YAML:		AppStream YAML (DEP-11) metadata.
 * @AS_FORMAT_KIND_DESKTOP_ENTRY:	XDG Desktop Entry data.
 *
 * Format of the AppStream metadata.
 **/
typedef enum {
	AS_FORMAT_KIND_UNKNOWN,
	AS_FORMAT_KIND_XML,
	AS_FORMAT_KIND_YAML,
	AS_FORMAT_KIND_DESKTOP_ENTRY,
	/*< private >*/
	AS_FORMAT_KIND_LAST
} AsFormatKind;

/**
 * AsUrlKind:
 * @AS_URL_KIND_UNKNOWN:	Type invalid or not known
 * @AS_URL_KIND_HOMEPAGE:	Project homepage
 * @AS_URL_KIND_BUGTRACKER:	Bugtracker
 * @AS_URL_KIND_FAQ:		FAQ page
 * @AS_URL_KIND_HELP:		Help manual
 * @AS_URL_KIND_DONATION:	Page with information about how to donate to the project
 * @AS_URL_KIND_TRANSLATE:	Page with instructions on how to translate the project / submit translations.
 * @AS_URL_KIND_CONTACT:	Contact the developers
 * @AS_URL_KIND_VCS_BROWSER:	Browse the source code
 * @AS_URL_KIND_CONTRIBUTE:	Help developing
 *
 * The URL type.
 **/
typedef enum {
	AS_URL_KIND_UNKNOWN,
	AS_URL_KIND_HOMEPAGE,
	AS_URL_KIND_BUGTRACKER,
	AS_URL_KIND_FAQ,
	AS_URL_KIND_HELP,
	AS_URL_KIND_DONATION,
	AS_URL_KIND_TRANSLATE,
	AS_URL_KIND_CONTACT,
	AS_URL_KIND_VCS_BROWSER,
	AS_URL_KIND_CONTRIBUTE,
	/*< private >*/
	AS_URL_KIND_LAST
} AsUrlKind;

/**
 * AsUrgencyKind:
 * @AS_URGENCY_KIND_UNKNOWN:	Urgency is unknown or not set
 * @AS_URGENCY_KIND_LOW:	Low urgency
 * @AS_URGENCY_KIND_MEDIUM:	Medium urgency
 * @AS_URGENCY_KIND_HIGH:	High urgency
 * @AS_URGENCY_KIND_CRITICAL:	Critical urgency
 *
 * The urgency of an #AsRelease
 **/
typedef enum {
	AS_URGENCY_KIND_UNKNOWN,
	AS_URGENCY_KIND_LOW,
	AS_URGENCY_KIND_MEDIUM,
	AS_URGENCY_KIND_HIGH,
	AS_URGENCY_KIND_CRITICAL,
	/*< private >*/
	AS_URGENCY_KIND_LAST
} AsUrgencyKind;

const gchar		*as_url_kind_to_string (AsUrlKind url_kind);
AsUrlKind		 as_url_kind_from_string (const gchar *url_kind);

const gchar		*as_format_kind_to_string (AsFormatKind kind);
AsFormatKind		 as_format_kind_from_string (const gchar *kind_str);

const gchar		*as_urgency_kind_to_string (AsUrgencyKind urgency_kind);
AsUrgencyKind		 as_urgency_kind_from_string (const gchar *urgency_kind);

G_END_DECLS

#endif /* __AS_ENUMS_H */
