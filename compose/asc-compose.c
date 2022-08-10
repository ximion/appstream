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
 * SECTION:asc-compose
 * @short_description: Compose collection metadata easily.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-compose.h"

#include <errno.h>
#include <glib/gi18n.h>

#include "as-utils-private.h"
#include "as-yaml.h"
#include "as-curl.h"

#include "asc-globals-private.h"
#include "asc-utils.h"
#include "asc-hint.h"
#include "asc-result.h"

#include "asc-utils-metainfo.h"
#include "asc-utils-l10n.h"
#include "asc-utils-screenshots.h"
#include "asc-utils-fonts.h"
#include "asc-image.h"

typedef struct
{
	GPtrArray	*units;
	GPtrArray	*results;
	AscUnit		*locale_unit;

	GHashTable	*allowed_cids;
	GRefString	*prefix;
	GRefString	*origin;
	gchar		*media_baseurl;
	AsFormatKind	format;
	guint		min_l10n_percentage;
	GPtrArray	*custom_allowed;
	gssize		max_scr_size_bytes;
	gchar		*cainfo;

	AscComposeFlags	flags;
	AscIconPolicy	*icon_policy;

	gchar		*data_result_dir;
	gchar		*icons_result_dir;
	gchar		*media_result_dir;
	gchar		*hints_result_dir;

	GHashTable	*known_cids;
	GMutex		mutex;

	AscCheckMetadataEarlyFn check_md_early_fn;
	gpointer	check_md_early_fn_udata;

	AscTranslateDesktopTextFn de_l10n_fn;
	gpointer	de_l10n_fn_udata;
} AscComposePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscCompose, asc_compose, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_compose_get_instance_private (o))

static void
asc_compose_init (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);

	priv->units = g_ptr_array_new_with_free_func (g_object_unref);
	priv->results = g_ptr_array_new_with_free_func (g_object_unref);
	priv->allowed_cids = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    NULL);
	priv->known_cids = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  NULL);
	priv->custom_allowed = g_ptr_array_new_with_free_func (g_free);
	g_mutex_init (&priv->mutex);

	/* defaults */
	priv->format = AS_FORMAT_KIND_XML;
	as_ref_string_assign_safe (&priv->prefix, "/usr");
	priv->min_l10n_percentage = 25;
	priv->max_scr_size_bytes = -1;
	priv->flags = ASC_COMPOSE_FLAG_USE_THREADS |
			ASC_COMPOSE_FLAG_ALLOW_NET |
			ASC_COMPOSE_FLAG_VALIDATE |
			ASC_COMPOSE_FLAG_STORE_SCREENSHOTS |
			ASC_COMPOSE_FLAG_ALLOW_SCREENCASTS |
			ASC_COMPOSE_FLAG_PROCESS_FONTS |
			ASC_COMPOSE_FLAG_PROCESS_TRANSLATIONS;

	/* the icon policy will initialize with default settings */
	priv->icon_policy = asc_icon_policy_new ();
}

static void
asc_compose_finalize (GObject *object)
{
	AscCompose *compose = ASC_COMPOSE (object);
	AscComposePrivate *priv = GET_PRIVATE (compose);

	g_ptr_array_unref (priv->units);
	g_ptr_array_unref (priv->results);
	g_ptr_array_unref (priv->custom_allowed);
	g_free (priv->cainfo);

	g_hash_table_unref (priv->allowed_cids);
	g_hash_table_unref (priv->known_cids);
	as_ref_string_release (priv->prefix);
	as_ref_string_release (priv->origin);
	g_free (priv->media_baseurl);

	g_free (priv->data_result_dir);
	g_free (priv->icons_result_dir);
	g_free (priv->media_result_dir);
	g_free (priv->hints_result_dir);

	if (priv->locale_unit != NULL)
		g_object_unref (priv->locale_unit);

	g_mutex_clear (&priv->mutex);

	G_OBJECT_CLASS (asc_compose_parent_class)->finalize (object);
}

static void
asc_compose_class_init (AscComposeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_compose_finalize;
}

/**
 * asc_compose_reset:
 * @compose: an #AscCompose instance.
 *
 * Reset the results, units and run-specific settings so the
 * instance can be reused for another metadata generation run.
 **/
void
asc_compose_reset (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	g_hash_table_remove_all (priv->allowed_cids);
	g_ptr_array_set_size (priv->units, 0);
	g_ptr_array_set_size (priv->results, 0);
	g_hash_table_remove_all (priv->known_cids);
}

/**
 * asc_compose_add_unit:
 * @compose: an #AscCompose instance.
 * @unit: The #AscUnit to add
 *
 * Add an #AscUnit as data source for metadata processing.
 **/
void
asc_compose_add_unit (AscCompose *compose, AscUnit *unit)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	/* sanity check */
	for (guint i = 0; i < priv->units->len; i++) {
		if (unit == g_ptr_array_index (priv->units, i)) {
			g_critical ("Not adding unit duplicate for processing!");
			return;
		}
	}
	g_ptr_array_add (priv->units,
			 g_object_ref (unit));
}

/**
 * asc_compose_add_allowed_cid:
 * @compose: an #AscCompose instance.
 * @component_id: The component-id to whitelist
 *
 * Adds a component ID to the allowlist. If the list is not empty, only
 * components in the list will be added to the metadata output.
 **/
void
asc_compose_add_allowed_cid (AscCompose *compose, const gchar *component_id)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	g_hash_table_add (priv->allowed_cids,
			  g_strdup (component_id));
}

/**
 * asc_compose_get_prefix:
 * @compose: an #AscCompose instance.
 *
 * Get the directory prefix used for processing.
 */
const gchar*
asc_compose_get_prefix (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->prefix;
}

/**
 * asc_compose_set_prefix:
 * @compose: an #AscCompose instance.
 * @prefix: a directory prefix, e.g. "/usr"
 *
 * Set the directory prefix the to-be-processed units are using.
 */
void
asc_compose_set_prefix (AscCompose *compose, const gchar *prefix)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	/* do a bit of sanitizing: "no prefix" means the prefix directory is the root directory */
	if (prefix == NULL || g_strcmp0 (prefix, "") == 0)
		as_ref_string_assign_safe (&priv->prefix, "/");
	else
		as_ref_string_assign_safe (&priv->prefix, prefix);
}

/**
 * asc_compose_get_origin:
 * @compose: an #AscCompose instance.
 *
 * Get the metadata origin field.
 */
const gchar*
asc_compose_get_origin (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->origin;
}

/**
 * asc_compose_set_origin:
 * @compose: an #AscCompose instance.
 * @origin: the origin.
 *
 * Set the metadata origin field (e.g. "debian" or "flathub")
 */
void
asc_compose_set_origin (AscCompose *compose, const gchar *origin)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	g_autofree gchar *tmp = NULL;
	tmp = g_markup_escape_text (origin, -1);
	as_ref_string_assign_safe (&priv->origin, tmp);
}

/**
 * asc_compose_get_format:
 * @compose: an #AscCompose instance.
 *
 * get the format type we are generating.
 */
AsFormatKind
asc_compose_get_format (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->format;
}

/**
 * asc_compose_set_format:
 * @compose: an #AscCompose instance.
 * @kind: The format, e.g. %AS_FORMAT_KIND_XML
 *
 * Set the format kind of the collection metadata that we should generate.
 */
void
asc_compose_set_format (AscCompose *compose, AsFormatKind kind)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	priv->format = kind;
}

/**
 * asc_compose_get_media_baseurl:
 * @compose: an #AscCompose instance.
 *
 * Get the media base URL to be used for the generated data,
 * or %NULL if this feature is not used.
 */
const gchar*
asc_compose_get_media_baseurl (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->media_baseurl;
}

/**
 * asc_compose_set_media_baseurl:
 * @compose: an #AscCompose instance.
 * @url: (nullable): the media base URL.
 *
 * Set the media base URL for the generated metadata. Can be %NULL.
 */
void
asc_compose_set_media_baseurl (AscCompose *compose, const gchar *url)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	as_assign_string_safe (priv->media_baseurl, url);
}

/**
 * asc_compose_get_flags:
 * @compose: an #AscCompose instance.
 *
 * Get the flags controlling compose behavior.
 */
AscComposeFlags
asc_compose_get_flags (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->flags;
}

/**
 * asc_compose_set_flags:
 * @compose: an #AscCompose instance.
 * @flags: The compose flags bitfield.
 *
 * Set compose flags bitfield that controls the enabled features
 * for this #AscCompose.
 */
void
asc_compose_set_flags (AscCompose *compose, AscComposeFlags flags)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	priv->flags = flags;
}

/**
 * asc_compose_add_flags:
 * @compose: an #AscCompose instance.
 * @flags: The compose flags to add.
 *
 * Add compose flags.
 */
