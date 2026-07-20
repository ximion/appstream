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
 * SECTION:asc-media
 * @short_description: Sandboxable media processing operations.
 * @include: appstream-compose.h
 *
 * #AscMedia provides all high-level media processing operations that
 * appstream-compose needs (processing images, extracting font metadata,
 * rendering font specimens, probing videos).
 *
 * The actual media processing is performed by a separate worker process
 * (`asc-mediaworker`), so parsing of untrusted media data is isolated from
 * the process using libappstream-compose. Input data is passed to the worker
 * exclusively via sealed memory file descriptors, and the worker writes its
 * results through a directory file descriptor provided per request, so it
 * requires no ambient filesystem access and can be sandboxed.
 *
 * An #AscMedia instance manages the lifecycle of exactly one worker process,
 * which is spawned lazily on first use and respawned (up to a limit) in case
 * it crashes. Instances must not be shared between threads - each thread
 * processing media should use its own #AscMedia instance (and thereby its
 * own worker process).
 */

#include "config.h"
#include "asc-media.h"
#include "asc-media-private.h"

#include <errno.h>
#include <fcntl.h>
#include <gio/gunixfdlist.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "as-utils-private.h"
#include "asc-globals-private.h"
#include "asc-media-ipc.h"

/* maximum amount of captured worker stderr output we keep for error reporting */
#define ASC_MEDIA_STDERR_BUF_SIZE 16384

/* how often we try to respawn a crashed worker before giving up */
#define ASC_MEDIA_RESPAWN_LIMIT 3

struct _AscMedia {
	GObject parent_instance;
};

typedef struct {
	gchar *worker_path;
	guint timeout_secs;
	guint64 max_input_size;
	guint32 memory_limit_mb;

	GSubprocess *worker_proc;
	GSocket *socket;
	GInputStream *stderr_stream;
	GString *stderr_buf;

	guint32 last_request_id;
	guint failure_count;
} AscMediaPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscMedia, asc_media, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_media_get_instance_private (o))

/**
 * asc_media_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
asc_media_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AscMediaError");
	return quark;
}

/**
 * asc_image_format_to_string:
 * @format: the %AscImageFormat.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @format
 **/
const gchar *
asc_image_format_to_string (AscImageFormat format)
{
	if (format == ASC_IMAGE_FORMAT_PNG)
		return "png";
	if (format == ASC_IMAGE_FORMAT_JXL)
		return "jxl";
	if (format == ASC_IMAGE_FORMAT_AVIF)
		return "avif";
	if (format == ASC_IMAGE_FORMAT_WEBP)
		return "webp";
	if (format == ASC_IMAGE_FORMAT_SVG)
		return "svg";
	if (format == ASC_IMAGE_FORMAT_SVGZ)
		return "svgz";
	if (format == ASC_IMAGE_FORMAT_JPEG)
		return "jpeg";
	if (format == ASC_IMAGE_FORMAT_GIF)
		return "gif";

	if (format == ASC_IMAGE_FORMAT_XPM)
		return "xpm";

	return NULL;
}

/**
 * asc_image_format_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AscImageFormat or %ASC_IMAGE_FORMAT_UNKNOWN for unknown
 **/
AscImageFormat
asc_image_format_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "png") == 0)
		return ASC_IMAGE_FORMAT_PNG;
	if (g_strcmp0 (str, "jxl") == 0)
		return ASC_IMAGE_FORMAT_JXL;
	if (g_strcmp0 (str, "avif") == 0)
		return ASC_IMAGE_FORMAT_AVIF;
	if (g_strcmp0 (str, "webp") == 0)
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_strcmp0 (str, "svg") == 0)
		return ASC_IMAGE_FORMAT_SVG;
	if (g_strcmp0 (str, "svgz") == 0)
		return ASC_IMAGE_FORMAT_SVGZ;
	if (g_strcmp0 (str, "jpeg") == 0)
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_strcmp0 (str, "gif") == 0)
		return ASC_IMAGE_FORMAT_GIF;
	if (g_strcmp0 (str, "xpm") == 0)
		return ASC_IMAGE_FORMAT_XPM;

	return ASC_IMAGE_FORMAT_UNKNOWN;
}

/**
 * asc_image_format_from_filename:
 * @fname: the filename.
 *
 * Returns the image format type based on the given file's filename.
 *
 * Returns: a #AscImageFormat or %ASC_IMAGE_FORMAT_UNKNOWN for unknown
 **/
AscImageFormat
asc_image_format_from_filename (const gchar *fname)
{
	g_autofree gchar *fname_low = g_ascii_strdown (fname, -1);

	if (g_str_has_suffix (fname_low, ".png"))
		return ASC_IMAGE_FORMAT_PNG;
	if (g_str_has_suffix (fname_low, ".jxl"))
		return ASC_IMAGE_FORMAT_JXL;
	if (g_str_has_suffix (fname_low, ".avif"))
		return ASC_IMAGE_FORMAT_AVIF;
	if (g_str_has_suffix (fname_low, ".webp"))
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_str_has_suffix (fname_low, ".svg"))
		return ASC_IMAGE_FORMAT_SVG;
	if (g_str_has_suffix (fname_low, ".svgz"))
		return ASC_IMAGE_FORMAT_SVGZ;
	if (g_str_has_suffix (fname_low, ".jpeg") || g_str_has_suffix (fname_low, ".jpg"))
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_str_has_suffix (fname_low, ".gif"))
		return ASC_IMAGE_FORMAT_GIF;
	if (g_str_has_suffix (fname_low, ".xpm"))
		return ASC_IMAGE_FORMAT_XPM;

	return ASC_IMAGE_FORMAT_UNKNOWN;
}

