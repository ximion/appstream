/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2017 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-xml.h"

#include <string.h>
#include "as-utils.h"
#include "as-utils-private.h"

/**
 * SECTION:as-xml
 * @short_description: Helper functions to parse AppStream XML data
 * @include: appstream.h
 */

/**
 * as_xml_get_node_value:
 */
gchar*
as_xml_get_node_value (xmlNode *node)
{
	gchar *content;
	content = (gchar*) xmlNodeGetContent (node);
	if (content != NULL)
		g_strstrip (content);

	return content;
}

/**
 * as_xmldata_get_node_locale:
 * @node: A XML node
 *
 * Returns: The locale of a node, if the node should be considered for inclusion.
 * %NULL if the node should be ignored due to a not-matching locale.
 */
gchar*
as_xmldata_get_node_locale (AsContext *ctx, xmlNode *node)
{
	gchar *lang;

	lang = (gchar*) xmlGetProp (node, (xmlChar*) "lang");

	if (lang == NULL) {
		lang = g_strdup ("C");
		goto out;
	}

	if (as_context_get_all_locale_enabled (ctx)) {
		/* we should read all languages */
		goto out;
	}

	if (as_utils_locale_is_compatible (as_context_get_locale (ctx), lang)) {
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

/**
 * as_xml_dump_node_children:
 */
gchar*
as_xml_dump_node_children (xmlNode *node)
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
 * as_xml_add_children_values_to_array:
 */
void
as_xml_add_children_values_to_array (xmlNode *node, const gchar *element_name, GPtrArray *array)
{
	xmlNode *iter;

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE) {
			continue;
		}
		if (g_strcmp0 ((const gchar*) iter->name, element_name) == 0) {
			gchar *content = as_xml_get_node_value (iter);
			/* transfer ownership of content to array */
			if (content != NULL)
				g_ptr_array_add (array, content);
		}
	}
}

/**
 * as_xml_get_children_as_string_list:
 */
GPtrArray*
as_xml_get_children_as_string_list (xmlNode *node, const gchar *element_name)
{
	GPtrArray *list;

	list = g_ptr_array_new_with_free_func (g_free);
	as_xml_add_children_values_to_array (node,
					     element_name,
					     list);
	return list;
}

/**
 * as_xml_parse_metainfo_description_node:
 */
void
as_xml_parse_metainfo_description_node (AsContext *ctx, xmlNode *node, GHFunc func, gpointer entity)
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

			lang = as_xmldata_get_node_locale (ctx, iter);
			if (lang == NULL)
				/* this locale is not for us */
				continue;

			str = g_hash_table_lookup (desc, lang);
			if (str == NULL) {
				str = g_string_new ("");
				g_hash_table_insert (desc, g_strdup (lang), str);
			}

			tmp = as_xml_get_node_value (iter);
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

				lang = as_xmldata_get_node_locale (ctx, iter2);
				if (lang == NULL)
					continue;

				/* if the language is new, we start it with an enum */
				str = g_hash_table_lookup (desc, lang);
				if (str == NULL) {
					str = g_string_new ("");
					g_string_append_printf (str, "<%s>\n", node_name);
					g_hash_table_insert (desc, g_strdup (lang), str);
				}

				tmp = as_xml_get_node_value (iter2);
				content = g_markup_escape_text (tmp, -1);
				g_string_append_printf (str, "  <%s>%s</%s>\n", (gchar*) iter2->name, content, (gchar*) iter2->name);
			}

			/* we might have updated the list by adding new locales, so fetch it again */
			g_list_free (vlist);
			vlist = g_hash_table_get_values (desc);

			/* close listing tags */
			for (l = vlist; l != NULL; l = l->next) {
				g_string_append_printf (l->data, "</%s>\n", node_name);
			}
		}
	}

	g_hash_table_foreach (desc, func, entity);
	g_hash_table_unref (desc);
}

/**
 * as_xml_add_description_node_helper:
 *
 * Add the description markup to the XML tree
 */
static gboolean
as_xml_add_description_node_helper (AsContext *ctx, xmlNode *root, xmlNode **desc_node, const gchar *description_markup, const gchar *lang)
{
	g_autofree gchar *xmldata = NULL;
	xmlDoc *doc;
	xmlNode *droot;
	xmlNode *dnode;
	xmlNode *iter;
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

	if (as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) {
		if (*desc_node == NULL)
			*desc_node = xmlNewChild (root, NULL, (xmlChar*) "description", NULL);
		dnode = *desc_node;
	} else {
		/* in collection-data parser mode, we might have multiple <description/> tags */
		dnode = xmlNewChild (root, NULL, (xmlChar*) "description", NULL);
	}

	localized = g_strcmp0 (lang, "C") != 0;
	if (as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) {
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

			if ((as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) && (localized)) {
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

				if ((as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) && (localized)) {
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
 * as_xml_add_description_node:
 *
 * Add a description node to the XML document tree.
 */
void
as_xml_add_description_node (AsContext *ctx, xmlNode *root, GHashTable *desc_table)
{
	GHashTableIter iter;
	gpointer key, value;
	xmlNode *desc_node = NULL;

	g_hash_table_iter_init (&iter, desc_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *locale = (const gchar*) key;
		const gchar *desc_markup = (const gchar*) value;

		as_xml_add_description_node_helper (ctx, root, &desc_node, desc_markup, locale);
	}
}
