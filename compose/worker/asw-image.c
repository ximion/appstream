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

/* Settings for images that must not lose any detail, primarily icons.
 * Lossless JPEG-XL actually produces smaller sizes for icon-style images with
 * their (compared to photos) simpler shapes and colors than its lossy profile does. */
static const AswImageSaverOptions asw_lossless_saver_options = {
	.png_compression = 9,
	.png_palette = FALSE,
	.png_effort = 4,
	.jxl_lossless = TRUE,
	.jxl_quality = 100,
	.jxl_effort = 7,
};

G_DEFINE_TYPE_WITH_PRIVATE (AswImage, asw_image, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asw_image_get_instance_private (o))

/* how much data we are willing to inflate a compressed image (SVGZ) into.
 * vector graphics are text, so they compress very well, but a legitimate icon
 * will never come anywhere close to this */
#define ASW_IMAGE_MAX_GUNZIP_SIZE_BYTES (64 * 1024 * 1024)

/* The SVG specification, librsvg, GdkPixbuf and every web browser convert physical units
 * (pt, mm, in) at 96 dots per inch, while libvips asks librsvg for 72. A drawing whose size
 * is given in physical units and that has no viewBox is laid out in a viewport that is only
 * 3/4 of its intended size that way, and anything reaching beyond that is simply cut off.
 * So we ask for the resolution everyone else assumes. */
#define ASW_SVG_DPI	  96
#define ASW_SVG_LOAD_OPTS "dpi=" G_STRINGIFY (ASW_SVG_DPI)

/* libvips applies the DPI as a plain multiplier on top of the size librsvg reports, so a load
 * that is supposed to yield the native size of a drawing has to cancel that factor out again.
 * We only do that where we actually want the native size: when rasterizing at a requested
 * size, libvips scales the drawing to that size anyway, and passing our own scale along would
 * interfere with the one it calculates. */
#define ASW_SVG_NATIVE_SCALE	 0.75 /* = 72.0 / ASW_SVG_DPI */
#define ASW_SVG_LOAD_OPTS_NATIVE ASW_SVG_LOAD_OPTS ",scale=" G_STRINGIFY (ASW_SVG_NATIVE_SCALE)

/**
 * asw_image_format_from_vips_loader:
 *
 * Map a libvips foreign-load operation class name (as returned by
 * vips_foreign_find_load() and friends) to an #AscImageFormat.
 */
static AscImageFormat
asw_image_format_from_vips_loader (const gchar *loader_name)
{
	if (loader_name == NULL)
		return ASC_IMAGE_FORMAT_UNKNOWN;

	if (g_str_has_prefix (loader_name, "VipsForeignLoadPng"))
		return ASC_IMAGE_FORMAT_PNG;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadJpeg"))
		return ASC_IMAGE_FORMAT_JPEG;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadJxl"))
		return ASC_IMAGE_FORMAT_JXL;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadWebp"))
		return ASC_IMAGE_FORMAT_WEBP;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadHeif"))
		return ASC_IMAGE_FORMAT_AVIF;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadSvg"))
		return ASC_IMAGE_FORMAT_SVG;
	if (g_str_has_prefix (loader_name, "VipsForeignLoadNsgif") ||
	    g_str_has_prefix (loader_name, "VipsForeignLoadGif"))
		return ASC_IMAGE_FORMAT_GIF;

	return ASC_IMAGE_FORMAT_UNKNOWN;
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

	/* Limit how many threads libvips uses: several worker processes may be
	 * running in parallel, and image operations scale poorly past this point
	 * anyway (PNG de-/encoding is single-threaded regardless, and even JPEG-XL
	 * scales less steeply after). An explicit setting wins, but the caller
	 * is still able to tweak this manually if needed. */
	if (g_getenv ("VIPS_CONCURRENCY") == NULL)
		vips_concurrency_set (MIN (vips_concurrency_get (), 8));

	/* Harden against untrusted input: block all image loaders, then explicitly
	 * permit only the formats that we want to support. */
	vips_operation_block_set ("VipsForeignLoad", TRUE);
	vips_operation_block_set ("VipsForeignLoadPng", FALSE);
	vips_operation_block_set ("VipsForeignLoadJpeg", FALSE);
	vips_operation_block_set ("VipsForeignLoadJxl", FALSE);
	vips_operation_block_set ("VipsForeignLoadWebp", FALSE);
	vips_operation_block_set ("VipsForeignLoadHeif", FALSE);
	vips_operation_block_set ("VipsForeignLoadNsgif", FALSE);
	vips_operation_block_set ("VipsForeignLoadSvg", FALSE);

	/* support for PNG, JPEG-XL and SVG is an absolute requirement */
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
	if (vips_type_find ("VipsForeignLoad", "svgload_buffer") == 0) {
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "The libvips library was built without SVG (librsvg) support. "
				     "Please rebuild libvips with SVG support enabled or contact "
				     "your distributor to enable it for you.");
		return FALSE;
	}

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

	if (vips_type_find ("VipsForeignLoad", "svgload_buffer") != 0) {
		/* the SVG loader transparently handles compressed SVG data as well */
		g_hash_table_add (res, g_strdup ("svg"));
		g_hash_table_add (res, g_strdup ("svgz"));
	}

	return res;
}