/**
 * asc_image_target_new:
 * @name: Filename the rendition should be stored as.
 * @scale_mode: an #AscImageScaleMode
 * @width: Target width.
 * @height: Target height.
 *
 * Create a new #AscImageTarget.
 *
 * Returns: (transfer full): an #AscImageTarget
 */
AscImageTarget *
asc_image_target_new (const gchar *name, AscImageScaleMode scale_mode, gint width, gint height)
{
	AscImageTarget *target;

	target = g_new0 (AscImageTarget, 1);
	target->name = g_strdup (name);
	target->scale_mode = scale_mode;
	target->width = width;
	target->height = height;

	return target;
}

/**
 * asc_image_target_free:
 *
 * Free an #AscImageTarget.
 */
void
asc_image_target_free (AscImageTarget *target)
{
	if (target == NULL)
		return;
	g_free (target->name);
	g_free (target->error_msg);
	g_free (target);
}

static AscImageTarget *
asc_image_target_copy (AscImageTarget *target)
{
	AscImageTarget *copy;

	copy = g_new0 (AscImageTarget, 1);
	*copy = *target;
	copy->name = g_strdup (target->name);
	copy->error_msg = g_strdup (target->error_msg);

	return copy;
}

G_DEFINE_BOXED_TYPE (AscImageTarget, asc_image_target, asc_image_target_copy, asc_image_target_free)

/**
 * asc_font_info_free:
 *
 * Free an #AscFontInfo.
 */
void
asc_font_info_free (AscFontInfo *info)
{
	if (info == NULL)
		return;
	g_free (info->family);
	g_free (info->style);
	g_free (info->fullname);
	g_free (info->id);
	g_free (info->description);
	g_free (info->homepage);
	g_strfreev (info->languages);
	g_free (info->preferred_language);
	g_free (info->sample_text);
	g_free (info->sample_icon_text);
	g_free (info);
}

static AscFontInfo *
asc_font_info_copy (AscFontInfo *info)
{
	AscFontInfo *copy;

	copy = g_new0 (AscFontInfo, 1);
	copy->family = g_strdup (info->family);
	copy->style = g_strdup (info->style);
	copy->fullname = g_strdup (info->fullname);
	copy->id = g_strdup (info->id);
	copy->description = g_strdup (info->description);
	copy->homepage = g_strdup (info->homepage);
	copy->languages = g_strdupv (info->languages);
	copy->preferred_language = g_strdup (info->preferred_language);
	copy->sample_text = g_strdup (info->sample_text);
	copy->sample_icon_text = g_strdup (info->sample_icon_text);

	return copy;
}

G_DEFINE_BOXED_TYPE (AscFontInfo, asc_font_info, asc_font_info_copy, asc_font_info_free)

static void asc_media_shutdown_worker (AscMedia *media, gboolean force);

static void
asc_media_finalize (GObject *object)
{
	AscMedia *media = ASC_MEDIA (object);
	AscMediaPrivate *priv = GET_PRIVATE (media);

	asc_media_shutdown_worker (media, FALSE);

	g_free (priv->worker_path);
	if (priv->stderr_buf != NULL)
		g_string_free (priv->stderr_buf, TRUE);

	G_OBJECT_CLASS (asc_media_parent_class)->finalize (object);
}

static void
asc_media_init (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);

	priv->timeout_secs = 120;
	priv->stderr_buf = g_string_new (NULL);
}

static void
asc_media_class_init (AscMediaClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_media_finalize;
}

/**
 * asc_media_new:
 *
 * Creates a new #AscMedia.
 **/
AscMedia *
asc_media_new (void)
{
	AscMedia *media;
	media = g_object_new (ASC_TYPE_MEDIA, NULL);
	return ASC_MEDIA (media);
}

/**
 * asc_media_get_request_timeout:
 * @media: an #AscMedia instance.
 *
 * Get the request timeout in seconds.
 */
guint
asc_media_get_request_timeout (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	return priv->timeout_secs;
}

/**
 * asc_media_set_request_timeout:
 * @media: an #AscMedia instance.
 * @seconds: New timeout in seconds, 0 for no timeout.
 *
 * Set the time a single media operation may take before the worker
 * is assumed to hang and gets killed.
 */
void
asc_media_set_request_timeout (AscMedia *media, guint seconds)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	priv->timeout_secs = seconds;
	if (priv->socket != NULL)
		g_socket_set_timeout (priv->socket, priv->timeout_secs);
}

/**
 * asc_media_get_worker_path:
 * @media: an #AscMedia instance.
 *
 * Get the path to the worker binary this instance will use,
 * or %NULL if the global default is used.
 */
const gchar *
asc_media_get_worker_path (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	return priv->worker_path;
}

/**
 * asc_media_set_worker_path:
 * @media: an #AscMedia instance.
 * @path: Path to an asc-mediaworker binary, or %NULL to use the default.
 *
 * Override the media worker binary used by this instance.
 */
void
asc_media_set_worker_path (AscMedia *media, const gchar *path)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	as_assign_string_safe (priv->worker_path, path);
}

/**
 * asc_media_set_max_input_size:
 * @media: an #AscMedia instance.
 * @max_size: Maximum media input size in bytes, 0 for no limit.
 *
 * Set the maximum size of media data the worker will accept for
 * processing. The limit is applied when the next worker process
 * is spawned.
 */
void
asc_media_set_max_input_size (AscMedia *media, guint64 max_size)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	priv->max_input_size = max_size;
}

/**
 * asc_media_set_memory_limit:
 * @media: an #AscMedia instance.
 * @limit_mib: Address space limit for the worker process in MiB, 0 for no limit.
 *
 * Limit the amount of memory the worker process may use.
 * The limit is applied when the next worker process is spawned.
 */
void
asc_media_set_memory_limit (AscMedia *media, guint32 limit_mib)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	priv->memory_limit_mb = limit_mib;
}