void
asc_compose_add_flags (AscCompose *compose, AscComposeFlags flags)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	as_flags_add (priv->flags, flags);
}

/**
 * asc_compose_remove_flags:
 * @compose: an #AscCompose instance.
 * @flags: The compose flags to remove.
 *
 * Remove compose flags.
 */
void
asc_compose_remove_flags (AscCompose *compose, AscComposeFlags flags)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	as_flags_remove (priv->flags, flags);
}

/**
 * asc_compose_get_icon_policy:
 * @compose: an #AscCompose instance.
 *
 * Get the policy for how icons should be distributed to
 * any AppStream clients.
 *
 * Returns: (transfer none): an #AscIconPolicy
 */
AscIconPolicy*
asc_compose_get_icon_policy (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->icon_policy;
}

/**
 * asc_compose_set_icon_policy:
 * @compose: an #AscCompose instance.
 * @policy: (not nullable): an #AscIconPolicy instance
 *
 * Set an icon policy object, overriding the existing one.
 */
void
asc_compose_set_icon_policy (AscCompose *compose, AscIconPolicy *policy)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_return_if_fail (policy != NULL);

	g_object_unref (priv->icon_policy);
	priv->icon_policy = g_object_ref (policy);
}

/**
 * asc_compose_get_cainfo:
 * @compose: an #AscCompose instance.
 *
 * Get the CA file used to verify peers with, or %NULL for default.
 */
const gchar*
asc_compose_get_cainfo (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->cainfo;
}

/**
 * asc_compose_set_cainfo:
 * @compose: an #AscCompose instance.
 * @cainfo: a valid file path
 *
 * Set a CA file holding one or more certificates to verify peers with
 * for download operations performed by this #AscCompose.
 */
void
asc_compose_set_cainfo (AscCompose *compose, const gchar *cainfo)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	as_assign_string_safe (priv->cainfo, cainfo);
}

/**
 * asc_compose_get_data_result_dir:
 * @compose: an #AscCompose instance.
 *
 * Get the data result directory.
 */
const gchar*
asc_compose_get_data_result_dir (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->data_result_dir;
}

/**
 * asc_compose_set_data_result_dir:
 * @compose: an #AscCompose instance.
 * @dir: the metadata save location.
 *
 * Set an output location where generated metadata should be saved.
 * If this is set to %NULL, no metadata will be saved.
 */
void
asc_compose_set_data_result_dir (AscCompose *compose, const gchar *dir)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	as_assign_string_safe (priv->data_result_dir, dir);
}

/**
 * asc_compose_get_icons_result_dir:
 * @compose: an #AscCompose instance.
 *
 * Get the icon result directory.
 */
const gchar*
asc_compose_get_icons_result_dir (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->icons_result_dir;
}

/**
 * asc_compose_set_icons_result_dir:
 * @compose: an #AscCompose instance.
 * @dir: the icon storage location.
 *
 * Set an output location where plain icons for the processed metadata
 * are stored.
 */
void
asc_compose_set_icons_result_dir (AscCompose *compose, const gchar *dir)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	as_assign_string_safe (priv->icons_result_dir, dir);
}

/**
 * asc_compose_get_media_result_dir:
 * @compose: an #AscCompose instance.
 *
 * Get the media result directory, that can be served on a webserver.
 */
const gchar*
asc_compose_get_media_result_dir (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->media_result_dir;
}

/**
 * asc_compose_set_media_result_dir:
 * @compose: an #AscCompose instance.
 * @dir: the media storage location.
 *
 * Set an output location to store media (screenshots, icons, ...) that
 * will be served on a webserver via the URL set as media baseurl.
 */
void
asc_compose_set_media_result_dir (AscCompose *compose, const gchar *dir)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	as_assign_string_safe (priv->media_result_dir, dir);
}

/**
 * asc_compose_get_hints_result_dir:
 * @compose: an #AscCompose instance.
 *
 * Get hints report output directory.
 */
const gchar*
asc_compose_get_hints_result_dir (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->hints_result_dir;
}

/**
 * asc_compose_set_hints_result_dir:
 * @compose: an #AscCompose instance.
 * @dir: the hints data directory.
 *
 * Set an output location for HTML reports of issues generated
 * during a compose run.
 */
void
asc_compose_set_hints_result_dir (AscCompose *compose, const gchar *dir)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	as_assign_string_safe (priv->hints_result_dir, dir);
}

/**
 * asc_compose_remove_custom_allowed:
 * @compose: an #AscCompose instance.
 * @key_id: the custom key to drop from the allowed list.
 *
 * Remove a key from the allowlist used to filter the `<custom/>` tag entries.
 */
void
asc_compose_remove_custom_allowed (AscCompose *compose, const gchar *key_id)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	for (guint i = 0; i < priv->custom_allowed->len; i++) {
		if (g_strcmp0 (g_ptr_array_index (priv->custom_allowed, i), key_id) == 0) {
			g_ptr_array_remove_index_fast (priv->custom_allowed, i);
			break;
		}
	}
}

/**
 * asc_compose_add_custom_allowed:
 * @compose: an #AscCompose instance.
 * @key_id: the custom key to add to the allowed list.
 *
 * Add a key to the allowlist that is used to filter custom tag values.
 */
void
asc_compose_add_custom_allowed (AscCompose *compose, const gchar *key_id)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	g_ptr_array_add (priv->custom_allowed, g_strdup (key_id));
}

/**
 * asc_compose_get_max_screenshot_size:
 * @compose: an #AscCompose instance.
 *
 * Get the maximum size a screenshot video or image can have.
 * A size < 0 may be returned for no limit, setting a limit of 0
 * will disable screenshots.
 */
gssize
asc_compose_get_max_screenshot_size (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->max_scr_size_bytes;
}

/**
 * asc_compose_set_max_screenshot_size:
 * @compose: an #AscCompose instance.
 * @size_bytes: maximum size of a screenshot image or video in bytes
 *
 * Set the maximum size a screenshot video or image can have.
 * A size < 0 may be set to allow unlimited sizes, setting a limit of 0
 * will disable screenshot caching entirely.
 */
void
asc_compose_set_max_screenshot_size (AscCompose *compose, gssize size_bytes)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	priv->max_scr_size_bytes = size_bytes;
}

/**
 * asc_compose_set_check_metadata_early_func:
 * @compose: an #AscCompose instance.
 * @func: (scope notified): the #AscCheckMetainfoLoadResultFn function to be called
 * @user_data: user data for @func
 *
 * Set an custom callback to be run when most of the metadata has been loaded,
 * but no expensive operations (like downloads or icon rendering) have been done yet.
 * This can be used to ignore unwanted components early on.
 *
 * The callback function may be called from any thread, so it needs to ensure thread safety on its own.
 */
void
asc_compose_set_check_metadata_early_func (AscCompose *compose, AscCheckMetadataEarlyFn func, gpointer user_data)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	priv->check_md_early_fn = func;
	priv->check_md_early_fn_udata = user_data;
}

/**
 * asc_compose_set_desktop_entry_l10n_func:
 * @compose: an #AscCompose instance.
 * @func: (scope notified): the #AscTranslateDesktopTextFn function to be called
 * @user_data: user data for @func
 *
 * Set a custom desktop-entry field localization functions to be run for specialized
 * desktop-entry localization schemes such as used in Ubuntu.
 *
 * The callback function may be called from any thread, so it needs to ensure thread safety on its own.
 */
void
asc_compose_set_desktop_entry_l10n_func (AscCompose *compose, AscTranslateDesktopTextFn func, gpointer user_data)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	priv->de_l10n_fn = func;
	priv->de_l10n_fn_udata = user_data;
}

/**
 * asc_compose_get_locale_unit:
 * @compose: an #AscCompose instance.
 *
 * Get the unit we use for locale processing
 *
 * Return: (transfer none) (nullable): The unit used for locale processing, or %NULL for default.
 */
AscUnit*
asc_compose_get_locale_unit (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->locale_unit;
}

/**
 * asc_compose_set_locale_unit:
 * @compose: an #AscCompose instance.
 * @locale_unit: the unit used for locale processing.
 *
 * Set a specific unit that is used for fetching locale information.
 * This may be useful in case a special language pack layout is used,
 * but is generally not necessary to be set explicitly, as locale
 * will be found in the unit where the metadata is by default.
 */
void
asc_compose_set_locale_unit (AscCompose *compose, AscUnit *locale_unit)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	if (priv->locale_unit == locale_unit)
		return;
	if (priv->locale_unit != NULL)
		g_object_unref (priv->locale_unit);
	priv->locale_unit = g_object_ref (locale_unit);
}

/**
 * asc_compose_get_results:
 * @compose: an #AscCompose instance.
 *
 * Get the results of the last processing run.
 *
 * Returns: (transfer none) (element-type AscResult): The results
 */
GPtrArray*
asc_compose_get_results (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	return priv->results;
}

