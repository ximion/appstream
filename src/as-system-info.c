/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2023 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-system-info
 * @short_description: Read information about the current OS and device.
 * @include: appstream.h
 *
 * This class reads information about the current operating system and device
 * that AppStream is running on. It can also be used by GUI toolkits to set
 * data that we can not automatically determine in a toolkit-independent way,
 * such as screen dimensions.
 *
 * AppStream uses this information to verify component relations
 * (as set in requires/recommends/supports etc. tags).
 *
 * See also: #AsComponent
 */

#include "config.h"
#include "as-system-info-private.h"

#include <gio/gio.h>
#include <errno.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <fnmatch.h>

#if defined(__linux__)
#include <sys/sysinfo.h>
#elif defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#ifdef HAVE_SYSTEMD
#include <systemd/sd-hwdb.h>
#include <systemd/sd-device.h>
#endif

#include "as-utils-private.h"

#define MB_IN_BYTES (1024 * 1024)

typedef struct
{
	gchar		*os_id;
	gchar		*os_cid;
	gchar		*os_name;
	gchar		*os_version;
	gchar		*os_homepage;

	gchar		*kernel_name;
	gchar		*kernel_version;

	gulong		memory_total;

	GPtrArray	*modaliases;
	GHashTable	*modalias_to_sysfs;

	gboolean	inputs_scanned;
	guint		input_controls;
	guint		tested_input_controls;

	gulong		display_length_shortest;
	gulong		display_length_longest;

#ifdef HAVE_SYSTEMD
	sd_hwdb		*hwdb;
#endif
} AsSystemInfoPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsSystemInfo, as_system_info, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_system_info_get_instance_private (o))

/**
 * as_system_info_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.16.0
 **/
G_DEFINE_QUARK (as-system-info-error-quark, as_system_info_error)

static void
as_system_info_init (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);

	priv->modaliases = g_ptr_array_new ();
	priv->modalias_to_sysfs = g_hash_table_new_full (g_str_hash, g_str_equal,
							 g_free, g_free);
}

static void
as_system_info_finalize (GObject *object)
{
	AsSystemInfo *sysinfo = AS_SYSTEM_INFO (object);
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);

	g_free (priv->os_id);
	g_free (priv->os_cid);
	g_free (priv->os_name);
	g_free (priv->os_version);
	g_free (priv->os_homepage);

	g_free (priv->kernel_name);
	g_free (priv->kernel_version);

	g_ptr_array_unref (priv->modaliases);
	g_hash_table_unref (priv->modalias_to_sysfs);

#ifdef HAVE_SYSTEMD
	if (priv->hwdb != NULL)
		sd_hwdb_unref (priv->hwdb);
#endif

	G_OBJECT_CLASS (as_system_info_parent_class)->finalize (object);
}

static void
as_system_info_class_init (AsSystemInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_system_info_finalize;
}

/**
 * as_system_info_load_os_release:
 */
void
as_system_info_load_os_release (AsSystemInfo *sysinfo, const gchar *os_release_fname)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	g_autoptr(GFile) f = NULL;
	g_autoptr(GError) error = NULL;
	gchar *line;

	/* skip if we already loaded data */
	if (priv->os_id != NULL)
		return;

	if (os_release_fname == NULL) {
		if (g_file_test ("/etc/os-release", G_FILE_TEST_EXISTS))
			os_release_fname = "/etc/os-release";
		else
			os_release_fname = "/usr/lib/os-release";
	}

	/* get details about the distribution we are running on */
	f = g_file_new_for_path (os_release_fname);
	if (g_file_query_exists (f, NULL)) {
		g_autoptr(GDataInputStream) dis = NULL;
		g_autoptr(GFileInputStream) fis = NULL;

		fis = g_file_read (f, NULL, &error);
		if (error != NULL) {
			g_warning ("Unable to read file '%s': %s",
				   os_release_fname, error->message);
			return;
		}
		dis = g_data_input_stream_new ((GInputStream*) fis);

		while ((line = g_data_input_stream_read_line (dis, NULL, NULL, &error)) != NULL) {
			g_auto(GStrv) data = NULL;
			g_autofree gchar *dvalue = NULL;
			if (error != NULL) {
				g_warning ("Unable to read line in file '%s': %s",
					   os_release_fname, error->message);
				g_free (line);
				return;
			}

			data = g_strsplit (line, "=", 2);
			if (g_strv_length (data) != 2) {
				g_free (line);
				continue;
			}

			dvalue = g_strdup (data[1]);
			if (g_str_has_prefix (dvalue, "\"")) {
				gchar *tmp;
				tmp = g_strndup (dvalue + 1, strlen(dvalue) - 2);
				g_free (dvalue);
				dvalue = tmp;
			}

			if (g_strcmp0 (data[0], "ID") == 0) {
				g_free (priv->os_id);
				priv->os_id = g_steal_pointer (&dvalue);

			} else if (g_strcmp0 (data[0], "NAME") == 0) {
				g_free (priv->os_name);
				priv->os_name = g_steal_pointer (&dvalue);

			} else if (g_strcmp0 (data[0], "VERSION_ID") == 0) {
				g_free (priv->os_version);
				priv->os_version = g_steal_pointer (&dvalue);

			} else if (g_strcmp0 (data[0], "HOME_URL") == 0) {
				g_free (priv->os_homepage);
				priv->os_homepage = g_steal_pointer (&dvalue);
			}

			g_free (line);
		}
	}
}

