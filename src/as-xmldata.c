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

/**
 * SECTION:as-xmldata
 * @short_description: AppStream data XML serialization.
 * @include: appstream.h
 *
 * Private class to serialize AppStream data into its XML representation and
 * deserialize the data again.
 * This class is used by #AsMetadata to read AppStream XML.
 *
 * See also: #AsComponent, #AsMetadata
 */

#include "as-xmldata.h"

#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlsave.h>
#include <string.h>

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-metadata.h"
#include "as-component-private.h"
#include "as-release-private.h"

typedef struct
{
	gchar *locale;
	gchar *origin;
	gchar *media_baseurl;

	gchar *arch;
	gint default_priority;

	AsParserMode mode;
	gboolean check_valid;

	gchar *last_error_msg;
} AsXMLDataPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsXMLData, as_xmldata, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_xmldata_get_instance_private (o))

static gchar	**as_xmldata_get_children_as_strv (AsXMLData *xdt, xmlNode *node, const gchar *element_name);

/**
 * libxml_generic_error:
 *
 * Catch out-of-context errors emitted by libxml2.
 */
static void
libxml_generic_error (AsXMLData *xdt, const char *format, ...)
{
	GString *str;
	va_list arg_ptr;
	static GMutex mutex;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	g_mutex_lock (&mutex);
	str = g_string_new (priv->last_error_msg? priv->last_error_msg : "");

	va_start (arg_ptr, format);
	g_string_append_vprintf (str, format, arg_ptr);
	va_end (arg_ptr);

	g_free (priv->last_error_msg);
	priv->last_error_msg = g_string_free (str, FALSE);
	g_mutex_unlock (&mutex);
}

/**
 * as_xmldata_init:
 **/
static void
as_xmldata_init (AsXMLData *xdt)
{
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	priv->default_priority = 0;
	priv->mode = AS_PARSER_MODE_UPSTREAM;
	priv->last_error_msg = NULL;
	priv->check_valid = TRUE;
}

/**
 * as_xmldata_finalize:
 **/
static void
as_xmldata_finalize (GObject *object)
{
	AsXMLData *xdt = AS_XMLDATA (object);
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	g_free (priv->locale);
	g_free (priv->origin);
	g_free (priv->media_baseurl);
	g_free (priv->arch);
	g_free (priv->last_error_msg);
	xmlSetGenericErrorFunc (xdt, NULL);

	G_OBJECT_CLASS (as_xmldata_parent_class)->finalize (object);
}

/**
 * as_xmldata_clear_error:
 */
static void
as_xmldata_clear_error (AsXMLData *xdt)
{
	static GMutex mutex;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	g_mutex_lock (&mutex);
	g_free (priv->last_error_msg);
	priv->last_error_msg = NULL;
	xmlSetGenericErrorFunc (xdt, (xmlGenericErrorFunc) libxml_generic_error);
	g_mutex_unlock (&mutex);
}

/**
 * as_xmldata_initialize:
 * @xdt: An instance of #AsXMLData
 *
 * Initialize the XML handler.
 */
void
as_xmldata_initialize (AsXMLData *xdt, const gchar *locale, const gchar *origin, const gchar *media_baseurl, const gchar *arch, gint priority)
{
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	g_free (priv->locale);
	priv->locale = g_strdup (locale);

	g_free (priv->origin);
	priv->origin = g_strdup (origin);

	g_free (priv->media_baseurl);
	priv->media_baseurl = g_strdup (media_baseurl);

	g_free (priv->arch);
	priv->arch = g_strdup (arch);

	priv->default_priority = priority;

	as_xmldata_clear_error (xdt);
}

/**
 * as_xmldata_get_node_value:
 */
static gchar*
as_xmldata_get_node_value (AsXMLData *xdt, xmlNode *node)
{
	gchar *content;
	content = (gchar*) xmlNodeGetContent (node);

	return content;
}

/**
 * as_xmldata_dump_node_children:
 */
static gchar*
as_xmldata_dump_node_children (AsXMLData *xdt, xmlNode *node)
{
	GString *str = NULL;
	xmlNode *iter;
	xmlBufferPtr nodeBuf;

	str = g_string_new ("");
	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE) {
					continue;
		}

		nodeBuf = xmlBufferCreate();
		xmlNodeDump (nodeBuf, NULL, iter, 0, 1);
		if (str->len > 0)
			g_string_append (str, "\n");
		g_string_append_printf (str, "%s", (const gchar*) nodeBuf->content);
		xmlBufferFree (nodeBuf);
	}

	return g_string_free (str, FALSE);
}

/**
 * as_xmldata_get_node_locale:
 * @node: A XML node
 *
 * Returns: The locale of a node, if the node should be considered for inclusion.
 * %NULL if the node should be ignored due to a not-matching locale.
 */
gchar*
as_xmldata_get_node_locale (AsXMLData *xdt, xmlNode *node)
{
	gchar *lang;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	lang = (gchar*) xmlGetProp (node, (xmlChar*) "lang");

	if (lang == NULL) {
		lang = g_strdup ("C");
		goto out;
	}

	if (g_strcmp0 (priv->locale, "ALL") == 0) {
		/* we should read all languages */
		goto out;
	}

	if (as_utils_locale_is_compatible (priv->locale, lang)) {
		goto out;
	}

	/* If we are here, we haven't found a matching locale.
	 * In that case, we return %NULL to indicate that this element should not be added.
	 */
	g_free (lang);
	lang = NULL;

out:
	return lang;
}

static gchar**
as_xmldata_get_children_as_strv (AsXMLData *xdt, xmlNode* node, const gchar* element_name)
{
	GPtrArray *list;
	xmlNode *iter;
	gchar **res;
	g_return_val_if_fail (xdt != NULL, NULL);
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
				s = g_strdup (content);
				g_strstrip (s);
				g_ptr_array_add (list, s);
			}
			g_free (content);
		}
	}

	res = as_ptr_array_to_strv (list);
	g_ptr_array_unref (list);
	return res;
}

/**
 * as_xmldata_process_image:
 *
 * Read node as image node and add it to an existing screenshot.
 */
static void
as_xmldata_process_image (AsXMLData *xdt, AsComponent *cpt, xmlNode *node, AsScreenshot *scr)
{
	g_autoptr(AsImage) img = NULL;
	g_autofree gchar *content = NULL;
	g_autofree gchar *stype = NULL;
	g_autofree gchar *lang = NULL;
	guint64 width;
	guint64 height;
	AsImageKind ikind;
	gchar *str;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	content = as_xmldata_get_node_value (xdt, node);
	if (content == NULL)
		return;
	g_strstrip (content);

	img = as_image_new ();
	lang = as_xmldata_get_node_locale (xdt, node);

	/* check if this image is for us */
	if (lang == NULL)
		return;
	as_image_set_locale (img, lang);

	str = (gchar*) xmlGetProp (node, (xmlChar*) "width");
	if (str == NULL) {
		width = 0;
	} else {
		width = g_ascii_strtoll (str, NULL, 10);
		g_free (str);
	}
	str = (gchar*) xmlGetProp (node, (xmlChar*) "height");
	if (str == NULL) {
		height = 0;
	} else {
		height = g_ascii_strtoll (str, NULL, 10);
		g_free (str);
	}

	as_image_set_width (img, width);
	as_image_set_height (img, height);

	stype = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	if (g_strcmp0 (stype, "thumbnail") == 0) {
		ikind = AS_IMAGE_KIND_THUMBNAIL;
		as_image_set_kind (img, ikind);
	} else {
		ikind = AS_IMAGE_KIND_SOURCE;
		as_image_set_kind (img, ikind);
	}

	/* discard invalid elements */
	if (priv->mode == AS_PARSER_MODE_DISTRO) {
		/* no sizes are okay for upstream XML, but not for distro XML */
		if ((width == 0) || (height == 0)) {
			if (ikind != AS_IMAGE_KIND_SOURCE) {
				/* thumbnails w/o size information must never happen */
				g_debug ("WARNING: Ignored screenshot thumbnail image without size information for %s", as_component_get_id (cpt));
				return;
			}
		}
	}

	if (priv->media_baseurl == NULL) {
		/* no baseurl, we can just set the value as URL */
		as_image_set_url (img, content);
	} else {
		/* handle the media baseurl */
		gchar *tmp;
		tmp = g_build_filename (priv->media_baseurl, content, NULL);
		as_image_set_url (img, tmp);
		g_free (tmp);
	}

	as_screenshot_add_image (scr, img);
}

