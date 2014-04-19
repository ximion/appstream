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

/**
 * SECTION:as-metadata
 * @short_description: Parser for AppStream metadata
 * @include: appstream.h
 *
 * This object parses AppStream metadata, including AppStream
 * upstream metadata, which is defined by upstream projects.
 * It returns an #AsComponent of the data.
 *
 * See also: #AsComponent, #AsDatabase
 */

#include <config.h>
#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "as-metadata.h"
#include "as-metadata-private.h"

#include "as-utils.h"

typedef struct _AsMetadataPrivate	AsMetadataPrivate;
struct _AsMetadataPrivate
{
	gchar* locale;
	AsParserMode mode;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsMetadata, as_metadata, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_metadata_get_instance_private (o))

static gchar*		as_metadata_parse_value (AsMetadata* metad, xmlNode* node, gboolean translated);
static gchar**		as_metadata_get_children_as_array (AsMetadata* metad, xmlNode* node, const gchar* element_name);

/**
 * as_metadata_finalize:
 **/
static void
as_metadata_finalize (GObject *object)
{
	AsMetadata *metad = AS_METADATA (object);
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	g_free (priv->locale);

	G_OBJECT_CLASS (as_metadata_parent_class)->finalize (object);
}

/**
 * as_metadata_init:
 **/
static void
as_metadata_init (AsMetadata *metad)
{
	const gchar * const *locale_names;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	locale_names = g_get_language_names ();
	priv->locale = g_strdup (locale_names[0]);

	priv->mode = AS_PARSER_MODE_UPSTREAM;
}

/**
 * as_metadata_class_init:
 **/
static void
as_metadata_class_init (AsMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_metadata_finalize;
}

/**
 * as_metadata_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_metadata_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsMetadataError");
	return quark;
}

static gchar*
as_metadata_parse_value (AsMetadata* metad, xmlNode* node, gboolean translated)
{
	AsMetadataPrivate *priv;
	gchar *content;
	gchar *lang;
	gchar *res;

	g_return_val_if_fail (metad != NULL, NULL);
	priv = GET_PRIVATE (metad);

	content = (gchar*) xmlNodeGetContent (node);
	lang = (gchar*) xmlGetProp (node, (xmlChar*) "lang");

	if (translated) {
		gchar *current_locale;
		gchar **strv;
		gchar *str;
		/* FIXME: If not-localized generic node comes _after_ the localized ones,
		 * the not-localized will override the localized. Wrong ordering should
		 * not happen, but we should deal with that case anyway.
		 */
		if (lang == NULL) {
			res = content;
			goto out;
		}
		current_locale = priv->locale;
		if (g_strcmp0 (lang, current_locale) == 0) {
			res = content;
			goto out;
		}
		strv = g_strsplit (current_locale, "_", 0);
		str = g_strdup (strv[0]);
		g_strfreev (strv);
		if (g_strcmp0 (lang, str) == 0) {
			res = content;
			g_free (str);
			goto out;
		}
		g_free (str);

		/* Haven't found a matching locale */
		res = NULL;
		g_free (content);
		goto out;
	}
	/* If we have a locale here, but want the untranslated item, return NULL */
	if (lang != NULL) {
		res = NULL;
		g_free (content);
		goto out;
	}
	res = content;

out:
	g_free (lang);
	return res;
}

static gchar**
as_metadata_get_children_as_array (AsMetadata* metad, xmlNode* node, const gchar* element_name)
{
	GPtrArray *list;
	xmlNode *iter;
	gchar **res;
	g_return_val_if_fail (metad != NULL, NULL);
	g_return_val_if_fail (element_name != NULL, NULL);
	list = g_ptr_array_new_with_free_func (g_free);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE) {
					continue;
		}
		if (g_strcmp0 ((gchar*) iter->name, element_name) == 0) {
			gchar* content = NULL;
			content = (gchar*) xmlNodeGetContent (iter);
			if (content != NULL) {
				gchar *s;
				s = as_string_strip (content);
				g_ptr_array_add (list, s);
			}
			g_free (content);
		}
	}

	res = as_ptr_array_to_strv (list);
	g_ptr_array_unref (list);
	return res;
}


