/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-component.h"
#include "as-component-private.h"

#include <glib.h>
#include <glib-object.h>

#include "as-utils.h"
#include "as-utils-private.h"

/**
 * SECTION:as-component
 * @short_description: Object representing a software component
 * @include: appstream.h
 *
 * This object represents an Appstream software component which is associated
 * to a package in the distribution's repositories.
 * A component can be anything, ranging from an application to a font, a codec or
 * even a non-visual software project providing libraries and python-modules for
 * other applications to use.
 *
 * The type of the component is stored as #AsComponentKind and can be queried to
 * find out which kind of component we're dealing with.
 *
 * See also: #AsProvidesKind, #AsDatabase
 */

typedef struct
{
	AsComponentKind kind;
	gchar			*active_locale;

	gchar			*id;
	gchar			*origin;
	gchar			**pkgnames;
	gchar			*source_pkgname;

	GHashTable		*name; /* localized entry */
	GHashTable		*summary; /* localized entry */
	GHashTable		*description; /* localized entry */
	GHashTable		*keywords; /* localized entry, value:strv */
	GHashTable		*developer_name; /* localized entry */

	gchar			**categories;
	gchar			*project_license;
	gchar			*project_group;
	gchar			**compulsory_for_desktops;

	GPtrArray		*extends; /* of string */
	GPtrArray		*extensions; /* of string */
	GPtrArray		*screenshots; /* of AsScreenshot elements */
	GPtrArray		*releases; /* of AsRelease elements */

	GHashTable		*provided; /* of int:object */
	GHashTable		*urls; /* of int:utf8 */
	GHashTable		*languages; /* of utf8:utf8 */
	GHashTable		*bundles; /* of int:utf8 */

	GPtrArray		*icons; /* of AsIcon elements */
	GHashTable		*icons_sizetab; /* of utf8:object (object owned by priv->icons array) */

	gint			priority; /* used internally */
} AsComponentPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsComponent, as_component, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_component_get_instance_private (o))

enum  {
	AS_COMPONENT_DUMMY_PROPERTY,
	AS_COMPONENT_KIND,
	AS_COMPONENT_PKGNAMES,
	AS_COMPONENT_ID,
	AS_COMPONENT_NAME,
	AS_COMPONENT_SUMMARY,
	AS_COMPONENT_DESCRIPTION,
	AS_COMPONENT_KEYWORDS,
	AS_COMPONENT_ICONS,
	AS_COMPONENT_URLS,
	AS_COMPONENT_CATEGORIES,
	AS_COMPONENT_PROJECT_LICENSE,
	AS_COMPONENT_PROJECT_GROUP,
	AS_COMPONENT_DEVELOPER_NAME,
	AS_COMPONENT_SCREENSHOTS
};

/**
 * as_component_kind_get_type:
 *
 * Defines registered component types.
 */
GType
as_component_kind_get_type (void)
{
	static volatile gsize as_component_kind_type_id__volatile = 0;
	if (g_once_init_enter (&as_component_kind_type_id__volatile)) {
		static const GEnumValue values[] = {
					{AS_COMPONENT_KIND_UNKNOWN, "AS_COMPONENT_KIND_UNKNOWN", "unknown"},
					{AS_COMPONENT_KIND_GENERIC, "AS_COMPONENT_KIND_GENERIC", "generic"},
					{AS_COMPONENT_KIND_DESKTOP_APP, "AS_COMPONENT_KIND_DESKTOP_APP", "desktop"},
					{AS_COMPONENT_KIND_FONT, "AS_COMPONENT_KIND_FONT", "font"},
					{AS_COMPONENT_KIND_CODEC, "AS_COMPONENT_KIND_CODEC", "codec"},
					{AS_COMPONENT_KIND_INPUTMETHOD, "AS_COMPONENT_KIND_INPUTMETHOD", "inputmethod"},
					{AS_COMPONENT_KIND_ADDON, "AS_COMPONENT_KIND_ADDON", "addon"},
					{AS_COMPONENT_KIND_FIRMWARE, "AS_COMPONENT_KIND_FIRMWARE", "firmware"},
					{AS_COMPONENT_KIND_LAST, "AS_COMPONENT_KIND_LAST", "last"},
					{0, NULL, NULL}
		};
		GType as_component_type_type_id;
		as_component_type_type_id = g_enum_register_static ("AsComponentKind", values);
		g_once_init_leave (&as_component_kind_type_id__volatile, as_component_type_type_id);
	}
	return as_component_kind_type_id__volatile;
}

/**
 * as_component_kind_to_string:
 * @kind: the %AsComponentKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar *
as_component_kind_to_string (AsComponentKind kind)
{
	if (kind == AS_COMPONENT_KIND_GENERIC)
		return "generic";
	if (kind == AS_COMPONENT_KIND_DESKTOP_APP)
		return "desktop";
	if (kind == AS_COMPONENT_KIND_FONT)
		return "font";
	if (kind == AS_COMPONENT_KIND_CODEC)
		return "codec";
	if (kind == AS_COMPONENT_KIND_INPUTMETHOD)
		return "inputmethod";
	if (kind == AS_COMPONENT_KIND_ADDON)
		return "addon";
	if (kind == AS_COMPONENT_KIND_FIRMWARE)
		return "firmware";
	return "unknown";
}

/**
 * as_component_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsComponentKind or %AS_COMPONENT_KIND_UNKNOWN for unknown
 **/
AsComponentKind
as_component_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "generic") == 0)
		return AS_COMPONENT_KIND_GENERIC;
	if (g_strcmp0 (kind_str, "desktop") == 0)
		return AS_COMPONENT_KIND_DESKTOP_APP;
	if (g_strcmp0 (kind_str, "font") == 0)
		return AS_COMPONENT_KIND_FONT;
	if (g_strcmp0 (kind_str, "codec") == 0)
		return AS_COMPONENT_KIND_CODEC;
	if (g_strcmp0 (kind_str, "inputmethod") == 0)
		return AS_COMPONENT_KIND_INPUTMETHOD;
	if (g_strcmp0 (kind_str, "addon") == 0)
		return AS_COMPONENT_KIND_ADDON;
	if (g_strcmp0 (kind_str, "firmware") == 0)
		return AS_COMPONENT_KIND_FIRMWARE;
	return AS_COMPONENT_KIND_UNKNOWN;
}

/**
 * as_component_init:
 **/
