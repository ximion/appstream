/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asw-image
 * @short_description: Basic image rendering functions.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asw-image-private.h"

#include <gio/gio.h>
#include <math.h>

#include "asc-globals.h"
#include "asc-media.h"
#include "asw-canvas.h"

struct _AswImage {
	GObject parent_instance;
};

typedef struct {
	VipsImage *vimg;
	gint width;
	gint height;
} AswImagePrivate;

/**
 * AswImageSaverOptions:
 *
 * Fine-grained encoder settings used when writing images to disk.
 */
typedef struct {
	gint png_compression;
	gboolean png_palette;
	gint png_effort;
	gboolean jxl_lossless;
	gint jxl_quality;
	gint jxl_effort;
} AswImageSaverOptions;

/* Defaults for large images like screenshots: PNG at maximum compression
 * (optipng squeezes out the rest, if enabled), JPEG-XL with a good
 * quality/size balance. */
static const AswImageSaverOptions asw_default_saver_options = {
	.png_compression = 9,
	.png_palette = FALSE,
	.png_effort = 4,
	.jxl_lossless = FALSE,
	.jxl_quality = 90,
	.jxl_effort = 7,
};

/* Settings for small images like icons: lossless JPEG-XL, which for
 * icon-sized images is usually even smaller than lossy encoding. */
static const AswImageSaverOptions asw_icon_saver_options = {
	.png_compression = 9,
	.png_palette = FALSE,
	.png_effort = 4,
	.jxl_lossless = TRUE,
	.jxl_quality = 100,
	.jxl_effort = 7,
};

G_DEFINE_TYPE_WITH_PRIVATE (AswImage, asw_image, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asw_image_get_instance_private (o))

/**
 * asw_image_format_to_string:
 * @format: the %AswImageFormat.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @format
 **/
const gchar *
asw_image_format_to_string (AswImageFormat format)
{
	if (format == ASW_IMAGE_FORMAT_PNG)
		return "png";
	if (format == ASW_IMAGE_FORMAT_JXL)
		return "jxl";
	if (format == ASW_IMAGE_FORMAT_AVIF)
		return "avif";
	if (format == ASW_IMAGE_FORMAT_WEBP)
		return "webp";
	if (format == ASW_IMAGE_FORMAT_SVG)
		return "svg";
	if (format == ASW_IMAGE_FORMAT_SVGZ)
		return "svgz";
	if (format == ASW_IMAGE_FORMAT_JPEG)
		return "jpeg";
	if (format == ASW_IMAGE_FORMAT_GIF)
		return "gif";

	if (format == ASW_IMAGE_FORMAT_XPM)
		return "xpm";

	return NULL;
}

/**
 * asw_image_format_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AswImageFormat or %ASW_IMAGE_FORMAT_UNKNOWN for unknown
 **/
AswImageFormat
asw_image_format_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "png") == 0)
		return ASW_IMAGE_FORMAT_PNG;
	if (g_strcmp0 (str, "jxl") == 0)
		return ASW_IMAGE_FORMAT_JXL;
	if (g_strcmp0 (str, "avif") == 0)
		return ASW_IMAGE_FORMAT_AVIF;
	if (g_strcmp0 (str, "webp") == 0)
		return ASW_IMAGE_FORMAT_WEBP;
	if (g_strcmp0 (str, "svg") == 0)
		return ASW_IMAGE_FORMAT_SVG;
	if (g_strcmp0 (str, "svgz") == 0)
		return ASW_IMAGE_FORMAT_SVGZ;
	if (g_strcmp0 (str, "jpeg") == 0)
		return ASW_IMAGE_FORMAT_JPEG;
	if (g_strcmp0 (str, "gif") == 0)
		return ASW_IMAGE_FORMAT_GIF;
	if (g_strcmp0 (str, "xpm") == 0)
		return ASW_IMAGE_FORMAT_XPM;

	return ASW_IMAGE_FORMAT_UNKNOWN;
}

/**
 * asw_image_format_from_filename:
 * @fname: the filename.
 *
 * Returns the image format type based on the given file's filename.
 *
 * Returns: a #AswImageFormat or %ASW_IMAGE_FORMAT_UNKNOWN for unknown
 **/
AswImageFormat
asw_image_format_from_filename (const gchar *fname)
{
	g_autofree gchar *fname_low = g_ascii_strdown (fname, -1);

	if (g_str_has_suffix (fname_low, ".png"))
		return ASW_IMAGE_FORMAT_PNG;
	if (g_str_has_suffix (fname_low, ".jxl"))
		return ASW_IMAGE_FORMAT_JXL;
	if (g_str_has_suffix (fname_low, ".avif"))
		return ASW_IMAGE_FORMAT_AVIF;
	if (g_str_has_suffix (fname_low, ".webp"))
		return ASW_IMAGE_FORMAT_WEBP;
	if (g_str_has_suffix (fname_low, ".svg"))
		return ASW_IMAGE_FORMAT_SVG;
	if (g_str_has_suffix (fname_low, ".svgz"))
		return ASW_IMAGE_FORMAT_SVGZ;
	if (g_str_has_suffix (fname_low, ".jpeg") || g_str_has_suffix (fname_low, ".jpg"))
		return ASW_IMAGE_FORMAT_JPEG;
	if (g_str_has_suffix (fname_low, ".gif"))
		return ASW_IMAGE_FORMAT_GIF;
	if (g_str_has_suffix (fname_low, ".xpm"))
		return ASW_IMAGE_FORMAT_XPM;

	return ASW_IMAGE_FORMAT_UNKNOWN;
}

