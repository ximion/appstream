/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asw-worker
 * @short_description: Request dispatcher of the AppStream media worker.
 *
 * The worker receives high-level media operations from libappstream-compose
 * via a socket, with all input data passed as sealed memfds and every rendered
 * result written into a writable file descriptor that the client opened for us.
 */

#include "config.h"
#include "asw-worker.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "as-utils-private.h"
#include "asc-globals.h"
#include "asc-media.h"
#include "asc-media-ipc.h"

#include "asw-font.h"
#include "asw-image-private.h"
#include "asw-sandbox.h"
#include "asw-video.h"
#include "asw-ipc.h"

struct _AswWorker {
	GObject parent_instance;
};

typedef struct {
	GSocket *socket;
} AswWorkerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AswWorker, asw_worker, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asw_worker_get_instance_private (o))

static void
asw_worker_finalize (GObject *object)
{
	AswWorker *worker = ASW_WORKER (object);
	AswWorkerPrivate *priv = GET_PRIVATE (worker);

	if (priv->socket != NULL)
		g_object_unref (priv->socket);

	G_OBJECT_CLASS (asw_worker_parent_class)->finalize (object);
}

static void
asw_worker_init (AswWorker *worker)
{
}

static void
asw_worker_class_init (AswWorkerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asw_worker_finalize;
}

/**
 * asw_worker_new_for_fd:
 * @socket_fd: File descriptor of the communication socket to the client.
 * @error: A #GError or %NULL
 *
 * Create a new #AswWorker communicating over the given socket.
 */
AswWorker *
asw_worker_new_for_fd (gint socket_fd, GError **error)
{
	g_autoptr(AswWorker) worker = ASW_WORKER (g_object_new (ASW_TYPE_WORKER, NULL));
	AswWorkerPrivate *priv = GET_PRIVATE (worker);

	priv->socket = g_socket_new_from_fd (socket_fd, error);
	if (priv->socket == NULL)
		return NULL;
	g_socket_set_blocking (priv->socket, TRUE);

	return g_steal_pointer (&worker);
}

/**
 * asw_worker_send_hello:
 *
 * Send the initial hello message to the client, communicating
 * protocol and program version for verification.
 */
gboolean
asw_worker_send_hello (AswWorker *worker, const AswSandboxInfo *sandbox, GError **error)
{
	AswWorkerPrivate *priv = GET_PRIVATE (worker);
	GVariantBuilder pb;
	GVariantBuilder fmt_builder;
	g_autoptr(GHashTable) formats = NULL;
	GHashTableIter ht_iter;
	gpointer ht_key;

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "protocol-version",
			       g_variant_new_uint32 (ASC_MEDIA_PROTOCOL_VERSION));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "program-version",
			       g_variant_new_string (PACKAGE_VERSION));

	g_variant_builder_init (&fmt_builder, G_VARIANT_TYPE ("as"));
	formats = asw_image_supported_format_names ();
	g_hash_table_iter_init (&ht_iter, formats);
	while (g_hash_table_iter_next (&ht_iter, &ht_key, NULL))
		g_variant_builder_add (&fmt_builder, "s", (const gchar *) ht_key);
	g_variant_builder_add (&pb, "{sv}", "image-formats", g_variant_builder_end (&fmt_builder));

	/* tell the client how well we managed to lock ourselves down */
	g_variant_builder_add (
	    &pb,
	    "{sv}",
	    "sandbox",
	    g_variant_new_string (asw_sandbox_state_to_token (
		sandbox == NULL ? ASW_SANDBOX_STATE_UNSUPPORTED : sandbox->state)));
	if (sandbox != NULL) {
		g_autofree gchar *sandbox_desc = asw_sandbox_info_describe (sandbox);
		g_variant_builder_add (&pb,
				       "{sv}",
				       "sandbox-detail",
				       g_variant_new_string (sandbox_desc));
	}

	return asw_ipc_send_response (priv->socket,
				      0, /* hello pseudo request-id */
				      ASC_MEDIA_STATUS_OK,
				      g_variant_builder_end (&pb),
				      error);
}

