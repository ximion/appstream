/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020-2021 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-curl
 * @short_description: Internal convenience wrapper around some cURL functions for AppStream
 * @include: appstream.h
 */

#include "config.h"
#include "as-curl.h"

#include <curl/curl.h>

struct _AsCurl
{
	GObject parent_instance;
};

typedef struct
{
	CURL		*curl;
	const gchar	*user_agent;
} AsCurlPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsCurl, as_curl, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_curl_get_instance_private (o))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CURLU, curl_url_cleanup)

G_DEFINE_QUARK (AsCurlError, as_curl_error)

/**
 * as_curl_is_url:
 *
 * Check if the URL is valid.
 */
gboolean
as_curl_is_url (const gchar *url)
{
	g_autoptr(CURLU) cu = curl_url ();
	return curl_url_set (cu, CURLUPART_URL, url, 0) == CURLUE_OK;
}

static void
as_curl_finalize (GObject *object)
{
	AsCurl *acurl = AS_CURL (object);
	AsCurlPrivate *priv = GET_PRIVATE (acurl);

	if (priv->curl != NULL)
		curl_easy_cleanup (priv->curl);

	G_OBJECT_CLASS (as_curl_parent_class)->finalize (object);
}

static void
as_curl_init (AsCurl *acurl)
{
	AsCurlPrivate *priv = GET_PRIVATE (acurl);

	priv->user_agent = "appstream/" PACKAGE_VERSION;
}

static void
as_curl_class_init (AsCurlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_curl_finalize;
}

static size_t
as_curl_download_write_bytearray_cb (char *ptr, size_t size, size_t nmemb, void *udata)
{
	GByteArray *buf = (GByteArray *) udata;
	gsize realsize = size * nmemb;
	g_byte_array_append (buf, (const guint8 *) ptr, realsize);
	return realsize;
}

/**
 * as_curl_download_bytes:
 *
 * Download an URL as GBytes.
 */
GBytes*
as_curl_download_bytes (AsCurl *acurl, const gchar *url, GError **error)
{
	AsCurlPrivate *priv = GET_PRIVATE (acurl);
	CURLcode res;
	gchar errbuf[CURL_ERROR_SIZE] = { '\0' };
	glong status_code = 0;
	g_autoptr(GByteArray) buf = g_byte_array_new ();

	curl_easy_setopt (priv->curl, CURLOPT_URL, url);
	curl_easy_setopt (priv->curl, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt (priv->curl, CURLOPT_WRITEFUNCTION, as_curl_download_write_bytearray_cb);
	curl_easy_setopt (priv->curl, CURLOPT_WRITEDATA, buf);

	res = curl_easy_perform (priv->curl);
	curl_easy_getinfo (priv->curl, CURLINFO_RESPONSE_CODE, &status_code);
	if (res != CURLE_OK) {
		g_debug ("cURL status-code was %ld", status_code);
		if (status_code == 429) {
			g_set_error (error,
				     AS_CURL_ERROR,
				     AS_CURL_ERROR_REMOTE,
				     "Failed to download due to server limit");
			return NULL;
		}
		if (errbuf[0] != '\0') {
			g_set_error (error,
				     AS_CURL_ERROR,
				     AS_CURL_ERROR_DOWNLOAD,
				     "Failed to download file: %s",
				     errbuf);
			return NULL;
		}
		g_set_error (error,
			     AS_CURL_ERROR,
			     AS_CURL_ERROR_DOWNLOAD,
			     "Failed to download file: %s",
			     curl_easy_strerror (res));
		return NULL;
	}
	if (status_code == 404) {
		g_set_error (error,
			     AS_CURL_ERROR,
			     AS_CURL_ERROR_REMOTE,
			     "URL was not found on server.");
		return NULL;
	} else if (status_code != 200) {
		g_set_error (error,
			     AS_CURL_ERROR,
			     AS_CURL_ERROR_REMOTE,
			     "Unexpected status code: %ld",
			     status_code);
		return NULL;
	}

	return g_byte_array_free_to_bytes (g_steal_pointer (&buf));
}

/**
 * as_curl_new:
 *
 * Creates a new #AsCurl.
 **/
AsCurl*
as_curl_new (GError **error)
{
	AsCurlPrivate *priv;
	const gchar *http_proxy;
	g_autoptr(AsCurl) acurl = g_object_new (AS_TYPE_CURL, NULL);
	priv = GET_PRIVATE (acurl);

	priv->curl = curl_easy_init ();
	if (priv->curl == NULL) {
		g_set_error_literal (error,
				     AS_CURL_ERROR,
				     AS_CURL_ERROR_FAILED,
				     "Failed to setup networking, could not initialize cURL.");
		return NULL;
	}

	if (g_getenv ("AS_CURL_VERBOSE") != NULL)
		curl_easy_setopt (priv->curl, CURLOPT_VERBOSE, 1L);

	curl_easy_setopt (priv->curl, CURLOPT_USERAGENT, priv->user_agent);
	curl_easy_setopt (priv->curl, CURLOPT_CONNECTTIMEOUT, 60L);
	curl_easy_setopt (priv->curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt (priv->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt (priv->curl, CURLOPT_MAXREDIRS, 8L);

	/* read common proxy environment variables */
	http_proxy = g_getenv ("https_proxy");
	if (http_proxy == NULL)
		http_proxy = g_getenv ("HTTPS_PROXY");
	if (http_proxy == NULL)
		http_proxy = g_getenv ("http_proxy");
	if (http_proxy == NULL)
		http_proxy = g_getenv ("HTTP_PROXY");
	if (http_proxy != NULL && strlen (http_proxy) > 0)
		curl_easy_setopt (priv->curl, CURLOPT_PROXY, http_proxy);

	return AS_CURL (g_steal_pointer (&acurl));
}