/**
 * asc_media_drain_worker_stderr:
 *
 * Read any pending stderr output from the worker process into
 * our (bounded) capture buffer.
 */
static void
asc_media_drain_worker_stderr (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);

	if (priv->stderr_stream == NULL)
		return;

	while (TRUE) {
		gchar buf[4096];
		gssize len;

		len = g_pollable_input_stream_read_nonblocking (
		    G_POLLABLE_INPUT_STREAM (priv->stderr_stream),
		    buf,
		    sizeof (buf),
		    NULL,
		    NULL);
		if (len <= 0)
			break;
		g_string_append_len (priv->stderr_buf, buf, len);

		/* only ever keep the tail of the output */
		if (priv->stderr_buf->len > ASC_MEDIA_STDERR_BUF_SIZE)
			g_string_erase (priv->stderr_buf,
					0,
					priv->stderr_buf->len - ASC_MEDIA_STDERR_BUF_SIZE);
	}
}

/**
 * asc_media_describe_worker_death:
 *
 * Create a human-readable description of how and why the worker
 * process died, including its last standard error output.
 */
static gchar *
asc_media_describe_worker_death (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_autoptr(GString) desc = g_string_new (NULL);

	if (priv->worker_proc != NULL) {
		if (g_subprocess_get_if_exited (priv->worker_proc))
			g_string_append_printf (desc,
						"Worker exited with status %i.",
						g_subprocess_get_exit_status (priv->worker_proc));
		else if (g_subprocess_get_if_signaled (priv->worker_proc))
			g_string_append_printf (desc,
						"Worker was killed by signal %i.",
						g_subprocess_get_term_sig (priv->worker_proc));
		else
			g_string_append (desc, "Worker terminated unexpectedly.");
	} else {
		g_string_append (desc, "Worker terminated unexpectedly.");
	}

	if (priv->stderr_buf->len > 0) {
		g_strchomp (priv->stderr_buf->str);
		priv->stderr_buf->len = strlen (priv->stderr_buf->str);
		g_string_append_printf (desc, " Last output: %s", priv->stderr_buf->str);
	}

	return g_string_free (g_steal_pointer (&desc), FALSE);
}

/**
 * asc_media_shutdown_worker:
 * @force: Whether to kill the worker immediately instead of asking it to quit.
 *
 * Terminate the worker process (if it is running) and clean up
 * all connection state.
 */
static void
asc_media_shutdown_worker (AscMedia *media, gboolean force)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);

	if (priv->worker_proc != NULL) {
		gboolean clean_quit = FALSE;

		if (!force && priv->socket != NULL) {
			g_autoptr(GVariant) payload = NULL;
			g_autoptr(GError) tmp_error = NULL;
			guint32 rid, status;
			gboolean eof = FALSE;

			/* ask the worker politely to quit */
			if (asc_media_wire_send_request (priv->socket,
							 ++priv->last_request_id,
							 ASC_MEDIA_OP_SHUTDOWN,
							 NULL,
							 NULL,
							 &tmp_error)) {
				clean_quit = asc_media_wire_receive_response (priv->socket,
									      &rid,
									      &status,
									      &payload,
									      &eof,
									      &tmp_error);
			}
		}

		if (!clean_quit && !force) {
			/* closing our socket end makes the worker exit on EOF */
			g_clear_object (&priv->socket);
		}
		if (force)
			g_subprocess_force_exit (priv->worker_proc);

		g_subprocess_wait (priv->worker_proc, NULL, NULL);
	}

	asc_media_drain_worker_stderr (media);

	g_clear_object (&priv->socket);
	g_clear_object (&priv->stderr_stream);
	g_clear_object (&priv->worker_proc);
}

/**
 * asc_media_stop:
 * @media: an #AscMedia instance.
 *
 * Stop the worker process of this instance, if it is running.
 * A new worker will be spawned automatically if another media
 * operation is requested.
 */
void
asc_media_stop (AscMedia *media)
{
	asc_media_shutdown_worker (media, FALSE);
}

/**
 * asc_media_resolve_worker_binary:
 *
 * Get the worker binary path to use for spawning.
 */
static const gchar *
asc_media_resolve_worker_binary (AscMedia *media)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);

	if (priv->worker_path != NULL)
		return priv->worker_path;
	return asc_globals_get_mediaworker_binary ();
}

/**
 * asc_media_worker_setup:
 *
 * Send the setup request with our current global configuration
 * to a freshly spawned worker.
 */
static gboolean
asc_media_worker_setup (AscMedia *media, GError **error)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_autoptr(GVariant) payload = NULL;
	GVariantBuilder pb;
	const gchar *optipng_path = asc_globals_get_optipng_binary ();
	const gchar *ffprobe_path = asc_globals_get_ffprobe_binary ();
	guint32 rid = 0;
	guint32 status = 0;
	gboolean eof = FALSE;

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "use-optipng",
			       g_variant_new_boolean (asc_globals_get_use_optipng ()));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "optipng-path",
			       g_variant_new_string (optipng_path ? optipng_path : ""));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "ffprobe-path",
			       g_variant_new_string (ffprobe_path ? ffprobe_path : ""));
	if (priv->max_input_size > 0)
		g_variant_builder_add (&pb,
				       "{sv}",
				       "max-input-size",
				       g_variant_new_uint64 (priv->max_input_size));
	if (priv->memory_limit_mb > 0)
		g_variant_builder_add (&pb,
				       "{sv}",
				       "memory-limit-mb",
				       g_variant_new_uint32 (priv->memory_limit_mb));

	if (!asc_media_wire_send_request (priv->socket,
					  ++priv->last_request_id,
					  ASC_MEDIA_OP_SETUP,
					  g_variant_builder_end (&pb),
					  NULL,
					  error))
		return FALSE;
	if (!asc_media_wire_receive_response (priv->socket, &rid, &status, &payload, &eof, error))
		return FALSE;
	if (rid != priv->last_request_id || status != ASC_MEDIA_STATUS_OK) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Worker rejected its setup request.");
		return FALSE;
	}

	return TRUE;
}