/**
 * asc_compose_fetch_components:
 * @compose: an #AscCompose instance.
 *
 * Get the results components extracted in the last data processing run.
 *
 * Returns: (transfer container) (element-type AsComponent): The components
 */
GPtrArray*
asc_compose_fetch_components (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	GPtrArray *cpts_result = g_ptr_array_new_with_free_func (g_object_unref);

	for (guint i = 0; i < priv->results->len; i++) {
		g_autoptr(GPtrArray) cpts = NULL;
		AscResult *res = ASC_RESULT (g_ptr_array_index (priv->results, i));
		cpts = asc_result_fetch_components (res);
		for (guint j = 0; j < cpts->len; j++)
			g_ptr_array_add (cpts_result,
					 g_object_ref (AS_COMPONENT (g_ptr_array_index (cpts, j))));
	}

	return cpts_result;
}

/**
 * asc_compose_has_errors:
 * @compose: an #AscCompose instance.
 *
 * Check if the last run generated any errors (which will cause metadata to be ignored).
 *
 * Returns: %TRUE if we had errors.
 */
gboolean
asc_compose_has_errors (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);

	for (guint i = 0; i < priv->results->len; i++) {
		g_autoptr(GPtrArray) hints = NULL;
		AscResult *res = ASC_RESULT (g_ptr_array_index (priv->results, i));
		hints = asc_result_fetch_hints_all (res);
		for (guint j = 0; j < hints->len; j++) {
			AscHint *hint = ASC_HINT (g_ptr_array_index (hints, j));
			if (asc_hint_is_error (hint))
				return TRUE;
		}
	}

	return FALSE;
}

typedef struct {
	AscUnit		*unit;
	AscResult	*result;
	GHashTable	*files_units_map;	/* no ref */
} AscComposeTask;

static AscComposeTask*
asc_compose_task_new (AscUnit *unit)
{
	AscComposeTask *ctask;
	ctask = g_new0 (AscComposeTask, 1);
	ctask->unit = g_object_ref (unit);
	ctask->result = asc_result_new ();
	return ctask;
}

static void
asc_compose_task_free (AscComposeTask *ctask)
{
	g_object_unref (ctask->unit);
	g_object_unref (ctask->result);
	g_free (ctask);
}

/**
 * asc_compose_find_icon_filename:
 */
static gchar*
asc_compose_find_icon_filename (AscCompose *compose,
				AscUnit *unit,
				const gchar *icon_name,
				guint icon_size,
				guint icon_scale)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	guint min_size_idx = 0;
	guint min_ext_idx = 0;
	gboolean vector_relaxed = FALSE;
	const gchar *supported_ext[] = { ".png",
					 ".svg",
					 ".svgz",
					 "",
					 NULL };
	const struct {
		guint size;
		const gchar *size_str;
	} sizes[] =  {
		{ 48,  "48x48" },
		{ 32,  "32x32" },
		{ 64,  "64x64" },
		{ 96,  "96x96" },
		{ 128, "128x128" },
		{ 256, "256x256" },
		{ 512, "512x512" },
		{ 0,   "scalable" },
		{ 0,   NULL }
	};
	const gchar *types[] = { "actions",
				 "apps",
				 "applets",
				 "categories",
				 "devices",
				 "emblems",
				 "emotes",
				 "filesystems",
				 "mimetypes",
				 "places",
				 "preferences",
				 "status",
				 "stock",
				 NULL };

	g_return_val_if_fail (icon_name != NULL, NULL);

	/* fallbacks & sanitizations */
	if (icon_scale <= 0)
		icon_scale = 1;
	if (icon_size > 512)
		icon_size = 512;

	/* is this an absolute path */
	if (icon_name[0] == '/') {
		g_autofree gchar *tmp = NULL;
		tmp = g_build_filename (priv->prefix, icon_name, NULL);
		if (!asc_unit_file_exists (unit, tmp))
			return NULL;
		return g_strdup (tmp);
	}

	/* select minimum size */
	for (guint i = 0; sizes[i].size_str != NULL; i++) {
		if (sizes[i].size >= icon_size) {
			min_size_idx = i;
			break;
		}
	}

	while (TRUE) {
		/* hicolor icon theme search */
		for (guint i = min_size_idx; sizes[i].size_str != NULL; i++) {
			g_autofree gchar *size = NULL;
			if (icon_scale == 1)
				size = g_strdup (sizes[i].size_str);
			else
				size = g_strdup_printf ("%s@%i", sizes[i].size_str, icon_scale);
			for (guint m = 0; types[m] != NULL; m++) {
				for (guint j = min_ext_idx; supported_ext[j] != NULL; j++) {
					g_autofree gchar *tmp = NULL;
					tmp = g_strdup_printf ("%s/share/icons/"
								"hicolor/%s/%s/%s%s",
								priv->prefix,
								size,
								types[m],
								icon_name,
								supported_ext[j]);
					if (asc_unit_file_exists (unit, tmp))
						return g_strdup (tmp);
				}
			}
		}

		/* breeze icon theme search, for KDE Plasma compatibility */
		for (guint i = min_size_idx; sizes[i].size_str != NULL; i++) {
			g_autofree gchar *size = NULL;
			if (icon_scale == 1)
				size = g_strdup_printf ("%i", sizes[i].size);
			else
				size = g_strdup_printf ("%i@%i", sizes[i].size, icon_scale);
			for (guint m = 0; types[m] != NULL; m++) {
				for (guint j = min_ext_idx; supported_ext[j] != NULL; j++) {
					g_autofree gchar *tmp = NULL;
					tmp = g_strdup_printf ("%s/share/icons/"
								"breeze/%s/%s/%s%s",
								priv->prefix,
								types[m],
								size,
								icon_name,
								supported_ext[j]);
					if (asc_unit_file_exists (unit, tmp))
						return g_strdup (tmp);
				}
			}
		}

		if (vector_relaxed) {
			break;
		} else {
			if (g_str_has_suffix (icon_name, ".png"))
				break;
			/* try again, searching for vector graphics that we can scale up */
			vector_relaxed = TRUE;
			min_size_idx = 0;
			/* start at index 1, where the SVG icons are */
			min_ext_idx = 1;
		}
	}

	/* failed to find any icon */
	return NULL;
}

