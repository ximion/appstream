/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
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
 * SECTION:asc-image
 * @short_description: Basic image rendering functions.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-image.h"

#include <gio/gio.h>
#include <math.h>

#include "asc-globals.h"

struct _AscImage
{
	GObject parent_instance;
};

typedef struct
{
	GdkPixbuf	*pix;
	guint		 width;
	guint		 height;
} AscImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscImage, asc_image, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_image_get_instance_private (o))

/**
 * asc_image_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
asc_image_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AscImageError");
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
const gchar*
asc_image_format_to_string (AscImageFormat format)
{
	if (format == ASC_IMAGE_FORMAT_PNG)
		return "png";
	if (format == ASC_IMAGE_FORMAT_JPEG)
		return "jpeg";
	if (format == ASC_IMAGE_FORMAT_GIF)
		return "gif";
	if (format == ASC_IMAGE_FORMAT_SVG)
		return "svg";
	if (format == ASC_IMAGE_FORMAT_SVGZ)
		return "svgz";
	if (format == ASC_IMAGE_FORMAT_WEBP)
		return "webp";
	if (format == ASC_IMAGE_FORMAT_AVIF)
		return "avif";
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
	if (g_strcmp0 (str, "jpeg") == 0)
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_strcmp0 (str, "gif") == 0)
		return ASC_IMAGE_FORMAT_GIF;
	if (g_strcmp0 (str, "svg") == 0)
		return ASC_IMAGE_FORMAT_SVG;
	if (g_strcmp0 (str, "svgz") == 0)
		return ASC_IMAGE_FORMAT_SVGZ;
	if (g_strcmp0 (str, "webp") == 0)
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_strcmp0 (str, "avif") == 0)
		return ASC_IMAGE_FORMAT_AVIF;
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
	if (g_str_has_suffix (fname_low, ".jpeg") || g_str_has_suffix (fname_low, ".jpg"))
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_str_has_suffix (fname_low, ".gif"))
		return ASC_IMAGE_FORMAT_GIF;
	if (g_str_has_suffix (fname_low, ".svg") )
		return ASC_IMAGE_FORMAT_SVG;
	if (g_str_has_suffix (fname_low, ".svgz"))
		return ASC_IMAGE_FORMAT_SVGZ;
	if (g_str_has_suffix (fname_low, ".webp"))
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_str_has_suffix (fname_low, ".avif"))
		return ASC_IMAGE_FORMAT_AVIF;
	if (g_str_has_suffix (fname_low, ".xpm"))
		return ASC_IMAGE_FORMAT_XPM;
	return ASC_IMAGE_FORMAT_UNKNOWN;
}

static void
asc_image_finalize (GObject *object)
{
	AscImage *image = ASC_IMAGE (object);
	AscImagePrivate *priv = GET_PRIVATE (image);

	if (priv->pix != NULL)
		g_object_unref (priv->pix);

	G_OBJECT_CLASS (asc_image_parent_class)->finalize (object);
}

static void
asc_image_init (AscImage *image)
{
}

static void
asc_image_class_init (AscImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_image_finalize;
}

/**
 * asc_image_new:
 *
 * Creates a new #AscImage.
 **/
AscImage*
asc_image_new (void)
{
	AscImage *image;
	image = g_object_new (ASC_TYPE_IMAGE, NULL);
	return ASC_IMAGE (image);
}

/**
 * asc_optimize_png:
 * @fname: Filename of the PNG image to optimize.
 * @error: A #GError or %NULL
 *
 * Optimizes a PNG graphic for size with optipng, if its binary
 * is available and this feature is enabled.
 **/