/**
 * asw_worker_lookup_fd:
 *
 * Resolve a file descriptor handle referenced in the request parameters.
 * Returns a duplicated fd owned by the caller, or -1 on error.
 */
static gint
asw_worker_lookup_fd (GVariant *params, GUnixFDList *fds, const gchar *key, GError **error)
{
	gint32 handle = -1;

	if (!g_variant_lookup (params, key, "h", &handle) || fds == NULL) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_PROTOCOL,
			     "Request was missing required fd '%s'.",
			     key);
		return -1;
	}

	return g_unix_fd_list_get (fds, handle, error);
}

/**
 * asw_worker_get_data_fd:
 *
 * Retrieve a sealed data memfd referenced in the request parameters.
 * Returns a duplicated fd owned by the caller, or -1 on error.
 */
static gint
asw_worker_get_data_fd (GVariant *params, GUnixFDList *fds, const gchar *key, GError **error)
{
	gint fd = asw_worker_lookup_fd (params, fds, key, error);
	if (fd < 0)
		return -1;

	if (!asw_memfd_verify_sealed (fd, NULL, error)) {
		close (fd);
		return -1;
	}

	return fd;
}

/**
 * asw_worker_get_readonly_file_fd:
 *
 * Retrieve a read-only file descriptor for a regular file that we are supposed
 * to read directly.
 * Returns a duplicated fd owned by the caller, or -1 on error.
 */
static gint
asw_worker_get_readonly_file_fd (GVariant *params,
				 GUnixFDList *fds,
				 const gchar *key,
				 GError **error)
{
	struct stat st;
	gint flags;
	gint fd = asw_worker_lookup_fd (params, fds, key, error);
	if (fd < 0)
		return -1;

	if (fstat (fd, &st) != 0 || !S_ISREG (st.st_mode)) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received data fd is not a regular file.");
		close (fd);
		return -1;
	}

	flags = fcntl (fd, F_GETFL);
	if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received data fd is not read-only.");
		close (fd);
		return -1;
	}

	return fd;
}

/**
 * asw_worker_get_output_fd:
 *
 * Retrieve the writable output fd of a single result entry.
 * The client opens these for us, so we only ever write into a file it has
 * already picked out.
 *
 * Returns: a duplicated fd owned by the caller, or -1 on error.
 */
static gint
asw_worker_get_output_fd (GVariant *entry, GUnixFDList *fds, GError **error)
{
	struct stat st;
	gint flags;
	gint fd = asw_worker_lookup_fd (entry, fds, "fd", error);
	if (fd < 0)
		return -1;

	if (fstat (fd, &st) != 0 || !S_ISREG (st.st_mode)) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received output fd is not a regular file.");
		close (fd);
		return -1;
	}

	flags = fcntl (fd, F_GETFL);
	if (flags < 0 || ((flags & O_ACCMODE) != O_WRONLY && (flags & O_ACCMODE) != O_RDWR)) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received output fd is not writable.");
		close (fd);
		return -1;
	}

	return fd;
}

/**
 * asw_worker_handle_setup:
 *
 * Apply the client's configuration to this worker process.
 */
static GVariant *
asw_worker_handle_setup (AswWorker *worker, GVariant *params, GUnixFDList *fds, GError **error)
{
	const gchar *ffprobe_path = NULL;
	guint32 memory_limit_mb = 0;

	g_variant_lookup (params, "ffprobe-path", "&s", &ffprobe_path);
	g_variant_lookup (params, "memory-limit-mb", "u", &memory_limit_mb);

	/* ffprobe is the only helper we still run ourselves: PNG size optimization
	 * happens in the client, which owns the resulting files */
	asc_globals_set_ffprobe_binary (as_is_empty (ffprobe_path) ? NULL : ffprobe_path);

	if (memory_limit_mb > 0) {
		struct rlimit limit;
		limit.rlim_cur = (rlim_t) memory_limit_mb * 1024 * 1024;
		limit.rlim_max = limit.rlim_cur;
		if (setrlimit (RLIMIT_AS, &limit) != 0)
			g_warning ("Unable to set worker memory limit: %s", g_strerror (errno));
	}

	return NULL;
}

/**
 * asw_worker_apply_font_overrides:
 *
 * Apply component-level overrides sent by the client to a loaded font.
 */