/**
 * as_system_info_get_os_id:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get the ID of the current operating system.
 *
 * Returns: the current OS ID.
 */
const gchar*
as_system_info_get_os_id (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_load_os_release (sysinfo, NULL);
	return priv->os_id;
}

/**
 * as_system_info_get_os_cid:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get the AppStream component ID of the current operating system.
 *
 * Returns: the component ID of the current OS.
 */
const gchar*
as_system_info_get_os_cid (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);

	as_system_info_load_os_release (sysinfo, NULL);
	if (priv->os_cid != NULL)
		return priv->os_cid;
	if (priv->os_homepage == NULL) {
		priv->os_cid = g_strdup (priv->os_id);
		return priv->os_cid;
	}

	priv->os_cid = as_utils_dns_to_rdns (priv->os_homepage, priv->os_id);
	if (priv->os_cid == NULL)
		return priv->os_id;
	return priv->os_cid;
}

/**
 * as_system_info_get_os_name:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get the humen-readable name of the current operating system.
 *
 * Returns: the name of the current OS.
 */
const gchar*
as_system_info_get_os_name (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_load_os_release (sysinfo, NULL);
	return priv->os_name;
}

/**
 * as_system_info_get_os_version:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get the version string of the current operating system.
 *
 * Returns: the version of the current OS.
 */
const gchar*
as_system_info_get_os_version (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_load_os_release (sysinfo, NULL);
	return priv->os_version;
}

/**
 * as_system_info_get_os_homepage:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get the homepage URL of the current operating system.
 *
 * Returns: the homepage of the current OS.
 */
const gchar*
as_system_info_get_os_homepage (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_load_os_release (sysinfo, NULL);
	return priv->os_homepage;
}

/**
 * as_system_info_read_kernel_details:
 */
static void
as_system_info_read_kernel_details (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	struct utsname utsbuf;
	gchar *tmp;

	if (priv->kernel_name != NULL)
		return;

	if (uname (&utsbuf) != 0) {
		g_warning ("Unable to read kernel information via uname: %s",
			   g_strerror (errno));
		return;
	}

	g_free (priv->kernel_name);
	priv->kernel_name = g_strdup (utsbuf.sysname);
	tmp = g_strrstr (utsbuf.release, "-");
	if (tmp != NULL)
		tmp[0] = '\0';

	g_free (priv->kernel_version);
	priv->kernel_version = g_strdup (utsbuf.release);
}

/**
 * as_system_info_get_kernel_name:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get the name of the current kernel, e.g. "Linux"
 *
 * Returns: the current OS kernel name
 */
const gchar*
as_system_info_get_kernel_name (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_read_kernel_details (sysinfo);
	return priv->kernel_name;
}

/**
 * as_system_info_get_kernel_version:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get the version of the current kernel, e.g. "6.2.0-2"
 *
 * Returns: the current kernel version
 */
const gchar*
as_system_info_get_kernel_version (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_read_kernel_details (sysinfo);
	return priv->kernel_version;
}