/**
 * asw_image_format_from_vips_loader:
 *
 * Map a libvips foreign-load operation class name (as returned by
 * vips_foreign_find_load() and friends) to an #AswImageFormat.
 */
static AswImageFormat
asw_image_format_from_vips_loader (const gchar *loader_name)
{
	if (loader_name == NULL)
		return ASW_IMAGE_FORMAT_UNKNOWN;

	if (g_str_has_prefix (loader_name, "VipsForeignLoadPng"))
		return ASW_IMAGE_FORMAT_PNG;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadJpeg"))
		return ASW_IMAGE_FORMAT_JPEG;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadJxl"))
		return ASW_IMAGE_FORMAT_JXL;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadWebp"))
		return ASW_IMAGE_FORMAT_WEBP;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadHeif"))
		return ASW_IMAGE_FORMAT_AVIF;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadSvg"))
		return ASW_IMAGE_FORMAT_SVG;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadNsgif") ||
	    g_str_has_prefix (loader_name, "VipsForeignLoadGif"))
		return ASW_IMAGE_FORMAT_GIF;

	return ASW_IMAGE_FORMAT_UNKNOWN;
}

/**
 * asw_vips_error:
 *
 * Set a #GError from the (thread-local) libvips error buffer and clear
 * the buffer, so errors can never leak into subsequent operations.
 *
 * Returns: Always %FALSE, for convenient use in return statements.
 */
static gboolean
asw_vips_error (const gchar *action, GError **error)
{
	g_autofree gchar *vips_msg = g_strdup (vips_error_buffer ());
	vips_error_clear ();
	g_set_error (error,
		     ASC_MEDIA_ERROR,
		     ASC_MEDIA_ERROR_FAILED,
		     "%s: %s",
		     action,
		     g_strchomp (vips_msg));
	return FALSE;
}

/**
 * asw_image_backend_init:
 * @argv0: The program name to register with libvips, or %NULL.
 * @error: A #GError or %NULL
 *
 * Initialize the libvips-based image processing backend and verify that
 * all image formats that we absolutely need are actually supported by
 * the installed libvips library.
 *
 * Returns: %TRUE on success.
 **/
gboolean
asw_image_backend_init (const gchar *argv0, GError **error)
{
	if (VIPS_INIT (argv0 != NULL ? argv0 : "appstream-compose"))
		return asw_vips_error ("Unable to initialize libvips", error);

	/* We process many unrelated, untrusted inputs sequentially, so a global
	 * operation cache would only retain memory without any reuse benefit. */
	vips_cache_set_max (0);

	/* Harden against untrusted input: block all image loaders, then explicitly
	 * permit only the formats that we want to support. */
	vips_operation_block_set ("VipsForeignLoad", TRUE);
	vips_operation_block_set ("VipsForeignLoadPng", FALSE);
	vips_operation_block_set ("VipsForeignLoadJpeg", FALSE);
	vips_operation_block_set ("VipsForeignLoadJxl", FALSE);
	vips_operation_block_set ("VipsForeignLoadWebp", FALSE);
	vips_operation_block_set ("VipsForeignLoadHeif", FALSE);
	vips_operation_block_set ("VipsForeignLoadNsgif", FALSE);
#ifdef HAVE_SVG_SUPPORT
	vips_operation_block_set ("VipsForeignLoadSvg", FALSE);
#endif

	/* support for PNG and JPEG-XL is an absolute requirement */
	if (vips_type_find ("VipsForeignLoad", "pngload_buffer") == 0 ||
	    vips_type_find ("VipsForeignSave", "pngsave") == 0) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "The libvips library was built without PNG support. "
				     "Please rebuild libvips with PNG support enabled or contact "
				     "your distributor to enable it for you.");
		return FALSE;
	}
	if (vips_type_find ("VipsForeignLoad", "jxlload_buffer") == 0 ||
	    vips_type_find ("VipsForeignSave", "jxlsave") == 0) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "The libvips library was built without JPEG-XL (libjxl) "
				     "support. Please rebuild libvips with JPEG-XL support enabled "
				     "or contact your distributor to enable it for you.");
		return FALSE;
	}
#ifdef HAVE_SVG_SUPPORT
	if (vips_type_find ("VipsForeignLoad", "svgload_buffer") == 0) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "The libvips library was built without SVG (librsvg) support. "
				     "Please rebuild libvips with SVG support enabled or contact "
				     "your distributor to enable it for you.");
		return FALSE;
	}
#endif

	/* allow tracking down refcount issues in the media worker */
	if (g_strcmp0 (g_getenv ("ASC_VIPS_LEAK"), "1") == 0)
		vips_leak_set (TRUE);

	return TRUE;
}

/**
 * asw_image_backend_shutdown:
 *
 * Shut down the libvips-based image processing backend.
 **/
void
asw_image_backend_shutdown (void)
{
	vips_shutdown ();
}

