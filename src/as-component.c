/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include <fnmatch.h>

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-stemmer.h"
#include "as-spdx.h"

#include "as-context-private.h"
#include "as-icon-private.h"
#include "as-screenshot-private.h"
#include "as-bundle-private.h"
#include "as-release-private.h"
#include "as-translation-private.h"
#include "as-suggested-private.h"
#include "as-content-rating-private.h"
#include "as-launchable-private.h"
#include "as-provided-private.h"
#include "as-relation-private.h"
#include "as-agreement-private.h"
#include "as-review-private.h"
#include "as-branding-private.h"
#include "as-desktop-entry.h"


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
	AsComponentKind 	kind;
	AsComponentScope	scope;
	AsOriginKind		origin_kind;
	AsContext		*context; /* the document context associated with this component */
	GRefString		*active_locale_override;
	gchar			*date_eol;

	gchar			*id;
	gchar			*data_id;
	gchar			**pkgnames;
	gchar			*source_pkgname;
	GRefString		*origin;
	GRefString		*branch;

	GHashTable		*name; /* localized entry */
	GHashTable		*summary; /* localized entry */
	GHashTable		*description; /* localized entry */
	GHashTable		*keywords; /* localized entry, value:strv */
	GHashTable		*developer_name; /* localized entry */

	GRefString		*metadata_license;
	GRefString		*project_license;
	GRefString		*project_group;

	GPtrArray		*launchables; /* of #AsLaunchable */
	GPtrArray		*categories; /* of utf8 */
	GPtrArray		*compulsory_for_desktops; /* of utf8 */
	GPtrArray		*extends; /* of utf8 */
	GPtrArray		*addons; /* of AsComponent */
	GPtrArray		*screenshots; /* of AsScreenshot elements */
	GPtrArray		*releases; /* of AsRelease elements */
	GPtrArray		*provided; /* of AsProvided */
	GPtrArray		*bundles; /* of AsBundle */
	GPtrArray		*suggestions; /* of AsSuggested elements */
	GPtrArray		*content_ratings; /* of AsContentRating */
	GPtrArray		*requires; /* of AsRelation */
	GPtrArray		*recommends; /* of AsRelation */
	GPtrArray		*supports; /* of AsRelation */
	GPtrArray		*replaces; /* of utf8 */
	GPtrArray		*agreements; /* of AsAgreement */
	GHashTable		*urls; /* of int:utf8 */

	GHashTable		*languages; /* of utf8:utf8 */
	GPtrArray		*translations; /* of AsTranslation */

	GPtrArray		*icons; /* of AsIcon elements */
	GPtrArray		*reviews; /* of AsReview */

	AsBranding		*branding;

	GRefString		*arch; /* the architecture this data was generated from */
	gint			priority; /* used internally */
	AsMergeKind		merge_kind; /* whether and how the component data should be merged */

	guint			sort_score; /* used to priorize components in listings */
	gsize			token_cache_valid;
	GHashTable		*token_cache; /* of RefString:AsTokenType* */

	AsValueFlags		value_flags;

	gboolean		ignored; /* whether we should ignore this component */

	GPtrArray		*tags;
	GHashTable		*name_variant_suffix; /* variant suffix for component name */
	GHashTable		*custom; /* of RefString:RefString, free-form user-defined custom data */
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
 * as_component_kind_to_string:
 * @kind: the #AsComponentKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar*
as_component_kind_to_string (AsComponentKind kind)
{
	if (kind == AS_COMPONENT_KIND_GENERIC)
		return "generic";
	if (kind == AS_COMPONENT_KIND_DESKTOP_APP)
		return "desktop-application";
	if (kind == AS_COMPONENT_KIND_CONSOLE_APP)
		return "console-application";
	if (kind == AS_COMPONENT_KIND_WEB_APP)
		return "web-application";
	if (kind == AS_COMPONENT_KIND_ADDON)
		return "addon";
	if (kind == AS_COMPONENT_KIND_FONT)
		return "font";
	if (kind == AS_COMPONENT_KIND_CODEC)
		return "codec";
	if (kind == AS_COMPONENT_KIND_INPUT_METHOD)
		return "inputmethod";
	if (kind == AS_COMPONENT_KIND_FIRMWARE)
		return "firmware";
	if (kind == AS_COMPONENT_KIND_DRIVER)
		return "driver";
	if (kind == AS_COMPONENT_KIND_LOCALIZATION)
		return "localization";
	if (kind == AS_COMPONENT_KIND_SERVICE)
		return "service";
	if (kind == AS_COMPONENT_KIND_REPOSITORY)
		return "repository";
	if (kind == AS_COMPONENT_KIND_OPERATING_SYSTEM)
		return "operating-system";
	if (kind == AS_COMPONENT_KIND_ICON_THEME)
		return "icon-theme";
	if (kind == AS_COMPONENT_KIND_RUNTIME)
		return "runtime";
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
	if (kind_str == NULL)
		return AS_COMPONENT_KIND_GENERIC;
	if (g_strcmp0 (kind_str, "generic") == 0)
		return AS_COMPONENT_KIND_GENERIC;
	if (g_strcmp0 (kind_str, "desktop-application") == 0)
		return AS_COMPONENT_KIND_DESKTOP_APP;
	if (g_strcmp0 (kind_str, "console-application") == 0)
		return AS_COMPONENT_KIND_CONSOLE_APP;
	if (g_strcmp0 (kind_str, "web-application") == 0)
		return AS_COMPONENT_KIND_WEB_APP;
	if (g_strcmp0 (kind_str, "addon") == 0)
		return AS_COMPONENT_KIND_ADDON;
	if (g_strcmp0 (kind_str, "font") == 0)
		return AS_COMPONENT_KIND_FONT;
	if (g_strcmp0 (kind_str, "codec") == 0)
		return AS_COMPONENT_KIND_CODEC;
	if (g_strcmp0 (kind_str, "inputmethod") == 0)
		return AS_COMPONENT_KIND_INPUT_METHOD;
	if (g_strcmp0 (kind_str, "firmware") == 0)
		return AS_COMPONENT_KIND_FIRMWARE;
	if (g_strcmp0 (kind_str, "driver") == 0)
		return AS_COMPONENT_KIND_DRIVER;
	if (g_strcmp0 (kind_str, "localization") == 0)
		return AS_COMPONENT_KIND_LOCALIZATION;
	if (g_strcmp0 (kind_str, "service") == 0)
		return AS_COMPONENT_KIND_SERVICE;
	if (g_strcmp0 (kind_str, "repository") == 0)
		return AS_COMPONENT_KIND_REPOSITORY;
	if (g_strcmp0 (kind_str, "operating-system") == 0)
		return AS_COMPONENT_KIND_OPERATING_SYSTEM;
	if (g_strcmp0 (kind_str, "icon-theme") == 0)
		return AS_COMPONENT_KIND_ICON_THEME;
	if (g_strcmp0 (kind_str, "runtime") == 0)
		return AS_COMPONENT_KIND_RUNTIME;

	/* legacy */
	if (g_strcmp0 (kind_str, "desktop") == 0)
		return AS_COMPONENT_KIND_DESKTOP_APP;
	if (g_strcmp0 (kind_str, "desktop-app") == 0)
		return AS_COMPONENT_KIND_DESKTOP_APP;

	return AS_COMPONENT_KIND_UNKNOWN;
}

/**
 * as_merge_kind_to_string:
 * @kind: the #AsMergeKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar*
as_merge_kind_to_string (AsMergeKind kind)
{
	if (kind == AS_MERGE_KIND_NONE)
		return "none";
	if (kind == AS_MERGE_KIND_REPLACE)
		return "replace";
	if (kind == AS_MERGE_KIND_APPEND)
		return "append";
	if (kind == AS_MERGE_KIND_REMOVE_COMPONENT)
		return "remove-component";

	return "unknown";
}

/**
 * as_merge_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsMergeKind or %AS_MERGE_KIND_NONE for unknown
 **/
AsMergeKind
as_merge_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "replace") == 0)
		return AS_MERGE_KIND_REPLACE;
	if (g_strcmp0 (kind_str, "append") == 0)
		return AS_MERGE_KIND_APPEND;
	if (g_strcmp0 (kind_str, "remove-component") == 0)
		return AS_MERGE_KIND_REMOVE_COMPONENT;

	return AS_MERGE_KIND_NONE;
}

/**
 * as_component_scope_to_string:
 * @scope: the #AsComponentScope.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @scope
 **/
const gchar*
as_component_scope_to_string (AsComponentScope scope)
{
	if (scope == AS_COMPONENT_SCOPE_SYSTEM)
		return "system";
	if (scope == AS_COMPONENT_SCOPE_USER)
		return "user";
	return "unknown";
}

/**
 * as_component_scope_from_string:
 * @scope_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsComponentScope or %AS_COMPONENT_SCOPE_UNKNOWN for unknown
 **/
AsComponentScope
as_component_scope_from_string (const gchar *scope_str)
{
	if (g_strcmp0 (scope_str, "system") == 0)
		return AS_COMPONENT_SCOPE_SYSTEM;
	if (g_strcmp0 (scope_str, "user") == 0)
		return AS_COMPONENT_SCOPE_USER;
	return AS_COMPONENT_SCOPE_UNKNOWN;
}

/**
 * as_component_init:
 **/
static void
as_component_init (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* translatable entities */
	priv->name = g_hash_table_new_full (g_str_hash, g_str_equal,
					    (GDestroyNotify) as_ref_string_release,
					    g_free);
	priv->summary = g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify) as_ref_string_release,
					       g_free);
	priv->description = g_hash_table_new_full (g_str_hash, g_str_equal,
						   (GDestroyNotify) as_ref_string_release,
						   g_free);
	priv->developer_name = g_hash_table_new_full (g_str_hash, g_str_equal,
						      (GDestroyNotify) as_ref_string_release,
						      g_free);
	priv->keywords = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) as_ref_string_release,
						(GDestroyNotify) g_strfreev);

	/* lists */
	priv->launchables = g_ptr_array_new_with_free_func (g_object_unref);
	priv->categories = g_ptr_array_new_with_free_func (g_free);
	priv->compulsory_for_desktops = g_ptr_array_new_with_free_func (g_free);
	priv->screenshots = g_ptr_array_new_with_free_func (g_object_unref);
	priv->releases = g_ptr_array_new_with_free_func (g_object_unref);
	priv->provided = g_ptr_array_new_with_free_func (g_object_unref);
	priv->bundles = g_ptr_array_new_with_free_func (g_object_unref);
	priv->extends = g_ptr_array_new_with_free_func (g_free);
	priv->addons = g_ptr_array_new_with_free_func (g_object_unref);
	priv->suggestions = g_ptr_array_new_with_free_func (g_object_unref);
	priv->icons = g_ptr_array_new_with_free_func (g_object_unref);
	priv->content_ratings = g_ptr_array_new_with_free_func (g_object_unref);
	priv->requires = g_ptr_array_new_with_free_func (g_object_unref);
	priv->recommends = g_ptr_array_new_with_free_func (g_object_unref);
	priv->supports = g_ptr_array_new_with_free_func (g_object_unref);
	priv->agreements = g_ptr_array_new_with_free_func (g_object_unref);
	priv->reviews = g_ptr_array_new_with_free_func (g_object_unref);

	/* others */
	priv->tags = g_ptr_array_new_with_free_func (g_free);
	priv->urls = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	priv->languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	priv->custom = g_hash_table_new_full (g_str_hash, g_str_equal,
					      (GDestroyNotify) g_ref_string_release,
					      (GDestroyNotify) g_ref_string_release);

	priv->token_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
						   (GDestroyNotify) g_ref_string_release,
						   g_free);

	priv->priority = 0;
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
	g_free (priv->data_id);
	g_free (priv->date_eol);
	g_free (priv->source_pkgname);
	g_strfreev (priv->pkgnames);
	as_ref_string_release (priv->metadata_license);
	as_ref_string_release (priv->project_license);
	as_ref_string_release (priv->project_group);
	as_ref_string_release (priv->active_locale_override);
	as_ref_string_release (priv->arch);
	as_ref_string_release (priv->origin);
	as_ref_string_release (priv->branch);

	g_hash_table_unref (priv->name);
	g_hash_table_unref (priv->summary);
	g_hash_table_unref (priv->description);
	g_hash_table_unref (priv->developer_name);
	g_hash_table_unref (priv->keywords);

	g_ptr_array_unref (priv->launchables);
	g_ptr_array_unref (priv->categories);
	g_ptr_array_unref (priv->compulsory_for_desktops);

	g_ptr_array_unref (priv->screenshots);
	g_ptr_array_unref (priv->releases);
	g_ptr_array_unref (priv->provided);
	g_ptr_array_unref (priv->bundles);
	g_ptr_array_unref (priv->extends);
	g_ptr_array_unref (priv->addons);
	g_ptr_array_unref (priv->suggestions);
	g_hash_table_unref (priv->urls);
	g_hash_table_unref (priv->languages);
	g_hash_table_unref (priv->custom);
	g_ptr_array_unref (priv->content_ratings);
	g_ptr_array_unref (priv->icons);
	g_ptr_array_unref (priv->tags);

	g_ptr_array_unref (priv->requires);
	g_ptr_array_unref (priv->recommends);
	g_ptr_array_unref (priv->supports);
	g_ptr_array_unref (priv->agreements);
	g_ptr_array_unref (priv->reviews);

	if (priv->replaces != NULL)
		g_ptr_array_unref (priv->replaces);

	if (priv->branding != NULL)
		g_object_unref (priv->branding);

	if (priv->translations != NULL)
		g_ptr_array_unref (priv->translations);

	if (priv->name_variant_suffix != NULL)
		g_hash_table_unref (priv->name_variant_suffix);

	g_hash_table_unref (priv->token_cache);

	if (priv->context != NULL)
		g_object_unref (priv->context);

	G_OBJECT_CLASS (as_component_parent_class)->finalize (object);
}

/**
 * as_component_invalidate_data_id:
 *
 * Internal method to mark the metadata-ID as outdated, so
 * it will be regenerated next time.
 */
static void
as_component_invalidate_data_id (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->data_id == NULL)
		return;
	g_free (priv->data_id);
	priv->data_id = NULL;
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
	const gchar *cname;
	const gchar *csummary;
	AsComponentKind ctype;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	ctype = priv->kind;
	if (ctype == AS_COMPONENT_KIND_UNKNOWN)
		return FALSE;
	if (priv->merge_kind != AS_MERGE_KIND_NONE) {
		/* merge components only need an ID to be valid */
		return !as_is_empty (priv->id);
	}

	cname = as_component_get_name (cpt);
	csummary = as_component_get_summary (cpt);
	if ((!as_is_empty (priv->id)) &&
		(!as_is_empty (cname)) &&
		(!as_is_empty (csummary))) {
			return TRUE;
	}

	return FALSE;
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
	g_autofree gchar *pkgs = NULL;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (as_component_has_package (cpt))
		pkgs = g_strjoinv (",", priv->pkgnames);
	else
		pkgs = g_strdup ("<none>");

	res = g_strdup_printf ("[%s::%s]> name: %s | summary: %s | package: %s",
				as_component_kind_to_string (priv->kind),
				as_component_get_data_id (cpt),
				as_component_get_name (cpt),
				as_component_get_summary (cpt),
				pkgs);

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
	g_return_if_fail (sshot != NULL);

	sslist = as_component_get_screenshots (cpt);
	g_ptr_array_add (sslist, g_object_ref (sshot));
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
 * as_component_add_release:
 * @cpt: a #AsComponent instance.
 * @release: The #AsRelease to add
 *
 * Add an #AsRelease to this component.
 **/
void
as_component_add_release (AsComponent *cpt, AsRelease* release)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_add (priv->releases, g_object_ref (release));
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
  * Returns: (element-type utf8) (transfer none) (nullable): A #GPtrArray or %NULL if not set.
  *
  * Since: 0.15.5
**/
GPtrArray*
as_component_get_extends (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->extends;
}

/**
 * as_component_add_extends:
 * @cpt: a #AsComponent instance.
 * @cpt_id: The id of a component which is extended by this component
 *
 * Add a reference to the extended component
 *
 * Since: 0.15.5
 **/
void
as_component_add_extends (AsComponent* cpt, const gchar* cpt_id)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (as_flags_contains (priv->value_flags, AS_VALUE_FLAG_DUPLICATE_CHECK)) {
		/* check for duplicates */
		if (as_ptr_array_find_string (priv->extends, cpt_id) != NULL)
			return;
	}
	g_ptr_array_add (priv->extends,
			 g_strdup (cpt_id));
}

/**
  * as_component_get_addons:
  * @cpt: an #AsComponent instance.
  *
  * Returns a list of #AsComponent objects which
  * are addons extending this component in functionality.
  *
  * This is the reverse of %as_component_get_extends()
  *
  * Returns: (transfer none) (element-type AsComponent): An array of #AsComponent.
  *
  * Since: 0.9.2
**/
GPtrArray*
as_component_get_addons (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->addons;
}

/**
 * as_component_add_addon:
 * @cpt: a #AsComponent instance.
 * @addon: The #AsComponent that extends @cpt
 *
 * Add a reference to the addon that is enhancing this component.
 *
 * Since: 0.9.2
 **/
void
as_component_add_addon (AsComponent* cpt, AsComponent *addon)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_add (priv->addons, g_object_ref (addon));
}

/**
 * as_component_get_bundles:
 * @cpt: a #AsComponent instance.
 *
 * Get a list of all software bundles associated with this component.
 *
 * Returns: (transfer none) (element-type AsBundle): A list of #AsBundle.
 *
 * Since: 0.10
 **/
GPtrArray*
as_component_get_bundles (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->bundles;
}

/**
 * as_component_set_bundles_array:
 * @cpt: a #AsComponent instance.
 *
 * Internal helper.
 **/
void
as_component_set_bundles_array (AsComponent *cpt, GPtrArray *bundles)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_assign_ptr_array_safe (priv->bundles, bundles);
	as_component_invalidate_data_id (cpt);
}

/**
 * as_component_get_bundle:
 * @cpt: a #AsComponent instance.
 * @bundle_kind: the bundle kind, e.g. %AS_BUNDLE_KIND_LIMBA.
 *
 * Gets a bundle identifier string.
 *
 * Returns: (transfer none) (nullable): An #AsBundle, or %NULL if not set.
 *
 * Since: 0.8.0
 **/
AsBundle*
as_component_get_bundle (AsComponent *cpt, AsBundleKind bundle_kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	guint i;

	for (i = 0; i < priv->bundles->len; i++) {
		AsBundle *bundle = AS_BUNDLE (g_ptr_array_index (priv->bundles, i));
		if (as_bundle_get_kind (bundle) == bundle_kind)
			return bundle;
	}

	return NULL;
}

/**
 * as_component_add_bundle:
 * @cpt: a #AsComponent instance.
 * @bundle: The #AsBundle to add.
 *
 * Adds a bundle to the component.
 *
 * Since: 0.8.0
 **/
void
as_component_add_bundle (AsComponent *cpt, AsBundle *bundle)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_add (priv->bundles,
			 g_object_ref (bundle));
	as_component_invalidate_data_id (cpt);
}

/**
 * as_component_has_bundle:
 * @cpt: a #AsComponent instance.
 *
 * Returns: %TRUE if this component has a bundle associated.
 **/
gboolean
as_component_has_bundle (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->bundles->len > 0;
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
 * as_component_get_date_eol:
 * @cpt: a #AsComponent instance.
 *
 * Gets the end-of-life date for the entire component.
 *
 * Returns: The EOL date as string in ISO8601 format.
 *
 * Since: 0.15.2
 **/
const gchar*
as_component_get_date_eol (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->date_eol;
}

/**
 * as_component_set_date_eol:
 * @cpt: a #AsComponent instance.
 * @date: the EOL date in ISO8601 format.
 *
 * Sets an end-of-life date for this component.
 *
 * Since: 0.15.2
 **/
void
as_component_set_date_eol (AsComponent *cpt, const gchar *date)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_assign_string_safe (priv->date_eol, date);
}

/**
 * as_component_get_timestamp_eol:
 * @cpt: a #AsComponent instance.
 *
 * Gets the UNIX timestamp for the date when this component
 * is out of support (end-of-life) and will receive no more
 * updates, not even security fixes.
 *
 * Returns: UNIX timestamp, or 0 for unset or invalid.
 *
 * Since: 0.15.2
 **/
guint64
as_component_get_timestamp_eol (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autoptr(GDateTime) time = NULL;

	if (priv->date_eol == NULL)
		return 0;

	time = as_iso8601_to_datetime (priv->date_eol);
	if (time != NULL) {
		return g_date_time_to_unix (time);
	} else {
		g_warning ("Unable to retrieve EOL timestamp from component EOL date: %s", priv->date_eol);
		return 0;
	}
}

