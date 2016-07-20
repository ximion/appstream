/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_ENUMS_H
#define __AS_ENUMS_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * AsUrlKind:
 * @AS_URL_KIND_UNKNOWN:	Type invalid or not known
 * @AS_URL_KIND_HOMEPAGE:	Project homepage
 * @AS_URL_KIND_BUGTRACKER:	Bugtracker
 * @AS_URL_KIND_FAQ:		FAQ page
 * @AS_URL_KIND_HELP:		Help manual
 * @AS_URL_KIND_DONATION:	Page with information about how to donate to the project
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
	/*< private >*/
	AS_URL_KIND_LAST
} AsUrlKind;

const gchar	*as_url_kind_to_string (AsUrlKind url_kind);
AsUrlKind	as_url_kind_from_string (const gchar *url_kind);

/**
 * AsBundleKind:
 * @AS_BUNDLE_KIND_UNKNOWN:	Type invalid or not known
 * @AS_BUNDLE_KIND_LIMBA:	A Limba bundle
 * @AS_BUNDLE_KIND_FLATPAK:	A Flatpak bundle
 *
 * The bundle type.
 **/
typedef enum {
	AS_BUNDLE_KIND_UNKNOWN,
	AS_BUNDLE_KIND_LIMBA,
	AS_BUNDLE_KIND_FLATPAK,
	/*< private >*/
	AS_BUNDLE_KIND_LAST
} AsBundleKind;

#define AS_BUNDLE_KIND_XDG_APP AS_BUNDLE_KIND_FLATPAK

const gchar	*as_bundle_kind_to_string (AsBundleKind bundle_kind);
AsBundleKind	as_bundle_kind_from_string (const gchar *bundle_kind);

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

const gchar	*as_urgency_kind_to_string (AsUrgencyKind urgency_kind);
AsUrgencyKind	 as_urgency_kind_from_string (const gchar *urgency_kind);

G_END_DECLS

#endif /* __AS_ENUMS_H */
