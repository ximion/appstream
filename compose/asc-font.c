/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:asc-font
 * @short_description: Font handling functions.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-font.h"

GMutex fontconfig_mutex;

typedef struct
{
	guint	dummy;
} AscFontPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscFont, asc_font, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_font_get_instance_private (o))

static void
asc_font_finalize (GObject *object)
{
	AscFont *font = ASC_FONT (object);
	AscFontPrivate *priv = GET_PRIVATE (font);

	priv->dummy = 0;

	G_OBJECT_CLASS (asc_font_parent_class)->finalize (object);
}

static void
asc_font_init (AscFont *font)
{
}

static void
asc_font_class_init (AscFontClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_font_finalize;
}

static AscFont*
asc_font_new (void)
{
	AscFont *font = g_object_new (ASC_TYPE_FONT, NULL);
	return ASC_FONT (font);
}

/**
 * asc_font_new_from_file:
 * @fname: Name of the file to load.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscFont from a file on the filesystem.
 **/
AscFont*
asc_font_new_from_file (const gchar* fname, GError **error)
{
	return asc_font_new ();
}

/**
 * asc_font_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscFont from data in memory.
 **/
AscFont*
asc_font_new_from_data (const void *data, gssize len, GError **error)
{
	return asc_font_new ();
}
