/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#define _GNU_SOURCE
#include "config.h"
#include "asc-directory-unit.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gstdio.h>

#ifdef HAVE_LINUX_OPENAT2_H
#include <linux/openat2.h>
#include <sys/syscall.h>
#endif

#include "as-utils-private.h"

/* flags to open a path with when all we want to know is whether it exists */
#ifdef O_PATH
#define ASC_O_LOOKUP (O_PATH | O_CLOEXEC)
#else
#define ASC_O_LOOKUP (O_RDONLY | O_CLOEXEC)
#endif

/* the amount of symbolic links we follow before giving up on a path */
#define ASC_MAX_SYMLINK_DEPTH 40

typedef struct {
	gchar *root_dir;
	gchar *root_dir_canonical;
	gint root_fd;
} AscDirectoryUnitPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscDirectoryUnit, asc_directory_unit, ASC_TYPE_UNIT)
#define GET_PRIVATE(o) (asc_directory_unit_get_instance_private (o))

static gboolean asc_directory_unit_open (AscUnit *unit, GError **error);
static void asc_directory_unit_close (AscUnit *unit);

static gboolean asc_directory_unit_file_exists (AscUnit *unit, const gchar *filename);
static GBytes *asc_directory_unit_read_data (AscUnit *unit, const gchar *filename, GError **error);

/**
 * asc_push_path_components:
 *
 * Split @path and push its components onto @stack, so that the leftmost
 * component is popped first.
 */
static void
asc_push_path_components (GPtrArray *stack, const gchar *path)
{
	gchar **parts = g_strsplit (path, G_DIR_SEPARATOR_S, -1);
	for (guint i = g_strv_length (parts); i > 0; i--)
		g_ptr_array_add (stack, parts[i - 1]);
	g_free (parts);
}

/**
 * asc_resolve_path_in_root:
 * @root: canonical, absolute path to the root directory.
 * @start_dir: (nullable): an already resolved directory within @root to interpret
 *    @path relative to, or %NULL to start at @root itself.
 * @path: the path to resolve.
 * @report_path: (nullable): the location to name in error messages, or %NULL to
 *    name the point below @root that resolution actually failed at.
 *
 * Resolve @path with @root taking the place of the filesystem root, so an
 * absolute symlink target is looked up below @root, and no amount of ".."
 * segments can climb out of it. Consequently, the result is always located
 * within @root.
 *
 * Returns: the fully resolved absolute path, or %NULL if any component of it
 * does not exist below @root or could not be resolved.
 */
static gchar *
asc_resolve_path_in_root (const gchar *root,
			  const gchar *start_dir,
			  const gchar *path,
			  const gchar *report_path,
			  GError **error)
{
	g_autoptr(GString) resolved = g_string_new (start_dir == NULL ? root : start_dir);
	g_autoptr(GPtrArray) pending = g_ptr_array_new_with_free_func (g_free);
	gsize root_len = strlen (root);
	guint n_links = 0;

	/* the filesystem root is the only canonical path with a trailing separator,
	 * and we add a separator of our own for every component */
	if (root_len == 1 && root[0] == G_DIR_SEPARATOR)
		root_len = 0;
	if (resolved->len == 1 && resolved->str[0] == G_DIR_SEPARATOR)
		g_string_truncate (resolved, 0);

	asc_push_path_components (pending, path);

	while (pending->len > 0) {
		g_autofree gchar *comp = g_ptr_array_steal_index (pending, pending->len - 1);
		g_autofree gchar *link_target = NULL;
		gsize comp_offset;
		GStatBuf lsb;

		/* empty and "." components do not move us anywhere */
		if (comp[0] == '\0' || g_str_equal (comp, "."))
			continue;

		if (g_str_equal (comp, "..")) {
			/* drop the last component, the root being its own parent
			 * just like the filesystem root is */
			for (gsize i = resolved->len; i > root_len; i--) {
				if (resolved->str[i - 1] == G_DIR_SEPARATOR) {
					g_string_truncate (resolved, i - 1);
					break;
				}
			}
			continue;
		}

		comp_offset = resolved->len;
		g_string_append_c (resolved, G_DIR_SEPARATOR);
		g_string_append (resolved, comp);

		if (g_lstat (resolved->str, &lsb) != 0) {
			g_set_error (error,
				     G_FILE_ERROR,
				     g_file_error_from_errno (errno),
				     "Unable to resolve '%s' within the root directory: %s",
				     report_path != NULL ? report_path : resolved->str + root_len,
				     g_strerror (errno));
			return NULL;
		}

		if (!S_ISLNK (lsb.st_mode))
			continue;

		if (++n_links > ASC_MAX_SYMLINK_DEPTH) {
			g_set_error (error,
				     G_FILE_ERROR,
				     G_FILE_ERROR_LOOP,
				     "Unable to resolve '%s' within the root directory: %s",
				     report_path != NULL ? report_path : resolved->str + root_len,
				     g_strerror (ELOOP));
			return NULL;
		}

		link_target = g_file_read_link (resolved->str, error);
		if (link_target == NULL)
			return NULL;

		if (g_path_is_absolute (link_target)) {
			/* an absolute link target is resolved from the root again */
			g_string_truncate (resolved, root_len);
		} else {
			/* the link is replaced by whatever it points at */
			g_string_truncate (resolved, comp_offset);
		}
		asc_push_path_components (pending, link_target);
	}

	/* if we are rooted at the filesystem root, a path that resolves to that
	 * root has consumed all of its components */
	if (resolved->len == 0)
		g_string_append_c (resolved, G_DIR_SEPARATOR);

	return g_string_free (g_steal_pointer (&resolved), FALSE);
}