static void
asw_image_finalize (GObject *object)
{
	AswImage *image = ASW_IMAGE (object);
	AswImagePrivate *priv = GET_PRIVATE (image);

	g_clear_object (&priv->vimg);

	G_OBJECT_CLASS (asw_image_parent_class)->finalize (object);
}

static void
asw_image_init (AswImage *image)
{
}

static void
asw_image_class_init (AswImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asw_image_finalize;
}

/**
 * asw_image_new:
 *
 * Creates a new #AswImage.
 **/
AswImage *
asw_image_new (void)
{
	AswImage *image;
	image = g_object_new (ASW_TYPE_IMAGE, NULL);
	return ASW_IMAGE (image);
}

/**
 * asw_optimize_png:
 * @fname: Filename of the PNG image to optimize.
 * @error: A #GError or %NULL
 *
 * Optimizes a PNG graphic for size with optipng, if its binary
 * is available and this feature is enabled.
 **/
gboolean
asw_optimize_png (const gchar *fname, GError **error)
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
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_NOT_FOUND,
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
 * asw_image_supported_format_names:
 *
 * Get a set of image format names we can currently read
 * (via libvips).
 *
 * Returns: (transfer full): A hash set of format names.
 **/
GHashTable *
asw_image_supported_format_names (void)
{
	static const struct {
		const gchar *loader_nick;
		const gchar *format;
	} loaders[] = {
		{ "pngload_buffer",  "png"  },
		{ "jpegload_buffer", "jpeg" },
		{ "jxlload_buffer",  "jxl"  },
		{ "webpload_buffer", "webp" },
		{ "heifload_buffer", "avif" },
		{ "gifload_buffer",  "gif"  },
		{ NULL,		NULL   }
	};
	GHashTable *res;

	res = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (guint i = 0; loaders[i].loader_nick != NULL; i++) {
		if (vips_type_find ("VipsForeignLoad", loaders[i].loader_nick) != 0)
			g_hash_table_add (res, g_strdup (loaders[i].format));
	}

#ifdef HAVE_SVG_SUPPORT
	if (vips_type_find ("VipsForeignLoad", "svgload_buffer") != 0) {
		g_hash_table_add (res, g_strdup ("svg"));
		g_hash_table_add (res, g_strdup ("svgz"));
	}
#endif

	return res;
}

/**
 * asw_vips_normalize:
 *
 * Normalize an image to the flat 8-bit (s)RGB representation that all
 * subsequent operations expect, applying any ICC profile and reducing
 * higher bit depths in the process.
 */
static gboolean
asw_vips_normalize (VipsImage *in, VipsImage **out, GError **error)
{
	if (vips_thumbnail_image (in,
				  out,
				  vips_image_get_width (in),
				  "height",
				  vips_image_get_height (in),
				  "size",
				  VIPS_SIZE_FORCE,
				  "no_rotate",
				  TRUE,
				  NULL) != 0)
		return asw_vips_error ("Unable to normalize image", error);
	return TRUE;
}

/**
 * asw_vips_resize_exact:
 *
 * Resize an image to the exact given dimensions, using premultiplied
 * alpha and a high-quality (Lanczos3) kernel.
 */
static gboolean
asw_vips_resize_exact (VipsImage *in, VipsImage **out, gint width, gint height, GError **error)
{
	if (vips_thumbnail_image (in,
				  out,
				  width,
				  "height",
				  height,
				  "size",
				  VIPS_SIZE_FORCE,
				  "no_rotate",
				  TRUE,
				  NULL) != 0)
		return asw_vips_error ("Unable to resize image", error);
	return TRUE;
}

/**
 * asw_vips_pad_center:
 *
 * Center an image on a (usually larger) transparent canvas of the
 * given dimensions, without scaling it.
 */
static gboolean
asw_vips_pad_center (VipsImage *in,
		     VipsImage **out,
		     gint dest_width,
		     gint dest_height,
		     GError **error)
{
	g_autoptr(VipsImage) srgb = NULL;
	g_autoptr(VipsImage) rgba = NULL;
	VipsArrayDouble *bg;
	static const double transparent[4] = { 0.0, 0.0, 0.0, 0.0 };
	gint ret;

	/* the padding is transparent, so the image itself needs an alpha channel too */
	if (vips_colourspace (in, &srgb, VIPS_INTERPRETATION_sRGB, NULL) != 0)
		return asw_vips_error ("Unable to convert image to sRGB", error);
	if (vips_image_hasalpha (srgb)) {
		rgba = g_object_ref (srgb);
	} else {
		if (vips_addalpha (srgb, &rgba, NULL) != 0)
			return asw_vips_error ("Unable to add alpha channel to image", error);
	}

	bg = vips_array_double_new (transparent, 4);
	ret = vips_embed (rgba,
			  out,
			  (dest_width - vips_image_get_width (rgba)) / 2,
			  (dest_height - vips_image_get_height (rgba)) / 2,
			  dest_width,
			  dest_height,
			  "extend",
			  VIPS_EXTEND_BACKGROUND,
			  "background",
			  bg,
			  NULL);
	vips_area_unref (VIPS_AREA (bg));
	if (ret != 0)
		return asw_vips_error ("Unable to pad image", error);
	return TRUE;
}

