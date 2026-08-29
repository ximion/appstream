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
#include "as-context-private.h"

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
 * AsDescTextCtx:
 * @str: The string the block element is assembled in.
 * @have_text: %TRUE as soon as any real content was written for this block.
 * @pending_space: %TRUE while a run of whitespace is waiting to be written out.
 *
 * State for serializing the text of one description block element. Whitespace in
 * descriptions is never significant, so we normalize it as we go: a pending space
 * is only ever written once actual content follows it, which collapses runs of
 * whitespace to a single space and drops it entirely at the start and the end of
 * the block.
 */
typedef struct {
	GString *str;
	gboolean have_text;
	gboolean pending_space;
} AsDescTextCtx;

/**
 * as_desc_text_ctx_init:
 *
 * Start a new block element in @str.
 */
static inline void
as_desc_text_ctx_init (AsDescTextCtx *ctx, GString *str)
{
	ctx->str = str;
	ctx->have_text = FALSE;
	ctx->pending_space = FALSE;
}

/**
 * as_xml_desc_flush_space:
 *
 * Write out a whitespace run that we held back, if there is one. This does not
 * count as content itself, so a block that holds nothing but markup and spaces
 * still ends up empty.
 */
static inline void
as_xml_desc_flush_space (AsDescTextCtx *ctx)
{
	if (!ctx->pending_space)
		return;
	ctx->pending_space = FALSE;
	g_string_append_c (ctx->str, ' ');
}

/**
 * as_xml_desc_append_content:
 *
 * Append a chunk of literal content, writing out any whitespace we held back
 * before it.
 */
static inline void
as_xml_desc_append_content (AsDescTextCtx *ctx, const gchar *data, gssize len)
{
	as_xml_desc_flush_space (ctx);
	if (len < 0)
		g_string_append (ctx->str, data);
	else
		g_string_append_len (ctx->str, data, len);
	ctx->have_text = TRUE;
}

/**
 * as_xml_desc_append_tag:
 *
 * Append the start or end tag of an element.
 */
static inline void
as_xml_desc_append_tag (GString *str, const gchar *name, gboolean closing)
{
	g_string_append_c (str, '<');
	if (closing)
		g_string_append_c (str, '/');
	g_string_append (str, name);
	g_string_append_c (str, '>');
}

/**
 * as_xml_desc_append_text:
 *
 * Append text to the block element currently being assembled, escaping all
 * characters that must not appear verbatim in XML character data and collapsing
 * every run of whitespace into a single space.
 */
static void
as_xml_desc_append_text (AsDescTextCtx *ctx, const gchar *text)
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
		case ' ':
			/* a lone space between two words is already exactly what we want,
			 * so we leave it in the current chunk and copy it along with the
			 * surrounding text - only whitespace that actually needs rewriting
			 * interrupts the copy */
			if ((p > chunk_start) && (p[1] != '\0') && !g_ascii_isspace (p[1]))
				continue;
			rep = NULL;
			break;
		case '\t':
		case '\n':
		case '\v':
		case '\f':
		case '\r':
			rep = NULL;
			break;
		default:
			continue;
		}

		if (p > chunk_start)
			as_xml_desc_append_content (ctx, chunk_start, p - chunk_start);
		if (rep == NULL) {
			/* hold the whitespace back - if content follows, it becomes a
			 * single space, otherwise it is dropped */
			if (ctx->have_text)
				ctx->pending_space = TRUE;
		} else {
			as_xml_desc_append_content (ctx, rep, -1);
		}
		chunk_start = p + 1;
	}

	if (p > chunk_start)
		as_xml_desc_append_content (ctx, chunk_start, p - chunk_start);
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
	case 7:
		return memcmp (name, "heading", 7) == 0;
	default:
		return FALSE;
	}
}

/**
 * as_xml_desc_append_inline_content:
 *
 * Serialize the content of a description paragraph or list item, permitting
 * only markup that is valid in AppStream descriptions. Any other element is
 * replaced by its (escaped) text content, and redundant whitespace is collapsed.
 */