/**
 * asc_openat2:
 *
 * Open @path below @dir_fd, with @dir_fd taking the place of the filesystem root.
 *
 * Returns: a file descriptor, or -1 with errno set. %ENOSYS means that this
 * system can not do the resolution for us at all.
 */
static gint
asc_openat2 (gint dir_fd, const gchar *path, gint flags)
{
#if defined(HAVE_OPENAT2) || defined(HAVE_LINUX_OPENAT2_H)
	struct open_how how = {
		.flags = flags,
		.resolve = RESOLVE_IN_ROOT,
	};
#ifdef HAVE_OPENAT2
	return openat2 (dir_fd, path, &how, sizeof (how));
#else
	/* the glibc wrapper is very recent, so we may have to ask the kernel directly */
	return (gint) syscall (SYS_openat2, dir_fd, path, &how, sizeof (how));
#endif
#else
	errno = ENOSYS;
	return -1;
#endif
}

/**
 * asc_openat2_usable:
 *
 * Check whether openat2() actually works here.
 * Sandboxes may reject the call (grrr!!!), so we try it instead of guessing.
 */
static gboolean
asc_openat2_usable (void)
{
	static gsize initialized = 0;
	static gboolean usable = FALSE;

	if (g_once_init_enter (&initialized)) {
		gint fd = asc_openat2 (AT_FDCWD, ".", ASC_O_LOOKUP);
		usable = fd >= 0;
		if (fd >= 0)
			g_close (fd, NULL);
		else
			g_debug ("Unable to use openat2() (%s), will resolve paths "
				 "in userspace instead.",
				 g_strerror (errno));
		g_once_init_leave (&initialized, 1);
	}

	return usable;
}

/**
 * asc_open_path_in_root:
 * @root_fd: descriptor for @root, or -1 if none is available.
 * @root: canonical, absolute path to the root directory.
 * @path: the path to open.
 * @flags: flags to open @path with.
 *
 * Open @path with @root taking the place of the filesystem root, which it can
 * not escape, neither via ".." segments nor via symbolic links.
 *
 * Returns: a file descriptor, or -1 if @path does not exist below @root.
 */
static gint
asc_open_path_in_root (gint root_fd,
		       const gchar *root,
		       const gchar *path,
		       gint flags,
		       GError **error)
{
	g_autofree gchar *fname_full = NULL;
	gint fd;

	/* openat2() gives an empty path no meaning, while we resolve it to the root */
	if (path == NULL || path[0] == '\0')
		path = ".";

	if (root_fd >= 0 && asc_openat2_usable ()) {
		/* the kernel confines the resolution for us, in a single call and without
		 * leaving a window between resolving the path and opening it */
		fd = asc_openat2 (root_fd, path, flags);
		if (fd < 0)
			g_set_error (error,
				     G_FILE_ERROR,
				     g_file_error_from_errno (errno),
				     "Unable to resolve '%s' within the root directory: %s",
				     path,
				     g_strerror (errno));
		return fd;
	}

	/* no kernel support, so we resolve the path ourselves. We name the location we
	 * were asked for in any error, just like the kernel does on the fast path */
	fname_full = asc_resolve_path_in_root (root, NULL, path, path, error);
	if (fname_full == NULL)
		return -1;

	fd = g_open (fname_full, flags, 0);
	if (fd < 0)
		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (errno),
			     "Unable to open '%s' within the root directory: %s",
			     path,
			     g_strerror (errno));
	return fd;
}

static void
asc_directory_unit_init (AscDirectoryUnit *dirunit)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (dirunit);

	priv->root_fd = -1;
}

static void
asc_directory_unit_clear_root_fd (AscDirectoryUnit *dirunit)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (dirunit);

	if (priv->root_fd < 0)
		return;
	g_close (priv->root_fd, NULL);
	priv->root_fd = -1;
}

