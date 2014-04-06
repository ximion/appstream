/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "as-utils.h"

struct _AsComponentPrivate {
	AsComponentKind kind;
	gchar *pkgname;
	gchar *idname;
	gchar *name;
	gchar *name_original;
	gchar *summary;
	gchar *description;
	gchar **keywords;
	gchar *icon;
	gchar *icon_url;
	gchar *homepage;
	gchar **categories;
	gchar **mimetypes;
	gchar *project_license;
	gchar *project_group;
	gchar **compulsory_for_desktops;
	GPtrArray *screenshots; /* AsScreenshot elements */
	GPtrArray *provides_items; /* utf8 */
};

static gpointer as_component_parent_class = NULL;

#define AS_COMPONENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_COMPONENT, AsComponentPrivate))

enum  {
	AS_COMPONENT_DUMMY_PROPERTY,
	AS_COMPONENT_KIND,
	AS_COMPONENT_PKGNAME,
	AS_COMPONENT_IDNAME,
	AS_COMPONENT_NAME,
	AS_COMPONENT_NAME_ORIGINAL,
	AS_COMPONENT_SUMMARY,
	AS_COMPONENT_DESCRIPTION,
	AS_COMPONENT_KEYWORDS,
	AS_COMPONENT_ICON,
	AS_COMPONENT_ICON_URL,
	AS_COMPONENT_HOMEPAGE,
	AS_COMPONENT_CATEGORIES,
	AS_COMPONENT_MIMETYPES,
	AS_COMPONENT_PROJECT_LICENSE,
	AS_COMPONENT_SCREENSHOTS
};

static void as_component_finalize (GObject* obj);
static void as_component_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void as_component_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);

/**
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
					{AS_COMPONENT_KIND_DESKTOP_APP, "AS_COMPONENT_KIND_DESKTOP_APP", "desktop-app"},
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
 * @kind: the #AsComponentKind.
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
		return "desktop-app";
	if (kind == AS_COMPONENT_KIND_FONT)
		return "font";
	if (kind == AS_COMPONENT_KIND_CODEC)
		return "codec";
	if (kind == AS_COMPONENT_KIND_INPUTMETHOD)
		return "inputmethod";
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
	if (g_strcmp0 (kind_str, "desktop-app") == 0)
		return AS_COMPONENT_KIND_DESKTOP_APP;
	if (g_strcmp0 (kind_str, "font") == 0)
		return AS_COMPONENT_KIND_FONT;
	if (g_strcmp0 (kind_str, "codec") == 0)
		return AS_COMPONENT_KIND_CODEC;
	if (g_strcmp0 (kind_str, "inputmethod") == 0)
		return AS_COMPONENT_KIND_INPUTMETHOD;
	return AS_COMPONENT_KIND_UNKNOWN;
}

AsComponent*
as_component_construct (GType object_type)
{
	AsComponent * self = NULL;
	gchar** strv;
	self = (AsComponent*) g_object_new (object_type, NULL);
	as_component_set_pkgname (self, "");
	as_component_set_idname (self, "");
	as_component_set_name_original (self, "");
	as_component_set_summary (self, "");
	as_component_set_description (self, "");
	as_component_set_homepage (self, "");
	as_component_set_icon (self, "");
	as_component_set_icon_url (self, "");
	as_component_set_project_license (self, "");
	as_component_set_project_group (self, "");
	self->priv->keywords = NULL;

	strv = g_new0 (gchar*, 1 + 1);
	strv[0] = NULL;
	as_component_set_categories (self, strv);
	self->priv->screenshots = g_ptr_array_new_with_free_func (g_object_unref);
	self->priv->provides_items = g_ptr_array_new_with_free_func (g_free);

	return self;
}

/**
 * as_component_new:
 *
 * Creates a new #AsComponent.
 *
 * Returns: (transfer full): an #AsComponent
 *
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
as_component_is_valid (AsComponent* self)
{
	gboolean ret = FALSE;
	AsComponentKind ctype;

	g_return_val_if_fail (self != NULL, FALSE);

	ctype = self->priv->kind;
	if (ctype == AS_COMPONENT_KIND_UNKNOWN)
		return FALSE;

	if ((g_strcmp0 (self->priv->pkgname, "") != 0) &&
		(g_strcmp0 (self->priv->idname, "") != 0) &&
		(g_strcmp0 (self->priv->name, "") != 0) &&
		(g_strcmp0 (self->priv->name_original, "") != 0)) {
		ret = TRUE;
		}

#if 0
	if ((ret) && ctype == AS_COMPONENT_KIND_DESKTOP_APP) {
		ret = g_strcmp0 (self->priv->desktop_file, "") != 0;
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
as_component_to_string (AsComponent* self)
{
	gchar* res = NULL;
	const gchar *name;
	g_return_val_if_fail (self != NULL, NULL);

	name = as_component_get_name (self);
	switch (self->priv->kind) {
		case AS_COMPONENT_KIND_DESKTOP_APP:
		{
			res = g_strdup_printf ("[DesktopApp::%s]> name: %s | package: %s | summary: %s", self->priv->idname, name, self->priv->pkgname, self->priv->summary);
			break;
		}
		default:
		{
			res = g_strdup_printf ("[Component::%s]> name: %s | package: %s | summary: %s", self->priv->idname, name, self->priv->pkgname, self->priv->summary);
			break;
		}
	}

	return res;
}


/**
 * as_component_set_categories_from_str:
 *
 * Set the categories list from a string
 *
 * @self a valid #AsComponent instance
 * @categories_str Comma-separated list of category-names
 */