/**
 * as_xmldata_process_screenshot:
 */
static void
as_xmldata_process_screenshot (AsXMLData *xdt, AsComponent *cpt, xmlNode *node, AsScreenshot *scr)
{
	xmlNode *iter;
	gchar *node_name;
	gboolean subnode_found = FALSE;

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		subnode_found = TRUE;

		node_name = (gchar*) iter->name;

		if (g_strcmp0 (node_name, "image") == 0) {
			as_xmldata_process_image (xdt, cpt, iter, scr);
		} else if (g_strcmp0 (node_name, "caption") == 0) {
			g_autofree gchar *content = NULL;
			g_autofree gchar *lang = NULL;

			content = as_xmldata_get_node_value (xdt, iter);
			if (content == NULL)
				continue;
			g_strstrip (content);


			lang = as_xmldata_get_node_locale (xdt, iter);
			if (lang != NULL)
				as_screenshot_set_caption (scr, content, lang);
		}
	}

	if (!subnode_found) {
		/*
		 * We are dealing with a legacy screenshots tag in the form of
		 * <screenshot>URL</screenshot>.
		 * We treat it as an <image/> tag here, which is roughly equivalent. */
		as_xmldata_process_image (xdt, cpt, node, scr);
	}
}

/**
 * as_xmldata_process_screenshots_tag:
 */
static void
as_xmldata_process_screenshots_tag (AsXMLData *xdt, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	AsScreenshot *sshot = NULL;
	gchar *prop;
	g_return_if_fail (xdt != NULL);
	g_return_if_fail (cpt != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "screenshot") == 0) {
			sshot = as_screenshot_new ();

			/* propagate locale */
			as_screenshot_set_active_locale (sshot, as_component_get_active_locale (cpt));

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (g_strcmp0 (prop, "default") == 0)
				as_screenshot_set_kind (sshot, AS_SCREENSHOT_KIND_DEFAULT);
			as_xmldata_process_screenshot (xdt, cpt, iter, sshot);
			if (as_screenshot_is_valid (sshot))
				as_component_add_screenshot (cpt, sshot);
			g_free (prop);
			g_object_unref (sshot);
		}
	}
}

/**
 * as_xmldata_process_suggests_tag:
 */
static void
as_xmldata_process_suggests_tag (AsXMLData *xdt, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	AsSuggested *suggested = NULL;
	AsSuggestedKind suggested_kind;
	gchar *node_name;
	gchar *type_str;
	gchar *content;

	g_return_if_fail (xdt != NULL);
	g_return_if_fail (cpt != NULL);

	suggested = as_suggested_new ();
	type_str = (gchar*) xmlGetProp (node, (xmlChar*) "type");

	if (type_str != NULL) {
		suggested_kind = as_suggested_kind_from_string (type_str);
		as_suggested_set_kind (suggested, suggested_kind);
	}

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;

		if (g_strcmp0 (node_name, "id") == 0) {
			content = as_xmldata_get_node_value (xdt, iter);

			if (content != NULL)
				as_suggested_add_component_id (suggested, content);
		}

		if (as_suggested_is_valid (suggested))
			as_component_add_suggestion (cpt, suggested);

	}
}

/**
 * as_xmldata_upstream_description_to_cpt:
 *
 * Helper function for GHashTable
 */
static void
as_xmldata_upstream_description_to_cpt (gchar *key, GString *value, AsComponent *cpt)
{
	g_assert (AS_IS_COMPONENT (cpt));

	as_component_set_description (cpt, value->str, key);
	g_string_free (value, TRUE);
}

/**
 * as_xmldata_upstream_description_to_release:
 *
 * Helper function for GHashTable
 */
static void
as_xmldata_upstream_description_to_release (gchar *key, GString *value, AsRelease *rel)
{
	g_assert (AS_IS_RELEASE (rel));

	as_release_set_description (rel, value->str, key);
	g_string_free (value, TRUE);
}

/**
 * as_xmldata_parse_upstream_description_tag:
 */
static void
as_xmldata_parse_upstream_description_tag (AsXMLData *xdt, xmlNode* node, GHFunc func, gpointer entity)
{
	xmlNode *iter;
	gchar *node_name;
	GHashTable *desc;

	desc = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		GString *str;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		if (g_strcmp0 (node_name, "p") == 0) {
			g_autofree gchar *lang = NULL;
			g_autofree gchar *content = NULL;
			g_autofree gchar *tmp = NULL;

			lang = as_xmldata_get_node_locale (xdt, iter);
			if (lang == NULL)
				/* this locale is not for us */
				continue;

			str = g_hash_table_lookup (desc, lang);
			if (str == NULL) {
				str = g_string_new ("");
				g_hash_table_insert (desc, g_strdup (lang), str);
			}

			tmp = as_xmldata_get_node_value (xdt, iter);
			content = g_markup_escape_text (tmp, -1);
			g_string_append_printf (str, "<%s>%s</%s>\n", node_name, content, node_name);

		} else if ((g_strcmp0 (node_name, "ul") == 0) || (g_strcmp0 (node_name, "ol") == 0)) {
			GList *l;
			g_autoptr(GList) vlist = NULL;
			xmlNode *iter2;

			/* append listing node tag to every locale string */
			vlist = g_hash_table_get_values (desc);
			for (l = vlist; l != NULL; l = l->next) {
				g_string_append_printf (l->data, "<%s>\n", node_name);
			}

			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				g_autofree gchar *lang = NULL;
				g_autofree gchar *content = NULL;
				g_autofree gchar *tmp = NULL;

				if (iter2->type != XML_ELEMENT_NODE)
					continue;
				if (g_strcmp0 ((const gchar*) iter2->name, "li") != 0)
					continue;

				lang = as_xmldata_get_node_locale (xdt, iter2);
				if (lang == NULL)
					continue;

				/* we can not allow adding new languages starting with a enum tag, so we skip the entry if the locale is unknown */
				str = g_hash_table_lookup (desc, lang);
				if (str == NULL)
					continue;

				tmp = as_xmldata_get_node_value (xdt, iter2);
				content = g_markup_escape_text (tmp, -1);
				g_string_append_printf (str, "  <%s>%s</%s>\n", (gchar*) iter2->name, content, (gchar*) iter2->name);
			}

			/* close listing tags */
			for (l = vlist; l != NULL; l = l->next) {
				g_string_append_printf (l->data, "</%s>\n", node_name);
			}
		}
	}

	g_hash_table_foreach (desc, func, entity);
	g_hash_table_unref (desc);
}

