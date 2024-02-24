/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
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

typedef struct _AsReleaseList	   AsReleaseList;
typedef struct _AsReleaseListClass AsReleaseListClass;

struct _AsReleaseList {
	GObject	   parent_instance;
	/*< private >*/
	GPtrArray *entries;
};

struct _AsReleaseListClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

GType as_release_list_get_type (void);
#define AS_TYPE_RELEASE_LIST (as_release_list_get_type ())
#define AS_RELEASE_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_RELEASE_LIST, AsReleaseList))
G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsReleaseList, g_object_unref)

/**
 * AsReleaseListKind:
 * @AS_RELEASE_LIST_KIND_UNKNOWN:		Unknown releases type
 * @AS_RELEASE_LIST_KIND_EMBEDDED:		Release info is embedded in metainfo file
 * @AS_RELEASE_LIST_KIND_EXTERNAL:		Release info is split to a separate file
 *
 * The kind of a releases block.
 *
 * Since: 0.16.0
 **/
typedef enum {
	AS_RELEASE_LIST_KIND_UNKNOWN,
	AS_RELEASE_LIST_KIND_EMBEDDED,
	AS_RELEASE_LIST_KIND_EXTERNAL,
	/*< private >*/
	AS_RELEASE_LIST_KIND_LAST
} AsReleaseListKind;

const gchar	 *as_release_list_kind_to_string (AsReleaseListKind kind);
AsReleaseListKind as_release_list_kind_from_string (const gchar *kind_str);

AsReleaseList	 *as_release_list_new (void);

#define as_release_list_index(rels, index_) \
	AS_RELEASE (g_ptr_array_index ((rels)->entries, (index_)))
#define as_release_list_len(rels) (rels)->entries->len

GPtrArray	 *as_release_list_get_entries (AsReleaseList *rels);
guint		  as_release_list_get_size (AsReleaseList *rels);
void		  as_release_list_set_size (AsReleaseList *rels, guint size);
AsRelease	 *as_release_list_index_safe (AsReleaseList *rels, guint index);
gboolean	  as_release_list_is_empty (AsReleaseList *rels);

void		  as_release_list_add (AsReleaseList *rels, AsRelease *release);
void		  as_release_list_clear (AsReleaseList *rels);

void		  as_release_list_sort (AsReleaseList *rels);

AsContext	 *as_release_list_get_context (AsReleaseList *rels);
void		  as_release_list_set_context (AsReleaseList *rels, AsContext *context);

AsReleaseListKind as_release_list_get_kind (AsReleaseList *rels);
void		  as_release_list_set_kind (AsReleaseList *rels, AsReleaseListKind kind);

const gchar	 *as_release_list_get_url (AsReleaseList *rels);
void		  as_release_list_set_url (AsReleaseList *rels, const gchar *url);

gboolean	  as_release_list_load_from_bytes (AsReleaseList *rels,
						   AsContext	 *context,
						   GBytes	 *bytes,
						   GError	**error);

G_END_DECLS