/**
 * asc_media_spawn_worker:
 *
 * Spawn a new worker process and perform the initial handshake with it.
 */
static gboolean
asc_media_spawn_worker (AscMedia *media, GError **error)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_autoptr(GSubprocessLauncher) launcher = NULL;
	g_autoptr(GVariant) hello = NULL;
	g_autoptr(GError) tmp_error = NULL;
	const gchar *worker_bin = NULL;
	const gchar *program_version = NULL;
	guint32 protocol_version = 0;
	guint32 rid = 0;
	guint32 status = 0;
	gboolean eof = FALSE;
	int sv[2];

	worker_bin = asc_media_resolve_worker_binary (media);
	if (worker_bin == NULL) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_DEAD_WORKER,
				     "Unable to find the asc-mediaworker binary. Check if the "
				     "AppStream installation is complete.");
		return FALSE;
	}

	if (socketpair (AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) != 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Unable to create worker socket pair: %s",
			     g_strerror (errno));
		return FALSE;
	}

	launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDERR_PIPE);
	g_subprocess_launcher_take_fd (launcher, sv[1], ASC_MEDIA_SOCKET_FD);

	priv->worker_proc = g_subprocess_launcher_spawn (launcher, &tmp_error, worker_bin, NULL);
	/* drop the launcher immediately: it holds the worker-side socket fd open in our
	 * process, which would prevent us from seeing an EOF if the worker dies */
	g_clear_object (&launcher);
	if (priv->worker_proc == NULL) {
		close (sv[0]);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Unable to spawn media worker '%s': %s",
			     worker_bin,
			     tmp_error->message);
		return FALSE;
	}
	priv->stderr_stream = g_object_ref (g_subprocess_get_stderr_pipe (priv->worker_proc));
	g_string_truncate (priv->stderr_buf, 0);

	priv->socket = g_socket_new_from_fd (sv[0], &tmp_error);
	if (priv->socket == NULL) {
		close (sv[0]);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Unable to set up worker communication: %s",
			     tmp_error->message);
		asc_media_shutdown_worker (media, TRUE);
		return FALSE;
	}
	g_socket_set_blocking (priv->socket, TRUE);
	g_socket_set_timeout (priv->socket, priv->timeout_secs);

	/* receive the hello message and validate that the worker matches us exactly */
	if (!asc_media_wire_receive_response (priv->socket,
					      &rid,
					      &status,
					      &hello,
					      &eof,
					      &tmp_error)) {
		g_autofree gchar *death_desc = NULL;
		asc_media_shutdown_worker (media, eof ? FALSE : TRUE);
		death_desc = asc_media_describe_worker_death (media);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Media worker vanished during handshake: %s %s",
			     tmp_error != NULL ? tmp_error->message : "",
			     death_desc);
		return FALSE;
	}

	g_variant_lookup (hello, "protocol-version", "u", &protocol_version);
	g_variant_lookup (hello, "program-version", "&s", &program_version);
	if (rid != 0 || status != ASC_MEDIA_STATUS_OK ||
	    protocol_version != ASC_MEDIA_PROTOCOL_VERSION ||
	    !as_str_equal0 (program_version, PACKAGE_VERSION)) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_PROTOCOL,
			     "Media worker '%s' is incompatible with this version of "
			     "libappstream-compose (worker version: %s, expected: %s).",
			     worker_bin,
			     program_version ? program_version : "unknown",
			     PACKAGE_VERSION);
		asc_media_shutdown_worker (media, TRUE);
		return FALSE;
	}

	/* configure the new worker */
	if (!asc_media_worker_setup (media, &tmp_error)) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Unable to configure the media worker: %s",
			     tmp_error->message);
		asc_media_shutdown_worker (media, TRUE);
		return FALSE;
	}

	g_debug ("Media worker started: %s", worker_bin);
	return TRUE;
}

/**
 * asc_media_ensure_worker:
 * @media: an #AscMedia instance.
 * @error: A #GError or %NULL
 *
 * Ensure a media worker process is running, spawning one if necessary.
 * This function is called implicitly by all media operations, calling
 * it explicitly is only useful to make worker startup problems surface
 * early.
 *
 * Returns: %TRUE if the worker is running.
 */
gboolean
asc_media_ensure_worker (AscMedia *media, GError **error)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);

	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);

	if (priv->socket != NULL)
		return TRUE;

	if (priv->failure_count >= ASC_MEDIA_RESPAWN_LIMIT) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "The media worker failed %u times, refusing to restart it again.",
			     priv->failure_count);
		return FALSE;
	}

	if (!asc_media_spawn_worker (media, error)) {
		priv->failure_count++;
		return FALSE;
	}

	return TRUE;
}

/**
 * asc_media_call:
 *
 * Perform a media operation: send the request, wait for the response
 * and handle all worker failure modes.
 *
 * Returns: (transfer full): The response payload, or %NULL on any error.
 */
