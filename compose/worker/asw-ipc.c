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
 * SECTION:asw-ipc
 * @short_description: Worker-side helpers for the media worker wire protocol.
 *
 * The low-level message framing is shared with libappstream-compose and
 * lives in asc-media-ipc.c, together with the client-side counterparts
 * of these functions.
 */

#define _GNU_SOURCE
#include "config.h"
#include "asw-ipc.h"
#include "asc-media.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

/**
 * asw_memfd_verify_sealed:
 * @fd: The file descriptor to verify.
 * @size_out: (out) (optional): Size of the data in bytes.
 * @error: A #GError or %NULL
 *
 * Verify that the given file descriptor is a fully sealed memfd,
 * so its contents can safely be mapped.
 *
 * Returns: %TRUE if the fd is safe to use.
 */
gboolean
asw_memfd_verify_sealed (gint fd, gsize *size_out, GError **error)
{
	struct stat st;
	gint seals;

	if (fstat (fd, &st) != 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_PROTOCOL,
			     "Unable to stat received fd: %s",
			     g_strerror (errno));
		return FALSE;
	}
	if (!S_ISREG (st.st_mode)) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received data fd is not a regular file.");
		return FALSE;
	}

	seals = fcntl (fd, F_GET_SEALS);
	if (seals < 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_PROTOCOL,
			     "Unable to read seals of received fd: %s",
			     g_strerror (errno));
		return FALSE;
	}
	if ((seals & (F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE)) !=
	    (F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE)) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received data fd is not properly sealed.");
		return FALSE;
	}

	if (size_out != NULL)
		*size_out = st.st_size;
	return TRUE;
}

static void
asw_memfd_unmap_cb (gpointer data)
{
	gpointer *closure = data;
	munmap (closure[0], GPOINTER_TO_SIZE (closure[1]));
	g_free (closure);
}

/**
 * asw_memfd_map_bytes:
 * @fd: A sealed memfd to map.
 * @error: A #GError or %NULL
 *
 * Verify the seals on the given memfd and map its contents read-only.
 *
 * Returns: (transfer full): The mapped data, or %NULL on error.
 */
GBytes *
asw_memfd_map_bytes (gint fd, GError **error)
{
	gsize size = 0;
	gpointer map;
	gpointer *closure;

	if (!asw_memfd_verify_sealed (fd, &size, error))
		return NULL;
	if (size == 0)
		return g_bytes_new_static ("", 0);

	map = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Unable to map received data: %s",
			     g_strerror (errno));
		return NULL;
	}

	closure = g_new0 (gpointer, 2);
	closure[0] = map;
	closure[1] = GSIZE_TO_POINTER (size);
	return g_bytes_new_with_free_func (map, size, asw_memfd_unmap_cb, closure);
}

/**
 * asw_wire_receive_request:
 *
 * Receive a request message in the media worker.
 */
gboolean
asw_ipc_receive_request (GSocket *socket,
			 guint32 *request_id,
			 AscMediaOp *op,
			 GVariant **params,
			 GUnixFDList **fds,
			 gboolean *eof,
			 GError **error)
{
	g_autoptr(GVariant) message = NULL;
	guint32 op_u;

	message = asc_media_ipc_receive_message (socket,
						 G_VARIANT_TYPE (ASC_MEDIA_REQUEST_VTYPE),
						 fds,
						 eof,
						 NULL, /* cancellable */
						 error);
	if (message == NULL)
		return FALSE;

	g_variant_get (message, "(uu@a{sv})", request_id, &op_u, params);
	*op = (op_u < ASC_MEDIA_OP_LAST) ? (AscMediaOp) op_u : ASC_MEDIA_OP_UNKNOWN;
	return TRUE;
}

/**
 * asw_wire_send_response:
 *
 * Send a response message from the media worker.
 */
gboolean
asw_ipc_send_response (GSocket *socket,
		       guint32 request_id,
		       guint32 status,
		       GVariant *payload,
		       GError **error)
{
	GVariant *message;
	g_autoptr(GVariant) payload_ref = NULL;

	if (payload == NULL)
		payload = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);
	payload_ref = g_variant_ref_sink (payload);

	message = g_variant_new ("(uu@a{sv})", request_id, status, payload_ref);
	return asc_media_ipc_send_message (socket, message, NULL, NULL, error);
}

/**
 * asw_wire_send_error_response:
 *
 * Send an error response for a failed operation, transporting
 * the operation's #GError to the client.
 */
gboolean
asw_ipc_send_error_response (GSocket *socket,
			     guint32 request_id,
			     const GError *op_error,
			     GError **error)
{
	GVariantBuilder pb;

	g_variant_builder_init (&pb, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&pb,
			       "{sv}",
			       ASC_MEDIA_IPC_KEY_ERROR_DOMAIN,
			       g_variant_new_string (g_quark_to_string (op_error->domain)));
	g_variant_builder_add (&pb,
			       "{sv}",
			       ASC_MEDIA_IPC_KEY_ERROR_CODE,
			       g_variant_new_int32 (op_error->code));
	g_variant_builder_add (&pb,
			       "{sv}",
			       ASC_MEDIA_IPC_KEY_ERROR_MESSAGE,
			       g_variant_new_string (op_error->message));

	return asw_ipc_send_response (socket,
				      request_id,
				      ASC_MEDIA_STATUS_ERROR,
				      g_variant_builder_end (&pb),
				      error);
}