gboolean
asc_optimize_png (const gchar *fname, GError **error)
{
	gint exit_status;
	gboolean r;
	const gchar *optipng_path;
	g_autofree gchar *opng_stdout = NULL;
	g_autofree gchar *opng_stderr = NULL;
	g_autofree const gchar **argv = NULL;
	GError *tmp_error = NULL;

	if (!asc_globals_get_use_optipng ())
		return TRUE;

	optipng_path = asc_globals_get_optipng_binary ();
	if (optipng_path == NULL) {
		g_set_error (error,
			     ASC_IMAGE_ERROR,
			     ASC_IMAGE_ERROR_FAILED,
			     "optipng not found in $PATH");
		return FALSE;
	}

	argv = g_new0 (const gchar*, 2 + 1);
	argv[0] = optipng_path;
	argv[1] = fname;

	/* NOTE: Maybe add an option to run optipng with stronger optimization? (>= -o4) */
	r = g_spawn_sync (NULL, /* working directory */
			  (gchar**) argv,
			  NULL, /* envp */
			  G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
			  NULL, /* child setup */
			  NULL, /* user data */
			  &opng_stdout,
			  &opng_stderr,
			  &exit_status,
			  &tmp_error);
	if (!r) {
		g_propagate_prefixed_error (error, tmp_error, "Failed to spawn optipng.");
		return FALSE;
	}

	if (exit_status != 0) {
		/* FIXME: Maybe emit this as proper error, instead of just logging it? */
		g_warning ("Optipng on '%s' failed with error code %i: %s%s",
			   fname, exit_status, opng_stderr? opng_stderr : "", opng_stdout? opng_stdout : "");
        }

        return TRUE;
}

/**
 * asc_image_supported_format_names:
 *
 * Get a set of image format names we can currently read
 * (via GdkPixbuf).
 *
 * Returns: (transfer full): A hash set of format names.
 **/
GHashTable*
asc_image_supported_format_names (void)
{
	g_autoptr(GSList) fm_list = NULL;
	GHashTable *res;

	res = g_hash_table_new_full (g_str_hash,
				     g_str_equal,
				     g_free,
				     NULL);
	fm_list = gdk_pixbuf_get_formats ();

	if (fm_list == NULL)
		return res;

	for (GSList *l = fm_list; l != NULL; l = l->next)
		g_hash_table_add (res,
				  gdk_pixbuf_format_get_name (l->data));

	return res;
}

/**
 * asc_image_load_pixbuf:
 **/
