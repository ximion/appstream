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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_ARTIFACT_H
#define __AS_ARTIFACT_H

#include <glib-object.h>
#include "as-checksum.h"
#include "as-bundle.h"

G_BEGIN_DECLS

#define AS_TYPE_ARTIFACT (as_artifact_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsArtifact, as_artifact, AS, ARTIFACT, GObject)

struct _AsArtifactClass
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
 * AsSizeKind:
 * @AS_SIZE_KIND_UNKNOWN:	Unknown size
 * @AS_SIZE_KIND_DOWNLOAD:	Size of download of component
 * @AS_SIZE_KIND_INSTALLED:	Size of installed component
 *
 * The artifact size kind.
 *
 * Since: 0.8.6
 **/
typedef enum {
	AS_SIZE_KIND_UNKNOWN,
	AS_SIZE_KIND_DOWNLOAD,
	AS_SIZE_KIND_INSTALLED,
	/*< private >*/
	AS_SIZE_KIND_LAST
} AsSizeKind;

const gchar	*as_size_kind_to_string (AsSizeKind size_kind);
AsSizeKind	as_size_kind_from_string (const gchar *size_kind);

/**
 * AsArtifactKind:
 * @AS_ARTIFACT_KIND_UNKNOWN:	Type invalid or not known
 * @AS_ARTIFACT_KIND_SOURCE:	The artifact describes software sources.
 * @AS_ARTIFACT_KIND_BINARY:	The artifact describes a binary distribution of the component.
 *
 * The artifact type.
 **/
typedef enum {
	AS_ARTIFACT_KIND_UNKNOWN,
	AS_ARTIFACT_KIND_SOURCE,
	AS_ARTIFACT_KIND_BINARY,
	/*< private >*/
	AS_ARTIFACT_KIND_LAST
} AsArtifactKind;

AsArtifactKind	 as_artifact_kind_from_string (const gchar *kind);
const gchar	*as_artifact_kind_to_string (AsArtifactKind kind);

AsArtifact	*as_artifact_new (void);

AsArtifactKind	 as_artifact_get_kind (AsArtifact *artifact);
void		 as_artifact_set_kind (AsArtifact *artifact,
					AsArtifactKind kind);

GPtrArray	*as_artifact_get_locations (AsArtifact *artifact);
void		as_artifact_add_location (AsArtifact *artifact,
						const gchar *location);

GPtrArray	*as_artifact_get_checksums (AsArtifact *artifact);
AsChecksum	*as_artifact_get_checksum (AsArtifact *artifact,
						AsChecksumKind kind);
void		as_artifact_add_checksum (AsArtifact *artifact,
					 AsChecksum *cs);

guint64		as_artifact_get_size (AsArtifact *artifact,
					AsSizeKind kind);
void		as_artifact_set_size (AsArtifact *artifact,
					guint64 size,
					AsSizeKind kind);

const gchar	*as_artifact_get_platform (AsArtifact *artifact);
void		as_artifact_set_platform (AsArtifact *artifact,
                                      const gchar *platform);

AsBundleKind	 as_artifact_get_bundle_kind (AsArtifact *artifact);
void		 as_artifact_set_bundle_kind (AsArtifact *artifact,
                                          AsBundleKind kind);

const gchar	*as_artifact_get_filename (AsArtifact *artifact);
void		as_artifact_set_filename (AsArtifact *artifact,
					  const gchar *filename);


G_END_DECLS

#endif /* __AS_ARTIFACT_H */
