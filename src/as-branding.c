/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-branding
 * @short_description: Description of branding for an #AsComponent.
 * @include: appstream.h
 *
 * This class provides information contained in an AppStream branding tag.
 * See https://www.freedesktop.org/software/appstream/docs/chap-Metadata.html#tag-branding
 * for more information.
 *
 * See also: #AsComponent
 */

#include "config.h"
#include "as-branding-private.h"

typedef struct
{
	GPtrArray	*colors; /* of AsBrandingColor */
} AsBrandingPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsBranding, as_branding, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_branding_get_instance_private (o))

/**
 * AsBrandingColorIter:
 *
 * A #AsBrandingColorIter structure represents an iterator that can be used
 * to iterate over the accent colors of an #AsBranding object.
 * #AsBrandingColorIter structures are typically allocated on the stack and
 * then initialized with as_branding_color_iter_init().
 */

typedef struct
{
	AsBranding 	*branding;
	guint		pos;
	gpointer	dummy3;
	gpointer	dummy4;
	gpointer	dummy5;
	gpointer	dummy6;
} RealBrandingColorIter;

G_STATIC_ASSERT (sizeof (AsBrandingColorIter) == sizeof (RealBrandingColorIter));

typedef struct {
	AsColorKind		kind;
	AsColorSchemeKind	scheme_preference;
	gchar			*value;
} AsBrandingColor;

static AsBrandingColor*
as_branding_color_new (AsColorKind kind,
		       AsColorSchemeKind scheme_preference)
{
	AsBrandingColor *bcolor;
	bcolor = g_slice_new0 (AsBrandingColor);

	bcolor->kind = kind;
	bcolor->scheme_preference = scheme_preference;
	return bcolor;
}

static void
as_branding_color_free (AsBrandingColor *bcolor)
{
	g_free (bcolor->value);
	g_slice_free (AsBrandingColor, bcolor);
}

/**
 * as_color_kind_to_string:
 * @kind: the %AsColorKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.15.2
 **/
const gchar*
as_color_kind_to_string (AsColorKind kind)
{
	if (kind == AS_COLOR_KIND_PRIMARY)
		return "primary";
	return "unknown";
}

/**
 * as_color_kind_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsColorKind or %AS_COLOR_KIND_UNKNOWN for unknown.
 *
 * Since: 0.15.2
 **/
AsColorKind
as_color_kind_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "primary") == 0)
		return AS_COLOR_KIND_PRIMARY;
	return AS_COLOR_KIND_UNKNOWN;
}

/**
 * as_color_scheme_kind_to_string:
 * @kind: the %AsColorSchemeKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.15.2
 **/
const gchar*
as_color_scheme_kind_to_string (AsColorSchemeKind kind)
{
	if (kind == AS_COLOR_SCHEME_KIND_LIGHT)
		return "light";
	if (kind == AS_COLOR_SCHEME_KIND_DARK)
		return "dark";
	return NULL;
}

/**
 * as_color_scheme_kind_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsColorKind or %AS_COLOR_SCHEME_KIND_UNKNOWN for unknown.
 *
 * Since: 0.15.2
 **/
AsColorSchemeKind
as_color_scheme_kind_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "light") == 0)
		return AS_COLOR_SCHEME_KIND_LIGHT;
	if (g_strcmp0 (str, "dark") == 0)
		return AS_COLOR_SCHEME_KIND_DARK;
	return AS_COLOR_SCHEME_KIND_UNKNOWN;
}

static void
as_branding_init (AsBranding *branding)
{
	AsBrandingPrivate *priv = GET_PRIVATE (branding);

	priv->colors = g_ptr_array_new_with_free_func ((GDestroyNotify) as_branding_color_free);
}

static void
as_branding_finalize (GObject *object)
{
	AsBranding *branding = AS_BRANDING (object);
	AsBrandingPrivate *priv = GET_PRIVATE (branding);

	g_ptr_array_unref (priv->colors);

	G_OBJECT_CLASS (as_branding_parent_class)->finalize (object);
}