static void
asw_worker_apply_font_overrides (AswFont *font, GVariant *params)
{
	const gchar *preferred_lang = NULL;
	const gchar *sample_text = NULL;
	const gchar *sample_icon_text = NULL;
	g_autofree const gchar **extra_langs = NULL;

	if (g_variant_lookup (params, "preferred-language", "&s", &preferred_lang) &&
	    !as_is_empty (preferred_lang))
		asw_font_set_preferred_language (font, preferred_lang);

	if (g_variant_lookup (params, "extra-languages", "^a&s", &extra_langs)) {
		for (guint i = 0; extra_langs[i] != NULL; i++)
			asw_font_add_language (font, extra_langs[i]);
	}

	if (g_variant_lookup (params, "sample-text", "&s", &sample_text) &&
	    !as_is_empty (sample_text))
		asw_font_set_sample_text (font, sample_text);

	if (g_variant_lookup (params, "sample-icon-text", "&s", &sample_icon_text) &&
	    !as_is_empty (sample_icon_text))
		asw_font_set_sample_icon_text (font, sample_icon_text);
}

/**
 * asw_worker_load_font:
 *
 * Load the font transmitted with the given request and apply
 * all requested overrides to it.
 */
static AswFont *
asw_worker_load_font (AswWorker *worker, GVariant *params, GUnixFDList *fds, GError **error)
{
	AswFont *font = NULL;
	const gchar *basename = NULL;
	gint font_fd;

	font_fd = asw_worker_get_data_fd (params, fds, "font-fd", error);
	if (font_fd < 0)
		return NULL;

	/* NOTE: The basename is only used as fallback identifier and for heuristics,
	 * it never becomes a path here - so we do not validate it as one. Result
	 * filenames derived from it are validated separately. */
	g_variant_lookup (params, "basename", "&s", &basename);
	if (as_is_empty (basename)) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Font request was missing the font file basename.");
		close (font_fd);
		return NULL;
	}

	font = asw_font_new_from_fd (font_fd, basename, error);
	close (font_fd);
	if (font == NULL)
		return NULL;

	asw_worker_apply_font_overrides (font, params);
	return font;
}

/**
 * asw_worker_handle_process_image:
 *
 * Load an image and write a set of renditions of it into the output
 * descriptors the client provided.
 */