/**
 * as_component_sanitize_date_eol:
 */
static gchar*
as_component_sanitize_date_eol (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autoptr(GDateTime) time = as_iso8601_to_datetime (priv->date_eol);
	if (time != NULL)
		return g_date_time_format_iso8601 (time);

	/* error, not a valid date */
	return NULL;
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
 * as_component_get_pkgname:
 * @cpt: a #AsComponent instance.
 *
 * Get the first package name of the list of packages that need to be installed
 * for this component to be present on the system.
 * Since most components consist of only one package, this is safe to use for
 * about 90% of all cases.
 *
 * However, to support a component fully, please use %as_component_get_pkgnames() for
 * getting all packages that need to be installed, and use this method only to
 * e.g. get the main package to perform a quick "is it installed?" check.
 *
 * Returns: (transfer none): String array of package names
 */
gchar*
as_component_get_pkgname (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if ((priv->pkgnames == NULL) || (priv->pkgnames[0] == NULL))
		return NULL;
	return priv->pkgnames[0];
}

/**
 * as_component_set_pkgnames:
 * @cpt: a #AsComponent instance.
 * @packages: (array zero-terminated=1):
 *
 * Set a list of package names this component consists of.
 * (This should usually be just one package name)
 */
void
as_component_set_pkgnames (AsComponent *cpt, gchar **packages)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (G_UNLIKELY (priv->pkgnames == packages))
		return;

	g_strfreev (priv->pkgnames);
	priv->pkgnames = g_strdupv (packages);
	g_object_notify ((GObject *) cpt, "pkgnames");
}

/**
 * as_component_set_pkgname:
 * @cpt: a #AsComponent instance.
 * @pkgname: the package name
 *
 * Set the package name that provides this component.
 *
 * Since: 0.14.5
 */
void
as_component_set_pkgname (AsComponent *cpt, const gchar *pkgname)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_strfreev (priv->pkgnames);
	priv->pkgnames = g_new0 (gchar*, 2);
	priv->pkgnames[0] = g_strdup (pkgname);
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
	as_assign_string_safe (priv->source_pkgname, spkgname);
}

/**
 * as_component_get_id:
 * @cpt: a #AsComponent instance.
 *
 * Get the unique AppStream identifier for this component.
 * This ID is unique for the described component, but does
 * not uniquely identify the metadata set.
 *
 * For a unique ID for this metadata set in the current
 * session, use %as_component_get_data_id()
 *
 * Returns: the unique AppStream identifier.
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
 * Set the AppStream identifier for this component.
 */
void
as_component_set_id (AsComponent *cpt, const gchar* value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	as_assign_string_safe (priv->id, value);
	g_object_notify ((GObject *) cpt, "id");
	as_component_invalidate_data_id (cpt);
}

/**
 * as_component_get_data_id:
 * @cpt: a #AsComponent instance.
 *
 * Get a unique identifier for this metadata set.
 * This unique ID is only valid for the current session,
 * as opposed to the AppStream ID which uniquely identifies
 * a software component.
 *
 * The format of the unique id usually is:
 * %{scope}/%{origin}/%{distribution_system}/%{appstream_id}
 *
 * For example:
 * system/os/package/org.example.FooBar
 *
 * Returns: the unique session-specific identifier.
 */
const gchar*
as_component_get_data_id (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->data_id == NULL)
		priv->data_id = as_utils_build_data_id_for_cpt (cpt);
	return priv->data_id;
}

/**
 * as_component_set_data_id:
 * @cpt: a #AsComponent instance.
 * @value: the unique session-specific identifier.
 *
 * Set the session-specific unique metadata identifier for this
 * component.
 * If two components have a different data_id but the same ID,
 * they will be treated as independent sets of metadata describing
 * the same component type.
 */
void
as_component_set_data_id (AsComponent *cpt, const gchar* value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_assign_string_safe (priv->data_id, value);
}

/**
 * as_component_get_origin:
 * @cpt: a #AsComponent instance.
 */
const gchar*
as_component_get_origin (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if ((priv->context != NULL) && (priv->origin == NULL))
		return as_context_get_origin (priv->context);
	return priv->origin;
}

/**
 * as_component_set_origin:
 * @cpt: a #AsComponent instance.
 * @origin: the origin.
 */
void
as_component_set_origin (AsComponent *cpt, const gchar *origin)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_ref_string_assign_safe (&priv->origin, origin);
	as_component_invalidate_data_id (cpt);
}

/**
 * as_component_get_branch:
 * @cpt: a #AsComponent instance.
 *
 * Gets the branch for the application.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.14.0
 */
const gchar*
as_component_get_branch (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->branch;
}

/**
 * as_component_set_branch:
 * @cpt: a #AsComponent instance.
 * @branch: the branch, e.g. "master" or "3-16".
 *
 * Set the branch that the component instance was sourced from.
 *
 * Since: 0.14.0
 */
void
as_component_set_branch (AsComponent *cpt, const gchar *branch)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_ref_string_assign_safe (&priv->branch, branch);
	as_component_invalidate_data_id (cpt);
}

/**
 * as_component_get_architecture:
 * @cpt: a #AsComponent instance.
 *
 * The architecture of the software component this data was generated from.
 */
const gchar*
as_component_get_architecture (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if ((priv->context != NULL) && (priv->arch == NULL))
		return as_context_get_architecture (priv->context);
	return priv->arch;
}

/**
 * as_component_set_architecture:
 * @cpt: a #AsComponent instance.
 * @arch: the architecture string.
 */
void
as_component_set_architecture (AsComponent *cpt, const gchar *arch)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_ref_string_assign_safe (&priv->arch, arch);
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
const gchar*
as_component_get_active_locale (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	const GRefString *locale;

	/* return context locale, if the locale isn't explicitly overridden for this component */
	if ((priv->context != NULL) && (priv->active_locale_override == NULL)) {
		locale = as_context_get_locale (priv->context);
	} else {
		locale = priv->active_locale_override;
	}

	if (locale == NULL)
		return "C";
	else
		return locale;
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
	as_ref_string_assign_safe (&priv->active_locale_override, locale);
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
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return as_context_localized_ht_get (priv->context,
					    priv->name,
					    priv->active_locale_override,
					    priv->value_flags);
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

	as_context_localized_ht_set (priv->context,
				     priv->name,
				     value,
				     locale);
	g_object_notify ((GObject *) cpt, "name");
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
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return as_context_localized_ht_get (priv->context,
					    priv->summary,
					    priv->active_locale_override,
					    priv->value_flags);
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

	as_context_localized_ht_set (priv->context,
				     priv->summary,
				     value,
				     locale);
	g_object_notify ((GObject *) cpt, "summary");
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
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return as_context_localized_ht_get (priv->context,
					    priv->description,
					    priv->active_locale_override,
					    priv->value_flags);
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

	as_context_localized_ht_set (priv->context,
				     priv->description,
				     value,
				     locale);
	g_object_notify ((GObject *) cpt, "description");
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
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	gchar **strv;

	strv = g_hash_table_lookup (priv->keywords, as_component_get_active_locale (cpt));
	if (strv == NULL) {
		/* fall back to untranslated */
		strv = g_hash_table_lookup (priv->keywords, "C");
	}

	return strv;
}

/**
 * as_component_get_keywords_table:
 * @cpt: a #AsComponent instance.
 *
 * Retrieve the internal hash table mapping languages to
 * keword arrays.
 *
 * Returns: (transfer none): Hash table locale->keyword arrays
 */
GHashTable*
as_component_get_keywords_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->keywords;
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
	g_autoptr(GPtrArray) keywords = NULL;

	/* if no locale was specified, we assume the default locale */
	if (locale == NULL)
		locale = as_component_get_active_locale (cpt);

	keywords = g_ptr_array_new ();

	if (value != NULL) {
		for (guint i = 0; value[i] != NULL; ++i) {
			if (g_strcmp0 (value[i], "") != 0)
				g_ptr_array_add (keywords, g_strdup (value[i]));
		}
	}
	g_ptr_array_add (keywords, NULL);

#if GLIB_CHECK_VERSION(2,64,0)
	g_hash_table_insert (priv->keywords,
				g_ref_string_new_intern (locale),
				(gchar **) (g_ptr_array_steal (keywords, NULL)));
#else
	g_hash_table_insert (priv->keywords,
				g_ref_string_new_intern (locale),
				(gchar **) g_ptr_array_free (g_steal_pointer (&keywords), FALSE));
#endif

	g_object_notify ((GObject *) cpt, "keywords");
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
 * use the %as_component_get_icons() function to get a list of all icons.
 *
 * Note that this function is not HiDPI aware! It will never return an icon with
 * a scaling factor > 1.
 *
 * Returns: (transfer none) (nullable): An icon matching the given width/height, or %NULL if not found.
 */
AsIcon*
as_component_get_icon_by_size (AsComponent *cpt, guint width, guint height)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	guint i;

	for (i = 0; i < priv->icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (priv->icons, i));
		/* ignore scaled icons */
		if (as_icon_get_scale (icon) > 1)
			continue;

		if ((as_icon_get_width (icon) == width) && (as_icon_get_height (icon) == height))
			return icon;
	}

	return NULL;
}

/**
 * as_component_get_icon_stock:
 * @cpt: an #AsComponent instance
 *
 * Gets a stock icon for this component if one is associated with it.
 * Will return %NULL otherwise.
 *
 * Returns: (transfer none) (nullable): An stock icon, or %NULL if none found.
 */
AsIcon*
as_component_get_icon_stock (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	for (guint i = 0; i < priv->icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (priv->icons, i));
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
			return icon;
		}
	}

	return NULL;
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
}

/**
 * as_component_get_categories:
 * @cpt: a #AsComponent instance.
 *
 * Returns: (transfer none) (element-type utf8): String array of categories
 */
GPtrArray*
as_component_get_categories (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->categories;
}

/**
 * as_component_add_category:
 * @cpt: a #AsComponent instance.
 * @category: the categories name to add.
 *
 * Add a category.
 */
void
as_component_add_category (AsComponent *cpt, const gchar *category)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (as_flags_contains (priv->value_flags, AS_VALUE_FLAG_DUPLICATE_CHECK)) {
		/* check for duplicates */
		if (as_ptr_array_find_string (priv->categories, category) != NULL)
			return;
	}
	g_ptr_array_add (priv->categories,
			 g_strdup (category));
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
as_component_has_category (AsComponent *cpt, const gchar *category)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return as_ptr_array_find_string (priv->categories, category) != NULL;
}

/**
 * as_component_get_metadata_license:
 * @cpt: a #AsComponent instance.
 *
 * The license the metadata iself is subjected to.
 *
 * Returns: the license.
 */
const gchar*
as_component_get_metadata_license (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->metadata_license;
}

/**
 * as_component_set_metadata_license:
 * @cpt: a #AsComponent instance.
 * @value: the metadata license.
 *
 * Set the license this metadata is licensed under.
 */
void
as_component_set_metadata_license (AsComponent *cpt, const gchar *value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_ref_string_assign_safe (&priv->metadata_license, value);
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
as_component_set_project_license (AsComponent *cpt, const gchar *value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	as_ref_string_assign_safe (&priv->project_license, value);
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
	as_ref_string_assign_safe (&priv->project_group, value);
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
	return as_context_localized_ht_get (priv->context,
					    priv->developer_name,
					    priv->active_locale_override,
					    priv->value_flags);
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
	as_context_localized_ht_set (priv->context,
				     priv->developer_name,
				     value,
				     locale);
}

/**
 * as_component_clear_tags:
 * @cpt: a #AsComponent instance.
 *
 * Remove all tags associated with this component.
 *
 * Since: 0.15.0
 */
void
as_component_clear_tags (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_remove_range (priv->tags, 0, priv->tags->len);
}

/**
 * as_component_add_tag:
 * @cpt: a #AsComponent instance.
 * @ns: The namespace the tag belongs to
 * @tag: The tag name
 *
 * Add a tag to this component.
 *
 * Returns: %TRUE if the tag was added.
 *
 * Since: 0.15.0
 */
gboolean
as_component_add_tag (AsComponent *cpt, const gchar *ns, const gchar *tag)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autofree gchar *tag_full = g_strconcat (ns, "::", tag, NULL);

	/* sanity check */
	if (g_strstr_len (tag, -1, "::") != NULL)
		return FALSE;

	for (guint i = 0; i < priv->tags->len; i++) {
		const gchar *tag_iter = g_ptr_array_index (priv->tags, i);
		if (g_strcmp0 (tag_iter, tag_full) == 0)
			return TRUE;
	}

	g_ptr_array_add (priv->tags,
			 g_steal_pointer (&tag_full));
	return TRUE;
}

/**
 * as_component_remove_tag:
 * @cpt: a #AsComponent instance.
 * @ns: The namespace the tag belongs to
 * @tag: The tag name
 *
 * Remove a tag from this component
 *
 * Returns: %TRUE if the tag was removed.
 *
 * Since: 0.15.0
 */
gboolean
as_component_remove_tag (AsComponent *cpt, const gchar *ns, const gchar *tag)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autofree gchar *tag_full = g_strconcat (ns, "::", tag, NULL);

	for (guint i = 0; i < priv->tags->len; i++) {
		const gchar *tag_iter = g_ptr_array_index (priv->tags, i);
		if (g_strcmp0 (tag_iter, tag_full) == 0) {
			g_ptr_array_remove_index_fast (priv->tags, i);
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * as_component_has_tag:
 * @cpt: a #AsComponent instance.
 * @ns: The namespace the tag belongs to
 * @tag: The tag name
 *
 * Test if the component is tagged with the selected
 * tag.
 *
 * Returns: %TRUE if tag exists.
 *
 * Since: 0.15.0
 */
gboolean
as_component_has_tag (AsComponent *cpt, const gchar *ns, const gchar *tag)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autofree gchar *tag_full = g_strconcat (ns, "::", tag, NULL);

	for (guint i = 0; i < priv->tags->len; i++) {
		const gchar *tag_iter = g_ptr_array_index (priv->tags, i);
		if (g_strcmp0 (tag_iter, tag_full) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * as_component_get_name_variant_suffix:
 * @cpt: a #AsComponent instance.
 *
 * Get variant suffix for the component name
 * (only to be displayed if two components have the same name).
 *
 * Returns: the variant suffix
 *
 * Since: 0.12.10
 */
const gchar*
as_component_get_name_variant_suffix (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->name_variant_suffix == NULL)
		return NULL;
	return as_context_localized_ht_get (priv->context,
					    priv->name_variant_suffix,
					    priv->active_locale_override,
					    priv->value_flags);
}

/**
 * as_component_set_name_variant_suffix:
 * @cpt: a #AsComponent instance.
 * @value: the developer or developer team name
 * @locale: (nullable): the locale, or %NULL. e.g. "en_GB"
 *
 * Set a variant suffix for the component name
 * (only to be displayed if components have the same name).
 *
 * Since: 0.12.10
 */
void
as_component_set_name_variant_suffix (AsComponent *cpt, const gchar *value, const gchar *locale)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->name_variant_suffix == NULL)
		priv->name_variant_suffix = g_hash_table_new_full (g_str_hash, g_str_equal,
								   (GDestroyNotify) as_ref_string_release,
								   g_free);
	as_context_localized_ht_set (priv->context,
				     priv->name_variant_suffix,
				     value,
				     locale);
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
 * Return value: (transfer none) (element-type utf8): A list of desktops where this component is compulsory
 **/
GPtrArray*
as_component_get_compulsory_for_desktops (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->compulsory_for_desktops;
}

/**
 * as_component_set_compulsory_for_desktop:
 * @cpt: a #AsComponent instance.
 * @desktop: The name of the desktop.
 *
 * Mark this component to be compulsory for the specified desktop environment.
 **/
void
as_component_set_compulsory_for_desktop (AsComponent *cpt, const gchar *desktop)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_return_if_fail (desktop != NULL);

	if (as_flags_contains (priv->value_flags, AS_VALUE_FLAG_DUPLICATE_CHECK)) {
		/* check for duplicates */
		if (as_ptr_array_find_string (priv->compulsory_for_desktops, desktop) != NULL)
			return;
	}
	g_ptr_array_add (priv->compulsory_for_desktops,
			 g_strdup (desktop));
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
as_component_is_compulsory_for_desktop (AsComponent *cpt, const gchar *desktop)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return as_ptr_array_find_string (priv->compulsory_for_desktops, desktop) != NULL;
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
 * Returns: (transfer none) (nullable): #AsProvided containing the items this component provides, or %NULL.
 **/
AsProvided*
as_component_get_provided_for_kind (AsComponent *cpt, AsProvidedKind kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	for (guint i = 0; i < priv->provided->len; i++) {
		AsProvided *prov = AS_PROVIDED (g_ptr_array_index (priv->provided, i));
		if (as_provided_get_kind (prov) == kind)
			return prov;
	}

	return NULL;
}

/**
 * as_component_get_provided:
 * @cpt: a #AsComponent instance.
 *
 * Get a list of #AsProvided objects associated with this component.
 *
 * Returns: (transfer none) (element-type AsProvided): A list of #AsProvided objects.
 **/
GPtrArray*
as_component_get_provided (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->provided;
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
	g_return_if_fail (prov != NULL);

	if (as_flags_contains (priv->value_flags, AS_VALUE_FLAG_DUPLICATE_CHECK)) {
		guint i;
		for (i = 0; i < priv->provided->len; i++) {
			AsProvided *eprov = AS_PROVIDED (g_ptr_array_index (priv->provided, i));
			if (as_provided_get_kind (prov) == as_provided_get_kind (eprov)) {
				/* replace existing entry */
				g_ptr_array_remove_index (priv->provided, i);
				g_ptr_array_add (priv->provided,
						 g_object_ref (prov));
				return;
			}
		}
	}

	g_ptr_array_add (priv->provided,
			 g_object_ref (prov));
}

/**
 * as_component_add_provided_item:
 * @cpt: a #AsComponent instance.
 * @kind: the kind of the provided item (e.g. %AS_PROVIDED_KIND_MEDIATYPE)
 * @item: the item to add.
 *
 * Adds a provided item to the component with the given @kind, creating a new
 * @AsProvided for this kind internally if necessary.
 **/
void
as_component_add_provided_item (AsComponent *cpt, AsProvidedKind kind, const gchar *item)
{
	AsProvided *prov;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* we just skip empty items */
	if (as_is_empty (item))
		return;

	prov = as_component_get_provided_for_kind (cpt, kind);
	if (prov == NULL) {
		prov = as_provided_new ();
		as_provided_set_kind (prov, kind);
		g_ptr_array_add (priv->provided, prov);
	}

	as_provided_add_item (prov, item);
}

/**
 * as_component_add_suggested:
 * @cpt: a #AsComponent instance.
 * @suggested: The #AsSuggested
 *
 * Add an #AsSuggested to this component.
 **/
void
as_component_add_suggested (AsComponent *cpt, AsSuggested *suggested)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_add (priv->suggestions,
			 g_object_ref (suggested));
}

/**
 * as_component_get_suggested:
 * @cpt: a #AsComponent instance.
 *
 * Get a list of associated suggestions.
 *
 * Returns: (transfer none) (element-type AsSuggested): an array of #AsSuggested instances
 */
GPtrArray*
as_component_get_suggested (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->suggestions;
}

/**
 * as_component_get_merge_kind:
 * @cpt: a #AsComponent instance.
 *
 * Get the merge method which should apply to duplicate components
 * with this ID.
 *
 * Returns: the #AsMergeKind of this component.
 *
 * Since: 0.9.8
 */
AsMergeKind
as_component_get_merge_kind (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->merge_kind;
}

/**
 * as_component_set_merge_kind:
 * @cpt: a #AsComponent instance.
 * @kind: the #AsMergeKind.
 *
 * Sets the #AsMergeKind for this component.
 *
 * Since: 0.9.8
 */
void
as_component_set_merge_kind (AsComponent *cpt, AsMergeKind kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	priv->merge_kind = kind;
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
gint
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
as_component_set_priority (AsComponent *cpt, gint priority)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	priv->priority = priority;
}

/**
 * as_component_get_sort_score:
 * @cpt: a #AsComponent instance.
 *
 * Returns the sorting priority of this component.
 *
 * This will only return a valid value if this component
 * was the result of or involved in a search operation which
 * returned sorted results.
 * In most cases you will not need to access this value explicitly,
 * as all results of search operations in AppStream are already sorted
 * from best match to worst.
 *
 * The returned value is an arbitrary integer value, valid only for
 * the search terms involved in the search operation that yielded
 * this component as a result.
 *
 * Since: 0.12.11
 */
guint
as_component_get_sort_score (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->sort_score;
}

/**
 * as_component_set_sort_score:
 * @cpt: a #AsComponent instance.
 * @score: the given sorting score
 *
 * Sets the sorting score of this component.
 *
 * Since: 0.9.8
 */
void
as_component_set_sort_score (AsComponent *cpt, guint score)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	priv->sort_score = score;
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
 * as_component_clear_languages:
 * @cpt: an #AsComponent instance.
 *
 * Remove all registered language translation information.
 *
 * Since: 0.14.5
 **/
void
as_component_clear_languages (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_hash_table_remove_all (priv->languages);
}

/**
 * as_component_get_languages_table:
 * @cpt: an #AsComponent instance.
 *
 * Get a HashMap mapping languages to their completion percentage
 *
 * Returns: (transfer none): locales map
 *
 * Since: 0.7.0
 **/
GHashTable*
as_component_get_languages_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->languages;
}