void
as_component_set_categories_from_str (AsComponent* self, const gchar* categories_str)
{
	gchar** cats = NULL;

	g_return_if_fail (self != NULL);
	g_return_if_fail (categories_str != NULL);

	cats = g_strsplit (categories_str, ",", 0);
	as_component_set_categories (self, cats);
	g_strfreev (cats);
}


void
as_component_add_screenshot (AsComponent* self, AsScreenshot* sshot)
{
	GPtrArray* sslist;

	g_return_if_fail (self != NULL);
	g_return_if_fail (sshot != NULL);
	sslist = as_component_get_screenshots (self);
	g_ptr_array_add (sslist, g_object_ref (sshot));
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
 * Internal function to create XML which gets stored in the AppStream database
 * for screenshots
 */
gchar*
as_component_dump_screenshot_data_xml (AsComponent* self)
{
	GPtrArray* sslist;
	xmlDoc *doc;
	xmlNode *root;
	guint i;
	AsScreenshot *sshot;
	gchar *xmlstr = NULL;
	g_return_val_if_fail (self != NULL, NULL);

	sslist = as_component_get_screenshots (self);
	if (sslist == 0) {
		return g_strdup ("");
	}

	doc = xmlNewDoc ((xmlChar*) NULL);
	root = xmlNewNode (NULL, (xmlChar*) "screenshots");
	xmlDocSetRootElement (doc, root);

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

	xmlDocDumpMemory (doc, (xmlChar**) (&xmlstr), NULL);
	xmlFreeDoc (doc);

	return xmlstr;
}

/**
 * Internal function to load the screenshot list
 * using the database-internal XML data.
 */
void
as_component_load_screenshots_from_internal_xml (AsComponent* self, const gchar* xmldata)
{
	xmlDoc* doc = NULL;
	xmlNode* root = NULL;
	xmlNode *iter;
	g_return_if_fail (self != NULL);
	g_return_if_fail (xmldata != NULL);

	if (as_utils_str_empty (xmldata)) {
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
			as_component_add_screenshot (self, sshot);
		}
	}
}

/**
 * as_component_complete:
 *
 * Private function to complete a AsComponent with
 * additional data found on the system.
 *
 * @scr_base_url Base url for screenshot-service, obtain via #AsDistroDetails
 */
