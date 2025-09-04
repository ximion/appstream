/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2025 Matthias Klumpp <matthias@tenstral.net>
 *
 * GdkPixbuf conversion authors:
 *     Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *     Cody Russell <bratsche@dfw.net>
 *     Federico Mena-Quintero <federico@gimp.org>
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
 * SECTION:asc-canvas
 * @short_description: Draw text and render SVG graphics.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-canvas.h"

#include <cairo.h>
#include <cairo-ft.h>
#include <math.h>
#ifdef HAVE_SVG_SUPPORT
#include <librsvg/rsvg.h>
#endif

#include "as-utils-private.h"
#include "asc-font-private.h"
#include "asc-image-private.h"

struct _AscCanvas {
	GObject parent_instance;
};

typedef struct {
	cairo_t *cr;
	cairo_surface_t *srf;

	gint width;
	gint height;
} AscCanvasPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscCanvas, asc_canvas, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_canvas_get_instance_private (o))

/**
 * asc_canvas_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
asc_canvas_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AscCanvasError");
	return quark;
}

static void
asc_canvas_finalize (GObject *object)
{
	AscCanvas *canvas = ASC_CANVAS (object);
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);

	if (priv->cr != NULL)
		cairo_destroy (priv->cr);
	if (priv->srf != NULL)
		cairo_surface_destroy (priv->srf);

	G_OBJECT_CLASS (asc_canvas_parent_class)->finalize (object);
}

static void
asc_canvas_init (AscCanvas *canvas)
{
}

static void
asc_canvas_class_init (AscCanvasClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_canvas_finalize;
}

/**
 * asc_canvas_new:
 *
 * Creates a new #AscFont.
 *
 * Returns: (transfer full): an #AscCanvas
 **/
AscCanvas *
asc_canvas_new (gint width, gint height)
{
	AscCanvasPrivate *priv;
	AscCanvas *canvas = ASC_CANVAS (g_object_new (ASC_TYPE_CANVAS, NULL));
	priv = GET_PRIVATE (canvas);

	priv->srf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
	priv->cr = cairo_create (priv->srf);

	priv->width = width;
	priv->height = height;

	return canvas;
}

/**
 * asc_canvas_get_width:
 * @canvas: an #AscCanvas instance.
 *
 * Gets the canvas width.
 **/
guint
asc_canvas_get_width (AscCanvas *canvas)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	return priv->width;
}

/**
 * asc_canvas_get_height:
 * @canvas: an #AscCanvas instance.
 *
 * Gets the canvas height.
 **/
guint
asc_canvas_get_height (AscCanvas *canvas)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	return priv->height;
}

/**
 * asc_canvas_render_svg:
 * @canvas: an #AscCanvas instance.
 * @stream: SVG data input stream.
 * @error: A #GError or %NULL
 *
 * Render an SVG graphic from the SVG data provided.
 **/
gboolean
asc_canvas_render_svg (AscCanvas *canvas, GInputStream *stream, GError **error)
{
#ifdef HAVE_SVG_SUPPORT
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	RsvgHandle *handle = NULL;
	gboolean ret = FALSE;
	gdouble srf_width, srf_height;
#if LIBRSVG_CHECK_VERSION(2, 52, 0)
	RsvgRectangle viewport;
#else
	RsvgDimensionData dims;
#endif

	/* NOTE: unfortunately, Cairo/RSvg may use Fontconfig internally, so
	 * we need to lock this down since a parallel-processed font
	 * might need to access this too. */
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);

	handle = rsvg_handle_new_from_stream_sync (stream,
						   NULL,
						   RSVG_HANDLE_FLAGS_NONE,
						   NULL,
						   error);
	if (handle == NULL)
		goto out;
	rsvg_handle_set_dpi (handle, 100);

	srf_width = (gdouble) cairo_image_surface_get_width (priv->srf);
	srf_height = (gdouble) cairo_image_surface_get_height (priv->srf);

	cairo_save (priv->cr);

#if LIBRSVG_CHECK_VERSION(2, 52, 0)
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = srf_width;
	viewport.height = srf_height;

	ret = rsvg_handle_render_document (handle, priv->cr, &viewport, error);
	if (!ret) {
		cairo_restore (priv->cr);
		g_prefix_error (error, "SVG graphic rendering failed:");
		goto out;
	}