static void
as_xmldata_process_releases_tag (AsXMLData *xdt, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	xmlNode *iter2;
	AsRelease *release = NULL;
	gchar *prop;
	guint64 timestamp;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);
	g_return_if_fail (cpt != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "release") == 0) {
			release = as_release_new ();

			/* propagate locale */
			as_release_set_active_locale (release, as_component_get_active_locale (cpt));

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "version");
			as_release_set_version (release, prop);
			g_free (prop);

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "date");
			if (prop != NULL) {
				g_autoptr(GDateTime) time;
				time = as_iso8601_to_datetime (prop);
				if (time != NULL) {
					as_release_set_timestamp (release, g_date_time_to_unix (time));
				} else {
					g_debug ("Invalid ISO-8601 date in releases of %s", as_component_get_id (cpt));
				}
				g_free (prop);
			}

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "timestamp");
			if (prop != NULL) {
				timestamp = g_ascii_strtoll (prop, NULL, 10);
				as_release_set_timestamp (release, timestamp);
				g_free (prop);
			}
			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "urgency");
			if (prop != NULL) {
				AsUrgencyKind ukind;
				ukind = as_urgency_kind_from_string (prop);
				as_release_set_urgency (release, ukind);
				g_free (prop);
			}

			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				gchar *content;

				if (iter->type != XML_ELEMENT_NODE)
					continue;

				if (g_strcmp0 ((gchar*) iter2->name, "location") == 0) {
					content = as_xmldata_get_node_value (xdt, iter2);
					as_release_add_location (release, content);
					g_free (content);
				} else if (g_strcmp0 ((gchar*) iter2->name, "checksum") == 0) {
					AsChecksumKind cs_kind;
					prop = (gchar*) xmlGetProp (iter2, (xmlChar*) "type");

					cs_kind = as_checksum_kind_from_string (prop);
					if (cs_kind != AS_CHECKSUM_KIND_NONE) {
						content = as_xmldata_get_node_value (xdt, iter2);
						as_release_set_checksum (release, content, cs_kind);
						g_free (content);
					}
					g_free (prop);
				} else if (g_strcmp0 ((gchar*) iter2->name, "size") == 0) {
					AsSizeKind s_kind;
					prop = (gchar*) xmlGetProp (iter2, (xmlChar*) "type");

					s_kind = as_size_kind_from_string (prop);
					if (s_kind != AS_SIZE_KIND_UNKNOWN) {
						guint64 size;

						content = as_xmldata_get_node_value (xdt, iter2);
						size = g_ascii_strtoull (content, NULL, 10);
						g_free (content);
						if (size > 0)
							as_release_set_size (release, size, s_kind);
					}
					g_free (prop);
				} else if (g_strcmp0 ((gchar*) iter2->name, "description") == 0) {
					if (priv->mode == AS_PARSER_MODE_DISTRO) {
						g_autofree gchar *lang;

						/* for distro XML, the "description" tag has a language property, so parsing it is simple */
						content = as_xmldata_dump_node_children (xdt, iter2);
						lang = as_xmldata_get_node_locale (xdt, iter2);
						if (lang != NULL)
							as_release_set_description (release, content, lang);
						g_free (content);
					} else {
						as_xmldata_parse_upstream_description_tag (xdt,
											iter2,
											(GHFunc) as_xmldata_upstream_description_to_release,
											release);
					}
				}
			}

			as_component_add_release (cpt, release);
			g_object_unref (release);
		}
	}
}

static void
as_xmldata_process_provides (AsXMLData *xdt, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	gchar *node_name;
	g_return_if_fail (xdt != NULL);
	g_return_if_fail (cpt != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		g_autofree gchar *content = NULL;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		content = as_xmldata_get_node_value (xdt, iter);
		if (content == NULL)
			continue;

		if (g_strcmp0 (node_name, "library") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_LIBRARY, content);
		} else if (g_strcmp0 (node_name, "binary") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_BINARY, content);
		} else if (g_strcmp0 (node_name, "font") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FONT, content);
		} else if (g_strcmp0 (node_name, "modalias") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_MODALIAS, content);
		} else if (g_strcmp0 (node_name, "firmware") == 0) {
			g_autofree gchar *fwtype = NULL;
			fwtype = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (fwtype != NULL) {
				if (g_strcmp0 (fwtype, "runtime") == 0)
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FIRMWARE_RUNTIME, content);
				else if (g_strcmp0 (fwtype, "flashed") == 0)
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FIRMWARE_FLASHED, content);
			}
		} else if (g_strcmp0 (node_name, "python2") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_PYTHON_2, content);
		} else if (g_strcmp0 (node_name, "python3") == 0) {
			as_component_add_provided_item (cpt, AS_PROVIDED_KIND_PYTHON, content);
		} else if (g_strcmp0 (node_name, "dbus") == 0) {
			g_autofree gchar *dbustype = NULL;
			dbustype = (gchar*) xmlGetProp (iter, (xmlChar*) "type");

			if (g_strcmp0 (dbustype, "system") == 0)
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_DBUS_SYSTEM, content);
			else if ((g_strcmp0 (dbustype, "user") == 0) || (g_strcmp0 (dbustype, "session") == 0))
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_DBUS_USER, content);
		}
	}
}

static void
as_xmldata_set_component_type_from_node (xmlNode *node, AsComponent *cpt)
{
	gchar *cpttype;

	/* find out which kind of component we are dealing with */
	cpttype = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	if ((cpttype == NULL) || (g_strcmp0 (cpttype, "generic") == 0)) {
		as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	} else {
		AsComponentKind ckind;
		ckind = as_component_kind_from_string (cpttype);
		as_component_set_kind (cpt, ckind);
		if (ckind == AS_COMPONENT_KIND_UNKNOWN)
			g_debug ("An unknown component was found: %s", cpttype);
	}
	g_free (cpttype);
}

static void
as_xmldata_process_languages_tag (AsXMLData *xdt, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	g_return_if_fail (xdt != NULL);
	g_return_if_fail (cpt != NULL);

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

			locale = as_xmldata_get_node_value (xdt, iter);
			as_component_add_language (cpt, locale, percentage);
		}
	}
}

/**
 * as_xml_icon_set_size_from_node:
 */
static void
as_xml_icon_set_size_from_node (xmlNode *node, AsIcon *icon)
{
	gchar *val;

	val = (gchar*) xmlGetProp (node, (xmlChar*) "width");
	if (val != NULL) {
		as_icon_set_width (icon, g_ascii_strtoll (val, NULL, 10));
		g_free (val);
	}
	val = (gchar*) xmlGetProp (node, (xmlChar*) "height");
	if (val != NULL) {
		as_icon_set_height (icon, g_ascii_strtoll (val, NULL, 10));
		g_free (val);
	}
}

/**
 * as_xmldata_parse_component_node:
 */