static void
as_metadata_process_screenshot (AsMetadata* metad, xmlNode* node, AsScreenshot* sshot)
{
	xmlNode *iter;
	gchar *node_name;
	gchar *content = NULL;
	g_return_if_fail (metad != NULL);
	g_return_if_fail (sshot != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		content = as_metadata_parse_value (metad, iter, TRUE);
		if (g_strcmp0 (node_name, "image") == 0) {
			AsImage *img;
			guint64 width;
			guint64 height;
			gchar *stype;
			gchar *str;
			if (content == NULL) {
				continue;
			}
			img = as_image_new ();

			str = (gchar*) xmlGetProp (iter, (xmlChar*) "width");
			width = g_ascii_strtoll (str, NULL, 10);
			g_free (str);
			str = (gchar*) xmlGetProp (iter, (xmlChar*) "height");
			height = g_ascii_strtoll (str, NULL, 10);
			g_free (str);
			/* discard invalid elements */
			if ((width == 0) || (height == 0)) {
				g_free (content);
				continue;
			}

			as_image_set_width (img, width);
			as_image_set_height (img, height);

			stype = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (g_strcmp0 (stype, "thumbnail") == 0) {
				as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
			} else {
				as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
			}
			g_free (stype);
			as_image_set_url (img, content);
			as_screenshot_add_image (sshot, img);
		} else if (g_strcmp0 (node_name, "caption") == 0) {
			if (content != NULL) {
				as_screenshot_set_caption (sshot, content);
			}
		}
		g_free (content);
	}
}

static void
as_metadata_process_screenshots_tag (AsMetadata* metad, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	AsScreenshot *sshot = NULL;
	gchar *prop;
	g_return_if_fail (metad != NULL);
	g_return_if_fail (cpt != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "screenshot") == 0) {
			sshot = as_screenshot_new ();
			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (g_strcmp0 (prop, "default") == 0)
				as_screenshot_set_kind (sshot, AS_SCREENSHOT_KIND_DEFAULT);
			as_metadata_process_screenshot (metad, iter, sshot);
			if (as_screenshot_is_valid (sshot))
				as_component_add_screenshot (cpt, sshot);
			g_free (prop);
			g_object_unref (sshot);
		}
	}
}

static void
as_metadata_process_releases_tag (AsMetadata* metad, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	xmlNode *iter2;
	AsRelease *release = NULL;
	gchar *prop;
	guint64 timestamp;
	g_return_if_fail (metad != NULL);
	g_return_if_fail (cpt != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "release") == 0) {
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
					content = as_metadata_parse_value (metad, iter2, TRUE);
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

static void
as_metadata_process_provides (AsMetadata* metad, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	gchar *node_name;
	gchar *content = NULL;
	GPtrArray *provided_items;
	g_return_if_fail (metad != NULL);
	g_return_if_fail (cpt != NULL);

	provided_items = as_component_get_provided_items (cpt);
	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		content = as_metadata_parse_value (metad, iter, TRUE);
		if (content == NULL)
			continue;

		if (g_strcmp0 (node_name, "library") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_LIBRARY, content));
		} else if (g_strcmp0 (node_name, "binary") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_BINARY, content));
		} else if (g_strcmp0 (node_name, "font") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_FONT, content));
		} else if (g_strcmp0 (node_name, "modalias") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_MODALIAS, content));
		} else if (g_strcmp0 (node_name, "firmware") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_FIRMWARE, content));
		} else if (g_strcmp0 (node_name, "python2") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_PYTHON2, content));
		} else if (g_strcmp0 (node_name, "python3") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_PYTHON3, content));
		}
		g_free (content);
	}
}

