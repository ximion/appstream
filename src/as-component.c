/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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
#include <stdlib.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

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

struct _AsComponentPrivate {
	AsComponentKind kind;
	gchar *id;
	gchar *origin;
	gchar **pkgnames;
	gchar *name;
	gchar *name_original;
	gchar *summary;
	gchar *description;
	gchar **keywords;
	gchar *icon;
	gchar **categories;
	gchar *project_license;
	gchar *project_group;
	gchar *developer_name;
	gchar **compulsory_for_desktops;
	GPtrArray *screenshots; /* of AsScreenshot elements */
	GPtrArray *provided_items; /* of utf8:string */
	GPtrArray *releases; /* of AsRelease */
	GHashTable *urls; /* of key:utf8 */
	GHashTable *icon_urls; /* of key:utf8 */
	GPtrArray *extends; /* of utf8:string */
	GHashTable *languages; /* of key:utf8 */
	int priority; /* used internally */
};

static gpointer as_component_parent_class = NULL;

#define AS_COMPONENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_COMPONENT, AsComponentPrivate))

enum  {
	AS_COMPONENT_DUMMY_PROPERTY,
	AS_COMPONENT_KIND,
	AS_COMPONENT_PKGNAMES,
	AS_COMPONENT_ID,
	AS_COMPONENT_NAME,
	AS_COMPONENT_NAME_ORIGINAL,
	AS_COMPONENT_SUMMARY,
	AS_COMPONENT_DESCRIPTION,
	AS_COMPONENT_KEYWORDS,
	AS_COMPONENT_ICON,
	AS_COMPONENT_ICON_URLS,
	AS_COMPONENT_URLS,
	AS_COMPONENT_CATEGORIES,
	AS_COMPONENT_PROJECT_LICENSE,
	AS_COMPONENT_PROJECT_GROUP,
	AS_COMPONENT_DEVELOPER_NAME,
	AS_COMPONENT_SCREENSHOTS
};

static void as_component_finalize (GObject* obj);
static void as_component_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void as_component_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);

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
	return AS_COMPONENT_KIND_UNKNOWN;
}

/**
 * as_component_construct:
 *
 * Construct an #AsComponent.
 *
 * Returns: (transfer full): a new #AsComponent
 **/
AsComponent*
as_component_construct (GType object_type)
{
	AsComponent *cpt = NULL;

	cpt = (AsComponent*) g_object_new (object_type, NULL);
	as_component_set_id (cpt, "");
	as_component_set_origin (cpt, "");
	as_component_set_name_original (cpt, "");
	as_component_set_summary (cpt, "");
	as_component_set_description (cpt, "");
	as_component_set_icon (cpt, "");
	as_component_set_project_license (cpt, "");
	as_component_set_project_group (cpt, "");
	cpt->priv->keywords = NULL;
	cpt->priv->categories = NULL;

	cpt->priv->screenshots = g_ptr_array_new_with_free_func (g_object_unref);
	cpt->priv->provided_items = g_ptr_array_new_with_free_func (g_free);
	cpt->priv->releases = g_ptr_array_new_with_free_func (g_object_unref);
	cpt->priv->extends = g_ptr_array_new_with_free_func (g_free);
	cpt->priv->icon_urls = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	cpt->priv->urls = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	cpt->priv->languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	as_component_set_priority (cpt, 0);

	return cpt;
}

static void
as_component_finalize (GObject* obj)
{
	AsComponent *cpt;
	cpt = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_COMPONENT, AsComponent);

	g_free (cpt->priv->id);
	g_strfreev (cpt->priv->pkgnames);
	g_free (cpt->priv->name);
	g_free (cpt->priv->name_original);
	g_free (cpt->priv->summary);
	g_free (cpt->priv->description);
	g_free (cpt->priv->icon);
	g_free (cpt->priv->project_license);
	g_free (cpt->priv->project_group);
	g_strfreev (cpt->priv->keywords);
	g_strfreev (cpt->priv->categories);
	g_strfreev (cpt->priv->compulsory_for_desktops);
	g_ptr_array_unref (cpt->priv->screenshots);
	g_ptr_array_unref (cpt->priv->provided_items);
	g_ptr_array_unref (cpt->priv->releases);
	g_ptr_array_unref (cpt->priv->extends);
	g_hash_table_unref (cpt->priv->urls);
	g_hash_table_unref (cpt->priv->icon_urls);
	g_hash_table_unref (cpt->priv->languages);

	G_OBJECT_CLASS (as_component_parent_class)->finalize (obj);
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
	return as_component_construct (AS_TYPE_COMPONENT);
}

/**
 * as_component_is_valid:
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
	AsComponentKind ctype;

	g_return_val_if_fail (cpt != NULL, FALSE);

	ctype = cpt->priv->kind;
	if (ctype == AS_COMPONENT_KIND_UNKNOWN)
		return FALSE;

	if ((cpt->priv->pkgnames != NULL) &&
		(cpt->priv->pkgnames[0] != NULL) &&
		(g_strcmp0 (cpt->priv->id, "") != 0) &&
		(g_strcmp0 (cpt->priv->name, "") != 0) &&
		(g_strcmp0 (cpt->priv->name_original, "") != 0)) {
			ret = TRUE;
	}

#if 0
	if ((ret) && ctype == AS_COMPONENT_KIND_DESKTOP_APP) {
		ret = g_strcmp0 (cpt->priv->desktop_file, "") != 0;
	}
#endif

	return ret;
}

/**
 * as_component_to_string:
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
	gchar *pkgs;
	g_return_val_if_fail (cpt != NULL, NULL);

	if (cpt->priv->pkgnames == NULL)
		pkgs = g_strdup ("?");
	else
		pkgs = g_strjoinv (",", cpt->priv->pkgnames);
	name = as_component_get_name (cpt);

	switch (cpt->priv->kind) {
		case AS_COMPONENT_KIND_DESKTOP_APP:
		{
			res = g_strdup_printf ("[DesktopApp::%s]> name: %s | package: %s | summary: %s", cpt->priv->id, name, pkgs, cpt->priv->summary);
			break;
		}
		default:
		{
			res = g_strdup_printf ("[Component::%s]> name: %s | package: %s | summary: %s", cpt->priv->id, name, pkgs, cpt->priv->summary);
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
	g_return_if_fail (cpt != NULL);
	g_return_if_fail (sshot != NULL);

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
	g_return_if_fail (cpt != NULL);
	g_return_if_fail (release != NULL);

	releases = as_component_get_releases (cpt);
	g_ptr_array_add (releases, g_object_ref (release));
}

/**
 * as_component_get_urls:
 * @cpt: a #AsComponent instance.
 *
 * Gets the URLs set for the component.
 *
 * Returns: (transfer none): URLs
 *
 * Since: 0.6.2
 **/
