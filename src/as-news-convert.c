/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2019 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-news-convert
 * @short_description: Read and write NEWS/Changelog files from metainfo
 * @include: appstream.h
 *
 * Read NEWS and other types of release information files and convert them
 * to AppStream metainfo data.
 * Also, write NEWS files from #AsRelease release information.
 *
 * This is a private/internal class.
 */

#include "config.h"
#include "as-news-convert.h"

#include "as-metadata.h"
#include "as-xml.h"
#include "as-yaml.h"

/**
 * as_news_format_kind_to_string:
 * @kind: the #AsNewsFormatKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.12.9
 **/
const gchar*
as_news_format_kind_to_string (AsNewsFormatKind kind)
{
	if (kind == AS_NEWS_FORMAT_KIND_YAML)
		return "yaml";
	if (kind == AS_NEWS_FORMAT_KIND_TEXT)
		return "text";
	return "unknown";
}

/**
 * as_news_format_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsNewsFormatKind or %AS_NEWS_FORMAT_KIND_UNKNOWN for unknown
 *
 * Since: 0.12.9
 **/
AsNewsFormatKind
as_news_format_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "yaml") == 0)
		return AS_NEWS_FORMAT_KIND_YAML;
	if (g_strcmp0 (kind_str, "text") == 0)
		return AS_NEWS_FORMAT_KIND_TEXT;
	return AS_NEWS_FORMAT_KIND_UNKNOWN;
}

/**
 * as_news_yaml_to_release:
 */
GPtrArray*
as_news_yaml_to_releases (const gchar *yaml_data, GError **error)
{
	yaml_parser_t parser;
	yaml_event_t event;
	gboolean parse = TRUE;
	gboolean ret = TRUE;
	g_autoptr(GPtrArray) releases = NULL;

	if (yaml_data == NULL) {
		g_set_error (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"YAML news file was empty.");
		return NULL;
	}

	releases = g_ptr_array_new_with_free_func (g_object_unref);

	/* initialize YAML parser */
	yaml_parser_initialize (&parser);
	yaml_parser_set_input_string (&parser, (unsigned char*) yaml_data, strlen (yaml_data));

	while (parse) {
		if (!yaml_parser_parse (&parser, &event)) {
			g_set_error (error,
					AS_METADATA_ERROR,
					AS_METADATA_ERROR_PARSE,
					"Could not parse YAML: %s", parser.problem);
			ret = FALSE;
			break;
		}

		if (event.type == YAML_DOCUMENT_START_EVENT) {
			GNode *n;
			GError *tmp_error = NULL;
			g_autoptr(GNode) root = NULL;
			g_autoptr(AsRelease) rel = as_release_new ();

			root = g_node_new (g_strdup (""));
			as_yaml_parse_layer (&parser, root, &tmp_error);
			if (tmp_error != NULL) {
				/* stop immediately, since we found an error when parsing the document */
				g_propagate_error (error, tmp_error);
				g_free (root->data);
				yaml_event_delete (&event);
				ret = FALSE;
				parse = FALSE;
				break;
			}

			for (n = root->children; n != NULL; n = n->next) {
				const gchar *key;
				const gchar *value;

				if ((n->data == NULL) || (n->children == NULL)) {
					/* skip an empty node */
					continue;
				}

				key = as_yaml_node_get_key (n);
				value = as_yaml_node_get_value (n);

				if (g_strcmp0 (key, "Version") == 0) {
					as_release_set_version (rel, value);
				} else if (g_strcmp0 (key, "Date") == 0) {
					as_release_set_date (rel, value);
				} else if (g_strcmp0 (key, "Type") == 0) {
					AsReleaseKind rkind = as_release_kind_from_string (value);
					if (rkind != AS_RELEASE_KIND_UNKNOWN)
						as_release_set_kind (rel, rkind);
				} else if ((g_strcmp0 (key, "Description") == 0) || (g_strcmp0 (key, "Notes") == 0)) {
					g_autoptr(GString) str = g_string_new ("");

					if ((n->children != NULL) && (n->children->next != NULL)) {
						GNode *dn;
						g_string_append (str, "<ul>");
						for (dn = n->children; dn != NULL; dn = dn->next) {
							g_string_append_printf (str, "<li>%s</li>", as_yaml_node_get_key (dn));
						}
						g_string_append (str, "</ul>");

					} else {
						/* we only have one list entry, or no list at all and a freefor text instead. Convert to a paragraph */
						g_auto(GStrv) paras = g_strsplit (value, "\n\n", -1);
						for (guint i = 0; paras[i] != NULL; i++)
							g_string_append_printf (str, "<p>%s</p>", paras[i]);
					}

					as_release_set_description (rel, str->str, "C");
				}
			}

			if (as_release_get_version (rel) != NULL)
				g_ptr_array_add (releases, g_steal_pointer (&rel));
			g_node_traverse (root,
					G_IN_ORDER,
					G_TRAVERSE_ALL,
					-1,
					as_yaml_free_node,
					NULL);
		}

		/* stop if end of stream is reached */
		if (event.type == YAML_STREAM_END_EVENT)
			parse = FALSE;

		yaml_event_delete (&event);
	}

	yaml_parser_delete (&parser);

	/* return NULL on error, otherwise return the list of releases */
	if (ret)
		return g_steal_pointer (&releases);
	else
		return NULL;
}

/**
 * as_news_yaml_write_handler_cb:
 *
 * Helper function to store the emitted YAML document.
 */