void
as_component_complete (AsComponent* self, gchar *scr_base_url)
{
	AsComponentPrivate *priv = self->priv;

	/* we want screenshot data from 3rd-party screenshot servers, if the component doesn't have screenshots defined already */
	if (priv->screenshots->len == 0) {
		gchar *url;
		AsImage *img;
		AsScreenshot *sshot;

		url = g_build_filename (scr_base_url, "screenshot", priv->pkgname, NULL);

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
		url = g_build_filename (scr_base_url, "thumbnail", priv->pkgname, NULL);
		img = as_image_new ();
		as_image_set_url (img, url);
		as_image_set_width (img, 160);
		as_image_set_height (img, 120);
		as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
		as_screenshot_add_image (sshot, img);

		/* add screenshot to component */
		as_component_add_screenshot (self, sshot);

		g_object_unref (img);
		g_object_unref (sshot);
		g_free (url);
	}
}

/**
 * as_component_provides_item:
 *
 * Checks if this component provides an item of the specified type
 *
 * @self a valid #AsComponent
 * @kind the kind of the provides-item
 * @value the value of the provides-item
 *
 * Returns: %TRUE if an item was found
 */
gboolean
as_component_provides_item (AsComponent *self, AsProvidesKind kind, const gchar *value)
{
	guint i;
	gboolean ret = FALSE;
	gchar *item;
	AsComponentPrivate *priv = self->priv;

	item = as_provides_item_create (kind, value);
	for (i = 0; i < priv->provides_items->len; i++) {
		gchar *cval;
		cval = (gchar*) g_ptr_array_index (priv->provides_items, i);
		if (g_strcmp0 (item, cval) == 0) {
			ret = TRUE;
			break;
		}
	}

	g_free (item);
	return ret;
}

AsComponentKind
as_component_get_kind (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, 0);
	return self->priv->kind;
}


void
as_component_set_kind (AsComponent* self, AsComponentKind value)
{
	g_return_if_fail (self != NULL);

	self->priv->kind = value;
	g_object_notify ((GObject *) self, "kind");
}


const gchar*
as_component_get_pkgname (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->pkgname;
}


void
as_component_set_pkgname (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->pkgname);
	self->priv->pkgname = g_strdup (value);
	g_object_notify ((GObject *) self, "pkgname");
}


const gchar*
as_component_get_idname (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->idname;
}


void
as_component_set_idname (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->idname);
	self->priv->idname = g_strdup (value);
	g_object_notify ((GObject *) self, "idname");
}


const gchar*
as_component_get_name (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	if (as_utils_str_empty (self->priv->name)) {
		self->priv->name = g_strdup (self->priv->name_original);
	}

	return self->priv->name;
}


void
as_component_set_name (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->name);
	self->priv->name = g_strdup (value);
	g_object_notify ((GObject *) self, "name");
}


const gchar*
as_component_get_name_original (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->name_original;
}


void
as_component_set_name_original (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->name_original);
	self->priv->name_original = g_strdup (value);
	g_object_notify ((GObject *) self, "name-original");
}


const gchar*
as_component_get_summary (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->summary;
}


void
as_component_set_summary (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->summary);
	self->priv->summary = g_strdup (value);
	g_object_notify ((GObject *) self, "summary");
}


const gchar*
as_component_get_description (AsComponent* self)
{
	const gchar* result;
	const gchar* _tmp0_ = NULL;
	g_return_val_if_fail (self != NULL, NULL);
	_tmp0_ = self->priv->description;
	result = _tmp0_;
	return result;
}


void
as_component_set_description (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->description);
	self->priv->description = g_strdup (value);
	g_object_notify ((GObject *) self, "description");
}


gchar**
as_component_get_keywords (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->keywords;
}

void
as_component_set_keywords (AsComponent* self, gchar** value)
{
	g_return_if_fail (self != NULL);

	g_strfreev (self->priv->keywords);
	self->priv->keywords = as_strv_dup (value);
	g_object_notify ((GObject *) self, "keywords");
}


const gchar*
as_component_get_icon (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->icon;
}


void
as_component_set_icon (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->icon);
	self->priv->icon = g_strdup (value);
	g_object_notify ((GObject *) self, "icon");
}


const gchar*
as_component_get_icon_url (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->icon_url;
}


void
as_component_set_icon_url (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->icon_url);
	self->priv->icon_url = g_strdup (value);
	g_object_notify ((GObject *) self, "icon-url");
}


const gchar*
as_component_get_homepage (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->homepage;
}