static void
as_xml_desc_append_inline_content (AsDescTextCtx *ctx, xmlNode *node, guint depth)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type == XML_TEXT_NODE || iter->type == XML_CDATA_SECTION_NODE) {
			as_xml_desc_append_text (ctx, (const gchar *) iter->content);
			continue;
		}

		if (iter->type != XML_ELEMENT_NODE && iter->type != XML_ENTITY_REF_NODE)
			continue;

		if (G_UNLIKELY (iter->type == XML_ENTITY_REF_NODE ||
				depth >= AS_DESCRIPTION_MARKUP_MAX_DEPTH)) {
			/* resolve entity references, and refuse to descend any deeper into
			 * excessively nested markup - in both cases we just take the text */
			g_autofree gchar *content = as_xml_get_node_value_raw (iter);
			as_xml_desc_append_text (ctx, content);
			continue;
		}

		if (as_xml_desc_is_inline_tag ((const gchar *) iter->name, -1)) {
			/* a space before the tag belongs outside of it, so we write it out
			 * first. A space *after* the content stays pending, so it ends up
			 * behind the closing tag instead of in front of it. */
			as_xml_desc_flush_space (ctx);

			/* the element is permitted, but none of its attributes ever are */
			as_xml_desc_append_tag (ctx->str, (const gchar *) iter->name, FALSE);
			as_xml_desc_append_inline_content (ctx, iter, depth + 1);
			as_xml_desc_append_tag (ctx->str, (const gchar *) iter->name, TRUE);
		} else {
			/* flatten invalid markup to its text content */
			as_xml_desc_append_inline_content (ctx, iter, depth + 1);
		}
	}
}

/**
 * as_xml_dump_description_heading_content:
 * @node: The heading element.
 *
 * Dump the text content of a description section heading, without its enclosing
 * tag. Headings are plain text, so any markup they contain is flattened, and
 * their whitespace is collapsed just like that of a paragraph.
 *
 * Returns: The escaped text, or %NULL if the node had no usable content.
 */
static gchar *
as_xml_dump_description_heading_content (xmlNode *node)
{
	g_autoptr(GString) str = NULL;
	g_autofree gchar *text = NULL;
	AsDescTextCtx ctx;

	/* ignore node if it is a space */
	if (G_UNLIKELY (node->type != XML_ELEMENT_NODE))
		return NULL;

	text = as_xml_get_node_value_raw (node);
	if (as_is_empty (text))
		return NULL;

	str = g_string_sized_new (strlen (text) + 8);
	as_desc_text_ctx_init (&ctx, str);
	as_xml_desc_append_text (&ctx, text);
	if (str->len == 0)
		return NULL;

	return g_string_free (g_steal_pointer (&str), FALSE);
}

/**
 * as_xml_desc_append_block_node:
 *
 * Serialize a description block element (paragraph or enumeration) and its
 * contents. The markup is emitted without any layout of its own - how it is
 * broken into lines is decided when we write it out. Invalid block elements as
 * well as elements without any content are dropped.
 */