static void
as_branding_class_init (AsBrandingClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_branding_finalize;
}

/**
 * as_branding_set_color:
 * @branding: an #AsBranding instance.
 * @kind: the #AsColorKind, e.g. %AS_COLOR_KIND_PRIMARY.
 * @scheme_preference: Type of color scheme preferred for this color, e.g. %AS_COLOR_SCHEME_KIND_LIGHT
 * @colorcode: a HTML color code.
 *
 * Sets a new accent color. If a color of the given kind with the given scheme preference already exists,
 * it will be overriden with the new color code.
 *
 * Since: 0.15.2
 **/
void
as_branding_set_color (AsBranding *branding,
		       AsColorKind kind,
		       AsColorSchemeKind scheme_preference,
		       const gchar *colorcode)
{
	AsBrandingPrivate *priv = GET_PRIVATE (branding);
	AsBrandingColor *color;

	for (guint i = 0; i < priv->colors->len; i++) {
		color = g_ptr_array_index (priv->colors, i);
		if (color->kind == kind && color->scheme_preference == scheme_preference) {
			g_free (color->value);
			color->value = g_strdup (colorcode);
			return;
		}
	}

	/* if we are here, the color didn't exist yet */
	color = as_branding_color_new (kind, scheme_preference);
	color->value = g_strdup (colorcode);
	g_ptr_array_add (priv->colors, color);
}

/**
 * as_branding_remove_color:
 * @branding: an #AsBranding instance.
 * @kind: the #AsColorKind, e.g. %AS_COLOR_KIND_PRIMARY.
 * @scheme_preference: Type of color scheme preferred for this color, e.g. %AS_COLOR_SCHEME_KIND_LIGHT
 *
 * Deletes a color that matches the given type and scheme preference.
 *
 * Since: 0.15.2
 **/
void
as_branding_remove_color (AsBranding *branding,
			  AsColorKind kind,
			  AsColorSchemeKind scheme_preference)
{
	AsBrandingPrivate *priv = GET_PRIVATE (branding);

	for (guint i = 0; i < priv->colors->len; i++) {
		AsBrandingColor *color = g_ptr_array_index (priv->colors, i);
		if (color->kind == kind && color->scheme_preference == scheme_preference) {
			g_ptr_array_remove_index_fast (priv->colors, i);
			return;
		}
	}
}


/**
 * as_branding_color_iter_init:
 * @iter: an uninitialized #AsBrandingColorIter
 * @branding: an #AsBranding
 *
 * Initializes a color iterator for the accent color list and associates it
 * it with @branding.
 * The #AsBrandingColorIter structure is typically allocated on the stack
 * and does not need to be freed explicitly.
 */
void
as_branding_color_iter_init (AsBrandingColorIter *iter, AsBranding *branding)
{
	RealBrandingColorIter *ri = (RealBrandingColorIter *) iter;

	g_return_if_fail (iter != NULL);
	g_return_if_fail (AS_IS_BRANDING (branding));

	ri->branding = branding;
	ri->pos = 0;
}

/**
 * as_branding_color_iter_next:
 * @iter: an initialized #AsBrandingColorIter
 * @kind: (out) (optional) (not nullable): Destination of the returned color kind.
 * @scheme_preference: (out) (optional) (not nullable): Destination of the returned color's scheme preference.
 * @value: (out) (optional) (not nullable): Destination of the returned color code.
 *
 * Returns the current color entry and advances the iterator.
 * Example:
 * |[<!-- language="C" -->
 * AsBrandingColorIter iter;
 * AsColorKind ckind;
 * AsColorSchemeKind scheme_preference;
 * const gchar *color_value;
 *
 * as_branding_color_iter_init (&iter, branding);
 * while (as_branding_color_iter_next (&iter, &ckind, &scheme_preference, &color_value)) {
 *     // do something with the color data
 * }
 * ]|
 *
 * Returns: %FALSE if the last entry has been reached.
 */