static void
as_component_init (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* our default locale is "unlocalized" */
	priv->active_locale = g_strdup ("C");

	/* translatable entities */
	priv->name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->summary = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->description = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->developer_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->keywords = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_strfreev);

	priv->screenshots = g_ptr_array_new_with_free_func (g_object_unref);
	priv->releases = g_ptr_array_new_with_free_func (g_object_unref);

	priv->icons = g_ptr_array_new_with_free_func (g_object_unref);
	priv->icons_sizetab = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	priv->provided = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
	priv->urls = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	priv->bundles = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	priv->languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	as_component_set_priority (cpt, 0);
}

/**
 * as_component_finalize:
 */
static void
as_component_finalize (GObject* object)
{
	AsComponent *cpt = AS_COMPONENT (object);
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_free (priv->id);
	g_strfreev (priv->pkgnames);
	g_free (priv->project_license);
	g_free (priv->project_group);
	g_free (priv->active_locale);

	g_hash_table_unref (priv->name);
	g_hash_table_unref (priv->summary);
	g_hash_table_unref (priv->description);
	g_hash_table_unref (priv->developer_name);
	g_hash_table_unref (priv->keywords);

	g_strfreev (priv->categories);
	g_strfreev (priv->compulsory_for_desktops);

	g_ptr_array_unref (priv->screenshots);
	g_ptr_array_unref (priv->releases);
	g_hash_table_unref (priv->provided);
	g_hash_table_unref (priv->urls);
	g_hash_table_unref (priv->languages);
	g_hash_table_unref (priv->bundles);

	if (priv->extends != NULL)
		g_ptr_array_unref (priv->extends);
	if (priv->extensions != NULL)
		g_ptr_array_unref (priv->extensions);

	g_ptr_array_unref (priv->icons);
	g_hash_table_unref (priv->icons_sizetab);

	G_OBJECT_CLASS (as_component_parent_class)->finalize (object);
}

/**
 * as_component_is_valid:
 * @cpt: a #AsComponent instance.
 *
 * Check if the essential properties of this Component are
 * populated with useful data.
 *
 * Returns: TRUE if the component data was validated successfully.
 */
gboolean
as_component_is_valid (AsComponent *cpt)
{
	gboolean ret = FALSE;
	const gchar *cname;
	AsComponentKind ctype;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	ctype = priv->kind;
	if (ctype == AS_COMPONENT_KIND_UNKNOWN)
		return FALSE;
	cname = as_component_get_name (cpt);

	if ((as_component_has_install_candidate (cpt)) &&
		(g_strcmp0 (priv->id, "") != 0) &&
		(cname != NULL) &&
		(g_strcmp0 (cname, "") != 0)) {
			ret = TRUE;
	}

#if 0
	if ((ret) && ctype == AS_COMPONENT_KIND_DESKTOP_APP) {
		ret = g_strcmp0 (priv->desktop_file, "") != 0;
	}
#endif

	return ret;
}

/**
 * as_component_to_string:
 * @cpt: a #AsComponent instance.
 *
 * Returns a string identifying this component.
 * (useful for debugging)
 *
 * Returns: (transfer full): A descriptive string
 **/
gchar*
as_component_to_string (AsComponent *cpt)
{
	gchar* res = NULL;
	const gchar *name;
	const gchar *summary;
	gchar *pkgs;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (as_component_has_package (cpt))
		pkgs = g_strjoinv (",", priv->pkgnames);
	else
		pkgs = g_strdup ("?");

	name = as_component_get_name (cpt);
	summary = as_component_get_summary (cpt);

	switch (priv->kind) {
		case AS_COMPONENT_KIND_DESKTOP_APP:
		{
			res = g_strdup_printf ("[DesktopApp::%s]> name: %s | package: %s | summary: %s", priv->id, name, pkgs, summary);
			break;
		}
		case AS_COMPONENT_KIND_UNKNOWN:
		{
			res = g_strdup_printf ("[UNKNOWN::%s]> name: %s | package: %s | summary: %s", priv->id, name, pkgs, summary);
			break;
		}
		default:
		{
			res = g_strdup_printf ("[Component::%s]> name: %s | package: %s | summary: %s", priv->id, name, pkgs, summary);
			break;
		}
	}

	g_free (pkgs);
	return res;
}

/**
 * as_component_add_screenshot:
 * @cpt: a #AsComponent instance.
 * @sshot: The #AsScreenshot to add
 *
 * Add an #AsScreenshot to this component.
 **/
void
as_component_add_screenshot (AsComponent *cpt, AsScreenshot* sshot)
{
	GPtrArray* sslist;

	sslist = as_component_get_screenshots (cpt);
	g_ptr_array_add (sslist, g_object_ref (sshot));
}

/**
 * as_component_add_release:
 * @cpt: a #AsComponent instance.
 * @release: The #AsRelease to add
 *
 * Add an #AsRelease to this component.
 **/
void
as_component_add_release (AsComponent *cpt, AsRelease* release)
{
	GPtrArray* releases;

	releases = as_component_get_releases (cpt);
	g_ptr_array_add (releases, g_object_ref (release));
}

/**
 * as_component_get_urls_table:
 * @cpt: a #AsComponent instance.
 *
 * Gets the URLs set for the component.
 *
 * Returns: (transfer none) (element-type AsUrlKind utf8): URLs
 *
 * Since: 0.6.2
 **/
GHashTable*
as_component_get_urls_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->urls;
}

/**
 * as_component_get_url:
 * @cpt: a #AsComponent instance.
 * @url_kind: the URL kind, e.g. %AS_URL_KIND_HOMEPAGE.
 *
 * Gets a URL.
 *
 * Returns: (nullable): string, or %NULL if unset
 *
 * Since: 0.6.2
 **/
const gchar*
as_component_get_url (AsComponent *cpt, AsUrlKind url_kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return g_hash_table_lookup (priv->urls,
				    GINT_TO_POINTER (url_kind));
}

/**
 * as_component_add_url:
 * @cpt: a #AsComponent instance.
 * @url_kind: the URL kind, e.g. %AS_URL_KIND_HOMEPAGE
 * @url: the full URL.
 *
 * Adds some URL data to the component.
 *
 * Since: 0.6.2
 **/
void
as_component_add_url (AsComponent *cpt, AsUrlKind url_kind, const gchar *url)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_hash_table_insert (priv->urls,
			     GINT_TO_POINTER (url_kind),
			     g_strdup (url));
}

/**
  * as_component_get_extends:
  * @cpt: an #AsComponent instance.
  *
  * Returns a string list of IDs of components which
  * are extended by this addon.
  *
  * See %as_component_get_extends() for the reverse.
  *
  * Returns: (element-type utf8) (transfer none): A #GPtrArray or %NULL if not set.
  *
  * Since: 0.7.0
**/
GPtrArray*
as_component_get_extends (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->extends == NULL)
		priv->extends = g_ptr_array_new_with_free_func (g_free);
	return priv->extends;
}