#else
	rsvg_handle_get_dimensions (handle, &dims);

	/* cairo_translate (cr, (srf_width - dims.width) / 2, (srf_height - dims.height) / 2); */
	cairo_scale (priv->cr, srf_width / dims.width, srf_height / dims.height);

	ret = rsvg_handle_render_cairo (handle, priv->cr);
	if (!ret) {
		cairo_restore (priv->cr);
		g_set_error_literal (error,
				     ASC_CANVAS_ERROR,
				     ASC_CANVAS_ERROR_DRAWING,
				     "SVG graphic rendering failed.");
		goto out;
	}
#endif

	ret = TRUE;
out:
	if (handle != NULL)
		g_object_unref (handle);
	return ret;
#else
	g_warning ("Unable to render SVG graphic: AppStream built without SVG support.");
	g_set_error_literal (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_UNSUPPORTED,
			     "AppStream was built without SVG support. This is an issue with your "
			     "AppStream distribution. "
			     "Please rebuild AppStream with SVG support enabled or contact your "
			     "distributor to enable it for you.");
	return FALSE;
#endif
}

/**
 * asc_canvas_draw_text_line:
 * @canvas: an #AscCanvas instance.
 * @font: an #AscFont to use for drawing the text.
 * @text: the text to draw.
 * @border_width: Border with around the text, set to -1 to use defaults.
 * @vertical_offset: Additional vertical offset for text positioning (positive moves down).
 * @error: A #GError or %NULL
 *
 * Draw a simple line of text without linebreaks to fill the canvas.
 **/
gboolean
asc_canvas_draw_text_line (AscCanvas *canvas,
			   AscFont *font,
			   const gchar *text,
			   gint border_width,
			   gint vertical_offset,
			   GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	cairo_font_face_t *cff = NULL;
	cairo_status_t status;
	cairo_text_extents_t te;
	cairo_font_extents_t fe;
	gint text_size;
	double x_origin;
	double y_baseline;
	gboolean ret = FALSE;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);

	/* set default value */
	if (border_width < 0)
		border_width = 4;

	if (text == NULL) {
		g_set_error_literal (error,
				     ASC_CANVAS_ERROR,
				     ASC_CANVAS_ERROR_FAILED,
				     "Can not draw NULL string.");
		return FALSE;
	}

	cff = cairo_ft_font_face_create_for_ft_face (asc_font_get_ftface (font), FT_LOAD_DEFAULT);

	/* set font face for Cairo surface */
	status = cairo_font_face_status (cff);
	if (status != CAIRO_STATUS_SUCCESS) {
		g_set_error (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FONT,
			     "Could not set font face for Cairo: %i",
			     status);
		goto out;
	}
	cairo_set_font_face (priv->cr, cff);

	text_size = 160; /* start with a large font size */
	while (text_size-- > 0) {
		cairo_set_font_size (priv->cr, text_size);
		cairo_text_extents (priv->cr, text, &te);
		if (te.width <= 0.01f || te.height <= 0.01f)
			continue;
		if (te.width < priv->width - (border_width * 2) &&
		    te.height < priv->height - (border_width * 2))
			break;
	}

	/* draw text, centered optically */
	cairo_set_source_rgb (priv->cr, 0.0, 0.0, 0.0);

	cairo_text_extents (priv->cr, text, &te);
	cairo_font_extents (priv->cr, &fe);

	/* horizontal: center the whole advance width (looks better for e.g. "F") */
	x_origin = (priv->width / 2.0) - te.x_advance / 2.0;

	/* vertical: center the glyph bounding box around canvas center, then apply offset */
	y_baseline = (priv->height / 2.0) - (te.y_bearing + te.height / 2.0) + vertical_offset;

	cairo_move_to (priv->cr, x_origin, y_baseline);
	cairo_set_source_rgb (priv->cr, 0.0, 0.0, 0.0);
	cairo_show_text (priv->cr, text);

	cairo_save (priv->cr);
	ret = TRUE;

out:
	if (cff != NULL)
		cairo_font_face_destroy (cff);
	return ret;
}

/**
 * asc_find_font_size:
 *
 * Helper function to find the largest font size that fits
 * “text” inside max_width_px.
 */
