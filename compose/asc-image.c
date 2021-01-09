/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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
} AscImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscImage, asc_image, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_image_get_instance_private (o))

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

static AscImage*
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
	g_autofree gchar *opng_stdout = NULL;
	g_autofree gchar *opng_stderr = NULL;
	g_autofree const gchar **argv = NULL;
	g_autoptr(GError) tmp_error = NULL;

	if (!asc_globals_get_use_optipng ())
		return TRUE;

	argv = g_new0 (const gchar*, 2 + 1);
	argv[0] = asc_globals_get_optipng_binary ();
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
				  g_strdup ((const gchar*) gdk_pixbuf_format_get_name (l->data)));

	return res;
}

/**
 * asc_image_new_from_file:
 * @fname: Name of the file to load.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscImage from a file on the filesystem.
 **/
AscImage*
asc_image_new_from_file (const gchar* fname, GError **error)
{
	g_autoptr(AscImage) image = asc_image_new();
	AscImagePrivate *priv = GET_PRIVATE (image);

	priv->pix = gdk_pixbuf_new_from_file (fname, error);
	if (priv->pix == NULL)
		return NULL;
	return g_steal_pointer (&image);
}

/**
 * asc_image_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscImage from data in memory.
 **/
AscImage*
asc_image_new_from_data (const void *data, gssize len, GError **error)
{
	g_autoptr(GInputStream) istream = NULL;
	g_autoptr(AscImage) image = asc_image_new();
	AscImagePrivate *priv = GET_PRIVATE (image);

	istream = g_memory_input_stream_new_from_data (data, len, NULL);
        priv->pix = gdk_pixbuf_new_from_stream (istream, NULL, error);
	if (priv->pix == NULL)
		return NULL;
	return g_steal_pointer (&image);
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
	return gdk_pixbuf_get_width (priv->pix);
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
	return gdk_pixbuf_get_height (priv->pix);
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
	GdkPixbuf *res_pix;

	res_pix = gdk_pixbuf_scale_simple (priv->pix, new_width, new_height, GDK_INTERP_BILINEAR);
	if (res_pix == NULL)
		g_error("Unable to allocate enough memory for image scaling.");

	/* set our current image to the scaled version */
	g_object_unref (priv->pix);
	priv->pix = res_pix;
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
 * asc_image_save_png:
 * @image: an #AscImage instance.
 * @fname: filename to save the image as.
 * @error: A #GError or %NULL
 *
 * Scale the image to fir in a square with the given edge length,
 * and keep its aspect ratio.
 **/
gboolean
asc_image_save_png (AscImage *image, const gchar *fname, GError **error)
{
	AscImagePrivate *priv = GET_PRIVATE (image);

        if (!gdk_pixbuf_save (priv->pix, fname, "png", error, NULL))
		return FALSE;

	asc_optimize_png (fname, error);
	return TRUE;
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
