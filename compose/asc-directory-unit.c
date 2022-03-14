/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
	unit_class->dir_exists = asc_directory_unit_file_exists;
	unit_class->read_data = asc_directory_unit_read_data;
}

static gboolean
asc_directory_unit_find_files_recursive (GPtrArray *files,
					 const gchar *path_orig,
					 guint path_orig_len,
					 const gchar *path,
					 GError **error)
{
	const gchar *tmp;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GError) tmp_error = NULL;

	dir = g_dir_open (path, 0, &tmp_error);
	if (dir == NULL) {
		/* just ignore locations we do not have access to */
		if (g_error_matches (tmp_error, G_FILE_ERROR, G_FILE_ERROR_ACCES))
			return TRUE;
		g_propagate_error (error, g_steal_pointer (&tmp_error));
		return FALSE;
	}

	while ((tmp = g_dir_read_name (dir)) != NULL) {
		g_autofree gchar *path_new = NULL;
		path_new = g_build_filename (path, tmp, NULL);

		/* search recursively, don't follow symlinks */
		if (g_file_test (path_new, G_FILE_TEST_IS_DIR) &&
		    !g_file_test (path_new, G_FILE_TEST_IS_SYMLINK)) {
			if (!asc_directory_unit_find_files_recursive (files,
								      path_orig,
								      path_orig_len,
								      path_new,
								      error))
				return FALSE;
		} else {
			g_ptr_array_add (files, g_strdup (path_new + path_orig_len));
		}
	}

	return TRUE;
}

static gboolean
asc_directory_unit_open (AscUnit *unit, GError **error)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (ASC_DIRECTORY_UNIT (unit));
	g_autoptr(GPtrArray) contents = NULL;
	GPtrArray *relevant_paths;
	guint root_dir_len = (guint) strlen (priv->root_dir);

	if (g_str_has_suffix (priv->root_dir, G_DIR_SEPARATOR_S))
		root_dir_len = root_dir_len > 0? root_dir_len - 1 : root_dir_len;

	contents = g_ptr_array_new_with_free_func (g_free);
	relevant_paths = asc_unit_get_relevant_paths (unit);

	g_debug ("Creating contents index for directory: %s", priv->root_dir);
	if (relevant_paths->len == 0) {
		/* create an index of all the data
		 * TODO: All of this is super wasteful, and may need a completely different approach */
		if (!asc_directory_unit_find_files_recursive (contents,
								priv->root_dir, root_dir_len,
								priv->root_dir,
								error))
			return FALSE;
	} else {
		/* only index data from paths that we care about */
		for (guint i = 0; i < relevant_paths->len; i++) {
			g_autofree gchar *check_path = NULL;
			const gchar *rel_path = g_ptr_array_index (relevant_paths, i);
			check_path = g_build_filename (priv->root_dir, rel_path, NULL);

			if (!asc_directory_unit_find_files_recursive (contents,
									priv->root_dir, root_dir_len,
									check_path,
									error))
				return FALSE;
		}
	}
	g_debug ("Index done for directory: %s", priv->root_dir);

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
	if (asc_unit_get_bundle_id (ASC_UNIT (dirunit)) == NULL)
		asc_unit_set_bundle_id (ASC_UNIT (dirunit), priv->root_dir);
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