AsComponent*
as_metadata_parse_component_node (AsMetadata* metad, xmlNode* node, GError **error)
{
	AsComponent* cpt;
	xmlNode *iter;
	const gchar *node_name;
	gchar *content;
	GPtrArray *compulsory_for_desktops;
	gchar **strv;
	gchar *cpttype;

	g_return_if_fail (metad != NULL);

	compulsory_for_desktops = g_ptr_array_new_with_free_func (g_free);

	/* a fresh app component */
	cpt = as_component_new ();

	/* find out which kind of component we are dealing with */
	cpttype = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	if ((cpttype == NULL) || (g_strcmp0 (cpttype, "generic") == 0)) {
		as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	} else if (g_strcmp0 (cpttype, "desktop") == 0) {
		as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);
	} else if (g_strcmp0 (cpttype, "font") == 0) {
		as_component_set_kind (cpt, AS_COMPONENT_KIND_FONT);
	} else if (g_strcmp0 (cpttype, "codec") == 0) {
		as_component_set_kind (cpt, AS_COMPONENT_KIND_CODEC);
	} else if (g_strcmp0 (cpttype, "inputmethod") == 0) {
		as_component_set_kind (cpt, AS_COMPONENT_KIND_INPUTMETHOD);
	} else {
		as_component_set_kind (cpt, AS_COMPONENT_KIND_UNKNOWN);
		g_debug ("An unknown component was found: %s", cpttype);
	}

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;
		content = as_metadata_parse_value (metad, iter, FALSE);
		if (g_strcmp0 (node_name, "id") == 0) {
				as_component_set_idname (cpt, content);
		} else if (g_strcmp0 (node_name, "pkgname") == 0) {
			if (content != NULL)
				as_component_set_pkgname (cpt, content);
		} else if (g_strcmp0 (node_name, "name") == 0) {
			if (content != NULL) {
				as_component_set_name_original (cpt, content);
			} else {
				content = as_metadata_parse_value (metad, iter, TRUE);
				if (content != NULL)
					as_component_set_name (cpt, content);
			}
		} else if (g_strcmp0 (node_name, "summary") == 0) {
			if (content != NULL) {
				as_component_set_summary (cpt, content);
			} else {
				content = as_metadata_parse_value (metad, iter, TRUE);
				if (content != NULL)
					as_component_set_summary (cpt, content);
			}
		} else if (g_strcmp0 (node_name, "description") == 0) {
			if (content != NULL) {
				as_component_set_description (cpt, content);
			} else {
				content = as_metadata_parse_value (metad, iter, TRUE);
				if (content != NULL)
					as_component_set_description (cpt, content);
			}
		} else if (g_strcmp0 (node_name, "icon") == 0) {
			gchar *prop;
			const gchar *icon_url;
			if (content == NULL)
				continue;
			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (g_strcmp0 (prop, "stock") == 0) {
				as_component_set_icon (cpt, content);
			} else if (g_strcmp0 (prop, "cached") == 0) {
				icon_url = as_component_get_icon_url (cpt);
				if ((g_strcmp0 (icon_url, "") == 0) || (g_str_has_prefix (icon_url, "http://")))
					as_component_set_icon_url (cpt, content);
			} else if (g_strcmp0 (prop, "local") == 0) {
				as_component_set_icon_url (cpt, content);
			} else if (g_strcmp0 (prop, "remote") == 0) {
				icon_url = as_component_get_icon_url (cpt);
				if (g_strcmp0 (icon_url, "") == 0)
					as_component_set_icon_url (cpt, content);
			}
		} else if (g_strcmp0 (node_name, "url") == 0) {
			if (content != NULL) {
				as_component_set_homepage (cpt, content);
			}
		} else if (g_strcmp0 (node_name, "categories") == 0) {
			gchar **cat_array;
			cat_array = as_metadata_get_children_as_array (metad, iter, "category");
			as_component_set_categories (cpt, cat_array);
		} else if (g_strcmp0 (node_name, "provides") == 0) {
			as_metadata_process_provides (metad, iter, cpt);
		} else if (g_strcmp0 (node_name, "screenshots") == 0) {
			as_metadata_process_screenshots_tag (metad, iter, cpt);
		} else if (g_strcmp0 (node_name, "project_license") == 0) {
			if (content != NULL)
				as_component_set_project_license (cpt, content);
		} else if (g_strcmp0 (node_name, "project_group") == 0) {
			if (content != NULL)
				as_component_set_project_group (cpt, content);
		} else if (g_strcmp0 (node_name, "compulsory_for_desktop") == 0) {
			if (content != NULL)
				g_ptr_array_add (compulsory_for_desktops, g_strdup (content));
		} else if (g_strcmp0 (node_name, "releases") == 0) {
			as_metadata_process_releases_tag (metad, iter, cpt);
		}
		g_free (content);
	}

	/* add compulsory information to component as strv */
	strv = as_ptr_array_to_strv (compulsory_for_desktops);
	as_component_set_compulsory_for_desktops (cpt, strv);
	g_ptr_array_unref (compulsory_for_desktops);
	g_strfreev (strv);

	if (as_component_is_valid (cpt)) {
		return cpt;
	} else {
		gchar *cpt_str;
		gchar *msg;
		cpt_str = as_component_to_string (cpt);
		msg = g_strdup_printf ("Invalid component: %s", cpt_str);
		g_free (cpt_str);
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     msg);
		g_free (msg);
		g_object_unref (cpt);
	}

	return NULL;
}

