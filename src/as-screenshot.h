/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C)      2014 Richard Hughes <richard@hughsie.com>
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

#ifndef __AS_SCREENSHOT_H
#define __AS_SCREENSHOT_H

#include <glib-object.h>

#include "as-image.h"

#define AS_TYPE_SCREENSHOT		(as_screenshot_get_type())
#define AS_SCREENSHOT(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_SCREENSHOT, AsScreenshot))
#define AS_SCREENSHOT_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_SCREENSHOT, AsScreenshotClass))
#define AS_IS_SCREENSHOT(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_SCREENSHOT))
#define AS_IS_SCREENSHOT_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_SCREENSHOT))
#define AS_SCREENSHOT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_SCREENSHOT, AsScreenshotClass))

G_BEGIN_DECLS

typedef struct _AsScreenshot		AsScreenshot;
typedef struct _AsScreenshotClass	AsScreenshotClass;

struct _AsScreenshot
{
	GObject			parent;
};

struct _AsScreenshotClass
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

/**
 * AsScreenshotKind:
 * @AS_SCREENSHOT_KIND_UNKNOWN:		Type invalid or not known
 * @AS_SCREENSHOT_KIND_NORMAL:		Optional screenshot
 * @AS_SCREENSHOT_KIND_DEFAULT:		Screenshot to show by default
 *
 * The screenshot type.
 **/
typedef enum {
	AS_SCREENSHOT_KIND_UNKNOWN,
	AS_SCREENSHOT_KIND_NORMAL,
	AS_SCREENSHOT_KIND_DEFAULT,
	/*< private >*/
	AS_SCREENSHOT_KIND_LAST
} AsScreenshotKind;

AsScreenshotKind	as_screenshot_kind_from_string (const gchar *kind);
const gchar			*as_screenshot_kind_to_string	(AsScreenshotKind kind);
gboolean			as_screenshot_is_valid (AsScreenshot *screenshot);

GType		 	as_screenshot_get_type		(void);
AsScreenshot	*as_screenshot_new		(void);

AsScreenshotKind	as_screenshot_get_kind (AsScreenshot *screenshot);
void		 		as_screenshot_set_kind (AsScreenshot *screenshot,
											AsScreenshotKind kind);

const gchar			*as_screenshot_get_caption (AsScreenshot *screenshot);
void				as_screenshot_set_caption (AsScreenshot *screenshot,
										const gchar *caption);


GPtrArray			*as_screenshot_get_images (AsScreenshot *screenshot);
void				as_screenshot_add_image (AsScreenshot *screenshot,
											AsImage *image);

G_END_DECLS

#endif /* __AS_SCREENSHOT_H */
