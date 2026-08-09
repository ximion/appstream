/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <libxml/xmlversion.h>

#include "as-utils.h"
#include "as-utils-private.h"

/**
 * SECTION:as-xml
 * @short_description: Helper functions to parse AppStream XML data
 * @include: appstream.h
 */

#if !defined(LIBXML_THREAD_ENABLED)
#error "libxml2 needs to be compiled with thread support!"
#endif

/**
 * as_xml_get_node_value:
 */
gchar *
as_xml_get_node_value (const xmlNode *node)
{
	gchar *content = as_xml_get_node_value_raw (node);
	if (content != NULL)
		as_strstripnl (content);

	return content;
}

/**
 * as_xml_get_node_value_refstr:
 *
 * Return the node value as an interned #GRefString.
 *
 * Returns: The #GRefString or %NULL if the value did not exist.
 */
GRefString *
as_xml_get_node_value_refstr (const xmlNode *node)
{
	g_autofree gchar *content = as_xml_get_node_value_raw (node);
	if (content != NULL)
		as_strstripnl (content);
	if (content == NULL)
		return NULL;
	return g_ref_string_new_intern (content);
}

/**
 * as_xml_get_prop_value_refstr:
 *
 * Return the property value as an interned #GRefString.
 *
 * Returns: The #GRefString or %NULL if the property did not exist.
 */
GRefString *
as_xml_get_prop_value_refstr (const xmlNode *node, const gchar *prop_name)
{
	g_autofree gchar *tmp = as_xml_get_prop_value (node, prop_name);
	if (tmp == NULL)
		return NULL;
	return g_ref_string_new_intern (tmp);
}

/**
 * as_xml_get_prop_value_as_int:
 *
 * Gets a XML node property, e.g. 34
 *
 * Return value: integer value, or %G_MAXINT for error
 **/
gint
as_xml_get_prop_value_as_int (const xmlNode *node, const gchar *prop_name)
{
	g_autofree gchar *tmp = NULL;
	gchar *endptr = NULL;
	gint64 value_tmp;

	tmp = as_xml_get_prop_value (node, prop_name);
	if (tmp == NULL)
		return G_MAXINT;
	value_tmp = g_ascii_strtoll (tmp, &endptr, 10);
	if (value_tmp == 0 && tmp == endptr)
		return G_MAXINT;
	if (value_tmp > G_MAXINT || value_tmp < G_MININT)
		return G_MAXINT;
	return (gint) value_tmp;
}

/**
 * as_xml_get_node_locale:
 * @node: a XML node
 *
 * Returns: The locale of a node, "C" if untranslated. Free result with %g_free
 */
gchar *
as_xml_get_node_locale (AsContext *ctx, xmlNode *node)
{
	gchar *lang = (gchar *) xmlGetProp (node, (xmlChar *) "lang");
	if (lang == NULL)
		lang = g_strdup ("C");

	return lang;
}

/**
 * as_xml_get_node_locale_match:
 * @node: A XML node
 *
 * Returns: The locale of a node, if the node should be considered for inclusion.
 * %NULL if the node should be ignored due to a not-matching locale.
 */
gchar *
as_xml_get_node_locale_match (AsContext *ctx, xmlNode *node)
{
	gchar *lang;

	lang = (gchar *) xmlGetProp (node, (xmlChar *) "lang");

	if (lang == NULL) {
		lang = g_strdup ("C");
		goto out;
	}

	if (as_context_get_locale_use_all (ctx)) {
		/* we should read all languages */
		goto out;
	}

	if (as_utils_locale_is_compatible (as_context_get_locale (ctx), lang)) {
		goto out;
	}

	/* If we are here, we haven't found a matching locale.
	 * In that case, we return %NULL to indicate that this element should not be added.
	 */
	g_free (g_steal_pointer (&lang));

out:
	return lang;
}

/**
 * as_xml_append_escaped:
 *
 * Append text to a string, escaping all characters that must not appear
 * verbatim in XML character data.
 */
static void
as_xml_append_escaped (GString *str, const gchar *text)
{
	const gchar *p;
	const gchar *chunk_start;

	if (text == NULL)
		return;

	chunk_start = text;
	for (p = text; *p != '\0'; p++) {
		const gchar *rep;

		switch (*p) {
		case '&':
			rep = "&amp;";
			break;
		case '<':
			rep = "&lt;";
			break;
		case '>':
			rep = "&gt;";
			break;
		case '\r':
			rep = "&#13;";
			break;
		default:
			continue;
		}

		if (p > chunk_start)
			g_string_append_len (str, chunk_start, p - chunk_start);
		g_string_append (str, rep);
		chunk_start = p + 1;
	}

	if (p > chunk_start)
		g_string_append_len (str, chunk_start, p - chunk_start);
}

/**
 * as_xml_desc_is_inline_tag:
 * @name: The element name.
 * @len: Length of @name.
 *
 * Check whether the given element name is permitted as inline markup
 * inside of a description paragraph or list item.
 */
static gboolean
as_xml_desc_is_inline_tag (const gchar *name, gssize len)
{
	if (len < 0)
		len = (gssize) strlen (name);

	if (len == 2)
		return memcmp (name, "em", 2) == 0;
	if (len == 4)
		return memcmp (name, "code", 4) == 0;
	return FALSE;
}

/**
 * as_xml_desc_is_block_tag:
 */
static gboolean
as_xml_desc_is_block_tag (const gchar *name, gssize len)
{
	if (len < 0)
		len = (gssize) strlen (name);

	switch (len) {
	case 1:
		return name[0] == 'p';
	case 2:
		return (memcmp (name, "ul", 2) == 0) || (memcmp (name, "ol", 2) == 0) ||
		       (memcmp (name, "li", 2) == 0);
	default:
		return FALSE;
	}
}