static gint
asc_find_font_size (cairo_t *cr,
		    cairo_font_face_t *face,
		    const gchar *text,
		    gint max_width_px,
		    gint max_size_px)
{
	cairo_text_extents_t te;
	cairo_set_font_face (cr, face);

	for (gint sz = max_size_px; sz > 4; --sz) {
		cairo_set_font_size (cr, sz);
		cairo_text_extents (cr, text, &te);
		if (te.width < max_width_px)
			return sz;
	}

	return 4; /* fallback */
}

/**
 * asc_canvas_draw_font_card:
 * @canvas:       the #AscCanvas to paint on
 * @font:         the #AscFont to showcase
 * @info_label:   short info label for the current font
 * @pangram:      pangram text (or %NULL for default)
 * @bg_letter:    letter to use as background (e.g. “a”)
 * @border_width: outer margin in px (or -1 for default)
 * @error:        A #GError or %NULL
 *
 * Draws a font specimen card to showcase the selected font.
 * Running this function may change the canvas height!
 *
 * Returns: %TRUE on success.
 */
gboolean
asc_canvas_draw_font_card (AscCanvas *canvas,
			   AscFont *font,
			   const gchar *info_label,
			   const gchar *pangram,
			   const gchar *bg_letter,
			   gint border_width,
			   GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	cairo_status_t status;
	cairo_font_face_t *cff = NULL;
	cairo_font_extents_t fe, fe_name;
	cairo_text_extents_t te;
	g_autoptr(GPtrArray) pangram_lines = NULL;
	g_auto(GStrv) words = NULL;
	gint y; /* running pen position */
	gint inner_w;
	gint size_name, size_bg_letter, size_infolabel;
	gint size_sample, size_bar;
	gint line_height;
	gint name_height, bar_height;
	gint large_name_baseline;
	const gchar *word_sample = NULL;
	const gint bar_padding = 6;	  /* px of vertical padding in colored bottom bar */
	const gint bar_text_spacing = 18; /* gap between white/black sample words in bar */
	const gint post_pangram_space = 10;

	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);

	/* defaults */
	if (as_is_empty (bg_letter))
		bg_letter = "a";
	if (as_is_empty (pangram))
		pangram = "The quick brown fox jumps over the lazy dog";
	if (as_is_empty (info_label))
		info_label = asc_font_get_style (font);
	if (border_width < 0)
		border_width = 16;

	cff = cairo_ft_font_face_create_for_ft_face (asc_font_get_ftface (font), FT_LOAD_DEFAULT);
	status = cairo_font_face_status (cff);
	if (status != CAIRO_STATUS_SUCCESS) {
		g_set_error (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FONT,
			     "Could not create Cairo font face: %d",
			     status);
		goto fail;
	}

	/* calculate dimensions */
	inner_w = priv->width - 2 * border_width;
	y = border_width;

	/* 1) large font full name */
	size_name = asc_find_font_size (priv->cr,
					cff,
					asc_font_get_fullname (font),
					inner_w,
					priv->height * 0.20);

	cairo_set_font_face (priv->cr, cff);
	cairo_set_font_size (priv->cr, size_name);
	cairo_font_extents (priv->cr, &fe_name);
	name_height = fe_name.ascent + fe_name.descent;
	large_name_baseline = border_width + fe_name.ascent;
	y += name_height;

	/* 2) big translucent background latter (we’ll draw it later at this baseline) */
	size_bg_letter = MIN (priv->height * 0.58, inner_w);

	/* 3) info label text */
	if (!as_is_empty (info_label)) {
		size_infolabel = MAX (10, size_name * 0.40);
		cairo_set_font_size (priv->cr, size_infolabel);
		cairo_font_extents (priv->cr, &fe);
		y += fe.ascent + fe.descent;
	} else {
		size_infolabel = 0;
	}

	/* 4) pangram size & wrapping */
	size_sample = MAX (10, size_name * 0.35);
	cairo_set_font_size (priv->cr, size_sample);
	cairo_font_extents (priv->cr, &fe);
	line_height = fe.ascent + fe.descent;

	pangram_lines = g_ptr_array_new_with_free_func (g_free);
	words = g_strsplit (pangram, " ", -1);
	{
		g_autoptr(GString) current = g_string_new (NULL);
		for (guint w = 0; words[w] != NULL; ++w) {
			const gchar *word = words[w];
			cairo_text_extents (priv->cr, word, &te);

			/* pick a decently long word as sample for later */
			if (as_is_empty (word_sample) && strlen (word) >= 5)
				word_sample = word;

			/* break too-long word char-by-char */
			if (te.width > inner_w) {
				const gchar *p = word;
				while (*p) {
					const gchar *s = p;
					const gchar *e = p;
					g_autofree gchar *chunk = NULL;
					cairo_text_extents_t te2;
					while (*e) {
						const gchar *n = g_utf8_next_char (e);
						gchar *tmp = g_strndup (s, n - s);
						cairo_text_extents (priv->cr, tmp, &te2);
						g_free (tmp);
						if (te2.width > inner_w)
							break;
						e = n;
					}
					if (e == s)
						e = g_utf8_next_char (e);
					chunk = g_strndup (s, e - s);
					if (current->len) {
						g_ptr_array_add (pangram_lines,
								 g_strdup (current->str));
						g_string_truncate (current, 0);
					}
					g_ptr_array_add (pangram_lines, g_steal_pointer (&chunk));
					p = e;
				}
				continue;
			}

			/* append word to current line if it fits, else start new line */
			if (current->len) {
				GString *test = g_string_new (current->str);
				g_string_append_c (test, ' ');
				g_string_append (test, word);
				cairo_text_extents (priv->cr, test->str, &te);
				g_string_free (test, TRUE);

				if (te.width < inner_w) {
					/* word fits */
					g_string_append_c (current, ' ');
					g_string_append (current, word);
				} else {
					g_ptr_array_add (pangram_lines, g_strdup (current->str));
					g_string_assign (current, word);
				}
			} else {
				g_string_assign (current, word);
			}
		}

		if (current->len)
			g_ptr_array_add (pangram_lines, g_strdup (current->str));
	}

	y += pangram_lines->len * line_height;

	y += post_pangram_space;

	/* get our bottom bar sample word, in case none that was long enough
	 * had previously been found */
	if (as_is_empty (word_sample)) {
		if (words != NULL && words[0] != NULL)
			word_sample = words[0];
		else
			word_sample = asc_font_get_sample_icon_text (font);
	}

	/* 5) bottom colored bar (side-by-side names, small size) */
	size_bar = MAX (8, size_name * 0.40);
	cairo_set_font_size (priv->cr, size_bar);
	cairo_font_extents (priv->cr, &fe);
	bar_height = fe.ascent + fe.descent + 2 * bar_padding;
	y += bar_height;

	if (priv->height != y) {
		cairo_surface_t *old;
		cairo_t *newcr;

		old = priv->srf;
		priv->srf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, priv->width, y);
		newcr = cairo_create (priv->srf);
		cairo_destroy (priv->cr);
		priv->cr = newcr;
		cairo_surface_destroy (old);
		priv->height = y;
	}

	/* start drawing */
	y = border_width;
	cairo_set_source_rgb (priv->cr, 1, 1, 1);
	cairo_paint (priv->cr);

	/* large font name */
	cairo_set_font_face (priv->cr, cff);
	cairo_set_font_size (priv->cr, size_name);
	cairo_text_extents (priv->cr, asc_font_get_fullname (font), &te);
	cairo_move_to (priv->cr,
		       border_width + (inner_w - te.width) / 2.0 - te.x_bearing,
		       large_name_baseline);
	cairo_set_source_rgb (priv->cr, 0, 0, 0);
	cairo_show_text (priv->cr, asc_font_get_fullname (font));
	y += name_height;

	/* translucent big letter – baseline slightly below large name */
	cairo_set_font_size (priv->cr, size_bg_letter);
	cairo_font_extents (priv->cr, &fe);
	cairo_text_extents (priv->cr, bg_letter, &te);
	cairo_set_source_rgba (priv->cr, 0, 0, 0, 0.08);
	cairo_move_to (priv->cr,
		       priv->width - border_width - te.width - te.x_bearing,
		       large_name_baseline + (gint) (fe.ascent * 0.30));
	cairo_show_text (priv->cr, bg_letter);

	/* info label */
	if (size_infolabel) {
		cairo_set_font_size (priv->cr, size_infolabel);
		cairo_font_extents (priv->cr, &fe);
		cairo_text_extents (priv->cr, info_label, &te);
		cairo_set_source_rgb (priv->cr, 0.0, 0.46, 0.60);
		cairo_move_to (priv->cr,
			       border_width + (inner_w - te.width) / 2.0 - te.x_bearing,
			       y + fe.ascent);
		cairo_show_text (priv->cr, info_label);
		y += fe.ascent + fe.descent;
	}

	/* pangram */
	cairo_set_font_size (priv->cr, size_sample);
	cairo_font_extents (priv->cr, &fe);
	cairo_set_source_rgb (priv->cr, 0, 0, 0);

	for (guint i = 0; i < pangram_lines->len; ++i) {
		const gchar *line = g_ptr_array_index (pangram_lines, i);
		cairo_move_to (priv->cr, border_width, y + fe.ascent);
		cairo_show_text (priv->cr, line);
		y += line_height;
	}

	/* add a little bit of space */
	y += post_pangram_space;

	/* colored bar */
	cairo_set_source_rgb (priv->cr, 0.0, 0.46, 0.60);
	cairo_rectangle (priv->cr, 0, y, priv->width, bar_height);
	cairo_fill (priv->cr);

	/* side-by-side names inside bar (white + black) */
	cairo_set_font_size (priv->cr, size_bar);
	cairo_font_extents (priv->cr, &fe);
	cairo_text_extents (priv->cr, word_sample, &te);
	{
		gint combined_w, x0, base_bar;
		combined_w = (gint) (te.width * 2 + bar_text_spacing);
		x0 = border_width + (inner_w - combined_w) / 2.0 - te.x_bearing;
		base_bar = y + bar_padding + fe.ascent;

		cairo_set_source_rgb (priv->cr, 1, 1, 1);
		cairo_move_to (priv->cr, x0, base_bar);
		cairo_show_text (priv->cr, word_sample);

		cairo_set_source_rgb (priv->cr, 0, 0, 0);
		cairo_move_to (priv->cr, x0 + (gint) te.width + bar_text_spacing, base_bar);
		cairo_show_text (priv->cr, word_sample);
	}

	cairo_surface_flush (priv->srf);

	/* tidy up */
	cairo_font_face_destroy (cff);

	return TRUE;