void
as_xmldata_parse_component_node (AsXMLData *xdt, xmlNode* node, AsComponent *cpt, GError **error)
{
	xmlNode *iter;
	const gchar *node_name;
	GPtrArray *compulsory_for_desktops;
	GPtrArray *pkgnames;
	gchar **strv;
	g_autofree gchar *priority_str;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	compulsory_for_desktops = g_ptr_array_new_with_free_func (g_free);
	pkgnames = g_ptr_array_new_with_free_func (g_free);

	/* set component kind */
	as_xmldata_set_component_type_from_node (node, cpt);

	/* set the priority for this component */
	priority_str = (gchar*) xmlGetProp (node, (xmlChar*) "priority");
	if (priority_str == NULL) {
		as_component_set_priority (cpt, priv->default_priority);
	} else {
		as_component_set_priority (cpt,
					   g_ascii_strtoll (priority_str, NULL, 10));

	}

	/* set active locale for this component */
	as_component_set_active_locale (cpt, priv->locale);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		g_autofree gchar *content = NULL;
		g_autofree gchar *lang = NULL;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (const gchar*) iter->name;
		content = as_xmldata_get_node_value (xdt, iter);
		g_strstrip (content);
		lang = as_xmldata_get_node_locale (xdt, iter);

		if (g_strcmp0 (node_name, "id") == 0) {
				as_component_set_id (cpt, content);
				if ((priv->mode == AS_PARSER_MODE_UPSTREAM) &&
					(as_component_get_kind (cpt) == AS_COMPONENT_KIND_GENERIC)) {
					/* parse legacy component type information */
					as_xmldata_set_component_type_from_node (iter, cpt);
				}
		} else if (g_strcmp0 (node_name, "pkgname") == 0) {
			if (content != NULL)
				g_ptr_array_add (pkgnames, g_strdup (content));
		} else if (g_strcmp0 (node_name, "source_pkgname") == 0) {
			as_component_set_source_pkgname (cpt, content);
		} else if (g_strcmp0 (node_name, "name") == 0) {
			if (lang != NULL)
				as_component_set_name (cpt, content, lang);
		} else if (g_strcmp0 (node_name, "summary") == 0) {
			if (lang != NULL)
				as_component_set_summary (cpt, content, lang);
		} else if (g_strcmp0 (node_name, "description") == 0) {
			if (priv->mode == AS_PARSER_MODE_DISTRO) {
				/* for distro XML, the "description" tag has a language property, so parsing it is simple */
				if (lang != NULL) {
					gchar *desc;
					desc = as_xmldata_dump_node_children (xdt, iter);
					as_component_set_description (cpt, desc, lang);
					g_free (desc);
				}
			} else {
				as_xmldata_parse_upstream_description_tag (xdt,
									iter,
									(GHFunc) as_xmldata_upstream_description_to_cpt,
									cpt);
			}
		} else if (g_strcmp0 (node_name, "icon") == 0) {
			gchar *prop;
			g_autoptr(AsIcon) icon = NULL;
			if (content == NULL)
				continue;

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			icon = as_icon_new ();

			if (g_strcmp0 (prop, "stock") == 0) {
				as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
				as_icon_set_name (icon, content);
				as_component_add_icon (cpt, icon);
			} else if (g_strcmp0 (prop, "cached") == 0) {
				as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
				as_icon_set_filename (icon, content);
				as_xml_icon_set_size_from_node (iter, icon);
				as_component_add_icon (cpt, icon);
			} else if (g_strcmp0 (prop, "local") == 0) {
				as_icon_set_kind (icon, AS_ICON_KIND_LOCAL);
				as_icon_set_filename (icon, content);
				as_xml_icon_set_size_from_node (iter, icon);
				as_component_add_icon (cpt, icon);
			} else if (g_strcmp0 (prop, "remote") == 0) {
				as_icon_set_kind (icon, AS_ICON_KIND_REMOTE);
				if (priv->media_baseurl == NULL) {
					/* no baseurl, we can just set the value as URL */
					as_icon_set_url (icon, content);
				} else {
					/* handle the media baseurl */
					gchar *tmp;
					tmp = g_build_filename (priv->media_baseurl, content, NULL);
					as_icon_set_url (icon, tmp);
					g_free (tmp);
				}
				as_xml_icon_set_size_from_node (iter, icon);
				as_component_add_icon (cpt, icon);
			}
		} else if (g_strcmp0 (node_name, "url") == 0) {
			if (content != NULL) {
				gchar *urltype_str;
				AsUrlKind url_kind;
				urltype_str = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
				url_kind = as_url_kind_from_string (urltype_str);
				if (url_kind != AS_URL_KIND_UNKNOWN)
					as_component_add_url (cpt, url_kind, content);
				g_free (urltype_str);
			}
		} else if (g_strcmp0 (node_name, "categories") == 0) {
			gchar **cat_array;
			cat_array = as_xmldata_get_children_as_strv (xdt, iter, "category");
			as_component_set_categories (cpt, cat_array);
			g_strfreev (cat_array);
		} else if (g_strcmp0 (node_name, "keywords") == 0) {
			gchar **kw_array;
			kw_array = as_xmldata_get_children_as_strv (xdt, iter, "keyword");
			as_component_set_keywords (cpt, kw_array, NULL);
			g_strfreev (kw_array);
		} else if (g_strcmp0 (node_name, "mimetypes") == 0) {
			g_auto(GStrv) mime_array = NULL;
			guint i;

			/* Mimetypes are essentially provided interfaces, that's why they belong into AsProvided.
			 * However, due to historic reasons, the spec has an own toplevel tag for them, so we need
			 * to parse them here. */
			mime_array = as_xmldata_get_children_as_strv (xdt, iter, "mimetype");
			for (i = 0; mime_array[i] != NULL; i++) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_MIMETYPE, mime_array[i]);
			}
		} else if (g_strcmp0 (node_name, "provides") == 0) {
			as_xmldata_process_provides (xdt, iter, cpt);
		} else if (g_strcmp0 (node_name, "screenshots") == 0) {
			as_xmldata_process_screenshots_tag (xdt, iter, cpt);
		} else if (g_strcmp0 (node_name, "suggests") == 0) {
			as_xmldata_process_suggests_tag (xdt, iter, cpt);
		} else if (g_strcmp0 (node_name, "project_license") == 0) {
			if (content != NULL)
				as_component_set_project_license (cpt, content);
		} else if (g_strcmp0 (node_name, "project_group") == 0) {
			if (content != NULL)
				as_component_set_project_group (cpt, content);
		} else if (g_strcmp0 (node_name, "developer_name") == 0) {
			if (lang != NULL)
				as_component_set_developer_name (cpt, content, lang);
		} else if (g_strcmp0 (node_name, "compulsory_for_desktop") == 0) {
			if (content != NULL)
				g_ptr_array_add (compulsory_for_desktops, g_strdup (content));
		} else if (g_strcmp0 (node_name, "releases") == 0) {
			as_xmldata_process_releases_tag (xdt, iter, cpt);
		} else if (g_strcmp0 (node_name, "extends") == 0) {
			if (content != NULL)
				as_component_add_extends (cpt, content);
		} else if (g_strcmp0 (node_name, "languages") == 0) {
			as_xmldata_process_languages_tag (xdt, iter, cpt);
		} else if (g_strcmp0 (node_name, "bundle") == 0) {
			if (content != NULL) {
				gchar *type_str;
				AsBundleKind bundle_kind;
				type_str = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
				bundle_kind = as_bundle_kind_from_string (type_str);
				if (bundle_kind != AS_BUNDLE_KIND_UNKNOWN)
					bundle_kind = AS_BUNDLE_KIND_LIMBA;
				as_component_add_bundle_id (cpt, bundle_kind, content);
				g_free (type_str);
			}
		} else if (g_strcmp0 (node_name, "translation") == 0) {
			if (content != NULL) {
				g_autofree gchar *trtype_str = NULL;
				AsTranslationKind tr_kind;
				trtype_str = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
				tr_kind = as_translation_kind_from_string (trtype_str);
				if (tr_kind != AS_TRANSLATION_KIND_UNKNOWN) {
					g_autoptr(AsTranslation) tr = NULL;

					tr = as_translation_new ();
					as_translation_set_kind (tr, tr_kind);
					as_translation_set_id (tr, content);
					as_component_add_translation (cpt, tr);
				}
			}
		}
	}

	/* set the origin of this component */
	as_component_set_origin (cpt, priv->origin);

	/* set the architecture of this component */
	as_component_set_architecture (cpt, priv->arch);

	/* add package name information to component */
	strv = as_ptr_array_to_strv (pkgnames);
	as_component_set_pkgnames (cpt, strv);
	g_ptr_array_unref (pkgnames);
	g_strfreev (strv);

	/* add compulsoriy information to component as strv */
	strv = as_ptr_array_to_strv (compulsory_for_desktops);
	as_component_set_compulsory_for_desktops (cpt, strv);
	g_ptr_array_unref (compulsory_for_desktops);
	g_strfreev (strv);
}

/**
 * as_xmldata_parse_components_node:
 */
static void
as_xmldata_parse_components_node (AsXMLData *xdt, GPtrArray *cpts, xmlNode* node, GError **error)
{
	xmlNode* iter;
	GError *tmp_error = NULL;
	gchar *priority_str;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	/* set origin of this metadata */
	g_free (priv->origin);
	priv->origin = (gchar*) xmlGetProp (node, (xmlChar*) "origin");

	/* set baseurl for the media files */
	g_free (priv->media_baseurl);
	priv->media_baseurl =  (gchar*) xmlGetProp (node, (xmlChar*) "media_baseurl");

	/* set architecture for the components */
	g_free (priv->arch);
	priv->arch =  (gchar*) xmlGetProp (node, (xmlChar*) "architecture");

	/* distro metadata allows setting a priority for components */
	priority_str = (gchar*) xmlGetProp (node, (xmlChar*) "priority");
	if (priority_str != NULL) {
		priv->default_priority = g_ascii_strtoll (priority_str, NULL, 10);
	}
	g_free (priority_str);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "component") == 0) {
			g_autoptr(AsComponent) cpt = NULL;

			cpt = as_component_new ();
			as_xmldata_parse_component_node (xdt, iter, cpt, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return;
			} else {
				g_ptr_array_add (cpts, g_object_ref (cpt));
			}
		}
	}
}

/**
 * as_xmldata_xml_add_node:
 *
 * Add node if value is not empty
 */
static xmlNode*
as_xmldata_xml_add_node (xmlNode *root, const gchar *name, const gchar *value)
{
	if (as_str_empty (value))
		return NULL;

	return xmlNewTextChild (root, NULL, (xmlChar*) name, (xmlChar*) value);
}

/**
 * as_xmldata_xml_add_description:
 *
 * Add the description markup to the XML tree
 */
