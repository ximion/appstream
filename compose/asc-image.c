/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include "asc-image-private.h"

#include <gio/gio.h>
#include <math.h>

#include "asc-globals.h"
#include "asc-canvas.h"

struct _AscImage {
	GObject parent_instance;
};

typedef struct {
	VipsImage *vimg;
	gint width;
	gint height;
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
const gchar *
asc_image_format_to_string (AscImageFormat format)
{
	if (format == ASC_IMAGE_FORMAT_PNG)
		return "png";
	if (format == ASC_IMAGE_FORMAT_JXL)
		return "jxl";
	if (format == ASC_IMAGE_FORMAT_AVIF)
		return "avif";
	if (format == ASC_IMAGE_FORMAT_WEBP)
		return "webp";
	if (format == ASC_IMAGE_FORMAT_SVG)
		return "svg";
	if (format == ASC_IMAGE_FORMAT_SVGZ)
		return "svgz";
	if (format == ASC_IMAGE_FORMAT_JPEG)
		return "jpeg";
	if (format == ASC_IMAGE_FORMAT_GIF)
		return "gif";

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
	if (g_strcmp0 (str, "jxl") == 0)
		return ASC_IMAGE_FORMAT_JXL;
	if (g_strcmp0 (str, "avif") == 0)
		return ASC_IMAGE_FORMAT_AVIF;
	if (g_strcmp0 (str, "webp") == 0)
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_strcmp0 (str, "svg") == 0)
		return ASC_IMAGE_FORMAT_SVG;
	if (g_strcmp0 (str, "svgz") == 0)
		return ASC_IMAGE_FORMAT_SVGZ;
	if (g_strcmp0 (str, "jpeg") == 0)
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_strcmp0 (str, "gif") == 0)
		return ASC_IMAGE_FORMAT_GIF;
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
	if (g_str_has_suffix (fname_low, ".jxl"))
		return ASC_IMAGE_FORMAT_JXL;
	if (g_str_has_suffix (fname_low, ".avif"))
		return ASC_IMAGE_FORMAT_AVIF;
	if (g_str_has_suffix (fname_low, ".webp"))
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_str_has_suffix (fname_low, ".svg"))
		return ASC_IMAGE_FORMAT_SVG;
	if (g_str_has_suffix (fname_low, ".svgz"))
		return ASC_IMAGE_FORMAT_SVGZ;
	if (g_str_has_suffix (fname_low, ".jpeg") || g_str_has_suffix (fname_low, ".jpg"))
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_str_has_suffix (fname_low, ".gif"))
		return ASC_IMAGE_FORMAT_GIF;
	if (g_str_has_suffix (fname_low, ".xpm"))
		return ASC_IMAGE_FORMAT_XPM;

	return ASC_IMAGE_FORMAT_UNKNOWN;
}

static void
asc_image_finalize (GObject *object)
{
	AscImage *image = ASC_IMAGE (object);
	AscImagePrivate *priv = GET_PRIVATE (image);

	if (priv->vimg != NULL)
		g_object_unref (priv->vimg);

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
AscImage *
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

	argv = g_new0 (const gchar *, 2 + 1);
	argv[0] = optipng_path;
	argv[1] = fname;

	/* NOTE: Maybe add an option to run optipng with stronger optimization? (>= -o4) */
	r = g_spawn_sync (NULL, /* working directory */
			  (gchar **) argv,
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
			   fname,
			   exit_status,
			   opng_stderr ? opng_stderr : "",
			   opng_stdout ? opng_stdout : "");
	}

	return TRUE;
}

/**
 * asc_image_supported_format_names:
 *
 * Get a set of image format names we can currently read
 * (via VIPS).
 *
 * Returns: (transfer full): A hash set of format names.
 **/