static GVariant *
asw_worker_handle_process_image (AswWorker *worker,
				 GVariant *params,
				 GUnixFDList *fds,
				 GError **error)
{
	g_autoptr(GBytes) img_bytes = NULL;
	g_autoptr(GVariant) targets = NULL;
	g_autoptr(AswImage) image = NULL;
	GVariantIter targets_iter;
	GVariant *target_dict = NULL;
	GVariantBuilder pb;
	GVariantBuilder results_builder;
	gint load_width = 0;
	gint load_height = 0;
	gint src_width, src_height;
	gint image_fd = -1;

	image_fd = asw_worker_get_data_fd (params, fds, "image-fd", error);
	if (image_fd < 0)
		return NULL;
	img_bytes = asw_memfd_map_bytes (image_fd, error);
	close (image_fd);
	if (img_bytes == NULL)
		return NULL;

	g_variant_lookup (params, "load-width", "i", &load_width);
	g_variant_lookup (params, "load-height", "i", &load_height);

	targets = g_variant_lookup_value (params, "targets", G_VARIANT_TYPE ("aa{sv}"));

	image = asw_image_new_from_data (g_bytes_get_data (img_bytes, NULL),
					 g_bytes_get_size (img_bytes),
					 load_width,
					 load_height,
					 error);
	if (image == NULL)
		return NULL;
	src_width = asw_image_get_width (image);
	src_height = asw_image_get_height (image);

	g_variant_builder_init (&results_builder, G_VARIANT_TYPE ("aa{sv}"));
	if (targets != NULL)
		g_variant_iter_init (&targets_iter, targets);
	while (targets != NULL && g_variant_iter_next (&targets_iter, "@a{sv}", &target_dict)) {
		g_autoptr(GVariant) target = target_dict;
		g_autoptr(GError) tmp_error = NULL;
		GVariantBuilder rb;
		guint32 format = ASC_IMAGE_FORMAT_UNKNOWN;
		gint width = 0;
		gint height = 0;
		guint32 scale_mode = ASC_IMAGE_SCALE_MODE_NONE;
		guint32 save_flags = 0;
		gboolean only_downscale = FALSE;
		gint min_src_width = 0;
		gint min_src_height = 0;
		gint max_src_width = 0;
		gint max_src_height = 0;
		gint result_width = 0;
		gint result_height = 0;
		gboolean skipped = FALSE;
		gboolean saved = FALSE;
		gint out_fd = -1;

		g_variant_lookup (target, "format", "u", &format);
		g_variant_lookup (target, "width", "i", &width);
		g_variant_lookup (target, "height", "i", &height);
		g_variant_lookup (target, "scale-mode", "u", &scale_mode);
		g_variant_lookup (target, "save-flags", "u", &save_flags);
		g_variant_lookup (target, "only-downscale", "b", &only_downscale);
		g_variant_lookup (target, "min-src-size", "(ii)", &min_src_width, &min_src_height);
		g_variant_lookup (target, "max-src-size", "(ii)", &max_src_width, &max_src_height);

		g_variant_builder_init (&rb, G_VARIANT_TYPE ("a{sv}"));

		/* check the skip-conditions for this rendition */
		if (min_src_width > 0 && src_width < min_src_width)
			skipped = TRUE;
		if (min_src_height > 0 && src_height < min_src_height)
			skipped = TRUE;
		if (max_src_width > 0 && src_width > max_src_width)
			skipped = TRUE;
		if (max_src_height > 0 && src_height > max_src_height)
			skipped = TRUE;
		if (only_downscale && (width > src_width || height > src_height))
			skipped = TRUE;

		if (skipped) {
			g_variant_builder_add (&rb,
					       "{sv}",
					       "skipped",
					       g_variant_new_boolean (TRUE));
			g_variant_builder_add_value (&results_builder, g_variant_builder_end (&rb));
			continue;
		}

		/* the client handed us an already-open descriptor for this rendition */
		out_fd = asw_worker_get_output_fd (target, fds, &tmp_error);
		if (out_fd < 0) {
			g_variant_builder_add (&rb,
					       "{sv}",
					       "error",
					       g_variant_new_string (tmp_error->message));
			g_variant_builder_add_value (&results_builder, g_variant_builder_end (&rb));
			continue;
		}

		switch ((AscImageScaleMode) scale_mode) {
		case ASC_IMAGE_SCALE_MODE_NONE:
			saved = asw_image_save_fd (image,
						   out_fd,
						   (AscImageFormat) format,
						   0,
						   0,
						   (AscImageSaveFlags) save_flags,
						   &tmp_error);
			result_width = src_width;
			result_height = src_height;
			break;
		case ASC_IMAGE_SCALE_MODE_PAD:
			saved = asw_image_save_fd (image,
						   out_fd,
						   (AscImageFormat) format,
						   width,
						   height,
						   (AscImageSaveFlags) save_flags,
						   &tmp_error);
			result_width = width;
			result_height = height;
			break;
		case ASC_IMAGE_SCALE_MODE_FIT_WIDTH:
		case ASC_IMAGE_SCALE_MODE_FIT_HEIGHT: {
			/* scale a temporary image proportionally, then store it in its new native size */
			g_autoptr(AswImage) scaled_img = asw_image_new ();
			gdouble scale;
			gint fit_width;
			gint fit_height;

			/* determine the resulting size, using the exact same maths that the
			 * scaling functions use, so we can reject degenerate results (the
			 * client relies on being able to predict these dimensions) */
			if (scale_mode == ASC_IMAGE_SCALE_MODE_FIT_WIDTH) {
				scale = (gdouble) width / (gdouble) src_width;
				fit_width = width;
				fit_height = (gint) floor (src_height * scale);
			} else {
				scale = (gdouble) height / (gdouble) src_height;
				fit_width = (gint) floor (src_width * scale);
				fit_height = height;
			}
			if (fit_width < 1 || fit_height < 1) {
				g_set_error (&tmp_error,
					     ASC_MEDIA_ERROR,
					     ASC_MEDIA_ERROR_FAILED,
					     "Scaling the %ix%i image to fit %ix%i would result in "
					     "an image of %ix%i pixels.",
					     src_width,
					     src_height,
					     width,
					     height,
					     fit_width,
					     fit_height);
				break;
			}

			asw_image_set_vips (scaled_img, asw_image_get_vips (image));
			if (scale_mode == ASC_IMAGE_SCALE_MODE_FIT_WIDTH)
				asw_image_scale_to_width (scaled_img, width);
			else
				asw_image_scale_to_height (scaled_img, height);
			saved = asw_image_save_fd (scaled_img,
						   out_fd,
						   (AscImageFormat) format,
						   0,
						   0,
						   (AscImageSaveFlags) save_flags,
						   &tmp_error);
			result_width = asw_image_get_width (scaled_img);
			result_height = asw_image_get_height (scaled_img);
			break;
		}
		default:
			g_set_error (&tmp_error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Unknown image scale mode: %u",
				     scale_mode);
			break;
		}

		close (out_fd);

		if (!saved) {
			g_variant_builder_add (&rb,
					       "{sv}",
					       "error",
					       g_variant_new_string (tmp_error->message));
		} else {
			g_variant_builder_add (&rb,
					       "{sv}",
					       "width",
					       g_variant_new_int32 (result_width));
			g_variant_builder_add (&rb,
					       "{sv}",
					       "height",
					       g_variant_new_int32 (result_height));
		}
		g_variant_builder_add_value (&results_builder, g_variant_builder_end (&rb));
	}

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&pb, "{sv}", "src-width", g_variant_new_int32 (src_width));
	g_variant_builder_add (&pb, "{sv}", "src-height", g_variant_new_int32 (src_height));
	g_variant_builder_add (&pb, "{sv}", "results", g_variant_builder_end (&results_builder));

	return g_variant_builder_end (&pb);
}

