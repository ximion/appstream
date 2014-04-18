/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef __AS_METADATA_H
#define __AS_METADATA_H

#include <glib-object.h>

#include "as-component.h"

#define AS_TYPE_METADATA			(as_metadata_get_type())
#define AS_METADATA(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_METADATA, AsMetadata))
#define AS_METADATA_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_METADATA, AsMetadataClass))
#define AS_IS_METADATA(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_METADATA))
#define AS_IS_METADATA_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_METADATA))
#define AS_METADATA_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_METADATA, AsMetadataClass))

G_BEGIN_DECLS

typedef struct _AsMetadata	AsMetadata;
typedef struct _AsMetadataClass	AsMetadataClass;

struct _AsMetadata
{
	GObject			parent;
};

struct _AsMetadataClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

GType		 as_metadata_get_type		(void);
AsMetadata	*as_metadata_new			(void);



G_END_DECLS

#endif /* __AS_METADATA_H */