/**
 * asw_image_store_vips:
 *
 * Fully evaluate an image pipeline into memory and make the result the
 * current image data of @image.
 * Decoding once into memory keeps the many renditions that may be derived
 * from one image cheap, and ensures no lazy pipeline outlives the input
 * buffer the image was loaded from.
 */
static gboolean
asw_image_store_vips (AswImage *image, VipsImage *vimg, GError **error)
{
	g_autoptr(VipsImage) vimg_mem = NULL;

	vimg_mem = vips_image_copy_memory (vimg);
	if (vimg_mem == NULL)
		return asw_vips_error ("Unable to read image data", error);
	asw_image_set_vips (image, vimg_mem);
	return TRUE;
}

/**
 * asw_vips_blur:
 * @in: Source image.
 * @out: (out): Location for the blurred image.
 * @sigma: Standard deviation of the gaussian blur.
 * @error: A #GError or %NULL
 *
 * Blurs an image.
 **/
gboolean
asw_vips_blur (VipsImage *in, VipsImage **out, gdouble sigma, GError **error)
{
	if (vips_gaussblur (in, out, sigma, NULL) != 0)
		return asw_vips_error ("Unable to blur image", error);
	return TRUE;
}

/**
 * asw_vips_sharpen:
 * @in: Source image.
 * @out: (out): Location for the sharpened image.
 * @sigma: Standard deviation of the unsharp mask, typical values are 1..3
 * @amount: Amount to sharpen the image, typical values are 0.1 to 0.9
 * @error: A #GError or %NULL
 *
 * Sharpens an image using an unsharp mask:
 * out = (1 + amount) * in - amount * blurred
 **/
gboolean
asw_vips_sharpen (VipsImage *in, VipsImage **out, gdouble sigma, gdouble amount, GError **error)
{
	g_autoptr(VipsImage) blurred = NULL;
	g_autoptr(VipsImage) base = NULL;
	g_autoptr(VipsImage) mask = NULL;
	g_autoptr(VipsImage) sum = NULL;

	if (vips_gaussblur (in, &blurred, sigma, NULL) != 0 ||
	    vips_linear1 (in, &base, 1.0 + amount, 0.0, NULL) != 0 ||
	    vips_linear1 (blurred, &mask, -amount, 0.0, NULL) != 0 ||
	    vips_add (base, mask, &sum, NULL) != 0 || vips_cast_uchar (sum, out, NULL) != 0)
		return asw_vips_error ("Unable to sharpen image", error);
	return TRUE;
}

/**
 * asw_image_load_vips:
 **/
static gboolean
asw_image_load_vips (AswImage *image,
		     VipsImage *vimg_raw,
		     gint dest_width,
		     gint dest_height,
		     gint src_size_min,
		     AswImageLoadFlags flags,
		     GError **error)
{
	g_autoptr(VipsImage) vimg = NULL;
	g_autoptr(VipsImage) vimg_scaled = NULL;
	g_autoptr(VipsImage) vimg_new = NULL;
	gint src_height;
	gint src_width;
	gint tmp_height;
	gint tmp_width;

	if (!asw_vips_normalize (vimg_raw, &vimg, error))
		return FALSE;
	src_width = vips_image_get_width (vimg);
	src_height = vips_image_get_height (vimg);

	/* check size */
	if (src_width < src_size_min && src_height < src_size_min) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Image was too small %ix%i",
			     src_width,
			     src_height);
		return FALSE;
	}

	/* don't do anything to an icon with the perfect size */
	if (src_width == dest_width && src_height == dest_height)
		return asw_image_store_vips (image, vimg, error);

	/* this makes icons look blurry, but allows the software center to look
	 * good as icons are properly aligned in the UI layout */
	if (as_flags_contains (flags, ASW_IMAGE_LOAD_FLAG_ALWAYS_RESIZE)) {
		if (!asw_vips_resize_exact (vimg, &vimg_new, dest_width, dest_height, error))
			return FALSE;
		return asw_image_store_vips (image, vimg_new, error);
	}

	/* never scale up, just pad */
	if (src_width < dest_width && src_height < dest_height) {
		g_debug ("Image padded to %dx%d as size %dx%d",
			 dest_width,
			 dest_height,
			 src_width,
			 src_height);
		if (!asw_vips_pad_center (vimg, &vimg_new, dest_width, dest_height, error))
			return FALSE;
		return asw_image_store_vips (image, vimg_new, error);
	}

	/* is the aspect ratio perfectly square */
	if (src_width == src_height) {
		if (!asw_vips_resize_exact (vimg, &vimg_new, dest_width, dest_height, error))
			return FALSE;
		return asw_image_store_vips (image, vimg_new, error);
	}

	/* scale, preserving the aspect ratio, and center on a transparent canvas */
	if (src_width > src_height) {
		tmp_width = dest_width;
		tmp_height = dest_height * src_height / src_width;
	} else {
		tmp_width = dest_width * src_width / src_height;
		tmp_height = dest_height;
	}
	if (!asw_vips_resize_exact (vimg, &vimg_scaled, tmp_width, tmp_height, error))
		return FALSE;
	if (as_flags_contains (flags, ASW_IMAGE_LOAD_FLAG_SHARPEN)) {
		g_autoptr(VipsImage) sharpened = NULL;
		if (!asw_vips_sharpen (vimg_scaled, &sharpened, 1.4, 0.5, error))
			return FALSE;
		g_set_object (&vimg_scaled, sharpened);
	}
	if (!asw_vips_pad_center (vimg_scaled, &vimg_new, dest_width, dest_height, error))
		return FALSE;
	return asw_image_store_vips (image, vimg_new, error);
}