/**
 * asw_worker_variant_string_or_empty:
 */
static GVariant *
asw_worker_variant_string_or_empty (const gchar *str)
{
	return g_variant_new_string (str != NULL ? str : "");
}

/**
 * asw_worker_handle_font_info:
 *
 * Extract all metadata from a font that the client needs.
 */
static GVariant *
asw_worker_handle_font_info (AswWorker *worker, GVariant *params, GUnixFDList *fds, GError **error)
{
	g_autoptr(AswFont) font = NULL;
	g_autoptr(GList) language_list = NULL;
	GVariantBuilder pb;
	GVariantBuilder lang_builder;

	font = asw_worker_load_font (worker, params, fds, error);
	if (font == NULL)
		return NULL;

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "family",
			       asw_worker_variant_string_or_empty (asw_font_get_family (font)));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "style",
			       asw_worker_variant_string_or_empty (asw_font_get_style (font)));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "fullname",
			       asw_worker_variant_string_or_empty (asw_font_get_fullname (font)));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "id",
			       asw_worker_variant_string_or_empty (asw_font_get_id (font)));
	g_variant_builder_add (
	    &pb,
	    "{sv}",
	    "description",
	    asw_worker_variant_string_or_empty (asw_font_get_description (font)));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "homepage",
			       asw_worker_variant_string_or_empty (asw_font_get_homepage (font)));
	g_variant_builder_add (
	    &pb,
	    "{sv}",
	    "preferred-language",
	    asw_worker_variant_string_or_empty (asw_font_get_preferred_language (font)));
	g_variant_builder_add (
	    &pb,
	    "{sv}",
	    "sample-text",
	    asw_worker_variant_string_or_empty (asw_font_get_sample_text (font)));
	g_variant_builder_add (
	    &pb,
	    "{sv}",
	    "sample-icon-text",
	    asw_worker_variant_string_or_empty (asw_font_get_sample_icon_text (font)));

	g_variant_builder_init (&lang_builder, G_VARIANT_TYPE ("as"));
	language_list = asw_font_get_language_list (font);
	for (GList *l = language_list; l != NULL; l = l->next)
		g_variant_builder_add (&lang_builder, "s", (const gchar *) l->data);
	g_variant_builder_add (&pb, "{sv}", "languages", g_variant_builder_end (&lang_builder));

	return g_variant_builder_end (&pb);
}

