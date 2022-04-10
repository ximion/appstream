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

/**
 * SECTION:as-curl
 * @short_description: Internal convenience wrapper around some cURL functions for AppStream
 * @include: appstream.h
 */

#include "config.h"
#include "as-curl.h"

#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <curl/curl.h>

struct _AsCurl
{
	GObject parent_instance;
};

typedef struct
{
	CURL		*curl;
	const gchar	*user_agent;

	curl_off_t	bytes_downloaded;
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

static int
as_curl_progress_dummy_cb (void *clientp,
			   curl_off_t dltotal,
			   curl_off_t dlnow,
			   curl_off_t ultotal,
			   curl_off_t ulnow)
{
	return 0;
}

static size_t
as_curl_download_write_bytearray_cb (char *ptr, size_t size, size_t nmemb, void *udata)
{
	GByteArray *buf = (GByteArray *) udata;
	gsize realsize = size * nmemb;
	g_byte_array_append (buf, (const guint8 *) ptr, realsize);
	return realsize;
}

static gboolean
as_curl_perform_download (AsCurl *acurl, gboolean abort_is_error, GError **error)
{
	AsCurlPrivate *priv = GET_PRIVATE (acurl);
	CURLcode res;
	gchar errbuf[CURL_ERROR_SIZE] = { '\0' };
	glong status_code = 0;

	curl_easy_setopt (priv->curl, CURLOPT_ERRORBUFFER, errbuf);

	res = curl_easy_perform (priv->curl);
	curl_easy_getinfo (priv->curl, CURLINFO_RESPONSE_CODE, &status_code);
	if (res != CURLE_OK) {
		/* check if this issue was an intentional abort */
		if (!abort_is_error && res == CURLE_ABORTED_BY_CALLBACK)
			goto verify_and_return;

		g_debug ("cURL status-code was %ld", status_code);
		if (status_code == 429) {
			g_set_error (error,
				     AS_CURL_ERROR,
				     AS_CURL_ERROR_REMOTE,
				     /* TRANSLATORS: We got a 429 error while trying to download data */
				     _("Failed to download due to server limit"));
			return FALSE;
		}
		if (errbuf[0] != '\0') {
			g_set_error (error,
				     AS_CURL_ERROR,
				     AS_CURL_ERROR_DOWNLOAD,
				     _("Failed to download file: %s"),
				     errbuf);
			return FALSE;
		}
		g_set_error (error,
			     AS_CURL_ERROR,
			     AS_CURL_ERROR_DOWNLOAD,
			     _("Failed to download file: %s"),
			     curl_easy_strerror (res));
		return FALSE;
	}

verify_and_return:
	if (status_code == 404) {
		g_set_error (error,
			     AS_CURL_ERROR,
			     AS_CURL_ERROR_REMOTE,
			     /* TRANSLATORS: We tried to download an URL, but received a 404 error code */
			     _("URL was not found on the server."));
		return FALSE;
	} else if (status_code == 302) {
		/* redirects are fine, we ignore them until we reach a different code */
		return TRUE;
	} else if (status_code != 200) {
		g_set_error (error,
			     AS_CURL_ERROR,
			     AS_CURL_ERROR_REMOTE,
			     /* TRANSLATORS: We received an uexpected HTTP status code while talking to a server, likely an error */
			     _("Unexpected status code: %ld"),
			     status_code);
		return FALSE;
	}

	return TRUE;
}

/**
 * as_curl_set_cainfo:
 * @acurl: an #AsCurl instance.
 * @cainfo: Path to a CA file.
 *
 * Set a CA file holding one or more certificates to verify the peer with.
 **/
void
as_curl_set_cainfo (AsCurl *acurl, const gchar *cainfo)
{
	AsCurlPrivate *priv = GET_PRIVATE (acurl);
	curl_easy_setopt (priv->curl, CURLOPT_CAINFO, cainfo);
}

/**
 * as_curl_download_bytes:
 * @acurl: an #AsCurl instance.
 * @url: URL to download
 * @error: a #GError.
 *
 * Download an URL as GBytes, returns %NULL on error.
 **/
GBytes*
as_curl_download_bytes (AsCurl *acurl, const gchar *url, GError **error)
{
	AsCurlPrivate *priv = GET_PRIVATE (acurl);
	g_autoptr(GByteArray) buf = g_byte_array_new ();

	curl_easy_setopt (priv->curl, CURLOPT_URL, url);
	curl_easy_setopt (priv->curl, CURLOPT_WRITEFUNCTION, as_curl_download_write_bytearray_cb);
	curl_easy_setopt (priv->curl, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt (priv->curl, CURLOPT_XFERINFOFUNCTION, as_curl_progress_dummy_cb);
	curl_easy_setopt (priv->curl, CURLOPT_XFERINFODATA, acurl);

	if (!as_curl_perform_download (acurl, TRUE, error))
		return NULL;

	return g_byte_array_free_to_bytes (g_steal_pointer (&buf));
}

static size_t
as_curl_download_write_data_stream_cb (char *ptr, size_t size, size_t nmemb, void *udata)
{
	GOutputStream *ostream = G_OUTPUT_STREAM (udata);
	gsize bytes_written;
	gsize realsize = size * nmemb;

	g_output_stream_write_all (ostream,
				   ptr, realsize,
				   &bytes_written,
				   NULL, NULL);
	return bytes_written;
}

/**
 * as_curl_download_to_filename:
 * @acurl: an #AsCurl instance.
 * @url: URL to download
 * @fname: the filename to store the downloaded data
 * @error: a #GError.
 *
 * Download an URL and store it as filename.
 **/
gboolean
as_curl_download_to_filename (AsCurl *acurl,
			      const gchar *url,
			      const gchar *fname,
			      GError **error)
{
	AsCurlPrivate *priv = GET_PRIVATE (acurl);
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFileOutputStream) fos = NULL;
	g_autoptr(GDataOutputStream) dos = NULL;
	GError *tmp_error = NULL;

	file = g_file_new_for_path (fname);
	if (g_file_query_exists (file, NULL))
		fos = g_file_replace (file,
					NULL,
					FALSE,
					G_FILE_CREATE_REPLACE_DESTINATION,
					NULL,
					&tmp_error);
	else
		fos = g_file_create (file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &tmp_error);

	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	dos = g_data_output_stream_new (G_OUTPUT_STREAM (fos));

	curl_easy_setopt (priv->curl, CURLOPT_URL, url);
	curl_easy_setopt (priv->curl, CURLOPT_WRITEFUNCTION, as_curl_download_write_data_stream_cb);
	curl_easy_setopt (priv->curl, CURLOPT_WRITEDATA, dos);
	curl_easy_setopt (priv->curl, CURLOPT_XFERINFOFUNCTION, as_curl_progress_dummy_cb);
	curl_easy_setopt (priv->curl, CURLOPT_XFERINFODATA, acurl);

	if (!as_curl_perform_download (acurl, TRUE, error))
		return FALSE;

	return TRUE;
}

static int
as_curl_progress_check_url_cb (void *clientp,
				curl_off_t dltotal,
				curl_off_t dlnow,
				curl_off_t ultotal,
				curl_off_t ulnow)
{
	AsCurlPrivate *priv = GET_PRIVATE ((AsCurl*) clientp);
	priv->bytes_downloaded = dlnow;

	/* stop after 2kb have been successfully downloaded - it turns out a lot
	 * of downloads fail later, so just checking for the first byte is not enough */
	if (dlnow >= 2048)
		return 1;
	return 0;
}

/**
 * as_curl_check_url_exists:
 * @acurl: an #AsCurl instance.
 * @url: URL to download
 * @error: a #GError.
 *
 * Test if an URL exists by downloading the first few bytes of data,
 * then aborting if no issue was received.
 * If the resource could not be accessed, and error is returned.
 **/
gboolean
as_curl_check_url_exists (AsCurl *acurl, const gchar *url, GError **error)
{
	AsCurlPrivate *priv = GET_PRIVATE (acurl);
	g_autoptr(GByteArray) buf = g_byte_array_new ();

	curl_easy_setopt (priv->curl, CURLOPT_URL, url);
	curl_easy_setopt (priv->curl, CURLOPT_WRITEFUNCTION, as_curl_download_write_bytearray_cb);
	curl_easy_setopt (priv->curl, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt (priv->curl, CURLOPT_XFERINFOFUNCTION, as_curl_progress_check_url_cb);
	curl_easy_setopt (priv->curl, CURLOPT_XFERINFODATA, acurl);

	priv->bytes_downloaded = 0;
	if (!as_curl_perform_download (acurl, FALSE, error))
		return FALSE;

	/* check if it's a zero sized file */
	if (buf->len == 0 && priv->bytes_downloaded == 0) {
		g_set_error (error,
			     AS_CURL_ERROR,
			     AS_CURL_ERROR_SIZE,
			     /* TRANSLATORS: We tried to download from an URL, but the retrieved data was empty */
			     _("Retrieved file size was zero."));
		return FALSE;
	}

	return TRUE;
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
	curl_easy_setopt (priv->curl, CURLOPT_FOLLOWLOCATION, 1L);

	/* some servers redirect a lot, but 8 redirections seems to be enough for all common cases */
	curl_easy_setopt (priv->curl, CURLOPT_MAXREDIRS, 8L);

	/* no progress feedback by default (set dummy function so we can keep CURLOPT_NOPROGRESS at false */
	curl_easy_setopt (priv->curl, CURLOPT_XFERINFOFUNCTION, as_curl_progress_dummy_cb);
	curl_easy_setopt (priv->curl, CURLOPT_NOPROGRESS, 0L);

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