GHashTable *
asc_image_supported_format_names (void)
{
	GHashTable *res;
	g_auto(GStrv) suffixes = NULL;

	res = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	suffixes = vips_foreign_get_suffixes ();
	if (suffixes != NULL) {
		for (guint i = 0; suffixes[i] != NULL; i++) {
			const gchar *sfx = suffixes[i];
			if (*sfx == '.')
				sfx++;

			if (g_ascii_strcasecmp (sfx, "png") == 0)
				g_hash_table_add (res, g_strdup ("png"));
			else if (g_ascii_strcasecmp (sfx, "jpeg") == 0 ||
				 g_ascii_strcasecmp (sfx, "jpg") == 0)
				g_hash_table_add (res, g_strdup ("jpeg"));
			else if (g_ascii_strcasecmp (sfx, "webp") == 0)
				g_hash_table_add (res, g_strdup ("webp"));
			else if (g_ascii_strcasecmp (sfx, "avif") == 0 ||
				 g_ascii_strcasecmp (sfx, "heif") == 0 ||
				 g_ascii_strcasecmp (sfx, "heic") == 0)
				g_hash_table_add (res, g_strdup ("avif"));
			else if (g_ascii_strcasecmp (sfx, "jxl") == 0)
				g_hash_table_add (res, g_strdup ("jxl"));
			else if (g_ascii_strcasecmp (sfx, "gif") == 0)
				g_hash_table_add (res, g_strdup ("gif"));
		}
	}

	/* SVG/SVGZ support is handled via our AscCanvas/librsvg path */
#ifdef HAVE_SVG_SUPPORT
	g_hash_table_add (res, g_strdup ("svg"));
	g_hash_table_add (res, g_strdup ("svgz"));
#endif

	return res;
}

/**
 * asc_image_set_vips_error:
 *
 * Convert the VIPS error buffer into a #GError and clear the VIPS buffer.
 **/
static void
asc_image_set_vips_error (GError **error, const gchar *context)
{
	g_set_error (error,
		     ASC_IMAGE_ERROR,
		     ASC_IMAGE_ERROR_FAILED,
		     "%s: %s",
		     context,
		     vips_error_buffer ());
	vips_error_clear ();
}

/**
 * asc_image_vips_ensure_alpha:
 *
 * Return the image with an alpha channel, adding a fully-opaque one if absent.
 *
 * Returns: a new #VipsImage or reference.
 **/
static VipsImage *
asc_image_vips_ensure_alpha (VipsImage *img, GError **error)
{
	VipsImage *out = NULL;

	if (vips_image_hasalpha (img))
		return g_object_ref (img);
	if (vips_addalpha (img, &out, NULL) != 0) {
		asc_image_set_vips_error (error, "Could not add alpha channel");
		return NULL;
	}
	return out;
}

/**
 * asc_image_load_vips:
 *
 * Smart-load a #VipsImage into an #AscImage, applying aspect-correct
 * resizing and transparent padding to reach @dest_width x @dest_height.
 **/