GHashTable*
as_component_get_urls (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->urls;
}

/**
 * as_component_get_url:
 * @cpt: a #AsComponent instance.
 * @url_kind: the URL kind, e.g. %AS_URL_KIND_HOMEPAGE.
 *
 * Gets a URL.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.6.2
 **/
const gchar *
as_component_get_url (AsComponent *cpt, AsUrlKind url_kind)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return g_hash_table_lookup (cpt->priv->urls,
				    as_url_kind_to_string (url_kind));
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
as_component_add_url (AsComponent *cpt,
					  AsUrlKind url_kind,
					  const gchar *url)
{
	g_return_if_fail (cpt != NULL);
	g_hash_table_insert (cpt->priv->urls,
			     g_strdup (as_url_kind_to_string (url_kind)),
			     g_strdup (url));
}

 /**
  * as_component_get_extends:
  * @cpt: an #AsComponent instance.
  *
  * Returns a string list of IDs of components which
  * are extended by this addon.
  *
  * Returns: (element-type utf8) (transfer none): an array
  *
  * Since: 0.7.0
**/
GPtrArray*
as_component_get_extends (AsComponent *cpt)
{
	AsComponentPrivate *priv = cpt->priv;
	return priv->extends;
}

/**
 * as_component_add_extends:
 * @cpt: a #AsComponent instance.
 * @cpt_id: The id of a component which is extended by this component
 *
 * Add a reference to the extended component
 **/
void
as_component_add_extends (AsComponent* cpt, const gchar* cpt_id)
{
	AsComponentPrivate *priv = cpt->priv;
	g_ptr_array_add (priv->extends, g_strdup (cpt_id));
}

static void
_as_component_serialize_image (AsImage *img, xmlNode *subnode)
{
	xmlNode* n_image = NULL;
	gchar *size;
	g_return_if_fail (img != NULL);
	g_return_if_fail (subnode != NULL);

	n_image = xmlNewTextChild (subnode, NULL, (xmlChar*) "image", (xmlChar*) as_image_get_url (img));
	if (as_image_get_kind (img) == AS_IMAGE_KIND_THUMBNAIL)
		xmlNewProp (n_image, (xmlChar*) "type", (xmlChar*) "thumbnail");
	else
		xmlNewProp (n_image, (xmlChar*) "type", (xmlChar*) "source");

	size = g_strdup_printf("%i", as_image_get_width (img));
	xmlNewProp (n_image, (xmlChar*) "width", (xmlChar*) size);
	g_free (size);

	size = g_strdup_printf("%i", as_image_get_height (img));
	xmlNewProp (n_image, (xmlChar*) "height", (xmlChar*) size);
	g_free (size);

	xmlAddChild (subnode, n_image);
}

/**
 * as_component_xml_add_screenshot_subnodes:
 *
 * Add screenshot subnodes to a root node
 */
static void
as_component_xml_add_screenshot_subnodes (AsComponent *cpt, xmlNode *root)
{
	GPtrArray* sslist;
	AsScreenshot *sshot;
	guint i;

	sslist = as_component_get_screenshots (cpt);
	for (i = 0; i < sslist->len; i++) {
		xmlNode *subnode;
		const gchar *str;
		GPtrArray *images;
		sshot = (AsScreenshot*) g_ptr_array_index (sslist, i);

		subnode = xmlNewTextChild (root, NULL, (xmlChar*) "screenshot", (xmlChar*) "");
		if (as_screenshot_get_kind (sshot) == AS_SCREENSHOT_KIND_DEFAULT)
			xmlNewProp (subnode, (xmlChar*) "type", (xmlChar*) "default");

		str = as_screenshot_get_caption (sshot);
		if (g_strcmp0 (str, "") != 0) {
			xmlNode* n_caption;
			n_caption = xmlNewTextChild (subnode, NULL, (xmlChar*) "caption", (xmlChar*) str);
			xmlAddChild (subnode, n_caption);
		}

		images = as_screenshot_get_images (sshot);
		g_ptr_array_foreach (images, (GFunc) _as_component_serialize_image, subnode);
	}
}

/**
 * as_component_dump_screenshot_data_xml:
 *
 * Internal function to create XML which gets stored in the AppStream database
 * for screenshots
 */
gchar*
as_component_dump_screenshot_data_xml (AsComponent *cpt)
{
	GPtrArray* sslist;
	xmlDoc *doc;
	xmlNode *root;
	gchar *xmlstr = NULL;
	g_return_val_if_fail (cpt != NULL, NULL);

	sslist = as_component_get_screenshots (cpt);
	if (sslist->len == 0) {
		return g_strdup ("");
	}

	doc = xmlNewDoc ((xmlChar*) NULL);
	root = xmlNewNode (NULL, (xmlChar*) "screenshots");
	xmlDocSetRootElement (doc, root);

	as_component_xml_add_screenshot_subnodes (cpt, root);

	xmlDocDumpMemory (doc, (xmlChar**) (&xmlstr), NULL);
	xmlFreeDoc (doc);

	return xmlstr;
}