/**
 * as_component_get_name_table:
 * @cpt: an #AsComponent instance.
 *
 * Get the internal locale to component name
 * mapping table.
 *
 * Returns: (transfer none): locale->names map
 **/
GHashTable*
as_component_get_name_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->name;
}

/**
 * as_component_get_summary_table:
 * @cpt: an #AsComponent instance.
 *
 * Get the internal locale to component summary
 * mapping table.
 *
 * Returns: (transfer none): locale->summary map
 **/
GHashTable*
as_component_get_summary_table (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->summary;
}

/**
 * as_component_get_translations:
 * @cpt: an #AsComponent instance.
 *
 * Get a #GPtrArray of #AsTranslation objects describing the
 * translation systems and translation-ids (e.g. Gettext domains) used
 * by this software component.
 *
 * Only set for metainfo files.
 *
 * Returns: (transfer none) (element-type AsTranslation): An array of #AsTranslation objects.
 *
 * Since: 0.9.2
 */
GPtrArray*
as_component_get_translations (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->translations == NULL)
		priv->translations = g_ptr_array_new_with_free_func (g_object_unref);
	return priv->translations;
}

/**
 * as_component_add_translation:
 * @cpt: an #AsComponent instance.
 * @tr: an #AsTranslation instance.
 *
 * Assign an #AsTranslation object describing the translation system used
 * by this component.
 *
 * Since: 0.9.2
 */
void
as_component_add_translation (AsComponent *cpt, AsTranslation *tr)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->translations == NULL)
		priv->translations = g_ptr_array_new_with_free_func (g_object_unref);
	g_ptr_array_add (priv->translations, g_object_ref (tr));
}

/**
 * as_component_get_scope:
 * @cpt: a #AsComponent instance.
 *
 * Returns: the #AsComponentScope of this component.
 *
 * Since: 0.10.2
 */
AsComponentScope
as_component_get_scope (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->scope;
}

/**
 * as_component_set_scope:
 * @cpt: a #AsComponent instance.
 * @scope: the #AsComponentKind.
 *
 * Sets the #AsComponentScope of this component.
 */
void
as_component_set_scope (AsComponent *cpt, AsComponentScope scope)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	priv->scope = scope;
}

/**
 * as_component_get_origin_kind:
 * @cpt: a #AsComponent instance.
 *
 * Returns: the #AsOriginKind of this component.
 *
 * Since: 0.10.2
 */
AsOriginKind
as_component_get_origin_kind (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->origin_kind;
}

/**
 * as_component_set_origin_kind:
 * @cpt: a #AsComponent instance.
 * @okind: the #AsOriginKind.
 *
 * Sets the #AsOriginKind of this component.
 */
void
as_component_set_origin_kind (AsComponent *cpt, AsOriginKind okind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	priv->origin_kind = okind;
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
as_component_refine_icons (AsComponent *cpt, GPtrArray *icon_paths)
{
	const gchar *extensions[] = { "png",
				      "svg",
				      "svgz",
				      "gif",
				      "ico",
				      "xcf",
				      NULL };
	const gchar *sizes[] = { "", "64x64", "128x128", NULL };
	const gchar *icon_fname = NULL;
	const gchar *origin;
	guint i, j, k, l;
	g_autoptr(GPtrArray) icons = NULL;
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (priv->icons->len == 0)
		return;

	/* take control of the old icon list and rewrite it */
	icons = priv->icons;
	priv->icons = g_ptr_array_new_with_free_func (g_object_unref);

	origin = as_component_get_origin (cpt);

	/* Process the icons we have and extract sizes */
	for (i = 0; i < icons->len; i++) {
		AsIconKind ikind;
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		ikind = as_icon_get_kind (icon);
		if (ikind == AS_ICON_KIND_REMOTE) {
			/* no processing / icon-search is needed (and possible) for remote icons */
			as_component_add_icon (cpt, icon);
			continue;
		} else if (ikind == AS_ICON_KIND_STOCK) {
			/* since AppStream 0.9, we are not expected to find stock icon names in cache
			 * directories anymore, so we can just add it without changes */
			as_component_add_icon (cpt, icon);
			continue;
		}

		if ((ikind != AS_ICON_KIND_CACHED) && (ikind != AS_ICON_KIND_LOCAL)) {
			g_warning ("Found icon of unknown type '%s' in '%s', skipping it.",
				   as_icon_kind_to_string (ikind), as_component_get_data_id (cpt));
			continue;
		}

		/* get icon name we want to resolve
		 * (it's "filename", because we should only get here if we have
		 *  a CACHED or LOCAL icon) */
		icon_fname = as_icon_get_filename (icon);

		if (g_str_has_prefix (icon_fname, "/")) {
			/* looks like this component already has a full icon path */
			as_component_add_icon (cpt, icon);

			/* assume 64x64px icon, if not defined otherwise */
			if ((as_icon_get_width (icon) == 0) && (as_icon_get_height (icon) == 0)) {
				as_icon_set_width (icon, 64);
				as_icon_set_height (icon, 64);
			}

			continue;
		}

		/* skip the full cache search if we already have size information */
		if ((ikind == AS_ICON_KIND_CACHED) && (as_icon_get_width (icon) > 0)) {
			for (l = 0; l < icon_paths->len; l++) {
				g_autofree gchar *tmp_icon_path_wh = NULL;
				const gchar *icon_path = (const gchar*) g_ptr_array_index (icon_paths, l);

				if (as_icon_get_scale (icon) <= 1) {
					tmp_icon_path_wh = g_strdup_printf ("%s/%s/%ix%i/%s",
										icon_path,
										origin,
										as_icon_get_width (icon),
										as_icon_get_height (icon),
										icon_fname);
				} else {
					tmp_icon_path_wh = g_strdup_printf ("%s/%s/%ix%i@%i/%s",
										icon_path,
										origin,
										as_icon_get_width (icon),
										as_icon_get_height (icon),
										as_icon_get_scale (icon),
										icon_fname);
				}

				if (g_file_test (tmp_icon_path_wh, G_FILE_TEST_EXISTS)) {
					as_icon_set_filename (icon, tmp_icon_path_wh);
					as_component_add_icon (cpt, icon);
					break;
				}
			}

			/* we don't need a full search anymore - the icon having size information means that
			 * it will be in the "origin" subdirectory with the appropriate size, so there is no
			 * reason to start a big icon-hunt */
			continue;
		}

		/* search local icon path */
		for (l = 0; l < icon_paths->len; l++) {
			const gchar *icon_path = (const gchar*) g_ptr_array_index (icon_paths, l);

			for (j = 0; sizes[j] != NULL; j++) {
				g_autofree gchar *tmp_icon_path = NULL;
				/* sometimes, the file already has an extension */
				tmp_icon_path = g_strdup_printf ("%s/%s/%s/%s",
								icon_path,
								origin,
								sizes[j],
								icon_fname);

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
					continue;
				}

				/* file not found, try extensions (we will not do this forever, better fix AppStream data!) */
				for (k = 0; extensions[k] != NULL; k++) {
					g_autofree gchar *tmp_icon_path_ext = NULL;
					tmp_icon_path_ext = g_strdup_printf ("%s/%s/%s/%s.%s",
								icon_path,
								origin,
								sizes[j],
								icon_fname,
								extensions[k]);

					if (g_file_test (tmp_icon_path_ext, G_FILE_TEST_EXISTS)) {
						/* we have an icon! */
						if (g_strcmp0 (sizes[j], "") == 0) {
							/* old icon directory, so assume 64x64 */
							as_component_add_icon_full (cpt,
									    as_icon_get_kind (icon),
									    "64x64",
									    tmp_icon_path_ext);
						} else {
							as_component_add_icon_full (cpt,
									    as_icon_get_kind (icon),
									    sizes[j],
									    tmp_icon_path_ext);
						}
					}
				}
			}
		}
	}
}

/**
 * as_component_complete:
 * @cpt: a #AsComponent instance.
 * @scr_service_url: Base url for screenshot-service, obtain via #AsDistroDetails
 * @icon_paths: String array of possible (cached) icon locations
 *
 * Private function to complete a AsComponent with
 * additional data found on the system.
 *
 * INTERNAL
 */
void
as_component_complete (AsComponent *cpt, gchar *scr_service_url, GPtrArray *icon_paths)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	/* improve icon paths */
	if (icon_paths != NULL)
		as_component_refine_icons (cpt, icon_paths);

	/* "fake" a launchable entry for desktop-apps that failed to include one. This is used for legacy compatibility */
	if ((priv->kind == AS_COMPONENT_KIND_DESKTOP_APP) && (priv->launchables->len <= 0)) {
		if (g_str_has_suffix (priv->id, ".desktop")) {
			g_autoptr(AsLaunchable) launchable = as_launchable_new ();
			as_launchable_set_kind (launchable, AS_LAUNCHABLE_KIND_DESKTOP_ID);
			as_launchable_add_entry (launchable, priv->id);
			as_component_add_launchable (cpt, launchable);
		}
	}

	/* if there is no screenshot service URL, there is nothing left to do for us */
	if (scr_service_url == NULL)
		return;

	/* we want screenshot data from 3rd-party screenshot servers, if the component doesn't have screenshots defined already */
	if ((priv->screenshots->len == 0) && (as_component_has_package (cpt))) {
		gchar *url;
		AsImage *img;
		g_autoptr(AsScreenshot) sshot = NULL;

		url = g_build_filename (scr_service_url, "screenshot", priv->pkgnames[0], NULL);

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
		as_screenshot_set_active_locale (sshot, as_component_get_active_locale (cpt));

		as_screenshot_add_image (sshot, img);
		as_screenshot_set_kind (sshot, AS_SCREENSHOT_KIND_DEFAULT);

		g_object_unref (img);
		g_free (url);

		/* add thumbnail */
		url = g_build_filename (scr_service_url, "thumbnail", priv->pkgnames[0], NULL);
		img = as_image_new ();
		as_image_set_url (img, url);
		as_image_set_width (img, 160);
		as_image_set_height (img, 120);
		as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
		as_screenshot_add_image (sshot, img);

		/* add screenshot to component */
		as_component_add_screenshot (cpt, sshot);

		g_object_unref (img);
		g_free (url);
	}
}

/**
 * as_component_add_token_helper:
 */
static void
as_component_add_token_helper (AsComponent *cpt,
				const gchar *value,
				AsSearchTokenMatch match_flag,
				AsStemmer *stemmer,
				GPtrArray *tokens_out)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	AsTokenType *match_pval;
	g_autofree gchar *token_stemmed = NULL;

	/* invalid */
	if (!as_utils_search_token_valid (value))
		return;
	/* small tokens are invalid unless they are in the summary / name of the component */
	if (match_flag < AS_SEARCH_TOKEN_MATCH_SUMMARY) {
		if (strlen (value) < 3)
			return;
	}

	/* create a stemmed version of our token */
	token_stemmed = as_stemmer_stem (stemmer, value);

	/* ignore the token if it couldn't be stemmed (usually means we had a low quality token) */
	if (token_stemmed == NULL)
		return;

	/* does the token already exist */
	match_pval = g_hash_table_lookup (priv->token_cache, token_stemmed);
	if (match_pval != NULL) {
		/* add token to the output array only if it was more important than the cached one */
		if (tokens_out != NULL && *match_pval < match_flag)
			g_ptr_array_add (tokens_out,
					g_steal_pointer (&token_stemmed));

		*match_pval |= match_flag;
		return;
	}

	/* create and add */
	match_pval = g_new0 (AsTokenType, 1);
	*match_pval = match_flag;
	g_hash_table_insert (priv->token_cache,
			     g_ref_string_new_intern (token_stemmed),
			     match_pval);

	/* add to the token output array, if we have one */
	if (tokens_out != NULL)
		g_ptr_array_add (tokens_out,
				 g_steal_pointer (&token_stemmed));
}

/**
 * as_component_add_token:
 */
static void
as_component_add_token (AsComponent *cpt,
			const gchar *value,
			gboolean allow_split,
			AsSearchTokenMatch match_flag,
			GPtrArray *tokens_out)
{
	/* get global stemmer instance (it's threadsafe and should survive this invocation) */
	AsStemmer *stemmer = as_stemmer_get ();

	/* add extra tokens for names like x-plane or half-life */
	if (allow_split && g_strstr_len (value, -1, "-") != NULL) {
		guint i;
		g_auto(GStrv) split = g_strsplit (value, "-", -1);
		for (i = 0; split[i] != NULL; i++)
			as_component_add_token_helper (cpt,
							split[i],
							match_flag,
							stemmer,
							tokens_out);
	}

	/* add the whole token always, even when we split on hyphen */
	as_component_add_token_helper (cpt,
					value,
					match_flag,
					stemmer,
					tokens_out);
}

/**
 * as_component_value_tokenize:
 *
 * Split a component value string into tokens.
 */
static gboolean
as_component_value_tokenize (AsComponent *cpt, const gchar *value, gchar ***tokens_utf8, gchar ***tokens_ascii)
{
	/* tokenize with UTF-8 fallbacks */
	if (g_strstr_len (value, -1, "+") == NULL &&
	    g_strstr_len (value, -1, "-") == NULL) {
		(*tokens_utf8) = g_str_tokenize_and_fold (value,
							  as_component_get_active_locale (cpt),
							  tokens_ascii);
	}

	/* we might still be able to extract tokens if g_str_tokenize_and_fold() can't do it or +/- were found */
	if ((*tokens_utf8) == NULL) {
		g_autofree gchar *delim = NULL;
		delim = g_utf8_strdown (value, -1);
		g_strdelimit (delim, "/,.;:", ' ');
		(*tokens_utf8) = g_strsplit (delim, " ", -1);
	}

	if (tokens_ascii == NULL)
		return (*tokens_utf8) != NULL;
	else
		return ((*tokens_utf8) != NULL) || ((*tokens_ascii) != NULL);
}

/**
 * as_component_add_tokens:
 */
static void
as_component_add_tokens (AsComponent *cpt,
			 const gchar *value,
			 gboolean allow_split,
			 AsSearchTokenMatch match_flag,
			 GPtrArray *tokens_out)
{
	guint i;
	g_auto(GStrv) values_utf8 = NULL;
	g_auto(GStrv) values_ascii = NULL;

	/* sanity check */
	if (value == NULL) {
		g_critical ("trying to add NULL search token to %s",
			    as_component_get_id (cpt));
		return;
	}

	/* create a set of tokens from the value string */
	if (!as_component_value_tokenize (cpt, value, &values_utf8, &values_ascii))
		return;

	/* add each token */
	for (i = 0; values_utf8 != NULL && values_utf8[i] != NULL; i++)
		as_component_add_token (cpt, values_utf8[i], allow_split, match_flag, tokens_out);
	for (i = 0; values_ascii != NULL && values_ascii[i] != NULL; i++)
		as_component_add_token (cpt, values_ascii[i], allow_split, match_flag, tokens_out);
}

/**
 * as_component_create_token_cache_target:
 */
static void
as_component_create_token_cache_target (AsComponent *cpt, AsComponent *donor, guint flags, GPtrArray *tokens_out)
{
	AsComponentPrivate *priv = GET_PRIVATE (donor);
	const gchar *tmp;
	gchar **keywords;
	AsProvided *prov;

	/* tokenize all the data we have */
	if (priv->id != NULL && as_flags_contains (flags, AS_SEARCH_TOKEN_MATCH_ID)) {
		as_component_add_token (cpt,
					priv->id,
					FALSE,
					AS_SEARCH_TOKEN_MATCH_ID,
					tokens_out);
	}

	tmp = as_component_get_name (cpt);
	if (tmp != NULL && as_flags_contains (flags, AS_SEARCH_TOKEN_MATCH_NAME)) {
		const gchar *name_c = g_hash_table_lookup (priv->name, "C");
		as_component_add_tokens (cpt, tmp, TRUE, AS_SEARCH_TOKEN_MATCH_NAME, tokens_out);
		if (name_c != NULL && g_strcmp0 (tmp, name_c) != 0)
			as_component_add_tokens (cpt, name_c, TRUE, AS_SEARCH_TOKEN_MATCH_NAME, tokens_out);
	}

	tmp = as_component_get_summary (cpt);
	if (tmp != NULL && as_flags_contains (flags, AS_SEARCH_TOKEN_MATCH_SUMMARY)) {
		as_component_add_tokens (cpt, tmp, TRUE, AS_SEARCH_TOKEN_MATCH_SUMMARY, tokens_out);
	}

	tmp = as_component_get_description (cpt);
	if (tmp != NULL && as_flags_contains (flags, AS_SEARCH_TOKEN_MATCH_DESCRIPTION)) {
		as_component_add_tokens (cpt, tmp, TRUE, AS_SEARCH_TOKEN_MATCH_DESCRIPTION, tokens_out);
	}

	keywords = as_component_get_keywords (cpt);
	if (keywords != NULL && as_flags_contains (flags, AS_SEARCH_TOKEN_MATCH_KEYWORD)) {
		for (guint i = 0; keywords[i] != NULL; i++)
			as_component_add_tokens (cpt, keywords[i], FALSE, AS_SEARCH_TOKEN_MATCH_KEYWORD, tokens_out);
	}

	prov = as_component_get_provided_for_kind (donor, AS_PROVIDED_KIND_MIMETYPE);
	if (prov != NULL && as_flags_contains (flags, AS_SEARCH_TOKEN_MATCH_MEDIATYPE)) {
		GPtrArray *items = as_provided_get_items (prov);
		for (guint i = 0; i < items->len; i++)
			as_component_add_token (cpt,
						(const gchar*) g_ptr_array_index (items, i),
						FALSE,
						AS_SEARCH_TOKEN_MATCH_MEDIATYPE,
						tokens_out);
	}

	if (priv->pkgnames != NULL && as_flags_contains (flags, AS_SEARCH_TOKEN_MATCH_PKGNAME)) {
		for (guint i = 0; priv->pkgnames[i] != NULL; i++)
			as_component_add_token (cpt,
						priv->pkgnames[i],
						FALSE,
						AS_SEARCH_TOKEN_MATCH_PKGNAME,
						tokens_out);
	}
}

/**
 * as_component_create_token_cache:
 * @cpt: a #AsComponent instance.
 *
 * Internal API.
 */
static void
as_component_create_token_cache (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	guint flags;

	if (!g_once_init_enter (&priv->token_cache_valid))
		return;

	flags = AS_SEARCH_TOKEN_MATCH_MEDIATYPE |
		AS_SEARCH_TOKEN_MATCH_PKGNAME |
		AS_SEARCH_TOKEN_MATCH_ORIGIN |
		AS_SEARCH_TOKEN_MATCH_DESCRIPTION |
		AS_SEARCH_TOKEN_MATCH_SUMMARY |
		AS_SEARCH_TOKEN_MATCH_KEYWORD |
		AS_SEARCH_TOKEN_MATCH_NAME |
		AS_SEARCH_TOKEN_MATCH_ID;

	as_component_create_token_cache_target (cpt, cpt, flags, NULL);

	for (guint i = 0; i < priv->addons->len; i++) {
		AsComponent *donor = g_ptr_array_index (priv->addons, i);
		as_component_create_token_cache_target (cpt, donor, flags, NULL);
	}

	g_once_init_leave (&priv->token_cache_valid, TRUE);
}

/**
 * as_component_generate_tokens_for:
 * @cpt: a #AsComponent instance.
 *
 * Internal API to get tokens only for a specific aspect of this
 * component.
 */
GPtrArray*
as_component_generate_tokens_for (AsComponent *cpt, AsSearchTokenMatch token_kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autoptr(GPtrArray) tokens = g_ptr_array_new_with_free_func (g_free);

	as_component_create_token_cache_target (cpt, cpt, token_kind, tokens);

	for (guint i = 0; i < priv->addons->len; i++) {
		AsComponent *donor = g_ptr_array_index (priv->addons, i);
		as_component_create_token_cache_target (cpt, donor, token_kind, tokens);
	}

	return g_steal_pointer (&tokens);
}