/**
 * asw_image_new_from_file:
 * @fname: Name of the file to load.
 * @dest_width: The suggested width of the constructed image, or 0 for the native size
 * @dest_height: The suggested height of the constructed image, or 0 for the native size
 * @flags: a #AswImageLoadFlags, e.g. %ASW_IMAGE_LOAD_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Creates a new #AswImage from a file on the filesystem.
 **/
AswImage *
asw_image_new_from_file (const gchar *fname,
			 gint dest_width,
			 gint dest_height,
			 AswImageLoadFlags flags,
			 GError **error)
{
	gboolean ret;
	g_autoptr(AswImage) image = asw_image_new ();

	ret = asw_image_load_filename (image, fname, dest_width, dest_height, 0, flags, error);
	if (!ret)
		return NULL;
	return g_steal_pointer (&image);
}

/**
 * asw_image_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @dest_width: The suggested width of the constructed image, or 0 for the native size
 * @dest_height: The suggested height of the constructed image, or 0 for the native size
 * @flags: a #AswImageLoadFlags, e.g. %ASW_IMAGE_LOAD_FLAG_NONE
 * @format_hint: Assume the specified image format for data, use %ASW_IMAGE_FORMAT_UNKNOWN to guess.
 * @error: A #GError or %NULL
 *
 * Creates a new #AswImage from data in memory.
 **/
AswImage *
asw_image_new_from_data (const void *data,
			 gssize len,
			 gint dest_width,
			 gint dest_height,
			 AswImageLoadFlags flags,
			 AswImageFormat format_hint,
			 GError **error)
{
	g_autoptr(GBytes) raw_bytes = NULL;
	g_autoptr(VipsImage) vimg = NULL;
	g_autoptr(AswImage) image = asw_image_new ();

	if (format_hint == ASW_IMAGE_FORMAT_SVGZ) {
		/* decompress the GZip data first */
		g_autoptr(GInputStream) istream = NULL;
		g_autoptr(GInputStream) dstream = NULL;
		g_autoptr(GConverter) conv = NULL;
		g_autoptr(GOutputStream) ostream = NULL;

		istream = g_memory_input_stream_new_from_data (data, len, NULL);
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		dstream = g_converter_input_stream_new (istream, conv);
		ostream = g_memory_output_stream_new_resizable ();
		if (g_output_stream_splice (ostream,
					    dstream,
					    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
						G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
					    NULL,
					    error) < 0)
			return NULL;
		raw_bytes = g_memory_output_stream_steal_as_bytes (
		    G_MEMORY_OUTPUT_STREAM (ostream));
		data = g_bytes_get_data (raw_bytes, NULL);
		len = (gssize) g_bytes_get_size (raw_bytes);
	}

	if (dest_width <= 0 && dest_height <= 0) {
		g_autoptr(VipsImage) vimg_norm = NULL;

		/* use the native size and don't perform any scaling */
		vimg = vips_image_new_from_buffer (data, (size_t) len, "", NULL);
		if (vimg == NULL) {
			asw_vips_error ("Unable to load image", error);
			return NULL;
		}
		if (!asw_vips_normalize (vimg, &vimg_norm, error))
			return NULL;
		if (!asw_image_store_vips (image, vimg_norm, error))
			return NULL;
		return g_steal_pointer (&image);
	}

	/* load & scale */
	if (as_flags_contains (flags, ASW_IMAGE_LOAD_FLAG_ALWAYS_RESIZE)) {
		gint tmp_width = dest_width > 0 ? dest_width : dest_height;
		gint tmp_height = dest_height > 0 ? dest_height : dest_width;

		/* load already scaled to fit the target size, so any vector
		 * graphics are rendered at the right resolution */
		if (vips_thumbnail_buffer ((void *) data,
					   (size_t) len,
					   &vimg,
					   tmp_width,
					   "height",
					   tmp_height,
					   "size",
					   VIPS_SIZE_BOTH,
					   "no_rotate",
					   TRUE,
					   NULL) != 0) {
			asw_vips_error ("Unable to load image", error);
			return NULL;
		}
	} else {
		/* just load, we will do resizing later */
		vimg = vips_image_new_from_buffer (data, (size_t) len, "", NULL);
		if (vimg == NULL) {
			asw_vips_error ("Unable to load image", error);
			return NULL;
		}
	}

	if (!asw_image_load_vips (image, vimg, dest_width, dest_height, 0, flags, error))
		return NULL;

	return g_steal_pointer (&image);
}

/**
 * asw_image_load_filename:
 * @image: a #AswImage instance.
 * @filename: filename to read from
 * @dest_width: The suggested width of the constructed image, or 0 for the native size
 * @dest_height: The suggested height of the constructed image, or 0 for the native size
 * @src_size_min: The smallest source size (width or height) allowed, or 0 for no limit
 * @flags: a #AswImageLoadFlags, e.g. %ASW_IMAGE_LOAD_FLAG_NONE
 * @error: A #GError or %NULL.
 *
 * Reads an image from a file.
 *
 * Returns: %TRUE for success
 **/