/**
 * as_xml_desc_append_inline_content:
 *
 * Serialize the content of a description paragraph or list item, permitting
 * only markup that is valid in AppStream descriptions. Any other element is
 * replaced by its (escaped) text content.
 */
static void
as_xml_desc_append_inline_content (GString *str, xmlNode *node, guint depth)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type == XML_TEXT_NODE || iter->type == XML_CDATA_SECTION_NODE) {
			as_xml_append_escaped (str, (const gchar *) iter->content);
			continue;
		}

		if (iter->type != XML_ELEMENT_NODE && iter->type != XML_ENTITY_REF_NODE)
			continue;

		if (G_UNLIKELY (iter->type == XML_ENTITY_REF_NODE ||
				depth >= AS_DESCRIPTION_MARKUP_MAX_DEPTH)) {
			/* resolve entity references, and refuse to descend any deeper into
			 * excessively nested markup - in both cases we just take the text */
			g_autofree gchar *content = as_xml_get_node_value_raw (iter);
			as_xml_append_escaped (str, content);
			continue;
		}

		if (as_xml_desc_is_inline_tag ((const gchar *) iter->name, -1)) {
			/* the element is permitted, but none of its attributes ever are */
			g_string_append_printf (str, "<%s>", (const gchar *) iter->name);
			as_xml_desc_append_inline_content (str, iter, depth + 1);
			g_string_append_printf (str, "</%s>", (const gchar *) iter->name);
		} else {
			/* flatten invalid markup to its text content */
			as_xml_desc_append_inline_content (str, iter, depth + 1);
		}
	}
}

/**
 * as_xml_desc_append_block_node:
 *
 * Serialize a description block element (paragraph or enumeration) and its
 * contents. Invalid block elements are dropped.
 */
static void
as_xml_desc_append_block_node (GString *str, xmlNode *node)
{
	const gchar *node_name = (const gchar *) node->name;

	if (as_str_equal0 (node_name, "p")) {
		g_string_append (str, "<p>");
		as_xml_desc_append_inline_content (str, node, 1);
		g_string_append (str, "</p>");
		return;
	}

	if (as_str_equal0 (node_name, "ul") || as_str_equal0 (node_name, "ol")) {
		g_string_append_printf (str, "<%s>", node_name);
		for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
			if (iter->type != XML_ELEMENT_NODE)
				continue;
			/* only list items are permitted in enumerations */
			if (!as_str_equal0 ((const gchar *) iter->name, "li"))
				continue;

			g_string_append (str, "<li>");
			as_xml_desc_append_inline_content (str, iter, 1);
			g_string_append (str, "</li>");
		}
		g_string_append_printf (str, "</%s>", node_name);
	}

	/* any other element is not valid description markup and dropped entirely */
}

/**
 * as_xml_dump_description_para_content:
 *
 * Dump the sanitized content of a description paragraph or list item,
 * without its enclosing tag.
 *
 * Returns: The markup, or %NULL if the node had no usable content.
 */
gchar *
as_xml_dump_description_para_content (xmlNode *node)
{
	g_autoptr(GString) str = NULL;

	/* ignore node if it is a space */
	if (G_UNLIKELY (node->type != XML_ELEMENT_NODE))
		return NULL;

	str = g_string_new ("");
	as_xml_desc_append_inline_content (str, node, 1);
	if (str->len == 0)
		return NULL;

	return g_string_free (g_steal_pointer (&str), FALSE);
}

/**
 * as_xml_desc_inline_content_is_valid:
 *
 * Check whether the content of a description paragraph or list item is already
 * exactly what %as_xml_desc_append_inline_content would emit for it.
 */
static gboolean
as_xml_desc_inline_content_is_valid (xmlNode *node, guint depth)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type == XML_TEXT_NODE)
			continue;

		/* CDATA sections, entity references, comments and processing instructions
		 * are all rewritten or dropped when the markup is serialized */
		if (iter->type != XML_ELEMENT_NODE)
			return FALSE;

		if (!as_xml_desc_is_inline_tag ((const gchar *) iter->name, -1))
			return FALSE;
		/* no attributes are permitted in description markup */
		if (iter->properties != NULL)
			return FALSE;
		/* deeper markup would have been flattened */
		if (depth >= AS_DESCRIPTION_MARKUP_MAX_DEPTH)
			return FALSE;

		if (!as_xml_desc_inline_content_is_valid (iter, depth + 1))
			return FALSE;
	}

	return TRUE;
}

/**
 * as_xml_desc_tree_is_valid:
 * @root: The node holding the description markup.
 *
 * Check whether the given description markup tree contains only valid markup.
 * Node types other than elements are ignored here, as those are never written
 * out anyway.
 *
 * Returns: %TRUE if the tree can be used as-is.
 */
static gboolean
as_xml_desc_tree_is_valid (xmlNode *root)
{
	for (xmlNode *iter = root->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;

		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (iter->properties != NULL)
			return FALSE;

		node_name = (const gchar *) iter->name;
		if (as_str_equal0 (node_name, "p")) {
			if (!as_xml_desc_inline_content_is_valid (iter, 1))
				return FALSE;
			continue;
		}

		if (as_str_equal0 (node_name, "ul") || as_str_equal0 (node_name, "ol")) {
			for (xmlNode *iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (iter2->type != XML_ELEMENT_NODE)
					continue;
				/* only list items are permitted in enumerations */
				if (!as_str_equal0 ((const gchar *) iter2->name, "li"))
					return FALSE;
				if (iter2->properties != NULL)
					return FALSE;
				if (!as_xml_desc_inline_content_is_valid (iter2, 1))
					return FALSE;
			}
			continue;
		}

		/* not a valid description block element */
		return FALSE;
	}

	return TRUE;
}