/**
 * as_system_info_set_kernel:
 * @sysinfo: a #AsSystemInfo instance.
 * @name: the kernel name.
 * @version: the kernel version.
 *
 * Override the kernel data.
 *
 * Internal API.
 */
void
as_system_info_set_kernel (AsSystemInfo *sysinfo, const gchar *name, const gchar *version)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_assign_string_safe (priv->kernel_name, name);
	as_assign_string_safe (priv->kernel_version, version);
}

/**
 * as_get_physical_memory_total:
 */
static gulong
as_get_physical_memory_total (void)
{
#if defined(__linux__)
	struct sysinfo si = { 0 };
	sysinfo (&si);
	if (si.mem_unit > 0)
		return (si.totalram * si.mem_unit) / MB_IN_BYTES;
	return 0;
#elif defined(__FreeBSD__)
	unsigned long physmem;
	sysctl ((int[]){ CTL_HW, HW_PHYSMEM }, 2, &physmem, &(size_t){ sizeof (physmem) }, NULL, 0);
	return physmem / MB_IN_BYTES;
#else
#error "Implementation of as_get_physical_memory_total() missing for this OS."
#endif
}

/**
 * as_system_info_get_memory_total:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get the current total amount of physical memory in MiB.
 *
 * Returns: the current total amount of usable memory in MiB
 */
gulong
as_system_info_get_memory_total (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	if (priv->memory_total == 0)
		priv->memory_total = as_get_physical_memory_total ();
	return priv->memory_total;
}

/**
 * as_system_info_set_memory_total:
 * @sysinfo: a #AsSystemInfo instance.
 * @size_mib: the size to set.
 *
 * Override the memory size.
 *
 * Internal API.
 */
void
as_system_info_set_memory_total (AsSystemInfo *sysinfo, gulong size_mib)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	priv->memory_total = size_mib;
}

/**
 * as_system_info_populate_modaliases_map_cb:
 */
static gboolean
as_system_info_populate_modaliases_map_cb (AsSystemInfo *sysinfo, const gchar *root_path)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir (root_path)) != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			if (ent->d_type == DT_LNK)
				continue;

			if (ent->d_type == DT_DIR) {
				g_autofree gchar *subdir_path = g_build_filename (root_path, ent->d_name, NULL);
				if (!as_str_equal0 (ent->d_name, ".") && !as_str_equal0 (ent->d_name, ".."))
					as_system_info_populate_modaliases_map_cb (sysinfo, subdir_path);
			} else {
				if (as_str_equal0 (ent->d_name, "modalias")) {
					gchar *contents = NULL;
					g_autoptr(GError) read_error = NULL;
					g_autofree gchar *path = g_build_filename (root_path, ent->d_name, NULL);

					if (!g_file_get_contents (path, &contents, NULL, &read_error)) {
						g_warning ("Error while reading modalias file %s: %s", path, read_error->message);
						closedir (dir);
						return FALSE;
					}
					contents = as_strstripnl (contents);
					g_hash_table_insert (priv->modalias_to_sysfs,
								contents,
								g_path_get_dirname (path));
				}
			}
		}
		closedir (dir);
	} else {
		g_warning ("Error while searching for modalias entries in %s: %s", root_path, g_strerror (errno));
	}

	return TRUE;
}

/**
 * as_system_info_populate_modaliases:
 */
static void
as_system_info_populate_modaliases (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	GHashTableIter ht_iter;
	gpointer ht_key;

	/* we never want to run this multiple times */
	if (priv->modaliases->len != 0)
		return;

	as_system_info_populate_modaliases_map_cb (sysinfo, "/sys/devices");
	g_hash_table_iter_init (&ht_iter, priv->modalias_to_sysfs);
	while (g_hash_table_iter_next (&ht_iter, &ht_key, NULL))
		g_ptr_array_add (priv->modaliases, ht_key);
}

/**
 * as_system_info_get_modaliases:
 * @sysinfo: a #AsSystemInfo instance.
 *
 * Get a list of modaliases for all the hardware on this system that has them.
 *
 * Returns: (transfer none) (element-type utf8): a list of modaliases on the system.
 */
GPtrArray*
as_system_info_get_modaliases (AsSystemInfo *sysinfo)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_populate_modaliases (sysinfo);
	return priv->modaliases;
}