/**
 * asw_worker_handle_render_font:
 *
 * Render font specimen cards or font icons into the output descriptors
 * the client provided.
 */
static GVariant *
asw_worker_handle_render_font (AswWorker *worker,
			       GVariant *params,
			       GUnixFDList *fds,
			       gboolean render_icon,
			       GError **error)
{
	g_autoptr(AswFont) font = NULL;
	g_autoptr(GVariant) entries = NULL;
	GVariantIter entries_iter;
	GVariant *entry_dict = NULL;
	GVariantBuilder pb;
	GVariantBuilder results_builder;
	const gchar *info_label = NULL;

	entries = g_variant_lookup_value (params, "entries", G_VARIANT_TYPE ("aa{sv}"));
	if (entries == NULL || g_variant_n_children (entries) == 0) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Font render request contained no entries.");
		return NULL;
	}

	font = asw_worker_load_font (worker, params, fds, error);
	if (font == NULL)
		return NULL;

	g_variant_lookup (params, "info-label", "&s", &info_label);
	if (as_is_empty (info_label))
		info_label = NULL;

	g_variant_builder_init (&results_builder, G_VARIANT_TYPE ("aa{sv}"));
	g_variant_iter_init (&entries_iter, entries);
	while (g_variant_iter_next (&entries_iter, "@a{sv}", &entry_dict)) {
		g_autoptr(GVariant) entry = entry_dict;
		g_autoptr(GError) tmp_error = NULL;
		GVariantBuilder rb;
		guint32 format = ASC_IMAGE_FORMAT_UNKNOWN;
		gint width = 0;
		gint height = 0;
		gint actual_width = 0;
		gint actual_height = 0;
		gboolean rendered = FALSE;
		gint out_fd = -1;

		g_variant_lookup (entry, "format", "u", &format);
		g_variant_lookup (entry, "width", "i", &width);
		g_variant_lookup (entry, "height", "i", &height);

		g_variant_builder_init (&rb, G_VARIANT_TYPE ("a{sv}"));

		/* the client handed us an already-open descriptor for this entry */
		out_fd = asw_worker_get_output_fd (entry, fds, &tmp_error);
		if (out_fd < 0) {
			g_variant_builder_add (&rb,
					       "{sv}",
					       "error",
					       g_variant_new_string (tmp_error->message));
			g_variant_builder_add_value (&results_builder, g_variant_builder_end (&rb));
			continue;
		}

		if (render_icon) {
			/* font icons are always square, so the width is our canvas size */
			rendered = asw_font_render_icon_to_fd (font,
							       out_fd,
							       (AscImageFormat) format,
							       width,
							       &actual_width,
							       &actual_height,
							       &tmp_error);
		} else {
			rendered = asw_font_render_card_to_fd (font,
							       out_fd,
							       (AscImageFormat) format,
							       width,
							       height,
							       info_label,
							       &actual_width,
							       &actual_height,
							       &tmp_error);
		}
		close (out_fd);

		if (!rendered) {
			g_variant_builder_add (&rb,
					       "{sv}",
					       "error",
					       g_variant_new_string (tmp_error->message));
		} else {
			g_variant_builder_add (&rb,
					       "{sv}",
					       "width",
					       g_variant_new_int32 (actual_width));
			g_variant_builder_add (&rb,
					       "{sv}",
					       "height",
					       g_variant_new_int32 (actual_height));
		}
		g_variant_builder_add_value (&results_builder, g_variant_builder_end (&rb));
	}

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&pb, "{sv}", "results", g_variant_builder_end (&results_builder));
	return g_variant_builder_end (&pb);
}

/**
 * asw_worker_handle_probe_video:
 *
 * Run ffprobe on the transmitted video file and return the raw results.
 */