static void
as_xml_desc_append_block_node (GString *str, xmlNode *node)
{
	const gchar *node_name = (const gchar *) node->name;

	if (as_str_equal0 (node_name, "p")) {
		AsDescTextCtx ctx;
		gsize block_start = str->len;
		gsize content_start;

		as_desc_text_ctx_init (&ctx, str);
		g_string_append (str, "<p>");
		content_start = str->len;
		as_xml_desc_append_inline_content (&ctx, node, 1);
		/* a paragraph that holds no content at all is dropped entirely, just
		 * like the MetaInfo reader drops it */
		if (str->len == content_start) {
			g_string_truncate (str, block_start);
			return;
		}
		g_string_append (str, "</p>");
		return;
	}

	if (as_str_equal0 (node_name, "heading")) {
		/* a section heading is plain text, so any markup it may contain is
		 * flattened to its text content */
		g_autofree gchar *content = as_xml_dump_description_heading_content (node);
		if (content == NULL)
			return;
		g_string_append_printf (str, "<heading>%s</heading>", content);
		return;
	}

	if (as_str_equal0 (node_name, "ul") || as_str_equal0 (node_name, "ol")) {
		gsize list_start = str->len;
		gboolean have_items = FALSE;

		g_string_append_printf (str, "<%s>", node_name);
		for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
			AsDescTextCtx ctx;
			gsize item_start, content_start;

			if (iter->type != XML_ELEMENT_NODE)
				continue;
			/* only list items are permitted in enumerations */
			if (!as_str_equal0 ((const gchar *) iter->name, "li"))
				continue;

			item_start = str->len;
			as_desc_text_ctx_init (&ctx, str);
			g_string_append (str, "  <li>");
			content_start = str->len;
			as_xml_desc_append_inline_content (&ctx, iter, 1);
			if (str->len == content_start) {
				/* an empty list item carries nothing, so we skip it */
				g_string_truncate (str, item_start);
				continue;
			}
			g_string_append (str, "</li>");
			have_items = TRUE;
		}

		/* an enumeration that ended up with no items at all is dropped, just
		 * like an empty paragraph is */
		if (!have_items) {
			g_string_truncate (str, list_start);
			return;
		}
		g_string_append_printf (str, "</%s>", node_name);
	}

	/* any other element is not valid description markup and dropped entirely */
}

/**
 * as_xml_dump_description_para_content:
 * @node: The paragraph or list item element.
 *
 * Dump the sanitized content of a description paragraph or list item, without
 * its enclosing tag. Runs of whitespace are collapsed into a single space.
 *
 * Returns: The markup, or %NULL if the node had no usable content.
 */
gchar *
as_xml_dump_description_para_content (xmlNode *node)
{
	g_autoptr(GString) str = NULL;
	AsDescTextCtx ctx;

	/* ignore node if it is a space */
	if (G_UNLIKELY (node->type != XML_ELEMENT_NODE))
		return NULL;

	str = g_string_new ("");
	as_desc_text_ctx_init (&ctx, str);
	as_xml_desc_append_inline_content (&ctx, node, 1);
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
 * as_xml_desc_text_is_blank:
 *
 * Check whether the content of a text node is whitespace only. Whitespace is
 * never significant in descriptions, so such a node carries nothing.
 */
static inline gboolean
as_xml_desc_text_is_blank (const xmlChar *content)
{
	if (content == NULL)
		return TRUE;

	for (const gchar *c = (const gchar *) content; *c != '\0'; c++) {
		if (!g_ascii_isspace (*c))
			return FALSE;
	}

	return TRUE;
}

/**
 * as_xml_desc_node_has_content:
 *
 * Check whether an element holds anything that we would write out. Whitespace
 * on its own is not content, as it is collapsed away, but inline markup is,
 * because its tags are written even when they enclose nothing.
 */
static gboolean
as_xml_desc_node_has_content (xmlNode *node)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type == XML_ELEMENT_NODE)
			return TRUE;
		if ((iter->type == XML_TEXT_NODE) && !as_xml_desc_text_is_blank (iter->content))
			return TRUE;
	}

	return FALSE;
}

/**
 * as_xml_desc_node_holds_only_elements:
 *
 * Check whether an element holds nothing but child elements. Text that sits
 * outside of a block element, a comment or a CDATA section is dropped when we
 * serialize the markup, so a tree that carries any of them is not something we
 * can pass through unchanged. Whitespace does not count, as the serializer
 * discards it just the same.
 */
static gboolean
as_xml_desc_node_holds_only_elements (xmlNode *node)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type == XML_ELEMENT_NODE)
			continue;
		if ((iter->type == XML_TEXT_NODE) && as_xml_desc_text_is_blank (iter->content))
			continue;
		return FALSE;
	}

	return TRUE;
}