static GVariant *
asc_media_call (AscMedia *media, AscMediaOp op, GVariant *params, GUnixFDList *fds, GError **error)
{
	AscMediaPrivate *priv = GET_PRIVATE (media);
	g_autoptr(GVariant) params_ref = g_variant_ref_sink (params);
	g_autoptr(GVariant) payload = NULL;
	g_autoptr(GError) tmp_error = NULL;
	guint32 request_id;
	guint32 rid = 0;
	guint32 status = 0;
	gboolean eof = FALSE;

	if (!asc_media_ensure_worker (media, error))
		return NULL;

	request_id = ++priv->last_request_id;
	if (!asc_media_wire_send_request (priv->socket,
					  request_id,
					  op,
					  params_ref,
					  fds,
					  &tmp_error))
		goto worker_failed;

	if (!asc_media_wire_receive_response (priv->socket,
					      &rid,
					      &status,
					      &payload,
					      &eof,
					      &tmp_error)) {
		if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) {
			priv->failure_count++;
			asc_media_shutdown_worker (media, TRUE);
			g_set_error (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_TIMEOUT,
				     "Media operation timed out after %u seconds, the worker "
				     "process was killed.",
				     priv->timeout_secs);
			return NULL;
		}
		goto worker_failed;
	}

	if (rid != request_id) {
		priv->failure_count++;
		asc_media_shutdown_worker (media, TRUE);
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received a worker response for the wrong request, "
				     "terminated the worker.");
		return NULL;
	}

	/* collect possible debug output */
	asc_media_drain_worker_stderr (media);

	if (status != ASC_MEDIA_STATUS_OK) {
		/* the operation failed, but the worker itself is healthy */
		if (error != NULL)
			*error = asc_media_wire_error_from_payload (payload);
		return NULL;
	}

	priv->failure_count = 0;
	return g_steal_pointer (&payload);

worker_failed:
	priv->failure_count++;
	{
		g_autofree gchar *death_desc = NULL;
		asc_media_shutdown_worker (media, eof ? FALSE : TRUE);
		death_desc = asc_media_describe_worker_death (media);
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_DEAD_WORKER,
			     "Media worker failed: %s %s",
			     tmp_error != NULL ? tmp_error->message : "",
			     death_desc);
	}
	return NULL;
}

/**
 * asc_media_fdlist_append:
 *
 * Add a file descriptor to the list, returning its handle value
 * for referencing it in message parameters.
 * The passed fd is consumed.
 */
static gint
asc_media_fdlist_append (GUnixFDList *fds, gint fd, GError **error)
{
	gint handle;

	handle = g_unix_fd_list_append (fds, fd, error);
	close (fd);
	return handle;
}

/**
 * asc_media_open_out_dir:
 *
 * Open an output directory to pass its fd to the worker.
 */
static gint
asc_media_open_out_dir (const gchar *out_dir, GError **error)
{
	gint fd;

	fd = open (out_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Unable to open media output directory '%s': %s",
			     out_dir,
			     g_strerror (errno));
		return -1;
	}
	return fd;
}

/**
 * asc_media_lookup_dup_nonempty:
 *
 * Fetch a string from a vardict, returning %NULL instead of
 * an empty string.
 */
static gchar *
asc_media_lookup_dup_nonempty (GVariant *dict, const gchar *key)
{
	const gchar *value = NULL;

	g_variant_lookup (dict, key, "&s", &value);
	if (as_is_empty (value))
		return NULL;
	return g_strdup (value);
}

/**
 * asc_media_apply_target_results:
 *
 * Transfer the per-rendition results from a response payload
 * into the caller's target structures.
 */
static gboolean
asc_media_apply_target_results (GVariant *payload, GPtrArray *targets, GError **error)
{
	g_autoptr(GVariant) results = NULL;
	GVariantIter results_iter;
	GVariant *result_dict = NULL;
	guint i = 0;

	results = g_variant_lookup_value (payload, "results", G_VARIANT_TYPE ("aa{sv}"));
	if (results == NULL || g_variant_n_children (results) != targets->len) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Worker did not return results for all requested renditions.");
		return FALSE;
	}

	g_variant_iter_init (&results_iter, results);
	while (g_variant_iter_next (&results_iter, "@a{sv}", &result_dict)) {
		g_autoptr(GVariant) result = result_dict;
		AscImageTarget *target = g_ptr_array_index (targets, i++);

		target->skipped = FALSE;
		target->result_width = 0;
		target->result_height = 0;
		g_free (g_steal_pointer (&target->error_msg));

		g_variant_lookup (result, "skipped", "b", &target->skipped);
		g_variant_lookup (result, "width", "i", &target->result_width);
		g_variant_lookup (result, "height", "i", &target->result_height);
		target->error_msg = asc_media_lookup_dup_nonempty (result, "error");
	}

	return TRUE;
}

/**
 * asc_media_process_image:
 * @media: an #AscMedia instance.
 * @img_data: The image data to process.
 * @format_hint: Assumed image format of the data, or %ASC_IMAGE_FORMAT_UNKNOWN to autodetect.
 * @load_width: Width to render vector graphics at, or 0 to load at the native size.
 * @load_height: Height to render vector graphics at, or 0 to load at the native size.
 * @load_flags: Flags to use when loading the image.
 * @out_dir: Directory to store the renditions in, may be %NULL if @targets is empty.
 * @targets: (nullable) (element-type AscImageTarget): Renditions to generate, or %NULL to just read image info.
 * @src_width: (out) (optional): Width of the (loaded) source image.
 * @src_height: (out) (optional): Height of the (loaded) source image.
 * @error: A #GError or %NULL
 *
 * Load an image (in the media worker process) and store an arbitrary set of
 * scaled renditions of it as PNG files in the given output directory.
 *
 * On success, the result fields of all @targets are updated. Individual
 * renditions may still have failed - this is recorded in their %error_msg
 * fields and does not affect the return value of this function.
 *
 * Returns: %TRUE if the image was processed.
 */
