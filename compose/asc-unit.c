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
 * SECTION:asc-unit
 * @short_description: A data source unit (package, bundle, database, ...) for #AscCompose to process
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-unit.h"

#include "as-utils-private.h"

typedef struct
{
	AsBundleKind	bundle_kind;
	gchar		*bundle_id;
	gchar		*bundle_id_safe;
	GPtrArray	*contents;
	GPtrArray	*relevant_paths;

	gpointer	user_data;
} AscUnitPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscUnit, asc_unit, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_unit_get_instance_private (o))

static void
asc_unit_init (AscUnit *unit)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);

	priv->bundle_kind = AS_BUNDLE_KIND_UNKNOWN;
	priv->contents = g_ptr_array_new_with_free_func (g_free);
	priv->relevant_paths = g_ptr_array_new_with_free_func (g_free);
}

static void
asc_unit_finalize (GObject *object)
{
	AscUnit *unit = ASC_UNIT (object);
	AscUnitPrivate *priv = GET_PRIVATE (unit);

	g_free (priv->bundle_id);
	g_free (priv->bundle_id_safe);
	g_ptr_array_unref (priv->contents);
	g_ptr_array_unref (priv->relevant_paths);

	G_OBJECT_CLASS (asc_unit_parent_class)->finalize (object);
}

static void
asc_unit_class_init (AscUnitClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_unit_finalize;
}

/**
 * asc_unit_get_bundle_kind:
 * @unit: an #AscUnit instance.
 *
 * Gets the bundle kind of this unit.
 **/
AsBundleKind
asc_unit_get_bundle_kind (AscUnit *unit)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	return priv->bundle_kind;
}

/**
 * asc_unit_set_bundle_kind:
 * @unit: an #AscUnit instance.
 *
 * Sets the kind of the bundle this unit represents.
 **/
void
asc_unit_set_bundle_kind (AscUnit *unit, AsBundleKind kind)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	priv->bundle_kind = kind;
}

/**
 * asc_unit_get_bundle_id:
 * @unit: an #AscUnit instance.
 *
 * Gets the ID name of the bundle (a package / Flatpak / any entity containing metadata)
 * that this unit represents.
 **/
const gchar*
asc_unit_get_bundle_id (AscUnit *unit)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	return priv->bundle_id;
}

/**
 * asc_unit_get_bundle_id_safe:
 * @unit: an #AscUnit instance.
 *
 * Gets the ID name of the bundle, normalized to be safe to use
 * in filenames. This may *not* be the same name as set via asc_unit_get_bundle_id()
 **/
const gchar*
asc_unit_get_bundle_id_safe (AscUnit *unit)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	return priv->bundle_id_safe;
}

/**
 * asc_unit_set_bundle_id:
 * @unit: an #AscUnit instance.
 * @id: The new ID.
 *
 * Sets the ID of the bundle represented by this unit.
 **/
void
asc_unit_set_bundle_id (AscUnit *unit, const gchar *id)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	GString *tmp;
	as_assign_string_safe (priv->bundle_id, id);

	tmp = g_string_new (priv->bundle_id);
	if (g_strcmp0 (tmp->str, "/") == 0) {
		g_string_truncate (tmp, 0);
		g_string_append (tmp, "root");
	} else {
		as_gstring_replace (tmp, "/", "-");
		as_gstring_replace (tmp, "\\", "-");
		as_gstring_replace (tmp, ":", "_");

		if (g_str_has_prefix (tmp->str, "-") || g_str_has_prefix (tmp->str, "."))
			g_string_erase (tmp, 0, 1);
		if (g_strcmp0 (tmp->str, "") == 0)
			g_string_append (tmp, "BADNAME");
	}

	g_free (priv->bundle_id_safe);
	priv->bundle_id_safe = g_string_free (tmp, FALSE);
}

/**
 * asc_unit_get_contents:
 * @unit: an #AscUnit instance.
 *
 * Get a list of all files contained by this unit.
 *
 * Returns: (transfer none) (element-type utf8) : A file listing
 **/
GPtrArray*
asc_unit_get_contents (AscUnit *unit)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	return priv->contents;
}

/**
 * asc_unit_set_contents:
 * @unit: an #AscUnit instance.
 * @contents: (element-type utf8): A list of files contained by this unit.
 *
 * Set list of files this unit contains.
 **/
void
asc_unit_set_contents (AscUnit *unit, GPtrArray *contents)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	if (priv->contents == contents)
		return;
	g_ptr_array_unref (priv->contents);
	priv->contents = g_ptr_array_ref (contents);
}

/**
 * asc_unit_get_relevant_paths:
 * @unit: an #AscUnit instance.
 *
 * Get a list of paths that are relevant for data processing.
 *
 * Returns: (transfer none) (element-type utf8) : A list of paths
 **/