/**
 * as_component_add_extends:
 * @cpt: a #AsComponent instance.
 * @cpt_id: The id of a component which is extended by this component
 *
 * Add a reference to the extended component
 *
 * Since: 0.7.0
 **/
void
as_component_add_extends (AsComponent* cpt, const gchar* cpt_id)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->extends == NULL)
		priv->extends = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (priv->extends, g_strdup (cpt_id));
}

/**
  * as_component_get_extensions:
  * @cpt: an #AsComponent instance.
  *
  * Returns a string list of IDs of components which
  * are addons extending this component in functionality.
  *
  * This is the reverse of %as_component_get_extends()
  *
  * Returns: (element-type utf8) (transfer none): A #GPtrArray or %NULL if not set.
  *
  * Since: 0.9.2
**/
GPtrArray*
as_component_get_extensions (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->extensions == NULL)
		priv->extensions = g_ptr_array_new_with_free_func (g_free);
	return priv->extensions;
}

/**
 * as_component_add_extension:
 * @cpt: a #AsComponent instance.
 * @cpt_id: The id of a component extending this component.
 *
 * Add a reference to the extension enhancing this component.
 *
 * Since: 0.9.2
 **/
void
as_component_add_extension (AsComponent* cpt, const gchar* cpt_id)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->extensions == NULL)
		priv->extensions = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (priv->extensions, g_strdup (cpt_id));
}

/**
 * as_component_get_bundles_table:
 * @cpt: a #AsComponent instance.
 *
 * Gets the bundle-ids set for the component.
 *
 * Returns: (transfer none) (element-type AsBundleKind utf8): Bundle ids
 *
 * Since: 0.8.0
 **/
GHashTable*
as_component_get_bundles_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->bundles;
}

/**
 * as_component_get_bundle_id:
 * @cpt: a #AsComponent instance.
 * @bundle_kind: the bundle kind, e.g. %AS_BUNDLE_KIND_LIMBA.
 *
 * Gets a bundle identifier string.
 *
 * Returns: (nullable): string, or %NULL if unset
 *
 * Since: 0.8.0
 **/
const gchar*
as_component_get_bundle_id (AsComponent *cpt, AsBundleKind bundle_kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return g_hash_table_lookup (priv->bundles,
				    GINT_TO_POINTER (bundle_kind));
}

/**
 * as_component_add_bundle_id:
 * @cpt: a #AsComponent instance.
 * @bundle_kind: the URL kind, e.g. %AS_BUNDLE_KIND_LIMBA
 * @id: The bundle identification string
 *
 * Adds a bundle identifier to the component.
 *
 * Since: 0.8.0
 **/
void
as_component_add_bundle_id (AsComponent *cpt, AsBundleKind bundle_kind, const gchar *id)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_hash_table_insert (priv->bundles,
			     GINT_TO_POINTER (bundle_kind),
			     g_strdup (id));
}

/**
 * as_component_has_bundle:
 * @cpt: a #AsComponent instance.
 *
 * Returns: %TRUE if this component has a bundle-id associated.
 **/
gboolean
as_component_has_bundle (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return g_hash_table_size (priv->bundles) > 0;
}

/**
 * as_component_set_bundles_table:
 * @cpt: a #AsComponent instance.
 *
 * Internal function.
 **/
void
as_component_set_bundles_table (AsComponent *cpt, GHashTable *bundles)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_hash_table_unref (priv->bundles);
	priv->bundles = g_hash_table_ref (bundles);
}

/**
 * as_component_has_package:
 * @cpt: a #AsComponent instance.
 *
 * Internal function.
 **/
gboolean
as_component_has_package (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return (priv->pkgnames != NULL) && (priv->pkgnames[0] != NULL);
}

/**
 * as_component_has_install_candidate:
 * @cpt: a #AsComponent instance.
 *
 * Returns: %TRUE if the component has an installable component (package, bundle, ...) set.
 **/
gboolean
as_component_has_install_candidate (AsComponent *cpt)
{
	return as_component_has_bundle (cpt) || as_component_has_package (cpt);
}

/**
 * as_component_get_kind:
 * @cpt: a #AsComponent instance.
 *
 * Returns the #AsComponentKind of this component.
 *
 * Returns: the kind of #this.
 */
AsComponentKind
as_component_get_kind (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->kind;
}

/**
 * as_component_set_kind:
 * @cpt: a #AsComponent instance.
 * @value: the #AsComponentKind.
 *
 * Sets the #AsComponentKind of this component.
 */
void
as_component_set_kind (AsComponent *cpt, AsComponentKind value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	priv->kind = value;
	g_object_notify ((GObject *) cpt, "kind");
}

/**
 * as_component_get_pkgnames:
 * @cpt: a #AsComponent instance.
 *
 * Get a list of package names which this component consists of.
 * This usually is just one package name.
 *
 * Returns: (transfer none): String array of package names
 */
gchar**
as_component_get_pkgnames (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->pkgnames;
}

/**
 * as_component_set_pkgnames:
 * @cpt: a #AsComponent instance.
 * @value: (array zero-terminated=1):
 *
 * Set a list of package names this component consists of.
 * (This should usually be just one package name)
 */
void
as_component_set_pkgnames (AsComponent *cpt, gchar** value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_strfreev (priv->pkgnames);
	priv->pkgnames = g_strdupv (value);
	g_object_notify ((GObject *) cpt, "pkgnames");
}

/**
 * as_component_get_source_pkgname:
 * @cpt: a #AsComponent instance.
 *
 * Returns: the source package name.
 */
const gchar*
as_component_get_source_pkgname (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->source_pkgname;
}

/**
 * as_component_set_source_pkgname:
 * @cpt: a #AsComponent instance.
 * @spkgname: the source package name.
 */
void
as_component_set_source_pkgname (AsComponent *cpt, const gchar* spkgname)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_free (priv->source_pkgname);
	priv->source_pkgname = g_strdup (spkgname);
}

/**
 * as_component_get_id:
 * @cpt: a #AsComponent instance.
 *
 * Set the unique identifier for this component.
 *
 * Returns: the unique identifier.
 */
const gchar*
as_component_get_id (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->id;
}

/**
 * as_component_set_id:
 * @cpt: a #AsComponent instance.
 * @value: the unique identifier.
 *
 * Set the unique identifier for this component.
 */