/**
 * as_xml_desc_tree_is_valid:
 * @root: The node holding the description markup.
 *
 * Check whether the given description markup tree contains only valid markup,
 * that is: whether serializing it would give us back exactly what we have.
 *
 * Returns: %TRUE if the tree can be used as-is.
 */
static gboolean
as_xml_desc_tree_is_valid (xmlNode *root)
{
	/* text outside of a block element is dropped when the markup is serialized,
	 * and a tree that holds nothing else is dropped in its entirety */
	if (!as_xml_desc_node_holds_only_elements (root))
		return FALSE;

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
			if (!as_xml_desc_node_has_content (iter))
				return FALSE;
			continue;
		}

		if (as_str_equal0 (node_name, "heading")) {
			/* headings are plain text: anything else is rewritten on output */
			for (xmlNode *iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (iter2->type != XML_TEXT_NODE)
					return FALSE;
			}
			if (!as_xml_desc_node_has_content (iter))
				return FALSE;
			continue;
		}

		if (as_str_equal0 (node_name, "ul") || as_str_equal0 (node_name, "ol")) {
			gboolean have_items = FALSE;

			/* an enumeration only ever brackets its items, so anything else
			 * in it is dropped - and the walk that copies the tree into the
			 * document can not represent it either */
			if (!as_xml_desc_node_holds_only_elements (iter))
				return FALSE;

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
				/* an item without any content is dropped, so it can not
				 * be what keeps the enumeration alive */
				if (!as_xml_desc_node_has_content (iter2))
					return FALSE;
				have_items = TRUE;
			}

			/* an enumeration without any items is dropped when we serialize the
			 * markup, so it is not something we can pass through as-is */
			if (!have_items)
				return FALSE;
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
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		as_xml_desc_append_block_node (str, iter);
	}

	return g_string_free (str, FALSE);
}

/**
 * as_xml_desc_is_enumeration_tag:
 *
 * Check whether the given element name is one of the two enumerations, which
 * bracket their items instead of holding text themselves.
 */
static inline gboolean
as_xml_desc_is_enumeration_tag (const gchar *name, gsize len)
{
	return (len == 2) && ((memcmp (name, "ul", 2) == 0) || (memcmp (name, "ol", 2) == 0));
}

/* libxml2 indents the documents we write by two spaces per level. */
#define AS_XML_OUTPUT_INDENT_STEP 2

/**
 * as_xml_desc_append_indent:
 *
 * Indent the current line by @indent spaces.
 */
static inline void
as_xml_desc_append_indent (GString *str, guint indent)
{
	for (guint i = 0; i < indent; i++)
		g_string_append_c (str, ' ');
}

/* The column we aim for when breaking up the text of a description block. We do
 * not break there, but at the first space that follows, so the lines end up a
 * little longer than this. */
#define AS_DESC_WRAP_COLUMN 100

/**
 * as_xml_desc_format_markup:
 * @markup: Sanitized description markup.
 * @len: Length of @markup, or -1 if it is %NULL-terminated.
 * @indent: Number of spaces the block elements should be indented by.
 *
 * Lay description markup out for output: every block element on a line of its
 * own at @indent, the items of an enumeration one step further in, and text
 * that runs past the wrap column broken up and lined up underneath the element
 * it belongs to.
 *
 * Returns: (transfer full): the formatted markup, or %NULL if @markup was %NULL.
 */
