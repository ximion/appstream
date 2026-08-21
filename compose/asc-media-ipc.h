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

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "as-macros-private.h"

AS_BEGIN_PRIVATE_DECLS

/* protocol revision */
#define ASC_MEDIA_PROTOCOL_VERSION 1

/* file descriptor number the worker inherits its communication socket on */
#define ASC_MEDIA_SOCKET_FD 3

/* maximum size of an in-band protocol message - bulk data is passed via memfd */
#define ASC_MEDIA_MAX_MSG_SIZE (128 * 1024)

/* GVariant type strings for the message envelopes */
#define ASC_MEDIA_REQUEST_VTYPE	 "(uua{sv})"
#define ASC_MEDIA_RESPONSE_VTYPE "(uua{sv})"

/* response status codes */
#define ASC_MEDIA_STATUS_OK    0
#define ASC_MEDIA_STATUS_ERROR 1

/* payload keys for error responses */
#define ASC_MEDIA_IPC_KEY_ERROR_DOMAIN	"error-domain"
#define ASC_MEDIA_IPC_KEY_ERROR_CODE	"error-code"
#define ASC_MEDIA_IPC_KEY_ERROR_MESSAGE "error-message"

/**
 * AscMediaOp:
 *
 * Operations the media worker can perform.
 **/
typedef enum {
	ASC_MEDIA_OP_UNKNOWN,
	ASC_MEDIA_OP_SETUP,
	ASC_MEDIA_OP_PROCESS_IMAGE,
	ASC_MEDIA_OP_FONT_INFO,
	ASC_MEDIA_OP_RENDER_FONT_CARD,
	ASC_MEDIA_OP_RENDER_FONT_ICON,
	ASC_MEDIA_OP_PROBE_VIDEO,
	ASC_MEDIA_OP_SHUTDOWN,
	/*< private >*/
	ASC_MEDIA_OP_LAST
} AscMediaOp;

/* low-level message framing, shared between client and worker */
AS_INTERNAL_VISIBLE
gboolean asc_media_ipc_send_message (GSocket	  *socket,
				     GVariant	  *message,
				     GUnixFDList  *fds,
				     GCancellable *cancellable,
				     GError	 **error);
AS_INTERNAL_VISIBLE
GVariant *asc_media_ipc_receive_message (GSocket	    *socket,
					 const GVariantType *message_type,
					 GUnixFDList	   **fds,
					 gboolean	    *eof,
					 GCancellable	    *cancellable,
					 GError		   **error);

/* client-side API, used by AscMedia only */
gint	  asc_memfd_new_sealed (const gchar  *name,
				gconstpointer data,
				gsize	      len,
				GError	    **error);

gboolean  asc_media_ipc_send_request (GSocket	   *socket,
				      guint32	    request_id,
				      AscMediaOp    op,
				      GVariant	   *params,
				      GUnixFDList  *fds,
				      GCancellable *cancellable,
				      GError	  **error);
gboolean  asc_media_ipc_receive_response (GSocket      *socket,
					  guint32      *request_id,
					  guint32      *status,
					  GVariant    **payload,
					  gboolean     *eof,
					  GCancellable *cancellable,
					  GError      **error);

GError	 *asc_media_ipc_error_from_payload (GVariant *payload);

AS_END_PRIVATE_DECLS