gboolean
asw_image_load_filename (AswImage *image,
			 const gchar *filename,
			 gint dest_width,
			 gint dest_height,
			 gint src_size_min,
			 AswImageLoadFlags flags,
			 GError **error)
{
	g_autoptr(VipsImage) vimg_src = NULL;
	gboolean is_svg = FALSE;

	g_return_val_if_fail (ASW_IS_IMAGE (image), FALSE);

	is_svg = g_str_has_suffix (filename, ".svg") || g_str_has_suffix (filename, ".svgz");
#ifndef HAVE_SVG_SUPPORT
	if (is_svg) {
		g_warning ("Unable to load SVG graphic: AppStream built without SVG support.");
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "AppStream was built without SVG support. This is an issue "
				     "with your AppStream distribution. "
				     "Please rebuild AppStream with SVG support enabled or contact "
				     "your distributor to enable it for you.");
		return FALSE;
	}
#endif

	/* only support allowed types, unless support for any image is explicitly requested */
	if (!as_flags_contains (flags, ASW_IMAGE_LOAD_FLAG_ALLOW_UNSUPPORTED)) {
		const gchar *loader_name = vips_foreign_find_load (filename);
		if (loader_name == NULL) {
			vips_error_clear ();
			g_set_error_literal (error,
					     ASC_MEDIA_ERROR,
					     ASC_MEDIA_ERROR_UNSUPPORTED,
					     "Image format was not recognized");
			return FALSE;
		}
		if (asw_image_format_from_vips_loader (loader_name) == ASW_IMAGE_FORMAT_UNKNOWN) {
			g_set_error (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "Image format %s is not supported",
				     loader_name);
			return FALSE;
		}
	}

	/* load the image at its native size */
	if (dest_width <= 0 && dest_height <= 0) {
		g_autoptr(VipsImage) vimg_norm = NULL;

		vimg_src = vips_image_new_from_file (filename, NULL);
		if (vimg_src == NULL)
			return asw_vips_error ("Unable to load image", error);
		if (!asw_vips_normalize (vimg_src, &vimg_norm, error))
			return FALSE;
		return asw_image_store_vips (image, vimg_norm, error);
	}

	/* open file in native size, but render vector graphics to fit the target size */
	if (is_svg) {
		gint tmp_width = dest_width > 0 ? dest_width : dest_height;
		gint tmp_height = dest_height > 0 ? dest_height : dest_width;

		if (vips_thumbnail (filename,
				    &vimg_src,
				    tmp_width,
				    "height",
				    tmp_height,
				    "size",
				    VIPS_SIZE_BOTH,
				    "no_rotate",
				    TRUE,
				    NULL) != 0)
			return asw_vips_error ("Unable to render SVG image", error);
	} else {
		vimg_src = vips_image_new_from_file (filename, NULL);
		if (vimg_src == NULL)
			return asw_vips_error ("Unable to load image", error);
	}

	/* resize as requested */
	return asw_image_load_vips (image,
				    vimg_src,
				    dest_width,
				    dest_height,
				    src_size_min,
				    flags,
				    error);
}

/**
 * asw_image_get_vips:
 * @image: a #AswImage instance.
 *
 * Gets the image data as #VipsImage, if set.
 *
 * Returns: (transfer none): the #VipsImage, or %NULL
 **/
VipsImage *
asw_image_get_vips (AswImage *image)
{
	AswImagePrivate *priv = GET_PRIVATE (image);
	g_return_val_if_fail (ASW_IS_IMAGE (image), NULL);
	return priv->vimg;
}

/**
 * asw_image_set_vips:
 * @image: a #AswImage instance.
 * @vimg: the #VipsImage, or %NULL
 *
 * Sets the image data.
 **/
void
asw_image_set_vips (AswImage *image, VipsImage *vimg)
{
	AswImagePrivate *priv = GET_PRIVATE (image);
	g_return_if_fail (ASW_IS_IMAGE (image));

	g_set_object (&priv->vimg, vimg);
	if (vimg == NULL)
		return;
	priv->width = vips_image_get_width (vimg);
	priv->height = vips_image_get_height (vimg);
}

/**
 * asw_image_get_width:
 * @image: an #AswImage instance.
 *
 * Gets the image width.
 **/
gint
asw_image_get_width (AswImage *image)
{
	AswImagePrivate *priv = GET_PRIVATE (image);
	return priv->width;
}

/**
 * asw_image_get_height:
 * @image: an #AswImage instance.
 *
 * Gets the image height.
 **/
gint
asw_image_get_height (AswImage *image)
{
	AswImagePrivate *priv = GET_PRIVATE (image);
	return priv->height;
}

/**
 * asw_image_scale:
 * @image: an #AswImage instance.
 * @new_width: The new width.
 * @new_height: the new height.
 *
 * Scale the image to the given size.
 **/