static gboolean
asc_image_load_vips (AscImage *image,
		     VipsImage *vimg,
		     gint dest_width,
		     gint dest_height,
		     gint src_size_min,
		     AscImageLoadFlags flags,
		     GError **error)
{
	gint img_width = vips_image_get_width (vimg);
	gint img_height = vips_image_get_height (vimg);
	gint tmp_width, tmp_height;
	g_autoptr(VipsImage) scaled = NULL;
	g_autoptr(VipsImage) with_alpha = NULL;
	g_autoptr(VipsImage) out = NULL;

	/* check minimum source size */
	if (src_size_min > 0 &&
	    img_width < src_size_min &&
	    img_height < src_size_min) {
		g_set_error (error,
			     ASC_IMAGE_ERROR,
			     ASC_IMAGE_ERROR_FAILED,
			     "Image was too small %ix%i",
			     img_width,
			     img_height);
		return FALSE;
	}

	/* nothing to do if the image already has the perfect size */
	if (img_width == dest_width && img_height == dest_height) {
		asc_image_set_vips (image, vimg);
		return TRUE;
	}

	/* force resize to exact target, ignoring aspect ratio (makes icons look
	 * blurry, but ensures they are properly aligned in UI layouts) */
	if (as_flags_contains (flags, ASC_IMAGE_LOAD_FLAG_ALWAYS_RESIZE)) {
		if (vips_thumbnail_image (vimg, &out, dest_width,
					  "height", dest_height,
					  "size", VIPS_SIZE_FORCE,
					  NULL) != 0) {
			asc_image_set_vips_error (error, "Failed to resize image");
			return FALSE;
		}
		asc_image_set_vips (image, out);
		return TRUE;
	}

	/* never scale up - center-pad with transparency instead */
	if (img_width < dest_width && img_height < dest_height) {
		g_debug ("Image padded to %dx%d as size %dx%d",
			 dest_width,
			 dest_height,
			 img_width,
			 img_height);

		with_alpha = asc_image_vips_ensure_alpha (vimg, error);
		if (with_alpha == NULL)
			return FALSE;

		if (vips_embed (with_alpha, &out,
				(dest_width - img_width) / 2,
				(dest_height - img_height) / 2,
				dest_width, dest_height,
				"extend", VIPS_EXTEND_BLACK, NULL) != 0) {
			asc_image_set_vips_error (error, "Failed to pad image");
			return FALSE;
		}
		asc_image_set_vips (image, out);
		return TRUE;
	}

	/* aspect ratio is perfectly square - scale to exact target */
	if (img_width == img_height) {
		if (vips_thumbnail_image (vimg, &out, dest_width,
					  "height", dest_height,
					  "size", VIPS_SIZE_DOWN,
					  NULL) != 0) {
			asc_image_set_vips_error (error, "Failed to scale image");
			return FALSE;
		}
		asc_image_set_vips (image, out);
		return TRUE;
	}

	/* non-square: scale to fit, then center-pad with transparency */
	if (img_width > img_height) {
		tmp_width  = dest_width;
		tmp_height = dest_height * img_height / img_width;
	} else {
		tmp_width  = dest_width * img_width / img_height;
		tmp_height = dest_height;
	}

	if (vips_thumbnail_image (vimg, &scaled, tmp_width,
				  "height", tmp_height,
				  "size", VIPS_SIZE_DOWN,
				  NULL) != 0) {
		asc_image_set_vips_error (error, "Failed to scale image");
		return FALSE;
	}

	if (as_flags_contains (flags, ASC_IMAGE_LOAD_FLAG_SHARPEN)) {
		g_autoptr(VipsImage) sharpened = NULL;
		/* best-effort sharpening; failure is non-fatal */
		if (vips_sharpen (scaled, &sharpened, NULL) == 0)
			g_set_object (&scaled, sharpened);
	}

	with_alpha = asc_image_vips_ensure_alpha (scaled, error);
	if (with_alpha == NULL)
		return FALSE;

	if (vips_embed (with_alpha, &out,
			(dest_width - tmp_width) / 2,
			(dest_height - tmp_height) / 2,
			dest_width, dest_height,
			"extend", VIPS_EXTEND_BLACK, NULL) != 0) {
		asc_image_set_vips_error (error, "Failed to pad image");
		return FALSE;
	}
	asc_image_set_vips (image, out);
	return TRUE;
}

/**
 * asc_image_new_from_file:
 * @fname: Name of the file to load.
 * @dest_width: The suggested width of the image, or 0 for the native size
 * @dest_height: The suggested height of the image, or 0 for the native size
 * @flags: a #AscImageLoadFlags, e.g. %ASC_IMAGE_LOAD_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Creates a new #AscImage from a file on the filesystem.
 **/
AscImage *
asc_image_new_from_file (const gchar *fname,
			 gint dest_width,
			 gint dest_height,
			 AscImageLoadFlags flags,
			 GError **error)
{
	gboolean ret;
	g_autoptr(AscImage) image = asc_image_new ();

	ret = asc_image_load_filename (image, fname, dest_width, dest_height, 0, flags, error);
	if (!ret)
		return NULL;
	return g_steal_pointer (&image);
}

/**
 * asc_image_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @dest_width: The suggested width of the constructed image, or 0 for the native size
 * @dest_height: The suggested height of the constructed image, or 0 for the native size
 * @flags: a #AscImageLoadFlags, e.g. %ASC_IMAGE_LOAD_FLAG_NONE
 * @format_hint: Assume the specified image format for data, use %ASC_IMAGE_FORMAT_UNKNOWN to guess.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscImage from data in memory.
 **/