/**
 * as_component_search_matches:
 * @cpt: a #AsComponent instance.
 * @term: the search term.
 *
 * Searches component data for a specific keyword.
 *
 * Returns: a match scrore, where 0 is no match and 100 is the best match.
 *
 * Since: 0.9.7
 **/
guint
as_component_search_matches (AsComponent *cpt, const gchar *term)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	AsTokenType *match_pval;
	GList *l;
	guint result = 0;
	g_autoptr(GList) keys = NULL;

	/* nothing to do */
	if (term == NULL)
		return 0;

	/* ensure the token cache is created */
	as_component_create_token_cache (cpt);

	/* find the exact match (which is more awesome than a partial match) */
	match_pval = g_hash_table_lookup (priv->token_cache, term);
	if (match_pval != NULL)
		return *match_pval << 2;

	/* need to do partial match */
	keys = g_hash_table_get_keys (priv->token_cache);
	for (l = keys; l != NULL; l = l->next) {
		const gchar *key = l->data;
		if (g_str_has_prefix (key, term)) {
			match_pval = g_hash_table_lookup (priv->token_cache, key);
			result |= *match_pval;
		}
	}

	return result;
}

/**
 * as_component_search_matches_all:
 * @cpt: a #AsComponent instance.
 * @terms: the search terms.
 *
 * Searches component data for all the specific keywords.
 *
 * Returns: a match score, where 0 is no match and larger numbers are better
 * matches.
 *
 * Since: 0.9.8
 */
guint
as_component_search_matches_all (AsComponent *cpt, gchar **terms)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	guint i;
	guint matches_sum = 0;
	guint tmp;

	priv->sort_score = 0;
	if (terms == NULL) {
		/* if the terms list is NULL, we usually had a too short search term when
		 * tokenizing the search string. In any case, we treat NULL as match-all
		 * value.
		 * (users will see a full list of all entries that way, which they will
		 * recognize as hint to make their search more narrow) */
		priv->sort_score = 1;
		return priv->sort_score;
	}

	/* do *all* search keywords match */
	for (i = 0; terms[i] != NULL; i++) {
		tmp = as_component_search_matches (cpt, terms[i]);
		if (tmp == 0)
			return 0;
		matches_sum |= tmp;
	}

	priv->sort_score = matches_sum;
	return priv->sort_score;
}

/**
 * as_component_get_search_tokens:
 * @cpt: a #AsComponent instance.
 *
 * Returns all search tokens for this component.
 *
 * Returns: (transfer container) (element-type utf8): The string search tokens
 *
 * Since: 0.9.7
 */
GPtrArray*
as_component_get_search_tokens (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	GList *l;
	GPtrArray *array;
	g_autoptr(GList) keys = NULL;

	/* ensure the token cache is created */
	as_component_create_token_cache (cpt);

	/* return all the token cache */
	keys = g_hash_table_get_keys (priv->token_cache);
	array = g_ptr_array_new_with_free_func (g_free);
	for (l = keys; l != NULL; l = l->next)
		g_ptr_array_add (array, g_strdup (l->data));

	return array;
}

/**
 * as_component_set_value_flags:
 * @cpt: a #AsComponent instance.
 * @flags: #AsValueFlags to set on @cpt.
 *
 */
void
as_component_set_value_flags (AsComponent *cpt, AsValueFlags flags)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	priv->value_flags = flags;
}

/**
 * as_component_get_value_flags:
 * @cpt: a #AsComponent instance.
 *
 * Returns: The #AsValueFlags that are set on @cpt.
 *
 */
AsValueFlags
as_component_get_value_flags (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->value_flags;
}

/**
 * as_component_has_desktop_group:
 *
 * Internal helper method for %as_component_is_member_of_category
 */
static gboolean
as_component_has_desktop_group (AsComponent *cpt, const gchar *desktop_group)
{
	guint i;
	g_auto(GStrv) split = g_strsplit (desktop_group, "::", -1);
	for (i = 0; split[i] != NULL; i++) {
		if (!as_component_has_category (cpt, split[i]))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_component_is_member_of_category:
 * @cpt: a #AsComponent instance.
 * @category: The category to test.
 *
 * Test if the component @cpt is a member of category @category.
 */
gboolean
as_component_is_member_of_category (AsComponent *cpt, AsCategory *category)
{
	GPtrArray *cdesktop_groups;
	guint i;

	cdesktop_groups = as_category_get_desktop_groups (category);
	for (i = 0; i < cdesktop_groups->len; i++) {
		const gchar *cdg_name = (const gchar*) g_ptr_array_index (cdesktop_groups, i);
		if (as_component_has_desktop_group (cpt, cdg_name))
			return TRUE;
	}

	return FALSE;
}

/**
 * as_component_set_ignored:
 * @cpt: a #AsComponent instance.
 * @ignore: %TRUE if the metadata in @cpt should be ignored.
 *
 * Since: 0.10.2
 */
void
as_component_set_ignored (AsComponent *cpt, gboolean ignore)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	priv->ignored = ignore;
}

/**
 * as_component_is_ignored:
 * @cpt: a #AsComponent instance.
 *
 * Returns: Whether this component's metadata should be ignored.
 *
 * Since: 0.10.2
 */
gboolean
as_component_is_ignored (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->ignored;
}

/**
 * as_component_get_custom:
 * @cpt: An #AsComponent.
 *
 * Returns: (transfer none): Hash table of custom user defined data fields.
 *
 * Since: 0.10.5
 */
GHashTable*
as_component_get_custom (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->custom;
}

/**
 * as_component_get_custom_value:
 * @cpt: An #AsComponent.
 * @key: Field name.
 *
 * Retrieve value for a custom data entry with the given key.
 *
 * Since: 0.10.5
 */
const gchar*
as_component_get_custom_value (AsComponent *cpt, const gchar *key)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (key == NULL)
		return NULL;
	return g_hash_table_lookup (priv->custom, key);
}

/**
 * as_component_insert_custom_value:
 * @cpt: An #AsComponent.
 * @key: Key name.
 * @value: A string value.
 *
 * Add a key and value pair to the custom data table.
 *
 * Returns: %TRUE if the key did not exist yet.
 *
 * Since: 0.10.5
 */
gboolean
as_component_insert_custom_value (AsComponent *cpt, const gchar *key, const gchar *value)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (key == NULL)
		return FALSE;
	return g_hash_table_insert (priv->custom,
				    g_ref_string_new_intern (key),
				    g_ref_string_new_intern (value));
}

/**
 * as_component_get_reviews:
 * @cpt: a #AsComponent instance.
 *
 * Gets any reviews associated with the component.
 *
 * Returns: (element-type AsReview) (transfer none): an array
 *
 * Since: 0.14.0
 **/
GPtrArray*
as_component_get_reviews (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->reviews;
}

/**
 * as_component_add_review:
 * @cpt: a #AsComponent instance.
 * @review: a #AsReview instance.
 *
 * Adds a user review to a software component.
 *
 * Since: 0.14.0
 **/
void
as_component_add_review (AsComponent *cpt, AsReview *review)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_add (priv->reviews, g_object_ref (review));
}

/**
 * as_component_get_content_ratings:
 * @cpt: a #AsComponent instance.
 *
 * Gets all content ratings defined for this software.
 *
 * Returns: (element-type AsContentRating) (transfer none): an array
 *
 * Since: 0.11.0
 **/
GPtrArray*
as_component_get_content_ratings (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->content_ratings;
}

/**
 * as_component_get_content_rating:
 * @cpt: a #AsComponent instance.
 * @kind: a ratings kind, e.g. "oars-1.0"
 *
 * Gets a content ratings of a specific type that are defined for this component.
 *
 * Returns: (transfer none) (nullable): a #AsContentRating or %NULL if not found
 *
 * Since: 0.11.0
 **/
AsContentRating*
as_component_get_content_rating (AsComponent *cpt, const gchar *kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	guint i;

	for (i = 0; i < priv->content_ratings->len; i++) {
		AsContentRating *content_rating = AS_CONTENT_RATING (g_ptr_array_index (priv->content_ratings, i));
		if (g_strcmp0 (as_content_rating_get_kind (content_rating), kind) == 0)
			return content_rating;
	}
	return NULL;
}

/**
 * as_component_add_content_rating:
 * @cpt: a #AsComponent instance.
 * @content_rating: a #AsContentRating instance.
 *
 * Adds a content rating to this component.
 *
 * Since: 0.11.0
 **/
void
as_component_add_content_rating (AsComponent *cpt, AsContentRating *content_rating)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_add (priv->content_ratings,
			 g_object_ref (content_rating));
}

/**
 * as_component_get_launchable:
 * @cpt: a #AsComponent instance.
 * @kind: a launch kind, e.g. %AS_LAUNCHABLE_KIND_DESKTOP_ID
 *
 * Gets a #AsLaunchable of a specific type that contains launchable entries for
 * this component.
 *
 * Returns: (transfer none) (nullable): a #AsLaunchable or %NULL if not found
 *
 * Since: 0.11.0
 **/
AsLaunchable*
as_component_get_launchable (AsComponent *cpt, AsLaunchableKind kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	guint i;

	for (i = 0; i < priv->launchables->len; i++) {
		AsLaunchable *launch = AS_LAUNCHABLE (g_ptr_array_index (priv->launchables, i));
		if (as_launchable_get_kind (launch) == kind)
			return launch;
	}
	return NULL;
}

/**
 * as_component_get_launchables:
 * @cpt: a #AsComponent instance.
 *
 * Returns: (transfer none) (element-type AsLaunchable): an array
 *
 * Since: 0.11.0
 **/
GPtrArray*
as_component_get_launchables (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->launchables;
}

/**
 * as_component_add_launchable:
 * @cpt: a #AsComponent instance.
 * @launchable: a #AsLaunchable instance.
 *
 * Adds a #AsLaunchable containing launchables entries for this component.
 *
 * Since: 0.11.0
 **/
void
as_component_add_launchable (AsComponent *cpt, AsLaunchable *launchable)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_return_if_fail (launchable != NULL);
	g_ptr_array_add (priv->launchables,
			 g_object_ref (launchable));
}

/**
 * as_component_get_requires:
 * @cpt: a #AsComponent instance.
 *
 * Get an array of items that are required by this component.
 *
 * Returns: (transfer none) (element-type AsRelation): an array
 *
 * Since: 0.12.0
 **/
GPtrArray*
as_component_get_requires (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->requires;
}

/**
 * as_component_get_recommends:
 * @cpt: a #AsComponent instance.
 *
 * Get an array of items that are recommended by this component.
 *
 * Returns: (transfer none) (element-type AsRelation): an array
 *
 * Since: 0.12.0
 **/
GPtrArray*
as_component_get_recommends (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->recommends;
}

/**
 * as_component_get_supports:
 * @cpt: a #AsComponent instance.
 *
 * Get an array of items that are supported by this component,
 * e.g. to indicate support for a specific piece of hardware.
 *
 * Returns: (transfer none) (element-type AsRelation): an array
 *
 * Since: 0.15.0
 **/
GPtrArray*
as_component_get_supports (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->supports;
}

/**
 * as_component_add_relation:
 * @cpt: a #AsComponent instance.
 * @relation: a #AsRelation instance.
 *
 * Adds a #AsRelation to set a recommends or requires relation of
 * component @cpt on the item mentioned in the #AsRelation.
 *
 * Since: 0.12.0
 **/
void
as_component_add_relation (AsComponent *cpt, AsRelation *relation)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	AsRelationKind kind = as_relation_get_kind (relation);

	if (kind == AS_RELATION_KIND_REQUIRES) {
		g_ptr_array_add (priv->requires,
				g_object_ref (relation));
	} else if (kind == AS_RELATION_KIND_RECOMMENDS) {
		g_ptr_array_add (priv->recommends,
				g_object_ref (relation));
	} else if (kind == AS_RELATION_KIND_SUPPORTS) {
		g_ptr_array_add (priv->supports,
				g_object_ref (relation));
	} else {
		g_warning ("Tried to add relation of unknown kind to component %s", priv->data_id);
	}
}

/**
 * as_component_get_replaces:
 * @cpt: a #AsComponent instance.
 *
 * Get a list of component IDs of components that this software replaces entirely.
 *
 * Returns: (transfer none) (element-type utf8): an array of component-IDs
 */
GPtrArray*
as_component_get_replaces (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->replaces == NULL)
		priv->replaces = g_ptr_array_new_with_free_func (g_free);
	return priv->replaces;
}

/**
 * as_component_add_replaces:
 * @cpt: a #AsComponent instance.
 * @cid: an AppStream component ID
 *
 * Add the component ID of a component that gets replaced by the current component.
 **/
void
as_component_add_replaces (AsComponent *cpt, const gchar *cid)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_return_if_fail (cid != NULL);
	if (priv->replaces == NULL)
		priv->replaces = g_ptr_array_new_with_free_func (g_free);

	g_ptr_array_add (priv->replaces,
			 g_strdup (cid));
}

/**
 * as_component_get_agreements:
 * @cpt: an #AsComponent instance.
 *
 * Get a list of all agreements registered with this software component.
 *
 * Returns: (transfer none) (element-type AsAgreement): An array of #AsAgreement.
 *
 * Since: 0.15.0
 **/
GPtrArray*
as_component_get_agreements (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->agreements;
}

/**
 * as_component_add_agreement:
 * @cpt: a #AsComponent instance.
 * @agreement: an #AsAgreement instance.
 *
 * Adds an agreement to the software component.
 *
 * Since: 0.12.1
 **/
void
as_component_add_agreement (AsComponent *cpt, AsAgreement *agreement)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_add (priv->agreements, g_object_ref (agreement));
}

/**
 * as_component_get_agreement_by_kind:
 * @cpt: a #AsComponent instance.
 * @kind: an agreement kind, e.g. %AS_AGREEMENT_KIND_EULA
 *
 * Gets an agreement the component has specified for the particular kind.
 *
 * Returns: (transfer none) (nullable): a #AsAgreement or %NULL for not found
 *
 * Since: 0.12.1
 **/
AsAgreement*
as_component_get_agreement_by_kind (AsComponent *cpt, AsAgreementKind kind)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	guint i;

	for (i = 0; i < priv->agreements->len; i++) {
		AsAgreement *agreement = AS_AGREEMENT (g_ptr_array_index (priv->agreements, i));
		if (as_agreement_get_kind (agreement) == kind)
			return agreement;
	}
	return NULL;
}

/**
 * as_component_get_branding:
 * @cpt: an #AsComponent instance.
 *
 * Get the branding associated with this component, or %NULL
 * in case this component has no special branding.
 *
 * Returns: (transfer none) (nullable): An #AsBranding.
 *
 * Since: 0.15.2
 **/
AsBranding*
as_component_get_branding (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->branding;
}

/**
 * as_component_set_branding:
 * @cpt: a #AsComponent instance.
 * @branding: an #AsBranding instance.
 *
 * Set branding for this component.
 *
 * Since: 0.15.2
 **/
void
as_component_set_branding (AsComponent *cpt, AsBranding *branding)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (branding == priv->branding)
		return;
	if (priv->branding != NULL)
		g_object_unref (priv->branding);
	priv->branding = g_object_ref (branding);
}

/**
 * as_component_is_free:
 * @cpt: a #AsComponent instance.
 *
 * Returns %TRUE if this component is free and open source software.
 * To determine this status, this function will check if it comes
 * from a vetted free-software-only source or whether its licenses
 * are only free software licenses.
 *
 * Returns: %TRUE if this component is free software.
 *
 * Since: 0.15.5
 */
gboolean
as_component_is_free (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	gboolean is_free = as_license_is_free_license (priv->project_license);
	if (is_free)
		return TRUE;

	/* The license check yielded a non-free license, which is also returned
	 * if no license was set. We also need to check the repository origin
	 * for packaged components
	 * TODO: We probably want a lot of this logic in a singleton, so we don't parse
	 * text files too often. */
	if (as_is_empty (priv->origin))
		return FALSE;
	if (as_utils_get_component_bundle_kind (cpt) == AS_BUNDLE_KIND_PACKAGE) {
		gboolean ret;
		g_autofree gchar *distro_id = NULL;
		g_auto(GStrv) origin_globs = NULL;
		g_autoptr(GKeyFile) kf = g_key_file_new ();
		ret = g_key_file_load_from_file (kf, AS_CONFIG_NAME, G_KEY_FILE_NONE, NULL);
		if (!ret) {
			g_debug ("Unable to read configuration file %s", AS_CONFIG_NAME);
			return FALSE;
		}
#if GLIB_CHECK_VERSION(2,64,0)
		distro_id = g_get_os_info (G_OS_INFO_KEY_ID);
		if (distro_id == NULL) {
			g_warning ("Unable to determine the ID for this operating system.");
			return FALSE;
		}
#else
		distro_id = g_strdup ("general");
#endif
		origin_globs = g_key_file_get_string_list (kf, distro_id, "FreeRepos", NULL, NULL);
		if (origin_globs == NULL)
			return FALSE;

		/* return a free component if any of the origin wildcards matches */
		for (guint i = 0; origin_globs[i] != NULL; i++) {
			if (fnmatch (origin_globs[i], priv->origin, FNM_NOESCAPE) == 0)
				return TRUE;
		}
	}

	return is_free;
}

/**
 * as_component_get_context:
 * @cpt: a #AsComponent instance.
 *
 * Get the #AsContext associated with this component.
 * This function may return %NULL if no context is set
 * (which will be the case if the component was not loaded from
 * a file or cache but constructed in memory).
 *
 * Returns: (transfer none) (nullable): the associated #AsContext or %NULL
 *
 * Since: 0.11.2
 */
AsContext*
as_component_get_context (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	return priv->context;
}

/**
 * as_component_set_context:
 * @cpt: a #AsComponent instance.
 * @context: the #AsContext.
 *
 * Sets the document context this component is associated
 * with.
 *
 * Since: 0.11.2
 */
void
as_component_set_context (AsComponent *cpt, AsContext *context)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	if (priv->context != NULL)
		g_object_unref (priv->context);
	priv->context = g_object_ref (context);

	/* reset individual properties, so the new context overrides them */
	as_ref_string_assign_safe (&priv->active_locale_override, NULL);
	as_ref_string_assign_safe (&priv->origin, NULL);

	g_free (priv->arch);
	priv->arch = NULL;
}

/**
 * as_copy_l10n_hashtable_hfunc:
 */
static void
as_copy_l10n_hashtable_hfunc (gpointer key, gpointer value, gpointer user_data)
{
	GHashTable *dest = (GHashTable*) user_data;
	g_hash_table_insert (dest, g_ref_string_acquire (key), g_strdup (value));
}

/**
 * as_copy_l10n_hashtable:
 *
 * Helper for as_component_merge_with_mode()
 */
static void
as_copy_l10n_hashtable (GHashTable *src, GHashTable *dest)
{
	/* don't copy if there is nothing to copy */
	if (g_hash_table_size (src) <= 0)
		return;

	/* clear our destination table */
	g_hash_table_remove_all (dest);

	/* copy */
	g_hash_table_foreach (src,
			      &as_copy_l10n_hashtable_hfunc,
			      dest);
}

/**
 * as_copy_gobject_array:
 *
 * Helper for as_component_merge_with_mode()
 *
 * NOTE: Only the object references are copied.
 */
static void
as_copy_gobject_array (GPtrArray *src, GPtrArray *dest)
{
	guint i;

	/* don't copy if there is nothing to copy */
	if (src->len <= 0)
		return;

	/* clear our destination table */
	g_ptr_array_remove_range (dest, 0, dest->len);

	/* copy */
	for (i = 0; i < src->len; i++) {
		GObject *obj = G_OBJECT (g_ptr_array_index (src, i));
		g_ptr_array_add (dest,
				 g_object_ref (obj));
	}
}

/**
 * as_component_merge_with_mode:
 * @cpt: An #AsComponent.
 * @source: An #AsComponent to merge into @cpt.
 * @merge_kind: How
 *
 * Merge data from component @source into @cpt, respecting the
 * merge parameter @merge_kind.
 */