gboolean
as_branding_color_iter_next (AsBrandingColorIter *iter,
			     AsColorKind *kind,
			     AsColorSchemeKind *scheme_preference,
			     const gchar **value)
{
	AsBrandingPrivate *priv;
	AsBrandingColor *color = NULL;
	RealBrandingColorIter *ri = (RealBrandingColorIter *) iter;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (kind != NULL, FALSE);
	g_return_val_if_fail (scheme_preference != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	priv = GET_PRIVATE (ri->branding);

	/* check if the iteration was finished */
	if (ri->pos >= priv->colors->len) {
		*value = NULL;
		return FALSE;
	}

	color = g_ptr_array_index (priv->colors, ri->pos);
	ri->pos++;
	*kind = color->kind;
	*scheme_preference = color->scheme_preference;
	*value = color->value;

	return TRUE;
}

/**
 * as_branding_get_color:
 * @branding: an #AsBranding instance.
 * @kind: the #AsColorKind, e.g. %AS_COLOR_KIND_PRIMARY.
 * @scheme_kind: Color scheme preference for the color, e.g. %AS_COLOR_SCHEME_KIND_LIGHT
 *
 * Retrieve a color of the given @kind that matches @scheme_kind.
 * If a color has no scheme preference defined, it will be returned for either scheme type,
 * unless a more suitable color was found.
 *
 * Returns: (nullable): The HTML color code of the found color, or %NULL if no color was found.
 *
 * Since: 0.15.2
 **/
const gchar*
as_branding_get_color (AsBranding *branding,
			AsColorKind kind,
			AsColorSchemeKind scheme_kind)
{
	AsBrandingPrivate *priv = GET_PRIVATE (branding);
	AsBrandingColor *generic_color = NULL;

	for (guint i = 0; i < priv->colors->len; i++) {
		AsBrandingColor *color = g_ptr_array_index (priv->colors, i);
		if (color->kind == kind) {
			if (color->scheme_preference == scheme_kind)
				return color->value;
			else if (color->scheme_preference == AS_COLOR_SCHEME_KIND_UNKNOWN)
				generic_color = color;
		}
	}

	if (generic_color != NULL)
		return generic_color->value;
	return NULL;
}

/**
 * as_branding_load_from_xml:
 * @branding: a #AsBranding instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads data from an XML node.
 **/
gboolean
as_branding_load_from_xml (AsBranding *branding, AsContext *ctx, xmlNode *node, GError **error)
{
	AsBrandingPrivate *priv = GET_PRIVATE (branding);

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "color") == 0) {
			AsBrandingColor *color = NULL;
			AsColorKind ckind;
			AsColorSchemeKind scheme_pref;
			gchar *tmp;

			tmp = as_xml_get_prop_value (iter, "type");
			ckind = as_color_kind_from_string (tmp);
			g_free (tmp);

			tmp = as_xml_get_prop_value (iter, "scheme_preference");
			scheme_pref = as_color_scheme_kind_from_string (tmp);
			g_free (tmp);

			color = as_branding_color_new (ckind, scheme_pref);
			color->value = as_xml_get_node_value (iter);
			g_ptr_array_add (priv->colors, color);
		}
	}

	return TRUE;
}