/**
 * as_component_load_screenshots_from_internal_xml:
 *
 * Internal function to load the screenshot list
 * using the database-internal XML data.
 */
void
as_component_load_screenshots_from_internal_xml (AsComponent *cpt, const gchar* xmldata)
{
	xmlDoc* doc = NULL;
	xmlNode* root = NULL;
	xmlNode *iter;
	g_return_if_fail (cpt != NULL);
	g_return_if_fail (xmldata != NULL);

	if (as_str_empty (xmldata)) {
		return;
	}

	doc = xmlParseDoc ((xmlChar*) xmldata);
	root = xmlDocGetRootElement (doc);

	if (root == NULL) {
		xmlFreeDoc (doc);
		return;
	}

	for (iter = root->children; iter != NULL; iter = iter->next) {
		if (g_strcmp0 ((gchar*) iter->name, "screenshot") == 0) {
			AsScreenshot* sshot;
			gchar *typestr;
			xmlNode *iter2;

			sshot = as_screenshot_new ();
			typestr = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (g_strcmp0 (typestr, "default") == 0)
				as_screenshot_set_kind (sshot, AS_SCREENSHOT_KIND_DEFAULT);
			else
				as_screenshot_set_kind (sshot, AS_SCREENSHOT_KIND_NORMAL);
			g_free (typestr);
			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				const gchar *node_name;
				gchar *content;

				node_name = (const gchar*) iter2->name;
				content = (gchar*) xmlNodeGetContent (iter2);
				if (g_strcmp0 (node_name, "image") == 0) {
					AsImage *img;
					gchar *str;
					guint64 width;
					guint64 height;
					gchar *imgtype;
					if (content == NULL)
						continue;
					img = as_image_new ();

					str = (gchar*) xmlGetProp (iter2, (xmlChar*) "width");
					if (str == NULL) {
						g_object_unref (img);
						continue;
					}
					width = g_ascii_strtoll (str, NULL, 10);
					g_free (str);

					str = (gchar*) xmlGetProp (iter2, (xmlChar*) "height");
					if (str == NULL) {
						g_object_unref (img);
						continue;
					}
					height = g_ascii_strtoll (str, NULL, 10);
					g_free (str);

					as_image_set_width (img, width);
					as_image_set_height (img, height);

					/* discard invalid elements */
					if ((width == 0) || (height == 0)) {
						g_object_unref (img);
						continue;
					}
					as_image_set_url (img, content);

					imgtype = (gchar*) xmlGetProp (iter2, (xmlChar*) "type");
					if (g_strcmp0 (imgtype, "thumbnail") == 0) {
						as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
					} else {
						as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
					}
					g_free (imgtype);

					as_screenshot_add_image (sshot, img);
				} else if (g_strcmp0 (node_name, "caption") == 0) {
					if (content != NULL)
						as_screenshot_set_caption (sshot, content);
				}
				g_free (content);
			}
			as_component_add_screenshot (cpt, sshot);
		}
	}
}

/**
 * as_component_xml_add_release_subnodes:
 *
 * Add release nodes to a root node
 */
static void
as_component_xml_add_release_subnodes (AsComponent *cpt, xmlNode *root)
{
	GPtrArray* releases;
	AsRelease *release;
	guint i;

	releases = as_component_get_releases (cpt);
	for (i = 0; i < releases->len; i++) {
		xmlNode *subnode;
		const gchar *str;
		gchar *timestamp;
		release = (AsRelease*) g_ptr_array_index (releases, i);

		subnode = xmlNewTextChild (root, NULL, (xmlChar*) "release", (xmlChar*) "");
		xmlNewProp (subnode, (xmlChar*) "version",
					(xmlChar*) as_release_get_version (release));
		timestamp = g_strdup_printf ("%ld", as_release_get_timestamp (release));
		xmlNewProp (subnode, (xmlChar*) "timestamp",
					(xmlChar*) timestamp);
		g_free (timestamp);

		str = as_release_get_description (release);
		if (g_strcmp0 (str, "") != 0) {
			xmlNode* n_desc;
			n_desc = xmlNewTextChild (subnode, NULL, (xmlChar*) "description", (xmlChar*) str);
			xmlAddChild (subnode, n_desc);
		}
	}
}

/**
 * as_component_dump_releases_data_xml:
 *
 * Internal function to create XML which gets stored in the AppStream database
 * for releases
 */
gchar*
as_component_dump_releases_data_xml (AsComponent *cpt)
{
	GPtrArray* releases;
	xmlDoc *doc;
	xmlNode *root;
	gchar *xmlstr = NULL;
	g_return_val_if_fail (cpt != NULL, NULL);

	releases = as_component_get_releases (cpt);
	if (releases->len == 0) {
		return g_strdup ("");
	}

	doc = xmlNewDoc ((xmlChar*) NULL);
	root = xmlNewNode (NULL, (xmlChar*) "releases");
	xmlDocSetRootElement (doc, root);

	as_component_xml_add_release_subnodes (cpt, root);

	xmlDocDumpMemory (doc, (xmlChar**) (&xmlstr), NULL);
	xmlFreeDoc (doc);

	return xmlstr;
}

/**
 * as_component_load_releases_from_internal_xml:
 *
 * Internal function to load the releases list
 * using the database-internal XML data.
 */