AscImage *
asc_image_new_from_data (const void *data,
			 gssize len,
			 gint dest_width,
			 gint dest_height,
			 AscImageLoadFlags flags,
			 AscImageFormat format_hint,
			 GError **error)
{
	g_autoptr(AscImage) image = asc_image_new ();

	if (format_hint == ASC_IMAGE_FORMAT_SVG || format_hint == ASC_IMAGE_FORMAT_SVGZ) {
#ifdef HAVE_SVG_SUPPORT
		g_autoptr(GInputStream) istream = NULL;
		g_autoptr(GInputStream) dstream = NULL;
		g_autoptr(AscCanvas) cv = NULL;
		VipsImage *svg_img;

		istream = g_memory_input_stream_new_from_data (data, len, NULL);
		if (format_hint == ASC_IMAGE_FORMAT_SVGZ) {
			g_autoptr(GConverter) conv = G_CONVERTER (
			    g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
			dstream = g_converter_input_stream_new (istream, conv);
		} else {
			dstream = g_object_ref (istream);
		}

		cv = asc_canvas_new (dest_width > 0 ? dest_width : 64,
				     dest_height > 0 ? dest_height : 64);
		if (!asc_canvas_render_svg (cv, dstream, error))
			return NULL;

		svg_img = asc_canvas_to_image (cv, error);
		if (svg_img == NULL)
			return NULL;
		asc_image_set_vips (image, svg_img);
		g_object_unref (svg_img);
		return g_steal_pointer (&image);
#else
		g_set_error_literal (error,
				     ASC_IMAGE_ERROR,
				     ASC_IMAGE_ERROR_UNSUPPORTED,
				     "AppStream was built without SVG support.");
		return NULL;
#endif
	}

	/* all other formats are loaded via VIPS */
	{
		g_autoptr(VipsImage) img = NULL;

		img = vips_image_new_from_buffer ((void *) data, (size_t) len, "", NULL);
		if (img == NULL) {
			asc_image_set_vips_error (error, "Failed to load image from buffer");
			return NULL;
		}

		/* return at native size if no target dimensions were given */
		if (dest_width <= 0 && dest_height <= 0) {
			asc_image_set_vips (image, img);
			return g_steal_pointer (&image);
		}

		if (!asc_image_load_vips (image, img, dest_width, dest_height, 0, flags, error))
			return NULL;

		return g_steal_pointer (&image);
	}
}

/**
 * asc_image_load_filename:
 * @image: a #AscImage instance.
 * @filename: filename to read from
 * @dest_width: The suggested width of the constructed image, or 0 for the native size
 * @dest_height: The suggested height of the constructed image, or 0 for the native size
 * @src_size_min: The smallest source size (width or height) allowed, or 0 for no limit
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
			 gint dest_width,
			 gint dest_height,
			 gint src_size_min,
			 AscImageLoadFlags flags,
			 GError **error)
{
	gboolean is_svg;

	g_return_val_if_fail (ASC_IS_IMAGE (image), FALSE);

	is_svg = g_str_has_suffix (filename, ".svg") || g_str_has_suffix (filename, ".svgz");

	/* SVG/SVGZ: render via AscCanvas/librsvg for accurate vector scaling */
#ifdef HAVE_SVG_SUPPORT
	if (is_svg) {
		g_autoptr(GFile) file = NULL;
		g_autoptr(GInputStream) file_stream = NULL;
		g_autoptr(GInputStream) stream_data = NULL;
		g_autoptr(AscCanvas) cv = NULL;
		VipsImage *svg_img;

		file = g_file_new_for_path (filename);
		if (!g_file_query_exists (file, NULL)) {
			g_set_error_literal (error,
					     ASC_IMAGE_ERROR,
					     ASC_IMAGE_ERROR_FAILED,
					     "Image file does not exist");
			return FALSE;
		}

		file_stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
		if (file_stream == NULL)
			return FALSE;

		if (g_str_has_suffix (filename, ".svgz")) {
			g_autoptr(GConverter) conv = G_CONVERTER (
			    g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
			stream_data = g_converter_input_stream_new (file_stream, conv);
		} else {
			stream_data = g_object_ref (file_stream);
		}

		cv = asc_canvas_new (dest_width > 0 ? dest_width : 64,
				     dest_height > 0 ? dest_height : 64);
		if (!asc_canvas_render_svg (cv, stream_data, error))
			return FALSE;

		svg_img = asc_canvas_to_image (cv, error);
		if (svg_img == NULL)
			return FALSE;
		asc_image_set_vips (image, svg_img);
		g_object_unref (svg_img);
		return TRUE;
	}
#else
	if (is_svg) {
		g_warning ("Unable to load SVG graphic: AppStream built without SVG support.");
		g_set_error_literal (error,
				     ASC_IMAGE_ERROR,
				     ASC_IMAGE_ERROR_UNSUPPORTED,
				     "AppStream was built without SVG support. This is an issue "
				     "with your AppStream distribution. "
				     "Please rebuild AppStream with SVG support enabled or contact "
				     "your distributor to enable it for you.");
		return FALSE;
	}
#endif

	/* only support allowed types, unless support for any image is explicitly requested */
	if (!as_flags_contains (flags, ASC_IMAGE_LOAD_FLAG_ALLOW_UNSUPPORTED)) {
		const gchar *loader;
		g_autofree gchar *fmt_name = NULL;

		loader = vips_foreign_find_load (filename);
		if (loader == NULL) {
			g_set_error_literal (error,
					     ASC_IMAGE_ERROR,
					     ASC_IMAGE_ERROR_UNSUPPORTED,
					     "Image format was not recognized");
			return FALSE;
		}

		/* map the VIPS loader class name to our format name */
		if (strstr (loader, "Png") || strstr (loader, "png"))
			fmt_name = g_strdup ("png");
		else if (strstr (loader, "Jpeg") || strstr (loader, "jpeg"))
			fmt_name = g_strdup ("jpeg");
		else if (strstr (loader, "Webp") || strstr (loader, "webp"))
			fmt_name = g_strdup ("webp");
		else if (strstr (loader, "Heif") || strstr (loader, "heif") ||
			 strstr (loader, "Avif") || strstr (loader, "avif"))
			fmt_name = g_strdup ("avif");
		else if (strstr (loader, "Jxl") || strstr (loader, "jxl"))
			fmt_name = g_strdup ("jxl");
		else if (strstr (loader, "Gif") || strstr (loader, "gif"))
			fmt_name = g_strdup ("gif");

		if (fmt_name == NULL ||
		    asc_image_format_from_string (fmt_name) == ASC_IMAGE_FORMAT_UNKNOWN) {
			g_set_error (error,
				     ASC_IMAGE_ERROR,
				     ASC_IMAGE_ERROR_UNSUPPORTED,
				     "Image format from loader '%s' is not supported",
				     loader);
			return FALSE;
		}
	}

	/* load image at native size */
	{
		g_autoptr(VipsImage) vimg = NULL;

		vimg = vips_image_new_from_file (filename, NULL);
		if (vimg == NULL) {
			asc_image_set_vips_error (error, "Failed to load image");
			return FALSE;
		}

		/* return at native size if no target dimensions were given */
		if (dest_width <= 0 && dest_height <= 0) {
			asc_image_set_vips (image, vimg);
			return TRUE;
		}

		return asc_image_load_vips (image,
					    vimg,
					    dest_width,
					    dest_height,
					    src_size_min,
					    flags,
					    error);
	}
}

/**
 * asc_image_get_vips:
 * @image: a #AscImage instance.
 *
 * Gets the VIPS image if set.
 *
 * Returns: (transfer none): the #VipsImage, or %NULL
 **/
VipsImage *
asc_image_get_vips (AscImage *image)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	g_return_val_if_fail (ASC_IS_IMAGE (image), NULL);
	return priv->vimg;
}