static int
as_news_yaml_write_handler_cb (void *ptr, unsigned char *buffer, size_t size)
{
	GString *str;
	str = (GString*) ptr;
	g_string_append_len (str, (const gchar*) buffer, size);

	return 1;
}

/**
 * as_news_releases_to_yaml:
 */
gboolean
as_news_releases_to_yaml (GPtrArray *releases, gchar **yaml_data)
{
	yaml_emitter_t emitter;
	yaml_event_t event;
	gboolean res = FALSE;
	gboolean report_validation_passed = TRUE;
	GString *yaml_result = g_string_new ("");

	yaml_emitter_initialize (&emitter);
	yaml_emitter_set_indent (&emitter, 2);
	yaml_emitter_set_unicode (&emitter, TRUE);
	yaml_emitter_set_width (&emitter, 255);
	yaml_emitter_set_output (&emitter, as_news_yaml_write_handler_cb, yaml_result);

	/* emit start event */
	yaml_stream_start_event_initialize (&event, YAML_UTF8_ENCODING);
	if (!yaml_emitter_emit (&emitter, &event)) {
		g_critical ("Failed to initialize YAML emitter.");
		g_string_free (yaml_result, TRUE);
		yaml_emitter_delete (&emitter);
		return FALSE;
	}

	for (guint i = 0; i < releases->len; i++) {
		AsRelease *rel = AS_RELEASE (g_ptr_array_index (releases, i));
		AsReleaseKind rkind = as_release_get_kind (rel);
		const gchar *rel_active_locale = as_release_get_active_locale (rel);
		const gchar *desc_markup;

		/* we only write the untranslated strings */
		as_release_set_active_locale (rel, "C");
		desc_markup = as_release_get_description (rel);

		/* new document for this release */
		yaml_document_start_event_initialize (&event, NULL, NULL, NULL, FALSE);
		res = yaml_emitter_emit (&emitter, &event);
		g_assert (res);

		/* main dict start */
		as_yaml_mapping_start (&emitter);

		as_yaml_emit_scalar_raw (&emitter, "Version");
		as_yaml_emit_scalar_raw (&emitter, as_release_get_version (rel));

		as_yaml_emit_entry (&emitter, "Date", as_release_get_date (rel));
		if (rkind != AS_RELEASE_KIND_STABLE)
			as_yaml_emit_entry (&emitter, "Type", as_release_kind_to_string (rkind));

		if (desc_markup != NULL) {
			xmlDoc *doc = NULL;
			xmlNode *root;
			xmlNode *iter;
			g_autofree gchar *xmldata = NULL;
			g_autoptr(GString) str = g_string_new ("");
			gboolean desc_listing = g_str_has_prefix (desc_markup, "<ul>") || g_str_has_prefix (desc_markup, "<ol>");

			/* make XML parser happy by providing a root element */
			xmldata = g_strdup_printf ("<root>%s</root>", desc_markup);

			doc = xmlParseDoc ((xmlChar*) xmldata);
			if (doc == NULL)
				goto xml_end;

			root = xmlDocGetRootElement (doc);
			if (root == NULL) {
				/* document was empty */
				goto xml_end;
			}

			if (desc_listing) {
				as_yaml_emit_scalar (&emitter, "Description");
				as_yaml_sequence_start (&emitter);
			}

			for (iter = root->children; iter != NULL; iter = iter->next) {
				gchar *content;
				xmlNode *iter2;
				/* discard spaces */
				if (iter->type != XML_ELEMENT_NODE)
					continue;

				if (g_strcmp0 ((gchar*) iter->name, "p") == 0) {
					content = (gchar*) xmlNodeGetContent (iter);

					if (desc_listing) {
						as_yaml_emit_scalar (&emitter, content);
					} else {
						if (str->len > 0)
							g_string_append (str, "\n\n");
						g_string_append (str, content);
					}

					g_free (content);
				} else if ((g_strcmp0 ((gchar*) iter->name, "ul") == 0) || (g_strcmp0 ((gchar*) iter->name, "ol") == 0)) {
					/* iterate over itemize contents */
					for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
						if (iter2->type != XML_ELEMENT_NODE)
							continue;
						if (g_strcmp0 ((gchar*) iter2->name, "li") == 0) {
							content = (gchar*) xmlNodeGetContent (iter2);

							if (desc_listing) {
								as_yaml_emit_scalar (&emitter, content);
							} else {
								if (str->len > 0)
									g_string_append (str, "\n\n");
								g_string_append (str, content);
							}

							g_free (content);
						}
					}
				}
			}

			if (desc_listing) {
				as_yaml_sequence_end (&emitter);
			} else {
				as_yaml_emit_long_entry_literal (&emitter, "Description", str->str);
			}

xml_end:
			if (doc != NULL)
				xmlFreeDoc (doc);
		}

		as_release_set_active_locale (rel, rel_active_locale);

		/* main dict end */
		as_yaml_mapping_end (&emitter);
		/* finalize the document */
		yaml_document_end_event_initialize (&event, 1);
		res = yaml_emitter_emit (&emitter, &event);
		g_assert (res);
	}

	/* end stream */
	yaml_stream_end_event_initialize (&event);
	res = yaml_emitter_emit (&emitter, &event);
	g_assert (res);

	yaml_emitter_flush (&emitter);
	yaml_emitter_delete (&emitter);
	*yaml_data = g_string_free (yaml_result, FALSE);
	return report_validation_passed;
}