void
as_component_load_releases_from_internal_xml (AsComponent *cpt, const gchar* xmldata)
{
	xmlDoc* doc = NULL;
	xmlNode* root = NULL;
	xmlNode *iter;
	g_return_if_fail (cpt != NULL);
	g_return_if_fail (xmldata != NULL);

	if (as_str_empty (xmldata)) {
		return;
	}

	doc = xmlParseDoc ((xmlChar*) xmldata);
	root = xmlDocGetRootElement (doc);

	if (root == NULL) {
		xmlFreeDoc (doc);
		return;
	}

	for (iter = root->children; iter != NULL; iter = iter->next) {
		if (g_strcmp0 ((gchar*) iter->name, "release") == 0) {
			AsRelease *release;
			gchar *prop;
			guint64 timestamp;
			xmlNode *iter2;
			release = as_release_new ();

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "version");
			as_release_set_version (release, prop);
			g_free (prop);

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "timestamp");
			timestamp = g_ascii_strtoll (prop, NULL, 10);
			as_release_set_timestamp (release, timestamp);
			g_free (prop);

			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (iter->type != XML_ELEMENT_NODE)
					continue;

				if (g_strcmp0 ((gchar*) iter->name, "description") == 0) {
					gchar *content;
					content = (gchar*) xmlNodeGetContent (iter2);
					as_release_set_description (release, content);
					g_free (content);
					break;
				}
			}

			as_component_add_release (cpt, release);
			g_object_unref (release);
		}
	}
}

/**
 * as_component_xml_add_node:
 *
 * Add node if value is not empty
 */
static xmlNode*
_as_component_xml_add_node (xmlNode *root, const gchar *name, const gchar *value)
{
	if (as_str_empty (value))
		return NULL;

	return xmlNewTextChild (root, NULL, (xmlChar*) name, (xmlChar*) value);
}

/**
 * _as_component_xml_add_description:
 *
 * Add the description markup to the XML tree
 */
static gboolean
_as_component_xml_add_description (xmlNode *root, const gchar *description_markup)
{
	gchar *xmldata;
	xmlDoc *doc;
	xmlNode *droot;
	xmlNode *dnode;
	xmlNode *iter;
	gboolean ret = TRUE;

	if (as_str_empty (description_markup))
		return FALSE;

	xmldata = g_strdup_printf ("<root>%s</root>", description_markup);
	doc = xmlParseDoc ((xmlChar*) xmldata);
	if (doc == NULL) {
		ret = FALSE;
		goto out;
	}

	droot = xmlDocGetRootElement (doc);
	if (droot == NULL) {
		ret = FALSE;
		goto out;
	}
	dnode = xmlNewChild (root, NULL, (xmlChar*) "description", NULL);

	for (iter = droot->children; iter != NULL; iter = iter->next) {
		xmlAddChild (dnode, xmlCopyNode (iter, TRUE));
	}

out:
	if (doc != NULL)
		xmlFreeDoc (doc);
	g_free (xmldata);
	return ret;
}

/**
 * as_component_xml_add_node_list:
 *
 * Add node if value is not empty
 */
static void
_as_component_xml_add_node_list (xmlNode *root, const gchar *name, const gchar *child_name, gchar **strv)
{
	xmlNode *node;
	guint i;

	if (strv == NULL)
		return;

	if (name == NULL)
		node = root;
	else
		node = xmlNewTextChild (root, NULL, (xmlChar*) name, NULL);
	for (i = 0; strv[i] != NULL; i++) {
		xmlNewTextChild (node, NULL, (xmlChar*) child_name, (xmlChar*) strv[i]);
	}
}

/**
 * as_component_to_xml:
 * @cpt: a valid #AsComponent
 *
 * Serialize the component data to XML.
 * Note that this will produce an unlocalized file only, using the
 * language which was selected when this component was obtained from the
 * database. You will not receive the source XML back.
 *
 * Returns: (transfer full): A string containing the XML. Free with g_free()
 */
gchar*
as_component_to_xml (AsComponent *cpt)
{
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *node;
	gchar **strv;
	gchar *xmlstr = NULL;
	AsComponentPrivate *priv = cpt->priv;
	g_return_val_if_fail (cpt != NULL, NULL);

	doc = xmlNewDoc ((xmlChar*) NULL);

	/* define component root node */
	root = xmlNewNode (NULL, (xmlChar*) "component");
	if ((priv->kind != AS_COMPONENT_KIND_GENERIC) && (priv->kind != AS_COMPONENT_KIND_UNKNOWN)) {
		xmlNewProp (root, (xmlChar*) "type",
					(xmlChar*) as_component_kind_to_string (priv->kind));
	}
	xmlDocSetRootElement (doc, root);

	_as_component_xml_add_node (root, "id", priv->id);
	_as_component_xml_add_node (root, "name", priv->name);
	_as_component_xml_add_node (root, "summary", priv->summary);
	_as_component_xml_add_node (root, "project_license", priv->project_license);
	_as_component_xml_add_node (root, "project_group", priv->project_group);
	_as_component_xml_add_node (root, "developer_name", priv->developer_name);
	_as_component_xml_add_description (root, priv->description);

	_as_component_xml_add_node_list (root, NULL, "pkgname", priv->pkgnames);
	strv = as_ptr_array_to_strv (priv->extends);
	_as_component_xml_add_node_list (root, NULL, "extends", strv);
	g_strfreev (strv);
	_as_component_xml_add_node_list (root, NULL, "compulsory_for_desktop", priv->compulsory_for_desktops);
	_as_component_xml_add_node_list (root, "keywords", "keyword", priv->keywords);
	_as_component_xml_add_node_list (root, "categories", "category", priv->categories);

	/* releases node */
	if (priv->releases->len > 0) {
		node = xmlNewTextChild (root, NULL, (xmlChar*) "releases", NULL);
		as_component_xml_add_release_subnodes (cpt, node);
	}

	/* screenshots node */
	if (priv->releases->len > 0) {
		node = xmlNewTextChild (root, NULL, (xmlChar*) "screenshots", NULL);
		as_component_xml_add_screenshot_subnodes (cpt, node);
	}

	xmlDocDumpMemory (doc, (xmlChar**) (&xmlstr), NULL);
	xmlFreeDoc (doc);

	return xmlstr;
}