gboolean
asc_media_process_image (AscMedia *media,
			 GBytes *img_data,
			 AscImageFormat format_hint,
			 gint load_width,
			 gint load_height,
			 AscImageLoadFlags load_flags,
			 const gchar *out_dir,
			 GPtrArray *targets,
			 gint *src_width,
			 gint *src_height,
			 GError **error)
{
	g_autoptr(GUnixFDList) fds = g_unix_fd_list_new ();
	g_autoptr(GVariant) payload = NULL;
	GVariantBuilder pb;
	const gchar *format_str;
	gint fd, handle;
	gboolean have_targets = targets != NULL && targets->len > 0;

	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);
	g_return_val_if_fail (img_data != NULL, FALSE);
	g_return_val_if_fail (!have_targets || out_dir != NULL, FALSE);

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));

	fd = asc_memfd_new_sealed ("asc-image-data",
				   g_bytes_get_data (img_data, NULL),
				   g_bytes_get_size (img_data),
				   error);
	if (fd < 0)
		return FALSE;
	handle = asc_media_fdlist_append (fds, fd, error);
	if (handle < 0)
		return FALSE;
	g_variant_builder_add (&pb, "{sv}", "image-fd", g_variant_new_handle (handle));

	format_str = asc_image_format_to_string (format_hint);
	g_variant_builder_add (&pb,
			       "{sv}",
			       "format-hint",
			       g_variant_new_string (format_str ? format_str : ""));
	g_variant_builder_add (&pb, "{sv}", "load-width", g_variant_new_int32 (load_width));
	g_variant_builder_add (&pb, "{sv}", "load-height", g_variant_new_int32 (load_height));
	g_variant_builder_add (&pb, "{sv}", "load-flags", g_variant_new_uint32 (load_flags));

	if (have_targets) {
		GVariantBuilder targets_builder;

		fd = asc_media_open_out_dir (out_dir, error);
		if (fd < 0)
			return FALSE;
		handle = asc_media_fdlist_append (fds, fd, error);
		if (handle < 0)
			return FALSE;
		g_variant_builder_add (&pb, "{sv}", "dir-fd", g_variant_new_handle (handle));

		g_variant_builder_init (&targets_builder, G_VARIANT_TYPE ("aa{sv}"));
		for (guint i = 0; i < targets->len; i++) {
			AscImageTarget *target = g_ptr_array_index (targets, i);
			GVariantBuilder tb;

			g_variant_builder_init (&tb, G_VARIANT_TYPE ("a{sv}"));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "name",
					       g_variant_new_string (target->name));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "width",
					       g_variant_new_int32 (target->width));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "height",
					       g_variant_new_int32 (target->height));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "scale-mode",
					       g_variant_new_uint32 (target->scale_mode));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "save-flags",
					       g_variant_new_uint32 (target->save_flags));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "only-downscale",
					       g_variant_new_boolean (target->only_downscale));
			g_variant_builder_add (&tb,
					       "{sv}",
					       "skip-if-src-width-gt",
					       g_variant_new_int32 (target->skip_if_src_width_gt));
			g_variant_builder_add_value (&targets_builder, g_variant_builder_end (&tb));
		}
		g_variant_builder_add (&pb,
				       "{sv}",
				       "targets",
				       g_variant_builder_end (&targets_builder));
	}

	payload = asc_media_call (media,
				  ASC_MEDIA_OP_PROCESS_IMAGE,
				  g_variant_builder_end (&pb),
				  fds,
				  error);
	if (payload == NULL)
		return FALSE;

	if (src_width != NULL)
		g_variant_lookup (payload, "src-width", "i", src_width);
	if (src_height != NULL)
		g_variant_lookup (payload, "src-height", "i", src_height);

	if (have_targets)
		return asc_media_apply_target_results (payload, targets, error);
	return TRUE;
}

/**
 * asc_media_add_font_params:
 *
 * Add the common parameters of all font operations to a request.
 */
static gboolean
asc_media_add_font_params (GVariantBuilder *pb,
			   GUnixFDList *fds,
			   GBytes *font_data,
			   const gchar *basename,
			   const gchar *preferred_language,
			   const gchar *const *extra_languages,
			   const gchar *custom_sample_text,
			   const gchar *custom_icon_text,
			   GError **error)
{
	gint fd, handle;

	fd = asc_memfd_new_sealed ("asc-font-data",
				   g_bytes_get_data (font_data, NULL),
				   g_bytes_get_size (font_data),
				   error);
	if (fd < 0)
		return FALSE;
	handle = asc_media_fdlist_append (fds, fd, error);
	if (handle < 0)
		return FALSE;
	g_variant_builder_add (pb, "{sv}", "font-fd", g_variant_new_handle (handle));
	g_variant_builder_add (pb, "{sv}", "basename", g_variant_new_string (basename));

	if (!as_is_empty (preferred_language))
		g_variant_builder_add (pb,
				       "{sv}",
				       "preferred-language",
				       g_variant_new_string (preferred_language));
	if (extra_languages != NULL && extra_languages[0] != NULL)
		g_variant_builder_add (pb,
				       "{sv}",
				       "extra-languages",
				       g_variant_new_strv (extra_languages, -1));
	if (!as_is_empty (custom_sample_text))
		g_variant_builder_add (pb,
				       "{sv}",
				       "sample-text",
				       g_variant_new_string (custom_sample_text));
	if (!as_is_empty (custom_icon_text))
		g_variant_builder_add (pb,
				       "{sv}",
				       "sample-icon-text",
				       g_variant_new_string (custom_icon_text));

	return TRUE;
}

/**
 * asc_media_read_font_info:
 * @media: an #AscMedia instance.
 * @font_data: The font file contents.
 * @basename: Basename of the font file, used for heuristics.
 * @preferred_language: (nullable): Language to prefer for font samples.
 * @extra_languages: (nullable): Additional languages this font supports.
 * @custom_sample_text: (nullable): Custom sample text override.
 * @custom_icon_text: (nullable): Custom icon text override (up to 3 characters).
 * @error: A #GError or %NULL
 *
 * Read all metadata from a font file (in the media worker process),
 * including the sample texts that would be used for rendering it.
 *
 * Returns: (transfer full): An #AscFontInfo, or %NULL on error.
 */