void
as_component_merge_with_mode (AsComponent *cpt, AsComponent *source, AsMergeKind merge_kind)
{
	guint i;
	AsComponent *dest_cpt = cpt;
	AsComponent *src_cpt = source;
	AsComponentPrivate *dest_priv = GET_PRIVATE (dest_cpt);
	AsComponentPrivate *src_priv = GET_PRIVATE (src_cpt);

	if (merge_kind == AS_MERGE_KIND_REMOVE_COMPONENT) {
		/* we can't remove ourselves from the pool, so this merge action is handled separately */
		return;
	}

	/* FIXME/TODO: We need to merge more attributes */

	/* merge stuff in append mode */
	if (merge_kind == AS_MERGE_KIND_APPEND) {
		GPtrArray *suggestions;
		GPtrArray *cats;

		/* merge categories */
		cats = as_component_get_categories (src_cpt);
		if (cats->len > 0) {
			g_autoptr(GHashTable) cat_table = NULL;
			GPtrArray *dest_categories;

			cat_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
			for (i = 0; i < cats->len; i++) {
				const gchar *cat = (const gchar*) g_ptr_array_index (cats, i);
				g_hash_table_add (cat_table, g_strdup (cat));
			}

			dest_categories = as_component_get_categories (dest_cpt);
			if (dest_categories->len > 0) {
				for (i = 0; i < dest_categories->len; i++) {
					const gchar *cat = (const gchar*) g_ptr_array_index (dest_categories, i);
					g_hash_table_add (cat_table, g_strdup (cat));
				}
			}

			g_ptr_array_set_size (dest_categories, 0);
			as_hash_table_string_keys_to_array (cat_table, dest_categories);
		}

		/* merge suggestions */
		suggestions = as_component_get_suggested (src_cpt);
		if (suggestions != NULL) {
			for (i = 0; i < suggestions->len; i++) {
				as_component_add_suggested (dest_cpt,
						    AS_SUGGESTED (g_ptr_array_index (suggestions, i)));
			}
		}

		/* merge icons */
		for (i = 0; i < src_priv->icons->len; i++) {
			AsIcon *icon = AS_ICON (g_ptr_array_index (src_priv->icons, i));

			/* this function will not replace existing icons */
			as_component_add_icon (dest_cpt, icon);
		}

		/* names */
		if (g_hash_table_size (dest_priv->name) <= 0)
			as_copy_l10n_hashtable (src_priv->name, dest_priv->name);

		/* summary */
		if (g_hash_table_size (dest_priv->summary) <= 0)
			as_copy_l10n_hashtable (src_priv->summary, dest_priv->summary);

		/* description */
		if (g_hash_table_size (dest_priv->description) <= 0)
			as_copy_l10n_hashtable (src_priv->description, dest_priv->description);
	}

	/* merge stuff in replace mode */
	if (merge_kind == AS_MERGE_KIND_REPLACE) {
		/* names */
		as_copy_l10n_hashtable (src_priv->name, dest_priv->name);

		/* summary */
		as_copy_l10n_hashtable (src_priv->summary, dest_priv->summary);

		/* description */
		as_copy_l10n_hashtable (src_priv->description, dest_priv->description);

		/* merge package names */
		if ((src_priv->pkgnames != NULL) && (src_priv->pkgnames[0] != NULL))
			as_component_set_pkgnames (dest_cpt, src_priv->pkgnames);

		/* merge bundles */
		if (as_component_has_bundle (src_cpt))
			as_component_set_bundles_array (dest_cpt, as_component_get_bundles (src_cpt));

		/* merge icons */
		as_copy_gobject_array (src_priv->icons, src_priv->icons);

		/* merge provided items */
		as_copy_gobject_array (src_priv->provided, src_priv->provided);
	}

	g_debug ("Merged data for '[%i] %s' <<- '[%i] %s'",
		 as_component_get_origin_kind (dest_cpt), as_component_get_data_id (dest_cpt),
		 as_component_get_origin_kind (src_cpt),  as_component_get_data_id (src_cpt));
}

/**
 * as_component_merge:
 * @cpt: An #AsComponent.
 * @source: An #AsComponent with a merge type.
 *
 * Merge data from component @source into @cpt, respecting the
 * merge mode of @source.
 *
 * Returns: %TRUE if the merge was successful.
 */
gboolean
as_component_merge (AsComponent *cpt, AsComponent *source)
{
	AsMergeKind merge_kind = as_component_get_merge_kind (source);
	g_return_val_if_fail (merge_kind != AS_MERGE_KIND_NONE, FALSE);

	as_component_merge_with_mode (cpt, source, merge_kind);
	return TRUE;
}

/**
 * as_component_set_kind_from_node:
 */
static void
as_component_set_kind_from_node (AsComponent *cpt, xmlNode *node)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autofree gchar *cpttype = NULL;

	/* find out which kind of component we are dealing with */
	cpttype = as_xml_get_prop_value (node, "type");
	if ((cpttype == NULL) || (g_strcmp0 (cpttype, "generic") == 0)) {
		priv->kind = AS_COMPONENT_KIND_GENERIC;
	} else {
		priv->kind = as_component_kind_from_string (cpttype);
		if (priv->kind == AS_COMPONENT_KIND_UNKNOWN)
			g_debug ("Found unknown component type: %s", cpttype);
	}
}

/**
 * as_component_load_provides_from_xml:
 */
static void
as_component_load_provides_from_xml (AsComponent *cpt, xmlNode *node)
{
	xmlNode *iter;
	gchar *node_name;

	for (iter = node->children; iter != NULL; iter = iter->next) {
		g_autofree gchar *content = NULL;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		content = as_xml_get_node_value (iter);
		if (content == NULL)
			continue;

		if (g_strcmp0 (node_name, "id") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_ID, content);
		} else if (g_strcmp0 (node_name, "mediatype") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_MEDIATYPE, content);
		} else if (g_strcmp0 (node_name, "library") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_LIBRARY, content);
		} else if (g_strcmp0 (node_name, "binary") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_BINARY, content);
		} else if (g_strcmp0 (node_name, "font") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FONT, content);
		} else if (g_strcmp0 (node_name, "modalias") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_MODALIAS, content);
		} else if (g_strcmp0 (node_name, "firmware") == 0) {
			g_autofree gchar *fw_type = NULL;
			fw_type = as_xml_get_prop_value (iter, "type");
			if (g_strcmp0 (fw_type, "runtime") == 0)
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FIRMWARE_RUNTIME, content);
			else if (g_strcmp0 (fw_type, "flashed") == 0)
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FIRMWARE_FLASHED, content);
		} else if (g_strcmp0 (node_name, "python2") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_PYTHON_2, content);
		} else if (g_strcmp0 (node_name, "python3") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_PYTHON, content);
		} else if (g_strcmp0 (node_name, "dbus") == 0) {
			g_autofree gchar *dbus_type = as_xml_get_prop_value (iter, "type");

			if (g_strcmp0 (dbus_type, "system") == 0)
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_DBUS_SYSTEM, content);
			else if ((g_strcmp0 (dbus_type, "user") == 0) || (g_strcmp0 (dbus_type, "session") == 0))
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_DBUS_USER, content);
		}
	}
}

/**
 * as_component_xml_parse_languages_node:
 */
static void
as_component_xml_parse_languages_node (AsComponent* cpt, xmlNode* node)
{
	xmlNode *iter;

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "lang") == 0) {
			guint64 percentage = 0;
			g_autofree gchar *locale = NULL;
			g_autofree gchar *prop = NULL;

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "percentage");
			if (prop != NULL)
				percentage = g_ascii_strtoll (prop, NULL, 10);

			locale = as_xml_get_node_value (iter);
			as_component_add_language (cpt, locale, percentage);
		}
	}
}

/**
 * as_component_load_launchable_from_xml:
 *
 * Loads <launchable/> data from an XML node.
 **/
static void
as_component_load_launchable_from_xml (AsComponent *cpt, xmlNode *node)
{
	AsLaunchableKind lkind;
	AsLaunchable *launchable;
	g_autofree gchar *lstr = NULL;
	g_autofree gchar *value = NULL;

	lstr = as_xml_get_prop_value (node, "type");
	lkind = as_launchable_kind_from_string (lstr);
	if (lkind == AS_LAUNCHABLE_KIND_UNKNOWN)
		return;

	launchable = as_component_get_launchable (cpt, lkind);
	if (launchable == NULL) {
		launchable = as_launchable_new ();
		as_launchable_set_kind (launchable, lkind);
		as_component_add_launchable (cpt, launchable);
		g_object_unref (launchable);
	}

	value = as_xml_get_node_value (node);
	if (value == NULL)
		return;
	as_launchable_add_entry (launchable, value);
}

/**
 * as_component_load_relations_from_xml:
 */
static void
as_component_load_relations_from_xml (AsComponent *cpt, AsContext *ctx, xmlNode *node, AsRelationKind kind)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		g_autoptr(AsRelation) relation = NULL;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		relation = as_relation_new ();
		as_relation_set_kind (relation, kind);

		if (as_relation_load_from_xml (relation, ctx, iter, NULL))
			as_component_add_relation (cpt, relation);
	}
}

/**
 * as_component_load_keywords_from_xml:
 */
static void
as_component_load_keywords_from_xml (AsComponent *cpt, AsContext *ctx, xmlNode *node)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	if (as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) {
		GHashTableIter ht_iter;
		gpointer ht_key;
		gpointer ht_value;
		g_autoptr(GHashTable) temp_kw = g_hash_table_new_full (g_str_hash,
								       g_str_equal,
								       NULL,
								       (GDestroyNotify) g_ptr_array_unref);

		for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
			g_autofree gchar *lang = NULL;
			GPtrArray *array = NULL;

			if (iter->type != XML_ELEMENT_NODE)
				continue;
			lang = as_xml_get_node_locale_match (ctx, iter);
			if (lang == NULL)
				continue;

			array = g_hash_table_lookup (temp_kw, lang);
			if (array == NULL) {
				array = g_ptr_array_new ();
				g_hash_table_insert (temp_kw,
						     g_ref_string_new_intern (lang),
						     array);
			}

			g_ptr_array_add (array,	as_xml_get_node_value (iter));
		}

		/* Deconstruct hash table and add contents to internal table.
		 * We try to duplicate as little memory as possible, as this function
		 * may be called quite often when processing many metainfo files.
		 * Note the the PtrArray with the terms will not auto-free its contents,
		 * and the hash table does not own its key and will not free it wither. */
		g_hash_table_iter_init (&ht_iter, temp_kw);
		while (g_hash_table_iter_next (&ht_iter, &ht_key, &ht_value)) {
			GPtrArray *array = (GPtrArray*) ht_value;
			gchar **strv = g_new0 (gchar*, array->len + 1);
			for (guint i = 0; i < array->len; i++)
				strv[i] = (gchar*) g_ptr_array_index (array, i);

			g_hash_table_insert (priv->keywords,
					     ht_key, /* GRefString */
					     strv);
		}
	} else {
		g_autofree gchar *lang = as_xml_get_node_locale_match (ctx, node);
		if (lang != NULL) {
			g_auto(GStrv) kw_array = NULL;
			kw_array = as_xml_get_children_as_strv (node, "keyword");
			as_component_set_keywords (cpt, kw_array, lang);
		}
	}
}

/**
 * as_component_releases_sort_cb:
 *
 * Callback for releases #GPtrArray sorting.
 *
 * NOTE: We sort in descending order here, so the most recent
 * release ends up at the top of the list.
 */
static gint
as_component_releases_sort_cb (gconstpointer a, gconstpointer b)
{
	AsRelease **rel1 = (AsRelease **) a;
	AsRelease **rel2 = (AsRelease **) b;
	gint ret;

	/* compare version strings */
	ret = as_release_vercmp (*rel1, *rel2);
	if (ret == 0)
		return 0;
	return (ret >= 1)? -1 : 1;
}

/**
 * as_component_sort_releases:
 *
 * Sort releases by their release date, starting with the most recent release.
 */
static void
as_component_sort_releases (AsComponent *cpt)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_ptr_array_sort (priv->releases, as_component_releases_sort_cb);
}

/**
 * as_component_load_from_xml:
 * @cpt: An #AsComponent.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads data from an XML node.
 **/
gboolean
as_component_load_from_xml (AsComponent *cpt, AsContext *ctx, xmlNode *node, GError **error)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	const gchar *node_name;
	g_autoptr(GPtrArray) pkgnames = NULL;
	g_autofree gchar *priority_str = NULL;
	g_autofree gchar *merge_str = NULL;
	g_autofree gchar *date_eol_str = NULL;

	/* sanity check */
	if ((g_strcmp0 ((gchar*) node->name, "component") != 0) && (g_strcmp0 ((gchar*) node->name, "application") != 0)) {
		g_set_error (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"Expected 'component' tag, but got '%s' instead.", (gchar*) node->name);
		return FALSE;
	}

	pkgnames = g_ptr_array_new_with_free_func (g_free);

	/* set component kind */
	as_component_set_kind_from_node (cpt, node);

	/* end-of-life date for this component */
	date_eol_str = as_xml_get_prop_value (node, "date_eol");
	if (date_eol_str != NULL) {
		g_free (priv->date_eol);
		priv->date_eol = g_steal_pointer (&date_eol_str);
	}

	/* set the priority for this component */
	priority_str = (gchar*) xmlGetProp (node, (xmlChar*) "priority");
	if (priority_str == NULL) {
		priv->priority = as_context_get_priority (ctx);
	} else {
		priv->priority = g_ascii_strtoll (priority_str, NULL, 10);
	}

	/* set the merge method for this component */
	merge_str = (gchar*) xmlGetProp (node, (xmlChar*) "merge");
	if (merge_str != NULL) {
		priv->merge_kind = as_merge_kind_from_string (merge_str);
	}

	/* set context for this component */
	as_component_set_context (cpt, ctx);

	/* clear any existing descriptions */
	g_hash_table_remove_all (priv->description);

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		AsTag tag_id;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (const gchar*) iter->name;
		tag_id = as_xml_tag_from_string (node_name);

		if (tag_id == AS_TAG_ID) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			as_component_set_id (cpt, content);
			if ((as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) && (priv->kind == AS_COMPONENT_KIND_GENERIC)) {
				/* parse legacy component type information */
				as_component_set_kind_from_node (cpt, iter);
			}
		} else if (tag_id == AS_TAG_PKGNAME) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			if (content != NULL)
				g_ptr_array_add (pkgnames, g_strdup (content));
		} else if (tag_id == AS_TAG_SOURCE_PKGNAME) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			as_component_set_source_pkgname (cpt, content);
		} else if (tag_id == AS_TAG_NAME) {
			g_autofree gchar *lang = NULL;
			g_autofree gchar *content = as_xml_get_node_value (iter);
			lang = as_xml_get_node_locale_match (ctx, iter);

			if (lang != NULL)
				as_component_set_name (cpt, content, lang);
		} else if (tag_id == AS_TAG_SUMMARY) {
			g_autofree gchar *lang = NULL;
			g_autofree gchar *content = as_xml_get_node_value (iter);
			lang = as_xml_get_node_locale_match (ctx, iter);
			if (lang != NULL)
				as_component_set_summary (cpt, content, lang);
		} else if (tag_id == AS_TAG_DESCRIPTION) {
			if (as_context_get_style (ctx) == AS_FORMAT_STYLE_COLLECTION) {
				g_autofree gchar *lang = as_xml_get_node_locale_match (ctx, iter);
				/* for collection XML, the "description" tag has a language property, so parsing it is simple */
				if (lang != NULL) {
					gchar *desc;
					desc = as_xml_dump_node_children (iter);
					as_component_set_description (cpt, desc, lang);
					g_free (desc);
				}
			} else {
				as_xml_parse_metainfo_description_node (ctx, iter, priv->description);
			}
		} else if (tag_id == AS_TAG_ICON) {
			g_autoptr(AsIcon) icon = NULL;
			g_autofree gchar *content = as_xml_get_node_value (iter);
			if (content == NULL)
				continue;
			icon = as_icon_new ();
			if (as_icon_load_from_xml (icon, ctx, iter, NULL))
				as_component_add_icon (cpt, icon);
		} else if (tag_id == AS_TAG_URL) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			if (content != NULL) {
				g_autofree gchar *urltype_str = NULL;
				AsUrlKind url_kind;
				urltype_str = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
				url_kind = as_url_kind_from_string (urltype_str);
				if (url_kind != AS_URL_KIND_UNKNOWN)
					as_component_add_url (cpt, url_kind, content);
			}
		} else if (tag_id == AS_TAG_CATEGORIES) {
			as_xml_add_children_values_to_array (iter,
							     "category",
							     priv->categories);
		} else if (tag_id == AS_TAG_KEYWORDS) {
			as_component_load_keywords_from_xml (cpt, ctx, iter);
		} else if (tag_id == AS_TAG_MIMETYPES) {
			g_autoptr(GPtrArray) mime_list = NULL;
			guint i;

			/* Mimetypes are essentially provided interfaces, that's why they belong into AsProvided.
			 * However, due to historic reasons, the spec has an own toplevel tag for them, so we need
			 * to parse them here. */
			mime_list = as_xml_get_children_as_string_list (iter, "mimetype");
			for (i = 0; i < mime_list->len; i++) {
				as_component_add_provided_item (cpt,
								AS_PROVIDED_KIND_MIMETYPE,
								(const gchar*) g_ptr_array_index (mime_list, i));
			}
		} else if (tag_id == AS_TAG_PROVIDES) {
			as_component_load_provides_from_xml (cpt, iter);
		} else if (tag_id == AS_TAG_SCREENSHOTS) {
			xmlNode *iter2;

			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (iter2->type != XML_ELEMENT_NODE)
					continue;
				if (g_strcmp0 ((const gchar*) iter2->name, "screenshot") == 0) {
					g_autoptr(AsScreenshot) screenshot = as_screenshot_new ();
					if (as_screenshot_load_from_xml (screenshot, ctx, iter2, NULL))
						as_component_add_screenshot (cpt, screenshot);
				}
			}
		} else if (tag_id == AS_TAG_METADATA_LICENSE) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			as_component_set_metadata_license (cpt, content);
		} else if (tag_id == AS_TAG_PROJECT_LICENSE) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			as_component_set_project_license (cpt, content);
		} else if (tag_id == AS_TAG_PROJECT_GROUP) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			as_component_set_project_group (cpt, content);
		} else if (tag_id == AS_TAG_DEVELOPER_NAME) {
			g_autofree gchar *lang = NULL;
			g_autofree gchar *content = as_xml_get_node_value (iter);
			lang = as_xml_get_node_locale_match (ctx, iter);
			if (lang != NULL)
				as_component_set_developer_name (cpt, content, lang);
		} else if (tag_id == AS_TAG_COMPULSORY_FOR_DESKTOP) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			if (content != NULL)
				as_component_set_compulsory_for_desktop (cpt, content);
		} else if (tag_id == AS_TAG_RELEASES) {
			for (xmlNode *iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (iter2->type != XML_ELEMENT_NODE)
					continue;
				if (g_strcmp0 ((const gchar*) iter2->name, "release") == 0) {
					g_autoptr(AsRelease) release = as_release_new ();
					if (as_release_load_from_xml (release, ctx, iter2, NULL))
						g_ptr_array_add (priv->releases, g_steal_pointer (&release));
				}
			}
		} else if (tag_id == AS_TAG_EXTENDS) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			as_component_add_extends (cpt, content);
		} else if (tag_id == AS_TAG_LANGUAGES) {
			as_component_xml_parse_languages_node (cpt, iter);
		} else if (tag_id == AS_TAG_LAUNCHABLE) {
			as_component_load_launchable_from_xml (cpt, iter);
		} else if (tag_id == AS_TAG_BUNDLE) {
			g_autoptr(AsBundle) bundle = as_bundle_new ();
			if (as_bundle_load_from_xml (bundle, ctx, iter, NULL))
				as_component_add_bundle (cpt, bundle);
		} else if (tag_id == AS_TAG_TRANSLATION) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			if (content != NULL) {
				g_autoptr(AsTranslation) tr = as_translation_new ();
				if (as_translation_load_from_xml (tr, ctx, iter, NULL))
					as_component_add_translation (cpt, tr);
			}
		} else if (tag_id == AS_TAG_SUGGESTS) {
			g_autoptr(AsSuggested) suggested = as_suggested_new ();
			if (as_suggested_load_from_xml (suggested, ctx, iter, NULL))
				as_component_add_suggested (cpt, suggested);
		} else if (tag_id == AS_TAG_CUSTOM) {
			as_xml_parse_custom_node (iter, as_component_get_custom (cpt));
		} else if (tag_id == AS_TAG_CONTENT_RATING) {
			g_autoptr(AsContentRating) ctrating = as_content_rating_new ();
			if (as_content_rating_load_from_xml (ctrating, ctx, iter, NULL))
				as_component_add_content_rating (cpt, ctrating);
		} else if (tag_id == AS_TAG_REPLACES) {
			if (priv->replaces != NULL)
				g_ptr_array_unref (priv->replaces);
			priv->replaces = as_xml_get_children_as_string_list (iter, "id");
		} else if (tag_id == AS_TAG_REQUIRES) {
			as_component_load_relations_from_xml (cpt, ctx, iter, AS_RELATION_KIND_REQUIRES);
		} else if (tag_id == AS_TAG_RECOMMENDS) {
			as_component_load_relations_from_xml (cpt, ctx, iter, AS_RELATION_KIND_RECOMMENDS);
		} else if (tag_id == AS_TAG_SUPPORTS) {
			as_component_load_relations_from_xml (cpt, ctx, iter, AS_RELATION_KIND_SUPPORTS);
		} else if (tag_id == AS_TAG_AGREEMENT) {
			g_autoptr(AsAgreement) agreement = as_agreement_new ();
			if (as_agreement_load_from_xml (agreement, ctx, iter, NULL))
				as_component_add_agreement (cpt, agreement);
		} else if (tag_id == AS_TAG_REVIEWS) {
			for (xmlNode *sn = iter->children; sn != NULL; sn = sn->next) {
				if (sn->type != XML_ELEMENT_NODE)
					continue;
				if (g_strcmp0 ((const gchar*) sn->name, "review") == 0) {
					g_autoptr(AsReview) review = as_review_new ();
					if (as_review_load_from_xml (review, ctx, sn, NULL))
						as_component_add_review (cpt, review);
				}
			}
		} else if (tag_id == AS_TAG_BRANDING) {
			g_autoptr(AsBranding) branding = as_branding_new ();
			if (as_branding_load_from_xml (branding, ctx, iter, NULL))
				as_component_set_branding (cpt, branding);
		} else if (tag_id == AS_TAG_TAGS) {
			for (xmlNode *sn = iter->children; sn != NULL; sn = sn->next) {
				g_autofree gchar *ns = NULL;
				g_autofree gchar *value = NULL;
				if (sn->type != XML_ELEMENT_NODE)
					continue;
				ns = as_xml_get_prop_value (sn, "namespace");
				if (sn == NULL)
					continue;
				value = as_xml_get_node_value (sn);
				as_component_add_tag (cpt, ns, value);
			}
		} else if (as_context_get_internal_mode (ctx)) {
			g_autofree gchar *content = as_xml_get_node_value (iter);
			/* internal information */

			if (tag_id == AS_TAG_INTERNAL_SCOPE) {
				priv->scope = as_component_scope_from_string (content);
			} else if (tag_id == AS_TAG_INTERNAL_ORIGIN) {
				as_component_set_origin (cpt, content);
			} else if (tag_id == AS_TAG_INTERNAL_BRANCH) {
				as_component_set_branch (cpt, content);
			}
		} else if (tag_id == AS_TAG_NAME_VARIANT_SUFFIX) {
			g_autofree gchar *lang = NULL;
			g_autofree gchar *content = as_xml_get_node_value (iter);
			lang = as_xml_get_node_locale_match (ctx, iter);
			if (lang != NULL)
				as_component_set_name_variant_suffix (cpt, content, lang);
		}
	}

	/* add package name information to component */
	{
		g_auto(GStrv) strv = as_ptr_array_to_strv (pkgnames);
		as_component_set_pkgnames (cpt, strv);
	}

	/* sanity check */
	if (as_is_empty (priv->id)) {
		g_set_error_literal (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"Component is invalid (essential tags are missing or empty).");
		return FALSE;
	}

	return TRUE;
}