/**
 * as_xml_dump_description_children:
 *
 * Dump the sanitized children of a `description` node, dropping any markup
 * that is not permitted in component descriptions.
 */
gchar *
as_xml_dump_description_children (xmlNode *node)
{
	GString *str = g_string_new ("");

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		gsize prev_len;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		prev_len = str->len;
		if (str->len > 0)
			g_string_append_c (str, '\n');
		as_xml_desc_append_block_node (str, iter);

		/* drop the separator again in case the node was not valid markup */
		if (str->len <= prev_len + 1)
			g_string_truncate (str, prev_len);
	}

	return g_string_free (str, FALSE);
}

/**
 * as_xml_desc_is_predefined_entity:
 *
 * Check whether @name (of length @len) is one of the five entities that XML
 * predefines, and which therefore need no entity declaration.
 */
static gboolean
as_xml_desc_is_predefined_entity (const gchar *name, gsize len)
{
	switch (len) {
	case 2:
		return (memcmp (name, "lt", 2) == 0) || (memcmp (name, "gt", 2) == 0);
	case 3:
		return memcmp (name, "amp", 3) == 0;
	case 4:
		return (memcmp (name, "quot", 4) == 0) || (memcmp (name, "apos", 4) == 0);
	default:
		return FALSE;
	}
}

/**
 * as_xml_desc_markup_is_valid:
 * @markup: The description markup to check.
 * @len: Length of @markup, or -1 if it is %NULL-terminated.
 *
 * Quickly check whether a string only consists of markup that is permitted
 * in component descriptions. This check deliberately cheap and does a string-scan
 * only, so anything unusual is rejected and has to be dealt with by actually parsing
 * the data (which is roughly 25 times as expensive).
 *
 * Returns: %TRUE if the markup can be used verbatim.
 */
static gboolean
as_xml_desc_markup_is_valid (const gchar *markup, gssize len)
{
	const gchar *end;
	guint inline_depth = 0;

	if (len < 0)
		len = (gssize) strlen (markup);
	end = markup + len;

	for (const gchar *p = markup; p < end; p++) {
		const gchar *name_start;
		gsize name_len;
		gboolean is_end_tag = FALSE;

		if (*p == '&') {
			/* only the predefined entities and character references are permitted */
			const gchar *ref = p + 1;
			if (ref < end && *ref == '#') {
				ref++;
				if (ref < end && (*ref == 'x' || *ref == 'X'))
					ref++;
				while (ref < end && g_ascii_isalnum (*ref))
					ref++;
			} else {
				while (ref < end && g_ascii_isalpha (*ref))
					ref++;
				if (!as_xml_desc_is_predefined_entity (p + 1, ref - p - 1))
					return FALSE;
			}
			if (ref >= end || *ref != ';' || ref == p + 1)
				return FALSE;
			p = ref;
			continue;
		}

		if (*p != '<')
			continue;

		p++;
		if (p < end && *p == '/') {
			is_end_tag = TRUE;
			p++;
		}
		name_start = p;
		while (p < end && g_ascii_islower (*p))
			p++;

		/* anything that isn't a plain start/end tag - an attribute, a comment,
		 * a processing instruction, a CDATA section - is rejected here */
		if (p >= end || *p != '>')
			return FALSE;
		name_len = p - name_start;
		if (!as_xml_desc_is_block_tag (name_start, name_len) &&
		    !as_xml_desc_is_inline_tag (name_start, name_len))
			return FALSE;

		/* keep track of how deeply the inline markup is nested, so we never
		 * pass through markup that the parser would have flattened */
		if (as_xml_desc_is_inline_tag (name_start, name_len)) {
			if (is_end_tag) {
				if (inline_depth > 0)
					inline_depth--;
			} else {
				if (++inline_depth >= AS_DESCRIPTION_MARKUP_MAX_DEPTH)
					return FALSE;
			}
		} else {
			/* a block-level element starts a new inline markup context */
			inline_depth = 0;
		}
	}

	return TRUE;
}

/**
 * as_xml_sanitize_description:
 * @markup: the XML description markup to sanitize.
 * @len: Length of @markup, or -1 if length is unknown and @markup is NULL-terminated.
 *
 * Remove any markup that is not permitted in AppStream description tags from
 * the given string. Invalid elements are replaced by their text content, all
 * attributes as well as any comments, processing instructions and CDATA
 * sections are dropped.
 *
 * This is used for markup that we can not validate while reading it, because we
 * only ever see the finished string (DEP-11 YAML data, or values set via the API).
 *
 * Returns: a newly allocated string, or %NULL if @markup was %NULL.
 */