void
as_component_set_homepage (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->homepage);
	self->priv->homepage = g_strdup (value);
	g_object_notify ((GObject *) self, "homepage");
}


gchar**
as_component_get_categories (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->categories;
}

void
as_component_set_categories (AsComponent* self, gchar** value)
{
	g_return_if_fail (self != NULL);

	g_strfreev (self->priv->categories);
	self->priv->categories = as_strv_dup (value);
	g_object_notify ((GObject *) self, "categories");
}


gchar**
as_component_get_mimetypes (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->mimetypes;
}

void
as_component_set_mimetypes (AsComponent* self, gchar** value)
{
	g_return_if_fail (self != NULL);

	g_strfreev (self->priv->mimetypes);
	self->priv->mimetypes = as_strv_dup (value);
	g_object_notify ((GObject *) self, "mimetypes");
}

/**
 * as_component_get_project_license:
 *
 * Get the license of the project this component belongs to.
 */
const gchar*
as_component_get_project_license (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->project_license;
}

/**
 * as_component_set_project_license:
 *
 * Set the project license.
 */
void
as_component_set_project_license (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->project_license);
	self->priv->project_license = g_strdup (value);
	g_object_notify ((GObject *) self, "project-license");
}

/**
 * as_component_get_project_group:
 *
 * Get the component's project group.
 */
const gchar*
as_component_get_project_group (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->project_group;
}

/**
 * as_component_set_project_group:
 *
 * Set the component's project group.
 */
void
as_component_set_project_group (AsComponent* self, const gchar* value)
{
	g_return_if_fail (self != NULL);

	g_free (self->priv->project_group);
	self->priv->project_group = g_strdup (value);
}

/**
 * as_component_get_screenshots:
 *
 * Get a list of associated screenshots.
 *
 * Returns: (element-type AsScreenshot) (transfer none): an array of #AsScreenshot instances
 */
GPtrArray*
as_component_get_screenshots (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->screenshots;
}

/**
 * as_component_get_compulsory_for_desktops:
 *
 * Return value: (transfer none): A list of desktops where this component is compulsory
 **/
gchar **
as_component_get_compulsory_for_desktops (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->compulsory_for_desktops;
}

/**
 * as_component_set_compulsory_for_desktops:
 *
 * Set a list of desktops where this component is compulsory.
 **/
void
as_component_set_compulsory_for_desktops (AsComponent* self, gchar** value)
{
	g_return_if_fail (self != NULL);

	g_strfreev (self->priv->compulsory_for_desktops);
	self->priv->compulsory_for_desktops = as_strv_dup (value);
}

/**
 * as_component_get_provides:
 *
 * Get an array of the provides-items this component is
 * associated with.
 *
 * Return value: (element-type utf8) (transfer none): A list of desktops where this component is compulsory
 **/