static gboolean
as_xmldata_xml_add_description (AsXMLData *xdt, xmlNode *root, xmlNode **desc_node, const gchar *description_markup, const gchar *lang)
{
	g_autofree gchar *xmldata = NULL;
	xmlDoc *doc;
	xmlNode *droot;
	xmlNode *dnode;
	xmlNode *iter;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);
	gboolean ret = TRUE;
	gboolean localized;

	if (as_str_empty (description_markup))
		return FALSE;

	/* skip cruft */
	if (as_is_cruft_locale (lang))
		return FALSE;

	xmldata = g_strdup_printf ("<root>%s</root>", description_markup);
	doc = xmlReadMemory (xmldata, strlen (xmldata),
			     NULL,
			     "utf-8",
			     XML_PARSE_NOBLANKS | XML_PARSE_NONET);
	if (doc == NULL) {
		ret = FALSE;
		goto out;
	}

	droot = xmlDocGetRootElement (doc);
	if (droot == NULL) {
		ret = FALSE;
		goto out;
	}

	if (priv->mode == AS_PARSER_MODE_UPSTREAM) {
		if (*desc_node == NULL)
			*desc_node = xmlNewChild (root, NULL, (xmlChar*) "description", NULL);
		dnode = *desc_node;
	} else {
		/* in distro parser mode, we might have multiple <description/> tags */
		dnode = xmlNewChild (root, NULL, (xmlChar*) "description", NULL);
	}

	localized = g_strcmp0 (lang, "C") != 0;
	if (priv->mode != AS_PARSER_MODE_UPSTREAM) {
		if (localized) {
			xmlNewProp (dnode,
					(xmlChar*) "xml:lang",
					(xmlChar*) lang);
		}
	}

	for (iter = droot->children; iter != NULL; iter = iter->next) {
		xmlNode *cn;

		if (g_strcmp0 ((const gchar*) iter->name, "p") == 0) {
			cn = xmlAddChild (dnode, xmlCopyNode (iter, TRUE));

			if ((priv->mode == AS_PARSER_MODE_UPSTREAM) && (localized)) {
				xmlNewProp (cn,
					(xmlChar*) "xml:lang",
					(xmlChar*) lang);
			}
		} else if ((g_strcmp0 ((const gchar*) iter->name, "ul") == 0) || (g_strcmp0 ((const gchar*) iter->name, "ol") == 0)) {
			xmlNode *iter2;
			xmlNode *enumNode;

			enumNode = xmlNewChild (dnode, NULL, iter->name, NULL);
			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (g_strcmp0 ((const gchar*) iter2->name, "li") != 0)
					continue;

				cn = xmlAddChild (enumNode, xmlCopyNode (iter2, TRUE));

				if ((priv->mode == AS_PARSER_MODE_UPSTREAM) && (localized)) {
					xmlNewProp (cn,
						(xmlChar*) "xml:lang",
						(xmlChar*) lang);
				}
			}

			continue;
		}
	}

out:
	if (doc != NULL)
		xmlFreeDoc (doc);
	return ret;
}

/**
 * as_xmldata_xml_add_node_list:
 *
 * Add node if value is not empty
 */
static void
as_xmldata_xml_add_node_list (xmlNode *root, const gchar *name, const gchar *child_name, gchar **strv)
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

typedef struct {
	AsXMLData *xdt;

	xmlNode *parent;
	xmlNode *nd;
	const gchar *node_name;
} AsLocaleWriteHelper;

/**
 * as_xml_lang_hashtable_to_nodes_cb:
 */
static void
as_xml_lang_hashtable_to_nodes_cb (gchar *key, gchar *value, AsLocaleWriteHelper *helper)
{
	xmlNode *cnode;
	if (as_str_empty (value))
		return;

	/* skip cruft */
	if (as_is_cruft_locale (key))
		return;

	cnode = xmlNewTextChild (helper->parent, NULL, (xmlChar*) helper->node_name, (xmlChar*) value);
	if (g_strcmp0 (key, "C") != 0) {
		xmlNewProp (cnode,
				(xmlChar*) "xml:lang",
				(xmlChar*) key);
	}
}

/**
 * as_xml_desc_lang_hashtable_to_nodes_cb:
 */
static void
as_xml_desc_lang_hashtable_to_nodes_cb (gchar *key, gchar *value, AsLocaleWriteHelper *helper)
{
	if (as_str_empty (value))
		return;

	as_xmldata_xml_add_description (helper->xdt, helper->parent, &helper->nd, value, key);
}

/**
 * as_xml_serialize_image:
 */
static void
as_xml_serialize_image (AsImage *img, xmlNode *subnode)
{
	xmlNode* n_image = NULL;
	gchar *size;
	const gchar *locale;
	g_return_if_fail (img != NULL);
	g_return_if_fail (subnode != NULL);

	n_image = xmlNewTextChild (subnode, NULL, (xmlChar*) "image", (xmlChar*) as_image_get_url (img));
	if (as_image_get_kind (img) == AS_IMAGE_KIND_THUMBNAIL)
		xmlNewProp (n_image, (xmlChar*) "type", (xmlChar*) "thumbnail");
	else
		xmlNewProp (n_image, (xmlChar*) "type", (xmlChar*) "source");

	if ((as_image_get_width (img) > 0) &&
		(as_image_get_height (img) > 0)) {
		size = g_strdup_printf("%i", as_image_get_width (img));
		xmlNewProp (n_image, (xmlChar*) "width", (xmlChar*) size);
		g_free (size);

		size = g_strdup_printf("%i", as_image_get_height (img));
		xmlNewProp (n_image, (xmlChar*) "height", (xmlChar*) size);
		g_free (size);
	}

	locale = as_image_get_locale (img);
	if ((locale != NULL) && (g_strcmp0 (locale, "C") != 0)) {
		xmlNewProp (n_image, (xmlChar*) "xml:lang", (xmlChar*) locale);
	}

	xmlAddChild (subnode, n_image);
}

/**
 * as_xmldata_add_screenshot_subnodes:
 *
 * Add screenshot subnodes to a root node
 */
static void
as_xmldata_add_screenshot_subnodes (AsComponent *cpt, xmlNode *root)
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

		subnode = xmlNewChild (root, NULL, (xmlChar*) "screenshot", (xmlChar*) "");
		if (as_screenshot_get_kind (sshot) == AS_SCREENSHOT_KIND_DEFAULT)
			xmlNewProp (subnode, (xmlChar*) "type", (xmlChar*) "default");

		str = as_screenshot_get_caption (sshot);
		if (str != NULL) {
			xmlNode* n_caption;
			n_caption = xmlNewTextChild (subnode, NULL, (xmlChar*) "caption", (xmlChar*) str);
			xmlAddChild (subnode, n_caption);
		}

		images = as_screenshot_get_images (sshot);
		g_ptr_array_foreach (images, (GFunc) as_xml_serialize_image, subnode);
	}
}

/**
 * as_xmldata_add_release_subnodes:
 *
 * Add release nodes to a root node
 */