AscFontInfo *
asc_media_read_font_info (AscMedia *media,
			  GBytes *font_data,
			  const gchar *basename,
			  const gchar *preferred_language,
			  const gchar *const *extra_languages,
			  const gchar *custom_sample_text,
			  const gchar *custom_icon_text,
			  GError **error)
{
	g_autoptr(GUnixFDList) fds = g_unix_fd_list_new ();
	g_autoptr(GVariant) payload = NULL;
	g_autoptr(AscFontInfo) info = NULL;
	GVariantBuilder pb;

	g_return_val_if_fail (ASC_IS_MEDIA (media), NULL);
	g_return_val_if_fail (font_data != NULL, NULL);
	g_return_val_if_fail (basename != NULL, NULL);

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	if (!asc_media_add_font_params (&pb,
					fds,
					font_data,
					basename,
					preferred_language,
					extra_languages,
					custom_sample_text,
					custom_icon_text,
					error)) {
		g_variant_builder_clear (&pb);
		return NULL;
	}

	payload = asc_media_call (media,
				  ASC_MEDIA_OP_FONT_INFO,
				  g_variant_builder_end (&pb),
				  fds,
				  error);
	if (payload == NULL)
		return NULL;

	info = g_new0 (AscFontInfo, 1);
	info->family = asc_media_lookup_dup_nonempty (payload, "family");
	info->style = asc_media_lookup_dup_nonempty (payload, "style");
	info->fullname = asc_media_lookup_dup_nonempty (payload, "fullname");
	info->id = asc_media_lookup_dup_nonempty (payload, "id");
	info->description = asc_media_lookup_dup_nonempty (payload, "description");
	info->homepage = asc_media_lookup_dup_nonempty (payload, "homepage");
	info->preferred_language = asc_media_lookup_dup_nonempty (payload, "preferred-language");
	info->sample_text = asc_media_lookup_dup_nonempty (payload, "sample-text");
	info->sample_icon_text = asc_media_lookup_dup_nonempty (payload, "sample-icon-text");
	g_variant_lookup (payload, "languages", "^as", &info->languages);

	return g_steal_pointer (&info);
}

/**
 * asc_media_render_font:
 *
 * Shared implementation for font card & font icon rendering.
 */
static gboolean
asc_media_render_font (AscMedia *media,
		       AscMediaOp op,
		       GBytes *font_data,
		       const gchar *basename,
		       const gchar *preferred_language,
		       const gchar *const *extra_languages,
		       const gchar *custom_sample_text,
		       const gchar *custom_icon_text,
		       const gchar *info_label,
		       const gchar *out_dir,
		       GPtrArray *targets,
		       GError **error)
{
	g_autoptr(GUnixFDList) fds = g_unix_fd_list_new ();
	g_autoptr(GVariant) payload = NULL;
	GVariantBuilder pb;
	GVariantBuilder entries_builder;
	gint fd, handle;

	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);
	g_return_val_if_fail (font_data != NULL, FALSE);
	g_return_val_if_fail (basename != NULL, FALSE);
	g_return_val_if_fail (out_dir != NULL, FALSE);
	g_return_val_if_fail (targets != NULL && targets->len > 0, FALSE);

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	if (!asc_media_add_font_params (&pb,
					fds,
					font_data,
					basename,
					preferred_language,
					extra_languages,
					custom_sample_text,
					custom_icon_text,
					error)) {
		g_variant_builder_clear (&pb);
		return FALSE;
	}

	fd = asc_media_open_out_dir (out_dir, error);
	if (fd < 0) {
		g_variant_builder_clear (&pb);
		return FALSE;
	}
	handle = asc_media_fdlist_append (fds, fd, error);
	if (handle < 0) {
		g_variant_builder_clear (&pb);
		return FALSE;
	}
	g_variant_builder_add (&pb, "{sv}", "dir-fd", g_variant_new_handle (handle));

	if (!as_is_empty (info_label))
		g_variant_builder_add (&pb,
				       "{sv}",
				       "info-label",
				       g_variant_new_string (info_label));

	g_variant_builder_init (&entries_builder, G_VARIANT_TYPE ("aa{sv}"));
	for (guint i = 0; i < targets->len; i++) {
		AscImageTarget *target = g_ptr_array_index (targets, i);
		GVariantBuilder eb;

		g_variant_builder_init (&eb, G_VARIANT_TYPE ("a{sv}"));
		g_variant_builder_add (&eb, "{sv}", "name", g_variant_new_string (target->name));
		if (op == ASC_MEDIA_OP_RENDER_FONT_ICON) {
			g_variant_builder_add (&eb,
					       "{sv}",
					       "size",
					       g_variant_new_uint32 ((guint32) target->width));
		} else {
			g_variant_builder_add (&eb,
					       "{sv}",
					       "width",
					       g_variant_new_int32 (target->width));
			g_variant_builder_add (&eb,
					       "{sv}",
					       "height",
					       g_variant_new_int32 (target->height));
		}
		g_variant_builder_add_value (&entries_builder, g_variant_builder_end (&eb));
	}
	g_variant_builder_add (&pb, "{sv}", "entries", g_variant_builder_end (&entries_builder));

	payload = asc_media_call (media, op, g_variant_builder_end (&pb), fds, error);
	if (payload == NULL)
		return FALSE;

	return asc_media_apply_target_results (payload, targets, error);
}