static void
asc_compose_process_icons (AscCompose *compose,
			   AscResult *cres,
			   AsComponent *cpt,
			   AscUnit *unit,
			   const gchar *icon_export_dir)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	GPtrArray *icons = NULL;
	g_autoptr(AsIcon) stock_icon = NULL;
	const gchar *icon_name = NULL;
	AscIconPolicyIter iter;
	guint size;
	guint scale_factor;
	AscIconState icon_state;

	/* do nothing if we have no icons to process */
	icons = as_component_get_icons (cpt);
	if (icons == NULL || icons->len == 0)
		return;

	/* find a suitable stock icon as template */
	for (guint i = 0; i < icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
			stock_icon = icon;
			break;
		}

		/* we cheat here to accomodate for apps which used the "local" icon type wrong */
		if (as_icon_get_kind (icon) == AS_ICON_KIND_LOCAL)
			stock_icon = icon;
	}
	/* drop all preexisting icons */
	if (stock_icon != NULL)
		stock_icon = g_object_ref (stock_icon);
	g_ptr_array_set_size (as_component_get_icons (cpt), 0);

	if (stock_icon == NULL) {
		asc_result_add_hint_simple (cres, cpt, "no-stock-icon");
		return;
	}
	icon_name = as_icon_get_name (stock_icon);
	if (as_is_empty (icon_name) || icon_name[0] == ' ') {
		/* and invalid stock icon is like having none at all */
		asc_result_add_hint_simple (cres, cpt, "no-stock-icon");
		return;
	}

	asc_icon_policy_iter_init (&iter, priv->icon_policy);
	while (asc_icon_policy_iter_next (&iter, &size, &scale_factor, &icon_state)) {
		g_autofree gchar *icon_fname = NULL;
		g_autofree gchar *res_icon_fname = NULL;
		g_autofree gchar *res_icon_size_str = NULL;
		g_autofree gchar *res_icon_sizedir = NULL;
		g_autofree gchar *res_icon_basename = NULL;
		g_autoptr(AscImage) img = NULL;
		g_autoptr(AsIcon) icon = NULL;
		g_autoptr(GBytes) img_bytes = NULL;
		gboolean is_vector_icon = FALSE;
		const void *img_data;
		gsize img_len;
		g_autoptr(GError) error = NULL;

		/* skip icon if its size should be skipped */
		if (icon_state == ASC_ICON_STATE_IGNORED)
			continue;

		icon_fname = asc_compose_find_icon_filename (compose,
								unit,
								icon_name,
								size,
								scale_factor);

		if (icon_fname == NULL) {
			/* only a 64x64px icon is mandatory, everything else is optional */
			if (size == 64 && scale_factor == 1) {
				asc_result_add_hint (cres, cpt,
							"icon-not-found",
							"icon_fname", icon_name,
							NULL);
				return;
			}
			continue;
		}

		is_vector_icon = g_str_has_suffix (icon_fname, ".svgz") || g_str_has_suffix (icon_fname, ".svg");
		img_bytes = asc_unit_read_data (unit, icon_fname, &error);
		if (img_bytes == NULL) {
			asc_result_add_hint (cres, cpt,
						"file-read-error",
						"fname", icon_fname,
						"msg", error->message,
						NULL);
			return;
		}
		img_data = g_bytes_get_data (img_bytes, &img_len);
		img = asc_image_new_from_data (img_data, img_len,
						is_vector_icon? size * scale_factor : 0,
						g_str_has_suffix (icon_fname, ".svgz"),
						ASC_IMAGE_LOAD_FLAG_ALWAYS_RESIZE,
						&error);
		if (img == NULL) {
			asc_result_add_hint (cres, cpt,
						"file-read-error",
						"fname", icon_fname,
						"msg", error->message,
						NULL);
			return;
		}

		/* we only take exact-ish size matches for 48x48px */
		if (size == 48 && asc_image_get_width (img) > 48)
			continue;

		res_icon_size_str = (scale_factor == 1)?
					g_strdup_printf ("%ix%i",
							 size, size)
					: g_strdup_printf ("%ix%i@%i",
							   size, size,
							   scale_factor);
		res_icon_sizedir = g_build_filename (icon_export_dir, res_icon_size_str, NULL);

		g_mkdir_with_parents (res_icon_sizedir, 0755);
		res_icon_basename = g_strdup_printf ("%s.png", as_component_get_id (cpt));
		res_icon_fname = g_build_filename (res_icon_sizedir,
							res_icon_basename,
							NULL);

		/* scale & save the image */
		g_debug ("Saving icon: %s", res_icon_fname);
		if (!asc_image_save_filename (img,
						res_icon_fname,
						size * scale_factor, size * scale_factor,
						ASC_IMAGE_SAVE_FLAG_OPTIMIZE,
						&error)) {
			asc_result_add_hint (cres, cpt,
						"icon-write-error",
						"fname", icon_fname,
						"msg", error->message,
						NULL);
			return;
		}

		/* create a remote reference if we have data for it */
		if (priv->media_result_dir != NULL && icon_state != ASC_ICON_STATE_CACHED_ONLY) {
			g_autofree gchar *icons_media_urlpart_dir = NULL;
			g_autofree gchar *icon_media_urlpart_fname = NULL;
			g_autofree gchar *icons_media_path = NULL;
			g_autofree gchar *icon_media_fname = NULL;
			g_autoptr(AsIcon) remote_icon = NULL;
			icons_media_urlpart_dir = g_strdup_printf ("%s/%s/%s",
								   asc_result_gcid_for_component (cres, cpt),
								   "icons",
								   res_icon_size_str);
			icon_media_urlpart_fname = g_strdup_printf ("%s/%s",
								     icons_media_urlpart_dir,
								     res_icon_basename);
			icons_media_path = g_build_filename (priv->media_result_dir,
								icons_media_urlpart_dir,
								NULL);
			icon_media_fname = g_build_filename (icons_media_path,
								res_icon_basename,
								NULL);
			g_mkdir_with_parents (icons_media_path, 0755);

			g_debug ("Adding media pool icon: %s", icon_media_fname);
			if (!as_copy_file (res_icon_fname, icon_media_fname, &error)) {
				g_warning ("Unable to write media pool icon: %s", icon_media_fname);
				asc_result_add_hint (cres, cpt,
						     "icon-write-error",
						     "fname", icon_fname,
						     "msg", error->message,
						     NULL);
				return;
			}

			/* add remote icon to metadata */
			remote_icon = as_icon_new ();
			as_icon_set_kind (remote_icon, AS_ICON_KIND_REMOTE);
			as_icon_set_width (remote_icon, size);
			as_icon_set_height (remote_icon, size);
			as_icon_set_scale (remote_icon, scale_factor);

			/* We can only make use of the media-baseurl-using partial URLs if screenshot storage
			 * is also enabled, because otherwise screenshots will use full URLs which conflicts
			 * with the media baseurl (as it is unconditionally prefixed to *all* media URLs */
			if (as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_STORE_SCREENSHOTS)) {
				as_icon_set_url (remote_icon, icon_media_urlpart_fname);
			} else {
				g_autofree gchar *icon_remote_url = g_strconcat (priv->media_baseurl, "/", icon_media_urlpart_fname, NULL);
				/* if priv->media_result_dir is set, media_baseurl will be set too (checked before each run) */
				as_icon_set_url (remote_icon, icon_remote_url);
			}

			as_component_add_icon (cpt, remote_icon);
		}

		/* add icon to metadata */
		if (icon_state != ASC_ICON_STATE_REMOTE_ONLY) {
			icon = as_icon_new ();
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
			as_icon_set_width (icon, size);
			as_icon_set_height (icon, size);
			as_icon_set_scale (icon, scale_factor);
			as_icon_set_name (icon, res_icon_basename);
			as_component_add_icon (cpt, icon);
		}
	}

	/* fix some stock icon mistakes and add the stock icon back */
	if (as_icon_get_kind (stock_icon) == AS_ICON_KIND_STOCK) {
		g_autofree gchar *tmp = NULL;
		gsize icon_name_len = strlen (icon_name);
		tmp = g_strdup (icon_name);

		if (g_str_has_suffix (icon_name, ".png") || g_str_has_suffix (icon_name, ".svg"))
			tmp[icon_name_len - 4] = '\0';
		else if (g_str_has_suffix (icon_name, ".svgz"))
			tmp[icon_name_len - 5] = '\0';

		as_icon_set_width (stock_icon, 0);
		as_icon_set_height (stock_icon, 0);
		as_icon_set_scale (stock_icon, 0);
		as_icon_set_name (stock_icon, tmp);
		as_component_add_icon (cpt, stock_icon);
	}

}

/**
 * as_compose_yaml_write_handler_cb:
 *
 * Helper function to store the emitted YAML document.
 */
static int
as_compose_yaml_write_handler_cb (void *ptr, unsigned char *buffer, size_t size)
{
	GString *str;
	str = (GString*) ptr;
	g_string_append_len (str, (const gchar*) buffer, size);

	return 1;
}

static gboolean
asc_compose_component_known (AscCompose *compose, AsComponent *cpt)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return g_hash_table_contains (priv->known_cids, as_component_get_id (cpt));
}

/**
 * asc_evaluate_custom_entry_cb:
 *
 * Helper function for asc_compose_finalize_components()
 */
static gboolean
asc_evaluate_custom_entry_cb (gpointer key_p, gpointer value_p, gpointer user_data)
{
	const gchar *key = (const gchar*) key_p;
	GPtrArray *whitelist = (GPtrArray*) user_data;

	for (guint i = 0; i < whitelist->len; i++) {
		if (g_strcmp0 (g_ptr_array_index (whitelist, i), key) == 0)
			return FALSE; /* do not delete, key is in whitelist */
	}

	/* remove key that was not allowed */
	return TRUE;
}