/**
 * as_system_info_modalias_to_syspath:
 * @sysinfo: a #AsSystemInfo instance.
 * @modalias: the modalias value to resolve.
 *
 * Receive a path in /sys for the devices with the given modalias.
 *
 * Returns: the syspath, or %NULL if none was found.
 */
const gchar*
as_system_info_modalias_to_syspath (AsSystemInfo *sysinfo, const gchar *modalias)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_populate_modaliases (sysinfo);
	return g_hash_table_lookup (priv->modalias_to_sysfs, modalias);
}

/**
 * as_system_info_has_device_matching_modalias:
 * @sysinfo: a #AsSystemInfo instance.
 * @modalias_glob: the modalias value to to look for, may contain wildcards.
 *
 * Check if there is a device on this system that matches the given modalias glob.
 *
 * Returns: %TRUE if a matching device was found.
 */
gboolean
as_system_info_has_device_matching_modalias (AsSystemInfo *sysinfo, const gchar *modalias_glob)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	as_system_info_populate_modaliases (sysinfo);

	for (guint i = 0; i < priv->modaliases->len; i++) {
		const gchar *modalias = (const gchar*) g_ptr_array_index (priv->modaliases, i);
		if (g_strcmp0 (modalias, modalias_glob) == 0)
			return TRUE;

		if (fnmatch (modalias, modalias_glob, FNM_NOESCAPE) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * as_system_info_get_device_name_from_syspath:
 */
static gchar*
as_system_info_get_device_name_from_syspath (AsSystemInfo *sysinfo,
					     const gchar *syspath,
					     const gchar *modalias,
					     gboolean allow_fallback,
					     GError **error)
{
#ifdef HAVE_SYSTEMD
	__attribute__((cleanup (sd_device_unrefp))) sd_device *device = NULL;
	const gchar *key, *value;
	gint r;
	const gchar *device_vendor = NULL;
	const gchar *device_model = NULL;
	const gchar *usb_class = NULL;
	const gchar *driver = NULL;
	gchar *result = NULL;

	r = sd_device_new_from_syspath (&device, syspath);
        if (r < 0) {
		g_set_error (error,
				AS_SYSTEM_INFO_ERROR,
				AS_SYSTEM_INFO_ERROR_FAILED,
				"Failure to read device information for %s: %s",
				syspath, g_strerror (errno));
		return NULL;
	}

	for (key = sd_device_get_property_first (device, &value);
	     key;
             key = sd_device_get_property_next (device, &value)) {
		if (g_strstr_len (key, -1, "_VENDOR") != NULL) {
			if (g_strstr_len (key, -1, "VENDOR_ID") == NULL && !g_str_has_suffix (key, "_ENC"))
				device_vendor = value;
		} else if (g_strstr_len (key, -1, "_MODEL") != NULL) {
			if (g_strstr_len (key, -1, "MODEL_ID") == NULL && !g_str_has_suffix (key, "_ENC"))
				device_model = value;
		} else if (as_str_equal0 (key, "DRIVER"))
			driver = value;
		else if (g_strstr_len (key, -1, "_USB_CLASS"))
			usb_class = value;
	}

	if (device_vendor != NULL) {
		if (device_model != NULL)
			result = g_strdup_printf ("%s - %s", device_vendor, device_model);
		else if (usb_class != NULL)
			result = g_strdup_printf ("%s - %s", device_vendor, usb_class);
	}
	if (result == NULL) {
		if (allow_fallback) {
			if (driver != NULL)
				result = g_strdup (driver);
			else
				result = g_strdup (modalias);
		} else {
			g_set_error (error,
					AS_SYSTEM_INFO_ERROR,
					AS_SYSTEM_INFO_ERROR_NOT_FOUND,
					"Unable to find good human-readable description for device %s",
					modalias);
		}
	}

	return result;
#else
	g_set_error_literal (error,
				AS_SYSTEM_INFO_ERROR,
				AS_SYSTEM_INFO_ERROR_FAILED,
				"Unable to determine device name: AppStream was built without systemd-udevd support.");
	return NULL;
#endif
}

/**
 * as_system_info_get_device_name_from_hwdb:
 */
static gchar*
as_system_info_get_device_name_from_hwdb (AsSystemInfo *sysinfo, const gchar *modalias_glob, gboolean allow_fallback, GError **error)
{
#ifdef HAVE_SYSTEMD
		AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
		gint ret = 0;
		const gchar *device_vendor = NULL;
		const gchar *device_model = NULL;

		if (priv->hwdb == NULL) {
			ret = sd_hwdb_new(&priv->hwdb);
			if (ret < 0) {
				g_set_error (error,
					     AS_SYSTEM_INFO_ERROR,
					     AS_SYSTEM_INFO_ERROR_FAILED,
					     "Unable to open hardware database: %s",
					     g_strerror (ret));
				return NULL;
			}
		}

		sd_hwdb_get (priv->hwdb, modalias_glob, "ID_VENDOR_FROM_DATABASE", &device_vendor);
		sd_hwdb_get (priv->hwdb, modalias_glob, "ID_MODEL_FROM_DATABASE", &device_model);

		if (device_vendor != NULL && device_model != NULL)
			return g_strdup_printf ("%s - %s", device_vendor, device_model);
		if (allow_fallback)
			return g_strdup (modalias_glob);

		g_set_error (error,
				AS_SYSTEM_INFO_ERROR,
				AS_SYSTEM_INFO_ERROR_NOT_FOUND,
				"Unable to find good human-readable description for device %s",
				modalias_glob);
		return NULL;
#else
	g_set_error_literal (error,
				AS_SYSTEM_INFO_ERROR,
				AS_SYSTEM_INFO_ERROR_FAILED,
				"Unable to determine device name: AppStream was built without systemd-hwdb support.");
	return NULL;
#endif
}

/**
 * as_system_info_get_device_name_for_modalias:
 * @sysinfo: a #AsSystemInfo instance.
 * @modalias: the modalias value to resolve (may contain wildcards).
 * @allow_fallback: fall back to low-quality data if no better information is available
 * @error: a #GError
 *
 * Return a human readable device name for the given modalias.
 * Will return the modalias again if no device name could be found,
 * and returns %NULL on error.
 * If @allow_fallback is set to %FALSE, this function will return %NULL and error
 * %AS_SYSTEM_INFO_ERROR_NOT_FOUND in case no suitable description could be found.
 *
 * Returns: a human-readable device name, or %NULL on error.
 */
gchar*
as_system_info_get_device_name_for_modalias (AsSystemInfo *sysinfo, const gchar *modalias, gboolean allow_fallback, GError **error)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	const gchar *syspath;

	syspath = g_hash_table_lookup (priv->modalias_to_sysfs, modalias);
	if (syspath == NULL)
		return as_system_info_get_device_name_from_hwdb (sysinfo,
								 modalias,
								 allow_fallback,
								 error);
	else
		return as_system_info_get_device_name_from_syspath (sysinfo,
								    syspath,
								    modalias,
								    allow_fallback,
								    error);
}

/**
 * as_system_info_has_device_with_property:
 */
static AsCheckResult
as_system_info_has_device_with_property (AsSystemInfo *sysinfo, const gchar *prop_key, const gchar *prop_value, GError **error)
{
#ifdef HAVE_SYSTEMD
	__attribute__((cleanup (sd_device_enumerator_unrefp))) sd_device_enumerator *e = NULL;
	gint r;

	r = sd_device_enumerator_new (&e);
	if (r < 0) {
		g_set_error (error,
				AS_SYSTEM_INFO_ERROR,
				AS_SYSTEM_INFO_ERROR_FAILED,
				"Unable to enumerate devices: %s",
				g_strerror (r));
		return AS_CHECK_RESULT_ERROR;
	}
	r = sd_device_enumerator_allow_uninitialized (e);
	if (r < 0) {
		g_set_error (error,
				AS_SYSTEM_INFO_ERROR,
				AS_SYSTEM_INFO_ERROR_FAILED,
				"Unable to enumerate uninitialized devices: %s",
				g_strerror (r));
		return AS_CHECK_RESULT_ERROR;
	}
	r = sd_device_enumerator_add_match_property (e, prop_key, prop_value);
	if (r < 0) {
		g_set_error (error,
				AS_SYSTEM_INFO_ERROR,
				AS_SYSTEM_INFO_ERROR_FAILED,
				"Unable to enumerate devices by property: %s",
				g_strerror (r));
		return AS_CHECK_RESULT_ERROR;
	}

	return sd_device_enumerator_get_device_first (e) != NULL? AS_CHECK_RESULT_TRUE : AS_CHECK_RESULT_FALSE;
#else
	g_set_error_literal (error,
				AS_SYSTEM_INFO_ERROR,
				AS_SYSTEM_INFO_ERROR_FAILED,
				"Unable to look for input device: AppStream was built without systemd-udevd support.");
	return AS_CHECK_RESULT_ERROR;
#endif
}

/**
 * as_system_info_mark_input_control_status:
 *
 * Mark an input control as set to a specific value.
 */
static void
as_system_info_mark_input_control_status (AsSystemInfo *sysinfo, AsControlKind kind, gboolean found)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);

	as_flags_add (priv->tested_input_controls, (1 << kind));
	if (found)
		as_flags_add (priv->input_controls, (1 << kind));
}