/**
 * asw_vips_normalize:
 *
 * Normalize an image to the flat 8-bit sRGB representation that all
 * subsequent operations expect, converting via any embedded ICC profile
 * and reducing higher bit depths in the process.
 * Everything we emit is sRGB, so consumers that do not colour-manage - which
 * is almost all of them - display our media correctly, and we can drop the
 * profile when saving.
 */
static gboolean
asw_vips_normalize (VipsImage *in, VipsImage **out, GError **error)
{
	/* only an embedded profile calls for an actual colour transform - without
	 * one the data already is sRGB and merely needs its band layout and bit
	 * depth adjusted, which is a great deal cheaper */
	if (vips_image_get_typeof (in, VIPS_META_ICC_NAME) != 0) {
		g_autoptr(VipsImage) converted = NULL;

		if (vips_icc_transform (in, &converted, "srgb", "embedded", TRUE, NULL) == 0) {
			*out = g_steal_pointer (&converted);
			return TRUE;
		}

		/* the input is untrusted, so an unusable profile must not make the
		 * image unusable as well: interpret its data as sRGB instead, which
		 * is what a viewer that does not colour-manage would do anyway */
		vips_error_clear ();
		g_debug ("Ignoring an ICC profile that we can not convert from.");
	}

	if (vips_colourspace (in, out, VIPS_INTERPRETATION_sRGB, NULL) != 0)
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
 * asw_vips_resize_fit:
 *
 * Resize an image to fit within the given dimensions, preserving its aspect
 * ratio, using premultiplied alpha and a high-quality (Lanczos3) kernel.
 * The result matches at least one of the given dimensions and never exceeds
 * either of them.
 */
static gboolean
asw_vips_resize_fit (VipsImage *in, VipsImage **out, gint width, gint height, GError **error)
{
	if (vips_thumbnail_image (in,
				  out,
				  width,
				  "height",
				  height,
				  "size",
				  VIPS_SIZE_BOTH,
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

	/* vips_embed() takes a negative offset for an image that does not fit and
	 * quietly crops it, which is never what we want */
	if (dest_width < vips_image_get_width (in) || dest_height < vips_image_get_height (in)) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_FAILED,
			     "Refusing to pad a %ix%i image onto a smaller %ix%i canvas.",
			     vips_image_get_width (in),
			     vips_image_get_height (in),
			     dest_width,
			     dest_height);
		return FALSE;
	}

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
 * asw_vips_sharpen:
 * @in: Source image.
 * @out: (out): Location for the sharpened image.
 * @error: A #GError or %NULL
 *
 * Sharpen an image to bring back detail that downscaling has softened.
 *
 * This works on lightness alone, in LAB space: an unsharp mask applied to
 * the color channels shifts hues, and one applied to the alpha channel
 * sharpens the transparency mask along with the picture.
 **/
static gboolean
asw_vips_sharpen (VipsImage *in, VipsImage **out, GError **error)
{
	if (vips_sharpen (in, out, NULL) != 0)
		return asw_vips_error ("Unable to sharpen image", error);
	return TRUE;
}

/**
 * asw_image_check_heif_codec:
 *
 * Refuse a HEIF image that is not AVIF.
 *
 * One libvips loader reads every codec a HEIF container may hold, so allowing
 * AVIF unavoidably lets HEIC and friends past the loader block list as well.
 * Those need patent-encumbered decoders that libheif ships as separate,
 * optional plugins, so accepting them would make the set of usable screenshots
 * depend on which plugin packages the machine building the catalog happens to
 * have. Turn them down deliberately instead, and say so.
 *
 * The codec is recorded in the container header, so this works the same
 * whether or not a decoder for it is installed.
 */
static gboolean
asw_image_check_heif_codec (VipsImage *vimg, GError **error)
{
	const gchar *compression = NULL;

	if (vips_image_get_typeof (vimg, "heif-compression") == 0)
		return TRUE;
	if (vips_image_get_string (vimg, "heif-compression", &compression) != 0) {
		vips_error_clear ();
		return TRUE;
	}
	if (g_strcmp0 (compression, "av1") == 0)
		return TRUE;

	g_set_error (error,
		     ASC_MEDIA_ERROR,
		     ASC_MEDIA_ERROR_UNSUPPORTED,
		     "Only AVIF images are read from HEIF containers, but this one holds "
		     "'%s' data.",
		     compression);
	return FALSE;
}

/**
 * asw_image_normalize_and_store:
 *
 * Bring a freshly decoded image into the flat 8-bit sRGB representation that
 * all further operations expect and make it the data of @image.
 *
 * No geometry is decided here: how a rendition is scaled and padded is
 * entirely up to the target it is written for.
 */
static gboolean
asw_image_normalize_and_store (AswImage *image, VipsImage *vimg_raw, GError **error)
{
	g_autoptr(VipsImage) vimg = NULL;

	if (!asw_image_check_heif_codec (vimg_raw, error))
		return FALSE;
	if (!asw_vips_normalize (vimg_raw, &vimg, error))
		return FALSE;
	return asw_image_store_vips (image, vimg, error);
}

/**
 * asw_image_new_from_file:
 * @fname: Name of the file to load.
 * @render_width: The width to rasterize vector graphics at, or 0 for their native size
 * @render_height: The height to rasterize vector graphics at, or 0 for their native size
 * @error: A #GError or %NULL
 *
 * Creates a new #AswImage from a file on the filesystem.
 **/
AswImage *
asw_image_new_from_file (const gchar *fname, gint render_width, gint render_height, GError **error)
{
	gboolean ret;
	g_autoptr(AswImage) image = asw_image_new ();

	ret = asw_image_load_filename (image, fname, render_width, render_height, error);
	if (!ret)
		return NULL;
	return g_steal_pointer (&image);
}

/**
 * asw_image_gunzip_bytes:
 *
 * Decompress gzip-compressed image data (in other words: SVGZ).
 *
 * VIPS does recognize gzipped SVG in a buffer, but only inflates about a kilobyte of it
 * in order to look for the SVG header. Vector graphics written by some tools carry a much
 * larger comment/doctype preamble than that, and would be rejected as an unknown format,
 * so we do the decompression ourselves to give VIPS something it can always identify.
 *
 * Returns: (transfer full): The decompressed data, or %NULL on error.
 **/
static GBytes *
asw_image_gunzip_bytes (const void *data, gssize len, GError **error)
{
	g_autoptr(GConverter) decomp = NULL;
	g_autoptr(GInputStream) mem_stream = NULL;
	g_autoptr(GInputStream) conv_stream = NULL;
	g_autoptr(GOutputStream) out_stream = NULL;

	gsize total_read = 0;

	decomp = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
	mem_stream = g_memory_input_stream_new_from_data (data, len, NULL);
	conv_stream = g_converter_input_stream_new (mem_stream, decomp);
	out_stream = g_memory_output_stream_new_resizable ();

	/* the data we handle here is untrusted, so we inflate it in chunks and give up as
	 * soon as the result grows out of proportion, rather than decompressing a bomb first
	 * and checking its size afterwards */
	while (TRUE) {
		guint8 buf[8192];
		gssize n_read = g_input_stream_read (conv_stream, buf, sizeof (buf), NULL, error);
		if (n_read < 0)
			return NULL;
		if (n_read == 0)
			break;

		total_read += (gsize) n_read;
		if (total_read > ASW_IMAGE_MAX_GUNZIP_SIZE_BYTES) {
			g_set_error_literal (
			    error,
			    ASC_MEDIA_ERROR,
			    ASC_MEDIA_ERROR_UNSUPPORTED,
			    "Refusing to process compressed image data that is too large.");
			return NULL;
		}

		if (!g_output_stream_write_all (out_stream, buf, n_read, NULL, NULL, error))
			return NULL;
	}

	if (!g_output_stream_close (out_stream, NULL, error))
		return NULL;
	return g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (out_stream));
}

/**
 * asw_image_check_data_format:
 *
 * Determine the format of the given (already decompressed) image data, and
 * reject anything we do not want to read.
 *
 * The set of readable formats is enforced by the loader block list that
 * asw_image_backend_init() installs, so this can not widen what libvips
 * accepts - it only lets us name the format we are turning down, instead of
 * leaving the caller with a generic "not in a known format".
 *
 * Returns: The detected format, or %ASC_IMAGE_FORMAT_UNKNOWN on error.
 */
static AscImageFormat
asw_image_check_data_format (const void *data, gssize len, GError **error)
{
	const gchar *loader_name = vips_foreign_find_load_buffer (data, (size_t) len);
	AscImageFormat format;

	if (loader_name == NULL) {
		vips_error_clear ();
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "Image format was not recognized");
		return ASC_IMAGE_FORMAT_UNKNOWN;
	}

	format = asw_image_format_from_vips_loader (loader_name);
	if (format == ASC_IMAGE_FORMAT_UNKNOWN)
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_UNSUPPORTED,
			     "Image format %s is not supported",
			     loader_name);
	return format;
}