static gboolean
asc_image_load_pixbuf (AscImage *image,
			GdkPixbuf *pixbuf,
			guint dest_size,
			guint src_size_min,
			AscImageLoadFlags flags,
			GError **error)
{
	guint pixbuf_height;
	guint pixbuf_width;
	guint tmp_height;
	guint tmp_width;
	g_autoptr(GdkPixbuf) pixbuf_tmp = NULL;
	g_autoptr(GdkPixbuf) pixbuf_new = NULL;

	/* check size */
	if (gdk_pixbuf_get_width (pixbuf) < (gint) src_size_min &&
	    gdk_pixbuf_get_height (pixbuf) < (gint) src_size_min) {
		g_set_error (error,
			     ASC_IMAGE_ERROR,
			     ASC_IMAGE_ERROR_FAILED,
			     "Image was too small %ix%i",
			     gdk_pixbuf_get_width (pixbuf),
			     gdk_pixbuf_get_height (pixbuf));
		return FALSE;
	}

	/* don't do anything to an icon with the perfect size */
	pixbuf_width = (guint) gdk_pixbuf_get_width (pixbuf);
	pixbuf_height = (guint) gdk_pixbuf_get_height (pixbuf);
	if (pixbuf_width == dest_size && pixbuf_height == dest_size) {
		asc_image_set_pixbuf (image, pixbuf);
		return TRUE;
	}

	/* this makes icons look blurry, but allows the software center to look
	 * good as icons are properly aligned in the UI layout */
	if (as_flags_contains (flags, ASC_IMAGE_LOAD_FLAG_ALWAYS_RESIZE)) {
		pixbuf_new = gdk_pixbuf_scale_simple (pixbuf,
						      (gint) dest_size,
						      (gint) dest_size,
						      GDK_INTERP_HYPER);
		asc_image_set_pixbuf (image, pixbuf_new);
		return TRUE;
	}

	/* never scale up, just pad */
	if (pixbuf_width < dest_size && pixbuf_height < dest_size) {
		g_debug ("icon padded to %ux%u as size %ux%u",
			 dest_size, dest_size,
			 pixbuf_width, pixbuf_height);
		pixbuf_new = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
					 (gint) dest_size, (gint) dest_size);
		gdk_pixbuf_fill (pixbuf_new, 0x00000000);
		gdk_pixbuf_copy_area (pixbuf,
				      0, 0, /* of src */
				      (gint) pixbuf_width,
				      (gint) pixbuf_height,
				      pixbuf_new,
				      (gint) (dest_size - pixbuf_width) / 2,
				      (gint) (dest_size - pixbuf_height) / 2);
		asc_image_set_pixbuf (image, pixbuf_new);
		return TRUE;
	}

	/* is the aspect ratio perfectly square */
	if (pixbuf_width == pixbuf_height) {
		pixbuf_new = gdk_pixbuf_scale_simple (pixbuf,
						      (gint) dest_size,
						      (gint) dest_size,
						      GDK_INTERP_HYPER);
		asc_image_set_pixbuf (image, pixbuf_new);
		return TRUE;
	}

	/* create new square pixbuf with alpha padding */
	pixbuf_new = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
				 (gint) dest_size, (gint) dest_size);
	gdk_pixbuf_fill (pixbuf_new, 0x00000000);
	if (pixbuf_width > pixbuf_height) {
		tmp_width = dest_size;
		tmp_height = dest_size * pixbuf_height / pixbuf_width;
	} else {
		tmp_width = dest_size * pixbuf_width / pixbuf_height;
		tmp_height = dest_size;
	}
	pixbuf_tmp = gdk_pixbuf_scale_simple (pixbuf,
					      (gint) tmp_width,
					      (gint) tmp_height,
					      GDK_INTERP_HYPER);
	if (as_flags_contains (flags, ASC_IMAGE_LOAD_FLAG_SHARPEN))
		asc_pixbuf_sharpen (pixbuf_tmp, 1, -0.5);
	gdk_pixbuf_copy_area (pixbuf_tmp,
			      0, 0, /* of src */
			      (gint) tmp_width, (gint) tmp_height,
			      pixbuf_new,
			      (gint) (dest_size - tmp_width) / 2,
			      (gint) (dest_size - tmp_height) / 2);
	asc_image_set_pixbuf (image, pixbuf_new);
	return TRUE;
}

/**
 * asc_image_new_from_file:
 * @fname: Name of the file to load.
 * @dest_size: The size of the constructed pixbuf, or 0 for the native size
 * @flags: a #AscImageLoadFlags, e.g. %ASC_IMAGE_LOAD_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Creates a new #AscImage from a file on the filesystem.
 **/
AscImage*
asc_image_new_from_file (const gchar* fname,
			 guint dest_size,
			 AscImageLoadFlags flags,
			 GError **error)
{
	gboolean ret;
	g_autoptr(AscImage) image = asc_image_new();

	ret = asc_image_load_filename (image,
					fname,
					dest_size,
					0,
					flags,
					error);
	if (!ret)
		return NULL;
	return g_steal_pointer (&image);
}

/**
 * asc_image_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @dest_size: The size of the constructed pixbuf, or 0 for the native size
 * @flags: a #AscImageLoadFlags, e.g. %ASC_IMAGE_LOAD_FLAG_NONE
 * @compressed: %TRUE if passed data is gzip-compressed
 * @error: A #GError or %NULL
 *
 * Creates a new #AscImage from data in memory.
 **/
