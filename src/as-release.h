/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_RELEASE_H
#define __AS_RELEASE_H

#include <glib-object.h>
#include "as-enums.h"
#include "as-artifact.h"
#include "as-issue.h"

G_BEGIN_DECLS

#define AS_TYPE_RELEASE (as_release_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsRelease, as_release, AS, RELEASE, GObject)

struct _AsReleaseClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * AsReleaseKind:
 * @AS_RELEASE_KIND_UNKNOWN:		Unknown release type
 * @AS_RELEASE_KIND_STABLE:		A stable release for end-users
 * @AS_RELEASE_KIND_DEVELOPMENT:	A development release or pre-release for testing
 *
 * The release size kind.
 *
 * Since: 0.12.0
 **/
typedef enum {
	AS_RELEASE_KIND_UNKNOWN,
	AS_RELEASE_KIND_STABLE,
	AS_RELEASE_KIND_DEVELOPMENT,
	/*< private >*/
	AS_RELEASE_KIND_LAST
} AsReleaseKind;

const gchar	*as_release_kind_to_string (AsReleaseKind kind);
AsReleaseKind	as_release_kind_from_string (const gchar *kind_str);

/**
 * AsReleaseUrlKind:
 * @AS_RELEASE_URL_KIND_UNKNOWN		Unknown release web URL type
 * @AS_RELEASE_URL_KIND_DETAILS:	Weblink to detailed release notes.
 *
 * The release URL kinds.
 *
 * Since: 0.12.5
 **/
typedef enum {
	AS_RELEASE_URL_KIND_UNKNOWN,
	AS_RELEASE_URL_KIND_DETAILS,
	/*< private >*/
	AS_RELEASE_URL_KIND_LAST
} AsReleaseUrlKind;

const gchar		*as_release_url_kind_to_string (AsReleaseUrlKind kind);
AsReleaseUrlKind	as_release_url_kind_from_string (const gchar *kind_str);

AsRelease		*as_release_new (void);

AsReleaseKind		as_release_get_kind (AsRelease *release);
void			as_release_set_kind (AsRelease *release,
						AsReleaseKind kind);

const gchar		*as_release_get_active_locale (AsRelease *release);
void			as_release_set_active_locale (AsRelease	*release,
							const gchar *locale);

const gchar		*as_release_get_version (AsRelease *release);
void			as_release_set_version (AsRelease *release,
						const gchar *version);

gint			as_release_vercmp (AsRelease *rel1,
					   AsRelease *rel2);

const gchar		*as_release_get_date (AsRelease *release);
void			as_release_set_date (AsRelease *release,
						const gchar *date);
guint64			as_release_get_timestamp (AsRelease *release);
void			as_release_set_timestamp (AsRelease *release,
							guint64 timestamp);

const gchar		*as_release_get_date_eol (AsRelease *release);
void			as_release_set_date_eol (AsRelease *release,
						 const gchar *date);
guint64			as_release_get_timestamp_eol (AsRelease *release);
void			as_release_set_timestamp_eol (AsRelease *release,
							guint64 timestamp);

const gchar		*as_release_get_description (AsRelease *release);
void			as_release_set_description (AsRelease *release,
							const gchar *description,
							const gchar *locale);

AsUrgencyKind		as_release_get_urgency (AsRelease *release);
void			as_release_set_urgency (AsRelease *release,
						AsUrgencyKind urgency);

GPtrArray		*as_release_get_issues (AsRelease *release);
void			as_release_add_issue (AsRelease *release,
						AsIssue *issue);

GPtrArray		*as_release_get_artifacts (AsRelease *release);
void			as_release_add_artifact (AsRelease *release,
					 AsArtifact *artifact);

const gchar		*as_release_get_url (AsRelease *release,
						AsReleaseUrlKind url_kind);
void			as_release_set_url (AsRelease *release,
					    AsReleaseUrlKind url_kind,
					    const gchar *url);

/* DEPRECATED */

G_DEPRECATED
guint64			as_release_get_size (AsRelease *release,
						AsSizeKind kind);
G_DEPRECATED
void			as_release_set_size (AsRelease *release,
						guint64 size,
						AsSizeKind kind);

G_DEPRECATED
GPtrArray		*as_release_get_locations (AsRelease *release);
G_DEPRECATED
void			as_release_add_location (AsRelease *release,
						 const gchar *location);

G_DEPRECATED
GPtrArray		*as_release_get_checksums (AsRelease *release);
G_DEPRECATED
AsChecksum		*as_release_get_checksum (AsRelease *release,
						  AsChecksumKind kind);
G_DEPRECATED
void			as_release_add_checksum (AsRelease *release,
						 AsChecksum *cs);

G_END_DECLS

#endif /* __AS_RELEASE_H */
