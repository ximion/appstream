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

G_BEGIN_DECLS

#define AS_TYPE_REFERENCE (as_reference_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsReference, as_reference, AS, REFERENCE, GObject)

struct _AsReferenceClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

/**
 * AsReferenceKind:
 * @AS_REFERENCE_KIND_UNKNOWN:		Unknown reference kind.
 * @AS_REFERENCE_KIND_DOI:		Digital Object Identifier
 * @AS_REFERENCE_KIND_CITATION_CFF:	Web URL to a Citation File Format file
 * @AS_REFERENCE_KIND_REGISTRY:		A generic registry.
 *
 * A reference type.
 **/
typedef enum {
	AS_REFERENCE_KIND_UNKNOWN,
	AS_REFERENCE_KIND_DOI,
	AS_REFERENCE_KIND_CITATION_CFF,
	AS_REFERENCE_KIND_REGISTRY,
	/*< private >*/
	AS_REFERENCE_KIND_LAST
} AsReferenceKind;

const gchar    *as_reference_kind_to_string (AsReferenceKind kind);
AsReferenceKind as_reference_kind_from_string (const gchar *str);

AsReference    *as_reference_new (void);

AsReferenceKind as_reference_get_kind (AsReference *reference);
void		as_reference_set_kind (AsReference *reference, AsReferenceKind kind);

const gchar    *as_reference_get_value (AsReference *reference);
void		as_reference_set_value (AsReference *reference, const gchar *value);

const gchar    *as_reference_get_registry_name (AsReference *reference);
void		as_reference_set_registry_name (AsReference *reference, const gchar *name);

G_END_DECLS