gchar *
as_xml_sanitize_description (const gchar *markup, gssize len)
{
	g_autoptr(GString) xmldata = NULL;
	xmlDoc *doc;
	xmlNode *root;
	gchar *res;

	if (markup == NULL)
		return NULL;
	if (len < 0)
		len = (gssize) strlen (markup);

	/* fast path: the markup is already valid, so we can use it as-is */
	if (G_LIKELY (as_xml_desc_markup_is_valid (markup, len)))
		return g_strndup (markup, len);

	/* make XML parser happy by providing a root element */
	xmldata = g_string_sized_new (len + 14);
	g_string_append (xmldata, "<root>");
	g_string_append_len (xmldata, markup, len);
	g_string_append (xmldata, "</root>");

	doc = as_xml_parse_document (xmldata->str, xmldata->len, FALSE, NULL);
	if (doc == NULL) {
		/* the data was not well-formed XML at all, so we can only escape it */
		return g_markup_escape_text (markup, len);
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL) {
		xmlFreeDoc (doc);
		return g_strdup ("");
	}

	res = as_xml_dump_description_children (root);
	xmlFreeDoc (doc);

	return res;
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
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((const gchar *) iter->name, element_name) == 0) {
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
GPtrArray *
as_xml_get_children_as_string_list (xmlNode *node, const gchar *element_name)
{
	GPtrArray *list;

	list = g_ptr_array_new_with_free_func (g_free);
	as_xml_add_children_values_to_array (node, element_name, list);
	return list;
}

/**
 * as_xml_get_children_as_strv:
 */
gchar **
as_xml_get_children_as_strv (xmlNode *node, const gchar *element_name)
{
	g_autoptr(GPtrArray) list = NULL;
	gchar **res;

	list = as_xml_get_children_as_string_list (node, element_name);
	res = as_ptr_array_to_strv (list);
	return res;
}

typedef struct {
	xmlDoc *doc;
	xmlNode *node;
	AsTag tag_id;
	gchar *locale;
	gboolean localized;
	xmlNode *d_node;
} AsXMLMarkupParseHelper;

/**
 * as_xml_markup_parse_helper_new: (skip)
 **/
static AsXMLMarkupParseHelper *
as_xml_markup_parse_helper_new (const gchar *markup, const gchar *locale)
{
	g_autofree gchar *xmldata = NULL;
	AsXMLMarkupParseHelper *helper = g_slice_new0 (AsXMLMarkupParseHelper);
	xmlNode *root;

	helper->locale = g_strdup (locale);

	xmldata = g_strconcat ("<root>", markup, "</root>", NULL);
	helper->doc = xmlReadMemory (xmldata,
				     strlen (xmldata),
				     NULL,
				     "utf-8",
				     XML_PARSE_NOBLANKS | XML_PARSE_NONET);
	if (helper->doc == NULL)
		goto fail;

	/* The markup may have been set via the API, or read from a format where we can not
	 * validate it while reading (like DEP-11 YAML), so we ensure that we never write
	 * anything that isn't valid description markup. We check if the tree is valid,
	 * and use it verbatim (valid data is the overwhelmingly common case). */
	root = xmlDocGetRootElement (helper->doc);
	if (G_UNLIKELY (root != NULL && !as_xml_desc_tree_is_valid (root))) {
		g_autofree gchar *safe_markup = as_xml_dump_description_children (root);
		g_autofree gchar *safe_xmldata = g_strconcat ("<root>",
							      safe_markup,
							      "</root>",
							      NULL);

		xmlFreeDoc (helper->doc);
		helper->doc = xmlReadMemory (safe_xmldata,
					     strlen (safe_xmldata),
					     NULL,
					     "utf-8",
					     XML_PARSE_NOBLANKS | XML_PARSE_NONET);
		if (helper->doc == NULL)
			goto fail;
		root = xmlDocGetRootElement (helper->doc);
	}

	helper->d_node = NULL;
	helper->node = root;
	if (helper->node != NULL)
		helper->node = helper->node->children;
	if (helper->node != NULL)
		helper->tag_id = as_xml_tag_from_string ((const gchar *) helper->node->name);

	helper->localized = (locale != NULL) && (g_strcmp0 (locale, "C") != 0);

	return helper;

fail:
	if (helper->doc != NULL)
		xmlFreeDoc (helper->doc);
	g_free (helper->locale);
	g_slice_free (AsXMLMarkupParseHelper, helper);
	return NULL;
}

/**
 * as_xml_markup_parse_helper_free: (skip)
 **/
static void
as_xml_markup_parse_helper_free (AsXMLMarkupParseHelper *helper)
{
	if (helper->doc != NULL)
		xmlFreeDoc (helper->doc);
	g_free (helper->locale);
	g_slice_free (AsXMLMarkupParseHelper, helper);
}
G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsXMLMarkupParseHelper, as_xml_markup_parse_helper_free)

/**
 * as_xml_markup_parse_helper_next: (skip)
 *
 * Advance to the next node.
 **/
static gboolean
as_xml_markup_parse_helper_next (AsXMLMarkupParseHelper *helper)
{
	if (helper->node == NULL)
		return FALSE;

	/* if we have a listing, jump into it */
	if ((helper->tag_id == AS_TAG_UL) || (helper->tag_id == AS_TAG_OL)) {
		helper->d_node = helper->node;
		helper->node = helper->node->children;
	} else {
		do {
			helper->node = helper->node->next;
		} while ((helper->node != NULL) && (helper->node->type != XML_ELEMENT_NODE));
	}

	if (helper->node == NULL) {
		if (helper->d_node != NULL) {
			helper->node = helper->d_node;
			helper->d_node = NULL;
			helper->node = helper->node->next;
		}
	}
	if (helper->node == NULL) {
		helper->tag_id = AS_TAG_UNKNOWN;
		return FALSE;
	}

	helper->tag_id = as_xml_tag_from_string ((const gchar *) helper->node->name);

	return TRUE;
}

/**
 * as_xml_markup_parse_helper_export_node: (skip)
 **/
static xmlNode *
as_xml_markup_parse_helper_export_node (AsXMLMarkupParseHelper *helper,
					xmlNode *parent,
					gboolean localized)
{
	if ((helper->tag_id == AS_TAG_P) || (helper->tag_id == AS_TAG_LI)) {
		/* add node and subnodes */
		xmlNode *cn = xmlAddChild (parent, xmlCopyNode (helper->node, TRUE));
		if (helper->localized && localized) {
			xmlNewProp (cn, (xmlChar *) "xml:lang", (xmlChar *) helper->locale);
		}

		return cn;
	}

	if ((helper->tag_id == AS_TAG_UL) || (helper->tag_id == AS_TAG_OL)) {
		return xmlNewChild (parent, NULL, helper->node->name, NULL);
	}

	return NULL;
}

typedef struct {
	guint elem_count;
	GString *data;
} AsXMLMetaInfoDescParseHelper;

/**
 * as_xml_metainfo_desc_parse_helper_new: (skip)
 **/