void
as_component_set_id (AsComponent *cpt, const gchar* value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_free (priv->id);
	priv->id = g_strdup (value);
	g_object_notify ((GObject *) cpt, "id");
}

/**
 * as_component_get_origin:
 * @cpt: a #AsComponent instance.
 */
const gchar*
as_component_get_origin (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->origin;
}

/**
 * as_component_set_origin:
 * @cpt: a #AsComponent instance.
 * @origin: the origin.
 */
void
as_component_set_origin (AsComponent *cpt, const gchar* origin)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* safety measure, so we never set this to NULL */
	if (origin == NULL)
		origin = "";
	g_free (priv->origin);
	priv->origin = g_strdup (origin);
}

/**
 * as_component_get_active_locale:
 * @cpt: a #AsComponent instance.
 *
 * Get the current active locale for this component, which
 * is used to get localized messages.
 *
 * Returns: the current active locale.
 */
gchar*
as_component_get_active_locale (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->active_locale;
}

/**
 * as_component_set_active_locale:
 * @cpt: a #AsComponent instance.
 * @locale: (nullable): the locale, or %NULL. e.g. "en_GB"
 *
 * Set the current active locale for this component, which
 * is used to get localized messages.
 * If the #AsComponent was fetched from a localized database, usually only
 * one locale is available.
 */
void
as_component_set_active_locale (AsComponent *cpt, const gchar *locale)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_free (priv->active_locale);
	priv->active_locale = g_strdup (locale);
}

/**
 * as_component_localized_get:
 * @cpt: a #AsComponent instance.
 * @lht: (element-type utf8 utf8): the #GHashTable on which the value will be retreived.
 *
 * Helper function to get a localized property using the current
 * active locale for this component.
 */
static gchar*
as_component_localized_get (AsComponent *cpt, GHashTable *lht)
{
	gchar *msg;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	msg = g_hash_table_lookup (lht, priv->active_locale);
	if (msg == NULL) {
		/* fall back to untranslated / default */
		msg = g_hash_table_lookup (lht, "C");
	}

	return msg;
}

/**
 * as_component_localized_set:
 * @cpt: a #AsComponent instance.
 * @lht: (element-type utf8 utf8): the #GHashTable on which the value will be added.
 * @value: the value to add.
 * @locale: (nullable): the locale, or %NULL. e.g. "en_GB".
 *
 * Helper function to set a localized property.
 */
static void
as_component_localized_set (AsComponent *cpt, GHashTable *lht, const gchar* value, const gchar *locale)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* safety measure, so we can always convert this to a C++ string */
	if (value == NULL)
		value = "";

	/* if no locale was specified, we assume the default locale */
	/* CAVE: %NULL does NOT mean lang=C! */
	if (locale == NULL)
		locale = priv->active_locale;

	g_hash_table_insert (lht,
						 g_strdup (locale),
						 g_strdup (value));
}

/**
 * as_component_get_name:
 * @cpt: a #AsComponent instance.
 *
 * A human-readable name for this component.
 *
 * Returns: the name.
 */
const gchar*
as_component_get_name (AsComponent *cpt)
{
	const gchar *name;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	name = as_component_localized_get (cpt, priv->name);
	/* prevent issues when converting to a C++ string */
	if (name == NULL)
		return "";
	return name;
}

/**
 * as_component_set_name:
 * @cpt: A valid #AsComponent
 * @value: The name
 * @locale: (nullable): The locale the used for this value, or %NULL to use the current active one.
 *
 * Set a human-readable name for this component.
 */
void
as_component_set_name (AsComponent *cpt, const gchar* value, const gchar *locale)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	as_component_localized_set (cpt, priv->name, value, locale);
	g_object_notify ((GObject *) cpt, "name");
}

/**
 * as_component_get_name_table:
 * @cpt: a #AsComponent instance.
 *
 * Internal method.
 */
GHashTable*
as_component_get_name_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->name;
}

/**
 * as_component_get_summary:
 * @cpt: a #AsComponent instance.
 *
 * Get a short description of this component.
 *
 * Returns: the summary.
 */
const gchar*
as_component_get_summary (AsComponent *cpt)
{
	const gchar *summary;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	summary = as_component_localized_get (cpt, priv->summary);
	/* prevent issues when converting to a C++ string */
	if (summary == NULL)
		return "";
	return summary;
}

/**
 * as_component_set_summary:
 * @cpt: A valid #AsComponent
 * @value: The summary
 * @locale: (nullable): The locale the used for this value, or %NULL to use the current active one.
 *
 * Set a short description for this component.
 */
void
as_component_set_summary (AsComponent *cpt, const gchar* value, const gchar *locale)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	as_component_localized_set (cpt, priv->summary, value, locale);
	g_object_notify ((GObject *) cpt, "summary");
}

/**
 * as_component_get_summary_table:
 * @cpt: a #AsComponent instance.
 *
 * Internal method.
 */
GHashTable*
as_component_get_summary_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->summary;
}

/**
 * as_component_get_description:
 * @cpt: a #AsComponent instance.
 *
 * Get the localized long description of this component.
 *
 * Returns: the description.
 */
const gchar*
as_component_get_description (AsComponent *cpt)
{
	const gchar *desc;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	desc = as_component_localized_get (cpt, priv->description);
	/* prevent issues when converting to a C++ string */
	if (desc == NULL)
		return "";
	return desc;
}

/**
 * as_component_set_description:
 * @cpt: A valid #AsComponent
 * @value: The long description
 * @locale: (nullable): The locale the used for this value, or %NULL to use the current active one.
 *
 * Set long description for this component.
 */
void
as_component_set_description (AsComponent *cpt, const gchar* value, const gchar *locale)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	as_component_localized_set (cpt, priv->description, value, locale);
	g_object_notify ((GObject *) cpt, "description");
}

/**
 * as_component_get_description_table:
 * @cpt: a #AsComponent instance.
 *
 * Internal method.
 */
GHashTable*
as_component_get_description_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->description;
}

/**
 * as_component_get_keywords:
 * @cpt: a #AsComponent instance.
 *
 * Returns: (transfer none): String array of keywords
 */
gchar**
as_component_get_keywords (AsComponent *cpt)
{
	gchar **strv;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	strv = g_hash_table_lookup (priv->keywords, priv->active_locale);
	if (strv == NULL) {
		/* fall back to untranslated */
		strv = g_hash_table_lookup (priv->keywords, "C");
	}

	return strv;
}