static void
asc_directory_unit_finalize (GObject *object)
{
	AscDirectoryUnit *dirunit = ASC_DIRECTORY_UNIT (object);
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (dirunit);

	asc_directory_unit_clear_root_fd (dirunit);
	g_free (priv->root_dir);
	g_free (priv->root_dir_canonical);

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

/**
 * asc_path_string_len:
 *
 * Length of the part of @path that names we derive from it replace.
 */
static gsize
asc_path_string_len (const gchar *path)
{
	gsize len = strlen (path);

	/* the filesystem root is the only path with a trailing separator, and every
	 * name we derive comes with a separator of its own */
	if (len == 1 && path[0] == G_DIR_SEPARATOR)
		return 0;
	return len;
}

/**
 * asc_directory_unit_find_files_recursive_internal:
 * @root: canonical, absolute path to the root directory of the unit.
 * @path: resolved, absolute path to the directory to index.
 * @virtual_prefix: location of @path within the unit.
 *
 * Add all files below @path to @files, named by their location within the unit.
 */
static gboolean
asc_directory_unit_find_files_recursive_internal (GPtrArray *files,
						  const gchar *root,
						  const gchar *path,
						  const gchar *virtual_prefix,
						  GHashTable *visited_dirs,
						  GError **error)
{
	const gchar *tmp;
	gsize path_prefix_len = asc_path_string_len (path);
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
		g_autofree gchar *vpath_new = NULL;
		g_autofree gchar *real_path = NULL;
		GStatBuf sb;
		gboolean is_dir;

		path_new = g_build_filename (path, tmp, NULL);
		if (g_lstat (path_new, &sb) != 0) {
			g_debug ("Skipping '%s' while indexing: %s", path_new, g_strerror (errno));
			continue;
		}

		/* the index refers to data by its location within the unit, which is not
		 * necessarily the location the data is physically stored at */
		vpath_new = g_strconcat (virtual_prefix, path_new + path_prefix_len, NULL);

		if (S_ISLNK (sb.st_mode)) {
			/* resolve the link within the unit, so we never index or descend into
			 * anything that is not part of it */
			real_path = asc_resolve_path_in_root (root, path, tmp, NULL, NULL);
			if (real_path == NULL) {
				/* the link can not be resolved within this unit, because it is
				 * dangling or points to a location that does not exist in the
				 * unit. That is usually a packaging error, so we keep it in the
				 * index as a plain entry and let the failure to read it be
				 * reported later, instead of silently dropping the data */
				g_ptr_array_add (files, g_steal_pointer (&vpath_new));
				continue;
			}

			is_dir = g_file_test (real_path, G_FILE_TEST_IS_DIR);
		} else {
			is_dir = S_ISDIR (sb.st_mode);
		}

		/* search recursively */
		if (is_dir) {
			const gchar *real_dir = (real_path != NULL) ? real_path : path_new;

			/* don't visit paths twice to avoid loops */
			if (g_hash_table_contains (visited_dirs, real_dir))
				continue;
			g_hash_table_add (visited_dirs, g_strdup (real_dir));

			if (!asc_directory_unit_find_files_recursive_internal (files,
									       root,
									       real_dir,
									       vpath_new,
									       visited_dirs,
									       error))
				return FALSE;
		} else {
			g_ptr_array_add (files, g_steal_pointer (&vpath_new));
		}
	}

	return TRUE;
}

static gboolean
asc_directory_unit_find_files_recursive (GPtrArray *files,
					 const gchar *root,
					 const gchar *path,
					 const gchar *virtual_prefix,
					 GError **error)
{
	g_autoptr(GHashTable) visited_dirs = NULL;
	visited_dirs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	g_debug ("Indexing location: %s", path);
	return asc_directory_unit_find_files_recursive_internal (files,
								 root,
								 path,
								 virtual_prefix,
								 visited_dirs,
								 error);
}