/**
 * as_component_xml_keywords_to_node:
 */
static void
as_component_xml_keywords_to_node (AsComponent *cpt, AsContext *ctx, xmlNode *root)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autoptr(GList) keys = NULL;
	xmlNode *mi_kw_node = NULL;

	if (g_hash_table_size (priv->keywords) == 0)
		return;

	/* we need to sort the keys here to get reproductible output */
	keys = g_hash_table_get_keys (priv->keywords);
	keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);

	if (as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO)
		mi_kw_node = as_xml_add_node (root, "keywords");

	for (GList *link = keys; link != NULL; link = link->next) {
		gboolean has_locale = FALSE;
		const gchar *locale = (const gchar*) link->data;
		gchar **kws = (gchar**) g_hash_table_lookup (priv->keywords, locale);

		/* skip cruft */
		if (as_is_cruft_locale (locale))
			continue;

		has_locale = g_strcmp0 (locale, "C") != 0;
		if (as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) {

			for (guint i = 0; kws[i] != NULL; i++) {
				xmlNode *kw_node = as_xml_add_text_node (mi_kw_node,
									 "keyword",
									 kws[i]);

				if (has_locale)
					as_xml_add_text_prop (kw_node, "xml:lang", locale);
			}
		} else {
			xmlNode *kws_node = as_xml_add_node_list_strv (root, "keywords", "keyword", kws);
			if (kws_node == NULL)
				continue;
			if (has_locale)
				as_xml_add_text_prop (kws_node, "xml:lang", locale);
		}
	}
}

/**
 * as_component_xml_serialize_provides:
 */
static void
as_component_xml_serialize_provides (AsComponent *cpt, xmlNode *cnode)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	xmlNode *node;
	GPtrArray *items;

	if (priv->provided->len == 0)
		return;

	node = xmlNewChild (cnode, NULL, (xmlChar*) "provides", NULL);
	for (guint i = 0; i < priv->provided->len; i++) {
		guint j;
		AsProvided *prov = AS_PROVIDED (g_ptr_array_index (priv->provided, i));

		items = as_provided_get_items (prov);
		switch (as_provided_get_kind (prov)) {
			case AS_PROVIDED_KIND_ID:
				as_xml_add_node_list (node, NULL,
							"id",
							items);
				break;
			case AS_PROVIDED_KIND_MEDIATYPE:
				as_xml_add_node_list (node, NULL,
							"mediatype",
							items);
				break;
			case AS_PROVIDED_KIND_LIBRARY:
				as_xml_add_node_list (node, NULL,
							"library",
							items);
				break;
			case AS_PROVIDED_KIND_BINARY:
				as_xml_add_node_list (node, NULL,
							"binary",
							items);
				break;
			case AS_PROVIDED_KIND_MODALIAS:
				as_xml_add_node_list (node, NULL,
							"modalias",
							items);
				break;
			case AS_PROVIDED_KIND_PYTHON_2:
				as_xml_add_node_list (node, NULL,
							"python2",
							items);
				break;
			case AS_PROVIDED_KIND_PYTHON:
				as_xml_add_node_list (node, NULL,
							"python3",
							items);
				break;
			case AS_PROVIDED_KIND_FONT:
				as_xml_add_node_list (node, NULL,
							"font",
							items);
				break;
			case AS_PROVIDED_KIND_FIRMWARE_RUNTIME:
				for (j = 0; j < items->len; j++) {
					xmlNode *n;
					n = xmlNewTextChild (node, NULL,
							     (xmlChar*) "firmware",
							     (xmlChar*) g_ptr_array_index (items, j));
					xmlNewProp (n,
						    (xmlChar*) "type",
						    (xmlChar*) "runtime");
				}
				break;
			case AS_PROVIDED_KIND_FIRMWARE_FLASHED:
				for (j = 0; j < items->len; j++) {
					xmlNode *n;
					n = xmlNewTextChild (node, NULL,
							     (xmlChar*) "firmware",
							     (xmlChar*) g_ptr_array_index (items, j));
					xmlNewProp (n,
						    (xmlChar*) "type",
						    (xmlChar*) "flashed");
				}
				break;
			case AS_PROVIDED_KIND_DBUS_SYSTEM:
				for (j = 0; j < items->len; j++) {
					xmlNode *n;
					n = xmlNewTextChild (node, NULL,
							     (xmlChar*) "dbus",
							     (xmlChar*) g_ptr_array_index (items, j));
					xmlNewProp (n,
						    (xmlChar*) "type",
						    (xmlChar*) "system");
				}
				break;
			case AS_PROVIDED_KIND_DBUS_USER:
				for (j = 0; j < items->len; j++) {
					xmlNode *n;
					n = xmlNewTextChild (node, NULL,
							     (xmlChar*) "dbus",
							     (xmlChar*) g_ptr_array_index (items, j));
					xmlNewProp (n,
						    (xmlChar*) "type",
						    (xmlChar*) "user");
				}
				break;

			default:
				g_debug ("Couldn't serialize provided-item type '%s'", as_provided_kind_to_string (as_provided_get_kind (prov)));
				break;
		}
	}
}

/**
 * as_component_xml_serialize_languages:
 */
static void
as_component_xml_serialize_languages (AsComponent *cpt, xmlNode *cptnode)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	xmlNode *node;
	g_autoptr(GList) keys = NULL;
	GList *link;

	if (g_hash_table_size (priv->languages) == 0)
		return;

	node = xmlNewChild (cptnode, NULL, (xmlChar*) "languages", NULL);
	keys = g_hash_table_get_keys (priv->languages);
	keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);
	for (link = keys; link != NULL; link = link->next) {
		guint percentage;
		const gchar *locale;
		xmlNode *l_node;
		g_autofree gchar *percentage_str = NULL;

		locale = (const gchar*) link->data;
		percentage = GPOINTER_TO_INT (g_hash_table_lookup (priv->languages, locale));
		percentage_str = g_strdup_printf("%i", percentage);

		l_node = xmlNewTextChild (node,
					  NULL,
					  (xmlChar*) "lang",
					  (xmlChar*) locale);
		xmlNewProp (l_node,
			    (xmlChar*) "percentage",
			    (xmlChar*) percentage_str);
	}
}

/**
 * as_component_to_xml_node:
 * @cpt: an #AsComponent.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
xmlNode*
as_component_to_xml_node (AsComponent *cpt, AsContext *ctx, xmlNode *root)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	xmlNode *cnode;

	/* define component root node properties */
	if (root == NULL)
		cnode = xmlNewNode (NULL, (xmlChar*) "component");
	else
		cnode = xmlNewChild (root, NULL, (xmlChar*) "component", NULL);

	if ((priv->kind != AS_COMPONENT_KIND_GENERIC) && (priv->kind != AS_COMPONENT_KIND_UNKNOWN)) {
		const gchar *kind_str;
		if ((as_context_get_format_version (ctx) < AS_FORMAT_VERSION_V0_10) && (priv->kind == AS_COMPONENT_KIND_DESKTOP_APP))
			kind_str = "desktop";
		else
			kind_str = as_component_kind_to_string (priv->kind);
		xmlNewProp (cnode, (xmlChar*) "type",
					(xmlChar*) kind_str);
	}

	/* set end-of-life date */
	if (priv->date_eol != NULL) {
		g_autofree gchar *time_str = as_component_sanitize_date_eol (cpt);
		if (time_str == NULL)
			g_debug ("Invalid ISO-8601 date_eol for component %s",
				 as_component_get_data_id (cpt));
		else
			as_xml_add_text_prop (cnode, "date_eol", time_str);
	}

	if (as_context_get_style (ctx) == AS_FORMAT_STYLE_COLLECTION) {
		/* write some propties which only exist in collection XML */
		if (priv->merge_kind != AS_MERGE_KIND_NONE) {
			xmlNewProp (cnode, (xmlChar*) "merge",
						(xmlChar*) as_merge_kind_to_string (priv->merge_kind));
		}

		if (priv->priority != 0) {
			g_autofree gchar *priority_str = g_strdup_printf ("%i", priv->priority);
			xmlNewProp (cnode, (xmlChar*) "priority", (xmlChar*) priority_str);
		}
	}

	/* component tags */
	as_xml_add_text_node (cnode, "id", as_component_get_id (cpt));

	/* name */
	as_xml_add_localized_text_node (cnode, "name", priv->name);

	/* name variant suffix */
	if (priv->name_variant_suffix != NULL)
		as_xml_add_localized_text_node (cnode, "name_variant_suffix", priv->name_variant_suffix);

	/* summary */
	as_xml_add_localized_text_node (cnode, "summary", priv->summary);

	/* order license and project group after name/summary */
	if (as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO)
		as_xml_add_text_node (cnode, "metadata_license", priv->metadata_license);

	/* project license */
	as_xml_add_text_node (cnode, "project_license", priv->project_license);

	/* developer name */
	as_xml_add_localized_text_node (cnode, "developer_name", priv->developer_name);

	/* project group */
	as_xml_add_text_node (cnode, "project_group", priv->project_group);

	/* long description */
	as_xml_add_description_node (ctx, cnode, priv->description, TRUE);

	/* extends nodes */
	as_xml_add_node_list (cnode, NULL, "extends", priv->extends);

	/* replaces */
	as_xml_add_node_list (cnode, "replaces", "id", priv->replaces);

	/* requires */
	if (priv->requires->len > 0) {
		xmlNode *rqnode = xmlNewChild (cnode, NULL, (xmlChar*) "requires", NULL);

		for (guint i = 0; i < priv->requires->len; i++) {
			AsRelation *relation = AS_RELATION (g_ptr_array_index (priv->requires, i));
			as_relation_to_xml_node (relation, ctx, rqnode);
		}
	}

	/* recommends */
	if (priv->recommends->len > 0) {
		xmlNode *rcnode = xmlNewChild (cnode, NULL, (xmlChar*) "recommends", NULL);

		for (guint i = 0; i < priv->recommends->len; i++) {
			AsRelation *relation = AS_RELATION (g_ptr_array_index (priv->recommends, i));
			as_relation_to_xml_node (relation, ctx, rcnode);
		}
	}

	/* supports */
	if (priv->supports->len > 0) {
		xmlNode *rcnode = xmlNewChild (cnode, NULL, (xmlChar*) "supports", NULL);

		for (guint i = 0; i < priv->supports->len; i++) {
			AsRelation *relation = AS_RELATION (g_ptr_array_index (priv->supports, i));
			as_relation_to_xml_node (relation, ctx, rcnode);
		}
	}

	/* suggests nodes */
	for (guint i = 0; i < priv->suggestions->len; i++) {
		AsSuggested *suggested = AS_SUGGESTED (g_ptr_array_index (priv->suggestions, i));
		as_suggested_to_xml_node (suggested, ctx, cnode);
	}

	/* package name(s) (if any) */
	as_xml_add_node_list_strv (cnode, NULL, "pkgname", priv->pkgnames);

	/* bundles */
	for (guint i = 0; i < priv->bundles->len; i++) {
		AsBundle *bundle = AS_BUNDLE (g_ptr_array_index (priv->bundles, i));
		as_bundle_to_xml_node (bundle, ctx, cnode);
	}

	/* launchables */
	for (guint i = 0; i < priv->launchables->len; i++) {
		AsLaunchable *launchable = AS_LAUNCHABLE (g_ptr_array_index (priv->launchables, i));
		as_launchable_to_xml_node (launchable, ctx, cnode);
	}

	/* compulsory for desktop(s) */
	as_xml_add_node_list (cnode, NULL, "compulsory_for_desktop", priv->compulsory_for_desktops);

	/* icons */
	for (guint i = 0; i < priv->icons->len; i++) {
		AsIcon *icon = AS_ICON (g_ptr_array_index (priv->icons, i));
		as_icon_to_xml_node (icon, ctx, cnode);
	}

	/* urls */
	for (guint i = AS_URL_KIND_UNKNOWN; i < AS_URL_KIND_LAST; i++) {
		xmlNode *n;
		const gchar *value;
		value = as_component_get_url (cpt, i);
		if (value == NULL)
			continue;

		n = xmlNewTextChild (cnode, NULL, (xmlChar*) "url", (xmlChar*) value);
		xmlNewProp (n, (xmlChar*) "type",
					(xmlChar*) as_url_kind_to_string (i));
	}

	/* software sorting catgories */
	as_xml_add_node_list (cnode, "categories", "category", priv->categories);

	/* provides node */
	as_component_xml_serialize_provides (cpt, cnode);

	/* translations */
	if (priv->translations != NULL) {
		for (guint i = 0; i < priv->translations->len; i++) {
			AsTranslation *tr = AS_TRANSLATION (g_ptr_array_index (priv->translations, i));
			as_translation_to_xml_node (tr, ctx, cnode);
		}
	}

	/* languages node */
	as_component_xml_serialize_languages (cpt, cnode);

	/* screenshots */
	if (priv->screenshots->len > 0) {
		xmlNode *rnode = xmlNewChild (cnode, NULL, (xmlChar*) "screenshots", NULL);

		for (guint i = 0; i < priv->screenshots->len; i++) {
			AsScreenshot *scr = AS_SCREENSHOT (g_ptr_array_index (priv->screenshots, i));
			as_screenshot_to_xml_node (scr, ctx, rnode);
		}
	}

	/* keywords */
	as_component_xml_keywords_to_node (cpt, ctx, cnode);

	/* agreements */
	for (guint i = 0; i < priv->agreements->len; i++) {
		AsAgreement *agreement = AS_AGREEMENT (g_ptr_array_index (priv->agreements, i));
		as_agreement_to_xml_node (agreement, ctx, cnode);
	}

	/* branding */
	if (priv->branding != NULL)
		as_branding_to_xml_node (priv->branding, ctx, cnode);

	/* releases */
	if (priv->releases->len > 0) {
		xmlNode *rnode = xmlNewChild (cnode, NULL, (xmlChar*) "releases", NULL);

		/* ensure releases are sorted, then emit XML nodes */
		as_component_sort_releases (cpt);
		for (guint i = 0; i < priv->releases->len; i++) {
			AsRelease *rel = AS_RELEASE (g_ptr_array_index (priv->releases, i));
			as_release_to_xml_node (rel, ctx, rnode);
		}
	}

	/* content_rating nodes */
	for (guint i = 0; i < priv->content_ratings->len; i++) {
		AsContentRating *ctrating = AS_CONTENT_RATING (g_ptr_array_index (priv->content_ratings, i));
		as_content_rating_to_xml_node (ctrating, ctx, cnode);
	}

	/* tags */
	if (priv->tags->len > 0) {
		xmlNode *tags_node = xmlNewChild (cnode,
						  NULL,
						  (xmlChar*) "tags",
						  NULL);
		for (guint i = 0; i < priv->tags->len; i++) {
			xmlNode *tag_node = NULL;
			g_auto(GStrv) parts = g_strsplit (g_ptr_array_index (priv->tags, i), "::", 2);
			g_assert (parts[1] != NULL);
			tag_node = as_xml_add_text_node (tags_node, "tag", parts[1]);
			as_xml_add_text_prop (tag_node, "namespace", parts[0]);
		}
	}

	/* custom node */
	as_xml_add_custom_node (cnode, "custom", priv->custom);

	/* reviews */
	if (priv->reviews->len > 0) {
		xmlNode *rnode = xmlNewChild (cnode, NULL, (xmlChar*) "reviews", NULL);

		for (guint i = 0; i < priv->reviews->len; i++) {
			AsReview *review = AS_REVIEW (g_ptr_array_index (priv->reviews, i));
			as_review_to_xml_node (review, ctx, rnode);
		}
	}

	/* internal information */
	if (as_context_get_internal_mode (ctx)) {
		const gchar *origin = as_component_get_origin (cpt);
		if (priv->scope != AS_COMPONENT_SCOPE_UNKNOWN)
			as_xml_add_text_node (cnode, "_asi_scope", as_component_scope_to_string (priv->scope));
		if (origin != NULL)
			as_xml_add_text_node (cnode, "_asi_origin", origin);
		if (priv->branch != NULL)
			as_xml_add_text_node (cnode, "_asi_branch", priv->branch);
	}

	return cnode;
}

/**
 * as_component_yaml_parse_keywords:
 *
 * Process a keywords node and add the data to an #AsComponent
 */
static void
as_component_yaml_parse_keywords (AsComponent *cpt, AsContext *ctx, GNode *node)
{
	GNode *tnode;

	for (tnode = node->children; tnode != NULL; tnode = tnode->next) {
		const gchar *locale = as_yaml_get_node_locale (ctx, tnode);
		if (locale != NULL) {
			g_autoptr(GPtrArray) keywords = NULL;
			g_auto(GStrv) strv = NULL;

			keywords = g_ptr_array_new_with_free_func (g_free);
			as_yaml_list_to_str_array (tnode, keywords);

			strv = as_ptr_array_to_strv (keywords);
			as_component_set_keywords (cpt,
						   strv,
						   locale);
		}
	}
}

/**
 * as_component_yaml_parse_urls:
 */
static void
as_component_yaml_parse_urls (AsComponent *cpt, GNode *node)
{
	AsUrlKind url_kind;
	for (GNode *n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);
		const gchar *value = as_yaml_node_get_value (n);

		url_kind = as_url_kind_from_string (key);
		if ((url_kind != AS_URL_KIND_UNKNOWN) && (value != NULL))
			as_component_add_url (cpt, url_kind, value);
	}
}

/**
 * as_component_yaml_parse_icon:
 */