/**
 * as_system_info_find_input_controls:
 */
static gboolean
as_system_info_find_input_controls (AsSystemInfo *sysinfo, GError **error)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	AsCheckResult res;

	/* skip scan if we have already tried it once */
	if (priv->inputs_scanned)
		return TRUE;

	/* console input is always present, unless the API user explicitly forbids it */
	as_system_info_mark_input_control_status (sysinfo, AS_CONTROL_KIND_CONSOLE, TRUE);
	priv->inputs_scanned = TRUE;

	/* autodetect all inputs we can */
	res = as_system_info_has_device_with_property (sysinfo, "ID_INPUT_KEYBOARD", "1", error);
	if (res == AS_CHECK_RESULT_ERROR)
		return FALSE;
	as_system_info_mark_input_control_status (sysinfo, AS_CONTROL_KIND_KEYBOARD, res == AS_CHECK_RESULT_TRUE);

	res = as_system_info_has_device_with_property (sysinfo, "ID_INPUT_MOUSE", "1", error);
	if (res == AS_CHECK_RESULT_ERROR)
		return FALSE;
	as_system_info_mark_input_control_status (sysinfo, AS_CONTROL_KIND_POINTING, res == AS_CHECK_RESULT_TRUE);
	if (res != AS_CHECK_RESULT_TRUE) {
		res = as_system_info_has_device_with_property (sysinfo, "ID_INPUT_TOUCHPAD", "1", error);
		if (res == AS_CHECK_RESULT_ERROR)
			return FALSE;
		as_system_info_mark_input_control_status (sysinfo, AS_CONTROL_KIND_POINTING, res == AS_CHECK_RESULT_TRUE);
	}

	res = as_system_info_has_device_with_property (sysinfo, "ID_INPUT_JOYSTICK", "1", error);
	if (res == AS_CHECK_RESULT_ERROR)
		return FALSE;
	as_system_info_mark_input_control_status (sysinfo, AS_CONTROL_KIND_GAMEPAD, res == AS_CHECK_RESULT_TRUE);

	res = as_system_info_has_device_with_property (sysinfo, "ID_INPUT_TABLET", "1", error);
	if (res == AS_CHECK_RESULT_ERROR)
		return FALSE;
	as_system_info_mark_input_control_status (sysinfo, AS_CONTROL_KIND_TABLET, res == AS_CHECK_RESULT_TRUE);

	res = as_system_info_has_device_with_property (sysinfo, "ID_INPUT_TOUCHSCREEN", "1", error);
	if (res == AS_CHECK_RESULT_ERROR)
		return FALSE;
	as_system_info_mark_input_control_status (sysinfo, AS_CONTROL_KIND_TOUCH, res == AS_CHECK_RESULT_TRUE);

	return TRUE;
}