static void
asc_compose_finalize_components (AscCompose *compose, AscResult *cres)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GPtrArray) final_cpts = NULL;

	final_cpts = asc_result_fetch_components (cres);
	for (guint i = 0; i < final_cpts->len; i++) {
		AsValueFlags value_flags;
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (final_cpts, i));
		AsComponentKind ckind = as_component_get_kind (cpt);

		/* add bundle data if we have any */
		if (asc_result_get_bundle_kind (cres) != AS_BUNDLE_KIND_UNKNOWN) {
			AsBundleKind bundle_kind = asc_result_get_bundle_kind (cres);
			g_ptr_array_set_size (as_component_get_bundles (cpt), 0);
			as_component_set_pkgname (cpt, NULL);

			if (bundle_kind == AS_BUNDLE_KIND_PACKAGE) {
				as_component_set_pkgname (cpt, asc_result_get_bundle_id (cres));
			} else {
				g_autoptr(AsBundle) bundle = as_bundle_new ();
				as_bundle_set_kind (bundle, bundle_kind);
				as_bundle_set_id (bundle, asc_result_get_bundle_id (cres));
			}
		}

		value_flags = as_component_get_value_flags (cpt);
		as_component_set_value_flags (cpt, value_flags | AS_VALUE_FLAG_NO_TRANSLATION_FALLBACK);
		as_component_set_active_locale (cpt, "C");

		if (ckind == AS_COMPONENT_KIND_UNKNOWN) {
			if (!asc_result_add_hint_simple (cres, cpt, "metainfo-unknown-type"))
				continue;
		}

		/* filter custom entries */
		if (!as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_PROPAGATE_CUSTOM)) {
			if (priv->custom_allowed->len == 0) {
				GHashTable *custom_entries = as_component_get_custom (cpt);
				/* no custom entries permitted in output */
				g_hash_table_remove_all (custom_entries);
			} else {
				GHashTable *custom_entries = as_component_get_custom (cpt);
				g_hash_table_foreach_remove (custom_entries,
								asc_evaluate_custom_entry_cb,
								priv->custom_allowed);
			}
		}

		/* only perform the next checks if we don't have a merge-component
		 * (which is by definition incomplete and only is required to have its ID present) */
		if (as_component_get_merge_kind (cpt) != AS_MERGE_KIND_NONE)
			continue;

		/* strip out release artifacts unless we were told to propagate them */
		if (!as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_PROPAGATE_ARTIFACTS)) {
			GPtrArray *releases = as_component_get_releases (cpt);
			for (guint j = 0; j < releases->len; j++) {
				AsRelease *rel = AS_RELEASE (g_ptr_array_index (releases, j));
				g_ptr_array_set_size (as_release_get_artifacts (rel), 0);
			}
		}

		if (as_is_empty (as_component_get_name (cpt)))
			if (!asc_result_add_hint_simple (cres, cpt, "metainfo-no-name"))
				continue;

		if (as_is_empty (as_component_get_summary (cpt)))
			if (!asc_result_add_hint_simple (cres, cpt, "metainfo-no-summary"))
				continue;

		/* ensure that everything that should have an icon has one */
		if (as_component_get_icons (cpt)->len == 0) {
			if (ckind == AS_COMPONENT_KIND_DESKTOP_APP) {
				if (!asc_result_add_hint_simple (cres, cpt, "gui-app-without-icon"))
					continue;
			} else if (ckind == AS_COMPONENT_KIND_WEB_APP) {
				if (!asc_result_add_hint_simple (cres, cpt, "web-app-without-icon"))
					continue;
			} else if (ckind == AS_COMPONENT_KIND_FONT) {
				if (!asc_result_add_hint_simple (cres, cpt, "font-without-icon"))
					continue;
			} else if (ckind == AS_COMPONENT_KIND_OPERATING_SYSTEM) {
				if (!asc_result_add_hint_simple (cres, cpt, "os-without-icon"))
					continue;
			}
		}

                if (ckind == AS_COMPONENT_KIND_DESKTOP_APP ||
		    ckind == AS_COMPONENT_KIND_CONSOLE_APP ||
		    ckind == AS_COMPONENT_KIND_WEB_APP) {
			/* desktop-application components are required to have a category */
			if (ckind != AS_COMPONENT_KIND_CONSOLE_APP) {
				if (as_component_get_categories (cpt)->len <= 0)
					if (!asc_result_add_hint_simple (cres, cpt, "no-valid-category"))
							continue;
			}

			if (as_is_empty (as_component_get_description (cpt))) {
				if (!asc_result_add_hint (cres,
							  cpt,
							  "description-missing",
							  "kind", as_component_kind_to_string (ckind),
							  NULL))
					continue;
			}
                }
	}
}