/**
 * asw_image_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @render_width: The width to rasterize vector graphics at, or 0 for their native size
 * @render_height: The height to rasterize vector graphics at, or 0 for their native size
 * @error: A #GError or %NULL
 *
 * Creates a new #AswImage from data in memory.
 * The image format is detected from the data itself.
 *
 * The render size only says how vector graphics should be rasterized - raster
 * images are always decoded at their native size, and the geometry of every
 * rendition is decided when the image is saved.
 **/
AswImage *
asw_image_new_from_data (const void *data,
			 gssize len,
			 gint render_width,
			 gint render_height,
			 GError **error)
{
	g_autoptr(VipsImage) vimg = NULL;
	g_autoptr(AswImage) image = asw_image_new ();
	g_autoptr(GBytes) gunzipped = NULL;
	AscImageFormat format;

	/* transparently handle gzip-compressed data (SVGZ) */
	if (len > 2 && ((const guint8 *) data)[0] == 0x1f && ((const guint8 *) data)[1] == 0x8b) {
		gunzipped = asw_image_gunzip_bytes (data, len, error);
		if (gunzipped == NULL)
			return NULL;
		data = g_bytes_get_data (gunzipped, NULL);
		len = (gssize) g_bytes_get_size (gunzipped);
	}

	format = asw_image_check_data_format (data, len, error);
	if (format == ASC_IMAGE_FORMAT_UNKNOWN)
		return NULL;

	/* vector graphics can be rasterized at whatever resolution we ask for */
	if (format == ASC_IMAGE_FORMAT_SVG && (render_width > 0 || render_height > 0)) {
		gint tmp_width = render_width > 0 ? render_width : render_height;
		gint tmp_height = render_height > 0 ? render_height : render_width;

		/* rasterize the drawing at the resolution it is wanted in, so it stays
		 * sharp instead of being blown up from a smaller rendering later */
		if (vips_thumbnail_buffer ((void *) data,
					   (size_t) len,
					   &vimg,
					   tmp_width,
					   "option_string",
					   ASW_SVG_LOAD_OPTS,
					   "height",
					   tmp_height,
					   "size",
					   VIPS_SIZE_BOTH,
					   "no_rotate",
					   TRUE,
					   "fail_on",
					   VIPS_FAIL_ON_ERROR,
					   NULL) != 0) {
			asw_vips_error ("Unable to render image", error);
			return NULL;
		}
	} else {
		vimg = vips_image_new_from_buffer (data,
						   (size_t) len,
						   format == ASC_IMAGE_FORMAT_SVG
						       ? ASW_SVG_LOAD_OPTS_NATIVE
						       : "", /* option string */
						   "fail_on",
						   VIPS_FAIL_ON_ERROR,
						   NULL);
		if (vimg == NULL) {
			asw_vips_error ("Unable to load image", error);
			return NULL;
		}
	}

	if (!asw_image_normalize_and_store (image, vimg, error))
		return NULL;

	return g_steal_pointer (&image);
}