AscImage*
asc_image_new_from_data (const void *data, gssize len,
			 guint dest_size,
			 gboolean compressed,
			 AscImageLoadFlags flags,
			 GError **error)
{
	gboolean ret;
	g_autoptr(GInputStream) istream = NULL;
	g_autoptr(GInputStream) dstream = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GdkPixbuf) pix = NULL;
	g_autoptr(AscImage) image = asc_image_new();

	istream = g_memory_input_stream_new_from_data (data, len, NULL);
	if (compressed) {
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		dstream = g_converter_input_stream_new (istream, conv);
	} else {
		dstream = g_object_ref (istream);
	}

	if (dest_size == 0) {
		/* use the native size and don't perform any scaling */
		pix = gdk_pixbuf_new_from_stream (dstream, NULL, error);
		if (pix == NULL)
			return NULL;

		asc_image_set_pixbuf (image, pix);
		return g_steal_pointer (&image);
	}

	/* load & scale */
	if (as_flags_contains (flags, ASC_IMAGE_LOAD_FLAG_ALWAYS_RESIZE)) {
		pix = gdk_pixbuf_new_from_stream_at_scale (dstream,
							dest_size, dest_size,
							TRUE,
							NULL,
							error);
		if (pix == NULL)
			return NULL;
	} else {
		/* just load, we will do resizing later */
		pix = gdk_pixbuf_new_from_stream (dstream, NULL, error);
		if (pix == NULL)
			return NULL;
	}
	ret = asc_image_load_pixbuf (image,
				     pix,
				     dest_size,
				     0,
				     flags,
				     error);
	if (!ret)
		return NULL;

	return g_steal_pointer (&image);
}

/**
 * asc_image_pixbuf_new_from_gz:
 *
 * Wrapper to allow GdkPixbuf to load SVG images from SVGZ files as well.
 */
static GdkPixbuf*
asc_image_pixbuf_new_from_gz (const gchar *filename, gint width, gint height, GError **error)
{
	g_autoptr(GFile) file = NULL;
	g_autoptr(GInputStream) file_stream = NULL;
	g_autoptr(GInputStream) stream_data = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GFileInfo) info = NULL;
	const gchar *content_type = NULL;

	file = g_file_new_for_path (filename);
	if (!g_file_query_exists (file, NULL)) {
		g_set_error_literal (error,
					ASC_IMAGE_ERROR,
					ASC_IMAGE_ERROR_FAILED,
					"Image file does not exist");
		return NULL;
	}
	info = g_file_query_info (file,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
	if (info != NULL)
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	file_stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
	if (file_stream == NULL)
		return NULL;

	if ((g_strcmp0 (content_type, "application/gzip") == 0) || (g_strcmp0 (content_type, "application/x-gzip") == 0)) {
		/* decompress the GZip stream */
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		stream_data = g_converter_input_stream_new (file_stream, conv);
	} else {
		stream_data = g_object_ref (file_stream);
	}

	if (width != 0 || height != 0)
		return gdk_pixbuf_new_from_stream_at_scale (stream_data,
							    width, height,
							    TRUE,
							    NULL,
							    error);
	else
		return gdk_pixbuf_new_from_stream (stream_data, NULL, error);
}

/**
 * asc_image_load_filename:
 * @image: a #AscImage instance.
 * @filename: filename to read from
 * @dest_size: The size of the constructed pixbuf, or 0 for the native size
 * @src_size_min: The smallest source size allowed, or 0 for none
 * @flags: a #AscImageLoadFlags, e.g. %ASC_IMAGE_LOAD_FLAG_NONE
 * @error: A #GError or %NULL.
 *
 * Reads an image from a file.
 *
 * Returns: %TRUE for success
 **/
