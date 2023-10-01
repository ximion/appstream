/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2023-2023 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined(__APPSTREAM_H) && !defined(AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#pragma once

#include <glib-object.h>

#include "as-release.h"

G_BEGIN_DECLS

typedef struct _AsReleases	AsReleases;
typedef struct _AsReleasesClass AsReleasesClass;

struct _AsReleases {
	GObject	   parent_instance;
	/*< private >*/
	GPtrArray *entries;
};

struct _AsReleasesClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

GType as_releases_get_type (void);
#define AS_TYPE_RELEASES (as_releases_get_type ())
#define AS_RELEASES(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_RELEASES, AsReleases))
G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsReleases, g_object_unref)

/**
 * AsReleasesKind:
 * @AS_RELEASES_KIND_UNKNOWN:		Unknown releases type
 * @AS_RELEASES_KIND_EMBEDDED:		Release info is embedded in metainfo file
 * @AS_RELEASES_KIND_EXTERNAL:		Release info is split to a separate file
 *
 * The kind of a releases block.
 *
 * Since: 0.16.0
 **/
typedef enum {
	AS_RELEASES_KIND_UNKNOWN,
	AS_RELEASES_KIND_EMBEDDED,
	AS_RELEASES_KIND_EXTERNAL,
	/*< private >*/
	AS_RELEASES_KIND_LAST
} AsReleasesKind;

const gchar   *as_releases_kind_to_string (AsReleasesKind kind);
AsReleasesKind as_releases_kind_from_string (const gchar *kind_str);

AsReleases    *as_releases_new (void);

#define as_releases_index(rels, index_) AS_RELEASE (g_ptr_array_index ((rels)->entries, (index_)))
#define as_releases_len(rels)		(rels)->entries->len

GPtrArray     *as_releases_get_entries (AsReleases *rels);
guint	       as_releases_get_size (AsReleases *rels);
void	       as_releases_set_size (AsReleases *rels, guint size);
AsRelease     *as_releases_index_safe (AsReleases *rels, guint index);
gboolean       as_releases_is_empty (AsReleases *rels);

void	       as_releases_add (AsReleases *rels, AsRelease *release);
void	       as_releases_clear (AsReleases *rels);

void	       as_releases_sort (AsReleases *rels);

AsContext     *as_releases_get_context (AsReleases *rels);
void	       as_releases_set_context (AsReleases *rels, AsContext *context);

AsReleasesKind as_releases_get_kind (AsReleases *rels);
void	       as_releases_set_kind (AsReleases *rels, AsReleasesKind kind);

const gchar   *as_releases_get_url (AsReleases *rels);
void	       as_releases_set_url (AsReleases *rels, const gchar *url);

gboolean       as_releases_load_from_bytes (AsReleases *rels, GBytes *bytes, GError **error);

G_END_DECLS