/**
 * asw_image_load_filename:
 * @image: a #AswImage instance.
 * @filename: filename to read from
 * @render_width: The width to rasterize vector graphics at, or 0 for their native size
 * @render_height: The height to rasterize vector graphics at, or 0 for their native size
 * @error: A #GError or %NULL.
 *
 * Reads an image from a file.
 *
 * As with asw_image_new_from_data(), the render size only applies to vector
 * graphics, and no rendition geometry is decided here.
 *
 * Returns: %TRUE for success
 **/
gboolean
asw_image_load_filename (AswImage *image,
			 const gchar *filename,
			 gint render_width,
			 gint render_height,
			 GError **error)
{
	g_autoptr(VipsImage) vimg_src = NULL;
	const gchar *loader_name = NULL;
	AscImageFormat format;

	g_return_val_if_fail (ASW_IS_IMAGE (image), FALSE);

	/* only read the formats we actually want to support. We go by what the loader made of
	 * the file rather than by its name, so we never hand SVG options to a loader that has
	 * no idea what to do with them */
	loader_name = vips_foreign_find_load (filename);
	if (loader_name == NULL) {
		vips_error_clear ();
		g_set_error_literal (error,
				     ASC_MEDIA_ERROR,
				     ASC_MEDIA_ERROR_UNSUPPORTED,
				     "Image format was not recognized");
		return FALSE;
	}
	format = asw_image_format_from_vips_loader (loader_name);
	if (format == ASC_IMAGE_FORMAT_UNKNOWN) {
		g_set_error (error,
			     ASC_MEDIA_ERROR,
			     ASC_MEDIA_ERROR_UNSUPPORTED,
			     "Image format %s is not supported",
			     loader_name);
		return FALSE;
	}

	/* open the file at its native size, but rasterize vector graphics at the
	 * resolution they were asked for */
	if (format == ASC_IMAGE_FORMAT_SVG && (render_width > 0 || render_height > 0)) {
		gint tmp_width = render_width > 0 ? render_width : render_height;
		gint tmp_height = render_height > 0 ? render_height : render_width;
		g_autoptr(VipsSource) source = vips_source_new_from_file (filename);
		if (source == NULL)
			return asw_vips_error ("Unable to read image", error);

		if (vips_thumbnail_source (source,
					   &vimg_src,
					   tmp_width,
					   "option_string",
					   ASW_SVG_LOAD_OPTS,
					   "height",
					   tmp_height,
					   "size",
					   VIPS_SIZE_BOTH,
					   "no_rotate",
					   TRUE,
					   "fail_on",
					   VIPS_FAIL_ON_ERROR,
					   NULL) != 0)
			return asw_vips_error ("Unable to render SVG image", error);
	} else if (format == ASC_IMAGE_FORMAT_SVG) {
		/* extra arguments are handed on to the loader that libvips selected */
		vimg_src = vips_image_new_from_file (filename,
						     "dpi",
						     (gdouble) ASW_SVG_DPI,
						     "scale",
						     (gdouble) ASW_SVG_NATIVE_SCALE,
						     "fail_on",
						     VIPS_FAIL_ON_ERROR,
						     NULL);
		if (vimg_src == NULL)
			return asw_vips_error ("Unable to load image", error);
	} else {
		vimg_src = vips_image_new_from_file (filename, "fail_on", VIPS_FAIL_ON_ERROR, NULL);
		if (vimg_src == NULL)
			return asw_vips_error ("Unable to load image", error);
	}

	return asw_image_normalize_and_store (image, vimg_src, error);
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
 * asw_image_save_vips_to_file:
 *
 * Encode an image to a file in the given format, using the provided
 * encoder settings (or our defaults if @opts is %NULL).
 *
 * All metadata is dropped: our images are normalized to sRGB, so no profile
 * needs to be embedded, and we must never copy Exif data (which may contain
 * GPS coordinates and similar) from an upstream image into catalog media.
 */
static gboolean
asw_image_save_vips_to_fd (VipsImage *vimg,
			   gint fd,
			   AscImageFormat format,
			   const AswImageSaverOptions *opts,
			   GError **error)
{
	g_autoptr(VipsTarget) target = NULL;

	if (opts == NULL)
		opts = &asw_default_saver_options;

	/* libvips duplicates the descriptor internally, so the caller keeps
	 * ownership of @fd and we must not close it here */
	target = vips_target_new_to_descriptor (fd);
	if (target == NULL)
		return asw_vips_error ("Unable to prepare the image output target", error);

	switch (format) {
	case ASC_IMAGE_FORMAT_PNG:
		if (opts->png_palette) {
			if (vips_pngsave_target (vimg,
						 target,
						 "compression",
						 opts->png_compression,
						 "palette",
						 TRUE,
						 "effort",
						 opts->png_effort,
						 "strip",
						 TRUE,
						 NULL) != 0)
				return asw_vips_error ("Unable to save PNG image", error);
		} else {
			if (vips_pngsave_target (vimg,
						 target,
						 "compression",
						 opts->png_compression,
						 "strip",
						 TRUE,
						 NULL) != 0)
				return asw_vips_error ("Unable to save PNG image", error);
		}
		return TRUE;
	case ASC_IMAGE_FORMAT_JXL:
		if (opts->jxl_lossless) {
			if (vips_jxlsave_target (vimg,
						 target,
						 "lossless",
						 TRUE,
						 "effort",
						 opts->jxl_effort,
						 "strip",
						 TRUE,
						 NULL) != 0)
				return asw_vips_error ("Unable to save JPEG-XL image", error);
		} else {
			if (vips_jxlsave_target (vimg,
						 target,
						 "Q",
						 opts->jxl_quality,
						 "effort",
						 opts->jxl_effort,
						 "strip",
						 TRUE,
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
			     asc_image_format_to_string (format));
		return FALSE;
	}
}

/**
 * asw_canvas_save_to_fd:
 * @canvas: The canvas to store.
 * @fd: Writable file descriptor to encode the canvas into.
 * @format: Target image format, e.g. %ASC_IMAGE_FORMAT_PNG
 * @lossless: %TRUE to encode without any loss of detail.
 * @error: A #GError or %NULL
 *
 * Saves a rendered canvas to a file descriptor in a specific format.
 * The descriptor stays owned by the caller.
 *
 * Returns: %TRUE for success
 **/
gboolean
asw_canvas_save_to_fd (AswCanvas *canvas,
		       gint fd,
		       AscImageFormat format,
		       gboolean lossless,
		       GError **error)
{
	g_autoptr(VipsImage) vimg = NULL;

	if (format == ASC_IMAGE_FORMAT_PNG) {
		/* we can just save that PNG directly */
		return asw_canvas_save_png_fd (canvas, fd, error);
	}

	vimg = asw_canvas_to_vips (canvas, error);
	if (vimg == NULL)
		return FALSE;
	return asw_image_save_vips_to_fd (vimg,
					  fd,
					  format,
					  lossless ? &asw_lossless_saver_options
						   : &asw_default_saver_options,
					  error);
}

/**
 * asw_image_save_vips:
 * @image: a #AswImage instance.
 * @width: target width, or 0 for default
 * @height: target height, or 0 for default
 * @flags: some #AscImageSaveFlags values, e.g. %ASC_IMAGE_SAVE_FLAG_SHARPEN
 * @error: A #GError or %NULL
 *
 * Scale the image to fit a specific size, preserving its aspect ratio, and
 * center the result on a transparent canvas of exactly that size.
 *
 * Returns: (transfer full): A #VipsImage of the specified size
 **/
VipsImage *
asw_image_save_vips (AswImage *image,
		     gint width,
		     gint height,
		     AscImageSaveFlags flags,
		     GError **error)
{
	AswImagePrivate *priv = GET_PRIVATE (image);
	g_autoptr(VipsImage) vimg_scaled = NULL;
	g_autoptr(VipsImage) vimg_new = NULL;
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

	if (!asw_vips_resize_fit (priv->vimg, &vimg_scaled, width, height, error))
		return NULL;

	if (as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_SHARPEN)) {
		g_autoptr(VipsImage) sharpened = NULL;
		if (!asw_vips_sharpen (vimg_scaled, &sharpened, error))
			return NULL;
		g_set_object (&vimg_scaled, sharpened);
	}

	/* the scaled image only matches one of the target dimensions unless it happens
	 * to share the aspect ratio of the target, so pad it out to the full canvas */
	if (!asw_vips_pad_center (vimg_scaled, &vimg_new, width, height, error))
		return NULL;
	return g_steal_pointer (&vimg_new);
}

/**
 * asw_image_save_fd:
 * @image: a #AswImage instance.
 * @fd: Writable file descriptor to encode the image into.
 * @format: Target image format, only %ASC_IMAGE_FORMAT_PNG and
 *          %ASC_IMAGE_FORMAT_JXL are permitted.
 * @width: target width, or 0 for default
 * @height: target height, or 0 for default
 * @flags: some #AscImageSaveFlags values, e.g. %ASC_IMAGE_SAVE_FLAG_SHARPEN
 * @error: A #GError or %NULL.
 *
 * Saves the image to a file descriptor, which stays owned by the caller.
 *
 * PNG size optimization (%ASC_IMAGE_SAVE_FLAG_OPTIMIZE) is not performed
 * here: it runs in the client process, which owns the resulting file.
 *
 * Returns: %TRUE for success
 **/
gboolean
asw_image_save_fd (AswImage *image,
		   gint fd,
		   AscImageFormat format,
		   gint width,
		   gint height,
		   AscImageSaveFlags flags,
		   GError **error)
{
	g_autoptr(VipsImage) vimg = NULL;

	vimg = asw_image_save_vips (image, width, height, flags, error);
	if (vimg == NULL)
		return FALSE;
	return asw_image_save_vips_to_fd (vimg,
					  fd,
					  format,
					  as_flags_contains (flags, ASC_IMAGE_SAVE_FLAG_LOSSLESS)
					      ? &asw_lossless_saver_options
					      : &asw_default_saver_options,
					  error);
}