/**
 * asc_image_set_vips:
 * @image: a #AscImage instance.
 * @img: the #VipsImage, or %NULL
 *
 * Sets the VIPS image.
 **/
void
asc_image_set_vips (AscImage *image, VipsImage *vimg)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	g_return_if_fail (ASC_IS_IMAGE (image));

	g_set_object (&priv->vimg, vimg);
	if (priv->vimg == NULL) {
		priv->width = 0;
		priv->height = 0;
		return;
	}
	priv->width = vips_image_get_width (priv->vimg);
	priv->height = vips_image_get_height (priv->vimg);
}

/**
 * asc_image_get_width:
 * @image: an #AscImage instance.
 *
 * Gets the image width.
 **/
gint
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
gint
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
 * @error: A #GError or %NULL
 *
 * Scale the image to the given size.
 **/
gboolean
asc_image_scale (AscImage *image, gint new_width, gint new_height, GError **error)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	g_autoptr(VipsImage) out = NULL;
	double hscale, vscale;

	g_return_val_if_fail (new_width > 0 && new_height > 0, FALSE);
	g_return_val_if_fail (priv->vimg != NULL, FALSE);

	hscale = (double) new_width / (double) vips_image_get_width (priv->vimg);
	vscale = (double) new_height / (double) vips_image_get_height (priv->vimg);

	if (vips_resize (priv->vimg, &out, hscale, "vscale", vscale, NULL) != 0) {
		asc_image_set_vips_error (error, "Unable to scale image");
		return FALSE;
	}

	asc_image_set_vips (image, out);
	return TRUE;
}