/**
 * as_component_provides_item:
 * @cpt: a valid #AsComponent
 * @kind: the kind of the provides-item
 * @value: the value of the provides-item
 *
 * Checks if this component provides an item of the specified type
 *
 * Returns: %TRUE if an item was found
 */
gboolean
as_component_provides_item (AsComponent *cpt, AsProvidesKind kind, const gchar *value)
{
	guint i;
	gboolean ret = FALSE;
	gchar *item;
	AsComponentPrivate *priv = cpt->priv;

	item = as_provides_item_create (kind, value, "");
	for (i = 0; i < priv->provided_items->len; i++) {
		gchar *cval;
		cval = (gchar*) g_ptr_array_index (priv->provided_items, i);
		if (g_strcmp0 (item, cval) == 0) {
			ret = TRUE;
			break;
		}
	}

	g_free (item);
	return ret;
}

/**
 * as_component_get_kind:
 *
 * Returns the #AsComponentKind of this component.
 */
AsComponentKind
as_component_get_kind (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, 0);
	return cpt->priv->kind;
}

/**
 * as_component_set_kind:
 *
 * Sets the #AsComponentKind of this component.
 */
void
as_component_set_kind (AsComponent *cpt, AsComponentKind value)
{
	g_return_if_fail (cpt != NULL);

	cpt->priv->kind = value;
	g_object_notify ((GObject *) cpt, "kind");
}

/**
 * as_component_get_pkgnames:
 *
 * Get a list of package names which this component consists of.
 * This usually is just one package name.
 *
 * Returns: (transfer none): String array of package names
 */
gchar**
as_component_get_pkgnames (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->pkgnames;
}

/**
 * as_component_set_pkgnames:
 * @value: (array zero-terminated=1):
 *
 * Set a list of package names this component consists of.
 * (This should usually be just one package name)
 */
void
as_component_set_pkgnames (AsComponent *cpt, gchar** value)
{
	g_return_if_fail (cpt != NULL);

	g_strfreev (cpt->priv->pkgnames);
	cpt->priv->pkgnames = g_strdupv (value);
	g_object_notify ((GObject *) cpt, "pkgnames");
}

/**
 * as_component_get_id:
 *
 * Set the unique identifier for this component.
 */
const gchar*
as_component_get_id (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->id;
}

/**
 * as_component_set_id:
 *
 * Set the unique identifier for this component.
 */
void
as_component_set_id (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	g_free (cpt->priv->id);
	cpt->priv->id = g_strdup (value);
	g_object_notify ((GObject *) cpt, "id");
}

/**
 * as_component_get_origin:
 */
const gchar*
as_component_get_origin (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->origin;
}

/**
 * as_component_set_origin:
 */
void
as_component_set_origin (AsComponent *cpt, const gchar* origin)
{
	g_return_if_fail (cpt != NULL);

	/* safety measure, so we never set this to NULL */
	if (origin == NULL)
		origin = "";
	g_free (cpt->priv->origin);
	cpt->priv->origin = g_strdup (origin);
}

/**
 * as_component_get_name:
 */
const gchar*
as_component_get_name (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	if (as_str_empty (cpt->priv->name)) {
		cpt->priv->name = g_strdup (cpt->priv->name_original);
	}

	return cpt->priv->name;
}

/**
 * as_component_set_name:
 */
void
as_component_set_name (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	/* safety measure, so we can always convert this to a C++ string */
	if (value == NULL)
		value = "";

	g_free (cpt->priv->name);
	cpt->priv->name = g_strdup (value);
	g_object_notify ((GObject *) cpt, "name");
}

const gchar*
as_component_get_name_original (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->name_original;
}

void
as_component_set_name_original (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	/* safety measure, so we can always convert this to a C++ string */
	if (value == NULL)
		value = "";

	g_free (cpt->priv->name_original);
	cpt->priv->name_original = g_strdup (value);
	g_object_notify ((GObject *) cpt, "name-original");
}

const gchar*
as_component_get_summary (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);

	return cpt->priv->summary;
}

void
as_component_set_summary (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	/* safety measure, so we can always convert this to a C++ string */
	if (value == NULL)
		value = "";

	g_free (cpt->priv->summary);
	cpt->priv->summary = g_strdup (value);
	g_object_notify ((GObject *) cpt, "summary");
}

const gchar*
as_component_get_description (AsComponent *cpt)
{
	const gchar* result;
	const gchar* _tmp0_ = NULL;
	g_return_val_if_fail (cpt != NULL, NULL);
	_tmp0_ = cpt->priv->description;
	result = _tmp0_;
	return result;
}

void
as_component_set_description (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	/* safety measure, so we can always convert this to a C++ string */
	if (value == NULL)
		value = "";

	g_free (cpt->priv->description);
	cpt->priv->description = g_strdup (value);
	g_object_notify ((GObject *) cpt, "description");
}

/**
 * as_component_get_keywords:
 *
 * Returns: (transfer none): String array of keywords
 */
gchar**
as_component_get_keywords (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->keywords;
}

/**
 * as_component_set_keywords:
 * @value: (array zero-terminated=1):
 */
void
as_component_set_keywords (AsComponent *cpt, gchar** value)
{
	g_return_if_fail (cpt != NULL);

	g_strfreev (cpt->priv->keywords);
	cpt->priv->keywords = g_strdupv (value);
	g_object_notify ((GObject *) cpt, "keywords");
}

/**
 * as_component_get_icon:
 * @cpt: an #AsComponent instance
 *
 * Returns the icon name for this component. This is usually
 * a stock icon name, e.g. "applications-science"
 */
const gchar*
as_component_get_icon (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->icon;
}