/**
 * as_component_set_keywords:
 * @cpt: a #AsComponent instance.
 * @value: (array zero-terminated=1): String-array of keywords
 * @locale: (nullable): Locale of the values, or %NULL to use current locale.
 *
 * Set keywords for this component.
 */
void
as_component_set_keywords (AsComponent *cpt, gchar **value, const gchar *locale)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* if no locale was specified, we assume the default locale */
	if (locale == NULL)
		locale = priv->active_locale;

	g_hash_table_insert (priv->keywords,
						 g_strdup (locale),
						 g_strdupv (value));

	g_object_notify ((GObject *) cpt, "keywords");
}

/**
 * as_component_get_keywords_table:
 * @cpt: a #AsComponent instance.
 *
 * Internal method.
 */
GHashTable*
as_component_get_keywords_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->keywords;
}

/**
 * as_component_get_icons:
 * @cpt: an #AsComponent instance
 *
 * Returns: (element-type AsIcon) (transfer none): A #GPtrArray of all icons for this component.
 */
GPtrArray*
as_component_get_icons (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->icons;
}

/**
 * as_component_get_icon_by_size:
 * @cpt: an #AsComponent instance
 * @width: The icon width in pixels.
 * @height: the icon height in pixels.
 *
 * Gets an icon matching the size constraints.
 * The icons are not filtered by type, and the first icon
 * which matches the size is returned.
 * If you want more control over which icons you use for displaying,
 * use the as_component_get_icons() function to get a list of all icons.
 *
 * Returns: (transfer none): An icon matching the given width/height, or %NULL if not found.
 */
AsIcon*
as_component_get_icon_by_size (AsComponent *cpt, guint width, guint height)
{
	g_autofree gchar *size = NULL;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if ((width == 0) && (height == 0))
		return NULL;

	size = g_strdup_printf ("%ix%i", width, height);
	return g_hash_table_lookup (priv->icons_sizetab, size);
}

/**
 * as_component_add_icon:
 * @cpt: an #AsComponent instance
 * @icon: the valid #AsIcon instance to add.
 *
 * Add an icon to this component.
 */
void
as_component_add_icon (AsComponent *cpt, AsIcon *icon)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_ptr_array_add (priv->icons, g_object_ref (icon));
	if ((as_icon_get_width (icon) != 0) && (as_icon_get_height (icon) != 0)) {
		gchar *size = NULL;
		size = g_strdup_printf ("%ix%i",
					as_icon_get_width (icon),
					as_icon_get_height (icon));
		g_hash_table_insert (priv->icons_sizetab, size, icon);
	}
}

/**
 * as_component_get_categories:
 * @cpt: a #AsComponent instance.
 *
 * Returns: (transfer none): String array of categories
 */
gchar**
as_component_get_categories (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->categories;
}

/**
 * as_component_set_categories:
 * @cpt: a #AsComponent instance.
 * @value: (array zero-terminated=1): the categories name
 */
void
as_component_set_categories (AsComponent *cpt, gchar** value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_strfreev (priv->categories);
	priv->categories = g_strdupv (value);
	g_object_notify ((GObject *) cpt, "categories");
}

/**
 * as_component_set_categories_from_str:
 * @cpt: a valid #AsComponent instance
 * @categories_str: Semicolon-separated list of category-names
 *
 * Set the categories list from a string
 */
void
as_component_set_categories_from_str (AsComponent *cpt, const gchar *categories_str)
{
	gchar** cats = NULL;

	g_return_if_fail (categories_str != NULL);

	cats = g_strsplit (categories_str, ";", 0);
	as_component_set_categories (cpt, cats);
	g_strfreev (cats);
}

/**
 * as_component_has_category:
 * @cpt: an #AsComponent object
 * @category: the specified category to check
 *
 * Check if component is in the specified category.
 *
 * Returns: %TRUE if the component is in the specified category.
 **/
