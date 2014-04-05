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

#include "appstream-xml.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <stdio.h>
#include <glib/gstdio.h>

#include "../as-utils.h"
#include "../as-settings-private.h"
#include "../as-menu-parser.h"

struct _AsProviderAppstreamXMLPrivate {
	gchar* locale;
	GList* system_categories;
};

const gchar* AS_APPSTREAM_XML_PATHS[3] = {AS_APPSTREAM_BASE_PATH "/xmls", "/var/cache/app-info/xmls", NULL};

static gpointer as_provider_appstream_xml_parent_class = NULL;

#define AS_PROVIDER_APPSTREAM_XML_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_PROVIDER_TYPE_APPSTREAM_XML, AsProviderAppstreamXMLPrivate))

static gchar*		as_provider_appstream_xml_parse_value (AsProviderAppstreamXML* self, xmlNode* node, gboolean translated);
static gchar**		as_provider_appstream_xml_get_children_as_array (AsProviderAppstreamXML* self, xmlNode* node, const gchar* element_name);
static void			as_provider_appstream_xml_process_screenshot (AsProviderAppstreamXML* self, xmlNode* node, AsScreenshot* sshot);
static void			as_provider_appstream_xml_process_screenshots_tag (AsProviderAppstreamXML* self, xmlNode* node, AsComponent* app);
static gboolean		as_provider_appstream_xml_process_single_document (AsProviderAppstreamXML* self, const gchar* xmldoc);
static gboolean		as_provider_appstream_xml_real_execute (AsDataProvider* base);
static void			as_provider_appstream_xml_finalize (GObject* obj);

AsProviderAppstreamXML* as_provider_appstream_xml_construct (GType object_type)
{
	AsProviderAppstreamXML * self = NULL;
	const gchar * const *locale_names;
	gchar *locale;
	GList *syscat;
	guint i;
	guint len;
	gchar **wfiles;
	self = (AsProviderAppstreamXML*) as_data_provider_construct (object_type);

	locale_names = g_get_language_names ();
	locale = g_strdup (locale_names[0]);
	self->priv->locale = locale;

	/* cache this for performance reasons */
	syscat = as_get_system_categories ();
	self->priv->system_categories = syscat;

	len = G_N_ELEMENTS (AS_APPSTREAM_XML_PATHS);
	wfiles = g_new0 (gchar *, len + 1);
	for (i = 0; i < len+1; i++) {
		if (i < len)
			wfiles[i] = g_strdup (AS_APPSTREAM_XML_PATHS[i]);
		else
			wfiles[i] = NULL;
	}
	as_data_provider_set_watch_files ((AsDataProvider*) self, wfiles);
	g_strfreev (wfiles);

	return self;
}


AsProviderAppstreamXML* as_provider_appstream_xml_new (void)
{
	return as_provider_appstream_xml_construct (AS_PROVIDER_TYPE_APPSTREAM_XML);
}


