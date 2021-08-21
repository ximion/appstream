/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2021 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-directory-unit
 * @short_description: A data source unit representing a simple directory tree
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-directory-unit.h"

#include "as-utils-private.h"

typedef struct
{
	gchar *root_dir;
	GHashTable *files_map;
} AscDirectoryUnitPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscDirectoryUnit, asc_directory_unit, ASC_TYPE_UNIT)
#define GET_PRIVATE(o) (asc_directory_unit_get_instance_private (o))

static gboolean		asc_directory_unit_open (AscUnit *unit,
						 GError **error);
static void		asc_directory_unit_close (AscUnit *unit);

static gboolean		asc_directory_unit_file_exists (AscUnit *unit,
							const gchar *filename);
static GBytes		*asc_directory_unit_read_data (AscUnit *unit,
							const gchar *filename,
							GError **error);

static void
asc_directory_unit_init (AscDirectoryUnit *dirunit)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (dirunit);

	priv->files_map = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 g_free,
						 g_free);
}

static void
asc_directory_unit_finalize (GObject *object)
{
	AscDirectoryUnit *dirunit = ASC_DIRECTORY_UNIT (object);
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (dirunit);

	g_free (priv->root_dir);
	g_hash_table_unref (priv->files_map);

	G_OBJECT_CLASS (asc_directory_unit_parent_class)->finalize (object);
}

static void
asc_directory_unit_class_init (AscDirectoryUnitClass *klass)
{
	AscUnitClass *unit_class;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_directory_unit_finalize;

	unit_class = ASC_UNIT_CLASS (klass);
	unit_class->open = asc_directory_unit_open;
	unit_class->close = asc_directory_unit_close;
	unit_class->file_exists = asc_directory_unit_file_exists;
	unit_class->read_data = asc_directory_unit_read_data;
}

static gboolean
asc_directory_unit_open (AscUnit *unit, GError **error)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (ASC_DIRECTORY_UNIT (unit));
	g_autoptr(GPtrArray) contents;
	guint root_dir_len = strlen (priv->root_dir);

	contents = as_utils_find_files (priv->root_dir, TRUE, error);
	if (contents == NULL)
		return FALSE;

	for (guint i = 0; i < contents->len; i++) {
		g_autofree gchar *fname = g_steal_pointer (&g_ptr_array_index (contents, i));
		g_ptr_array_index (contents, i) = g_canonicalize_filename (fname + root_dir_len, priv->root_dir);
	}

	asc_unit_set_contents (unit, contents);
	return TRUE;
}

static void
asc_directory_unit_close (AscUnit *unit)
{
	/* we don't actually need to do anything here */
	return;
}

static gboolean
asc_directory_unit_file_exists (AscUnit *unit, const gchar *filename)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (ASC_DIRECTORY_UNIT (unit));
	g_autofree gchar *fname_full = g_build_filename (priv->root_dir, filename, NULL);

	return g_file_test (fname_full, G_FILE_TEST_EXISTS);
}

static GBytes*
asc_directory_unit_read_data (AscUnit *unit, const gchar *filename, GError **error)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (ASC_DIRECTORY_UNIT (unit));
	g_autoptr(GMappedFile) mfile = NULL;
	g_autofree gchar *fname_full = g_build_filename (priv->root_dir, filename, NULL);

	mfile = g_mapped_file_new (fname_full, FALSE, error);
	if (!mfile)
		return NULL;
	return g_mapped_file_get_bytes (mfile);
}

/**
 * asc_directory_unit_get_root:
 * @dirunit: an #AscDirectoryUnit instance.
 *
 * Get the root directory path for this unit.
 **/
const gchar*
asc_directory_unit_get_root (AscDirectoryUnit *dirunit)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (dirunit);
	return priv->root_dir;
}

/**
 * asc_directory_unit_set_root:
 * @dirunit: an #AscDirectoryUnit instance.
 * @root_dir: Absolute directory path
 *
 * Sets the root directory path for this unit.
 **/
void
asc_directory_unit_set_root (AscDirectoryUnit *dirunit, const gchar *root_dir)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (dirunit);
	as_assign_string_safe (priv->root_dir, root_dir);
}

/**
 * asc_directory_unit_new:
 *
 * Creates a new #AscDirectoryUnit.
 **/
AscDirectoryUnit*
asc_directory_unit_new (const gchar *root_dir)
{
	AscDirectoryUnit *dirunit;
	dirunit = g_object_new (ASC_TYPE_DIRECTORY_UNIT, NULL);
	asc_directory_unit_set_root (ASC_DIRECTORY_UNIT (dirunit), root_dir);
	return ASC_DIRECTORY_UNIT (dirunit);
}