gboolean
as_component_has_category (AsComponent *cpt, const gchar* category)
{
	gchar **categories;
	guint i;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	categories = priv->categories;
	for (i = 0; categories[i] != NULL; i++) {
		if (g_strcmp0 (categories[i], category) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * as_component_get_project_license:
 * @cpt: a #AsComponent instance.
 *
 * Get the license of the project this component belongs to.
 *
 * Returns: the license.
 */
const gchar*
as_component_get_project_license (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->project_license;
}

/**
 * as_component_set_project_license:
 * @cpt: a #AsComponent instance.
 * @value: the project license.
 *
 * Set the project license.
 */
void
as_component_set_project_license (AsComponent *cpt, const gchar* value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_free (priv->project_license);
	priv->project_license = g_strdup (value);
	g_object_notify ((GObject *) cpt, "project-license");
}

/**
 * as_component_get_project_group:
 * @cpt: a #AsComponent instance.
 *
 * Get the component's project group.
 *
 * Returns: the project group.
 */
const gchar*
as_component_get_project_group (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->project_group;
}

/**
 * as_component_set_project_group:
 * @cpt: a #AsComponent instance.
 * @value: the project group.
 *
 * Set the component's project group.
 */
void
as_component_set_project_group (AsComponent *cpt, const gchar *value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_free (priv->project_group);
	priv->project_group = g_strdup (value);
}

/**
 * as_component_get_developer_name:
 * @cpt: a #AsComponent instance.
 *
 * Get the component's developer or development team name.
 *
 * Returns: the developer name.
 */
const gchar*
as_component_get_developer_name (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return as_component_localized_get (cpt, priv->developer_name);
}

/**
 * as_component_set_developer_name:
 * @cpt: a #AsComponent instance.
 * @value: the developer or developer team name
 * @locale: (nullable): the locale, or %NULL. e.g. "en_GB"
 *
 * Set the the component's developer or development team name.
 */
void
as_component_set_developer_name (AsComponent *cpt, const gchar *value, const gchar *locale)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_component_localized_set (cpt, priv->developer_name, value, locale);
}

/**
 * as_component_get_developer_name_table:
 * @cpt: a #AsComponent instance.
 *
 * Internal method.
 */
GHashTable*
as_component_get_developer_name_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->developer_name;
}

/**
 * as_component_get_screenshots:
 * @cpt: a #AsComponent instance.
 *
 * Get a list of associated screenshots.
 *
 * Returns: (element-type AsScreenshot) (transfer none): an array of #AsScreenshot instances
 */
GPtrArray*
as_component_get_screenshots (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	return priv->screenshots;
}

/**
 * as_component_get_compulsory_for_desktops:
 * @cpt: a #AsComponent instance.
 *
 * Return value: (transfer none): A list of desktops where this component is compulsory
 **/
gchar **
as_component_get_compulsory_for_desktops (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	return priv->compulsory_for_desktops;
}

/**
 * as_component_set_compulsory_for_desktops:
 * @cpt: a #AsComponent instance.
 * @value: (array zero-terminated=1): the array of desktop ids.
 *
 * Set a list of desktops where this component is compulsory.
 **/
void
as_component_set_compulsory_for_desktops (AsComponent *cpt, gchar** value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	g_strfreev (priv->compulsory_for_desktops);
	priv->compulsory_for_desktops = g_strdupv (value);
}

/**
 * as_component_is_compulsory_for_desktop:
 * @cpt: an #AsComponent object
 * @desktop: the desktop-id to test for
 *
 * Check if this component is compulsory for the given desktop.
 *
 * Returns: %TRUE if compulsory, %FALSE otherwise.
 **/
gboolean
as_component_is_compulsory_for_desktop (AsComponent *cpt, const gchar* desktop)
{
	gchar **compulsory_for_desktops;
	guint i;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	compulsory_for_desktops = priv->compulsory_for_desktops;
	for (i = 0; compulsory_for_desktops[i] != NULL; i++) {
		if (g_strcmp0 (compulsory_for_desktops[i], desktop) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * as_component_get_provided_for_kind:
 * @cpt: a #AsComponent instance.
 * @kind: kind of the provided item, e.g. %AS_PROVIDED_KIND_MIMETYPE
 *
 * Get an #AsProvided object for the given interface type,
 * containing information about the public interfaces (mimetypes, firmware, DBus services, ...)
 * this component provides.
 *
 * Returns: (transfer none): #AsProvided containing the items this component provides, or %NULL.
 **/
AsProvided*
as_component_get_provided_for_kind (AsComponent *cpt, AsProvidedKind kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return AS_PROVIDED (g_hash_table_lookup (priv->provided, GINT_TO_POINTER (kind)));
}

/**
 * as_component_get_provided:
 * @cpt: a #AsComponent instance.
 *
 * Get a list of #AsProvided objects associated with this component.
 *
 * Returns: (transfer container) (element-type AsProvided): A list of #AsProvided objects.
 **/
GList*
as_component_get_provided (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return g_hash_table_get_values (priv->provided);
}

/**
 * as_component_add_provided:
 * @cpt: a #AsComponent instance.
 * @prov: a #AsProvided instance.
 *
 * Add a set of provided items to this component.
 *
 * Since: 0.6.2
 **/
void
as_component_add_provided (AsComponent *cpt, AsProvided *prov)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_hash_table_insert (priv->provided,
				GINT_TO_POINTER (as_provided_get_kind (prov)),
				g_object_ref (prov));
}

/**
 * as_component_add_provided_item:
 * @cpt: a #AsComponent instance.
 * @kind: the kind of the provided item (e.g. %AS_PROVIDED_KIND_MIMETYPE)
 * @item: the item to add.
 *
 * Adds a provided item to the component.
 *
 * Internal function for use by the metadata reading classes.
 **/
void
as_component_add_provided_item (AsComponent *cpt, AsProvidedKind kind, const gchar *item)
{
	AsProvided *prov;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* we just skip empty items */
	if (as_str_empty (item))
		return;

	prov = as_component_get_provided_for_kind (cpt, kind);
	if (prov == NULL) {
		prov = as_provided_new ();
		as_provided_set_kind (prov, kind);
		g_hash_table_insert (priv->provided, GINT_TO_POINTER (kind), prov);
	}

	as_provided_add_item (prov, item);
}

/**
 * as_component_get_releases:
 * @cpt: a #AsComponent instance.
 *
 * Get an array of the #AsRelease items this component
 * provides.
 *
 * Return value: (element-type AsRelease) (transfer none): A list of releases
 **/
GPtrArray*
as_component_get_releases (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	return priv->releases;
}

/**
 * as_component_get_priority:
 * @cpt: a #AsComponent instance.
 *
 * Returns the priority of this component.
 * This method is used internally.
 *
 * Since: 0.6.1
 */
int
as_component_get_priority (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->priority;
}

/**
 * as_component_set_priority:
 * @cpt: a #AsComponent instance.
 * @priority: the given priority
 *
 * Sets the priority of this component.
 * This method is used internally.
 *
 * Since: 0.6.1
 */
void
as_component_set_priority (AsComponent *cpt, int priority)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	priv->priority = priority;
}

/**
 * as_component_add_language:
 * @cpt: an #AsComponent instance.
 * @locale: (nullable): the locale, or %NULL. e.g. "en_GB"
 * @percentage: the percentage completion of the translation, 0 for locales with unknown amount of translation
 *
 * Adds a language to the component.
 *
 * Since: 0.7.0
 **/
void
as_component_add_language (AsComponent *cpt, const gchar *locale, gint percentage)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->languages,
				g_strdup (locale),
				GINT_TO_POINTER (percentage));
}

/**
 * as_component_get_language:
 * @cpt: an #AsComponent instance.
 * @locale: (nullable): the locale, or %NULL. e.g. "en_GB"
 *
 * Gets the translation coverage in percent for a specific locale
 *
 * Returns: a percentage value, -1 if locale was not found
 *
 * Since: 0.7.0
 **/
gint
as_component_get_language (AsComponent *cpt, const gchar *locale)
{
	gboolean ret;
	gpointer value = NULL;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (locale == NULL)
		locale = "C";
	ret = g_hash_table_lookup_extended (priv->languages,
					    locale, NULL, &value);
	if (!ret)
		return -1;
	return GPOINTER_TO_INT (value);
}

/**
 * as_component_get_languages:
 * @cpt: an #AsComponent instance.
 *
 * Get a list of all languages.
 *
 * Returns: (transfer container) (element-type utf8): list of locales
 *
 * Since: 0.7.0
 **/
GList*
as_component_get_languages (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return g_hash_table_get_keys (priv->languages);
}

/**
 * as_component_get_languages_map:
 * @cpt: an #AsComponent instance.
 *
 * Get a HashMap mapping languages to their completion percentage
 *
 * Returns: (transfer none): locales map
 *
 * Since: 0.7.0
 **/
GHashTable*
as_component_get_languages_map (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->languages;
}

/**
 * as_component_add_icon_full:
 *
 * Internal helper function for as_component_refine_icons()
 */