static void
as_component_yaml_parse_icon (AsComponent *cpt, AsContext *ctx, GNode *node, AsIconKind kind)
{
	GNode *n;
	guint64 size;
	guint scale;
	g_autoptr(AsIcon) icon = NULL;

	icon = as_icon_new ();
	as_icon_set_kind (icon, kind);

	for (n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);
		const gchar *value = as_yaml_node_get_value (n);

		if (g_strcmp0 (key, "width") == 0) {
			size = g_ascii_strtoull (value, NULL, 10);
			as_icon_set_width (icon, size);
		} else if (g_strcmp0 (key, "height") == 0) {
			size = g_ascii_strtoull (value, NULL, 10);
			as_icon_set_height (icon, size);
		} else if (g_strcmp0 (key, "scale") == 0) {
			scale = g_ascii_strtoull (value, NULL, 10);
			as_icon_set_scale (icon, scale);
		} else {
			if (kind == AS_ICON_KIND_REMOTE) {
				if (g_strcmp0 (key, "url") == 0) {
					if (as_context_has_media_baseurl (ctx)) {
						/* handle the media baseurl */
						g_autofree gchar *url = NULL;
						url = g_build_filename (as_context_get_media_baseurl (ctx), value, NULL);
						as_icon_set_url (icon, url);
					} else {
						/* no baseurl, we can just set the value as URL */
						as_icon_set_url (icon, value);
					}
				}
			} else {
				if (g_strcmp0 (key, "name") == 0) {
					as_icon_set_filename (icon, value);
				}
			}
		}
	}

	as_component_add_icon (cpt, icon);
}

/**
 * as_component_yaml_parse_icons:
 */
static void
as_component_yaml_parse_icons (AsComponent *cpt, AsContext *ctx, GNode *node)
{
	GNode *n;
	for (n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);
		const gchar *value = as_yaml_node_get_value (n);

		if (g_strcmp0 (key, "stock") == 0) {
			g_autoptr(AsIcon) icon = as_icon_new ();
			as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
			as_icon_set_name (icon, value);
			as_component_add_icon (cpt, icon);
		} else if (g_strcmp0 (key, "cached") == 0) {
			if (value != NULL) {
				g_autoptr(AsIcon) icon = as_icon_new ();
				/* we have a legacy YAML file */
				as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
				as_icon_set_filename (icon, value);
				as_component_add_icon (cpt, icon);
			} else {
				GNode *sn;
				/* we have a recent YAML file */
				for (sn = n->children; sn != NULL; sn = sn->next)
					as_component_yaml_parse_icon (cpt, ctx, sn, AS_ICON_KIND_CACHED);
			}
		} else if (g_strcmp0 (key, "local") == 0) {
			GNode *sn;
			for (sn = n->children; sn != NULL; sn = sn->next)
				as_component_yaml_parse_icon (cpt, ctx, sn, AS_ICON_KIND_LOCAL);
		} else if (g_strcmp0 (key, "remote") == 0) {
			GNode *sn;
			for (sn = n->children; sn != NULL; sn = sn->next)
				as_component_yaml_parse_icon (cpt, ctx, sn, AS_ICON_KIND_REMOTE);
		}
	}
}

/**
 * as_component_yaml_parse_provides:
 */
static void
as_component_yaml_parse_provides (AsComponent *cpt, GNode *node)
{
	GNode *n;
	GNode *sn;

	for (n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);

		if (g_strcmp0 (key, "ids") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_ID, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "libraries") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_LIBRARY, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "binaries") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_BINARY, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "fonts") == 0) {
			GNode *dn;
			for (sn = n->children; sn != NULL; sn = sn->next) {
				for (dn = sn->children; dn != NULL; dn = dn->next) {
					gchar *dvalue = NULL;
					const gchar *dkey = (const gchar*) dn->data;
					if (dn->children)
						dvalue = (gchar*) dn->children->data;
					if (dvalue == NULL)
						continue;
					if (g_strcmp0 (dkey, "name") == 0)
						as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FONT, dvalue);
				}
			}
		} else if (g_strcmp0 (key, "modaliases") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_MODALIAS, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "firmware") == 0) {
			GNode *dn;
			for (sn = n->children; sn != NULL; sn = sn->next) {
				gchar *kind = NULL;
				gchar *fwdata = NULL;
				for (dn = sn->children; dn != NULL; dn = dn->next) {
					gchar *dvalue;
					const gchar *dkey = (const gchar*) dn->data;
					if (dn->children)
						dvalue = (gchar*) dn->children->data;
					else
						continue;
					if (g_strcmp0 (dkey, "type") == 0) {
						kind = dvalue;
					} else if ((g_strcmp0 (dkey, "guid") == 0) || (g_strcmp0 (dkey, "file") == 0)) {
						fwdata = dvalue;
					}
				}
				/* we don't add malformed provides types */
				if ((kind == NULL) || (fwdata == NULL))
					continue;

				if (g_strcmp0 (kind, "runtime") == 0)
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FIRMWARE_RUNTIME, fwdata);
				else if (g_strcmp0 (kind, "flashed") == 0)
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FIRMWARE_FLASHED, fwdata);
			}
		} else if (g_strcmp0 (key, "python2") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_PYTHON_2, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "python3") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_PYTHON, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "mediatypes") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_MEDIATYPE, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "dbus") == 0) {
			GNode *dn;
			for (sn = n->children; sn != NULL; sn = sn->next) {
				gchar *kind = NULL;
				gchar *service = NULL;
				for (dn = sn->children; dn != NULL; dn = dn->next) {
					gchar *dvalue = NULL;
					const gchar *dkey = (const gchar*) dn->data;
					if (dn->children)
						dvalue = (gchar*) dn->children->data;
					if (g_strcmp0 (dkey, "type") == 0) {
						kind = dvalue;
					} else if (g_strcmp0 (dkey, "service") == 0) {
						service = dvalue;
					}
				}
				/* we don't add malformed provides types */
				if ((kind == NULL) || (service == NULL))
					continue;

				if (g_strcmp0 (kind, "system") == 0)
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_DBUS_SYSTEM, service);
				else if ((g_strcmp0 (kind, "user") == 0) || (g_strcmp0 (kind, "session") == 0))
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_DBUS_USER, service);
			}
		}
	}
}

/**
 * as_component_yaml_parse_languages:
 */
static void
as_component_yaml_parse_languages (AsComponent *cpt, GNode *node)
{
	GNode *sn;

	for (sn = node->children; sn != NULL; sn = sn->next) {
		GNode *n;
		g_autofree gchar *locale = NULL;
		g_autofree gchar *percentage_str = NULL;

		for (n = sn->children; n != NULL; n = n->next) {
			const gchar *key = as_yaml_node_get_key (n);
			const gchar *value = as_yaml_node_get_value (n);

			if (g_strcmp0 (key, "locale") == 0) {
				if (locale == NULL)
					locale = g_strdup (value);
			} else if (g_strcmp0 (key, "percentage") == 0) {
				if (percentage_str == NULL)
					percentage_str = g_strdup (value);
			} else {
				as_yaml_print_unknown ("Languages", key);
			}
		}

		if ((locale != NULL) && (percentage_str != NULL))
			as_component_add_language (cpt,
						   locale,
						   g_ascii_strtoll (percentage_str, NULL, 10));
	}
}

/**
 * as_component_yaml_parse_relations:
 */
static void
as_component_yaml_parse_relations (AsComponent *cpt, AsContext *ctx, GNode *node, AsRelationKind kind)
{
	for (GNode *n = node->children; n != NULL; n = n->next) {
		g_autoptr(AsRelation) relation = as_relation_new ();

		as_relation_set_kind (relation, kind);
		if (as_relation_load_from_yaml (relation, ctx, n, NULL))
			as_component_add_relation (cpt, relation);
	}
}

/**
 * as_component_yaml_parse_custom:
 */
static void
as_component_yaml_parse_custom (AsComponent *cpt, GNode *node)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

	for (GNode *sn = node->children; sn != NULL; sn = sn->next) {
		g_hash_table_insert (priv->custom,
				     as_yaml_node_get_key_refstr (sn),
				     as_yaml_node_get_value_refstr (sn));
	}
}

/**
 * as_component_load_from_yaml:
 * @cpt: an #AsComponent.
 * @ctx: the AppStream document context.
 * @root: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_component_load_from_yaml (AsComponent *cpt, AsContext *ctx, GNode *root, GError **error)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	GNode *node;

	/* set context for this component */
	as_component_set_context (cpt, ctx);

	/* set component default priority */
	priv->priority = as_context_get_priority (ctx);

	for (node = root->children; node != NULL; node = node->next) {
		const gchar *key;
		const gchar *value;
		AsTag field_id;

		if (node->children == NULL)
			continue;

		key = as_yaml_node_get_key (node);
		value = as_yaml_node_get_value (node);
		field_id = as_yaml_tag_from_string (key);

		if (field_id == AS_TAG_TYPE) {
			if (g_strcmp0 (value, "generic") == 0)
				priv->kind = AS_COMPONENT_KIND_GENERIC;
			else
				priv->kind = as_component_kind_from_string (value);
		} else if (field_id == AS_TAG_ID) {
			as_component_set_id (cpt, value);
		} else if (field_id == AS_TAG_PRIORITY) {
			priv->priority = g_ascii_strtoll (value, NULL, 10);
		} else if (field_id == AS_TAG_MERGE) {
			priv->merge_kind = as_merge_kind_from_string (value);
		} else if (field_id == AS_TAG_DATE_EOL) {
			as_component_set_date_eol (cpt, value);
		} else if (field_id == AS_TAG_PKGNAME) {
			g_strfreev (priv->pkgnames);

			priv->pkgnames = g_new0 (gchar*, 1 + 1);
			priv->pkgnames[0] = g_strdup (value);
			priv->pkgnames[1] = NULL;
			g_object_notify ((GObject *) cpt, "pkgnames");
		} else if (field_id == AS_TAG_SOURCE_PKGNAME) {
			as_component_set_source_pkgname (cpt, value);
		} else if (field_id == AS_TAG_NAME) {
			as_yaml_set_localized_table (ctx, node, priv->name);
			g_object_notify ((GObject *) cpt, "name");
		} else if (field_id == AS_TAG_SUMMARY) {
			as_yaml_set_localized_table (ctx, node, priv->summary);
			g_object_notify ((GObject *) cpt, "summary");
		} else if (field_id == AS_TAG_DESCRIPTION) {
			as_yaml_set_localized_table (ctx, node, priv->description);
			g_object_notify ((GObject *) cpt, "description");
		} else if (field_id == AS_TAG_DEVELOPER_NAME) {
			as_yaml_set_localized_table (ctx, node, priv->developer_name);
		} else if (field_id == AS_TAG_PROJECT_LICENSE) {
			as_component_set_project_license (cpt, value);
		} else if (field_id == AS_TAG_PROJECT_GROUP) {
			as_component_set_project_group (cpt, value);
		} else if (field_id == AS_TAG_CATEGORIES) {
			as_yaml_list_to_str_array (node, priv->categories);
		} else if (field_id == AS_TAG_COMPULSORY_FOR_DESKTOP) {
			as_yaml_list_to_str_array (node, priv->compulsory_for_desktops);
		} else if (field_id == AS_TAG_EXTENDS) {
			as_yaml_list_to_str_array (node, priv->extends);
		} else if (field_id == AS_TAG_KEYWORDS) {
			as_component_yaml_parse_keywords (cpt, ctx, node);
		} else if (field_id == AS_TAG_URL) {
			as_component_yaml_parse_urls (cpt, node);
		} else if (field_id == AS_TAG_ICON) {
			as_component_yaml_parse_icons (cpt, ctx, node);
		} else if (field_id == AS_TAG_BUNDLE) {
			for (GNode *n = node->children; n != NULL; n = n->next) {
				g_autoptr(AsBundle) bundle = as_bundle_new ();
				if (as_bundle_load_from_yaml (bundle, ctx, n, NULL))
					as_component_add_bundle (cpt, bundle);
			}
		} else if (field_id == AS_TAG_LAUNCHABLE) {
			for (GNode *n = node->children; n != NULL; n = n->next) {
				g_autoptr(AsLaunchable) launch = as_launchable_new ();
				if (as_launchable_load_from_yaml (launch, ctx, n, NULL))
					as_component_add_launchable (cpt, launch);
			}
		} else if (field_id == AS_TAG_PROVIDES) {
			as_component_yaml_parse_provides (cpt, node);
		} else if (field_id == AS_TAG_SCREENSHOTS) {
			for (GNode * n = node->children; n != NULL; n = n->next) {
				g_autoptr(AsScreenshot) scr = as_screenshot_new ();
				if (as_screenshot_load_from_yaml (scr, ctx, n, NULL))
					as_component_add_screenshot (cpt, scr);
			}
		} else if (field_id == AS_TAG_LANGUAGES) {
			as_component_yaml_parse_languages (cpt, node);
		} else if (field_id == AS_TAG_RELEASES) {
			for (GNode *n = node->children; n != NULL; n = n->next) {
				g_autoptr(AsRelease) release = as_release_new ();
				if (as_release_load_from_yaml (release, ctx, n, NULL))
					g_ptr_array_add (priv->releases, g_steal_pointer (&release));
			}
		} else if (field_id == AS_TAG_SUGGESTS) {
			for (GNode *n = node->children; n != NULL; n = n->next) {
				g_autoptr(AsSuggested) suggested = as_suggested_new ();
				if (as_suggested_load_from_yaml (suggested, ctx, n, NULL))
					as_component_add_suggested (cpt, suggested);
			}
		} else if (field_id == AS_TAG_CONTENT_RATING) {
			for (GNode *n = node->children; n != NULL; n = n->next) {
				g_autoptr(AsContentRating) rating = as_content_rating_new ();
				if (as_content_rating_load_from_yaml (rating, ctx, n, NULL))
					as_component_add_content_rating (cpt, rating);
			}
		} else if (field_id == AS_TAG_REPLACES) {
			if (priv->replaces != NULL)
				g_ptr_array_unref (priv->replaces);
			priv->replaces = g_ptr_array_new_with_free_func (g_free);
			for (GNode *n = node->children; n != NULL; n = n->next) {
				for (GNode *sn = n->children; sn != NULL; sn = sn->next) {
					if (g_strcmp0 (as_yaml_node_get_key (sn), "id") != 0)
						continue;
					g_ptr_array_add (priv->replaces,
							g_strdup (as_yaml_node_get_value (sn)));
				}
			}
		} else if (field_id == AS_TAG_REQUIRES) {
			as_component_yaml_parse_relations (cpt, ctx, node, AS_RELATION_KIND_REQUIRES);
		} else if (field_id == AS_TAG_RECOMMENDS) {
			as_component_yaml_parse_relations (cpt, ctx, node, AS_RELATION_KIND_RECOMMENDS);
		} else if (field_id == AS_TAG_SUPPORTS) {
			as_component_yaml_parse_relations (cpt, ctx, node, AS_RELATION_KIND_SUPPORTS);
		} else if (field_id == AS_TAG_AGREEMENT) {
			for (GNode *n = node->children; n != NULL; n = n->next) {
				g_autoptr(AsAgreement) agreement = as_agreement_new ();
				if (as_agreement_load_from_yaml (agreement, ctx, n, NULL))
					as_component_add_agreement (cpt, agreement);
			}
		} else if (field_id == AS_TAG_BRANDING) {
			g_autoptr(AsBranding) branding = as_branding_new ();
			if (as_branding_load_from_yaml (branding, ctx, node, NULL))
				as_component_set_branding (cpt, branding);
		} else if (field_id == AS_TAG_NAME_VARIANT_SUFFIX) {
			if (priv->name_variant_suffix != NULL)
				g_hash_table_unref (priv->name_variant_suffix);
			priv->name_variant_suffix = g_hash_table_new_full (g_str_hash, g_str_equal,
									   (GDestroyNotify) as_ref_string_release,
									   g_free);
			as_yaml_set_localized_table (ctx, node, priv->name_variant_suffix);
		} else if (field_id == AS_TAG_TAGS) {
			for (GNode *tags_n = node->children; tags_n != NULL; tags_n = tags_n->next) {
				const gchar *ns = NULL;
				const gchar *tag_value = NULL;

				for (GNode *tag_n = tags_n->children; tag_n != NULL; tag_n = tag_n->next) {
					const gchar *key = as_yaml_node_get_key (tag_n);
					const gchar *value = as_yaml_node_get_value (tag_n);
					if (g_strcmp0 (key, "namespace") == 0)
						ns = value;
					else if (g_strcmp0 (key, "tag") == 0)
						tag_value = value;
				}
				as_component_add_tag (cpt, ns, tag_value);
			}
		} else if (field_id == AS_TAG_CUSTOM) {
			as_component_yaml_parse_custom (cpt, node);
		} else if (field_id == AS_TAG_REVIEWS) {
			for (GNode *n = node->children; n != NULL; n = n->next) {
				g_autoptr(AsReview) review = as_review_new ();
				if (as_review_load_from_yaml (review, ctx, n, NULL))
					as_component_add_review (cpt, review);
			}
		} else {
			as_yaml_print_unknown ("root", key);
		}
	}

	/* sanity check */
	if (as_is_empty (priv->id)) {
		g_set_error_literal (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"Component is invalid (essential tags are missing or empty).");
		return FALSE;
	}

	return TRUE;
}

/**
 * as_component_yaml_emit_icons:
 */
static void
as_component_yaml_emit_icons (AsComponent* cpt, yaml_emitter_t *emitter, GPtrArray *icons)
{
	guint i;
	GHashTableIter iter;
	gpointer key, value;
	gboolean stock_icon_added = FALSE;
	g_autoptr(GHashTable) icons_table = NULL;

	/* we need to emit the icons in order, so we first need to sort them properly */
	icons_table = g_hash_table_new_full (g_direct_hash,
					     g_direct_equal,
					     NULL,
					     (GDestroyNotify) g_ptr_array_unref);
	for (i = 0; i < icons->len; i++) {
		GPtrArray *ilist;
		AsIconKind ikind;
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		ikind = as_icon_get_kind (icon);
		ilist = g_hash_table_lookup (icons_table, GINT_TO_POINTER (ikind));
		if (ilist == NULL) {
			ilist = g_ptr_array_new ();
			g_hash_table_insert (icons_table, GINT_TO_POINTER (ikind), ilist);
		}

		g_ptr_array_add (ilist, icon);
	}

	g_hash_table_iter_init (&iter, icons_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GPtrArray *ilist;
		AsIconKind ikind;

		ikind = (AsIconKind) GPOINTER_TO_INT (key);
		ilist = (GPtrArray*) value;

		if (ikind == AS_ICON_KIND_STOCK) {
			/* there can always be only one stock icon, so this is easy */
			if (!stock_icon_added)
				as_yaml_emit_entry (emitter,
						    as_icon_kind_to_string (ikind),
						    as_icon_get_name (AS_ICON (g_ptr_array_index (ilist, 0))));
			stock_icon_added = TRUE;
		} else {
			as_yaml_emit_scalar (emitter, as_icon_kind_to_string (ikind));
			as_yaml_sequence_start (emitter);

			for (i = 0; i < ilist->len; i++) {
				AsIcon *icon = AS_ICON (g_ptr_array_index (ilist, i));

				as_yaml_mapping_start (emitter);

				if (ikind == AS_ICON_KIND_REMOTE)
					as_yaml_emit_entry (emitter, "url", as_icon_get_url (icon));
				else if (ikind == AS_ICON_KIND_LOCAL)
					as_yaml_emit_entry (emitter, "name", as_icon_get_filename (icon));
				else
					as_yaml_emit_entry (emitter, "name", as_icon_get_name (icon));

				if (as_icon_get_width (icon) > 0) {
					as_yaml_emit_entry_uint64 (emitter,
								 "width",
								 as_icon_get_width (icon));
				}

				if (as_icon_get_height (icon) > 0) {
					as_yaml_emit_entry_uint64 (emitter,
								 "height",
								 as_icon_get_height (icon));
				}

				if (as_icon_get_scale (icon) > 1) {
					as_yaml_emit_entry_uint64 (emitter,
								 "scale",
								 as_icon_get_scale (icon));
				}

				as_yaml_mapping_end (emitter);
			}

			as_yaml_sequence_end (emitter);
		}
	}
}

/**
 * as_component_yaml_emit_provides:
 */