void
asw_image_scale (AswImage *image, gint new_width, gint new_height)
{
	AswImagePrivate *priv = GET_PRIVATE (image);
	g_autoptr(VipsImage) vimg_scaled = NULL;
	g_autoptr(GError) error = NULL;

	g_return_if_fail (new_width > 0 && new_height > 0);
	g_return_if_fail (priv->vimg != NULL);

	if (!asw_vips_resize_exact (priv->vimg, &vimg_scaled, new_width, new_height, &error) ||
	    !asw_image_store_vips (image, vimg_scaled, &error))
		g_error ("Unable to scale image: %s", error->message);
}

/**
 * asw_image_scale_to_width:
 * @image: an #AswImage instance.
 * @new_width: The new width.
 *
 * Scale the image to the given width, preserving
 * its aspect ratio.
 **/
void
asw_image_scale_to_width (AswImage *image, gint new_width)
{
	double scale;
	gint new_height;

	g_return_if_fail (new_width > 0);

	scale = (double) new_width / (double) asw_image_get_width (image);
	new_height = floor (asw_image_get_height (image) * scale);

	asw_image_scale (image, new_width, new_height);
}

/**
 * asw_image_scale_to_height:
 * @image: an #AswImage instance.
 * @new_height: the new height.
 *
 * Scale the image to the given height, preserving
 * its aspect ratio.
 **/
void
asw_image_scale_to_height (AswImage *image, gint new_height)
{
	double scale;
	gint new_width;

	g_return_if_fail (new_height > 0);

	scale = (double) new_height / (double) asw_image_get_height (image);
	new_width = floor (asw_image_get_width (image) * scale);

	asw_image_scale (image, new_width, new_height);
}

/**
 * asw_image_scale_to_fit:
 * @image: an #AswImage instance.
 * @size: the maximum edge length.
 *
 * Scale the image to fir in a square with the given edge length,
 * and keep its aspect ratio.
 **/
void
asw_image_scale_to_fit (AswImage *image, gint size)
{
	g_return_if_fail (size > 0);
	if (asw_image_get_height (image) > asw_image_get_width (image))
		asw_image_scale_to_height (image, size);
	else
		asw_image_scale_to_width (image, size);
}

/**
 * asw_image_save_vips_to_file:
 *
 * Encode an image to a file in the given format, using the provided
 * encoder settings (or our defaults if @opts is %NULL).
 */
static gboolean
asw_image_save_vips_to_file (VipsImage *vimg,
			     const gchar *fname,
			     AswImageFormat format,
			     const AswImageSaverOptions *opts,
			     GError **error)
{
	if (opts == NULL)
		opts = &asw_default_saver_options;

	switch (format) {
	case ASW_IMAGE_FORMAT_PNG:
		if (opts->png_palette) {
			if (vips_pngsave (vimg,
					  fname,
					  "compression",
					  opts->png_compression,
					  "palette",
					  TRUE,
					  "effort",
					  opts->png_effort,
					  NULL) != 0)
				return asw_vips_error ("Unable to save PNG image", error);
		} else {
			if (vips_pngsave (vimg,
					  fname,
					  "compression",
					  opts->png_compression,
					  NULL) != 0)
				return asw_vips_error ("Unable to save PNG image", error);
		}
		return TRUE;
	case ASW_IMAGE_FORMAT_JXL:
		if (opts->jxl_lossless) {
			if (vips_jxlsave (vimg,
					  fname,
					  "lossless",
					  TRUE,
					  "effort",
					  opts->jxl_effort,
					  NULL) != 0)
				return asw_vips_error ("Unable to save JPEG-XL image", error);
		} else {
			if (vips_jxlsave (vimg,
					  fname,
					  "Q",
					  opts->jxl_quality,
					  "effort",
					  opts->jxl_effort,
					  NULL) != 0)
				return asw_vips_error ("Unable to save JPEG-XL image", error);
		}
		return TRUE;
	default:
		/* we only support writing PNG and JPEG-XL images */
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_UNSUPPORTED,
			     "Can not save image as %s",
			     asw_image_format_to_string (format));
		return FALSE;
	}
}

/**
 * asw_render_svg_to_file:
 * @stream: Input stream with SVG data.
 * @width: Target width.
 * @height: Target height.
 * @format: Target image format, e.g. %ASW_IMAGE_FORMAT_PNG
 * @filename: Filename to write to.
 * @error: A #GError or %NULL
 *
 * Renders SVG data from a stream to a file in a specific format.
 *
 * Returns: %TRUE for success
 **/
gboolean
asw_render_svg_to_file (GInputStream *stream,
			gint width,
			gint height,
			AswImageFormat format,
			const gchar *filename,
			GError **error)
{
	g_autoptr(AswCanvas) cv = NULL;
	g_autoptr(VipsImage) vimg = NULL;

	g_return_val_if_fail (width > 0 && height > 0, FALSE);

	if (format == ASW_IMAGE_FORMAT_UNKNOWN) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_UNSUPPORTED,
			     "Unknown image format specified");
		return FALSE;
	}
	if (format == ASW_IMAGE_FORMAT_SVG || format == ASW_IMAGE_FORMAT_SVGZ) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_UNSUPPORTED,
			     "Can not render existing SVG data to SVG");
		return FALSE;
	}

	cv = asw_canvas_new (width, height);
	if (!asw_canvas_render_svg (cv, stream, error))
		return FALSE;

	if (format == ASW_IMAGE_FORMAT_PNG) {
		/* we can just save that PNG directly */
		return asw_canvas_save_png (cv, filename, error);
	}

	/* save to other formats - this renders icons, so use the icon encoder settings */
	vimg = asw_canvas_to_vips (cv, error);
	if (vimg == NULL)
		return FALSE;
	return asw_image_save_vips_to_file (vimg, filename, format, &asw_icon_saver_options, error);
}