GPtrArray*
as_component_get_provides (AsComponent* self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->provides_items;
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
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_PKGNAME, g_param_spec_string ("pkgname", "pkgname", "pkgname", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_IDNAME, g_param_spec_string ("idname", "idname", "idname", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_NAME, g_param_spec_string ("name", "name", "name", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_NAME_ORIGINAL, g_param_spec_string ("name-original", "name-original", "name-original", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_SUMMARY, g_param_spec_string ("summary", "summary", "summary", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_DESCRIPTION, g_param_spec_string ("description", "description", "description", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_KEYWORDS, g_param_spec_boxed ("keywords", "keywords", "keywords", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_ICON, g_param_spec_string ("icon", "icon", "icon", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_ICON_URL, g_param_spec_string ("icon-url", "icon-url", "icon-url", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_HOMEPAGE, g_param_spec_string ("homepage", "homepage", "homepage", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_CATEGORIES, g_param_spec_boxed ("categories", "categories", "categories", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_MIMETYPES, g_param_spec_boxed ("mimetypes", "mimetypes", "mimetypes", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_PROJECT_LICENSE, g_param_spec_string ("project-license", "project-license", "project-license", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_COMPONENT_SCREENSHOTS, g_param_spec_boxed ("screenshots", "screenshots", "screenshots", G_TYPE_PTR_ARRAY, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}


static void
as_component_instance_init (AsComponent * self)
{
	self->priv = AS_COMPONENT_GET_PRIVATE (self);
}


static void
as_component_finalize (GObject* obj)
{
	AsComponent *self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_COMPONENT, AsComponent);
	g_free (self->priv->pkgname);
	g_free (self->priv->idname);
	g_free (self->priv->name);
	g_free (self->priv->name_original);
	g_free (self->priv->summary);
	g_free (self->priv->description);
	g_free (self->priv->icon);
	g_free (self->priv->icon_url);
	g_free (self->priv->homepage);
	g_free (self->priv->project_license);
	g_free (self->priv->project_group);
	g_strfreev (self->priv->keywords);
	g_strfreev (self->priv->categories);
	g_strfreev (self->priv->mimetypes);
	g_strfreev (self->priv->compulsory_for_desktops);
	g_ptr_array_unref (self->priv->screenshots);
	g_ptr_array_unref (self->priv->provides_items);
	G_OBJECT_CLASS (as_component_parent_class)->finalize (obj);
}


/**
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
	AsComponent * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_COMPONENT, AsComponent);
	switch (property_id) {
		case AS_COMPONENT_KIND:
			g_value_set_enum (value, as_component_get_kind (self));
			break;
		case AS_COMPONENT_PKGNAME:
			g_value_set_string (value, as_component_get_pkgname (self));
			break;
		case AS_COMPONENT_IDNAME:
			g_value_set_string (value, as_component_get_idname (self));
			break;
		case AS_COMPONENT_NAME:
			g_value_set_string (value, as_component_get_name (self));
			break;
		case AS_COMPONENT_NAME_ORIGINAL:
			g_value_set_string (value, as_component_get_name_original (self));
			break;
		case AS_COMPONENT_SUMMARY:
			g_value_set_string (value, as_component_get_summary (self));
			break;
		case AS_COMPONENT_DESCRIPTION:
			g_value_set_string (value, as_component_get_description (self));
			break;
		case AS_COMPONENT_KEYWORDS:
			g_value_set_boxed (value, as_component_get_keywords (self));
			break;
		case AS_COMPONENT_ICON:
			g_value_set_string (value, as_component_get_icon (self));
			break;
		case AS_COMPONENT_ICON_URL:
			g_value_set_string (value, as_component_get_icon_url (self));
			break;
		case AS_COMPONENT_HOMEPAGE:
			g_value_set_string (value, as_component_get_homepage (self));
			break;
		case AS_COMPONENT_CATEGORIES:
			g_value_set_boxed (value, as_component_get_categories (self));
			break;
		case AS_COMPONENT_MIMETYPES:
			g_value_set_boxed (value, as_component_get_mimetypes (self));
			break;
		case AS_COMPONENT_PROJECT_LICENSE:
			g_value_set_string (value, as_component_get_project_license (self));
			break;
		case AS_COMPONENT_SCREENSHOTS:
			g_value_set_boxed (value, as_component_get_screenshots (self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void
as_component_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	AsComponent * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_COMPONENT, AsComponent);
	switch (property_id) {
		case AS_COMPONENT_KIND:
			as_component_set_kind (self, g_value_get_enum (value));
			break;
		case AS_COMPONENT_PKGNAME:
			as_component_set_pkgname (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_IDNAME:
			as_component_set_idname (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_NAME:
			as_component_set_name (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_NAME_ORIGINAL:
			as_component_set_name_original (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_SUMMARY:
			as_component_set_summary (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_DESCRIPTION:
			as_component_set_description (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_KEYWORDS:
			as_component_set_keywords (self, g_value_get_boxed (value));
			break;
		case AS_COMPONENT_ICON:
			as_component_set_icon (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_ICON_URL:
			as_component_set_icon_url (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_HOMEPAGE:
			as_component_set_homepage (self, g_value_get_string (value));
			break;
		case AS_COMPONENT_CATEGORIES:
			as_component_set_categories (self, g_value_get_boxed (value));
			break;
		case AS_COMPONENT_MIMETYPES:
			as_component_set_mimetypes (self, g_value_get_boxed (value));
			break;
		case AS_COMPONENT_PROJECT_LICENSE:
			as_component_set_project_license (self, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}