static void
as_xmldata_add_release_subnodes (AsXMLData *xdt, AsComponent *cpt, xmlNode *root)
{
	GPtrArray *releases;
	AsRelease *release;
	guint i;
	AsLocaleWriteHelper helper;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	/* prepare helper */
	helper.xdt = xdt;
	helper.nd = NULL;

	releases = as_component_get_releases (cpt);
	for (i = 0; i < releases->len; i++) {
		xmlNode *subnode;
		glong unixtime;
		GPtrArray *locations;
		guint j;
		release = (AsRelease*) g_ptr_array_index (releases, i);

		/* set release version */
		subnode = xmlNewChild (root, NULL, (xmlChar*) "release", (xmlChar*) "");
		xmlNewProp (subnode, (xmlChar*) "version",
					(xmlChar*) as_release_get_version (release));

		/* set release timestamp / date */
		unixtime = as_release_get_timestamp (release);
		if (unixtime > 0) {
			g_autofree gchar *time_str = NULL;

			if (priv->mode == AS_PARSER_MODE_DISTRO) {
				time_str = g_strdup_printf ("%ld", unixtime);
				xmlNewProp (subnode, (xmlChar*) "timestamp",
						(xmlChar*) time_str);
			} else {
				GTimeVal time;
				time.tv_sec = unixtime;
				time.tv_usec = 0;
				time_str = g_time_val_to_iso8601 (&time);
				xmlNewProp (subnode, (xmlChar*) "date",
						(xmlChar*) time_str);
			}
		}

		/* set release urgency, if we have one */
		if (as_release_get_urgency (release) != AS_URGENCY_KIND_UNKNOWN) {
			const gchar *urgency_str;
			urgency_str = as_urgency_kind_to_string (as_release_get_urgency (release));
			xmlNewProp (subnode, (xmlChar*) "urgency",
					(xmlChar*) urgency_str);
		}

		/* add location urls */
		locations = as_release_get_locations (release);
		for (j = 0; j < locations->len; j++) {
			const gchar *lurl;
			lurl = (const gchar*) g_ptr_array_index (locations, j);
			xmlNewTextChild (subnode, NULL, (xmlChar*) "location", (xmlChar*) lurl);
		}

		/* add checksum node */
		for (j = 0; j < AS_CHECKSUM_KIND_LAST; j++) {
			if (as_release_get_checksum (release, j) != NULL) {
				xmlNode *cs_node;
				cs_node = xmlNewTextChild (subnode,
								NULL,
								(xmlChar*) "checksum",
								(xmlChar*) as_release_get_checksum (release, j));
				xmlNewProp (cs_node,
						(xmlChar*) "type",
						(xmlChar*) as_checksum_kind_to_string (j));
			}
		}

		/* add size node */
		for (j = 0; j < AS_SIZE_KIND_LAST; j++) {
			if (as_release_get_size (release, j) > 0) {
				xmlNode *s_node;
				g_autofree gchar *size_str;

				size_str = g_strdup_printf ("%" G_GUINT64_FORMAT, as_release_get_size (release, j));
				s_node = xmlNewTextChild (subnode,
							  NULL,
							  (xmlChar*) "size",
							  (xmlChar*) size_str);
				xmlNewProp (s_node,
						(xmlChar*) "type",
						(xmlChar*) as_size_kind_to_string (j));
			}
		}

		/* add description */
		helper.parent = subnode;
		helper.node_name = "description";
		g_hash_table_foreach (as_release_get_description_table (release),
				      (GHFunc) as_xml_desc_lang_hashtable_to_nodes_cb,
				      &helper);
	}
}

/**
 * as_xml_serialize_provides:
 */
static void
as_xml_serialize_provides (AsComponent *cpt, xmlNode *cnode)
{
	xmlNode *node;
	g_autoptr(GList) prov_list = NULL;
	GList *l;
	gchar **items;
	guint i;
	AsProvided *prov_mime;

	prov_list = as_component_get_provided (cpt);
	if (prov_list == NULL)
		return;

	prov_mime = as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_MIMETYPE);
	if (prov_mime != NULL) {
		/* mimetypes get special treatment in XML for historical reasons */
		node = xmlNewChild (cnode, NULL, (xmlChar*) "mimetypes", NULL);
		items = as_provided_get_items (prov_mime);

		for (i = 0; items[i] != NULL; i++) {
			xmlNewTextChild (node,
					  NULL,
					  (xmlChar*) "mimetype",
					  (xmlChar*) items[i]);
		}
	}

	/* check if we only had mimetype provided items, in that case we don't need to continue */
	if ((as_provided_get_kind (AS_PROVIDED (prov_list->data)) == AS_PROVIDED_KIND_MIMETYPE) &&
	    (prov_list->next == NULL))
		return;

	node = xmlNewChild (cnode, NULL, (xmlChar*) "provides", NULL);
	for (l = prov_list; l != NULL; l = l->next) {
		AsProvided *prov = AS_PROVIDED (l->data);

		items = as_provided_get_items (prov);
		switch (as_provided_get_kind (prov)) {
			case AS_PROVIDED_KIND_MIMETYPE:
				/* we already handled those */
				break;
			case AS_PROVIDED_KIND_LIBRARY:
				as_xmldata_xml_add_node_list (node, NULL,
							      "library",
							      items);
				break;
			case AS_PROVIDED_KIND_BINARY:
				as_xmldata_xml_add_node_list (node, NULL,
							      "binary",
							      items);
				break;
			case AS_PROVIDED_KIND_MODALIAS:
				as_xmldata_xml_add_node_list (node, NULL,
							      "modalias",
							      items);
				break;
			case AS_PROVIDED_KIND_PYTHON_2:
				as_xmldata_xml_add_node_list (node, NULL,
							      "python2",
							      items);
				break;
			case AS_PROVIDED_KIND_PYTHON:
				as_xmldata_xml_add_node_list (node, NULL,
							      "python3",
							      items);
				break;
			case AS_PROVIDED_KIND_FIRMWARE_RUNTIME:
				for (i = 0; items[i] != NULL; i++) {
					xmlNode *n;
					n = xmlNewTextChild (node, NULL, (xmlChar*) "firmware", (xmlChar*) items[i]);
					xmlNewProp (n, (xmlChar*) "type", (xmlChar*) "runtime");
				}
				break;
			case AS_PROVIDED_KIND_FIRMWARE_FLASHED:
				for (i = 0; items[i] != NULL; i++) {
					xmlNode *n;
					n = xmlNewTextChild (node, NULL, (xmlChar*) "firmware", (xmlChar*) items[i]);
					xmlNewProp (n, (xmlChar*) "type", (xmlChar*) "runtime");
				}
				break;
			case AS_PROVIDED_KIND_DBUS_SYSTEM:
				for (i = 0; items[i] != NULL; i++) {
					xmlNode *n;
					n = xmlNewTextChild (node, NULL, (xmlChar*) "dbus", (xmlChar*) items[i]);
					xmlNewProp (n, (xmlChar*) "type", (xmlChar*) "system");
				}
				break;
			case AS_PROVIDED_KIND_DBUS_USER:
				for (i = 0; items[i] != NULL; i++) {
					xmlNode *n;
					n = xmlNewTextChild (node, NULL, (xmlChar*) "dbus", (xmlChar*) items[i]);
					xmlNewProp (n, (xmlChar*) "type", (xmlChar*) "user");
				}
				break;

			default:
				/* TODO: Serialize fonts tag properly */
				g_debug ("Couldn't serialize provided-item type '%s'", as_provided_kind_to_string (as_provided_get_kind (prov)));
				break;
		}
	}
}

/**
 * as_xml_serialize_languages:
 */