static AsXMLMetaInfoDescParseHelper *
as_xml_metainfo_desc_parse_helper_new (void)
{
	AsXMLMetaInfoDescParseHelper *helper = g_slice_new0 (AsXMLMetaInfoDescParseHelper);
	helper->data = g_string_new ("");
	helper->elem_count = 0;
	return helper;
}

/**
 * as_xml_metainfo_desc_parse_helper_free: (skip)
 **/
static gchar *
as_xml_metainfo_desc_parse_helper_free (AsXMLMetaInfoDescParseHelper *helper)
{
	gchar *data = g_string_free (helper->data, FALSE);
	g_slice_free (AsXMLMetaInfoDescParseHelper, helper);
	return data;
}

/**
 * as_xml_parse_metainfo_description_node:
 */
void
as_xml_parse_metainfo_description_node (AsContext *ctx, xmlNode *node, GHashTable *l10n_desc)
{
	g_autoptr(GHashTable) tmp_desc = NULL;
	GHashTableIter res_iter;
	gpointer res_value;
	gpointer res_key;
	AsXMLMetaInfoDescParseHelper *phelper;
	guint untranslated_elem_count = 0;

	tmp_desc = g_hash_table_new_full (g_str_hash,
					  g_str_equal,
					  (GDestroyNotify) g_ref_string_release,
					  NULL);
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		AsTag tag_id;
		const gchar *node_name = (const gchar *) iter->name;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		tag_id = as_xml_tag_from_string (node_name);

		if (tag_id == AS_TAG_P) {
			g_autofree gchar *lang = NULL;
			g_autofree gchar *content = NULL;

			lang = as_xml_get_node_locale_match (ctx, iter);
			if (lang == NULL)
				/* this locale is not for us */
				continue;

			phelper = g_hash_table_lookup (tmp_desc, lang);
			if (phelper == NULL) {
				phelper = as_xml_metainfo_desc_parse_helper_new ();
				g_hash_table_insert (tmp_desc,
						     g_ref_string_new_intern (lang),
						     phelper);
			}

			content = as_xml_dump_description_para_content (iter);
			if (content != NULL) {
				g_string_append_printf (phelper->data, "<p>%s</p>\n", content);
				phelper->elem_count += 1;
			}

		} else if ((tag_id == AS_TAG_UL) || (tag_id == AS_TAG_OL)) {
			GHashTableIter htiter;
			gpointer hvalue;
			xmlNode *iter2;

			/* append listing node tag to every locale string */
			g_hash_table_iter_init (&htiter, tmp_desc);
			while (g_hash_table_iter_next (&htiter, NULL, &hvalue)) {
				GString *hstr = ((AsXMLMetaInfoDescParseHelper *) hvalue)->data;
				g_string_append_printf (hstr, "<%s>\n", node_name);
			}

			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				g_autofree gchar *lang = NULL;
				g_autofree gchar *content = NULL;
				AsTag iter2_tag_id = as_xml_tag_from_string (
				    (const gchar *) iter2->name);

				if (iter2->type != XML_ELEMENT_NODE)
					continue;
				if (iter2_tag_id != AS_TAG_LI)
					continue;

				lang = as_xml_get_node_locale_match (ctx, iter2);
				if (lang == NULL)
					continue;

				/* if the language is new, we add a listing tag first */
				phelper = g_hash_table_lookup (tmp_desc, lang);
				if (phelper == NULL) {
					phelper = as_xml_metainfo_desc_parse_helper_new ();
					g_string_append_printf (phelper->data, "<%s>\n", node_name);
					g_hash_table_insert (tmp_desc,
							     g_ref_string_new_intern (lang),
							     phelper);
				}

				content = as_xml_dump_description_para_content (iter2);
				if (content != NULL) {
					g_string_append_printf (phelper->data,
								"  <%s>%s</%s>\n",
								(gchar *) iter2->name,
								content,
								(gchar *) iter2->name);
					phelper->elem_count += 1;
				}
			}

			/* close listing tags */
			g_hash_table_iter_init (&htiter, tmp_desc);
			while (g_hash_table_iter_next (&htiter, NULL, &hvalue)) {
				GString *hstr = ((AsXMLMetaInfoDescParseHelper *) hvalue)->data;
				g_string_append_printf (hstr, "</%s>\n", node_name);
			}
		}
	}

	phelper = g_hash_table_lookup (tmp_desc, "C");
	if (phelper != NULL)
		untranslated_elem_count = phelper->elem_count;

	/* finalize the data */
	g_hash_table_iter_init (&res_iter, tmp_desc);
	while (g_hash_table_iter_next (&res_iter, &res_key, &res_value)) {
		g_autofree gchar *text = NULL;
		guint elem_count;
		phelper = (AsXMLMetaInfoDescParseHelper *) res_value;

		elem_count = phelper->elem_count;
		text = as_xml_metainfo_desc_parse_helper_free (phelper);

		/* we require at the very least either more than 3 elements of the description to be translated or
		 * all of the elements if there are less than 3 elements to accept a translation.
		 * See https://github.com/ximion/appstream/issues/293 for more information on the kind of issue that
		 * caused this workaround. */
		if (elem_count < 3) {
			if (elem_count < untranslated_elem_count)
				continue;
		}

		g_hash_table_insert (l10n_desc,
				     g_ref_string_acquire (res_key),
				     g_steal_pointer (&text));
	}
}

/**
 * as_xml_add_description_catalog_mode_helper:
 *
 * Add the description markup for AppStream catalog XML to the tree.
 */