gchar *
as_xml_desc_format_markup (const gchar *markup, gssize len, guint indent)
{
	GString *out;
	const gchar *end;
	guint list_depth = 0;
	gsize column = 0;
	gboolean pending_space = FALSE;

	if (markup == NULL)
		return NULL;
	if (len < 0)
		len = (gssize) strlen (markup);

	end = markup + len;
	out = g_string_sized_new (len + 32);
	for (const gchar *p = markup; p < end;) {
		const gchar *tag_end = NULL;
		const gchar *run_end;
		guint line_indent;
		gboolean starts_line = FALSE;
		gboolean opens_list = FALSE;
		gsize chars = 0;

		if (g_ascii_isspace (*p)) {
			pending_space = TRUE;
			p++;
			continue;
		}

		if (*p == '<')
			tag_end = memchr (p, '>', end - p);
		if (tag_end != NULL) {
			gboolean is_end_tag = (p[1] == '/');
			const gchar *name = p + (is_end_tag ? 2 : 1);
			gsize name_len = tag_end - name;

			if (as_xml_desc_is_enumeration_tag (name, name_len)) {
				/* an enumeration brackets its items, which sit one step
				 * further in than it does */
				starts_line = TRUE;
				opens_list = !is_end_tag;
				if (is_end_tag && (list_depth > 0))
					list_depth--;
			} else if (!is_end_tag) {
				starts_line = as_xml_desc_is_block_tag (name, name_len);
			}
		}
		line_indent = indent + (list_depth * AS_XML_OUTPUT_INDENT_STEP);

		if (starts_line) {
			pending_space = FALSE;
			if (out->len > 0)
				g_string_append_c (out, '\n');
			as_xml_desc_append_indent (out, line_indent);
			g_string_append_len (out, p, tag_end - p + 1);
			column = line_indent + (tag_end - p + 1);
			if (opens_list)
				list_depth++;
			p = tag_end + 1;
			continue;
		}

		/* the first line is the only one we can still be at the start of */
		if (out->len == 0) {
			as_xml_desc_append_indent (out, line_indent);
			column = line_indent;
		}

		/* a space that we held back is only ever written out once we know that
		 * content follows it, and it is where we break the line */
		if (pending_space) {
			pending_space = FALSE;
			/* comparing against the indentation keeps us from breaking, or
			 * indenting twice, when nothing is on the line yet */
			if (column > line_indent) {
				if (column >= AS_DESC_WRAP_COLUMN) {
					g_string_append_c (out, '\n');
					as_xml_desc_append_indent (out, line_indent);
					column = line_indent;
				} else {
					g_string_append_c (out, ' ');
					column++;
				}
			}
		}

		/* Copy the next word, or the inline or end tag that is glued to it. We
		 * stop at every tag so that a block element which begins right behind
		 * one still gets a line of its own. */
		if (*p == '<') {
			run_end = (tag_end == NULL) ? p + 1 : tag_end + 1;
		} else {
			run_end = p;
			while ((run_end < end) && (*run_end != '<') && !g_ascii_isspace (*run_end))
				run_end++;
		}
		for (const gchar *c = p; c < run_end; c++) {
			/* continuation bytes do not start a new character */
			if ((*c & 0xC0) != 0x80)
				chars++;
		}
		g_string_append_len (out, p, run_end - p);
		column += chars;
		p = run_end;
	}

	return g_string_free (out, FALSE);
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
gboolean
as_xml_desc_markup_is_valid (const gchar *markup, gssize len)
{
	const gchar *end;
	guint inline_depth = 0;
	gboolean in_heading = FALSE;

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
			/* headings are plain text and permit no inline markup at all */
			if (in_heading)
				return FALSE;
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
			in_heading = !is_end_tag && (name_len == 7) &&
				     (memcmp (name_start, "heading", 7) == 0);
		}
	}

	return TRUE;
}

/**
 * as_xml_desc_append_inline_md_text:
 *
 * Append literal text to @str, backslash-escaping any character that would
 * otherwise be read back as Markdown. An asterisk only needs escaping if it is
 * adjacent to a non-whitespace character, as isolated asterisks can not delimit
 * an emphasis span, and a hash only if it could start a heading.
 */