static void
as_xml_serialize_languages (AsComponent *cpt, xmlNode *cnode)
{
	xmlNode *node;
	GHashTable *lang_table;
	GHashTableIter iter;
	gpointer key, value;

	lang_table = as_component_get_languages_map (cpt);
	if (g_hash_table_size (lang_table) == 0)
		return;

	node = xmlNewChild (cnode, NULL, (xmlChar*) "languages", NULL);
	g_hash_table_iter_init (&iter, lang_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		guint percentage;
		const gchar *locale;
		xmlNode *l_node;
		g_autofree gchar *percentage_str = NULL;

		locale = (const gchar*) key;
		percentage = GPOINTER_TO_INT (value);
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
 * as_xmldata_component_to_node:
 * @cpt: a valid #AsComponent
 *
 * Serialize the component data to an xmlNode.
 *
 */
static xmlNode*
as_xmldata_component_to_node (AsXMLData *xdt, AsComponent *cpt)
{
	xmlNode *cnode;
	xmlNode *node;
	gchar **strv;
	GPtrArray *releases;
	GPtrArray *screenshots;
	GPtrArray *icons;
	AsComponentKind kind;
	AsLocaleWriteHelper helper;
	guint i;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);
	g_return_val_if_fail (cpt != NULL, NULL);

	/* define component root node */
	kind = as_component_get_kind (cpt);
	cnode = xmlNewNode (NULL, (xmlChar*) "component");
	if ((kind != AS_COMPONENT_KIND_GENERIC) && (kind != AS_COMPONENT_KIND_UNKNOWN)) {
		xmlNewProp (cnode, (xmlChar*) "type",
					(xmlChar*) as_component_kind_to_string (kind));
	}

	as_xmldata_xml_add_node (cnode, "id", as_component_get_id (cpt));

	helper.parent = cnode;
	helper.xdt = xdt;
	helper.nd = NULL;
	helper.node_name = "name";
	g_hash_table_foreach (as_component_get_name_table (cpt),
					(GHFunc) as_xml_lang_hashtable_to_nodes_cb,
					&helper);

	helper.nd = NULL;
	helper.node_name = "summary";
	g_hash_table_foreach (as_component_get_summary_table (cpt),
					(GHFunc) as_xml_lang_hashtable_to_nodes_cb,
					&helper);

	helper.nd = NULL;
	helper.node_name = "developer_name";
	g_hash_table_foreach (as_component_get_developer_name_table (cpt),
					(GHFunc) as_xml_lang_hashtable_to_nodes_cb,
					&helper);

	helper.nd = NULL;
	helper.node_name = "description";
	g_hash_table_foreach (as_component_get_description_table (cpt),
					(GHFunc) as_xml_desc_lang_hashtable_to_nodes_cb,
					&helper);

	as_xmldata_xml_add_node (cnode, "project_license", as_component_get_project_license (cpt));
	as_xmldata_xml_add_node (cnode, "project_group", as_component_get_project_group (cpt));

	as_xmldata_xml_add_node_list (cnode, NULL, "pkgname", as_component_get_pkgnames (cpt));
	strv = as_ptr_array_to_strv (as_component_get_extends (cpt));
	as_xmldata_xml_add_node_list (cnode, NULL, "extends", strv);
	g_strfreev (strv);
	as_xmldata_xml_add_node_list (cnode, NULL, "compulsory_for_desktop", as_component_get_compulsory_for_desktops (cpt));
	as_xmldata_xml_add_node_list (cnode, "keywords", "keyword", as_component_get_keywords (cpt));
	as_xmldata_xml_add_node_list (cnode, "categories", "category", as_component_get_categories (cpt));

	/* urls */
	for (i = AS_URL_KIND_UNKNOWN; i < AS_URL_KIND_LAST; i++) {
		xmlNode *n;
		const gchar *value;
		value = as_component_get_url (cpt, i);
		if (value == NULL)
			continue;

		n = xmlNewTextChild (cnode, NULL, (xmlChar*) "url", (xmlChar*) value);
		xmlNewProp (n, (xmlChar*) "type",
					(xmlChar*) as_url_kind_to_string (i));
	}

	/* icons */
	icons = as_component_get_icons (cpt);
	for (i = 0; i < icons->len; i++) {
		AsIconKind ikind;
		xmlNode *n;
		const gchar *value;
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		ikind = as_icon_get_kind (icon);
		if (ikind == AS_ICON_KIND_LOCAL)
			value = as_icon_get_filename (icon);
		else if (ikind == AS_ICON_KIND_REMOTE)
			value = as_icon_get_url (icon);
		else
			value = as_icon_get_name (icon);

		if (value == NULL)
			continue;

		n = xmlNewTextChild (cnode, NULL, (xmlChar*) "icon", (xmlChar*) value);
		xmlNewProp (n, (xmlChar*) "type",
					(xmlChar*) as_icon_kind_to_string (ikind));

		if (ikind != AS_ICON_KIND_STOCK) {
			if (as_icon_get_width (icon) > 0) {
				g_autofree gchar *size = NULL;
				size = g_strdup_printf ("%i", as_icon_get_width (icon));
				xmlNewProp (n, (xmlChar*) "width", (xmlChar*) size);
			}

			if (as_icon_get_height (icon) > 0) {
				g_autofree gchar *size = NULL;
				size = g_strdup_printf ("%i", as_icon_get_height (icon));
				xmlNewProp (n, (xmlChar*) "height", (xmlChar*) size);
			}
		}
	}

	/* bundles */
	for (i = AS_BUNDLE_KIND_UNKNOWN; i < AS_BUNDLE_KIND_LAST; i++) {
		xmlNode *n;
		const gchar *value;
		value = as_component_get_bundle_id (cpt, i);
		if (value == NULL)
			continue;

		n = xmlNewTextChild (cnode, NULL, (xmlChar*) "bundle", (xmlChar*) value);
		xmlNewProp (n, (xmlChar*) "type",
					(xmlChar*) as_bundle_kind_to_string (i));
	}

	/* translations */
	if (priv->mode == AS_PARSER_MODE_UPSTREAM) {
		GPtrArray *translations;

		/* the translations tag is only valid in metainfo files */
		translations = as_component_get_translations (cpt);
		for (i = 0; i < translations->len; i++) {
			AsTranslation *tr = AS_TRANSLATION (g_ptr_array_index (translations, i));
			xmlNode *n;
			n = xmlNewTextChild (cnode, NULL, (xmlChar*) "translation", (xmlChar*) as_translation_get_id (tr));
			xmlNewProp (n, (xmlChar*) "type",
						(xmlChar*) as_translation_kind_to_string (as_translation_get_kind (tr)));
		}
	}

	/* screenshots node */
	screenshots = as_component_get_screenshots (cpt);
	if (screenshots->len > 0) {
		node = xmlNewChild (cnode, NULL, (xmlChar*) "screenshots", NULL);
		as_xmldata_add_screenshot_subnodes (cpt, node);
	}

	/* releases node */
	releases = as_component_get_releases (cpt);
	if (releases->len > 0) {
		node = xmlNewChild (cnode, NULL, (xmlChar*) "releases", NULL);
		as_xmldata_add_release_subnodes (xdt, cpt, node);
	}

	/* provides & mimetypes node */
	as_xml_serialize_provides (cpt, cnode);

	/* languages node */
	as_xml_serialize_languages (cpt, cnode);

	return cnode;
}

/**
 * as_xmldata_update_cpt_with_upstream_data:
 * @xdt: An instance of #AsXMLData
 * @data: XML representing upstream metadata.
 * @cpt: Component that should be updated.
 * @error: A #GError
 *
 * Parse AppStream upstream metadata.
 */
gboolean
as_xmldata_update_cpt_with_upstream_data (AsXMLData *xdt, const gchar *data, AsComponent *cpt, GError **error)
{
	xmlDoc *doc;
	xmlNode *root;
	gboolean ret = FALSE;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	if (data == NULL) {
		/* empty document means no components */
		return FALSE;
	}

	doc = as_xmldata_parse_document (xdt, data, error);
	if (doc == NULL)
		return FALSE;
	root = xmlDocGetRootElement (doc);

	/* switch to upstream format parsing */
	priv->mode = AS_PARSER_MODE_UPSTREAM;

	ret = TRUE;
	if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_UNEXPECTED_FORMAT_KIND,
				     "Tried to parse distro metadata as upstream metadata.");
		goto out;
	} else if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
		as_xmldata_parse_component_node (xdt, root, cpt, error);
	} else if  (g_strcmp0 ((gchar*) root->name, "application") == 0) {
		g_debug ("Parsing legacy AppStream metadata file.");
		as_xmldata_parse_component_node (xdt, root, cpt, error);
	} else {
		g_set_error_literal (error,
					AS_METADATA_ERROR,
					AS_METADATA_ERROR_FAILED,
					"XML file does not contain valid AppStream data!");
		ret = FALSE;
		goto out;
	}

out:
	xmlFreeDoc (doc);
	return ret;
}

/**
 * as_xmldata_parse_upstream_data:
 * @xdt: An instance of #AsXMLData
 * @data: XML representing upstream metadata.
 * @error: A #GError
 *
 * Parse AppStream upstream metadata.
 *
 * Returns: (transfer full): An #AsComponent, deserialized from the XML.
 */
AsComponent*
as_xmldata_parse_upstream_data (AsXMLData *xdt, const gchar *data, GError **error)
{
	g_autoptr(AsComponent) cpt = NULL;
	gboolean ret;

	cpt = as_component_new ();
	ret = as_xmldata_update_cpt_with_upstream_data (xdt, data, cpt, error);

	if (ret)
		return g_object_ref (cpt);
	else
		return NULL;
}

/**
 * as_xmldata_parse_document:
 */
