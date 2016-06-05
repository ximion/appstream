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
#include <string.h>

#include "as-xml-utils.h"
#include "as-utils-private.h"
#include "as-metadata.h"

/**
 * libxml_generic_error:
 *
 * Catch out-of-context errors emitted by libxml2.
 */
static void
libxml_generic_error (gchar *last_error_msg, const char *format, ...)
{
	GString *str;
	va_list arg_ptr;
	static GMutex mutex;

	g_mutex_lock (&mutex);
	str = g_string_new (last_error_msg? last_error_msg : "");

	va_start (arg_ptr, format);
	g_string_append_vprintf (str, format, arg_ptr);
	va_end (arg_ptr);

	g_free (last_error_msg);
	last_error_msg = g_string_free (str, FALSE);
	g_mutex_unlock (&mutex);
}

/**
 * as_xml_clear_error:
 */
static void
as_xml_clear_error (gchar *last_error_msg)
{
	static GMutex mutex;

	g_mutex_lock (&mutex);
	last_error_msg = NULL;
	xmlSetGenericErrorFunc (last_error_msg, (xmlGenericErrorFunc) libxml_generic_error);
	g_mutex_unlock (&mutex);
}

/**
 * as_xml_get_node_value:
 */
gchar*
as_xml_get_node_value (xmlNode *node)
{
	gchar *content;
	content = (gchar*) xmlNodeGetContent (node);

	return content;
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
 * as_xml_get_children_as_strv:
 */
gchar**
as_xml_get_children_as_strv (xmlNode* node, const gchar* element_name)
{
	GPtrArray *list;
	xmlNode *iter;
	gchar **res;
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
 * as_xml_parse_document:
 */
xmlDoc*
as_xml_parse_document (const gchar *data, GError **error)
{
	xmlDoc *doc;
	xmlNode *root;
	gchar* last_error_msg;

	if (data == NULL)
		return NULL;

	as_xml_clear_error (last_error_msg);
	doc = xmlReadMemory (data, strlen (data),
			     NULL,
			     "utf-8",
			     XML_PARSE_NOBLANKS | XML_PARSE_NONET);
	if (doc == NULL) {
		g_set_error (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"Could not parse XML data: %s", last_error_msg);
		g_free (last_error_msg);
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