/**
 * as_branding_to_xml_node:
 * @branding: a #AsBranding instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_branding_to_xml_node (AsBranding *branding, AsContext *ctx, xmlNode *root)
{
	AsBrandingPrivate *priv = GET_PRIVATE (branding);
	xmlNode *branding_n;

	branding_n = xmlNewChild (root, NULL, (xmlChar*) "branding", (xmlChar*) "");

	for (guint i = 0; i < priv->colors->len; i++) {
		xmlNode *n;
		AsBrandingColor *color = g_ptr_array_index (priv->colors, i);
		if (color->kind == AS_COLOR_KIND_UNKNOWN || color->value == NULL)
			continue;

		n = as_xml_add_text_node (branding_n,
					  "color",
					  color->value);
		as_xml_add_text_prop (n, "type",
				      as_color_kind_to_string (color->kind));
		if (color->scheme_preference != AS_COLOR_SCHEME_KIND_UNKNOWN)
			as_xml_add_text_prop (n, "scheme_preference",
					      as_color_scheme_kind_to_string (color->scheme_preference));

	}
}

static void
as_branding_load_color_from_yaml (AsBranding *branding, GNode *node, AsBrandingColor *color)
{
	for (GNode *color_n = node->children; color_n != NULL; color_n = color_n->next) {
		const gchar *key = as_yaml_node_get_key (color_n);
		const gchar *value = as_yaml_node_get_value (color_n);

		if (g_strcmp0 (key, "type") == 0)
			color->kind = as_color_kind_from_string (value);
		else if (g_strcmp0 (key, "scheme-preference") == 0)
			color->scheme_preference = as_color_scheme_kind_from_string (value);
		else if (g_strcmp0 (key, "value") == 0)
			color->value = g_strdup (value);
	}
}

/**
 * as_branding_load_from_yaml:
 * @branding: a #AsBranding instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_branding_load_from_yaml (AsBranding *branding, AsContext *ctx, GNode *node, GError **error)
{
	AsBrandingPrivate *priv = GET_PRIVATE (branding);

	for (GNode *n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);

		if (g_strcmp0 (key, "colors") == 0) {
			for (GNode *sn = n->children; sn != NULL; sn = sn->next) {
				AsBrandingColor *color = as_branding_color_new (AS_COLOR_KIND_UNKNOWN, AS_COLOR_SCHEME_KIND_UNKNOWN);
				as_branding_load_color_from_yaml (branding, sn, color);
				if (color->kind == AS_COLOR_KIND_UNKNOWN) {
					as_branding_color_free (color);
					continue;
				}
				g_ptr_array_add (priv->colors, color);
			}

		} else {
			as_yaml_print_unknown ("branding", key);
		}
	}

	return TRUE;
}

/**
 * as_branding_emit_yaml:
 * @branding: a #AsBranding instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_branding_emit_yaml (AsBranding *branding, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsBrandingPrivate *priv = GET_PRIVATE (branding);

	if (priv->colors->len == 0)
		return;

	/* start mapping for this branding */
	as_yaml_emit_scalar (emitter, "Branding");
	as_yaml_mapping_start (emitter);

	as_yaml_emit_scalar (emitter, "colors");
	as_yaml_sequence_start (emitter);

	for (guint i = 0; i < priv->colors->len; i++) {
		AsBrandingColor *color = g_ptr_array_index (priv->colors, i);
		as_yaml_mapping_start (emitter);

		as_yaml_emit_entry (emitter,
				   "type",
				    as_color_kind_to_string (color->kind));
		if (color->scheme_preference != AS_COLOR_SCHEME_KIND_UNKNOWN)
			as_yaml_emit_entry (emitter,
					    "scheme-preference",
					    as_color_scheme_kind_to_string (color->scheme_preference));

		as_yaml_emit_entry (emitter,
				   "value",
				    color->value);

		as_yaml_mapping_end (emitter);
	}

	as_yaml_sequence_end (emitter);

	/* end mapping for the branding */
	as_yaml_mapping_end (emitter);
}

/**
 * as_branding_new:
 *
 * Creates a new #AsBranding.
 *
 * Returns: (transfer full): a #AsBranding
 *
 * Since: 0.10
 **/
AsBranding*
as_branding_new (void)
{
	AsBranding *branding;
	branding = g_object_new (AS_TYPE_BRANDING, NULL);
	return AS_BRANDING (branding);
}