xmlDoc*
as_xmldata_parse_document (AsXMLData *xdt, const gchar *data, GError **error)
{
	xmlDoc *doc;
	xmlNode *root;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	if (data == NULL) {
		/* empty document means no components */
		return NULL;
	}

	as_xmldata_clear_error (xdt);
	doc = xmlReadMemory (data, strlen (data),
			     NULL,
			     "utf-8",
			     XML_PARSE_NOBLANKS | XML_PARSE_NONET);
	if (doc == NULL) {
		g_set_error (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"Could not parse XML data: %s", priv->last_error_msg);
		return NULL;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "The XML document is empty.");
		xmlFreeDoc (doc);
		return NULL;
	}

	return doc;
}

/**
 * as_xmldata_parse_distro_data:
 * @xdt: An instance of #AsXMLData
 * @data: XML representing distro metadata.
 * @error: A #GError
 *
 * Parse AppStream upstream metadata.
 *
 * Returns: (transfer full) (element-type AsComponent): An array of #AsComponent, deserialized from the XML.
 */
GPtrArray*
as_xmldata_parse_distro_data (AsXMLData *xdt, const gchar *data, GError **error)
{
	xmlDoc *doc;
	xmlNode *root;
	GPtrArray *cpts = NULL;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	doc = as_xmldata_parse_document (xdt, data, error);
	if (doc == NULL)
		return NULL;
	root = xmlDocGetRootElement (doc);

	priv->mode = AS_PARSER_MODE_DISTRO;
	cpts = g_ptr_array_new_with_free_func (g_object_unref);

	if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
		as_xmldata_parse_components_node (xdt, cpts, root, error);
	} else if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
		g_autoptr(AsComponent) cpt = NULL;

		cpt = as_component_new ();
		/* we explicitly allow parsing single component entries in distro-XML mode, since this is a scenario
		 * which might very well happen, e.g. in AppStream metadata generators */
		as_xmldata_parse_component_node (xdt, root, cpt, error);
		g_ptr_array_add (cpts, g_object_ref (cpt));
	} else {
		g_set_error_literal (error,
					AS_METADATA_ERROR,
					AS_METADATA_ERROR_FAILED,
					"XML file does not contain valid AppStream data!");
		goto out;
	}

out:
	xmlFreeDoc (doc);
	return cpts;
}

/**
 * as_xmldata_serialize_to_upstream:
 * @xdt: An instance of #AsXMLData
 * @cpt: The component to serialize.
 *
 * Serialize an #AsComponent to upstream XML.
 *
 * Returns: XML metadata.
 */
gchar*
as_xmldata_serialize_to_upstream (AsXMLData *xdt, AsComponent *cpt)
{
	xmlDoc *doc;
	xmlNode *root;
	gchar *xmlstr = NULL;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	if ((priv->check_valid) && (!as_component_is_valid (cpt))) {
		g_debug ("Can not serialize '%s': Component is invalid.", as_component_get_id (cpt));
		return NULL;
	}
	as_xmldata_clear_error (xdt);

	priv->mode = AS_PARSER_MODE_UPSTREAM;
	doc = xmlNewDoc ((xmlChar*) NULL);

	/* define component root node */
	root = as_xmldata_component_to_node (xdt, cpt);
	if (root == NULL)
		goto out;
	xmlDocSetRootElement (doc, root);

out:
	xmlDocDumpFormatMemoryEnc (doc, (xmlChar**) (&xmlstr), NULL, "utf-8", TRUE);
	xmlFreeDoc (doc);

	return xmlstr;
}

/**
 * as_xmldata_serialize_to_distro_with_rootnode:
 *
 * Returns: Valid distro XML metadata.
 */
static gchar*
as_xmldata_serialize_to_distro_with_rootnode (AsXMLData *xdt, GPtrArray *cpts)
{
	xmlDoc *doc;
	xmlNode *root;
	gchar *xmlstr = NULL;
	guint i;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	/* initialize */
	as_xmldata_clear_error (xdt);
	priv->mode = AS_PARSER_MODE_DISTRO;

	root = xmlNewNode (NULL, (xmlChar*) "components");
	xmlNewProp (root, (xmlChar*) "version", (xmlChar*) "0.8");
	if (priv->origin != NULL)
		xmlNewProp (root, (xmlChar*) "origin", (xmlChar*) priv->origin);
	if (priv->arch != NULL)
		xmlNewProp (root, (xmlChar*) "architecture", (xmlChar*) priv->arch);

	for (i = 0; i < cpts->len; i++) {
		AsComponent *cpt;
		xmlNode *node;
		cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));

		if ((priv->check_valid) && (!as_component_is_valid (cpt))) {
			g_debug ("Can not serialize '%s': Component is invalid.", as_component_get_id (cpt));
			continue;
		}

		node = as_xmldata_component_to_node (xdt, cpt);
		if (node == NULL)
			continue;
		xmlAddChild (root, node);
	}

	doc = xmlNewDoc ((xmlChar*) NULL);
	xmlDocSetRootElement (doc, root);

	xmlDocDumpFormatMemoryEnc (doc, (xmlChar**) (&xmlstr), NULL, "utf-8", TRUE);
	xmlFreeDoc (doc);

	return xmlstr;
}

/**
 * as_xmldata_serialize_to_distro_without_rootnode:
 *
 * Returns: Distro XML metadata slices without rootnode.
 */
static gchar*
as_xmldata_serialize_to_distro_without_rootnode (AsXMLData *xdt, GPtrArray *cpts)
{
	guint i;
	GString *out_data;
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);

	out_data = g_string_new ("");
	priv->mode = AS_PARSER_MODE_DISTRO;
	as_xmldata_clear_error (xdt);

	for (i = 0; i < cpts->len; i++) {
		AsComponent *cpt;
		xmlDoc *doc;
		xmlNode *node;
		xmlBufferPtr buf;
		xmlSaveCtxtPtr sctx;
		cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));

		node = as_xmldata_component_to_node (xdt, cpt);
		if (node == NULL)
			continue;

		doc = xmlNewDoc ((xmlChar*) NULL);
		xmlDocSetRootElement (doc, node);

		buf = xmlBufferCreate ();
		sctx = xmlSaveToBuffer (buf, "utf-8", XML_SAVE_FORMAT | XML_SAVE_NO_DECL);
		xmlSaveDoc (sctx, doc);
		xmlSaveClose (sctx);

		g_string_append (out_data, (const gchar*) xmlBufferContent (buf));
		xmlBufferFree (buf);
		xmlFreeDoc (doc);
	}

	return g_string_free (out_data, FALSE);
}

/**
 * as_xmldata_serialize_to_distro:
 * @xdt: An instance of #AsXMLData
 * @cpt: The component to serialize.
 *
 * Serialize an #AsComponent to distro XML.
 *
 * Returns: XML metadata.
 */
gchar*
as_xmldata_serialize_to_distro (AsXMLData *xdt, GPtrArray *cpts, gboolean write_header)
{
	if (cpts->len == 0)
		return NULL;

	if (write_header)
		return as_xmldata_serialize_to_distro_with_rootnode (xdt, cpts);
	else
		return as_xmldata_serialize_to_distro_without_rootnode (xdt, cpts);
}

/**
 * as_xmldata_get_parser_mode:
 */
AsParserMode
as_xmldata_get_parser_mode (AsXMLData *xdt)
{
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);
	return priv->mode;
}

/**
 * as_xmldata_set_parser_mode:
 */
void
as_xmldata_set_parser_mode (AsXMLData *xdt, AsParserMode mode)
{
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);
	priv->mode = mode;
}

/**
 * as_xmldata_set_check_valid:
 * @check: %TRUE if check should be enabled.
 *
 * Set whether we should perform basic validity checks on the component
 * before serializing it to XML.
 */
void
as_xmldata_set_check_valid (AsXMLData *xdt, gboolean check)
{
	AsXMLDataPrivate *priv = GET_PRIVATE (xdt);
	priv->check_valid = check;
}

/**
 * as_xmldata_class_init:
 **/
static void
as_xmldata_class_init (AsXMLDataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_xmldata_finalize;
}

/**
 * as_xmldata_new:
 */
AsXMLData*
as_xmldata_new (void)
{
	AsXMLData *xdt;
	xdt = g_object_new (AS_TYPE_XMLDATA, NULL);
	return AS_XMLDATA (xdt);
}