static void
asc_compose_process_task_cb (AscComposeTask *ctask, AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autofree gchar *metainfo_dir = NULL;
	g_autofree gchar *app_dir = NULL;
	g_autofree gchar *share_dir = NULL;
	g_autoptr(AsMetadata) mdata = NULL;
	g_autoptr(AsValidator) validator = NULL;
	g_autoptr(GPtrArray) mi_fnames = NULL;
	g_autoptr(GHashTable) de_fname_map = NULL;
	g_autoptr(GPtrArray) found_cpts = NULL;
	g_autoptr(AsCurl) acurl = NULL;
	g_autoptr(GError) tmp_error = NULL;
	gboolean has_fonts = FALSE;
	gboolean filter_cpts = FALSE;
	GPtrArray *contents = NULL;

	/* propagate unit bundle ID */
	asc_result_set_bundle_id (ctask->result,
				  asc_unit_get_bundle_id (ctask->unit));
	asc_result_set_bundle_kind (ctask->result,
				  asc_unit_get_bundle_kind (ctask->unit));

	/* configure metadata loader */
	mdata = as_metadata_new ();
	as_metadata_set_locale (mdata, "ALL");
	as_metadata_set_format_style (mdata, AS_FORMAT_STYLE_METAINFO);

	/* create validator */
	validator = as_validator_new ();

	/* Curl interface for this task */
	acurl = as_curl_new (&tmp_error);
	if (acurl == NULL) {
		g_critical ("Unable to initialize networking: %s", tmp_error->message);
		g_error_free (g_steal_pointer (&tmp_error));
	}
	if (priv->cainfo != NULL)
		as_curl_set_cainfo (acurl, priv->cainfo);

	/* give unit a hint as to which paths we want to read */
	share_dir = g_build_filename (priv->prefix, "share", NULL);
	asc_unit_add_relevant_path (ctask->unit, share_dir);

	/* open our unit for reading */
	if (!asc_unit_open (ctask->unit, &tmp_error)) {
		g_warning ("Failed to open unit: %s", tmp_error->message);
		asc_result_add_hint (ctask->result,
					NULL,
					"unit-read-error",
					"name", asc_unit_get_bundle_id (ctask->unit),
					"msg", tmp_error->message,
					NULL);
		return;
	}
	contents = asc_unit_get_contents (ctask->unit);

	/* collect interesting data for this unit */
	metainfo_dir = g_build_filename (share_dir, "metainfo", NULL);
	app_dir = g_build_filename (share_dir, "applications", NULL);
	mi_fnames = g_ptr_array_new_with_free_func (g_free);
	de_fname_map = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);

	g_debug ("Looking for metainfo files in: %s", metainfo_dir);
	for (guint i = 0; i < contents->len; i++) {
		const gchar *fname = g_ptr_array_index (contents, i);

		if (g_str_has_prefix (fname, metainfo_dir) &&
			(g_str_has_suffix (fname, ".metainfo.xml") || g_str_has_suffix (fname, ".appdata.xml"))) {
			g_ptr_array_add (mi_fnames, g_strdup (fname));
			continue;
		}
		if (g_str_has_prefix (fname, app_dir) && g_str_has_suffix (fname, ".desktop")) {
			g_hash_table_insert (de_fname_map,
					     g_path_get_basename (fname),
					     g_strdup (fname));
			continue;
		}
	}

	/* check if we need to filter components */
	filter_cpts = g_hash_table_size (priv->allowed_cids) > 0;

	/* process metadata */
	for (guint i = 0; i < mi_fnames->len; i++) {
		g_autoptr(GBytes) mi_bytes = NULL;
		g_autoptr(GError) local_error = NULL;
		g_autoptr(AsComponent) cpt = NULL;
		g_autofree gchar *mi_basename = NULL;
		const gchar *mi_fname = g_ptr_array_index (mi_fnames, i);
		mi_basename = g_path_get_basename (mi_fname);

		g_debug ("Processing: %s", mi_fname);
		mi_bytes = asc_unit_read_data (ctask->unit, mi_fname, &local_error);
		if (mi_bytes == NULL) {
			asc_result_add_hint_by_cid (ctask->result,
						    mi_basename,
						    "file-read-error",
						    "fname", mi_fname,
						    "msg", local_error->message,
						    NULL);
			g_debug ("Failed '%s': %s", mi_basename, local_error->message);
			continue;
		}
		as_metadata_clear_components (mdata);
		cpt = asc_parse_metainfo_data (ctask->result,
						mdata,
						mi_bytes,
						mi_basename);
		if (cpt == NULL) {
			g_debug ("Rejected: %s", mi_basename);
			continue;
		}

		/* filter out this component if it isn't on the allowlist */
		if (filter_cpts) {
			if (!g_hash_table_contains (priv->allowed_cids, as_component_get_id (cpt))) {
				asc_result_remove_component (ctask->result, cpt);
				continue;
			}
		}

		/* check if we have a duplicate */
		if (asc_compose_component_known (compose, cpt)) {
			asc_result_add_hint_simple (ctask->result, cpt, "duplicate-component");
			continue;
		} else {
			g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
			g_hash_table_add (priv->known_cids,
					  g_strdup (as_component_get_id (cpt)));
		}

		/* validate the data */
		if (as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_VALIDATE)) {
			asc_validate_metainfo_data_for_component (ctask->result,
									validator,
									cpt,
									mi_bytes,
									mi_basename);
		}

		/* legacy support: Synthesize launchable entry if none was set,
		 * but only if we actually need to do that.
		 * At the moment we determine whether a .desktop file is needed by checking
		 * if the metainfo file defines an icon (which is commonly provided by the .desktop
		 * file instead of the metainfo file).
		 * This heuristic is, of course, not ideal, which is why everything should have a launchable tag.
		 */
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
			AsLaunchable *launchable = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
			if (launchable == NULL) {
				AsIcon *stock_icon = NULL;
				GPtrArray *icons = as_component_get_icons (cpt);

				for (guint i = 0; i < icons->len; i++) {
					AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));
					if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
						stock_icon = icon;
						break;
					}
				}
				if (stock_icon == NULL) {
					g_autoptr(AsLaunchable) launch = as_launchable_new ();
					g_autofree gchar *synth_desktop_id = NULL;

					if (g_str_has_suffix (as_component_get_id (cpt), ".desktop"))
						synth_desktop_id = g_strdup (as_component_get_id (cpt));
					else
						synth_desktop_id = g_strdup_printf ("%s.desktop",
										    as_component_get_id (cpt));

					as_launchable_set_kind (launch, AS_LAUNCHABLE_KIND_DESKTOP_ID);
					as_launchable_add_entry (launch, synth_desktop_id);
					as_component_add_launchable (cpt, launch);
				}
			}
		}

		/* find an accompanying desktop-entry file, if one exists */
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
			AsLaunchable *launchable = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
			if (launchable != NULL) {
				GPtrArray *launch_entries = as_launchable_get_entries (launchable);
				for (guint j = 0; j < launch_entries->len; j++) {
					const gchar *de_basename = g_ptr_array_index (launch_entries, j);
					const gchar *de_fname = g_hash_table_lookup (de_fname_map, de_basename);
					if (de_fname == NULL) {
						asc_result_add_hint (ctask->result,
								     cpt,
								     "missing-launchable-desktop-file",
								     "desktop_id", de_basename,
								     NULL);
						continue;
					}

					if (j == 0) {
						/* add data from the first desktop-entry file to this app */
						g_autoptr(AsComponent) de_cpt = NULL;
						g_autoptr(GBytes) de_bytes = NULL;

						g_debug ("Reading: %s", de_fname);
						de_bytes = asc_unit_read_data (ctask->unit, de_fname, &local_error);
						if (de_bytes == NULL) {
							asc_result_add_hint (ctask->result,
									     cpt,
									     "file-read-error",
									     "fname", de_fname,
									     "msg", local_error->message,
									     NULL);
							g_error_free (g_steal_pointer (&local_error));
							g_hash_table_remove (de_fname_map, de_basename);
							continue;
						}

						de_cpt = asc_parse_desktop_entry_data (ctask->result,
											cpt,
											de_bytes,
											de_basename,
											TRUE, /* ignore NoDisplay & Co. */
											AS_FORMAT_VERSION_CURRENT,
											priv->de_l10n_fn,
										        priv->de_l10n_fn_udata);
						if (de_cpt != NULL) {
							/* update component hash based on new source data */
							asc_result_update_component_gcid (ctask->result,
											  cpt,
											  de_bytes);
						}
					}
					g_hash_table_remove (de_fname_map, de_basename);
				} /* end launch entry loop */
			}
		} /* end of desktop-entry support */
	} /* end of metadata parsing loop */

	/* process the remaining .desktop files */
	if (as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_PROCESS_UNPAIRED_DESKTOP)) {
		GHashTableIter ht_iter;
		gpointer ht_key;
		gpointer ht_value;

		g_hash_table_iter_init (&ht_iter, de_fname_map);
		while (g_hash_table_iter_next (&ht_iter, &ht_key, &ht_value)) {
			const gchar *de_fname = (const gchar*) ht_value;
			const gchar *de_basename = (const gchar*) ht_key;
			g_autoptr(AsComponent) de_cpt = NULL;
			g_autoptr(GBytes) de_bytes = NULL;
			g_autoptr(GError) local_error = NULL;

			g_debug ("Reading orphan desktop-entry: %s", de_fname);
			de_bytes = asc_unit_read_data (ctask->unit, de_fname, &local_error);
			if (de_bytes == NULL) {
				asc_result_add_hint_by_cid (ctask->result,
							    de_basename,
							    "file-read-error",
							    "fname", de_fname,
							    "msg", local_error->message,
							    NULL);
				g_error_free (g_steal_pointer (&local_error));
				continue;
			}

			/* synthesize component from desktop entry. The component will be auto-added
			 * to the results set if it is valid. */
			de_cpt = asc_parse_desktop_entry_data (ctask->result,
								NULL, /* existing component */
								de_bytes,
								de_basename,
								FALSE, /* don't ignore NoDisplay & Co. */
								AS_FORMAT_VERSION_CURRENT,
								priv->de_l10n_fn,
								priv->de_l10n_fn_udata);
			if (de_cpt != NULL)
				asc_result_add_hint_simple (ctask->result, de_cpt, "no-metainfo");
		}
	}

	/* allow external function to alter the detected components early on before we do expensive processing */
	if (priv->check_md_early_fn != NULL)
		priv->check_md_early_fn (ctask->result,
					 ctask->unit,
					 priv->check_md_early_fn_udata);

	/* process translation status */
	if (as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_PROCESS_TRANSLATIONS)) {
		if (priv->locale_unit == NULL) {
			asc_read_translation_status (ctask->result,
							ctask->unit,
							priv->prefix,
							priv->min_l10n_percentage);
		} else {
			asc_read_translation_status (ctask->result,
							priv->locale_unit,
							priv->prefix,
							priv->min_l10n_percentage);
		}
	}

	/* process icons and screenshots */
	found_cpts = asc_result_fetch_components (ctask->result);
	for (guint i = 0; i < found_cpts->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (found_cpts, i));

		/* icons */
		if (!as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_IGNORE_ICONS)) {
			g_autofree gchar *icons_export_dir = NULL;
			if (priv->icons_result_dir == NULL)
				icons_export_dir = g_build_filename (asc_globals_get_tmp_dir (),
									asc_result_gcid_for_component (ctask->result, cpt),
									"icons",
									NULL);
			else
				icons_export_dir = g_strdup (priv->icons_result_dir);

			asc_compose_process_icons (compose,
						   ctask->result,
						   cpt,
						   ctask->unit,
						   icons_export_dir);
			/* skip the next steps if the component has been ignored */
			if (asc_result_is_ignored (ctask->result, cpt))
				continue;
		}

		/* screenshots, but only if we allow network access */
		if (as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_ALLOW_NET) && acurl != NULL)
			asc_process_screenshots (ctask->result,
						 cpt,
						 acurl,
						 priv->media_result_dir,
						 priv->max_scr_size_bytes,
						 as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_ALLOW_SCREENCASTS),
						 as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_STORE_SCREENSHOTS));

		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_FONT)
			has_fonts = TRUE;
	}

	/* handle all font components present in this unit */
	if (has_fonts && as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_PROCESS_FONTS)) {
		asc_process_fonts (ctask->result,
				   ctask->unit,
				   priv->media_result_dir,
				   priv->icons_result_dir,
				   priv->icon_policy,
				   priv->flags);
	}

	/* clean up superfluous hints in case we were filtering the results, as some rejected
	 * components may have generated errors while we were inspecting them */
	if (filter_cpts) {
		const gchar **cids = asc_result_get_component_ids_with_hints (ctask->result);
		for (guint i = 0; cids[i] != NULL; i++) {
			if (!g_hash_table_contains (priv->allowed_cids, cids[i]))
				asc_result_remove_hints_for_cid (ctask->result, cids[i]);
		}
	}

	/* postprocess components and add remaining values and hints */
	if (!as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_NO_FINAL_CHECK))
		asc_compose_finalize_components (compose, ctask->result);

	asc_unit_close (ctask->unit);
}