fail:
	if (cff)
		cairo_font_face_destroy (cff);

	return FALSE;
}

/**
 * asc_canvas_draw_shape:
 * @canvas: An #AscCanvas instance.
 * @shape: An #AscCanvasShape to draw.
 * @border_width: Border width around the shape, set to -1 to use defaults.
 * @red: Red color value (0.0-1.0).
 * @green: Green color value (0.0-1.0).
 * @blue: Blue color value (0.0-1.0).
 * @error: A #GError or %NULL
 *
 * Draw a shape on the canvas.
 *
 * Returns: %TRUE on success.
 **/
gboolean
asc_canvas_draw_shape (AscCanvas *canvas,
		       AscCanvasShape shape,
		       gint border_width,
		       double red,
		       double green,
		       double blue,
		       GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);

	/* set default border value */
	if (border_width < 0)
		border_width = 4;

	if (shape == ASC_CANVAS_SHAPE_CIRCLE) {
		double radius = MIN (priv->width, priv->height) / 2.0 - border_width;
		cairo_set_source_rgb (priv->cr, red, green, blue);
		cairo_arc (priv->cr, priv->width / 2.0, priv->height / 2.0, radius, 0, 2 * G_PI);
		cairo_fill (priv->cr);
	} else if (shape == ASC_CANVAS_SHAPE_HEXAGON) {
		double radius = MIN (priv->width, priv->height) / 2.0 - border_width;
		double angle_step = G_PI / 3.0; /* 60 degrees in radians */
		cairo_set_source_rgb (priv->cr, red, green, blue);
		cairo_move_to (priv->cr,
			       priv->width / 2.0 + radius * cos (0),
			       priv->height / 2.0 + radius * sin (0));
		for (gint i = 1; i < 6; i++) {
			cairo_line_to (priv->cr,
				       priv->width / 2.0 + radius * cos (i * angle_step),
				       priv->height / 2.0 + radius * sin (i * angle_step));
		}
		cairo_close_path (priv->cr);
		cairo_fill (priv->cr);
	} else if (shape == ASC_CANVAS_SHAPE_CVL_TRIANGLE) {
		double radius = MIN (priv->width, priv->height) / 2.0 - border_width;
		double cx = priv->width / 2.0;
		double cy = priv->height / 2.0 +
			    radius * 0.15;	  /* Move center down for better visual balance */
		double angle_offset = -G_PI / 2;  /* Start from top */
		double angle_step = G_PI * 2 / 3; /* 120 degrees between vertices */
		double vertices[3][2];
		double curve_radius = radius * 0.15; /* Subtle curve for better shape */

		/* Create a curvilinear triangle using three circular arcs */
		/* Each arc connects two vertices of an equilateral triangle */

		cairo_set_source_rgb (priv->cr, red, green, blue);
		cairo_new_path (priv->cr);

		for (gint i = 0; i < 3; i++) {
			double angle = angle_step * i + angle_offset;
			vertices[i][0] = cx + radius * cos (angle);
			vertices[i][1] = cy + radius * sin (angle);
		}

		/* Draw three curved sides connecting the vertices */
		/* The curve radius determines how "bulged" the sides are */

		cairo_move_to (priv->cr, vertices[0][0], vertices[0][1]);

		for (gint i = 0; i < 3; i++) {
			gint next = (i + 1) % 3;

			/* Calculate midpoint between current and next vertex */
			double mid_x = (vertices[i][0] + vertices[next][0]) / 2.0;
			double mid_y = (vertices[i][1] + vertices[next][1]) / 2.0;

			/* Calculate direction from center to midpoint (outward direction) */
			double center_to_mid_x = mid_x - cx;
			double center_to_mid_y = mid_y - cy;
			double center_to_mid_length = sqrt (center_to_mid_x * center_to_mid_x +
							    center_to_mid_y * center_to_mid_y);

			/* Normalize the outward direction */
			double outward_x = center_to_mid_x / center_to_mid_length;
			double outward_y = center_to_mid_y / center_to_mid_length;

			/* Control point for the curve (outward from midpoint to create outward bulge) */
			double ctrl_x = mid_x + outward_x * curve_radius;
			double ctrl_y = mid_y + outward_y * curve_radius;

			/* Draw curved line to next vertex */
			cairo_curve_to (priv->cr,
					ctrl_x,
					ctrl_y,
					ctrl_x,
					ctrl_y,
					vertices[next][0],
					vertices[next][1]);
		}

		cairo_close_path (priv->cr);
		cairo_fill (priv->cr);
	} else {
		g_set_error_literal (error,
				     ASC_CANVAS_ERROR,
				     ASC_CANVAS_ERROR_UNSUPPORTED,
				     "Unsupported shape type.");
		return FALSE;
	}

	return TRUE;
}