/**
 * as_system_info_has_input_control:
 * @sysinfo: a #AsSystemInfo instance.
 * @kind: the #AsControlKind to test for.
 * @error: a #GError
 *
 * Test if the current system has a specific user input control method.
 * Returns %AS_CHECK_RESULT_UNKNOWN if we could not test for an input control method,
 * %AS_CHECK_RESULT_ERROR on error and %AS_CHECK_RESULT_FALSE if the control was not found.
 *
 * Returns: %AS_CHECK_RESULT_TRUE if control was found
 */
AsCheckResult
as_system_info_has_input_control (AsSystemInfo *sysinfo, AsControlKind kind, GError **error)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	g_return_val_if_fail (kind < AS_CONTROL_KIND_LAST, AS_CHECK_RESULT_UNKNOWN);
	g_return_val_if_fail (kind != AS_CONTROL_KIND_UNKNOWN, AS_CHECK_RESULT_UNKNOWN);

	if (!as_system_info_find_input_controls (sysinfo, error))
		return AS_CHECK_RESULT_ERROR;

	/* if we tried to autodetect and haven't found a device, return FALSE,
	 * but if we didn't even try to autodetect an input control, return UNKNOWN */

	if (as_flags_contains (priv->input_controls, (1 << kind)))
		return AS_CHECK_RESULT_TRUE;
	else if (!as_flags_contains (priv->tested_input_controls, (1 << kind)))
		return AS_CHECK_RESULT_UNKNOWN;
	return AS_CHECK_RESULT_FALSE;
}