static void
as_component_add_icon_full (AsComponent *cpt, AsIconKind kind, const gchar *size_str, const gchar *fname)
{
	g_autoptr(AsIcon) icon = NULL;

	icon = as_icon_new ();
	as_icon_set_kind (icon, kind);
	as_icon_set_filename (icon, fname);

	if (g_strcmp0 (size_str, "128x128") == 0) {
		as_icon_set_width (icon, 128);
		as_icon_set_height (icon, 128);
	} else {
		/* it's either "64x64", emptystring or NULL, in any case we assume 64x64
		 * This has to be adapted as soon as we support more than 2 icon sizes, but
		 * we are lazy here to not hurt performance too much. */
		as_icon_set_width (icon, 64);
		as_icon_set_height (icon, 64);
	}

	as_component_add_icon (cpt, icon);
}

/**
 * as_component_refine_icons:
 * @cpt: a #AsComponent instance.
 *
 * We use this method to ensure the "icon" and "icon_url" properties of
 * a component are properly set, by finding the icons in default directories.
 */
static void
as_component_refine_icons (AsComponent *cpt, gchar **icon_paths)
{
	const gchar *exensions[] = { "png",
				     "svg",
				     "svgz",
				     "gif",
				     "ico",
				     "xcf",
				     NULL };
	const gchar *sizes[] = { "", "64x64", "128x128", NULL };
	gchar *tmp_icon_path = NULL;
	const gchar *icon_url = NULL;
	guint i, j, k, l;
	g_autoptr(GPtrArray) icons = NULL;
	g_autoptr(GHashTable) icons_sizetab = NULL;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (priv->icons->len == 0)
		return;

	/* take control of the old icon list and hashtable and rewrite it */
	icons = priv->icons;
	icons_sizetab = priv->icons_sizetab;
	priv->icons = g_ptr_array_new_with_free_func (g_object_unref);
	priv->icons_sizetab = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* Process the icons we have and extract sizes */
	for (i = 0; i < icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		/* we can't do anything about remote icons (yet?), so just don't do anything to them */
		if (as_icon_get_kind (icon) == AS_ICON_KIND_REMOTE) {
			as_component_add_icon (cpt, icon);
			continue;
		}

		/* get some icon name we want to resolve */
		if (as_icon_get_kind (icon) == AS_ICON_KIND_CACHED)
			icon_url = as_icon_get_filename (icon);
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK)
			icon_url = as_icon_get_name (icon);

		if (g_str_has_prefix (icon_url, "/") ||
			g_str_has_prefix (icon_url, "http://")) {
			/* looks like this component already has a full icon path,
			 * or is a weblink. */
			as_component_add_icon (cpt, icon);
			continue;
		}

		/* search local icon path */
		for (l = 0; icon_paths[l] != NULL; l++) {
			for (j = 0; sizes[j] != NULL; j++) {
				/* sometimes, the file already has an extension */
				tmp_icon_path = g_strdup_printf ("%s/%s/%s/%s",
								icon_paths[l],
								priv->origin,
								sizes[j],
								icon_url);
				if (g_file_test (tmp_icon_path, G_FILE_TEST_EXISTS)) {
					/* we have an icon! */
					if (g_strcmp0 (sizes[j], "") == 0) {
						/* old icon directory, so assume 64x64 */
						as_component_add_icon_full (cpt,
									    as_icon_get_kind (icon),
									    "64x64",
									    tmp_icon_path);
					} else {
						as_component_add_icon_full (cpt,
									    as_icon_get_kind (icon),
									    sizes[j],
									    tmp_icon_path);
					}

					g_free (tmp_icon_path);
					tmp_icon_path = NULL;
					continue;
				}
				g_free (tmp_icon_path);
				tmp_icon_path = NULL;

				/* file not found, try extensions (we will not do this forever, better fix AppStream data!) */
				for (k = 0; exensions[k] != NULL; k++) {
					tmp_icon_path = g_strdup_printf ("%s/%s/%s/%s.%s",
								icon_paths[l],
								priv->origin,
								sizes[j],
								icon_url,
								exensions[k]);
					if (g_file_test (tmp_icon_path, G_FILE_TEST_EXISTS)) {
						/* we have an icon! */
						if (g_strcmp0 (sizes[j], "") == 0) {
							/* old icon directory, so assume 64x64 */
							as_component_add_icon_full (cpt,
									    as_icon_get_kind (icon),
									    "64x64",
									    tmp_icon_path);
						} else {
							as_component_add_icon_full (cpt,
									    as_icon_get_kind (icon),
									    sizes[j],
									    tmp_icon_path);
						}
					}

					g_free (tmp_icon_path);
					tmp_icon_path = NULL;
				}
			}
		}
	}

	if (tmp_icon_path != NULL) {
		g_free (tmp_icon_path);
	}
}

/**
 * as_component_complete:
 * @cpt: a #AsComponent instance.
 * @scr_base_url: Base url for screenshot-service, obtain via #AsDistroDetails
 * @icon_paths: Zero-terminated string array of possible (cached) icon locations
 *
 * Private function to complete a AsComponent with
 * additional data found on the system.
 *
 * INTERNAL
 */
void
as_component_complete (AsComponent *cpt, gchar *scr_base_url, gchar **icon_paths)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* we want screenshot data from 3rd-party screenshot servers, if the component doesn't have screenshots defined already */
	if ((priv->screenshots->len == 0) && (as_component_has_package (cpt))) {
		gchar *url;
		AsImage *img;
		AsScreenshot *sshot;

		url = g_build_filename (scr_base_url, "screenshot", priv->pkgnames[0], NULL);

		/* screenshots.debian.net-like services dont specify a size, so we choose the default sizes
		 * (800x600 for source-type images, 160x120 for thumbnails)
		 */

		/* add main image */
		img = as_image_new ();
		as_image_set_url (img, url);
		as_image_set_width (img, 800);
		as_image_set_height (img, 600);
		as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);

		sshot = as_screenshot_new ();

		/* propagate locale */
		as_screenshot_set_active_locale (sshot, priv->active_locale);

		as_screenshot_add_image (sshot, img);
		as_screenshot_set_kind (sshot, AS_SCREENSHOT_KIND_DEFAULT);

		g_object_unref (img);
		g_free (url);

		/* add thumbnail */
		url = g_build_filename (scr_base_url, "thumbnail", priv->pkgnames[0], NULL);
		img = as_image_new ();
		as_image_set_url (img, url);
		as_image_set_width (img, 160);
		as_image_set_height (img, 120);
		as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
		as_screenshot_add_image (sshot, img);

		/* add screenshot to component */
		as_component_add_screenshot (cpt, sshot);

		g_object_unref (img);
		g_object_unref (sshot);
		g_free (url);
	}

	/* improve icon paths */
	as_component_refine_icons (cpt, icon_paths);
}