/**
 * asw_image_save_vips:
 * @image: a #AswImage instance.
 * @width: target width, or 0 for default
 * @height: target height, or 0 for default
 * @flags: some #AswImageSaveFlags values, e.g. %ASW_IMAGE_SAVE_FLAG_PAD_16_9
 * @error: A #GError or %NULL
 *
 * Resamples the image to a specific size.
 *
 * Returns: (transfer full): A #VipsImage of the specified size
 **/
VipsImage *
asw_image_save_vips (AswImage *image,
		     gint width,
		     gint height,
		     AswImageSaveFlags flags,
		     GError **error)
{
	AswImagePrivate *priv = GET_PRIVATE (image);
	g_autoptr(VipsImage) vimg_scaled = NULL;
	g_autoptr(VipsImage) vimg_new = NULL;
	gint tmp_height;
	gint tmp_width;
	gint src_height;
	gint src_width;

	g_return_val_if_fail (ASW_IS_IMAGE (image), NULL);

	/* never set */
	if (priv->vimg == NULL) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_FAILED,
				     "No image data was loaded.");
		return NULL;
	}

	src_width = vips_image_get_width (priv->vimg);
	src_height = vips_image_get_height (priv->vimg);

	/* 0 means 'default' */
	if (width <= 0)
		width = src_width;
	if (height <= 0)
		height = src_height;

	/* don't do anything to an image with the correct size */
	if (width == src_width && height == src_height)
		return g_object_ref (priv->vimg);

	/* is the aspect ratio of the source perfectly 16:9 */
	if (flags == ASW_IMAGE_SAVE_FLAG_NONE || (src_width / 16) * 9 == src_height) {
		if (!asw_vips_resize_exact (priv->vimg, &vimg_scaled, width, height, error))
			return NULL;
		if (as_flags_contains (flags, ASW_IMAGE_SAVE_FLAG_SHARPEN)) {
			g_autoptr(VipsImage) sharpened = NULL;
			if (!asw_vips_sharpen (vimg_scaled, &sharpened, 1.4, 0.5, error))
				return NULL;
			g_set_object (&vimg_scaled, sharpened);
		}
		if (as_flags_contains (flags, ASW_IMAGE_SAVE_FLAG_BLUR)) {
			g_autoptr(VipsImage) blurred = NULL;
			if (!asw_vips_blur (vimg_scaled, &blurred, 5.5, error))
				return NULL;
			g_set_object (&vimg_scaled, blurred);
		}
		return g_steal_pointer (&vimg_scaled);
	}

	/* scale and pad to a new 16:9 rectangle with transparency */
	if (src_width * 9 > src_height * 16) {
		tmp_width = width;
		tmp_height = width * src_height / src_width;
	} else {
		tmp_width = height * src_width / src_height;
		tmp_height = height;
	}
	if (!asw_vips_resize_exact (priv->vimg, &vimg_scaled, tmp_width, tmp_height, error))
		return NULL;
	if (as_flags_contains (flags, ASW_IMAGE_SAVE_FLAG_SHARPEN)) {
		g_autoptr(VipsImage) sharpened = NULL;
		if (!asw_vips_sharpen (vimg_scaled, &sharpened, 1.4, 0.5, error))
			return NULL;
		g_set_object (&vimg_scaled, sharpened);
	}
	if (as_flags_contains (flags, ASW_IMAGE_SAVE_FLAG_BLUR)) {
		g_autoptr(VipsImage) blurred = NULL;
		if (!asw_vips_blur (vimg_scaled, &blurred, 5.5, error))
			return NULL;
		g_set_object (&vimg_scaled, blurred);
	}
	if (!asw_vips_pad_center (vimg_scaled, &vimg_new, width, height, error))
		return NULL;
	return g_steal_pointer (&vimg_new);
}

/**
 * asw_image_save_filename:
 * @image: a #AswImage instance.
 * @filename: filename to write to
 * @width: target width, or 0 for default
 * @height: target height, or 0 for default
 * @flags: some #AswImageSaveFlags values, e.g. %ASW_IMAGE_SAVE_FLAG_PAD_16_9
 * @error: A #GError or %NULL.
 *
 * Saves the image to a file.
 *
 * Returns: %TRUE for success
 **/
gboolean
asw_image_save_filename (AswImage *image,
			 const gchar *filename,
			 gint width,
			 gint height,
			 AswImageSaveFlags flags,
			 GError **error)
{
	g_autoptr(VipsImage) vimg = NULL;

	/* save source file */
	vimg = asw_image_save_vips (image, width, height, flags, error);
	if (vimg == NULL)
		return FALSE;
	if (!asw_image_save_vips_to_file (vimg, filename, ASW_IMAGE_FORMAT_PNG, NULL, error))
		return FALSE;

	if (!as_flags_contains (flags, ASW_IMAGE_SAVE_FLAG_OPTIMIZE))
		return TRUE;
	return asw_optimize_png (filename, error);
}