GPtrArray*
asc_unit_get_relevant_paths (AscUnit *unit)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	return priv->relevant_paths;
}

/**
 * asc_unit_add_relevant_path:
 * @unit: an #AscUnit instance.
 * @path: path to be considered relevant
 *
 * Add a path to the list of relevant directories.
 * A unit may only read data in paths that were previously
 * registered as relevant.
 **/
void
asc_unit_add_relevant_path (AscUnit *unit, const gchar *path)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);

	/* duplicate check */
	for (guint i = 0; i < priv->relevant_paths->len; i++) {
		if (g_strcmp0 (g_ptr_array_index (priv->relevant_paths, i), path) == 0)
			return;
	}
	g_ptr_array_add (priv->relevant_paths, g_strdup (path));
}

/**
 * asc_unit_open:
 * @unit: an #AscUnit instance.
 * @error: A #GError
 *
 * Open this unit, populating its content listing.
 **/
gboolean
asc_unit_open (AscUnit *unit, GError **error)
{
	AscUnitClass *klass;
	g_return_val_if_fail (ASC_IS_UNIT (unit), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	klass = ASC_UNIT_GET_CLASS (unit);
	g_return_val_if_fail (klass->open != NULL, FALSE);
	return klass->open (unit, error);
}

/**
 * asc_unit_close:
 * @unit: an #AscUnit instance.
 *
 * Close this unit, possibly freeing its resources. Calls to read_data() or
 * get_contents() may not produce results until open() is called again.
 **/
void
asc_unit_close (AscUnit *unit)
{
	AscUnitClass *klass;
	g_return_if_fail (ASC_IS_UNIT (unit));

	klass = ASC_UNIT_GET_CLASS (unit);
	g_return_if_fail (klass->close != NULL);
	klass->close (unit);
}

/**
 * asc_unit_file_exists:
 * @unit: an #AscUnit instance.
 * @filename: The filename to check.
 *
 * Returns %TRUE if the filename exists and is readable using %asc_unit_read_data.
 **/
gboolean
asc_unit_file_exists (AscUnit *unit, const gchar *filename)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	AscUnitClass *klass;
	g_return_val_if_fail (ASC_IS_UNIT (unit), FALSE);

	klass = ASC_UNIT_GET_CLASS (unit);

	if (klass->file_exists == NULL && priv->contents != NULL) {
		/* fallback */
		for (guint i = 0; i < priv->contents->len; i++) {
			if (g_strcmp0 (filename, g_ptr_array_index (priv->contents, i)) == 0)
				return TRUE;
		}
		return FALSE;
	}

	g_return_val_if_fail (klass->file_exists != NULL, FALSE);
	return klass->file_exists (unit, filename);
}

/**
 * asc_unit_dir_exists:
 * @unit: an #AscUnit instance.
 * @dirname: The directory name to check.
 *
 * Returns %TRUE if the directory exists and files in it are readable.
 **/
gboolean
asc_unit_dir_exists (AscUnit *unit, const gchar *dirname)
{
	AscUnitClass *klass;
	g_return_val_if_fail (ASC_IS_UNIT (unit), FALSE);

	klass = ASC_UNIT_GET_CLASS (unit);
	g_return_val_if_fail (klass->dir_exists != NULL, FALSE);
	return klass->dir_exists (unit, dirname);
}

/**
 * asc_unit_read_data:
 * @unit: an #AscUnit instance.
 * @filename: The file to read data for.
 * @error: A #GError
 *
 * Read the contents of the selected file into memory and return them.
 **/
GBytes*
asc_unit_read_data (AscUnit *unit, const gchar *filename, GError **error)
{
	AscUnitClass *klass;
	g_return_val_if_fail (ASC_IS_UNIT (unit), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	klass = ASC_UNIT_GET_CLASS (unit);
	g_return_val_if_fail (klass->read_data != NULL, FALSE);
	return klass->read_data (unit, filename, error);
}

/**
 * asc_unit_get_user_data:
 * @unit: an #AscUnit instance.
 *
 * Get user-defined data. This is a helper
 * function for bindings.
 **/
gpointer
asc_unit_get_user_data (AscUnit *unit)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	return priv->user_data;
}

/**
 * asc_unit_set_user_data:
 * @unit: an #AscUnit instance.
 * @user_data: the user data
 *
 * Assign user-defined data to this object. This is a helper
 * function for bindings.
 **/
void
asc_unit_set_user_data (AscUnit *unit, gpointer user_data)
{
	AscUnitPrivate *priv = GET_PRIVATE (unit);
	priv->user_data = user_data;
}

/**
 * asc_unit_new:
 *
 * Creates a new #AscUnit.
 **/
AscUnit*
asc_unit_new (void)
{
	AscUnit *unit;
	unit = g_object_new (ASC_TYPE_UNIT, NULL);
	return ASC_UNIT (unit);
}