/**
 * asc_media_render_font_card:
 * @media: an #AscMedia instance.
 * @font_data: The font file contents.
 * @basename: Basename of the font file, used for heuristics.
 * @preferred_language: (nullable): Language to prefer for font samples.
 * @extra_languages: (nullable): Additional languages this font supports.
 * @custom_sample_text: (nullable): Custom sample text override.
 * @custom_icon_text: (nullable): Custom icon text override.
 * @info_label: (nullable): Short info label for the card, or %NULL for the default.
 * @out_dir: Directory to store the rendered cards in.
 * @targets: (element-type AscImageTarget): Card sizes to render.
 * @error: A #GError or %NULL
 *
 * Render font specimen cards showcasing the given font (in the media worker
 * process) and store them as PNG images in the output directory.
 *
 * The card render may adjust the image height, so the target result
 * dimensions must be used by the caller. Failures of individual renditions
 * are recorded in the targets' %error_msg fields.
 *
 * Returns: %TRUE if the font was processed.
 */
gboolean
asc_media_render_font_card (AscMedia *media,
			    GBytes *font_data,
			    const gchar *basename,
			    const gchar *preferred_language,
			    const gchar *const *extra_languages,
			    const gchar *custom_sample_text,
			    const gchar *custom_icon_text,
			    const gchar *info_label,
			    const gchar *out_dir,
			    GPtrArray *targets,
			    GError **error)
{
	return asc_media_render_font (media,
				      ASC_MEDIA_OP_RENDER_FONT_CARD,
				      font_data,
				      basename,
				      preferred_language,
				      extra_languages,
				      custom_sample_text,
				      custom_icon_text,
				      info_label,
				      out_dir,
				      targets,
				      error);
}

/**
 * asc_media_render_font_icon:
 * @media: an #AscMedia instance.
 * @font_data: The font file contents.
 * @basename: Basename of the font file, used for heuristics.
 * @preferred_language: (nullable): Language to prefer for font samples.
 * @extra_languages: (nullable): Additional languages this font supports.
 * @custom_sample_text: (nullable): Custom sample text override.
 * @custom_icon_text: (nullable): Custom icon text override.
 * @out_dir: Directory to store the rendered icons in.
 * @targets: (element-type AscImageTarget): Icon sizes to render (the target width is used as canvas size).
 * @error: A #GError or %NULL
 *
 * Render icons for the given font (in the media worker process), each
 * consisting of a background shape with the font's sample icon text on
 * top, and store them as PNG images in the output directory.
 *
 * Failures of individual renditions are recorded in the targets'
 * %error_msg fields.
 *
 * Returns: %TRUE if the font was processed.
 */
gboolean
asc_media_render_font_icon (AscMedia *media,
			    GBytes *font_data,
			    const gchar *basename,
			    const gchar *preferred_language,
			    const gchar *const *extra_languages,
			    const gchar *custom_sample_text,
			    const gchar *custom_icon_text,
			    const gchar *out_dir,
			    GPtrArray *targets,
			    GError **error)
{
	return asc_media_render_font (media,
				      ASC_MEDIA_OP_RENDER_FONT_ICON,
				      font_data,
				      basename,
				      preferred_language,
				      extra_languages,
				      custom_sample_text,
				      custom_icon_text,
				      NULL, /* info label */
				      out_dir,
				      targets,
				      error);
}

/**
 * asc_media_probe_video:
 * @media: an #AscMedia instance.
 * @video_data: The video file contents.
 * @basename: Basename of the video file.
 * @codec_name: (out) (optional): Name of the video codec.
 * @audio_codec_name: (out) (optional): Name of the audio codec, or %NULL if the video has no audio.
 * @format_name: (out) (optional): Name of the container format.
 * @width: (out) (optional): Video width.
 * @height: (out) (optional): Video height.
 * @error: A #GError or %NULL
 *
 * Probe a video file for its container format, codecs and dimensions
 * (using ffprobe, run by the media worker process).
 *
 * Returns: %TRUE if the video was probed successfully.
 */
gboolean
asc_media_probe_video (AscMedia *media,
		       GBytes *video_data,
		       const gchar *basename,
		       gchar **codec_name,
		       gchar **audio_codec_name,
		       gchar **format_name,
		       gint *width,
		       gint *height,
		       GError **error)
{
	g_autoptr(GUnixFDList) fds = g_unix_fd_list_new ();
	g_autoptr(GVariant) payload = NULL;
	GVariantBuilder pb;
	gint fd, handle;

	g_return_val_if_fail (ASC_IS_MEDIA (media), FALSE);
	g_return_val_if_fail (video_data != NULL, FALSE);

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));

	fd = asc_memfd_new_sealed ("asc-video-data",
				   g_bytes_get_data (video_data, NULL),
				   g_bytes_get_size (video_data),
				   error);
	if (fd < 0)
		return FALSE;
	handle = asc_media_fdlist_append (fds, fd, error);
	if (handle < 0)
		return FALSE;
	g_variant_builder_add (&pb, "{sv}", "video-fd", g_variant_new_handle (handle));
	g_variant_builder_add (&pb,
			       "{sv}",
			       "basename",
			       g_variant_new_string (basename ? basename : "video"));

	payload = asc_media_call (media,
				  ASC_MEDIA_OP_PROBE_VIDEO,
				  g_variant_builder_end (&pb),
				  fds,
				  error);
	if (payload == NULL)
		return FALSE;

	if (codec_name != NULL)
		*codec_name = asc_media_lookup_dup_nonempty (payload, "codec-name");
	if (audio_codec_name != NULL)
		*audio_codec_name = asc_media_lookup_dup_nonempty (payload, "audio-codec-name");
	if (format_name != NULL)
		*format_name = asc_media_lookup_dup_nonempty (payload, "format-name");
	if (width != NULL)
		g_variant_lookup (payload, "width", "i", width);
	if (height != NULL)
		g_variant_lookup (payload, "height", "i", height);

	return TRUE;
}