/**
 * asc_calculate_text_border_width_for_icon_shape:
 * @bg_shape: The background shape.
 * @canvas_size: The size of the canvas (width or height, assuming square).
 * @shape_border_width: The border width used when drawing the shape.
 *
 * Calculate an appropriate border width for text placement inside a given shape.
 * This ensures that text does not overlap the edges of the shape, especially for
 * non-rectangular shapes like circles, hexagons, and triangles.
 *
 * Returns: The calculated border width for text placement.
 **/
gint
asc_calculate_text_border_width_for_icon_shape (AscCanvasShape bg_shape,
						gint canvas_size,
						gint shape_border_width)
{
	switch (bg_shape) {
	case ASC_CANVAS_SHAPE_CIRCLE:
		/* For circles, calculate inscribed square dimensions to ensure text fits */
		/* The inscribed square in a circle has side length = radius * sqrt(2) */
		{
			double radius = canvas_size / 2.0 - shape_border_width;
			double inscribed_square_side = radius * sqrt (2);

			return (gint) ((canvas_size - inscribed_square_side) / 2.0);
		}
	case ASC_CANVAS_SHAPE_HEXAGON:
		/* For hexagons, calculate the inscribed rectangle that fits within all sides */
		/* A regular hexagon with flat top/bottom has narrower width at middle */
		{
			double radius = canvas_size / 2.0 - shape_border_width;
			double hex_width = radius * sqrt (3);
			double hex_height = radius * 2.0;
			/* use conservative safe area that's 75% of the hexagon's inscribed rectangle */
			double safe_dimension = (hex_width < hex_height ? hex_width : hex_height) *
						0.75;

			return (gint) ((canvas_size - safe_dimension) / 2.0);
		}
	case ASC_CANVAS_SHAPE_CVL_TRIANGLE:
		/* For curvilinear triangles, calculate the inscribed circle area */
		/* The triangle is oriented with a vertex at the top */ {
			double radius = canvas_size / 2.0 - shape_border_width;
			double inscribed_circle_radius = radius / 2.0;
			double safe_diameter = inscribed_circle_radius * 2.0 *
					       1.1; /* 110% for better text utilization */

			return (gint) ((canvas_size - safe_diameter) / 2.0);
		}
	default:
		/* fallback */
		return shape_border_width;
	}
}