static gboolean
asc_directory_unit_open (AscUnit *unit, GError **error)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (ASC_DIRECTORY_UNIT (unit));
	g_autoptr(GPtrArray) contents = NULL;
	GPtrArray *relevant_paths;
	gsize root_dir_len;

	/* resolve the root directory, so we can confine all data reads to it. We index
	 * the resolved location as well, so the index and any later read agree on what
	 * belongs to this unit */
	asc_directory_unit_clear_root_fd (ASC_DIRECTORY_UNIT (unit));
	g_clear_pointer (&priv->root_dir_canonical, g_free);
	priv->root_dir_canonical = realpath (priv->root_dir, NULL);
	if (priv->root_dir_canonical == NULL) {
		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (errno),
			     "Unable to resolve root directory '%s': %s",
			     priv->root_dir,
			     g_strerror (errno));
		return FALSE;
	}
	root_dir_len = asc_path_string_len (priv->root_dir_canonical);

	/* anchor for openat2(); if we can not get a descriptor, we resolve
	 * paths in userspace instead (so we ignore errors here) */
	priv->root_fd = g_open (priv->root_dir_canonical, ASC_O_LOOKUP | O_DIRECTORY, 0);

	contents = g_ptr_array_new_with_free_func (g_free);
	relevant_paths = asc_unit_get_relevant_paths (unit);

	g_debug ("Creating contents index for directory: %s", priv->root_dir);
	if (relevant_paths->len == 0) {
		/* create an index of all the data
		 * TODO: All of this is super wasteful, and may need a completely different approach */
		if (!asc_directory_unit_find_files_recursive (contents,
							      priv->root_dir_canonical,
							      priv->root_dir_canonical,
							      "",
							      error))
			return FALSE;
	} else {
		/* only index data from paths that we care about */
		for (guint i = 0; i < relevant_paths->len; i++) {
			g_autofree gchar *check_path = NULL;
			g_autofree gchar *real_path = NULL;
			const gchar *rel_path = g_ptr_array_index (relevant_paths, i);
			check_path = g_build_filename (priv->root_dir_canonical, rel_path, NULL);

			/* the location we are asked to index may be a symbolic link itself, so
			 * we resolve it within the unit before walking it, while still indexing
			 * everything below it under the location it is expected at */
			real_path = asc_resolve_path_in_root (priv->root_dir_canonical,
							      NULL,
							      check_path + root_dir_len,
							      NULL,
							      error);
			if (real_path == NULL)
				return FALSE;

			if (!asc_directory_unit_find_files_recursive (contents,
								      priv->root_dir_canonical,
								      real_path,
								      check_path + root_dir_len,
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
	asc_directory_unit_clear_root_fd (ASC_DIRECTORY_UNIT (unit));
}

/**
 * asc_directory_unit_open_file:
 *
 * Open @filename with the root directory of this unit taking the place of the
 * filesystem root, which it can not escape, neither via ".." segments nor via
 * symbolic links.
 *
 * Returns: a file descriptor, or -1 if the file does not exist within the unit.
 */
static gint
asc_directory_unit_open_file (AscDirectoryUnit *dirunit,
			      const gchar *filename,
			      gint flags,
			      GError **error)
{
	AscDirectoryUnitPrivate *priv = GET_PRIVATE (dirunit);

	if (priv->root_dir_canonical == NULL) {
		g_set_error_literal (error,
				     G_FILE_ERROR,
				     G_FILE_ERROR_FAILED,
				     "The unit was not opened, so no data can be read from it.");
		return -1;
	}

	return asc_open_path_in_root (priv->root_fd,
				      priv->root_dir_canonical,
				      filename,
				      flags,
				      error);
}

static gboolean
asc_directory_unit_file_exists (AscUnit *unit, const gchar *filename)
{
	gint fd = asc_directory_unit_open_file (ASC_DIRECTORY_UNIT (unit),
						filename,
						ASC_O_LOOKUP,
						NULL);
	if (fd < 0)
		return FALSE;

	g_close (fd, NULL);
	return TRUE;
}

static GBytes *
asc_directory_unit_read_data (AscUnit *unit, const gchar *filename, GError **error)
{
	g_autoptr(GMappedFile) mfile = NULL;
	gint fd;

	fd = asc_directory_unit_open_file (ASC_DIRECTORY_UNIT (unit),
					   filename,
					   O_RDONLY | O_CLOEXEC,
					   error);
	if (fd < 0)
		return NULL;

	mfile = g_mapped_file_new_from_fd (fd, FALSE, error);
	g_close (fd, NULL);

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
const gchar *
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
	g_clear_pointer (&priv->root_dir_canonical, g_free);
	asc_directory_unit_clear_root_fd (dirunit);
	if (asc_unit_get_bundle_id (ASC_UNIT (dirunit)) == NULL)
		asc_unit_set_bundle_id (ASC_UNIT (dirunit), priv->root_dir);
}

/**
 * asc_directory_unit_new:
 *
 * Creates a new #AscDirectoryUnit.
 **/
AscDirectoryUnit *
asc_directory_unit_new (const gchar *root_dir)
{
	AscDirectoryUnit *dirunit;
	dirunit = g_object_new (ASC_TYPE_DIRECTORY_UNIT, NULL);
	asc_directory_unit_set_root (ASC_DIRECTORY_UNIT (dirunit), root_dir);
	return ASC_DIRECTORY_UNIT (dirunit);
}