static gchar* as_provider_appstream_xml_parse_value (AsProviderAppstreamXML* self, xmlNode* node, gboolean translated)
{
	gchar *content;
	gchar *lang;
	gchar *res;

	g_return_val_if_fail (self != NULL, NULL);

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
		current_locale = self->priv->locale;
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
as_provider_appstream_xml_get_children_as_array (AsProviderAppstreamXML* self, xmlNode* node, const gchar* element_name)
{
	GPtrArray *list;
	xmlNode *iter;
	gchar **res;
	g_return_val_if_fail (self != NULL, NULL);
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
as_provider_appstream_xml_process_screenshot (AsProviderAppstreamXML* self, xmlNode* node, AsScreenshot* sshot)
{
	xmlNode *iter;
	gchar *node_name;
	gchar *content = NULL;
	g_return_if_fail (self != NULL);
	g_return_if_fail (sshot != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		content = as_provider_appstream_xml_parse_value (self, iter, TRUE);
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

static void as_provider_appstream_xml_process_screenshots_tag (AsProviderAppstreamXML* self, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	AsScreenshot *sshot = NULL;
	gchar *prop;
	g_return_if_fail (self != NULL);
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
			as_provider_appstream_xml_process_screenshot (self, iter, sshot);
			if (as_screenshot_is_valid (sshot))
				as_component_add_screenshot (cpt, sshot);
			g_free (prop);
			g_object_unref (sshot);
		}
	}
}

static void
as_provider_appstream_xml_parse_component_node (AsProviderAppstreamXML* self, xmlNode* node)
{
	AsComponent* cpt;
	xmlNode *iter;
	const gchar *node_name;
	gchar *content;
	GPtrArray *compulsory_for_desktops;
	gchar **strv;
	gchar *cpttype;

	g_return_if_fail (self != NULL);

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
		content = as_provider_appstream_xml_parse_value (self, iter, FALSE);
		if (g_strcmp0 (node_name, "id") == 0) {
				as_component_set_idname (cpt, content);
		} else if (g_strcmp0 (node_name, "pkgname") == 0) {
			if (content != NULL)
				as_component_set_pkgname (cpt, content);
		} else if (g_strcmp0 (node_name, "name") == 0) {
			if (content != NULL) {
				as_component_set_name_original (cpt, content);
			} else {
				content = as_provider_appstream_xml_parse_value (self, iter, TRUE);
				if (content != NULL)
					as_component_set_name (cpt, content);
			}
		} else if (g_strcmp0 (node_name, "summary") == 0) {
			if (content != NULL) {
				as_component_set_summary (cpt, content);
			} else {
				content = as_provider_appstream_xml_parse_value (self, iter, TRUE);
				if (content != NULL)
					as_component_set_summary (cpt, content);
			}
		} else if (g_strcmp0 (node_name, "description") == 0) {
			if (content != NULL) {
				as_component_set_description (cpt, content);
			} else {
				content = as_provider_appstream_xml_parse_value (self, iter, TRUE);
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
			cat_array = as_provider_appstream_xml_get_children_as_array (self, iter, "category");
			as_component_set_categories (cpt, cat_array);
		} else if (g_strcmp0 (node_name, "screenshots") == 0) {
			as_provider_appstream_xml_process_screenshots_tag (self, iter, cpt);
		} else if (g_strcmp0 (node_name, "project_license") == 0) {
			if (content != NULL)
				as_component_set_project_license (cpt, content);
		} else if (g_strcmp0 (node_name, "project_group") == 0) {
			if (content != NULL)
				as_component_set_project_group (cpt, content);
		} else if (g_strcmp0 (node_name, "compulsory_for_desktop") == 0) {
			if (content != NULL)
				g_ptr_array_add (compulsory_for_desktops, g_strdup (content));
		}
		g_free (content);
	}

	/* add compulsory information to component as strv */
	strv = as_ptr_array_to_strv (compulsory_for_desktops);
	as_component_set_compulsory_for_desktops (cpt, strv);
	g_ptr_array_unref (compulsory_for_desktops);
	g_strfreev (strv);

	if (as_component_is_valid (cpt)) {
		as_data_provider_emit_application ((AsDataProvider*) self, cpt);
	} else {
		gchar *cpt_str;
		gchar *msg;
		cpt_str = as_component_to_string (cpt);
		msg = g_strdup_printf ("Invalid component found: %s", cpt_str);
		g_free (cpt_str);
		as_data_provider_log_warning ((AsDataProvider*) self, msg);
		g_free (msg);
	}

	g_object_unref (cpt);
}


static gboolean
as_provider_appstream_xml_process_single_document (AsProviderAppstreamXML* self, const gchar* xmldoc_str)
{
	xmlDoc* doc;
	xmlNode* root;
	xmlNode* iter;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (xmldoc_str != NULL, FALSE);

	doc = xmlParseDoc ((xmlChar*) xmldoc_str);
	if (doc == NULL) {
		fprintf (stderr, "Could not parse XML!");
		return FALSE;
	}

	root = xmlDocGetRootElement (doc);
	if (doc == NULL) {
		fprintf (stderr, "The XML document is empty.");
		return FALSE;
	}

	if (g_strcmp0 ((gchar*) root->name, "components") != 0) {
		fprintf (stderr, "XML file does not contain valid AppStream data!");
		return FALSE;
	}

	for (iter = root->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "component") == 0)
			as_provider_appstream_xml_parse_component_node (self, iter);
	}
	xmlFreeDoc (doc);

	return TRUE;
}

gboolean
as_provider_appstream_xml_process_compressed_file (AsProviderAppstreamXML* self, GFile* infile)
{
	GFileInputStream* src_stream;
	GMemoryOutputStream* mem_os;
	GConverterOutputStream* conv_stream;
	GZlibDecompressor* zdecomp;
	guint8* data;
	gboolean ret;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (infile != NULL, FALSE);

	src_stream = g_file_read (infile, NULL, NULL);
	mem_os = (GMemoryOutputStream*) g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
	zdecomp = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
	conv_stream = (GConverterOutputStream*) g_converter_output_stream_new ((GOutputStream*) mem_os, (GConverter*) zdecomp);
	g_object_unref (zdecomp);

	g_output_stream_splice ((GOutputStream*) conv_stream, (GInputStream*) src_stream, 0, NULL, NULL);
	data = g_memory_output_stream_get_data (mem_os);
	ret = as_provider_appstream_xml_process_single_document (self, (const gchar*) data);

	g_object_unref (conv_stream);
	g_object_unref (mem_os);
	g_object_unref (src_stream);
	return ret;
}


