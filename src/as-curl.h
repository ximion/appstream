/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (AS_COMPILATION)
#error "Can not use internal AppStream API from external project."
#endif

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_CURL (as_curl_get_type ())
G_DECLARE_FINAL_TYPE (AsCurl, as_curl, AS, CURL, GObject)

/**
 * AsCurlError:
 * @AS_CURL_ERROR_FAILED:	Generic failure.
 * @AS_CURL_ERROR_REMOTE:	Some issue happened on the remote side.
 * @AS_CURL_ERROR_DOWNLOAD:	Download failed.
 * @AS_CURL_ERROR_SIZE:		Some filesize value was unexpected.
 *
 * An cURL error.
 **/
typedef enum {
	AS_CURL_ERROR_FAILED,
	AS_CURL_ERROR_REMOTE,
	AS_CURL_ERROR_DOWNLOAD,
	AS_CURL_ERROR_SIZE,
	/*< private >*/
	AS_CURL_ERROR_LAST
} AsCurlError;

#define	AS_CURL_ERROR	as_curl_error_quark ()
GQuark			as_curl_error_quark (void);

AsCurl			*as_curl_new (GError **error);

void			as_curl_set_cainfo (AsCurl *acurl,
						const gchar *cainfo);

GBytes			*as_curl_download_bytes (AsCurl *acurl,
						 const gchar *url,
						 GError **error);
gboolean		as_curl_download_to_filename (AsCurl *acurl,
							const gchar *url,
							const gchar *fname,
							GError **error);

gboolean		as_curl_check_url_exists (AsCurl *acurl,
						  const gchar *url,
						  GError **error);

gboolean		as_curl_is_url (const gchar *url);

G_END_DECLS