static gboolean
as_xml_add_description_catalog_mode_helper (xmlNode *parent,
					    const gchar *description_markup,
					    const gchar *lang)
{
	xmlNode *dnode;
	xmlNode *cnode;
	g_autoptr(AsXMLMarkupParseHelper) helper = NULL;

	if (as_is_empty (description_markup))
		return FALSE;

	/* skip cruft */
	if (as_is_cruft_locale (lang))
		return FALSE;

	helper = as_xml_markup_parse_helper_new (description_markup, lang);
	if (helper == NULL)
		return FALSE;

	dnode = xmlNewChild (parent, NULL, (xmlChar *) "description", NULL);
	if (helper->localized) {
		xmlNewProp (dnode, (xmlChar *) "xml:lang", (xmlChar *) lang);
	}
	cnode = dnode;

	if (helper->node == NULL)
		return FALSE;

	do {
		if ((helper->tag_id == AS_TAG_UL) || (helper->tag_id == AS_TAG_OL)) {
			cnode = as_xml_markup_parse_helper_export_node (helper, dnode, FALSE);
		} else {
			if (helper->tag_id != AS_TAG_LI)
				cnode = dnode;

			as_xml_markup_parse_helper_export_node (helper, cnode, FALSE);
		}
	} while (as_xml_markup_parse_helper_next (helper));

	return TRUE;
}

/**
 * as_xml_add_description_node:
 *
 * Add a description node to the XML document tree, allowing to mark
 * MetaInfo description blocks as untranslatable.
 */
void
as_xml_add_description_node (AsContext *ctx,
			     xmlNode *root,
			     GHashTable *desc_table,
			     gboolean mi_translatable)
{
	g_autoptr(GList) keys = NULL;
	keys = g_hash_table_get_keys (desc_table);
	keys = g_list_sort (keys, (GCompareFunc) g_ascii_strcasecmp);

	if (as_context_get_style (ctx) == AS_FORMAT_STYLE_METAINFO) {
		/* for metainfo files, we try to interleave translated and untranslated lines, just like in the original files.
		 * Of course this is imperfect and fails as soon as some lines are not translated, but for a fully translated
		 * file this works well enough */
		AsXMLMarkupParseHelper *c_helper;
		xmlNode *dnode = NULL;
		xmlNode *cnode = NULL;
		g_autoptr(GPtrArray) markup_nodes = g_ptr_array_new_with_free_func (
		    (GDestroyNotify) as_xml_markup_parse_helper_free);

		for (GList *link = keys; link != NULL; link = link->next) {
			const gchar *locale = (const gchar *) link->data;
			const gchar *desc_markup = g_hash_table_lookup (desc_table, locale);
			AsXMLMarkupParseHelper *helper;

			if (as_is_cruft_locale (locale))
				continue;

			helper = as_xml_markup_parse_helper_new (desc_markup, locale);
			if (helper == NULL)
				continue;

			/* unlocalized entries should always be sorted first */
			if (helper->localized)
				g_ptr_array_add (markup_nodes, helper);
			else
				g_ptr_array_insert (markup_nodes, 0, helper);
		}

		/* check if there is something to do */
		if (markup_nodes->len <= 0)
			return;

		/* the first helper in our list is always for the unlocalized entries, unless we have none of these,
		 * in which case we just take the first localization */
		c_helper = (AsXMLMarkupParseHelper *) g_ptr_array_index (markup_nodes, 0);

		dnode = xmlNewChild (root, NULL, (xmlChar *) "description", NULL);
		if (!mi_translatable)
			as_xml_add_text_prop (dnode, "translate", "no");

		cnode = dnode;
		do {
			if ((c_helper->tag_id == AS_TAG_UL) || (c_helper->tag_id == AS_TAG_OL)) {
				cnode = as_xml_markup_parse_helper_export_node (c_helper,
										dnode,
										TRUE);
			} else {
				if (c_helper->tag_id != AS_TAG_LI)
					cnode = dnode;

				as_xml_markup_parse_helper_export_node (c_helper, cnode, TRUE);
			}

			for (guint i = 1; i < markup_nodes->len; ++i) {
				AsXMLMarkupParseHelper *helper = g_ptr_array_index (markup_nodes,
										    i);
				if (helper->node == NULL)
					continue;
				if (c_helper->tag_id != helper->tag_id)
					continue;
				if ((helper->tag_id != AS_TAG_UL) && (helper->tag_id != AS_TAG_OL))
					as_xml_markup_parse_helper_export_node (helper,
										cnode,
										TRUE);
				as_xml_markup_parse_helper_next (helper);
			}
		} while (as_xml_markup_parse_helper_next (c_helper));

		/* Due to imbalances caused by untranslated tags, we just append all the information that we couldn't match
		 * to the end of the file. This isn't great, but the best we can do here, since the original mapping of
		 * untranslated to translated sections is gone at this point */
		for (guint i = 0; i < markup_nodes->len; ++i) {
			AsXMLMarkupParseHelper *helper = g_ptr_array_index (markup_nodes, i);
			if (helper->node == NULL)
				continue;
			do {
				if ((helper->tag_id == AS_TAG_UL) ||
				    (helper->tag_id == AS_TAG_OL)) {
					cnode = as_xml_markup_parse_helper_export_node (helper,
											dnode,
											TRUE);
				} else {
					if (helper->tag_id != AS_TAG_LI)
						cnode = dnode;

					as_xml_markup_parse_helper_export_node (helper,
										cnode,
										TRUE);
				}
			} while (as_xml_markup_parse_helper_next (helper));
		}
	} else {
		/* we have a catalog XML file, so write in that format (which is much faster and easier to do) */
		for (GList *link = keys; link != NULL; link = link->next) {
			const gchar *locale = (const gchar *) link->data;
			const gchar *desc_markup = g_hash_table_lookup (desc_table, locale);

			if (as_is_cruft_locale (locale))
				continue;

			as_xml_add_description_catalog_mode_helper (root, desc_markup, locale);
		}
	}
}

