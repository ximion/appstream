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
#include "asc-image.h"

typedef struct
{
	GPtrArray	*units;
	GPtrArray	*results;

	GHashTable	*allowed_cids;
	GRefString	*prefix;
	GRefString	*origin;
	gchar		*media_baseurl;
	AsFormatKind	format;
	guint		min_l10n_percentage;
	AscComposeFlags	flags;

	gchar		*data_result_dir;
	gchar		*icons_result_dir;
	gchar		*media_result_dir;
	gchar		*hints_result_dir;

	GHashTable	*known_cids;
	GMutex		mutex;
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
	g_mutex_init (&priv->mutex);

	/* defaults */
	priv->format = AS_FORMAT_KIND_XML;
	as_ref_string_assign_safe (&priv->prefix, "/usr");
	priv->min_l10n_percentage = 25;
	priv->flags = ASC_COMPOSE_FLAG_ALLOW_NET |
			ASC_COMPOSE_FLAG_VALIDATE |
			ASC_COMPOSE_FLAG_STORE_SCREENSHOTS;
}

static void
asc_compose_finalize (GObject *object)
{
	AscCompose *compose = ASC_COMPOSE (object);
	AscComposePrivate *priv = GET_PRIVATE (compose);

	g_ptr_array_unref (priv->units);
	g_ptr_array_unref (priv->results);

	g_hash_table_unref (priv->allowed_cids);
	g_hash_table_unref (priv->known_cids);
	as_ref_string_release (priv->prefix);
	as_ref_string_release (priv->origin);
	g_free (priv->media_baseurl);

	g_free (priv->data_result_dir);
	g_free (priv->icons_result_dir);
	g_free (priv->media_result_dir);
	g_free (priv->hints_result_dir);

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
	as_assign_string_safe (priv->hints_result_dir, dir);
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
			   AscUnit *unit)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	const gint sizes[] = {  48,
				64,
				128,
				-1 };
	const gint scale_factors[] = {  1,
					2,
					-1 };
	GPtrArray *icons = NULL;
	g_autoptr(AsIcon) stock_icon = NULL;
	gboolean stock_icon_found = FALSE;
	const gchar *icon_name = NULL;

	/* do nothing if we have no icons to process */
	icons = as_component_get_icons (cpt);
	if (icons == NULL || icons->len == 0)
		return;

	/* find a suitable stock icon as template */
	for (guint i = 0; i < icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
			stock_icon = icon;
			stock_icon_found = TRUE;
			break;
		}
		if (as_icon_get_kind (icon) == AS_ICON_KIND_LOCAL) {
			stock_icon = icon;
			stock_icon_found = TRUE;
		}
	}
	/* drop all preexisting icons */
	if (stock_icon != NULL)
		stock_icon = g_object_ref (stock_icon);
	g_ptr_array_set_size (as_component_get_icons (cpt), 0);

	if (!stock_icon_found) {
		asc_result_add_hint_simple (cres, cpt, "no-stock-icon");
		return;
	}

	icon_name = as_icon_get_name (stock_icon);
	for (guint k = 0; scale_factors[k] > 0; k++) {
		for (guint i = 0; sizes[i] > 0; i++) {
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
			icon_fname = asc_compose_find_icon_filename (compose,
									unit,
									icon_name,
									sizes[i],
									scale_factors[k]);

			if (icon_fname == NULL) {
				/* only a 64x64px icon is mandatory, everything else is optional */
				if (sizes[i] == 64 && scale_factors[k] == 1) {
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
							is_vector_icon? sizes[i] * scale_factors[k] : 0,
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
			if (sizes[i] == 48 && asc_image_get_width (img) > 48)
				continue;

			res_icon_size_str = (scale_factors[k] == 1)?
						g_strdup_printf ("%ix%i",
								 sizes[i], sizes[i])
						: g_strdup_printf ("%ix%i@%i",
								   sizes[i], sizes[i],
								   scale_factors[k]);
			res_icon_sizedir = g_build_filename (priv->icons_result_dir, res_icon_size_str, NULL);

			g_mkdir_with_parents (res_icon_sizedir, 0755);
			res_icon_basename = g_strdup_printf ("%s.png", as_component_get_id (cpt));
			res_icon_fname = g_build_filename (res_icon_sizedir,
							   res_icon_basename,
							   NULL);

			/* scale & save the image */
			g_debug ("Saving icon: %s", res_icon_fname);
			if (!asc_image_save_filename (img,
							res_icon_fname,
							sizes[i] * scale_factors[k], sizes[i] * scale_factors[k],
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
			if (priv->media_result_dir != NULL) {
				g_autofree gchar *icons_media_urlpart_dir = NULL;
				g_autofree gchar *icons_media_urlpart_fname = NULL;
				g_autofree gchar *icons_media_path = NULL;
				g_autofree gchar *icon_media_fname = NULL;
				g_autoptr(AsIcon) remote_icon = NULL;
				icons_media_urlpart_dir = g_strdup_printf ("%s/%s/%s",
								           asc_result_gcid_for_component (cres, cpt),
								           "icons",
								           res_icon_size_str);
				icons_media_urlpart_fname = g_strdup_printf ("%s/%s",
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
				as_icon_set_width (remote_icon, sizes[i]);
				as_icon_set_height (remote_icon, sizes[i]);
				as_icon_set_scale (remote_icon, scale_factors[k]);
				as_icon_set_url (remote_icon, icons_media_urlpart_fname);
				as_component_add_icon (cpt, remote_icon);
			}

			/* add icon to metadata */
			icon = as_icon_new ();
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
			as_icon_set_width (icon, sizes[i]);
			as_icon_set_height (icon, sizes[i]);
			as_icon_set_scale (icon, scale_factors[k]);
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

	/* give unit a hint as to which paths we want to read */
	share_dir = g_build_filename (priv->prefix, "share", NULL);
	asc_unit_add_relevant_path (ctask->unit, share_dir);

	/* open our unit for reading */
	if (!asc_unit_open (ctask->unit, &tmp_error)) {
		g_warning ("Failed to open unit: %s", tmp_error->message);
		return;
	}
	contents = asc_unit_get_contents (ctask->unit);

	/* collect interesting data for this unit */
	metainfo_dir = g_build_filename (share_dir, "metainfo", NULL);
	app_dir = g_build_filename (share_dir, "applications", NULL);
	mi_fnames = g_ptr_array_new_with_free_func (g_free);
	de_fname_map = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
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

		/* find an accompanying desktop-entry file, if one exists */
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
			const gchar *cid = NULL;
			AsLaunchable *launchable = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
			gboolean de_ref_found = FALSE;
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

						de_ref_found = TRUE;
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
							continue;
						}

						de_cpt = asc_parse_desktop_entry_data (ctask->result,
											cpt,
											de_bytes,
											de_basename,
											TRUE, /* ignore NoDisplay & Co. */
											AS_FORMAT_VERSION_CURRENT,
											NULL, NULL);
						if (de_cpt != NULL) {
							/* update component hash based on new source data */
							asc_result_update_component_gcid (ctask->result,
											  cpt,
											  de_bytes);
						}
					}
				}
			}

			/* legacy support */
			cid = as_component_get_id (cpt);
			if (!de_ref_found && g_str_has_suffix (cid, ".desktop")) {
				if (!g_hash_table_contains (de_fname_map, cid)) {
					asc_result_add_hint (ctask->result,
								cpt,
								"missing-launchable-desktop-file",
								"desktop_id", cid,
								NULL);
				} else {
					g_autoptr(GBytes) de_bytes = NULL;
					g_autofree gchar *de_fname = g_build_filename (app_dir, cid, NULL);

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
					} else {
						g_autoptr(AsComponent) de_cpt = NULL;
						de_cpt = asc_parse_desktop_entry_data (ctask->result,
											cpt,
											de_bytes,
											cid,
											TRUE, /* ignore NoDisplay & Co. */
											AS_FORMAT_VERSION_CURRENT,
											NULL, NULL);
						if (de_cpt != NULL) {
							/* update component hash based on new source data */
							asc_result_update_component_gcid (ctask->result,
											  cpt,
											  de_bytes);
						}
					}
				}
			} /* end of desktop-entry legacy support */
		} /* end of desktop-entry support */

		/* add bundle data */
		if (asc_unit_get_bundle_kind (ctask->unit) != AS_BUNDLE_KIND_UNKNOWN) {
			AsBundleKind bundle_kind = asc_unit_get_bundle_kind (ctask->unit);
			g_ptr_array_set_size (as_component_get_bundles (cpt), 0);
			as_component_set_pkgname (cpt, NULL);

			if (bundle_kind == AS_BUNDLE_KIND_PACKAGE) {
				as_component_set_pkgname (cpt, asc_unit_get_bundle_id (ctask->unit));
			} else {
				g_autoptr(AsBundle) bundle = as_bundle_new ();
				as_bundle_set_kind (bundle, bundle_kind);
				as_bundle_set_id (bundle, asc_unit_get_bundle_id (ctask->unit));
			}
		}
	} /* end of metadata parsing loop */

	/* process translation status */
	asc_read_translation_status (ctask->result,
				     ctask->unit,
				     priv->prefix,
				     25 /* minimum translation percentage */);

	/* process icons and screenshots */
	found_cpts = asc_result_fetch_components (ctask->result);
	for (guint i = 0; i < found_cpts->len; i++) {
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (found_cpts, i));

		/* icons */
		asc_compose_process_icons (compose,
					   ctask->result,
					   cpt,
					   ctask->unit);

		/* screenshots, but only if we allow network access */
		if (as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_ALLOW_NET) && acurl != NULL)
			asc_process_screenshots (ctask->result,
						 cpt,
						 acurl,
						 priv->media_result_dir,
						 as_flags_contains (priv->flags, ASC_COMPOSE_FLAG_STORE_SCREENSHOTS));
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
asc_compose_save_metadata_result (AscCompose *compose, GError **error)
{
	AscComposePrivate *priv = GET_PRIVATE (compose);
	g_autoptr(AsMetadata) mdata = NULL;
	g_autofree gchar *data_basename = NULL;
	g_autofree gchar *data_fname = NULL;

	mdata = as_metadata_new ();
	as_metadata_set_format_style (mdata, AS_FORMAT_STYLE_COLLECTION);
	as_metadata_set_format_version (mdata, AS_FORMAT_VERSION_CURRENT);
	if (priv->media_baseurl != NULL)
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

	for (guint i = 0; i < priv->results->len; i++) {
		g_autoptr(GPtrArray) cpts = NULL;
		AscResult *result = ASC_RESULT (g_ptr_array_index (priv->results, i));
		cpts = asc_result_fetch_components (result);
		for (guint j = 0; j < cpts->len; j++) {
			as_metadata_add_component (mdata,
						   AS_COMPONENT(g_ptr_array_index (cpts, j)));
		}
	}

	data_fname = g_build_filename (priv->data_result_dir, data_basename, NULL);
	return as_metadata_save_collection (mdata, data_fname, priv->format, error);
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
	GThreadPool *tpool = NULL;
	g_autoptr(GPtrArray) tasks = NULL;

	/* test if output directories are set */
	if (priv->data_result_dir == NULL) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     _("Metadata output directory is not set."));
		return NULL;
	}
	if (priv->icons_result_dir == NULL) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     _("Icon output directory is not set."));
		return NULL;
	}
	/* hint output directory is optional */

	if (priv->media_baseurl == NULL && priv->media_result_dir != NULL) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     _("Media result directory is set, but media base URL is not. A media base URL is needed "
				       "to export media that is served via the media URL."));
		return NULL;
	}

	tasks = g_ptr_array_new_with_free_func ((GDestroyNotify) asc_compose_task_free);

	for (guint i = 0; i < priv->units->len; i++) {
		AscUnit *unit;
		AscComposeTask *ctask;
		unit = g_ptr_array_index (priv->units, i);
		ctask = asc_compose_task_new (unit);
		g_ptr_array_add (tasks, ctask);
	}

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

	/* collect results */
	for (guint i = 0; i < tasks->len; i++) {
		AscComposeTask *ctask = g_ptr_array_index (tasks, i);
		g_ptr_array_add (priv->results, g_object_ref (ctask->result));
	}

	/* write result */
	if (priv->data_result_dir != NULL) {
		if (!asc_compose_save_metadata_result (compose, error))
			return NULL;
	}

	/* write hints */
	if (priv->hints_result_dir != NULL) {
		if (!asc_compose_export_hints_data_yaml (compose, error))
			return NULL;
		if (!asc_compose_export_hints_data_html (compose, error))
			return NULL;
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
