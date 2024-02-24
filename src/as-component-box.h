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

#include "as-component.h"

G_BEGIN_DECLS

typedef struct _AsComponentBox	    AsComponentBox;
typedef struct _AsComponentBoxClass AsComponentBoxClass;

struct _AsComponentBox {
	GObject	   parent_instance;
	/*< private >*/
	GPtrArray *cpts;
};

struct _AsComponentBoxClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

GType as_component_box_get_type (void);
#define AS_TYPE_COMPONENT_BOX (as_component_box_get_type ())
#define AS_COMPONENT_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_COMPONENT_BOX, AsComponentBox))
G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsComponentBox, g_object_unref)

/**
 * AsComponentBoxFlags:
 * @AS_COMPONENT_BOX_FLAG_NONE:		No flags.
 * @AS_COMPONENT_BOX_FLAG_NO_CHECKS:	Only perform the most basic verification.
 *
 * Flags controlling the component box behavior.
 **/
typedef enum {
	AS_COMPONENT_BOX_FLAG_NONE	= 0,
	AS_COMPONENT_BOX_FLAG_NO_CHECKS = 1 << 0,
} AsComponentBoxFlags;

AsComponentBox *as_component_box_new (AsComponentBoxFlags flags);
AsComponentBox *as_component_box_new_simple (void);

#define as_component_box_index(cbox, index_) \
	AS_COMPONENT (g_ptr_array_index ((cbox)->cpts, (index_)))
#define as_component_box_len(cbox) (cbox)->cpts->len

GPtrArray	   *as_component_box_as_array (AsComponentBox *cbox);
AsComponentBoxFlags as_component_box_get_flags (AsComponentBox *cbox);

guint		    as_component_box_get_size (AsComponentBox *cbox);
AsComponent	   *as_component_box_index_safe (AsComponentBox *cbox, guint index);
gboolean	    as_component_box_is_empty (AsComponentBox *cbox);

gboolean	    as_component_box_add (AsComponentBox *cbox, AsComponent *cpt, GError **error);
void		    as_component_box_clear (AsComponentBox *cbox);
void		    as_component_box_remove_at (AsComponentBox *cbox, guint index);

void		    as_component_box_sort (AsComponentBox *cbox);
void		    as_component_box_sort_by_score (AsComponentBox *cbox);

G_END_DECLS