/**
 * as_component_set_icon:
 * @cpt: an #AsComponent instance
 *
 * Set a stock icon name for this component,
 * e.g. "applications-science"
 */
void
as_component_set_icon (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	/* safety measure, so we can always convert this to a C++ string */
	if (value == NULL)
		value = "";

	g_free (cpt->priv->icon);
	cpt->priv->icon = g_strdup (value);
	g_object_notify ((GObject *) cpt, "icon");
}

/**
 * as_component_get_icon_url:
 * @cpt: an #AsComponent instance
 *
 * Returns the full url of this icon, e.g.
 * "/usr/share/icons/hicolor/64x64/foobar.png"
 * This might also be an http url pointing at a remote location.
 *
 * The icon will have the default icon size, which is usually 64x64 pixels.
 */
const gchar*
as_component_get_icon_url (AsComponent *cpt)
{
	gchar *icon_url;
	g_return_val_if_fail (cpt != NULL, NULL);

	icon_url = g_hash_table_lookup (cpt->priv->icon_urls, "64x64");
	if (icon_url == NULL)
		return "";
	return icon_url;
}

/**
 * as_component_add_icon_url:
 * @cpt: an #AsComponent instance
 * @width: An icon width
 * @height: An icon height
 * @value: The full icon url
 *
 * Set an icon url for this component, which can be a remote
 * or local location.
 *
 * Since: 0.7.4
 */
void
as_component_add_icon_url (AsComponent *cpt, int width, int height, const gchar* value)
{
	gchar *size;
	g_return_if_fail (cpt != NULL);

	/* safety measure, to protect against invalid path values */
	if (value == NULL)
		value = "";

	size = g_strdup_printf ("%ix%i", width, height);
	g_hash_table_insert (cpt->priv->icon_urls, size, g_strdup (value));
}

/**
 * as_component_get_icon_url_for_size:
 * @cpt: an #AsComponent instance
 * @width: An icon width
 * @height: An icon height
 *
 * Returns the full url for an icon with the given width and height.
 * In case no icon matching the size is found, %NULL is returned.
 * The returned path will either be a http link or an absolute, local
 * path to the image file of the icon.
 *
 * Since: 0.7.4
 */
const gchar*
as_component_get_icon_url_for_size (AsComponent *cpt, int width, int height)
{
	gchar *size;
	gchar *icon_url;
	g_return_val_if_fail (cpt != NULL, NULL);

	size = g_strdup_printf ("%ix%i", width, height);
	icon_url = g_hash_table_lookup (cpt->priv->icon_urls, size);
	g_free (size);

	return icon_url;
}

/**
 * as_component_get_icon_urls:
 * @cpt: a #AsComponent instance.
 *
 * Gets the icon-urls has table for the component.
 *
 * Returns: (transfer none): A hash map of icon urls and sizes
 *
 * Since: 0.7.4
 **/
GHashTable*
as_component_get_icon_urls (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->icon_urls;
}

/**
 * as_component_get_categories:
 *
 * Returns: (transfer none): String array of categories
 */
gchar**
as_component_get_categories (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->categories;
}

/**
 * as_component_set_categories:
 * @value: (array zero-terminated=1):
 */
void
as_component_set_categories (AsComponent *cpt, gchar** value)
{
	g_return_if_fail (cpt != NULL);

	g_strfreev (cpt->priv->categories);
	cpt->priv->categories = g_strdupv (value);
	g_object_notify ((GObject *) cpt, "categories");
}

/**
 * as_component_set_categories_from_str:
 *
 * Set the categories list from a string
 *
 * @cpt: a valid #AsComponent instance
 * @categories_str: Semicolon-separated list of category-names
 */
void
as_component_set_categories_from_str (AsComponent *cpt, const gchar* categories_str)
{
	gchar** cats = NULL;

	g_return_if_fail (cpt != NULL);
	g_return_if_fail (categories_str != NULL);

	cats = g_strsplit (categories_str, ";", 0);
	as_component_set_categories (cpt, cats);
	g_strfreev (cats);
}

/**
 * as_component_has_category:
 * @cpt: an #AsComponent object
 *
 * Check if component is in the specified category.
 **/
gboolean
as_component_has_category (AsComponent *cpt, const gchar* category)
{
	gchar **categories;
	guint i;
	g_return_val_if_fail (cpt != NULL, FALSE);

	categories = cpt->priv->categories;
	for (i = 0; categories[i] != NULL; i++) {
		if (g_strcmp0 (categories[i], category) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * as_component_get_project_license:
 *
 * Get the license of the project this component belongs to.
 */
const gchar*
as_component_get_project_license (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->project_license;
}

/**
 * as_component_set_project_license:
 *
 * Set the project license.
 */
void
as_component_set_project_license (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	g_free (cpt->priv->project_license);
	cpt->priv->project_license = g_strdup (value);
	g_object_notify ((GObject *) cpt, "project-license");
}

/**
 * as_component_get_project_group:
 *
 * Get the component's project group.
 */
const gchar*
as_component_get_project_group (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->project_group;
}

/**
 * as_component_set_project_group:
 *
 * Set the component's project group.
 */
void
as_component_set_project_group (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	g_free (cpt->priv->project_group);
	cpt->priv->project_group = g_strdup (value);
}

/**
 * as_component_get_developer_name:
 *
 * Get the component's developer or development team name.
 */
const gchar*
as_component_get_developer_name (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);
	return cpt->priv->developer_name;
}

/**
 * as_component_set_developer_name:
 *
 * Set the the component's developer or development team name.
 */
void
as_component_set_developer_name (AsComponent *cpt, const gchar* value)
{
	g_return_if_fail (cpt != NULL);

	g_free (cpt->priv->developer_name);
	cpt->priv->developer_name = g_strdup (value);
}

/**
 * as_component_get_screenshots:
 *
 * Get a list of associated screenshots.
 *
 * Returns: (element-type AsScreenshot) (transfer none): an array of #AsScreenshot instances
 */
GPtrArray*
as_component_get_screenshots (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);

	return cpt->priv->screenshots;
}

/**
 * as_component_get_compulsory_for_desktops:
 *
 * Return value: (transfer none): A list of desktops where this component is compulsory
 **/
gchar **
as_component_get_compulsory_for_desktops (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);

	return cpt->priv->compulsory_for_desktops;
}