static gboolean
asc_compose_export_hints_data_yaml (AscCompose *compose, GError **error)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	yaml_emitter_t emitter;
	yaml_event_t event;
	gboolean res = FALSE;
	g_auto(GStrv) all_hint_tags = NULL;
	g_autofree gchar *yaml_fname = NULL;
	g_autoptr(GString) yaml_result = g_string_new ("");

	/* don't export anything if export dir isn't set */
	if (priv->hints_result_dir == NULL)
		return TRUE;

	yaml_emitter_initialize (&emitter);
	yaml_emitter_set_indent (&emitter, 2);
	yaml_emitter_set_unicode (&emitter, TRUE);
	yaml_emitter_set_width (&emitter, 100);
	yaml_emitter_set_output (&emitter, as_compose_yaml_write_handler_cb, yaml_result);

	/* emit start event */
	yaml_stream_start_event_initialize (&event, YAML_UTF8_ENCODING);
	if (!yaml_emitter_emit (&emitter, &event)) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     "Failed to initialize YAML emitter.");
		yaml_emitter_delete (&emitter);
		return FALSE;
	}

	/* new document for the tag list */
	yaml_document_start_event_initialize (&event, NULL, NULL, NULL, FALSE);
	res = yaml_emitter_emit (&emitter, &event);
	g_assert (res);
	all_hint_tags = asc_globals_get_hint_tags ();

	as_yaml_sequence_start (&emitter);
	for (guint i = 0; all_hint_tags[i] != NULL; i++) {
		const gchar *tag = all_hint_tags[i];
		/* main dict start */
		as_yaml_mapping_start (&emitter);

		as_yaml_emit_entry (&emitter,
				    "Tag",
				    tag);
		as_yaml_emit_entry (&emitter,
				    "Severity",
				    as_issue_severity_to_string (asc_globals_hint_tag_severity (tag)));
		as_yaml_emit_entry (&emitter,
				    "Explanation",
				    asc_globals_hint_tag_explanation (tag));
		/* main dict end */
		as_yaml_mapping_end (&emitter);
	}
	as_yaml_sequence_end (&emitter);

	/* finalize the tag list document */
	yaml_document_end_event_initialize (&event, 1);
	res = yaml_emitter_emit (&emitter, &event);
	g_assert (res);

	/* new document for the actual issue hints */
	yaml_document_start_event_initialize (&event, NULL, NULL, NULL, FALSE);
	res = yaml_emitter_emit (&emitter, &event);
	g_assert (res);

	as_yaml_sequence_start (&emitter);
	for (guint i = 0; i < priv->results->len; i++) {
		g_autofree const gchar **hints_cids = NULL;
		AscResult *result = ASC_RESULT (g_ptr_array_index (priv->results, i));

		hints_cids = asc_result_get_component_ids_with_hints (result);
		if (hints_cids == NULL)
			continue;

		as_yaml_mapping_start (&emitter);
		as_yaml_emit_entry (&emitter, "Unit", asc_result_get_bundle_id (result));
		as_yaml_emit_scalar (&emitter, "Hints");
		as_yaml_sequence_start (&emitter);
		for (guint j = 0; hints_cids[j] != NULL; j++) {
			GPtrArray *hints = asc_result_get_hints (result, hints_cids[j]);

			as_yaml_mapping_start (&emitter);
			as_yaml_emit_scalar (&emitter, hints_cids[j]);
			as_yaml_sequence_start (&emitter);
			for (guint k = 0; k < hints->len; k++) {
				GPtrArray *vars;
				AscHint *hint = ASC_HINT (g_ptr_array_index (hints, k));
				as_yaml_mapping_start (&emitter);
				as_yaml_emit_entry (&emitter, "tag", asc_hint_get_tag (hint));

				vars = asc_hint_get_explanation_vars_list (hint);
				as_yaml_emit_scalar (&emitter, "variables");
				as_yaml_mapping_start (&emitter);
				for (guint l = 0; l < vars->len; l += 2) {
					as_yaml_emit_entry (&emitter,
							    g_ptr_array_index (vars, l),
							    g_ptr_array_index (vars, l + 1));
				}
				as_yaml_mapping_end (&emitter);

				/* end hint mapping */
				as_yaml_mapping_end (&emitter);
			}
			as_yaml_sequence_end (&emitter);
			as_yaml_mapping_end (&emitter);
		}
		as_yaml_sequence_end (&emitter);
		as_yaml_mapping_end (&emitter);
	}
	as_yaml_sequence_end (&emitter);

	/* finalize the hints document */
	yaml_document_end_event_initialize (&event, 1);
	res = yaml_emitter_emit (&emitter, &event);
	g_assert (res);

	/* end stream */
	yaml_stream_end_event_initialize (&event);
	res = yaml_emitter_emit (&emitter, &event);
	g_assert (res);

	yaml_emitter_flush (&emitter);
	yaml_emitter_delete (&emitter);

	g_mkdir_with_parents (priv->hints_result_dir, 0755);
	yaml_fname = g_strdup_printf ("%s/%s.hints.yaml", priv->hints_result_dir, priv->origin);
	return g_file_set_contents (yaml_fname, yaml_result->str, yaml_result->len, error);
}


static gboolean
asc_compose_export_hints_data_html (AscCompose *compose, GError **error)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GString) html = NULL;
	g_autofree gchar *html_fname = NULL;

	/* create header */
	html = g_string_new ("");
	g_string_append (html, "<!DOCTYPE html>\n"
			       "<html lang=\"en\">\n");
	g_string_append (html, "<head>\n");
	g_string_append (html, "<meta http-equiv=\"Content-Type\" content=\"text/html; "
			       "charset=UTF-8\" />\n");
	g_string_append (html, "<meta name=\"generator\" content=\"appstream-compose " PACKAGE_VERSION "\" />\n");
	g_string_append_printf (html, "<title>Compose issue hints for \"%s\"</title>\n", priv->origin);

	g_string_append (html,
	"\n<style type=\"text/css\">\n"
	"body {\n"
	"	margin-top: 2em;\n"
	"	margin-left: 5%;\n"
	"	margin-right: 5%;\n"
	"	font-family: 'Lucida Grande', Verdana, Arial, Sans-Serif;\n"
	"}\n"
	"a {\n"
	"    color: #337ab7;\n"
	"    text-decoration: none;\n"
	"    background-color: transparent;\n"
	"}\n"
	".permalink {\n"
	"    font-size: 75%;\n"
	"    color: #999;\n"
	"    line-height: 100%;\n"
	"    font-weight: normal;\n"
	"    text-decoration: none;\n"
	"}\n"
	".label {\n"
	"    border-radius: 0.25em;\n"
	"    color: #fff;\n"
	"    display: inline;\n"
	"    font-size: 75%;\n"
	"    font-weight: 700;\n"
	"    line-height: 1;\n"
	"    padding: 0.2em 0.6em 0.3em;\n"
	"    text-align: center;\n"
	"    vertical-align: baseline;\n"
	"    white-space: nowrap;\n"
	"}\n"
	".label-info {\n"
	"   background-color: #5bc0de;\n"
	"}\n"
	".label-warning {\n"
	"    background-color: #f0ad4e;\n"
	"}\n"
	".label-error {\n"
	"    background-color: #d9534f;\n"
	"}\n"
	".label-neutral {\n"
	"    background-color: #777;\n"
	"}\n"
	".content {\n"
	"    width: 60%;\n"
	"}\n"
	"</style>\n\n");

	g_string_append (html, "</head>\n");
	g_string_append (html, "<body>\n");

	for (guint i = 0; i < priv->results->len; i++) {
		g_autofree const gchar **hints_cids = NULL;
		g_autofree gchar *bundle_hstr = NULL;
		AscResult *result = ASC_RESULT (g_ptr_array_index (priv->results, i));

		hints_cids = asc_result_get_component_ids_with_hints (result);
		if (hints_cids == NULL)
			continue;

		g_string_append_printf (html, "<h1 style=\"font-weight: 100;\">Compose issue hints for \"%s\"</h1>\n", priv->origin);
		g_string_append (html, "<div class=\"content\">");
		bundle_hstr = g_markup_escape_text (asc_result_get_bundle_id (result), -1);
		g_string_append_printf (html, "<h2>Unit: %s</h2>\n<hr/>\n", bundle_hstr);

		for (guint j = 0; hints_cids[j] != NULL; j++) {
			g_autofree gchar *cid_hstr = NULL;
			GPtrArray *hints = asc_result_get_hints (result, hints_cids[j]);

			cid_hstr = g_markup_escape_text (hints_cids[j], -1);
			g_string_append_printf (html, "<h3 id=\"%s\">%s <a title=\"Permalink\" class=\"permalink\" href=\"#%s\">#</a></h3>\n",
						cid_hstr, cid_hstr, cid_hstr);
			g_string_append (html, "<ul>\n");
			for (guint k = 0; k < hints->len; k++) {
				g_autofree gchar *explanation = NULL;
				const gchar *label_style;
				AsIssueSeverity severity;
				AscHint *hint = ASC_HINT (g_ptr_array_index (hints, k));

				severity = asc_hint_get_severity (hint);
				switch (severity) {
					case AS_ISSUE_SEVERITY_ERROR:
						label_style = "label-error";
						break;
					case AS_ISSUE_SEVERITY_WARNING:
						label_style = "label-warning";
						break;
					case AS_ISSUE_SEVERITY_INFO:
						label_style = "label-info";
						break;
					case AS_ISSUE_SEVERITY_PEDANTIC:
						label_style = "label-neutral";
						break;
					default:
						label_style = "label-neutral";
				}

				explanation = asc_hint_format_explanation (hint);
				g_string_append_printf (html, "    <li>\n    <strong>%s</strong>&nbsp;<span class=\"label %s\">%s</span>\n",
							asc_hint_get_tag (hint), label_style, as_issue_severity_to_string (severity));
				g_string_append_printf (html, "    <p>%s</p>\n    </li>\n",
							explanation);
			}
			g_string_append (html, "</ul>\n");
		}
	}

	g_string_append (html, "</div>\n");
	g_string_append (html, "</body>\n");
	g_string_append (html, "</html>\n");

	g_mkdir_with_parents (priv->hints_result_dir, 0755);
	html_fname = g_strdup_printf ("%s/%s.hints.html", priv->hints_result_dir, priv->origin);
	return g_file_set_contents (html_fname, html->str, html->len, error);
}