/**
 * asc_image_scale_to_width:
 * @image: an #AscImage instance.
 * @new_width: The new width.
 *
 * Scale the image to the given width, preserving
 * its aspect ratio.
 **/
gboolean
asc_image_scale_to_width (AscImage *image, gint new_width, GError **error)
{
	double scale;
	gint new_height;

	g_return_val_if_fail (new_width > 0, FALSE);

	scale = (double) new_width / (double) asc_image_get_width (image);
	new_height = floor (asc_image_get_height (image) * scale);

	return asc_image_scale (image, new_width, new_height, error);
}

/**
 * asc_image_scale_to_height:
 * @image: an #AscImage instance.
 * @new_height: the new height.
 * @error: A #GError or %NULL
 *
 * Scale the image to the given height, preserving
 * its aspect ratio.
 **/
gboolean
asc_image_scale_to_height (AscImage *image, gint new_height, GError **error)
{
	double scale;
	gint new_width;

	g_return_val_if_fail (new_height > 0, FALSE);

	scale = (double) new_height / (double) asc_image_get_height (image);
	new_width = floor (asc_image_get_width (image) * scale);

	return asc_image_scale (image, new_width, new_height, error);
}

/**
 * asc_image_scale_to_fit:
 * @image: an #AscImage instance.
 * @size: the maximum edge length.
 *
 * Scale the image to fit in a square with the given edge length,
 * and keep its aspect ratio.
 **/
gboolean
asc_image_scale_to_fit (AscImage *image, gint size, GError **error)
{
	g_return_val_if_fail (size > 0, FALSE);
	if (asc_image_get_height (image) > asc_image_get_width (image))
		return asc_image_scale_to_height (image, size, error);
	else
		return asc_image_scale_to_width (image, size, error);
}

/**
 * asc_render_svg_to_file:
 * @stream: Input stream with SVG data.
 * @width: Target width.
 * @height: Target height.
 * @format: Target image format, e.g. %ASC_IMAGE_FORMAT_PNG
 * @filename: Filename to write to.
 * @error: A #GError or %NULL
 *
 * Renders SVG data from a stream to a file in a specific format.
 *
 * Returns: %TRUE for success
 **/