/**
 * as_component_set_compulsory_for_desktops:
 *
 * Set a list of desktops where this component is compulsory.
 **/
void
as_component_set_compulsory_for_desktops (AsComponent *cpt, gchar** value)
{
	g_return_if_fail (cpt != NULL);

	g_strfreev (cpt->priv->compulsory_for_desktops);
	cpt->priv->compulsory_for_desktops = g_strdupv (value);
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
	g_return_val_if_fail (cpt != NULL, FALSE);

	compulsory_for_desktops = cpt->priv->compulsory_for_desktops;
	for (i = 0; compulsory_for_desktops[i] != NULL; i++) {
		if (g_strcmp0 (compulsory_for_desktops[i], desktop) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * as_component_get_provided_items:
 *
 * Get an array of the provides-items this component is
 * associated with.
 *
 * Return value: (element-type utf8) (transfer none): A list of desktops where this component is compulsory
 **/
GPtrArray*
as_component_get_provided_items (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);

	return cpt->priv->provided_items;
}

/**
 * as_component_add_provided_item:
 * @cpt: a #AsComponent instance.
 * @kind: the kind of the provided item (e.g. %AS_PROVIDES_KIND_MIMETYPE)
 * @data: (allow-none) (default NULL): additional data associated with this item, or %NULL.
 *
 * Adds a provided item to the component.
 *
 * Since: 0.6.2
 **/
void
as_component_add_provided_item (AsComponent *cpt, AsProvidesKind kind, const gchar *value, const gchar *data)
{
	g_return_if_fail (cpt != NULL);
	/* we just skip empty items */
	if (as_str_empty (value))
		return;
	g_ptr_array_add (cpt->priv->provided_items,
			     as_provides_item_create (kind, value, data));
}

/**
 * as_component_get_releases:
 *
 * Get an array of the #AsRelease items this component
 * provides.
 *
 * Return value: (element-type AsRelease) (transfer none): A list of releases
 **/
GPtrArray*
as_component_get_releases (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, NULL);

	return cpt->priv->releases;
}

/**
 * as_component_get_priority:
 *
 * Returns the priority of this component.
 * This method is used internally.
 *
 * Since: 0.6.1
 */
int
as_component_get_priority (AsComponent *cpt)
{
	g_return_val_if_fail (cpt != NULL, 0);
	return cpt->priv->priority;
}

/**
 * as_component_set_priority:
 *
 * Sets the priority of this component.
 * This method is used internally.
 *
 * Since: 0.6.1
 */
void
as_component_set_priority (AsComponent *cpt, int priority)
{
	g_return_if_fail (cpt != NULL);
	cpt->priv->priority = priority;
}

/**
 * as_component_add_language:
 * @cpt: an #AsComponent instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @percentage: the percentage completion of the translation, 0 for locales with unknown amount of translation
 *
 * Adds a language to the component.
 *
 * Since: 0.7.0
 **/
void
as_component_add_language (AsComponent *cpt, const gchar *locale, gint percentage)
{
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (cpt->priv->languages,
						 g_strdup (locale),
						 GINT_TO_POINTER (percentage));
}

/**
 * as_component_get_language:
 * @cpt: an #AsComponent instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
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

	if (locale == NULL)
		locale = "C";
	ret = g_hash_table_lookup_extended (cpt->priv->languages,
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
	return g_hash_table_get_keys (cpt->priv->languages);
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
	return cpt->priv->languages;
}

/**
 * as_component_refine_icon:
 *
 * We use this method to ensure the "icon" and "icon_url" properties of
 * a component are properly set, by finding the icons in default directories.
 */
void
as_component_refine_icon (AsComponent *cpt, gchar **icon_paths)
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
	gchar *icon_url = NULL;
	guint i, j, k;
	AsComponentPrivate *priv = cpt->priv;

	/* See if we have an icon without known size.
	 * These icons have a zero-dimensional width and height (therefore the "0x0" key)
	 */
	icon_url = g_strdup (g_hash_table_lookup (priv->icon_urls, "0x0"));
	if (icon_url == NULL) {
		/* okay, see if we have a stock icon */
		icon_url = g_strdup (as_component_get_icon (cpt));
		if ((icon_url == NULL) || (g_strcmp0 (icon_url, "") == 0)) {
			/* nothing to do... */
			return;
		}
	}
	g_hash_table_remove (priv->icon_urls, "0x0");

	if (g_str_has_prefix (icon_url, "/") ||
		g_str_has_prefix (icon_url, "http://")) {
		/* looks like this component already has a full icon path,
		 * or is a weblink. We assume 64x64 in that case
		 */
		as_component_add_icon_url (cpt, 64, 64, icon_url);
		goto out;
	}
	tmp_icon_path = NULL;

	/* search local icon path */
	for (i = 0; icon_paths[i] != NULL; i++) {
		for (j = 0; sizes[j] != NULL; j++) {
			/* sometimes, the file already has an extension */
			tmp_icon_path = g_strdup_printf ("%s/%s/%s/%s",
							icon_paths[i],
							priv->origin,
							sizes[j],
							icon_url);
			if (g_file_test (tmp_icon_path, G_FILE_TEST_EXISTS)) {
				/* we have an icon! */
				if (g_strcmp0 (sizes[j], "") == 0) {
					/* old icon directory, so assume 64x64 */
					as_component_add_icon_url (cpt, 64, 64, g_strdup (tmp_icon_path));
				} else {
					g_hash_table_insert (priv->icon_urls, g_strdup (sizes[j]), g_strdup (tmp_icon_path));
				}
				g_free (tmp_icon_path);
				continue;
			}
			g_free (tmp_icon_path);

			/* file not found, try extensions (we will not do this forever, better fix AppStream data!) */
			for (k = 0; exensions[k] != NULL; k++) {
				tmp_icon_path = g_strdup_printf ("%s/%s/%s/%s.%s",
							icon_paths[i],
							priv->origin,
							sizes[j],
							icon_url,
							exensions[k]);
				if (g_file_test (tmp_icon_path, G_FILE_TEST_EXISTS)) {
					/* we have an icon! */
					if (g_strcmp0 (sizes[j], "") == 0) {
						/* old icon directory, so assume 64x64 */
						as_component_add_icon_url (cpt, 64, 64, g_strdup (tmp_icon_path));
					} else {
						g_hash_table_insert (priv->icon_urls, g_strdup (sizes[j]), g_strdup (tmp_icon_path));
					}
				}
				g_free (tmp_icon_path);
				tmp_icon_path = NULL;
			}
		}
	}

out:
	if (icon_url != NULL)
		g_free (icon_url);
	if (tmp_icon_path != NULL) {
		g_free (tmp_icon_path);
	}
}