gboolean
asc_image_load_filename (AscImage *image,
			 const gchar *filename,
			 guint dest_size,
			 guint src_size_min,
			 AscImageLoadFlags flags,
			 GError **error)
{
	g_autoptr(GdkPixbuf) pixbuf_src = NULL;

	g_return_val_if_fail (ASC_IS_IMAGE (image), FALSE);

	/* only support allowed types, unless support for any image is explicitly requested */
	if (!as_flags_contains(flags, ASC_IMAGE_LOAD_FLAG_ALLOW_UNSUPPORTED)) {
		GdkPixbufFormat *fmt;
		g_autofree gchar *name = NULL;
		fmt = gdk_pixbuf_get_file_info (filename, NULL, NULL);
		if (fmt == NULL) {
			g_set_error_literal (error,
					     ASC_IMAGE_ERROR,
					     ASC_IMAGE_ERROR_FAILED,
					     "Image format was not recognized");
			return FALSE;
		}
		name = gdk_pixbuf_format_get_name (fmt);
		if (asc_image_format_from_string (name) == ASC_IMAGE_FORMAT_UNKNOWN) {
			g_set_error (error,
				     ASC_IMAGE_ERROR,
				     ASC_IMAGE_ERROR_FAILED,
				     "Image format %s is not supported",
				     name);
			return FALSE;
		}
	}

	/* load the image of the native size */
	if (dest_size == 0) {
		g_autoptr(GdkPixbuf) pixbuf = NULL;
		pixbuf = asc_image_pixbuf_new_from_gz (filename, 0, 0, error);
		if (pixbuf == NULL)
			return FALSE;
		asc_image_set_pixbuf (image, pixbuf);
		return TRUE;
	}

	/* open file in native size */
	if (g_str_has_suffix (filename, ".svg") || g_str_has_suffix (filename, ".svgz")) {
		pixbuf_src = asc_image_pixbuf_new_from_gz (filename,
							   (gint) dest_size,
							   (gint) dest_size,
							   error);
	} else {
		pixbuf_src = asc_image_pixbuf_new_from_gz (filename, 0, 0, error);
	}
	if (pixbuf_src == NULL)
		return FALSE;

	/* create from pixbuf & resize */
	return asc_image_load_pixbuf (image,
				      pixbuf_src,
				      dest_size,
				      src_size_min,
				      flags,
				      error);
}

/**
 * asc_image_get_pixbuf:
 * @image: a #AscImage instance.
 *
 * Gets the image pixbuf if set.
 *
 * Returns: (transfer none): the #GdkPixbuf, or %NULL
 **/
GdkPixbuf *
asc_image_get_pixbuf (AscImage *image)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	g_return_val_if_fail (ASC_IS_IMAGE (image), NULL);
	return priv->pix;
}

/**
 * asc_image_set_pixbuf:
 * @image: a #AscImage instance.
 * @pixbuf: the #GdkPixbuf, or %NULL
 *
 * Sets the image pixbuf.
 **/
void
asc_image_set_pixbuf (AscImage *image, GdkPixbuf *pixbuf)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	g_return_if_fail (ASC_IS_IMAGE (image));

	g_set_object (&priv->pix, pixbuf);
	if (pixbuf == NULL)
		return;
	priv->width = (guint) gdk_pixbuf_get_width (pixbuf);
	priv->height = (guint) gdk_pixbuf_get_height (pixbuf);
}

/**
 * asc_image_get_width:
 * @image: an #AscImage instance.
 *
 * Gets the image width.
 **/
guint
asc_image_get_width (AscImage *image)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	return priv->width;
}

/**
 * asc_image_get_height:
 * @image: an #AscImage instance.
 *
 * Gets the image height.
 **/
guint
asc_image_get_height (AscImage *image)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	return priv->height;
}

/**
 * asc_image_scale:
 * @image: an #AscImage instance.
 * @new_width: The new width.
 * @new_height: the new height.
 *
 * Scale the image to the given size.
 **/
void
asc_image_scale (AscImage *image, guint new_width, guint new_height)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	g_autoptr(GdkPixbuf) res_pix = NULL;
	if (priv->pix == NULL)
		return;

	res_pix = gdk_pixbuf_scale_simple (priv->pix,
					   new_width,
					   new_height,
					   GDK_INTERP_BILINEAR);
	if (res_pix == NULL)
		g_error("Unable to allocate enough memory for image scaling.");

	/* set our current image to the scaled version */
	asc_image_set_pixbuf (image, res_pix);
}

/**
 * asc_image_scale_to_width:
 * @image: an #AscImage instance.
 * @new_width: The new width.
 *
 * Scale the image to the given width, preserving
 * its aspect ratio.
 **/
void
asc_image_scale_to_width (AscImage *image, guint new_width)
{
	double scale;
	guint new_height;
	scale = (double) new_width / (double) asc_image_get_width (image);
        new_height = floor (asc_image_get_height (image) * scale);

        asc_image_scale (image, new_width, new_height);
}