static AsComponent*
as_metadata_process_document (AsMetadata *metad, const gchar* xmldoc_str, GError **error)
{
	xmlDoc* doc;
	xmlNode* root;
	AsComponent *cpt = NULL;

	g_return_val_if_fail (metad != NULL, FALSE);
	g_return_val_if_fail (xmldoc_str != NULL, FALSE);

	doc = xmlParseDoc ((xmlChar*) xmldoc_str);
	if (doc == NULL) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "Could not parse XML!");
		return NULL;
	}

	root = xmlDocGetRootElement (doc);
	if (doc == NULL) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "The XML document is empty.");
		return NULL;
	}

	if (g_strcmp0 ((gchar*) root->name, "component") != 0) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "XML file does not contain valid AppStream data!");
		goto out;
	}

	cpt = as_metadata_parse_component_node (metad, root, error);

out:
	xmlFreeDoc (doc);

	return cpt;
}

/**
 * as_metadata_parse_data:
 * @metad: A valid #AsMetadata instance
 * @data: XML data describing a component
 * @error: A #GError or %NULL.
 *
 * Parses AppStream upstream metadata.
 *
 * Returns: (transfer full): the #AsComponent of this data, or NULL on error
 **/
AsComponent*
as_metadata_parse_data (AsMetadata* metad, const gchar *data, GError **error)
{
	AsComponent *cpt;
	g_return_val_if_fail (metad != NULL, NULL);
	g_return_val_if_fail (data != NULL, NULL);

	cpt = as_metadata_process_document (metad, data, error);

	return cpt;
}

/**
 * as_metadata_parse_file:
 * @metad: A valid #AsMetadata instance
 * @infile: #GFile for the upstream metadata
 * @error: A #GError or %NULL.
 *
 * Parses an AppStream upstream metadata file.
 *
 * Returns: (transfer full): the #AsComponent of this file, or NULL on error
 **/
AsComponent*
as_metadata_parse_file (AsMetadata* metad, GFile* infile, GError **error)
{
	AsComponent *cpt;
	gchar* xml_doc;
	gchar* line = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;

	g_return_val_if_fail (metad != NULL, NULL);
	g_return_val_if_fail (infile != NULL, NULL);

	xml_doc = g_strdup ("");
	ir = g_file_read (infile, NULL, NULL);
	dis = g_data_input_stream_new ((GInputStream*) ir);
	g_object_unref (ir);

	while (TRUE) {
		gchar *str;
		gchar *tmp;

		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		str = g_strconcat (line, "\n", NULL);
		g_free (line);
		tmp = g_strconcat (xml_doc, str, NULL);
		g_free (str);
		g_free (xml_doc);
		xml_doc = tmp;
	}

	cpt = as_metadata_process_document (metad, xml_doc, error);
	g_object_unref (dis);
	g_free (xml_doc);

	return cpt;
}

/**
 * as_metadata_set_locale:
 * @metad: a #AsMezadata instance.
 * @locale: the locale.
 *
 * Sets the current locale whcih should be used when parsing metadata.
 **/
void
as_metadata_set_locale (AsMetadata *metad, const gchar *locale)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_free (priv->locale);
	priv->locale = g_strdup (locale);
}

/**
 * as_metadata_get_locale:
 * @metad: a #AsMezadata instance.
 *
 * Gets the currently used locale.
 *
 * Returns: Locale used for metadata parsing.
 **/
const gchar *
as_metadata_get_locale (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->locale;
}

/**
 * as_metadata_set_parser_mode:
 * @metad: a #AsMezadata instance.
 * @mode: the #AsParserMode.
 *
 * Sets the current metadata parsing mode.
 **/
void
as_metadata_set_parser_mode (AsMetadata *metad, AsParserMode mode)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	priv->mode = mode;
}

/**
 * as_metadata_get_parser_mode:
 * @metad: a #AsMezadata instance.
 *
 * Gets the current parser mode
 *
 * Returns: an #AsParserMode
 **/
AsParserMode
as_metadata_get_parser_mode (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->mode;
}

/**
 * as_metadata_new:
 *
 * Creates a new #AsMetadata.
 *
 * Returns: (transfer full): a #AsMetadata
 **/
AsMetadata*
as_metadata_new (void)
{
	AsMetadata *metad;
	metad = g_object_new (AS_TYPE_METADATA, NULL);
	return AS_METADATA (metad);
}