/**
 * as_xml_add_description_node_raw:
 *
 * Add a simple description node in verbatim, performing only basic markup
 * validation. The node will not have a language property attached.
 *
 * Returns: The new xmlNode, or %NULL if no node was appended.
 */
xmlNode *
as_xml_add_description_node_raw (xmlNode *root, const gchar *description)
{
	xmlNode *dnode;
	xmlNode *cnode;
	g_autoptr(AsXMLMarkupParseHelper) helper = NULL;

	if (as_is_empty (description))
		return NULL;

	helper = as_xml_markup_parse_helper_new (description, NULL);
	if (helper == NULL)
		return NULL;

	dnode = xmlNewChild (root, NULL, (xmlChar *) "description", NULL);
	if (helper->node == NULL)
		return NULL;
	cnode = dnode;

	do {
		if ((helper->tag_id == AS_TAG_UL) || (helper->tag_id == AS_TAG_OL)) {
			cnode = as_xml_markup_parse_helper_export_node (helper, dnode, FALSE);
		} else {
			if (helper->tag_id != AS_TAG_LI)
				cnode = dnode;

			as_xml_markup_parse_helper_export_node (helper, cnode, FALSE);
		}
	} while (as_xml_markup_parse_helper_next (helper));

	return dnode;
}

/**
 * as_xml_add_localized_text_node:
 *
 * Add set of localized XML nodes based on a localization table.
 */
void
as_xml_add_localized_text_node (xmlNode *root, const gchar *node_name, GHashTable *value_table)
{
	g_autoptr(GList) keys = NULL;

	keys = g_hash_table_get_keys (value_table);
	keys = g_list_sort (keys, (GCompareFunc) g_ascii_strcasecmp);
	for (GList *link = keys; link != NULL; link = link->next) {
		xmlNode *cnode;
		const gchar *locale = (const gchar *) link->data;
		const gchar *str = (const gchar *) g_hash_table_lookup (value_table, locale);

		if (as_is_empty (str))
			continue;

		/* skip cruft */
		if (as_is_cruft_locale (locale))
			continue;

		cnode = xmlNewTextChild (root, NULL, (xmlChar *) node_name, (xmlChar *) str);
		if (g_strcmp0 (locale, "C") != 0) {
			xmlNewProp (cnode, (xmlChar *) "xml:lang", (xmlChar *) locale);
		}
	}
}

/**
 * as_xml_add_node_list_strv:
 *
 * Add node with a list of children containing the strv contents.
 */
xmlNode *
as_xml_add_node_list_strv (xmlNode *root, const gchar *name, const gchar *child_name, gchar **strv)
{
	xmlNode *node;

	/* don't add the node if we have no values */
	if (strv == NULL)
		return NULL;
	if (strv[0] == NULL)
		return NULL;

	if (name == NULL)
		node = root;
	else
		node = xmlNewChild (root, NULL, (xmlChar *) name, NULL);
	for (guint i = 0; strv[i] != NULL; i++) {
		xmlNewTextChild (node, NULL, (xmlChar *) child_name, (xmlChar *) strv[i]);
	}

	return node;
}

/**
 * as_xml_add_node_list:
 *
 * Add node with a list of children containing the string array contents.
 */
xmlNode *
as_xml_add_node_list (xmlNode *root, const gchar *name, const gchar *child_name, GPtrArray *array)
{
	xmlNode *node;

	/* don't add the node if we have no values */
	if (array == NULL)
		return NULL;
	if (array->len == 0)
		return NULL;

	if (name == NULL)
		node = root;
	else
		node = xmlNewChild (root, NULL, (xmlChar *) name, NULL);

	for (guint i = 0; i < array->len; i++) {
		const xmlChar *value = (const xmlChar *) g_ptr_array_index (array, i);
		xmlNewTextChild (node, NULL, (xmlChar *) child_name, value);
	}

	return node;
}

/**
 * as_xml_parse_custom_node:
 *
 * Parse a custom key/value table from XML into a #GHashTable
 * using #GRefString as key/value.
 */
void
as_xml_parse_custom_node (xmlNode *node, GHashTable *custom)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		g_autofree gchar *key_str = NULL;

		if (iter->type != XML_ELEMENT_NODE)
			continue;
		if (g_strcmp0 ((gchar *) iter->name, "value") != 0)
			continue;

		key_str = (gchar *) xmlGetProp (iter, (xmlChar *) "key");
		if (key_str == NULL)
			continue;

		g_hash_table_insert (custom,
				     g_ref_string_new_intern (key_str),
				     as_xml_get_node_value_refstr (iter));
	}
}

/**
 * as_xml_add_custom_node:
 *
 * Add a custom key/value table to the XML DOM.
 * The #GHashTable should use #GRefString as keys/values.
 */
void
as_xml_add_custom_node (xmlNode *root, const gchar *node_name, GHashTable *custom)
{
	xmlNode *node;
	g_autoptr(GList) keys = NULL;

	if (g_hash_table_size (custom) == 0)
		return;

	node = xmlNewChild (root, NULL, (xmlChar *) node_name, NULL);
	keys = g_hash_table_get_keys (custom);
	keys = g_list_sort (keys, (GCompareFunc) g_ascii_strcasecmp);
	for (GList *link = keys; link != NULL; link = link->next) {
		const GRefString *key = (const GRefString *) link->data;

		xmlNode *snode = xmlNewTextChild (node,
						  NULL,
						  (xmlChar *) "value",
						  (xmlChar *) g_hash_table_lookup (custom, key));
		xmlNewProp (snode, (xmlChar *) "key", (xmlChar *) key);
	}
}

/**
 * as_xml_add_text_node:
 * @root: The node to add a child to.
 * @name: The new node name.
 * @value: The new node value.
 *
 * Add node if value is not empty.
 */
