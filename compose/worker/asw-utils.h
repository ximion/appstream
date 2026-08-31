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

#include <glib.h>

G_BEGIN_DECLS

/* how much of a file we need to determine what it is */
#define ASW_MEDIA_HEAD_LEN 1024

gsize	 asw_read_fd_head (gint	   fd,
			   guchar *buf,
			   gsize   buf_len);

gboolean asw_data_is_matroska (const guchar *data,
			       gsize	     len);

gboolean asw_describe_data (const guchar *data,
			    gsize	  len,
			    gchar	**content_type);

gchar	*asw_describe_wrong_media (const gchar *content_type,
				   const gchar *expected_prefix);

G_END_DECLS