/**
 * as_component_get_property:
 */
static void
as_component_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	AsComponent *cpt;
	cpt = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_COMPONENT, AsComponent);
	switch (property_id) {
		case AS_COMPONENT_KIND:
			g_value_set_enum (value, as_component_get_kind (cpt));
			break;
		case AS_COMPONENT_PKGNAMES:
			g_value_set_boxed (value, as_component_get_pkgnames (cpt));
			break;
		case AS_COMPONENT_ID:
			g_value_set_string (value, as_component_get_id (cpt));
			break;
		case AS_COMPONENT_NAME:
			g_value_set_string (value, as_component_get_name (cpt));
			break;
		case AS_COMPONENT_SUMMARY:
			g_value_set_string (value, as_component_get_summary (cpt));
			break;
		case AS_COMPONENT_DESCRIPTION:
			g_value_set_string (value, as_component_get_description (cpt));
			break;
		case AS_COMPONENT_KEYWORDS:
			g_value_set_boxed (value, as_component_get_keywords (cpt));
			break;
		case AS_COMPONENT_ICONS:
			g_value_set_pointer (value, as_component_get_icons (cpt));
			break;
		case AS_COMPONENT_URLS:
			g_value_set_boxed (value, as_component_get_urls_table (cpt));
			break;
		case AS_COMPONENT_CATEGORIES:
			g_value_set_boxed (value, as_component_get_categories (cpt));
			break;
		case AS_COMPONENT_PROJECT_LICENSE:
			g_value_set_string (value, as_component_get_project_license (cpt));
			break;
		case AS_COMPONENT_PROJECT_GROUP:
			g_value_set_string (value, as_component_get_project_group (cpt));
			break;
		case AS_COMPONENT_DEVELOPER_NAME:
			g_value_set_string (value, as_component_get_developer_name (cpt));
			break;
		case AS_COMPONENT_SCREENSHOTS:
			g_value_set_boxed (value, as_component_get_screenshots (cpt));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/**
 * as_component_set_property:
 */
static void
as_component_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	AsComponent *cpt;
	cpt = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_COMPONENT, AsComponent);

	switch (property_id) {
		case AS_COMPONENT_KIND:
			as_component_set_kind (cpt, g_value_get_enum (value));
			break;
		case AS_COMPONENT_PKGNAMES:
			as_component_set_pkgnames (cpt, g_value_get_boxed (value));
			break;
		case AS_COMPONENT_ID:
			as_component_set_id (cpt, g_value_get_string (value));
			break;
		case AS_COMPONENT_NAME:
			as_component_set_name (cpt, g_value_get_string (value), NULL);
			break;
		case AS_COMPONENT_SUMMARY:
			as_component_set_summary (cpt, g_value_get_string (value), NULL);
			break;
		case AS_COMPONENT_DESCRIPTION:
			as_component_set_description (cpt, g_value_get_string (value), NULL);
			break;
		case AS_COMPONENT_KEYWORDS:
			as_component_set_keywords (cpt, g_value_get_boxed (value), NULL);
			break;
		case AS_COMPONENT_CATEGORIES:
			as_component_set_categories (cpt, g_value_get_boxed (value));
			break;
		case AS_COMPONENT_PROJECT_LICENSE:
			as_component_set_project_license (cpt, g_value_get_string (value));
			break;
		case AS_COMPONENT_PROJECT_GROUP:
			as_component_set_project_group (cpt, g_value_get_string (value));
			break;
		case AS_COMPONENT_DEVELOPER_NAME:
			as_component_set_developer_name (cpt, g_value_get_string (value), NULL);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/**
 * as_component_class_init:
 */
static void
as_component_class_init (AsComponentClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_component_finalize;
	object_class->get_property = as_component_get_property;
	object_class->set_property = as_component_set_property;
	/**
	 * AsComponent:kind:
	 *
	 * the #AsComponentKind of this component
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_KIND,
					g_param_spec_enum ("kind", "kind", "kind", AS_TYPE_COMPONENT_KIND, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:pkgnames:
	 *
	 * string array of packages name
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_PKGNAMES,
					g_param_spec_boxed ("pkgnames", "pkgnames", "pkgnames", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:id:
	 *
	 * the unique identifier
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_ID,
					g_param_spec_string ("id", "id", "id", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:name:
	 *
	 * the name
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_NAME,
					g_param_spec_string ("name", "name", "name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:summary:
	 *
	 * the summary
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_SUMMARY,
					g_param_spec_string ("summary", "summary", "summary", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:description:
	 *
	 * the description
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_DESCRIPTION,
					g_param_spec_string ("description", "description", "description", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:keywords:
	 *
	 * string array of keywords
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_KEYWORDS,
					g_param_spec_boxed ("keywords", "keywords", "keywords", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:icons: (type GList(AsIcon))
	 *
	 * hash map of icon urls and sizes
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_ICONS,
					g_param_spec_pointer ("icons", "icons", "icons", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	/**
	 * AsComponent:urls: (type GHashTable(AsUrlKind,utf8))
	 *
	 * the urls associated with this component
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_URLS,
					g_param_spec_boxed ("urls", "urls", "urls", G_TYPE_HASH_TABLE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	/**
	 * AsComponent:categories:
	 *
	 * string array of categories
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_CATEGORIES,
					g_param_spec_boxed ("categories", "categories", "categories", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:project-license:
	 *
	 * the project license
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_PROJECT_LICENSE,
					g_param_spec_string ("project-license", "project-license", "project-license", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:project-group:
	 *
	 * the project group
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_PROJECT_GROUP,
					g_param_spec_string ("project-group", "project-group", "project-group", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:developer-name:
	 *
	 * the developer name
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_DEVELOPER_NAME,
					g_param_spec_string ("developer-name", "developer-name", "developer-name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	/**
	 * AsComponent:screenshots: (type GPtrArray(AsScreenshot)):
	 *
	 * An array of #AsScreenshot instances
	 */
	g_object_class_install_property (object_class,
					AS_COMPONENT_SCREENSHOTS,
					g_param_spec_boxed ("screenshots", "screenshots", "screenshots", G_TYPE_PTR_ARRAY, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}

/**
 * as_component_new:
 *
 * Creates a new #AsComponent.
 *
 * Returns: (transfer full): a new #AsComponent
 **/
AsComponent*
as_component_new (void)
{
	AsComponent *cpt;
	cpt = g_object_new (AS_TYPE_COMPONENT, NULL);
	return AS_COMPONENT (cpt);
}