/**
 * asc_canvas_draw_text:
 * @canvas: an #AscCanvas instance.
 * @font: an #AscFont to use for drawing the text.
 * @border_width: Border with around the text (set to -1 to use defaults).
 * @line_pad: Padding between lines (set to -1 to use defaults).
 * @error: A #GError or %NULL
 *
 * Draw a longer text with linebreaks.
 **/
gboolean
asc_canvas_draw_text (AscCanvas *canvas,
		      AscFont *font,
		      const gchar *text,
		      gint border_width,
		      gint line_pad,
		      GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	gboolean ret = FALSE;
	cairo_font_face_t *cff = NULL;
	cairo_status_t status;
	cairo_text_extents_t te;
	g_auto(GStrv) lines = NULL;
	guint lines_len;
	guint line_padding;
	const gchar *longest_line;
	gint text_size;
	double x_pos, y_pos, te_height;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);

	/* set default values */
	if (border_width < 0)
		border_width = 4;
	if (line_pad < 0)
		line_pad = 2;

	if (text == NULL) {
		g_set_error_literal (error,
				     ASC_CANVAS_ERROR,
				     ASC_CANVAS_ERROR_FAILED,
				     "Can not draw NULL string.");
		return FALSE;
	}

	cff = cairo_ft_font_face_create_for_ft_face (asc_font_get_ftface (font), FT_LOAD_DEFAULT);
	/* set font face for Cairo surface */
	status = cairo_font_face_status (cff);
	if (status != CAIRO_STATUS_SUCCESS) {
		g_set_error (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FONT,
			     "Could not set font face for Cairo: %i",
			     status);
		goto out;
	}
	cairo_set_font_face (priv->cr, cff);

	/* calculate best font size */
	line_padding = line_pad;
	lines = g_strsplit (text, "\n", -1);
	lines_len = g_strv_length (lines);
	if (lines_len <= 1) {
		line_padding = 0;
		longest_line = text;
	} else {
		guint ll = 0;
		longest_line = lines[0];
		for (guint i = 0; lines[i] != NULL; i++) {
			guint l_len = strlen (lines[i]);
			if (l_len > ll) {
				longest_line = lines[i];
				ll = l_len;
			}
		}
	}

	text_size = 128;
	while (text_size-- > 0) {
		cairo_set_font_size (priv->cr, text_size);
		cairo_text_extents (priv->cr, longest_line, &te);
		if (te.width <= 0.01f || te.height <= 0.01f)
			continue;
		if (te.width < priv->width - (border_width * 2) &&
		    (te.height * lines_len + line_padding) < priv->height - (border_width * 2))
			break;
	}

	/* center text and draw it */
	x_pos = (priv->width / 2.0) - te.width / 2 - te.x_bearing;
	te_height = (double) te.height * lines_len + line_padding * ((double) lines_len - 1);
	y_pos = (te_height / 2) - te_height / 2 - te.y_bearing + border_width;
	cairo_move_to (priv->cr, x_pos, y_pos);
	cairo_set_source_rgb (priv->cr, 0.0, 0.0, 0.0);

	for (guint i = 0; lines[i] != NULL; i++) {
		cairo_show_text (priv->cr, lines[i]);
		y_pos += te.height + line_padding;
		cairo_move_to (priv->cr, x_pos, y_pos);
	}

	cairo_save (priv->cr);
	ret = TRUE;