gboolean
as_provider_appstream_xml_process_file (AsProviderAppstreamXML* self, GFile* infile)
{
	gboolean ret;
	gchar* xml_doc;
	gchar* line = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (infile != NULL, FALSE);

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

	ret = as_provider_appstream_xml_process_single_document (self, xml_doc);
	g_object_unref (dis);
	g_free (xml_doc);
	return ret;
}


static gboolean
as_provider_appstream_xml_real_execute (AsDataProvider* base)
{
	AsProviderAppstreamXML * self;
	GPtrArray* xml_files;
	guint i;
	guint len;
	gchar *path;
	GFile *infile;

	self = (AsProviderAppstreamXML*) base;
	xml_files = g_ptr_array_new_with_free_func (g_free);

	len = G_N_ELEMENTS (AS_APPSTREAM_XML_PATHS);
	for (i = 0; i < len; i++) {
		GPtrArray *xmls;
		guint j;

		path = g_strdup (AS_APPSTREAM_XML_PATHS[i]);
		if (path == NULL)
			continue;
		if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
			g_free (path);
			continue;
		}
		xmls = as_utils_find_files_matching (path, "*.xml*", FALSE);
		if (xmls == NULL)
			continue;
		for (j = 0; j < xmls->len; j++) {
			const gchar *val;
			val = (const gchar *) g_ptr_array_index (xmls, j);
			g_ptr_array_add (xml_files, g_strdup (val));
		}

		g_free (path);
		g_ptr_array_unref (xmls);
	}

	if (xml_files->len == 0) {
		g_ptr_array_unref (xml_files);
		return FALSE;
	}

	for (i = 0; i < xml_files->len; i++) {
		gchar *fname;
		fname = (gchar*) g_ptr_array_index (xml_files, i);
		infile = g_file_new_for_path (fname);
		if (!g_file_query_exists (infile, NULL)) {
			fprintf (stderr, "File '%s' does not exist.", fname);
			g_object_unref (infile);
			continue;
		}

		if (g_str_has_suffix (fname, ".xml")) {
			as_provider_appstream_xml_process_file (self, infile);
		} else if (g_str_has_suffix (fname, ".xml.gz")) {
			as_provider_appstream_xml_process_compressed_file (self, infile);
		}
		g_object_unref (infile);
	}
	g_ptr_array_unref (xml_files);

	return TRUE;
}

static void
as_provider_appstream_xml_class_init (AsProviderAppstreamXMLClass * klass)
{
	as_provider_appstream_xml_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsProviderAppstreamXMLPrivate));
	AS_DATA_PROVIDER_CLASS (klass)->execute = as_provider_appstream_xml_real_execute;
	G_OBJECT_CLASS (klass)->finalize = as_provider_appstream_xml_finalize;
}


static void
as_provider_appstream_xml_instance_init (AsProviderAppstreamXML * self)
{
	self->priv = AS_PROVIDER_APPSTREAM_XML_GET_PRIVATE (self);
}


static void
as_provider_appstream_xml_finalize (GObject* obj)
{
	AsProviderAppstreamXML * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_PROVIDER_TYPE_APPSTREAM_XML, AsProviderAppstreamXML);
	g_free (self->priv->locale);
	g_object_unref (self->priv->system_categories);
	G_OBJECT_CLASS (as_provider_appstream_xml_parent_class)->finalize (obj);
}


GType
as_provider_appstream_xml_get_type (void)
{
	static volatile gsize as_provider_appstream_xml_type_id__volatile = 0;
	if (g_once_init_enter (&as_provider_appstream_xml_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = { sizeof (AsProviderAppstreamXMLClass),
										(GBaseInitFunc) NULL,
										(GBaseFinalizeFunc) NULL,
										(GClassInitFunc) as_provider_appstream_xml_class_init,
										(GClassFinalizeFunc) NULL,
										NULL, sizeof (AsProviderAppstreamXML),
										0,
										(GInstanceInitFunc) as_provider_appstream_xml_instance_init,
										NULL
		};
		GType as_provider_appstream_xml_type_id;
		as_provider_appstream_xml_type_id = g_type_register_static (AS_TYPE_DATA_PROVIDER, "AsProviderAppstreamXML", &g_define_type_info, 0);
		g_once_init_leave (&as_provider_appstream_xml_type_id__volatile, as_provider_appstream_xml_type_id);
	}
	return as_provider_appstream_xml_type_id__volatile;
}