static gboolean
asc_compose_save_metadata_result (AscCompose *compose, gboolean *results_not_empty, GError **error)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(AsMetadata) mdata = NULL;
	g_autofree gchar *data_basename = NULL;
	g_autofree gchar *data_fname = NULL;

	mdata = as_metadata_new ();
	as_metadata_set_format_style (mdata, AS_FORMAT_STYLE_COLLECTION);
	as_metadata_set_format_version (mdata, AS_FORMAT_VERSION_CURRENT);

	/* Set baseurl only if one is set and we actually store any screenshot media. If no screenshot media
	 * is stored, upstream's URLs are used and having a media base URL makes no sense */
	if (priv->media_baseurl != NULL && as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_STORE_SCREENSHOTS))
		as_metadata_set_media_baseurl (mdata, priv->media_baseurl);

	if (priv->format == AS_FORMAT_KIND_YAML)
		data_basename = g_strdup_printf ("%s.yml.gz", priv->origin);
	else
		data_basename = g_strdup_printf ("%s.xml.gz", priv->origin);

	if (g_mkdir_with_parents (priv->data_result_dir, 0755)) {
		g_set_error (error,
			     ASC_COMPOSE_ERROR,
			     ASC_COMPOSE_ERROR_FAILED,
			     "failed to create %s: %s",
			     priv->data_result_dir,
			     strerror (errno));
		return FALSE;
	}

	if (results_not_empty != NULL)
		*results_not_empty = FALSE;

	for (guint i = 0; i < priv->results->len; i++) {
		g_autoptr(GPtrArray) cpts = NULL;
		AscResult *result = ASC_RESULT (g_ptr_array_index (priv->results, i));
		cpts = asc_result_fetch_components (result);
		for (guint j = 0; j < cpts->len; j++)
			as_metadata_add_component (mdata,
						   AS_COMPONENT(g_ptr_array_index (cpts, j)));

		if (cpts->len > 0 && results_not_empty != NULL)
			*results_not_empty = TRUE;
	}

	data_fname = g_build_filename (priv->data_result_dir, data_basename, NULL);
	return as_metadata_save_collection (mdata, data_fname, priv->format, error);
}

/**
 * asc_compose_finalize_results:
 * @compose: an #AscCompose instance.
 *
 * Perform final validation of generated data.
 * Calling this function is not necessary, unless the final check was explicitly
 * disabled using the %ASC_COMPOSE_FLAG_NO_FINAL_CHECK flag.
 */
void
asc_compose_finalize_results (AscCompose *compose)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);

	for (guint i = 0; i < priv->results->len; i++) {
		AscResult *cres = ASC_RESULT (g_ptr_array_index (priv->results, i));
		asc_compose_finalize_components (compose, cres);
	}
}

/**
 * asc_compose_finalize_result:
 * @compose: an #AscCompose instance.
 * @result: the #AscResult to finalize
 *
 * Perform final validation of generated data for the specified
 * result container.
 */
void
asc_compose_finalize_result (AscCompose *compose, AscResult *result)
{
	asc_compose_finalize_components (compose, result);
}

/**
 * asc_compose_run:
 * @compose: an #AscCompose instance.
 * @cancellable: a #GCancellable.
 * @error: A #GError or %NULL.
 *
 * Process the registered units and generate collection metadata from
 * found components.
 *
 * Returns: (transfer none) (element-type AscResult): The results, or %NULL on error
 */
GPtrArray*
asc_compose_run (AscCompose *compose, GCancellable *cancellable, GError **error)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(GPtrArray) tasks = NULL;
	gboolean temp_dir_created = FALSE;
	gboolean results_generated = FALSE;

	/* ensure icon output dir is set, hint and data output dirs are optional */
	if (priv->icons_result_dir == NULL && !as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_IGNORE_ICONS)) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     _("Icon output directory is not set."));
		return NULL;
	}

	if (priv->media_baseurl == NULL && priv->media_result_dir != NULL) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     _("Media result directory is set, but media base URL is not. A media base URL is needed "
				       "to export media that is served via the media URL."));
		return NULL;
	}

	if (priv->media_result_dir == NULL) {
		AscIconPolicyIter ip_iter;
		guint icon_size;
		guint scale_factor;
		AscIconState icon_state;
		asc_icon_policy_iter_init (&ip_iter, priv->icon_policy);
		while (asc_icon_policy_iter_next (&ip_iter, &icon_size, &scale_factor, &icon_state)) {
			if (icon_state == ASC_ICON_STATE_IGNORED)
				continue;
			if (icon_state == ASC_ICON_STATE_REMOTE_ONLY || icon_state == ASC_ICON_STATE_CACHED_REMOTE) {
				g_debug ("No media export directory set, but icon %ix%i@%i is set for remote delivery. Disabling remote for this icon type.",
					 icon_size, icon_size, scale_factor);
				asc_icon_policy_set_policy (priv->icon_policy,
							    icon_size,
							    scale_factor,
							    ASC_ICON_STATE_CACHED_ONLY);
			}

		}

		if (as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_STORE_SCREENSHOTS)) {
			g_debug ("No media export directory set, but screenshots are supposed to be stored. Disabling screenshot storage.");
			as_flags_remove (priv->flags, ASC_COMPOSE_FLAG_STORE_SCREENSHOTS);
		}
	}

	if (g_file_test (asc_globals_get_tmp_dir (), G_FILE_TEST_EXISTS)) {
		g_debug ("Will use existing directory '%s' for temporary data (and will not delete it later).",
			 asc_globals_get_tmp_dir ());
		temp_dir_created = FALSE;
	} else {
		g_debug ("Will use temporary directory '%s' and delete it after this run.",
			 asc_globals_get_tmp_dir ());
		temp_dir_created = TRUE;
	}

	/* sanity check to ensure resources can be loaded */
	as_utils_ensure_resources ();

	tasks = g_ptr_array_new_with_free_func ((GDestroyNotify) asc_compose_task_free);

	for (guint i = 0; i < priv->units->len; i++) {
		AscComposeTask *ctask;
		AscUnit *unit = g_ptr_array_index (priv->units, i);
		ctask = asc_compose_task_new (unit);
		g_ptr_array_add (tasks, ctask);
	}

	if (as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_USE_THREADS)) {
		GThreadPool *tpool = NULL;
		tpool = g_thread_pool_new ((GFunc) asc_compose_process_task_cb,
					   compose,
					   -1, /* max threads */
					   FALSE, /* exclusive */
					   error);
		if (tpool == NULL)
			return NULL;

		/* launch all processing tasks in parallel */
		for (guint i = 0; i < tasks->len; i++)
			g_thread_pool_push (tpool, g_ptr_array_index (tasks, i), NULL);

		/* shutdown thread pool, wait for all tasks to complete */
		g_thread_pool_free (tpool, FALSE, TRUE);
	} else {
		/* run everything in sequence */
		for (guint i = 0; i < tasks->len; i++)
			asc_compose_process_task_cb ((AscComposeTask *) g_ptr_array_index (tasks, i),
						     compose);
	}

	/* collect results */
	for (guint i = 0; i < tasks->len; i++) {
		AscComposeTask *ctask = g_ptr_array_index (tasks, i);
		g_ptr_array_add (priv->results, g_object_ref (ctask->result));
	}

	/* write result */
	if (priv->data_result_dir != NULL) {
		if (!asc_compose_save_metadata_result (compose, &results_generated, error))
			return NULL;
	}

	/* check if we (unexpectedly) had no results */
	if (g_hash_table_size (priv->allowed_cids) > 0 && !results_generated) {
		/* we had filters set but generated no results - this was most certainly not intended */
		AscComposeTask *ctask = g_ptr_array_index (tasks, 0);
		asc_result_add_hint_simple (ctask->result,
						NULL,
						"filters-but-no-output");
	}

	/* write hints */
	if (priv->hints_result_dir != NULL) {
		if (!asc_compose_export_hints_data_yaml (compose, error))
			return NULL;
		if (!asc_compose_export_hints_data_html (compose, error))
			return NULL;
	}

	/* clean up */
	if (temp_dir_created) {
		g_debug ("Removing temporary directory '%s'", asc_globals_get_tmp_dir ());
		if (!as_utils_delete_dir_recursive (asc_globals_get_tmp_dir ()))
			g_debug ("Failed to remove temporary directory.");
	}

	return priv->results;
}

/**
 * asc_compose_new:
 *
 * Creates a new #AscCompose.
 **/
AscCompose*
asc_compose_new (void)
{
	AscCompose *compose;
	compose = g_object_new (ASC_TYPE_COMPOSE, NULL);
	return ASC_COMPOSE (compose);
}