/**
 * as_system_info_set_input_control:
 * @sysinfo: a #AsSystemInfo instance.
 * @kind: the #AsControlKind to set.
 * @found: %TRUE if the control should be marked as found.
 *
 * Explicitly mark a user input control as present or not present on this system.
 */
void
as_system_info_set_input_control (AsSystemInfo *sysinfo, AsControlKind kind, gboolean found)
{
	g_return_if_fail (kind < AS_CONTROL_KIND_LAST);
	g_return_if_fail (kind != AS_CONTROL_KIND_UNKNOWN);
	as_system_info_find_input_controls (sysinfo, NULL);
	as_system_info_mark_input_control_status (sysinfo, kind, found);
}

/**
 * as_system_info_get_display_length:
 * @sysinfo: a #AsSystemInfo instance.
 * @side: the #AsDisplaySideKind to select.
 *
 * Get the current display length for the given side kind.
 * If the display size is unknown, this function will return 0.
 *
 * Returns: the display size in logical pixels.
 */
gulong
as_system_info_get_display_length (AsSystemInfo *sysinfo, AsDisplaySideKind side)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	g_return_val_if_fail (side < AS_DISPLAY_SIDE_KIND_LAST, 0);
	g_return_val_if_fail (side != AS_DISPLAY_SIDE_KIND_UNKNOWN, 0);

	if (side == AS_DISPLAY_SIDE_KIND_LONGEST)
		return priv->display_length_longest;
	return priv->display_length_shortest;
}

/**
 * as_system_info_set_display_length:
 * @sysinfo: a #AsSystemInfo instance.
 * @side: the #AsDisplaySideKind to select.
 * @value_dip: the length value in device-independt pixels.
 *
 * Set the current display length for the given side kind.
 * The size needs to be in device-independent pixels, see the
 * AppStream documentation for more information:
 * https://freedesktop.org/software/appstream/docs/chap-Metadata.html#tag-relations-display_length
 */
void
as_system_info_set_display_length (AsSystemInfo *sysinfo, AsDisplaySideKind side, gulong value_dip)
{
	AsSystemInfoPrivate *priv = GET_PRIVATE (sysinfo);
	g_return_if_fail (side < AS_DISPLAY_SIDE_KIND_LAST);
	g_return_if_fail (side != AS_DISPLAY_SIDE_KIND_UNKNOWN);

	if (side == AS_DISPLAY_SIDE_KIND_LONGEST)
		priv->display_length_longest = value_dip;
	priv->display_length_shortest = value_dip;
}

/**
 * as_system_info_new:
 *
 * Creates a new #AsSystemInfo.
 *
 * Returns: (transfer full): a #AsSystemInfo
 *
 * Since: 0.10
 **/
AsSystemInfo*
as_system_info_new (void)
{
	AsSystemInfo *sysinfo;
	sysinfo = g_object_new (AS_TYPE_SYSTEM_INFO, NULL);
	return AS_SYSTEM_INFO (sysinfo);
}

/**
 * as_get_current_distro_component_id:
 *
 * Returns the component-ID of the current distribution based on contents
 * of the `/etc/os-release` file.
 * This function is a shorthand for %as_distro_details_get_cid
 */
gchar*
as_get_current_distro_component_id (void)
{
	g_autoptr(AsSystemInfo) sysinfo = as_system_info_new ();
	return g_strdup (as_system_info_get_os_cid (sysinfo));

}