out:
	if (cff != NULL)
		cairo_font_face_destroy (cff);
	return ret;
}

/**
 * asc_canvas_save_png:
 * @canvas: an #AscCanvas instance.
 * @fname: Filename to save to.
 * @error: A #GError or %NULL
 *
 * Save canvas to PNG file.
 **/
gboolean
asc_canvas_save_png (AscCanvas *canvas, const gchar *fname, GError **error)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	cairo_status_t status;

	status = cairo_surface_write_to_png (priv->srf, fname);
	if (status != CAIRO_STATUS_SUCCESS) {
		g_set_error (error,
			     ASC_CANVAS_ERROR,
			     ASC_CANVAS_ERROR_FONT,
			     "Could not save canvas to PNG: %s",
			     cairo_status_to_string (status));
		return FALSE;
	}

	return asc_optimize_png (fname, error);
}

static void
convert_alpha (guchar *dest_data,
	       int dest_stride,
	       guchar *src_data,
	       int src_stride,
	       int src_x,
	       int src_y,
	       int width,
	       int height)
{
	int x, y;

	src_data += src_stride * src_y + src_x * 4;

	for (y = 0; y < height; y++) {
		guint32 *src = (guint32 *) (void *) src_data;

		for (x = 0; x < width; x++) {
			guint alpha = src[x] >> 24;

			if (alpha == 0) {
				dest_data[x * 4 + 0] = 0;
				dest_data[x * 4 + 1] = 0;
				dest_data[x * 4 + 2] = 0;
			} else {
				dest_data[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 +
							alpha / 2) /
						       alpha;
				dest_data[x * 4 + 1] = (((src[x] & 0x00ff00) >> 8) * 255 +
							alpha / 2) /
						       alpha;
				dest_data[x * 4 + 2] = (((src[x] & 0x0000ff) >> 0) * 255 +
							alpha / 2) /
						       alpha;
			}
			dest_data[x * 4 + 3] = alpha;
		}

		src_data += src_stride;
		dest_data += dest_stride;
	}
}