/**
 * as_component_complete:
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
	AsComponentPrivate *priv = cpt->priv;

	/* we want screenshot data from 3rd-party screenshot servers, if the component doesn't have screenshots defined already */
	if ((priv->screenshots->len == 0) && (priv->pkgnames != NULL)) {
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
	as_component_refine_icon (cpt, icon_paths);
}

static void
as_component_class_init (AsComponentClass * klass)
{
	as_component_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsComponentPrivate));
	G_OBJECT_CLASS (klass)->get_property = as_component_get_property;
	G_OBJECT_CLASS (klass)->set_property = as_component_set_property;
	G_OBJECT_CLASS (klass)->finalize = as_component_finalize;
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_KIND, g_param_spec_enum ("kind", "kind", "kind", AS_TYPE_COMPONENT_KIND, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_PKGNAMES, g_param_spec_boxed ("pkgnames", "pkgnames", "pkgnames", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_ID, g_param_spec_string ("id", "id", "id", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_NAME, g_param_spec_string ("name", "name", "name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_NAME_ORIGINAL, g_param_spec_string ("name-original", "name-original", "name-original", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_SUMMARY, g_param_spec_string ("summary", "summary", "summary", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_DESCRIPTION, g_param_spec_string ("description", "description", "description", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_KEYWORDS, g_param_spec_boxed ("keywords", "keywords", "keywords", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_ICON, g_param_spec_string ("icon", "icon", "icon", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_ICON_URLS, g_param_spec_boxed ("icon-urls", "icon-urls", "icon-urls", G_TYPE_HASH_TABLE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_URLS, g_param_spec_boxed ("urls", "urls", "urls", G_TYPE_HASH_TABLE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_CATEGORIES, g_param_spec_boxed ("categories", "categories", "categories", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_PROJECT_LICENSE, g_param_spec_string ("project-license", "project-license", "project-license", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_PROJECT_GROUP, g_param_spec_string ("project-group", "project-group", "project-group", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_DEVELOPER_NAME, g_param_spec_string ("developer-name", "developer-name", "developer-name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_SCREENSHOTS, g_param_spec_boxed ("screenshots", "screenshots", "screenshots", G_TYPE_PTR_ARRAY, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}

static void
as_component_instance_init (AsComponent *cpt)
{
	cpt->priv = AS_COMPONENT_GET_PRIVATE (cpt);
}

/**
 * as_component_get_type:
 *
 * Class to store data describing a component in AppStream
 */
GType
as_component_get_type (void)
{
	static volatile gsize as_component_type_id__volatile = 0;
	if (g_once_init_enter (&as_component_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
					sizeof (AsComponentClass),
					(GBaseInitFunc) NULL,
					(GBaseFinalizeFunc) NULL,
					(GClassInitFunc) as_component_class_init,
					(GClassFinalizeFunc) NULL,
					NULL,
					sizeof (AsComponent),
					0,
					(GInstanceInitFunc) as_component_instance_init,
					NULL
		};
		GType as_component_type_id;
		as_component_type_id = g_type_register_static (G_TYPE_OBJECT, "AsComponent", &g_define_type_info, 0);
		g_once_init_leave (&as_component_type_id__volatile, as_component_type_id);
	}
	return as_component_type_id__volatile;
}

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
		case AS_COMPONENT_NAME_ORIGINAL:
			g_value_set_string (value, as_component_get_name_original (cpt));
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
		case AS_COMPONENT_ICON:
			g_value_set_string (value, as_component_get_icon (cpt));
			break;
		case AS_COMPONENT_ICON_URLS:
			g_value_set_boxed (value, as_component_get_icon_urls (cpt));
			break;
		case AS_COMPONENT_URLS:
			g_value_set_boxed (value, as_component_get_urls (cpt));
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
			as_component_set_name (cpt, g_value_get_string (value));
			break;
		case AS_COMPONENT_NAME_ORIGINAL:
			as_component_set_name_original (cpt, g_value_get_string (value));
			break;
		case AS_COMPONENT_SUMMARY:
			as_component_set_summary (cpt, g_value_get_string (value));
			break;
		case AS_COMPONENT_DESCRIPTION:
			as_component_set_description (cpt, g_value_get_string (value));
			break;
		case AS_COMPONENT_KEYWORDS:
			as_component_set_keywords (cpt, g_value_get_boxed (value));
			break;
		case AS_COMPONENT_ICON:
			as_component_set_icon (cpt, g_value_get_string (value));
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
			as_component_set_developer_name (cpt, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}