/**
 * asc_image_scale_to_height:
 * @image: an #AscImage instance.
 * @new_height: the new height.
 *
 * Scale the image to the given height, preserving
 * its aspect ratio.
 **/
void
asc_image_scale_to_height (AscImage *image, guint new_height)
{
	double scale;
	guint new_width;
	scale = (double) new_height / (double) asc_image_get_height (image);
        new_width = floor (asc_image_get_width (image) * scale);

        asc_image_scale (image, new_width, new_height);
}

/**
 * asc_image_scale_to_fit:
 * @image: an #AscImage instance.
 * @size: the maximum edge length.
 *
 * Scale the image to fir in a square with the given edge length,
 * and keep its aspect ratio.
 **/
void
asc_image_scale_to_fit (AscImage *image, guint size)
{
	if (asc_image_get_height (image) > asc_image_get_width (image))
		asc_image_scale_to_height (image, size);
	else
		asc_image_scale_to_width (image, size);
}

/**
 * asc_image_save_pixbuf:
 * @image: a #AscImage instance.
 * @width: target width, or 0 for default
 * @height: target height, or 0 for default
 * @flags: some #AscImageSaveFlags values, e.g. %ASC_IMAGE_SAVE_FLAG_PAD_16_9
 *
 * Resamples a pixbuf to a specific size.
 *
 * Returns: (transfer full): A #GdkPixbuf of the specified size
 **/
GdkPixbuf *
asc_image_save_pixbuf (AscImage *image,
			guint width,
			guint height,
			AscImageSaveFlags flags)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	GdkPixbuf *pixbuf = NULL;
	guint tmp_height;
	guint tmp_width;
	guint pixbuf_height;
	guint pixbuf_width;
	g_autoptr(GdkPixbuf) pixbuf_tmp = NULL;

	g_return_val_if_fail (ASC_IS_IMAGE (image), NULL);

	/* never set */
	if (priv->pix == NULL)
		return NULL;

	/* 0 means 'default' */
	if (width == 0)
		width = (guint) gdk_pixbuf_get_width (priv->pix);
	if (height == 0)
		height = (guint) gdk_pixbuf_get_height (priv->pix);

	/* don't do anything to an image with the correct size */
	pixbuf_width = (guint) gdk_pixbuf_get_width (priv->pix);
	pixbuf_height = (guint) gdk_pixbuf_get_height (priv->pix);
	if (width == pixbuf_width && height == pixbuf_height)
		return g_object_ref (priv->pix);

	/* is the aspect ratio of the source perfectly 16:9 */
	if (flags == ASC_IMAGE_SAVE_FLAG_NONE ||
	    (pixbuf_width / 16) * 9 == pixbuf_height) {
		pixbuf = gdk_pixbuf_scale_simple (priv->pix,
						  (gint) width, (gint) height,
						  GDK_INTERP_HYPER);
		if (as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_SHARPEN))
			asc_pixbuf_sharpen (pixbuf, 1, -0.5);
		if (as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_BLUR))
			asc_pixbuf_blur (pixbuf, 5, 3);
		return pixbuf;
	}

	/* create new 16:9 pixbuf with alpha padding */
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 TRUE, 8,
				 (gint) width,
				 (gint) height);
	gdk_pixbuf_fill (pixbuf, 0x00000000);
	/* check the ratio to see which property needs to be fitted and which needs
	 * to be reduced */
	if (pixbuf_width * 9 > pixbuf_height * 16) {
		tmp_width = width;
		tmp_height = width * pixbuf_height / pixbuf_width;
	} else {
		tmp_width = height * pixbuf_width / pixbuf_height;
		tmp_height = height;
	}
	pixbuf_tmp = gdk_pixbuf_scale_simple (priv->pix,
					      (gint) tmp_width,
					      (gint) tmp_height,
					      GDK_INTERP_HYPER);
	if (as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_SHARPEN))
		asc_pixbuf_sharpen (pixbuf_tmp, 1, -0.5);
	if (as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_BLUR))
		asc_pixbuf_blur (pixbuf_tmp, 5, 3);
	gdk_pixbuf_copy_area (pixbuf_tmp,
			      0, 0, /* of src */
			      (gint) tmp_width,
			      (gint) tmp_height,
			      pixbuf,
			      (gint) (width - tmp_width) / 2,
			      (gint) (height - tmp_height) / 2);
	return pixbuf;
}