static GVariant *
asw_worker_handle_probe_video (AswWorker *worker,
			       GVariant *params,
			       GUnixFDList *fds,
			       GError **error)
{
	g_autoptr(AswVideoProbe) probe = NULL;
	GVariantBuilder pb;
	gint video_fd;

	/* videos can be large, so the client passes the file to us directly */
	video_fd = asw_worker_get_readonly_file_fd (params, fds, "video-fd", error);
	if (video_fd < 0)
		return NULL;

	probe = asw_probe_video (video_fd, error);
	close (video_fd);
	if (probe == NULL)
		return NULL;

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "codec-name",
			       asw_worker_variant_string_or_empty (probe->codec_name));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "audio-codec-name",
			       asw_worker_variant_string_or_empty (probe->audio_codec_name));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "format-name",
			       asw_worker_variant_string_or_empty (probe->format_name));
	g_variant_builder_add (&pb, "{sv}", "width", g_variant_new_int32 (probe->width));
	g_variant_builder_add (&pb, "{sv}", "height", g_variant_new_int32 (probe->height));

	return g_variant_builder_end (&pb);
}

/**
 * asw_worker_run:
 *
 * Run the worker's request loop until the client shuts us down
 * or closes the connection.
 *
 * Returns: The exit status for the worker process.
 */
gint
asw_worker_run (AswWorker *worker)
{
	AswWorkerPrivate *priv = GET_PRIVATE (worker);

	while (TRUE) {
		g_autoptr(GVariant) params = NULL;
		g_autoptr(GVariant) payload = NULL;
		g_autoptr(GUnixFDList) fds = NULL;
		g_autoptr(GError) error = NULL;
		g_autoptr(GError) op_error = NULL;
		guint32 request_id = 0;
		AscMediaOp op = ASC_MEDIA_OP_UNKNOWN;
		gboolean eof = FALSE;

		if (!asw_ipc_receive_request (priv->socket,
					      &request_id,
					      &op,
					      &params,
					      &fds,
					      &eof,
					      &error)) {
			if (eof) {
				/* the client is gone, we are done */
				return 0;
			}
			g_printerr ("asc-mediaworker: Failed to read request: %s\n",
				    error->message);
			return 2;
		}

		switch (op) {
		case ASC_MEDIA_OP_SETUP:
			payload = asw_worker_handle_setup (worker, params, fds, &op_error);
			break;
		case ASC_MEDIA_OP_PROCESS_IMAGE:
			payload = asw_worker_handle_process_image (worker, params, fds, &op_error);
			break;
		case ASC_MEDIA_OP_FONT_INFO:
			payload = asw_worker_handle_font_info (worker, params, fds, &op_error);
			break;
		case ASC_MEDIA_OP_RENDER_FONT_CARD:
			payload = asw_worker_handle_render_font (worker,
								 params,
								 fds,
								 FALSE,
								 &op_error);
			break;
		case ASC_MEDIA_OP_RENDER_FONT_ICON:
			payload = asw_worker_handle_render_font (worker,
								 params,
								 fds,
								 TRUE,
								 &op_error);
			break;
		case ASC_MEDIA_OP_PROBE_VIDEO:
			payload = asw_worker_handle_probe_video (worker, params, fds, &op_error);
			break;
		case ASC_MEDIA_OP_SHUTDOWN:
			if (!asw_ipc_send_response (priv->socket,
						    request_id,
						    ASC_MEDIA_STATUS_OK,
						    NULL,
						    &error)) {
				g_printerr ("asc-mediaworker: Failed to acknowledge shutdown: %s\n",
					    error->message);
				return 2;
			}
			return 0;
		default:
			g_set_error (&op_error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "Unknown media worker operation requested: %u",
				     (guint) op);
			break;
		}

		if (payload != NULL)
			g_variant_ref_sink (payload);

		if (op_error != NULL) {
			if (!asw_ipc_send_error_response (priv->socket,
							  request_id,
							  op_error,
							  &error)) {
				g_printerr ("asc-mediaworker: Failed to send error response: %s\n",
					    error->message);
				return 2;
			}
		} else {
			if (!asw_ipc_send_response (priv->socket,
						    request_id,
						    ASC_MEDIA_STATUS_OK,
						    payload,
						    &error)) {
				g_printerr ("asc-mediaworker: Failed to send response: %s\n",
					    error->message);
				return 2;
			}
		}
	}
}