static void
convert_no_alpha (guchar *dest_data,
		  int dest_stride,
		  guchar *src_data,
		  int src_stride,
		  int src_x,
		  int src_y,
		  int width,
		  int height)
{
	int x, y;

	src_data += src_stride * src_y + src_x * 4;

	for (y = 0; y < height; y++) {
		guint32 *src = (guint32 *) (void *) src_data;

		for (x = 0; x < width; x++) {
			dest_data[x * 3 + 0] = src[x] >> 16;
			dest_data[x * 3 + 1] = src[x] >> 8;
			dest_data[x * 3 + 2] = src[x];
		}

		src_data += src_stride;
		dest_data += dest_stride;
	}
}

/**
 * asc_canvas_to_pixbuf:
 * @canvas: an #AscCanvas instance.
 *
 * Convert the canvas to a #GdkPixbuf.
 *
 * Returns: (transfer full) (nullable): a #GdkPixbuf or %NULL on error.
 **/
GdkPixbuf *
asc_canvas_to_pixbuf (AscCanvas *canvas)
{
	AscCanvasPrivate *priv = GET_PRIVATE (canvas);
	cairo_content_t content;
	GdkPixbuf *dest;

	/* sanity check */
	g_return_val_if_fail (priv->width > 0 && priv->height > 0, NULL);

	content = cairo_surface_get_content (priv->srf) | CAIRO_CONTENT_COLOR;
	dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			       !!(content & CAIRO_CONTENT_ALPHA),
			       8,
			       priv->width,
			       priv->height);

	cairo_surface_flush (priv->srf);
	if (cairo_surface_status (priv->srf) || dest == NULL) {
		cairo_surface_destroy (priv->srf);
		g_clear_object (&dest);
		return NULL;
	}

	if (gdk_pixbuf_get_has_alpha (dest))
		convert_alpha (gdk_pixbuf_get_pixels (dest),
			       gdk_pixbuf_get_rowstride (dest),
			       cairo_image_surface_get_data (priv->srf),
			       cairo_image_surface_get_stride (priv->srf),
			       0,
			       0,
			       priv->width,
			       priv->height);
	else
		convert_no_alpha (gdk_pixbuf_get_pixels (dest),
				  gdk_pixbuf_get_rowstride (dest),
				  cairo_image_surface_get_data (priv->srf),
				  cairo_image_surface_get_stride (priv->srf),
				  0,
				  0,
				  priv->width,
				  priv->height);

	return dest;
}