static void
as_xml_desc_append_inline_md_text (GString *str, const gchar *text)
{
	for (const gchar *c = text; c[0] != '\0'; c++) {
		if (c[0] == '`') {
			g_string_append (str, "\\`");
		} else if (c[0] == '*') {
			gboolean after_word = (c != text) && !g_ascii_isspace (*(c - 1));
			gboolean before_word = (c[1] != '\0') && !g_ascii_isspace (c[1]);
			if (after_word || before_word)
				g_string_append_c (str, '\\');
			g_string_append_c (str, '*');
		} else if (c[0] == '#') {
			/* A hash introduces a heading when a run of them starts a line
			 * and is followed by a space. Escaping it wherever it could be
			 * read that way keeps ordinary text out of the heading syntax,
			 * no matter where the line ends up being broken. */
			const gchar *run = c;
			gboolean at_line_start = (c == text) || g_ascii_isspace (*(c - 1));
			while (run[0] == '#')
				run++;
			if (at_line_start && (run[0] == ' ' || run[0] == '\t' || run[0] == '\n'))
				g_string_append_c (str, '\\');
			g_string_append_c (str, '#');
		} else if (c[0] == '\\' &&
			   (c[1] == '*' || c[1] == '`' || c[1] == '#' || c[1] == '\\')) {
			g_string_append (str, "\\\\");
		} else {
			g_string_append_c (str, c[0]);
		}
	}
}

/**
 * as_xml_desc_append_inline_md_node:
 *
 * Serialize the children of @node as inline Markdown, mapping the inline tags
 * AppStream permits onto their Markdown equivalents. Any other element is
 * flattened to its text content, mirroring as_xml_desc_append_inline_content().
 */
static void
as_xml_desc_append_inline_md_node (GString *str, xmlNode *node, guint depth)
{
	if (depth >= AS_DESCRIPTION_MARKUP_MAX_DEPTH)
		return;

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if ((iter->type == XML_TEXT_NODE) || (iter->type == XML_CDATA_SECTION_NODE)) {
			if (iter->content != NULL)
				as_xml_desc_append_inline_md_text (str,
								   (const gchar *) iter->content);
			continue;
		}
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (as_str_equal0 (iter->name, "em")) {
			g_string_append_c (str, '*');
			as_xml_desc_append_inline_md_node (str, iter, depth + 1);
			g_string_append_c (str, '*');
		} else if (as_str_equal0 (iter->name, "code")) {
			/* the content of a code span is literal, so it is emitted verbatim */
			g_autofree gchar *content = as_xml_get_node_value_raw (iter);
			g_string_append_printf (str, "`%s`", (content == NULL) ? "" : content);
		} else {
			/* flatten invalid markup to its text content */
			as_xml_desc_append_inline_md_node (str, iter, depth + 1);
		}
	}
}

/**
 * as_xml_desc_to_inline_md:
 * @node: the XML node whose children should be serialized.
 *
 * Convert the inline content of a description block element (a paragraph or
 * list item) into Markdown, preserving the <em/> and <code/> tags that
 * AppStream permits as `*emphasis*` and `` `code` ``.
 *
 * Returns: (transfer full): a newly allocated string.
 */
