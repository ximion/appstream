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

#include "asc-media-ipc.h"

G_BEGIN_DECLS

gboolean asw_memfd_verify_sealed (gint	   fd,
				  gsize	  *size_out,
				  GError **error);
GBytes	*asw_memfd_map_bytes (gint     fd,
			      GError **error);

gboolean asw_ipc_receive_request (GSocket      *socket,
				  guint32      *request_id,
				  AscMediaOp   *op,
				  GVariant    **params,
				  GUnixFDList **fds,
				  gboolean     *eof,
				  GError      **error);

gboolean asw_ipc_send_response (GSocket	 *socket,
				guint32	  request_id,
				guint32	  status,
				GVariant *payload,
				GError	**error);
gboolean asw_ipc_send_error_response (GSocket	   *socket,
				      guint32	    request_id,
				      const GError *op_error,
				      GError	  **error);

G_END_DECLS
