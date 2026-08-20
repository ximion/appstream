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
 * SECTION:asc-media-ipc
 * @short_description: Protocol between libappstream-compose and the media worker.
 *
 * Messages are GVariants sent as single datagrams over a SOCK_SEQPACKET Unix
 * socket pair. Bulk data (images, fonts, videos) is passed exclusively via
 * sealed memfds attached with SCM_RIGHTS; rendered results are written by the
 * worker through a directory file descriptor provided by the client.
 */

#define _GNU_SOURCE
#include "config.h"
#include "asc-media-ipc.h"
#include "asc-media.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <gio/gunixfdmessage.h>

/* all seals we apply to data passed between the processes */
#define ASC_MEMFD_DATA_SEALS (F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL)

/**
 * asc_memfd_new_sealed:
 * @name: Debug name for the memfd.
 * @data: Data to write into the new memfd.
 * @len: Length of @data.
 * @error: A #GError or %NULL
 *
 * Create a new memfd containing a copy of the given data, sealed so
 * neither size nor contents can be changed anymore.
 *
 * Returns: A new file descriptor, or -1 on error.
 */
gint
asc_memfd_new_sealed (const gchar *name, gconstpointer data, gsize len, GError **error)
{
	gint fd;
	gsize written = 0;

	fd = memfd_create (name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd < 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Unable to create memfd: %s",
			     g_strerror (errno));
		return -1;
	}

	while (written < len) {
		gssize ret = write (fd, ((const guint8 *) data) + written, len - written);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			g_set_error (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_FAILED,
				     "Unable to write to memfd: %s",
				     g_strerror (errno));
			close (fd);
			return -1;
		}
		written += ret;
	}

	if (fcntl (fd, F_ADD_SEALS, ASC_MEMFD_DATA_SEALS) != 0) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Unable to seal memfd: %s",
			     g_strerror (errno));
		close (fd);
		return -1;
	}

	return fd;
}

/**
 * asc_media_wire_send_message:
 *
 * Send a single serialized GVariant envelope as one datagram,
 * with an optional list of file descriptors attached.
 */
gboolean
asc_media_ipc_send_message (GSocket *socket,
			    GVariant *message,
			    GUnixFDList *fds,
			    GCancellable *cancellable,
			    GError **error)
{
	g_autoptr(GVariant) msg = g_variant_ref_sink (message);
	gconstpointer data;
	gsize data_len;
	GOutputVector vector;
	GSocketControlMessage *cmsg = NULL;
	GSocketControlMessage *cmsgs[1];
	gint n_cmsgs = 0;
	gssize ret;
	gboolean result = FALSE;

	data = g_variant_get_data (msg);
	data_len = g_variant_get_size (msg);

	if (data_len > ASC_MEDIA_MAX_MSG_SIZE) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_PROTOCOL,
			     "Cannot send oversized protocol message (%" G_GSIZE_FORMAT " bytes).",
			     data_len);
		return FALSE;
	}

	vector.buffer = data;
	vector.size = data_len;

	if (fds != NULL && g_unix_fd_list_get_length (fds) > 0) {
		cmsg = g_unix_fd_message_new_with_fd_list (fds);
		cmsgs[0] = cmsg;
		n_cmsgs = 1;
	}

	/* NOTE: GSocket already retries on EINTR internally */
	ret = g_socket_send_message (socket,
				     NULL, /* address */
				     &vector,
				     1,
				     n_cmsgs > 0 ? cmsgs : NULL,
				     n_cmsgs,
				     G_SOCKET_MSG_NONE,
				     cancellable,
				     error);
	if (ret < 0)
		goto out;

	if ((gsize) ret != data_len) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Only sent partial message.");
		goto out;
	}
	result = TRUE;

out:
	if (cmsg != NULL)
		g_object_unref (cmsg);
	return result;
}

/**
 * asc_media_wire_receive_message:
 *
 * Receive a single datagram and deserialize it into a GVariant envelope
 * of the given type. Any attached file descriptors are returned as well.
 * Sets @eof to %TRUE (without error) if the connection was closed.
 */