xmlNode *
as_xml_add_text_node (xmlNode *root, const gchar *name, const gchar *value)
{
	if (as_is_empty (value))
		return NULL;

	return xmlNewTextChild (root, NULL, (xmlChar *) name, (xmlChar *) value);
}

/**
 * as_xml_add_uint_node:
 * @root: The node to add a child to.
 * @name: The new node name.
 * @value: The new node value.
 *
 * Add node with the given integer value.
 */
xmlNode *
as_xml_add_uint_node (xmlNode *root, const gchar *name, guint64 value)
{
	g_autofree gchar *value_str = NULL;

	value_str = g_strdup_printf ("%" G_GUINT64_FORMAT, value);
	return xmlNewTextChild (root, NULL, (xmlChar *) name, (xmlChar *) value_str);
}

/**
 * as_xml_add_text_prop:
 * @node: The node to attach a property to.
 * @name: The new property name.
 * @value: The new property value.
 *
 * Add property to node if value is not empty.
 */
xmlAttr *
as_xml_add_text_prop (xmlNode *node, const gchar *name, const gchar *value)
{
	if (as_is_empty (value))
		return NULL;

	return xmlNewProp (node, (xmlChar *) name, (xmlChar *) value);
}

/**
 * as_xml_add_uint_prop:
 * @node: The node to attach a property to.
 * @name: The new property name.
 * @value: The new property value.
 *
 * Add integer property to node.
 */
xmlAttr *
as_xml_add_uint_prop (xmlNode *node, const gchar *name, guint64 value)
{
	g_autofree gchar *value_str = NULL;

	value_str = g_strdup_printf ("%" G_GUINT64_FORMAT, value);
	return xmlNewProp (node, (xmlChar *) name, (xmlChar *) value_str);
}

/**
 * libxml_generic_error:
 *
 * Catch out-of-context errors emitted by libxml2.
 */
static void
libxml_generic_error (gchar **error_str_ptr, const char *format, ...)
{
	GString *str;
	va_list arg_ptr;
	gchar *error_str;
	static GMutex mutex;
	g_assert (error_str_ptr != NULL);

	error_str = (*error_str_ptr);

	g_mutex_lock (&mutex);
	str = g_string_new (error_str ? error_str : "");

	va_start (arg_ptr, format);
	g_string_append_vprintf (str, format, arg_ptr);
	va_end (arg_ptr);

	g_free (error_str);
	*error_str_ptr = g_string_free (str, FALSE);
	g_mutex_unlock (&mutex);
}

/**
 * as_xml_set_out_of_context_error:
 *
 * NOTE: The error-function is supposed to be set & called
 * thread-local, so we don't need to do any locking here. We just
 * need to make sure it is set for each thread.
 */
static void
as_xml_set_out_of_context_error (gchar **error_msg_ptr)
{
	if (error_msg_ptr == NULL) {
		xmlSetGenericErrorFunc (NULL, NULL);
	} else {
		g_free (*error_msg_ptr);
		(*error_msg_ptr) = NULL;
		xmlSetGenericErrorFunc (error_msg_ptr, (xmlGenericErrorFunc) libxml_generic_error);
	}
}

/**
 * as_xml_parse_document:
 */
xmlDoc *
as_xml_parse_document (const gchar *data, gssize len, gboolean pedantic, GError **error)
{
	xmlDoc *doc;
	xmlNode *root;
	gint parser_options;
	g_autofree gchar *error_msg_str = NULL;

	if (data == NULL) {
		/* empty document means no components */
		return NULL;
	}

	if (len < 0)
		len = strlen (data);

	parser_options = XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_BIG_LINES;
	if (pedantic)
		parser_options |= XML_PARSE_PEDANTIC;

	as_xml_set_out_of_context_error (&error_msg_str);
	doc = xmlReadMemory (data, len, NULL, "utf-8", parser_options);
	if (doc == NULL) {
		if (error_msg_str == NULL) {
			g_set_error (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_PARSE,
				     "Could not parse XML data (no details received)");
		} else {
			g_set_error (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_PARSE,
				     "Could not parse XML data: %s",
				     error_msg_str);
		}
		as_xml_set_out_of_context_error (NULL);
		return NULL;
	}
	as_xml_set_out_of_context_error (NULL);

	root = xmlDocGetRootElement (doc);
	if (root == NULL) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_PARSE,
				     "The XML document is empty.");
		xmlFreeDoc (doc);
		return NULL;
	}

	return doc;
}

/**
 * as_xml_node_free_to_str:
 * @root: The document root node.
 *
 * Converts an XML node into its textural representation.
 * This takes ownership of the root node and frees it in
 * the process.
 *
 * Returns: XML metadata.
 */
gchar *
as_xml_node_free_to_str (xmlNode *root, GError **error)
{
	xmlDoc *doc;
	gchar *xmlstr = NULL;
	g_autofree gchar *error_msg_str = NULL;

	as_xml_set_out_of_context_error (&error_msg_str);
	doc = xmlNewDoc ((xmlChar *) NULL);
	if (root == NULL)
		goto out;

	xmlDocSetRootElement (doc, root);
	xmlDocDumpFormatMemoryEnc (doc, (xmlChar **) (&xmlstr), NULL, "utf-8", TRUE);

	if (error_msg_str != NULL) {
		if (error == NULL) {
			g_warning ("Could not serialize XML document: %s", error_msg_str);
			g_free (g_steal_pointer (&xmlstr));
			goto out;
		} else {
			g_set_error (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "Could not serialize XML document: %s",
				     error_msg_str);
			g_free (g_steal_pointer (&xmlstr));
			goto out;
		}
	}

out:
	as_xml_set_out_of_context_error (NULL);
	xmlFreeDoc (doc);
	return xmlstr;
}