/**
 * asc_image_save_filename:
 * @image: a #AscImage instance.
 * @filename: filename to write to
 * @width: target width, or 0 for default
 * @height: target height, or 0 for default
 * @flags: some #AscImageSaveFlags values, e.g. %ASC_IMAGE_SAVE_FLAG_PAD_16_9
 * @error: A #GError or %NULL.
 *
 * Saves the image to a file.
 *
 * Returns: %TRUE for success
 **/
gboolean
asc_image_save_filename (AscImage *image,
		        const gchar *filename,
		        guint width,
		        guint height,
		        AscImageSaveFlags flags,
		        GError **error)
{
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	/* save source file */
	pixbuf = asc_image_save_pixbuf (image, width, height, flags);
	if (!gdk_pixbuf_save (pixbuf, filename, "png", error, NULL))
		return FALSE;

	if (!as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_OPTIMIZE))
		return TRUE;
	return asc_optimize_png (filename, error);
}

static void
asc_pixbuf_blur_private (GdkPixbuf *src, GdkPixbuf *dest, gint radius, guchar *div_kernel_size)
{
	gint width, height, src_rowstride, dest_rowstride, n_channels;
	guchar *p_src, *p_dest, *c1, *c2;
	gint x, y, i, i1, i2, width_minus_1, height_minus_1, radius_plus_1;
	gint r, g, b, a;
	guchar *p_dest_row, *p_dest_col;

	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	n_channels = gdk_pixbuf_get_n_channels (src);
	radius_plus_1 = radius + 1;

	/* horizontal blur */
	p_src = gdk_pixbuf_get_pixels (src);
	p_dest = gdk_pixbuf_get_pixels (dest);
	src_rowstride = gdk_pixbuf_get_rowstride (src);
	dest_rowstride = gdk_pixbuf_get_rowstride (dest);
	width_minus_1 = width - 1;
	for (y = 0; y < height; y++) {

		/* calc the initial sums of the kernel */
		r = g = b = a = 0;
		for (i = -radius; i <= radius; i++) {
			c1 = p_src + (CLAMP (i, 0, width_minus_1) * n_channels);
			r += c1[0];
			g += c1[1];
			b += c1[2];
		}

		p_dest_row = p_dest;
		for (x = 0; x < width; x++) {
			/* set as the mean of the kernel */
			p_dest_row[0] = div_kernel_size[r];
			p_dest_row[1] = div_kernel_size[g];
			p_dest_row[2] = div_kernel_size[b];
			p_dest_row += n_channels;

			/* the pixel to add to the kernel */
			i1 = x + radius_plus_1;
			if (i1 > width_minus_1)
				i1 = width_minus_1;
			c1 = p_src + (i1 * n_channels);

			/* the pixel to remove from the kernel */
			i2 = x - radius;
			if (i2 < 0)
				i2 = 0;
			c2 = p_src + (i2 * n_channels);

			/* calc the new sums of the kernel */
			r += c1[0] - c2[0];
			g += c1[1] - c2[1];
			b += c1[2] - c2[2];
		}

		p_src += src_rowstride;
		p_dest += dest_rowstride;
	}

	/* vertical blur */
	p_src = gdk_pixbuf_get_pixels (dest);
	p_dest = gdk_pixbuf_get_pixels (src);
	src_rowstride = gdk_pixbuf_get_rowstride (dest);
	dest_rowstride = gdk_pixbuf_get_rowstride (src);
	height_minus_1 = height - 1;
	for (x = 0; x < width; x++) {

		/* calc the initial sums of the kernel */
		r = g = b = a = 0;
		for (i = -radius; i <= radius; i++) {
			c1 = p_src + (CLAMP (i, 0, height_minus_1) * src_rowstride);
			r += c1[0];
			g += c1[1];
			b += c1[2];
		}

		p_dest_col = p_dest;
		for (y = 0; y < height; y++) {
			/* set as the mean of the kernel */

			p_dest_col[0] = div_kernel_size[r];
			p_dest_col[1] = div_kernel_size[g];
			p_dest_col[2] = div_kernel_size[b];
			p_dest_col += dest_rowstride;

			/* the pixel to add to the kernel */
			i1 = y + radius_plus_1;
			if (i1 > height_minus_1)
				i1 = height_minus_1;
			c1 = p_src + (i1 * src_rowstride);

			/* the pixel to remove from the kernel */
			i2 = y - radius;
			if (i2 < 0)
				i2 = 0;
			c2 = p_src + (i2 * src_rowstride);

			/* calc the new sums of the kernel */
			r += c1[0] - c2[0];
			g += c1[1] - c2[1];
			b += c1[2] - c2[2];
		}

		p_src += n_channels;
		p_dest += n_channels;
	}
}