gboolean
asc_render_svg_to_file (GInputStream *stream,
			gint width,
			gint height,
			AscImageFormat format,
			const gchar *filename,
			GError **error)
{
	g_autoptr(AscCanvas) cv = NULL;
	g_autoptr(VipsImage) vimg = NULL;

	g_return_val_if_fail (width > 0 && height > 0, FALSE);

	if (format == ASC_IMAGE_FORMAT_UNKNOWN) {
		g_set_error (error,
			     ASC_IMAGE_ERROR,
			     ASC_IMAGE_ERROR_UNSUPPORTED,
			     "Unknown image format specified");
		return FALSE;
	}
	if (format == ASC_IMAGE_FORMAT_SVG || format == ASC_IMAGE_FORMAT_SVGZ) {
		g_set_error (error,
			     ASC_IMAGE_ERROR,
			     ASC_IMAGE_ERROR_UNSUPPORTED,
			     "Can not render existing SVG data to SVG");
		return FALSE;
	}

	cv = asc_canvas_new (width, height);
	if (!asc_canvas_render_svg (cv, stream, error))
		return FALSE;

	if (format == ASC_IMAGE_FORMAT_PNG) {
		/* we can just save that PNG directly */
		return asc_canvas_save_png (cv, filename, error);
	}

	/* save to other formats */
	vimg = asc_canvas_to_image (cv, error);
	if (vimg == NULL)
		return FALSE;
	if (vips_image_write_to_file (vimg, filename, NULL) != 0) {
		g_set_error (error,
			     ASC_IMAGE_ERROR,
			     ASC_IMAGE_ERROR_FAILED,
			     "Failed to write image to '%s': %s",
			     filename,
			     vips_error_buffer ());
		vips_error_clear ();
		return FALSE;
	}
	return TRUE;
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
			 gint width,
			 gint height,
			 AscImageSaveFlags flags,
			 GError **error)
{
	AscImagePrivate *priv = GET_PRIVATE (image);
	g_autoptr(VipsImage) scaled = NULL;
	g_autoptr(VipsImage) alpha = NULL;
	g_autoptr(VipsImage) out = NULL;
	gint src_width, src_height;
	gint tmp_width, tmp_height;

	g_return_val_if_fail (ASC_IS_IMAGE (image), FALSE);

	if (priv->vimg == NULL) {
		g_set_error_literal (error, ASC_IMAGE_ERROR, ASC_IMAGE_ERROR_FAILED,
				     "No image data to save");
		return FALSE;
	}

	src_width = vips_image_get_width (priv->vimg);
	src_height = vips_image_get_height (priv->vimg);

	/* 0 means 'default' */
	if (width <= 0)
		width = src_width;
	if (height <= 0)
		height = src_height;

	/* is the aspect ratio of the source perfectly 16:9 */
	if (flags == ASC_IMAGE_SAVE_FLAG_NONE || (src_width / 16) * 9 == src_height) {
		if (vips_thumbnail_image (priv->vimg,
					  &scaled,
					  width,
					  "height",
					  height,
					  "size",
					  VIPS_SIZE_FORCE,
					  NULL) != 0) {
			asc_image_set_vips_error (error, "Unable to scale image");
			return FALSE;
		}
		out = g_object_ref (scaled);
	} else {
		/* create 16:9 output with alpha padding */
		if (src_width * 9 > src_height * 16) {
			tmp_width = width;
			tmp_height = width * src_height / src_width;
		} else {
			tmp_width = height * src_width / src_height;
			tmp_height = height;
		}
		if (vips_thumbnail_image (priv->vimg,
					  &scaled,
					  tmp_width,
					  "height",
					  tmp_height,
					  "size",
					  VIPS_SIZE_FORCE,
					  NULL) != 0) {
			asc_image_set_vips_error (error, "Unable to scale image for padding");
			return FALSE;
		}

		alpha = asc_image_vips_ensure_alpha (scaled, error);
		if (alpha == NULL)
			return FALSE;

		if (vips_embed (alpha,
				&out,
				(width - tmp_width) / 2,
				(height - tmp_height) / 2,
				width,
				height,
				"extend",
				VIPS_EXTEND_BLACK,
				NULL) != 0) {
			asc_image_set_vips_error (error, "Unable to pad image to 16:9");
			return FALSE;
		}
	}

	if (as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_SHARPEN)) {
		g_autoptr(VipsImage) sharpened = NULL;
		if (vips_sharpen (out, &sharpened, NULL) != 0) {
			asc_image_set_vips_error (error, "Unable to sharpen image");
			return FALSE;
		}
		g_set_object (&out, sharpened);
	}
	if (as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_BLUR)) {
		g_autoptr(VipsImage) blurred = NULL;
		if (vips_gaussblur (out, &blurred, 3.0, NULL) != 0) {
			asc_image_set_vips_error (error, "Unable to blur image");
			return FALSE;
		}
		g_set_object (&out, blurred);
	}

	if (vips_pngsave (out, filename, NULL) != 0) {
		asc_image_set_vips_error (error, "Unable to save image as PNG");
		return FALSE;
	}

	if (!as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_OPTIMIZE))
		return TRUE;
	return asc_optimize_png (filename, error);
}