static void
as_component_yaml_emit_provides (AsComponent *cpt, yaml_emitter_t *emitter)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	guint i;

	g_autoptr(GPtrArray) dbus_system = NULL;
	g_autoptr(GPtrArray) dbus_user = NULL;

	g_autoptr(GPtrArray) fw_runtime = NULL;
	g_autoptr(GPtrArray) fw_flashed = NULL;

	if (priv->provided->len == 0)
		return;

	as_yaml_emit_scalar (emitter, "Provides");
	as_yaml_mapping_start (emitter);
	for (i = 0; i < priv->provided->len; i++) {
		AsProvidedKind kind;
		GPtrArray *items;
		guint j;
		AsProvided *prov = AS_PROVIDED (g_ptr_array_index (priv->provided, i));

		items = as_provided_get_items (prov);
		if (items->len == 0)
			continue;

		kind = as_provided_get_kind (prov);
		switch (kind) {
			case AS_PROVIDED_KIND_ID:
				as_yaml_emit_sequence (emitter,
							"ids",
							items);
				break;
			case AS_PROVIDED_KIND_LIBRARY:
				as_yaml_emit_sequence (emitter,
							"libraries",
							items);
				break;
			case AS_PROVIDED_KIND_BINARY:
				as_yaml_emit_sequence (emitter,
							"binaries",
							items);
				break;
			case AS_PROVIDED_KIND_MEDIATYPE:
				as_yaml_emit_sequence (emitter,
							"mediatypes",
							items);
				break;
			case AS_PROVIDED_KIND_PYTHON_2:
				as_yaml_emit_sequence (emitter,
							"python2",
							items);
				break;
			case AS_PROVIDED_KIND_PYTHON:
				as_yaml_emit_sequence (emitter,
							"python3",
							items);
				break;
			case AS_PROVIDED_KIND_MODALIAS:
				as_yaml_emit_sequence (emitter,
							"modaliases",
							items);
				break;
			case AS_PROVIDED_KIND_FONT:
				as_yaml_emit_scalar (emitter, "fonts");

				as_yaml_sequence_start (emitter);
				for (j = 0; j < items->len; j++) {
					as_yaml_mapping_start (emitter);
					as_yaml_emit_entry (emitter,
							    "name",
							    (const gchar*) g_ptr_array_index (items, j));
					as_yaml_mapping_end (emitter);
				}
				as_yaml_sequence_end (emitter);
				break;
			case AS_PROVIDED_KIND_DBUS_SYSTEM:
				if (dbus_system == NULL) {
					dbus_system = g_ptr_array_ref (items);
				} else {
					g_critical ("Hit dbus:system twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_DBUS_USER:
				if (dbus_user == NULL) {
					dbus_user = g_ptr_array_ref (items);
				} else {
					g_critical ("Hit dbus:user twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_FIRMWARE_RUNTIME:
				if (fw_runtime == NULL) {
					fw_runtime = g_ptr_array_ref (items);
				} else {
					g_critical ("Hit firmware:runtime twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_FIRMWARE_FLASHED:
				if (fw_flashed == NULL) {
					fw_flashed = g_ptr_array_ref (items);
				} else {
					g_critical ("Hit dbus-user twice, this should never happen!");
				}
				break;
			default:
				g_warning ("Ignoring unknown type of provided items: %s", as_provided_kind_to_string (kind));
				break;
		}
	}

	/* dbus subsection */
	if ((dbus_system != NULL) || (dbus_user != NULL)) {
		as_yaml_emit_scalar (emitter, "dbus");
		as_yaml_sequence_start (emitter);

		if (dbus_system != NULL) {
			for (i = 0; i < dbus_system->len; i++) {
				const gchar *value = (const gchar*) g_ptr_array_index (dbus_system, i);
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "system");
				as_yaml_emit_entry (emitter, "service", value);

				as_yaml_mapping_end (emitter);
			}
		}

		if (dbus_user != NULL) {
			for (i = 0; i < dbus_user->len; i++) {
				const gchar *value = (const gchar*) g_ptr_array_index (dbus_user, i);

				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "user");
				as_yaml_emit_entry (emitter, "service", value);

				as_yaml_mapping_end (emitter);
			}
		}

		as_yaml_sequence_end (emitter);
	}

	/* firmware subsection */
	if ((fw_runtime != NULL) || (fw_flashed != NULL)) {
		as_yaml_emit_scalar (emitter, "firmware");
		as_yaml_sequence_start (emitter);

		if (fw_runtime != NULL) {
			for (i = 0; i < fw_runtime->len; i++) {
				const gchar *value = g_ptr_array_index (fw_runtime, i);
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "runtime");
				as_yaml_emit_entry (emitter, "file", value);

				as_yaml_mapping_end (emitter);
			}
		}

		if (fw_flashed != NULL) {
			for (i = 0; i < fw_flashed->len; i++) {
				const gchar *value = g_ptr_array_index (fw_flashed, i);
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "flashed");
				as_yaml_emit_entry (emitter, "guid", value);

				as_yaml_mapping_end (emitter);
			}
		}

		as_yaml_sequence_end (emitter);
	}

	as_yaml_mapping_end (emitter);
}

/**
 * as_component_yaml_emit_languages:
 */
static void
as_component_yaml_emit_languages (AsComponent *cpt, yaml_emitter_t *emitter)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autoptr(GList) keys = NULL;
	GList *link;

	if (g_hash_table_size (priv->languages) == 0)
		return;

	as_yaml_emit_scalar (emitter, "Languages");
	as_yaml_sequence_start (emitter);

	keys = g_hash_table_get_keys (priv->languages);
	keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);
	for (link = keys; link != NULL; link = link->next) {
		guint percentage;
		const gchar *locale;

		locale = (const gchar*) link->data;
		percentage = GPOINTER_TO_INT (g_hash_table_lookup (priv->languages, locale));

		as_yaml_mapping_start (emitter);
		as_yaml_emit_entry (emitter, "locale", locale);
		as_yaml_emit_entry_uint64 (emitter, "percentage", percentage);
		as_yaml_mapping_end (emitter);
	}

	as_yaml_sequence_end (emitter);
}

/**
 * as_component_yaml_emit_custom:
 */
static void
as_component_yaml_emit_custom (AsComponent *cpt, yaml_emitter_t *emitter)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	g_autoptr(GList) keys = NULL;
	GList *link;

	if (g_hash_table_size (priv->custom) == 0)
		return;

	as_yaml_emit_scalar (emitter, "Custom");
	as_yaml_mapping_start (emitter);

	keys = g_hash_table_get_keys (priv->custom);
	keys = g_list_sort (keys, (GCompareFunc) g_ascii_strcasecmp);
	for (link = keys; link != NULL; link = link->next) {
		as_yaml_emit_entry (emitter,
				    (const gchar*) link->data,
				    (const gchar*) g_hash_table_lookup (priv->custom, link->data));
	}

	as_yaml_mapping_end (emitter);
}

/**
 * as_component_emit_yaml:
 * @cpt: an #AsComponent.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_component_emit_yaml (AsComponent *cpt, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	gint res;
	const gchar *cstr;
	yaml_event_t event;

	/* new document for this component */
	yaml_document_start_event_initialize (&event, NULL, NULL, NULL, FALSE);
	res = yaml_emitter_emit (emitter, &event);
	g_assert (res);

	/* open main mapping */
	as_yaml_mapping_start (emitter);

	/* write component kind */
	if ((as_context_get_format_version (ctx) < AS_FORMAT_VERSION_V0_10) && (priv->kind == AS_COMPONENT_KIND_DESKTOP_APP))
		cstr = "desktop-app";
	else
		cstr = as_component_kind_to_string (priv->kind);
	as_yaml_emit_entry (emitter, "Type", cstr);

	/* AppStream-ID */
	as_yaml_emit_entry (emitter, "ID", priv->id);

	/* Priority */
	if (priv->priority != 0) {
		g_autofree gchar *priority_str = g_strdup_printf ("%i", priv->priority);
		as_yaml_emit_entry (emitter,
				    "Priority",
				    priority_str);
	}

	/* merge strategy */
	if (priv->merge_kind != AS_MERGE_KIND_NONE) {
		as_yaml_emit_entry (emitter,
				    "Merge",
				    as_merge_kind_to_string (priv->merge_kind));
	}

	/* End-of-life date */
	if (priv->date_eol != NULL) {
		g_autofree gchar *time_str = as_component_sanitize_date_eol (cpt);
		if (time_str == NULL)
			g_debug ("Invalid ISO-8601 date_eol for component %s",
				 as_component_get_data_id (cpt));
		else
			as_yaml_emit_entry (emitter, "DateEOL", time_str);
	}

	/* SourcePackage */
	as_yaml_emit_entry (emitter, "SourcePackage", priv->source_pkgname);

	/* Package */
	/* NOTE: a DEP-11 components do *not* support multiple packages per component */
	if ((priv->pkgnames != NULL) && (priv->pkgnames[0] != NULL))
		as_yaml_emit_entry (emitter, "Package", priv->pkgnames[0]);

	/* Extends */
	as_yaml_emit_sequence (emitter, "Extends", priv->extends);

	/* Name */
	as_yaml_emit_localized_entry (emitter, "Name", priv->name);

	/* Summary */
	as_yaml_emit_localized_entry (emitter, "Summary", priv->summary);

	/* Description */
	as_yaml_emit_long_localized_entry (emitter, "Description", priv->description);

	/* NameVariantSuffix */
	if (priv->name_variant_suffix != NULL)
		as_yaml_emit_localized_entry (emitter, "NameVariantSuffix", priv->name_variant_suffix);

	/* DeveloperName */
	as_yaml_emit_localized_entry (emitter, "DeveloperName", priv->developer_name);

	/* ProjectGroup */
	as_yaml_emit_entry (emitter, "ProjectGroup", priv->project_group);

	/* ProjectLicense */
	as_yaml_emit_entry (emitter, "ProjectLicense", priv->project_license);

	/* CompulsoryForDesktops */
	as_yaml_emit_sequence_from_str_array (emitter, "CompulsoryForDesktops", priv->compulsory_for_desktops);

	/* Categories */
	as_yaml_emit_sequence_from_str_array (emitter, "Categories", priv->categories);

	/* Keywords */
	as_yaml_emit_localized_strv (emitter, "Keywords", priv->keywords);

	/* Urls */
	if (g_hash_table_size (priv->urls) > 0) {
		GHashTableIter iter;
		gpointer key, value;

		as_yaml_emit_scalar (emitter, "Url");
		as_yaml_mapping_start (emitter);

		g_hash_table_iter_init (&iter, priv->urls);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			as_yaml_emit_entry (emitter, as_url_kind_to_string (GPOINTER_TO_INT (key)), (const gchar*) value);
		}
		as_yaml_mapping_end (emitter);
	}

	/* Icons */
	if (priv->icons->len > 0) {
		as_yaml_emit_scalar (emitter, "Icon");
		as_yaml_mapping_start (emitter);
		as_component_yaml_emit_icons (cpt, emitter, priv->icons);
		as_yaml_mapping_end (emitter);
	}

	/* Bundles */
	if (priv->bundles->len > 0) {
		as_yaml_emit_scalar (emitter, "Bundles");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->bundles->len; i++) {
			AsBundle *bundle = AS_BUNDLE (g_ptr_array_index (priv->bundles, i));
			as_bundle_emit_yaml (bundle, ctx, emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Launchable */
	if (priv->launchables->len > 0) {
		as_yaml_emit_scalar (emitter, "Launchable");
		as_yaml_mapping_start (emitter);

		for (guint i = 0; i < priv->launchables->len; i++) {
			AsLaunchable *launch = AS_LAUNCHABLE (g_ptr_array_index (priv->launchables, i));
			as_launchable_emit_yaml (launch, ctx, emitter);
		}

		as_yaml_mapping_end (emitter);
	}

	/* Replaces */
	if (priv->replaces != NULL && priv->replaces->len > 0) {
		as_yaml_emit_scalar (emitter, "Replaces");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->replaces->len; i++) {
			as_yaml_mapping_start (emitter);
			as_yaml_emit_entry (emitter, "id", (const gchar*) g_ptr_array_index (priv->replaces, i));
			as_yaml_mapping_end (emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Requires */
	if (priv->requires->len > 0) {
		as_yaml_emit_scalar (emitter, "Requires");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->requires->len; i++) {
			AsRelation *relation = AS_RELATION (g_ptr_array_index (priv->requires, i));
			as_relation_emit_yaml (relation, ctx, emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Recommends */
	if (priv->recommends->len > 0) {
		as_yaml_emit_scalar (emitter, "Recommends");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->recommends->len; i++) {
			AsRelation *relation = AS_RELATION (g_ptr_array_index (priv->recommends, i));
			as_relation_emit_yaml (relation, ctx, emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Supports */
	if (priv->supports->len > 0) {
		as_yaml_emit_scalar (emitter, "Supports");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->supports->len; i++) {
			AsRelation *relation = AS_RELATION (g_ptr_array_index (priv->supports, i));
			as_relation_emit_yaml (relation, ctx, emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Provides */
	as_component_yaml_emit_provides (cpt, emitter);

	/* Suggests */
	if (priv->suggestions->len > 0) {
		as_yaml_emit_scalar (emitter, "Suggests");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->suggestions->len; i++) {
			AsSuggested *suggested = AS_SUGGESTED (g_ptr_array_index (priv->suggestions, i));
			as_suggested_emit_yaml (suggested, ctx, emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Screenshots */
	if (priv->screenshots->len > 0) {
		as_yaml_emit_scalar (emitter, "Screenshots");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->screenshots->len; i++) {
			AsScreenshot *screenshot = AS_SCREENSHOT (g_ptr_array_index (priv->screenshots, i));
			as_screenshot_emit_yaml (screenshot, ctx, emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Translation details */
	as_component_yaml_emit_languages (cpt, emitter);

	/* Agreements */
	if (priv->agreements->len > 0) {
		as_yaml_emit_scalar (emitter, "Agreements");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->agreements->len; i++) {
			AsAgreement *agreement = AS_AGREEMENT (g_ptr_array_index (priv->agreements, i));
			as_agreement_emit_yaml (agreement, ctx, emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Branding */
	if (priv->branding != NULL)
		as_branding_emit_yaml (priv->branding, ctx, emitter);

	/* Releases */
	if (priv->releases->len > 0) {
		/* ensure releases are sorted */
		as_component_sort_releases (cpt);

		as_yaml_emit_scalar (emitter, "Releases");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->releases->len; i++) {
			AsRelease *release = AS_RELEASE (g_ptr_array_index (priv->releases, i));
			as_release_emit_yaml (release, ctx, emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* ContentRating */
	if (priv->content_ratings->len > 0) {
		as_yaml_emit_scalar (emitter, "ContentRating");
		as_yaml_mapping_start (emitter);

		for (guint i = 0; i < priv->content_ratings->len; i++) {
			AsContentRating *content_rating = AS_CONTENT_RATING (g_ptr_array_index (priv->content_ratings, i));
			as_content_rating_emit_yaml (content_rating, ctx, emitter);
		}

		as_yaml_mapping_end (emitter);
	}

	/* Tags */
	if (priv->tags->len > 0) {
		as_yaml_emit_scalar (emitter, "Tags");
		as_yaml_sequence_start (emitter);

		for (guint i = 0; i < priv->tags->len; i++) {
			g_auto(GStrv) parts = g_strsplit (g_ptr_array_index (priv->tags, i), "::", 2);
			g_assert (parts[1] != NULL);

			as_yaml_mapping_start (emitter);
			as_yaml_emit_entry (emitter, "namespace", parts[0]);
			as_yaml_emit_entry (emitter, "tag", parts[1]);
			as_yaml_mapping_end (emitter);
		}

		as_yaml_sequence_end (emitter);
	}

	/* Custom fields */
	as_component_yaml_emit_custom (cpt, emitter);

	/* Reviews */
	if (priv->reviews->len > 0) {
		as_yaml_emit_scalar (emitter, "Reviews");
		as_yaml_mapping_start (emitter);

		for (guint i = 0; i < priv->reviews->len; i++) {
			AsReview *review = AS_REVIEW (g_ptr_array_index (priv->reviews, i));
			as_review_emit_yaml (review, ctx, emitter);
		}

		as_yaml_mapping_end (emitter);
	}

	/* close main mapping */
	as_yaml_mapping_end (emitter);

	/* finalize the document */
	yaml_document_end_event_initialize (&event, 1);
	res = yaml_emitter_emit (emitter, &event);
	g_assert (res);
}

/**
 * as_component_load_from_bytes:
 * @cpt: an #AsComponent instance.
 * @context: an #AsContext instance.
 * @format: the format of the data to load, e.g. %AS_FORMAT_KIND_XML
 * @bytes: the data to load.
 * @error: a #GError.
 *
 * Load metadata for this component from an XML string.
 * You normally do not want to use this method directly and instead use the more
 * convenient API of #AsMetadata to create and update components.
 *
 * If this function returns %TRUE, a valid component is returned unless the selected
 * format was %AS_FORMAT_KIND_DESKTOP_ENTRY, in which case a component ID will have to
 * be set explicitly by the caller in order to make the component valid.
 *
 * Returns: %TRUE on success.
 *
 * Since: 0.14.0
 */
gboolean
as_component_load_from_bytes (AsComponent *cpt, AsContext *context, AsFormatKind format, GBytes *bytes, GError **error)
{
	AsComponentPrivate *priv = GET_PRIVATE (cpt);
	gsize data_len;
	const gchar *data = g_bytes_get_data (bytes, &data_len);

	if (data_len == 0) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "No data submitted to load component from.");
		return FALSE;
	}

	if ((format == AS_FORMAT_KIND_XML) || (format == AS_FORMAT_KIND_UNKNOWN)) {
		xmlDoc *doc;
		xmlNode *root;
		gboolean ret;
		g_return_val_if_fail (context != NULL, FALSE);

		doc = as_xml_parse_document (data, data_len, error);
		if (doc == NULL)
			return FALSE;
		root = xmlDocGetRootElement (doc);

		ret = as_component_load_from_xml (cpt, context, root, error);
		xmlFreeDoc (doc);
		return ret;
	}

	if (format == AS_FORMAT_KIND_DESKTOP_ENTRY) {
		GError *tmp_error = NULL;
		as_desktop_entry_parse_data (cpt,
					     data,
					     data_len,
					     AS_FORMAT_VERSION_CURRENT,
					     TRUE,
					     NULL, /* issues */
					     NULL, /* l10n function */
					     NULL, /* l10n func user data */
					     &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}

		/* update context */
		priv->priority = as_context_get_priority (context);
		as_component_set_context (cpt, context);

		return TRUE;
	}

	g_set_error_literal (error,
			     AS_METADATA_ERROR,
			     AS_METADATA_ERROR_FAILED,
			     "Unable to load data: Unknown or unsupported format for this operation.");
	return FALSE;
}

/**
 * as_component_load_from_xml_data:
 * @cpt: an #AsComponent instance.
 * @context: an #AsContext instance.
 * @data: The XML data to load.
 * @error: a #GError.
 *
 * Load metadata for this component from an XML string.
 * You normally do not want to use this method directly and instead use the more
 * convenient API of #AsMetadata to create and update components.
 *
 * Returns: %TRUE on success.
 *
 * Since: 0.12.10
 */
gboolean
as_component_load_from_xml_data (AsComponent *cpt, AsContext *context, const gchar *data, GError **error)
{
	g_autoptr(GBytes) bytes = g_bytes_new_static (data, strlen (data));
	return as_component_load_from_bytes (cpt,
					     context,
					     AS_FORMAT_KIND_XML,
					     bytes,
					     error);
}

/**
 * as_component_to_xml_data:
 * @cpt: an #AsComponent instance.
 * @context: an #AsContext instance.
 * @error: a #GError.
 *
 * Serialize this component into an XML string.
 * You normally do not want to use this method directly and instead use the more
 * convenient API of #AsMetadata to serialize components.
 *
 * Returns: %TRUE on success.
 *
 * Since: 0.12.10
 */
gchar*
as_component_to_xml_data (AsComponent *cpt, AsContext *context, GError **error)
{
	xmlNode *node;
	g_return_val_if_fail (context != NULL, NULL);

	node = as_component_to_xml_node (cpt, context, NULL);
	return as_xml_node_to_str (node, error);
}

/**
 * as_component_get_desktop_id:
 * @cpt: a #AsComponent instance.
 *
 * Get the Desktop Entry ID for this component, if it is
 * of type "desktop-application".
 * For most desktop-application components, this is the name
 * of the .desktop file.
 *
 * Refer to https://specifications.freedesktop.org/desktop-entry-spec/latest/ape.html for more
 * information.
 *
 * Returns: The desktop file id.
 *
 * Since: 0.9.8
 * Deprecated: 0.11.0: Replaced by #AsLaunchable and %as_component_get_launchable
 */
const gchar*
as_component_get_desktop_id (AsComponent *cpt)
{
	AsLaunchable *launch;
	GPtrArray *entries;

	launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	if (launch == NULL)
		return NULL;

	entries = as_launchable_get_entries (launch);
	if (entries->len <= 0)
		return 0;

	return (const gchar*) g_ptr_array_index (entries, 0);
}

/**
 * as_component_get_property:
 */
static void
as_component_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	AsComponent *cpt;
	cpt = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_COMPONENT, AsComponent);
	AsComponentPrivate *priv = GET_PRIVATE (cpt);

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
			g_value_set_boxed (value, priv->urls);
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
					g_param_spec_boxed ("categories", "categories", "categories", G_TYPE_PTR_ARRAY, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
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