/**
 * as_pixbuf_blur:
 * @src: the GdkPixbuf.
 * @radius: the pixel radius for the gaussian blur, typical values are 1..3
 * @iterations: Amount to blur the image, typical values are 1..5
 *
 * Blurs an image. Warning, this method is s..l..o..w... for large images.
 *
 * Since: 0.14.0
 **/
void
asc_pixbuf_blur (GdkPixbuf *src, gint radius, gint iterations)
{
	gint kernel_size;
	gint i;
	g_autofree guchar *div_kernel_size = NULL;
	g_autoptr(GdkPixbuf) tmp = NULL;

	tmp = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			      gdk_pixbuf_get_has_alpha (src),
			      gdk_pixbuf_get_bits_per_sample (src),
			      gdk_pixbuf_get_width (src),
			      gdk_pixbuf_get_height (src));
	kernel_size = 2 * radius + 1;
	div_kernel_size = g_new (guchar, 256 * kernel_size);
	for (i = 0; i < 256 * kernel_size; i++)
		div_kernel_size[i] = (guchar) (i / kernel_size);

	while (iterations-- > 0)
		asc_pixbuf_blur_private (src, tmp, radius, div_kernel_size);
}

#define interpolate_value(original, reference, distance)		\
	(CLAMP (((distance) * (reference)) +				\
		((1.0 - (distance)) * (original)), 0, 255))

/**
 * as_pixbuf_sharpen:
 * @src: the GdkPixbuf.
 * @radius: the pixel radius for the unsharp mask, typical values are 1..3
 * @amount: Amount to sharpen the image, typical values are -0.1 to -0.9
 *
 * Sharpens an image. Warning, this method is s..l..o..w... for large images.
 **/
void
asc_pixbuf_sharpen (GdkPixbuf *src, gint radius, gdouble amount)
{
	gint width, height, rowstride, n_channels;
	gint x, y;
	guchar *p_blurred;
	guchar *p_blurred_row;
	guchar *p_src;
	guchar *p_src_row;
	g_autoptr(GdkPixbuf) blurred = NULL;

	blurred = gdk_pixbuf_copy (src);
	asc_pixbuf_blur (blurred, radius, 3);

	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	rowstride = gdk_pixbuf_get_rowstride (src);
	n_channels = gdk_pixbuf_get_n_channels (src);

	p_src = gdk_pixbuf_get_pixels (src);
	p_blurred = gdk_pixbuf_get_pixels (blurred);

	for (y = 0; y < height; y++) {
		p_src_row = p_src;
		p_blurred_row = p_blurred;
		for (x = 0; x < width; x++) {
			p_src_row[0] = (guchar) interpolate_value (p_src_row[0],
							  p_blurred_row[0],
							  amount);
			p_src_row[1] = (guchar) interpolate_value (p_src_row[1],
							  p_blurred_row[1],
							  amount);
			p_src_row[2] = (guchar) interpolate_value (p_src_row[2],
							  p_blurred_row[2],
							  amount);
			p_src_row += n_channels;
			p_blurred_row += n_channels;
		}
		p_src += rowstride;
		p_blurred += rowstride;
	}
}