gchar *
as_xml_desc_to_inline_md (xmlNode *node)
{
	GString *str;

	if (node == NULL)
		return NULL;

	str = g_string_new ("");
	as_xml_desc_append_inline_md_node (str, node, 0);
	return g_string_free (str, FALSE);
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
 * as_xml_desc_parse_fragment:
 *
 * Parse a description markup fragment, which has no root element of its own.
 */
static xmlDoc *
as_xml_desc_parse_fragment (const gchar *markup)
{
	g_autofree gchar *xmldata = g_strconcat ("<root>", markup, "</root>", NULL);
	return xmlReadMemory (xmldata,
			      strlen (xmldata),
			      NULL,
			      "utf-8",
			      XML_PARSE_NOBLANKS | XML_PARSE_NONET);
}

/**
 * as_xml_markup_parse_helper_new: (skip)
 * @markup: The description markup.
 * @indent: Number of spaces the block elements will be indented by on output.
 * @locale: (nullable): The locale the markup is in.
 **/
static AsXMLMarkupParseHelper *
as_xml_markup_parse_helper_new (const gchar *markup, guint indent, const gchar *locale)
{
	g_autofree gchar *formatted = NULL;
	AsXMLMarkupParseHelper *helper = g_slice_new0 (AsXMLMarkupParseHelper);
	xmlNode *root;

	helper->locale = g_strdup (locale);

	/* We lay the markup out before parsing it, because the line breaks have to be
	 * part of the tree that we copy into the document. Checking the markup does not
	 * change it, so doing this first is the same as doing it afterwards - only the
	 * markup we have to rewrite below needs to be laid out again. */
	formatted = as_xml_desc_format_markup (markup, -1, indent);
	helper->doc = as_xml_desc_parse_fragment (formatted);
	if (helper->doc == NULL)
		goto fail;

	/* The markup may have been set via the API, or read from a format where we can not
	 * validate it while reading (like DEP-11 YAML), so we ensure that we never write
	 * anything that isn't valid description markup. We check if the tree is valid,
	 * and use it verbatim (valid data is the overwhelmingly common case). */
	root = xmlDocGetRootElement (helper->doc);
	if (G_UNLIKELY (root != NULL && !as_xml_desc_tree_is_valid (root))) {
		g_autofree gchar *safe_markup = as_xml_dump_description_children (root);
		g_autofree gchar *safe_formatted = as_xml_desc_format_markup (safe_markup,
									      -1,
									      indent);

		xmlFreeDoc (helper->doc);
		helper->doc = as_xml_desc_parse_fragment (safe_formatted);
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
	if ((helper->tag_id == AS_TAG_P) || (helper->tag_id == AS_TAG_LI) ||
	    (helper->tag_id == AS_TAG_HEADING)) {
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
	gboolean list_open;
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
	g_autoptr(GPtrArray) open_lists = NULL;
	GHashTableIter res_iter;
	gpointer res_value;
	gpointer res_key;
	AsXMLMetaInfoDescParseHelper *phelper;
	guint untranslated_elem_count = 0;

	/* scratch space listing the locales that have an enumeration open, reused for
	 * every enumeration we encounter (the helpers in here are owned by tmp_desc) */
	open_lists = g_ptr_array_new ();

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

		if ((tag_id == AS_TAG_P) || (tag_id == AS_TAG_HEADING)) {
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

			content = (tag_id == AS_TAG_HEADING)
				      ? as_xml_dump_description_heading_content (iter)
				      : as_xml_dump_description_para_content (iter);
			if (content != NULL) {
				g_string_append_printf (phelper->data,
							"<%s>%s</%s>",
							node_name,
							content,
							node_name);
				phelper->elem_count += 1;
			}

		} else if ((tag_id == AS_TAG_UL) || (tag_id == AS_TAG_OL)) {
			for (xmlNode *iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
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

				content = as_xml_dump_description_para_content (iter2);
				if (content == NULL)
					continue;

				phelper = g_hash_table_lookup (tmp_desc, lang);
				if (phelper == NULL) {
					phelper = as_xml_metainfo_desc_parse_helper_new ();
					g_hash_table_insert (tmp_desc,
							     g_ref_string_new_intern (lang),
							     phelper);
				}

				/* Open the enumeration the first time this locale contributes an
				 * item to it. Doing this lazily - instead of opening and closing
				 * it for every known locale - is what keeps the amount of data we
				 * generate proportional to the amount of data we were given: a
				 * document can otherwise declare many locales and many empty
				 * enumerations, and have us emit markup for each combination of
				 * the two. */
				if (!phelper->list_open) {
					g_string_append_printf (phelper->data, "<%s>", node_name);
					phelper->list_open = TRUE;
					g_ptr_array_add (open_lists, phelper);
				}

				g_string_append_printf (phelper->data,
							"  <%s>%s</%s>",
							(gchar *) iter2->name,
							content,
							(gchar *) iter2->name);
				phelper->elem_count += 1;
			}

			/* close the enumeration again for every locale that entered it */
			for (guint i = 0; i < open_lists->len; i++) {
				phelper = g_ptr_array_index (open_lists, i);
				g_string_append_printf (phelper->data, "</%s>", node_name);
				phelper->list_open = FALSE;
			}
			g_ptr_array_set_size (open_lists, 0);
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
 * as_xml_desc_output_indent:
 *
 * Work out the indentation that the description block elements will end up at
 * once the document is written out, so that text we break across lines can be
 * lined up underneath them.
 */
static guint
as_xml_desc_output_indent (AsContext *ctx, xmlNode *parent)
{
	guint level = 0;

	/* The component node is assembled detached and only added to its document
	 * afterwards, so walking up from @parent tells us how deep it sits inside
	 * of that component, counting the component node itself. */
	for (xmlNode *iter = parent; iter != NULL; iter = iter->parent) {
		if (iter->type != XML_ELEMENT_NODE)
			break;
		level++;
	}

	/* catalog XML collects all components in a `components` element, which
	 * pushes every one of them one level further in - MetaInfo files hold a
	 * single component and have no such element */
	if ((ctx != NULL) && (as_context_get_style (ctx) == AS_FORMAT_STYLE_CATALOG))
		level++;

	/* the `description` element sits below @parent, and its blocks below that */
	return (level + 1) * AS_XML_OUTPUT_INDENT_STEP;
}

/**
 * as_xml_add_description_catalog_mode_helper:
 *
 * Add the description markup for AppStream catalog XML to the tree.
 */
static gboolean
as_xml_add_description_catalog_mode_helper (AsContext *ctx,
					    xmlNode *parent,
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

	helper = as_xml_markup_parse_helper_new (description_markup,
						 as_xml_desc_output_indent (ctx, parent),
						 lang);
	if (helper == NULL)
		return FALSE;

	/* nothing of the markup survived sanitization, so there is no description
	 * left for us to write */
	if (helper->node == NULL)
		return FALSE;

	dnode = xmlNewChild (parent, NULL, (xmlChar *) "description", NULL);
	if (helper->localized) {
		xmlNewProp (dnode, (xmlChar *) "xml:lang", (xmlChar *) lang);
	}
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

			helper = as_xml_markup_parse_helper_new (
			    desc_markup,
			    as_xml_desc_output_indent (ctx, root),
			    locale);
			if (helper == NULL)
				continue;
			/* nothing of the markup survived sanitization, so this locale
			 * has no description left for us to write */
			if (helper->node == NULL) {
				as_xml_markup_parse_helper_free (helper);
				continue;
			}

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

			as_xml_add_description_catalog_mode_helper (ctx, root, desc_markup, locale);
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
as_xml_add_description_node_raw (AsContext *ctx, xmlNode *root, const gchar *description)
{
	xmlNode *dnode;
	xmlNode *cnode;
	g_autoptr(AsXMLMarkupParseHelper) helper = NULL;

	if (as_is_empty (description))
		return NULL;

	helper = as_xml_markup_parse_helper_new (description,
						 as_xml_desc_output_indent (ctx, root),
						 NULL);
	if (helper == NULL)
		return NULL;

	/* nothing of the markup survived sanitization, so there is no description
	 * left for us to write */
	if (helper->node == NULL)
		return NULL;

	dnode = xmlNewChild (root, NULL, (xmlChar *) "description", NULL);
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
		/* no data at all is not a valid document */
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_PARSE,
				     "The XML document is empty.");
		return NULL;
	}

	if (len < 0)
		len = strlen (data);

	/* NB: XML_PARSE_NOBLANKS is worth roughly 15% of the time it takes to load a
	 * catalog, so we keep it even though its heuristic drops the whitespace in
	 * markup like `<em>a</em> <em>b</em>`, where the space between two inline
	 * spans is the only text the paragraph has. */
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