GVariant *
asc_media_ipc_receive_message (GSocket *socket,
			       const GVariantType *message_type,
			       GUnixFDList **fds,
			       gboolean *eof,
			       GCancellable *cancellable,
			       GError **error)
{
	g_autofree guint8 *buffer = NULL;
	g_autoptr(GVariant) message = NULL;
	GInputVector vector;
	GSocketControlMessage **cmsgs = NULL;
	gint n_cmsgs = 0;
	gint msg_flags = 0;
	gssize ret;
	GVariant *result = NULL;

	if (fds != NULL)
		*fds = NULL;
	*eof = FALSE;

	buffer = g_malloc (ASC_MEDIA_MAX_MSG_SIZE);
	vector.buffer = buffer;
	vector.size = ASC_MEDIA_MAX_MSG_SIZE;

	/* NOTE: GSocket already retries on EINTR internally */
	ret = g_socket_receive_message (socket,
					NULL, /* address */
					&vector,
					1,
					&cmsgs,
					&n_cmsgs,
					&msg_flags,
					cancellable,
					error);
	if (ret < 0)
		return NULL;

	if (ret == 0) {
		/* connection was closed by the other side */
		*eof = TRUE;
		return NULL;
	}

	/* extract any attached file descriptors */
	for (gint i = 0; i < n_cmsgs; i++) {
		if (G_IS_UNIX_FD_MESSAGE (cmsgs[i]) && fds != NULL && *fds == NULL) {
			*fds = g_object_ref (
			    g_unix_fd_message_get_fd_list (G_UNIX_FD_MESSAGE (cmsgs[i])));
		}
		g_object_unref (cmsgs[i]);
	}
	g_free (cmsgs);

	if (msg_flags & MSG_TRUNC) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received a truncated protocol message.");
		goto out;
	}

	{
		/* the variant takes ownership of the buffer */
		guint8 *msg_data = g_steal_pointer (&buffer);
		message = g_variant_new_from_data (message_type,
						   msg_data,
						   ret,
						   FALSE, /* untrusted */
						   g_free,
						   msg_data);
	}
	g_variant_ref_sink (message);

	if (!g_variant_is_normal_form (message)) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received a malformed protocol message.");
		goto out;
	}

	result = g_steal_pointer (&message);

out:
	if (result == NULL && fds != NULL)
		g_clear_object (fds);
	return result;
}

/**
 * asc_media_wire_send_request:
 *
 * Send a request message to the media worker.
 */
gboolean
asc_media_ipc_send_request (GSocket *socket,
			    guint32 request_id,
			    AscMediaOp op,
			    GVariant *params,
			    GUnixFDList *fds,
			    GCancellable *cancellable,
			    GError **error)
{
	GVariant *message;
	g_autoptr(GVariant) params_ref = NULL;

	if (params == NULL)
		params = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);
	params_ref = g_variant_ref_sink (params);

	message = g_variant_new ("(uu@a{sv})", request_id, (guint32) op, params_ref);
	return asc_media_ipc_send_message (socket, message, fds, cancellable, error);
}

/**
 * asc_media_wire_receive_response:
 *
 * Receive a response message from the media worker.
 * Responses never carry file descriptors.
 */
gboolean
asc_media_ipc_receive_response (GSocket *socket,
				guint32 *request_id,
				guint32 *status,
				GVariant **payload,
				gboolean *eof,
				GCancellable *cancellable,
				GError **error)
{
	g_autoptr(GVariant) message = NULL;
	g_autoptr(GUnixFDList) fds = NULL;

	message = asc_media_ipc_receive_message (socket,
						 G_VARIANT_TYPE (ASC_MEDIA_RESPONSE_VTYPE),
						 &fds,
						 eof,
						 cancellable,
						 error);
	if (message == NULL)
		return FALSE;

	/* we never expect fds in responses - if any arrive, they are closed with the list */
	if (fds != NULL && g_unix_fd_list_get_length (fds) > 0) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_PROTOCOL,
				     "Received unexpected file descriptors in worker response.");
		return FALSE;
	}

	g_variant_get (message, "(uu@a{sv})", request_id, status, payload);
	return TRUE;
}

/**
 * asc_media_wire_error_from_payload:
 *
 * Re-materialize a #GError transported in an error response payload.
 *
 * Returns: (transfer full): A new #GError.
 */
GError *
asc_media_ipc_error_from_payload (GVariant *payload)
{
	const gchar *domain_str = NULL;
	const gchar *message = NULL;
	gint32 code = 0;

	g_variant_lookup (payload, ASC_MEDIA_IPC_KEY_ERROR_DOMAIN, "&s", &domain_str);
	g_variant_lookup (payload, ASC_MEDIA_IPC_KEY_ERROR_CODE, "i", &code);
	g_variant_lookup (payload, ASC_MEDIA_IPC_KEY_ERROR_MESSAGE, "&s", &message);

	if (domain_str == NULL || message == NULL)
		return g_error_new_literal (ASC_MEDIA_ERROR,
					    ASC_MEDIA_ERROR_PROTOCOL,
					    "Worker sent an invalid error response.");

	return g_error_new_literal (g_quark_from_string (domain_str), code, message);
}
