/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2022 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
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
 * These functions are private/internal.
 */

#include "config.h"
#include "as-news-convert.h"

#include "as-metadata.h"
#include "as-xml.h"
#include "as-yaml.h"
#include "as-utils-private.h"
#include "as-release-private.h"

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
	if (kind_str == NULL)
		return AS_NEWS_FORMAT_KIND_UNKNOWN;
	if (g_strcmp0 (kind_str, "yaml") == 0)
		return AS_NEWS_FORMAT_KIND_YAML;
	if (g_strcmp0 (kind_str, "text") == 0)
		return AS_NEWS_FORMAT_KIND_TEXT;
	return AS_NEWS_FORMAT_KIND_UNKNOWN;
}

/**
 * as_releases_to_metainfo_xml_chunk:
 *
 * Internal helper method to convert release objects to XML.
 */
gchar*
as_releases_to_metainfo_xml_chunk (GPtrArray *releases, GError **error)
{
	g_autoptr(AsContext) ctx = NULL;
	xmlNode *root;
	xmlNode *n_releases;
	g_auto(GStrv) strv = NULL;
	g_autofree gchar *xml_raw = NULL;
	guint lines;

	ctx = as_context_new ();
	as_context_set_locale (ctx, "C");
	as_context_set_style (ctx, AS_FORMAT_STYLE_METAINFO);

	root = xmlNewNode (NULL, (xmlChar*) "component");
	n_releases = xmlNewChild (root, NULL, (xmlChar*) "releases", NULL);

	for (guint i = 0; i < releases->len; ++i) {
		AsRelease *release = AS_RELEASE (g_ptr_array_index (releases, i));
		as_release_to_xml_node (release,
					ctx,
					n_releases);
	}

	xml_raw = as_xml_node_to_str (root, error);
	if ((error != NULL) && (*error != NULL))
		return NULL;

	/* this is inefficient, but we don't actually need to be very fast here */
	strv = g_strsplit (xml_raw, "\n", -1);
	lines = g_strv_length (strv);
	if (lines < 4)
		return NULL; /* something went wrong here */
	g_free(strv[lines - 1]);
	g_free(strv[lines - 2]);
	strv[lines - 2] = NULL;

	return g_strjoinv ("\n", strv + 2);
}

/**
 * as_news_yaml_to_release:
 */
static GPtrArray*
as_news_yaml_to_releases (const gchar *yaml_data,
			  gint limit,
			  GError **error)
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
							g_autofree gchar *escaped = g_markup_escape_text (as_yaml_node_get_key (dn), -1);
							g_string_append_printf (str, "<li>%s</li>", escaped);
						}
						g_string_append (str, "</ul>");

					} else {

						/* we only have one list entry, or no list at all and a freeform text instead. Convert to paragraphs */
						g_auto(GStrv) paras = g_strsplit (value, "\n\n", -1);
						for (guint i = 0; paras[i] != NULL; i++) {
							g_auto(GStrv) lines = NULL;
							g_autoptr(GString) para = NULL;
							gboolean in_listing = FALSE;
							gboolean in_paragraph = FALSE;
							g_autofree gchar *escaped = g_markup_escape_text (paras[i], -1);

							para = g_string_new ("");
							lines = g_strsplit (escaped, "\n", -1);
							for (guint j = 0; lines[j] != NULL; j++) {
								if (g_str_has_prefix (lines[j], " -") || g_str_has_prefix (lines[j], " *")) {
									/* we have a list */
									if (in_paragraph) {
										g_string_truncate (str, str->len - 1);
										g_string_append (str, "</p>\n");
										in_paragraph = FALSE;
									}
									if (in_listing) {
										g_string_append (str, "</li>\n<li>");
									} else {
										g_string_append (str, "<ul>\n<li>");
									}
									g_string_append (str, lines[j] + 3);
									in_listing = TRUE;
									continue;
								} else if (in_listing) {
									if (g_str_has_prefix (lines[j], "   ")) {
										g_string_append_printf (str, " %s", lines[j] + 3);
									} else {
										g_string_append (str, "</li>\n</ul>\n");
										in_listing = FALSE;
										g_string_append_printf (str, "<p>%s\n", lines[j]);
										in_paragraph = TRUE;
									}
								} else {
									g_string_append_printf (str, "<p>%s\n", lines[j]);
									in_paragraph = TRUE;
								}
							}
							if (in_listing)
								g_string_append (str, "</li>\n</ul>\n");
							if (in_paragraph) {
									g_string_truncate (str, str->len - 1);
									g_string_append (str, "</p>\n");
							}
						}
					}

					as_release_set_description (rel, str->str, "C");
				}
			}

			if (as_release_get_version (rel) != NULL) {
				g_ptr_array_add (releases, g_steal_pointer (&rel));
				if (limit > 0 && releases->len >= (guint) limit)
					parse = FALSE;
			}

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
static gboolean
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
			if (g_strstr_len (desc_markup, -1, "<p>") != NULL) {
				/* we have paragraphs - just convert the markup to a simple text */
				g_autofree gchar *md = NULL;
				md = as_description_markup_convert (desc_markup,
								    AS_MARKUP_KIND_MARKDOWN,
								    NULL);
				if (md != NULL)
					as_yaml_emit_long_entry_literal (&emitter, "Description", md);
			} else {
				xmlDoc *doc = NULL;
				xmlNode *root;
				xmlNode *iter;
				g_autofree gchar *xmldata = NULL;

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

				as_yaml_emit_scalar (&emitter, "Description");
				as_yaml_sequence_start (&emitter);

				for (iter = root->children; iter != NULL; iter = iter->next) {
					xmlNode *iter2;
					/* discard spaces */
					if (iter->type != XML_ELEMENT_NODE)
						continue;

					if ((g_strcmp0 ((gchar*) iter->name, "ul") == 0) || (g_strcmp0 ((gchar*) iter->name, "ol") == 0)) {
						/* iterate over itemize contents */
						for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
							if (iter2->type != XML_ELEMENT_NODE)
								continue;
							if (g_strcmp0 ((gchar*) iter2->name, "li") == 0) {
								g_autofree gchar *content = (gchar*) xmlNodeGetContent (iter2);
								as_yaml_emit_scalar (&emitter, as_strstripnl (content));
							}
						}
					}
				}

				as_yaml_sequence_end (&emitter);

			xml_end:
				if (doc != NULL)
					xmlFreeDoc (doc);
			}
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

typedef enum {
	AS_NEWS_SECTION_KIND_UNKNOWN,
	AS_NEWS_SECTION_KIND_HEADER,
	AS_NEWS_SECTION_KIND_NOTES,
	AS_NEWS_SECTION_KIND_BUGFIX,
	AS_NEWS_SECTION_KIND_FEATURES,
	AS_NEWS_SECTION_KIND_MISC,
	AS_NEWS_SECTION_KIND_TRANSLATION,
	AS_NEWS_SECTION_KIND_DOCUMENTATION,
	AS_NEWS_SECTION_KIND_CONTRIBUTORS,
	AS_NEWS_SECTION_KIND_TRANSLATORS,
	AS_NEWS_SECTION_KIND_LAST
} AsNewsSectionKind;

static AsNewsSectionKind
as_news_text_guess_section (const gchar *lines)
{
	if (g_strstr_len (lines, -1, "~~~~") != NULL)
		return AS_NEWS_SECTION_KIND_HEADER;
	if (g_strstr_len (lines, -1, "----") != NULL)
		return AS_NEWS_SECTION_KIND_HEADER;
	if (g_strstr_len (lines, -1, "Bugfix:\n") != NULL)
		return AS_NEWS_SECTION_KIND_BUGFIX;
	if (g_strstr_len (lines, -1, "Bugfixes:\n") != NULL)
		return AS_NEWS_SECTION_KIND_BUGFIX;
	if (g_strstr_len (lines, -1, "Bug fixes:\n") != NULL)
		return AS_NEWS_SECTION_KIND_BUGFIX;
	if (g_strstr_len (lines, -1, "Features:\n") != NULL)
		return AS_NEWS_SECTION_KIND_FEATURES;
	if (g_strstr_len (lines, -1, "Removed features:\n") != NULL)
		return AS_NEWS_SECTION_KIND_FEATURES;
	if (g_strstr_len (lines, -1, "Specification:\n") != NULL)
		return AS_NEWS_SECTION_KIND_DOCUMENTATION;
	if (g_strstr_len (lines, -1, "Documentation:\n") != NULL)
		return AS_NEWS_SECTION_KIND_DOCUMENTATION;
	if (g_strstr_len (lines, -1, "Notes:\n") != NULL)
		return AS_NEWS_SECTION_KIND_NOTES;
	if (g_strstr_len (lines, -1, "Note:\n") != NULL)
		return AS_NEWS_SECTION_KIND_NOTES;
	if (g_strstr_len (lines, -1, "Miscellaneous:\n") != NULL)
		return AS_NEWS_SECTION_KIND_MISC;
	if (g_strstr_len (lines, -1, "Misc:\n") != NULL)
		return AS_NEWS_SECTION_KIND_MISC;
	if (g_strstr_len (lines, -1, "Translations:\n") != NULL)
		return AS_NEWS_SECTION_KIND_TRANSLATION;
	if (g_strstr_len (lines, -1, "Translation:\n") != NULL)
		return AS_NEWS_SECTION_KIND_TRANSLATION;
	if (g_strstr_len (lines, -1, "Translations\n") != NULL)
		return AS_NEWS_SECTION_KIND_TRANSLATION;
	if (g_strstr_len (lines, -1, "Contributors:\n") != NULL)
		return AS_NEWS_SECTION_KIND_CONTRIBUTORS;
	if (g_strstr_len (lines, -1, "With contributions from:\n") != NULL)
		return AS_NEWS_SECTION_KIND_CONTRIBUTORS;
	if (g_strstr_len (lines, -1, "Thanks to:\n") != NULL)
		return AS_NEWS_SECTION_KIND_CONTRIBUTORS;
	if (g_strstr_len (lines, -1, "Translators:\n") != NULL)
		return AS_NEWS_SECTION_KIND_TRANSLATORS;
	return AS_NEWS_SECTION_KIND_UNKNOWN;
}

static void
as_news_text_add_markup (GString *desc, const gchar *tag, const gchar *line)
{
	g_autofree gchar *escaped = NULL;

	/* empty line means do nothing */
	if (line != NULL && line[0] == '\0')
		return;

	if (line == NULL) {
		g_string_append_printf (desc, "<%s>\n", tag);
	} else {
		gchar *tmp;
		escaped = g_markup_escape_text (line, -1);
		tmp = g_strrstr (escaped, " (");
		if (tmp != NULL)
			*tmp = '\0';

		g_string_append_printf (desc, "<%s>%s</%s>\n",
					tag, escaped, tag);
	}
}

static gboolean
as_news_text_to_release_hdr (AsRelease *release, GString *desc, const gchar *txt, GError **error)
{
	guint i;
	const gchar *version = NULL;
	const gchar *release_txt = NULL;
	g_auto(GStrv) release_split = NULL;
	g_autoptr(GDateTime) dt = NULL;
	g_auto(GStrv) lines = NULL;
	g_autofree gchar *date_str = NULL;

	/* get info */
	lines = g_strsplit (txt, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		if (g_str_has_prefix (lines[i], "Version ")) {
			version = lines[i] + 8;
			continue;
		}
		if (g_str_has_prefix (lines[i], "Released: ")) {
			release_txt = lines[i] + 10;
			continue;
		}
	}

	/* check these exist */
	if (version == NULL) {
		g_set_error (error,
			     AS_METADATA_ERROR,
			     AS_METADATA_ERROR_FAILED,
			     "Unable to find version in: %s", txt);
		return FALSE;
	}
	if (release_txt == NULL) {
		g_set_error (error,
			     AS_METADATA_ERROR,
			     AS_METADATA_ERROR_FAILED,
			     "Unable to find release in: %s", txt);
		return FALSE;
	}

	/* apply version number */
	as_release_set_version (release, version);

	/* check if the release is unreleased */
	if ((g_strstr_len (release_txt, -1, "-xx") != NULL) ||
	    (g_strstr_len (release_txt, -1, "-XX") != NULL) ||
	    (g_strstr_len (release_txt, -1, "-??") != NULL)) {
		g_autoptr(GDateTime) dt_now = g_date_time_new_now_local ();
		date_str = g_date_time_format_iso8601 (dt_now);
		as_release_set_kind (release, AS_RELEASE_KIND_DEVELOPMENT);
		as_release_set_date (release, date_str);

		/* no further date parsing is needed at this point */
		return TRUE;
	} else {
		as_release_set_kind (release, AS_RELEASE_KIND_STABLE);
	}

	/* parse date */
	release_split = g_strsplit (release_txt, "-", -1);
	if (g_strv_length (release_split) != 3) {
		g_set_error (error,
			     AS_METADATA_ERROR,
			     AS_METADATA_ERROR_FAILED,
			     "Unable to parse release: %s", release_txt);
		return FALSE;
	}
	dt = g_date_time_new_local (atoi (release_split[0]),
				    atoi (release_split[1]),
				    atoi (release_split[2]),
				    0, 0, 0);
	if (dt == NULL) {
		g_set_error (error,
			     AS_METADATA_ERROR,
			     AS_METADATA_ERROR_FAILED,
			     "Unable to create release: %s", release_txt);
		return FALSE;
	}

	/* set release properties */
	date_str = g_strdup_printf ("%s-%s-%s", release_split[0],
						release_split[1],
						release_split[2]);
	as_release_set_date (release, date_str);

	return TRUE;
}

static gboolean
as_news_text_to_list_markup (GString *desc, gchar **lines, GError **error)
{
	guint i;

	as_news_text_add_markup (desc, "ul", NULL);
	for (i = 0; lines[i] != NULL; i++) {
		guint prefix = 0;
		g_strstrip(lines[i]);
		if ((g_str_has_prefix (lines[i], "- ")) || (g_str_has_prefix (lines[i], "* ")))
			prefix = 2;
		as_news_text_add_markup (desc, "li", lines[i] + prefix);
	}
	as_news_text_add_markup (desc, "/ul", NULL);
	return TRUE;
}

static gboolean
as_news_text_to_para_markup (GString *desc, const gchar *txt, GError **error)
{
	g_auto(GStrv) lines = NULL;
	gboolean para_generated = FALSE;

	if (g_strstr_len (txt, -1, "* ") != NULL || g_strstr_len (txt, -1, "- ") != NULL) {
		/* enumerations to paragraphs */
		lines = g_strsplit (txt, "\n", -1);
		for (guint i = 1; lines[i] != NULL; i++) {
			guint prefix = 0;
			g_strstrip (lines[i]);
			if ((g_str_has_prefix (lines[i], "- ")) || (g_str_has_prefix (lines[i], "* ")))
				prefix = 2;

			as_news_text_add_markup (desc, "p", lines[i] + prefix);
			para_generated = TRUE;
		}
	} else {
		/* freeform text to paragraphs */
		const gchar *txt_content = g_strstr_len (txt, -1, "\n");
		if (txt_content == NULL) {
			g_set_error (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "Unable to write sensible paragraph markup (missing header) for: %s.", txt);
			return FALSE;
		}
		lines = g_strsplit (txt_content, "\n\n", -1);
		for (guint i = 0; lines[i] != NULL; i++) {
			g_strstrip (lines[i]);

			as_news_text_add_markup (desc, "p", lines[i]);
			para_generated = TRUE;
		}
	}

	if (!para_generated) {
		g_set_error (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"Unable to write sensible paragraph markup (source data may be malformed) for: %s.", txt);
		return FALSE;
	}

	return TRUE;
}

/**
 * as_news_text_to_releases:
 */
static GPtrArray*
as_news_text_to_releases (const gchar *data, gint limit, GError **error)
{
	guint i;
	g_autoptr(GString) data_str = NULL;
	g_autoptr(GString) desc = NULL;
	g_auto(GStrv) split = NULL;
	g_autoptr(GPtrArray) releases = NULL;
	g_autoptr(AsRelease) rel = NULL;
	gboolean limit_reached = FALSE;

	if (data == NULL) {
		g_set_error (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"YAML news file was empty.");
		return NULL;
	}

	releases = g_ptr_array_new_with_free_func (g_object_unref);

	/* try to unsplit lines */
	data_str = g_string_new (data);
	as_gstring_replace2 (data_str, "\n   ", " ", 0);

	/* break up into sections */
	desc = g_string_new ("");
	split = g_strsplit (data_str->str, "\n\n", -1);
	for (i = 0; split[i] != NULL; i++) {
		g_auto(GStrv) lines = NULL;

		/* ignore empty sections */
		if (split[i][0] == '\0')
			continue;

		switch (as_news_text_guess_section (split[i])) {
		case AS_NEWS_SECTION_KIND_HEADER:
		{
			/* flush old release content and create new release */
			if (desc->len > 0) {
				as_release_set_description (rel, desc->str, "C");
				g_string_truncate (desc, 0);
			}
			if (rel != NULL) {
				g_ptr_array_add (releases, g_steal_pointer (&rel));
				if (limit > 0 && releases->len >= (guint) limit)
					limit_reached = TRUE;
			}
			rel = as_release_new ();

			/* parse header */
			if (!as_news_text_to_release_hdr (rel, desc, split[i], error)) {
				g_prefix_error (error,
						"Unable to parse NEWS header '%s': ",
						split[i]);
				return NULL;
			}
			break;
		}
		case AS_NEWS_SECTION_KIND_BUGFIX:
			lines = g_strsplit (split[i], "\n", -1);
			if (g_strv_length (lines) == 2) {
				as_news_text_add_markup (desc, "p",
							 "This release fixes the following bug:");
			} else {
				as_news_text_add_markup (desc, "p",
							 "This release fixes the following bugs:");
			}
			if (!as_news_text_to_list_markup (desc, lines + 1, error))
				return FALSE;
			break;
		case AS_NEWS_SECTION_KIND_NOTES:
			if (!as_news_text_to_para_markup (desc, split[i], error))
				return FALSE;
			break;
		case AS_NEWS_SECTION_KIND_FEATURES:
			lines = g_strsplit (split[i], "\n", -1);
			if (g_strv_length (lines) == 2) {
				as_news_text_add_markup (desc, "p",
							 "This release adds the following feature:");
			} else {
				as_news_text_add_markup (desc, "p",
							 "This release adds the following features:");
			}
			if (!as_news_text_to_list_markup (desc, lines + 1, error))
				return FALSE;
			break;
		case AS_NEWS_SECTION_KIND_MISC:
			lines = g_strsplit (split[i], "\n", -1);
			if (g_strv_length (lines) == 2) {
				as_news_text_add_markup (desc, "p",
							 "This release includes the following change:");
			} else {
				as_news_text_add_markup (desc, "p",
							 "This release includes the following changes:");
			}
			if (!as_news_text_to_list_markup (desc, lines + 1, error))
				return FALSE;
			break;
		case AS_NEWS_SECTION_KIND_DOCUMENTATION:
			lines = g_strsplit (split[i], "\n", -1);
			as_news_text_add_markup (desc, "p",
						 "This release updates documentation:");
			if (!as_news_text_to_list_markup (desc, lines + 1, error))
				return FALSE;
			break;
		case AS_NEWS_SECTION_KIND_TRANSLATION:
			as_news_text_add_markup (desc, "p",
						 "This release updates translations.");
			break;
		case AS_NEWS_SECTION_KIND_CONTRIBUTORS:
			as_news_text_add_markup (desc, "p",
						 "With contributions from:");

			if (g_strstr_len (split[i], -1, "* ") != NULL ||
			    g_strstr_len (split[i], -1, "- ") != NULL) {
				lines = g_strsplit (split[i], "\n", -1);
				if (!as_news_text_to_list_markup (desc, lines + 1, error))
					return FALSE;
			} else {
				if (!as_news_text_to_para_markup (desc, split[i], error))
					return FALSE;
			}
			break;
		case AS_NEWS_SECTION_KIND_TRANSLATORS:
			as_news_text_add_markup (desc, "p",
						 "Updated localization by:");

			if (g_strstr_len (split[i], -1, "* ") != NULL ||
			    g_strstr_len (split[i], -1, "- ") != NULL) {
				lines = g_strsplit (split[i], "\n", -1);
				if (!as_news_text_to_list_markup (desc, lines + 1, error))
					return FALSE;
			} else {
				if (!as_news_text_to_para_markup (desc, split[i], error))
					return FALSE;
			}
			break;
		default:
			g_set_error (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "Failed to detect section '%s'", split[i]);
			return FALSE;
		}

		if (limit_reached)
			break;
	}

	/* flush old release content */
	if (desc->len > 0 && !limit_reached) {
		as_release_set_description (rel, desc->str, "C");
		g_ptr_array_add (releases, g_steal_pointer (&rel));
	}

	return g_steal_pointer (&releases);
}

/**
 * as_news_releases_to_text:
 */
static gboolean
as_news_releases_to_text (GPtrArray *releases, gchar **md_data)
{
	g_autoptr(GString) str = NULL;

	str = g_string_new ("");
	for (guint i = 0; i < releases->len; i++) {

		const gchar *tmp;
		g_autofree gchar *version = NULL;
		g_autofree gchar *date = NULL;
		g_autoptr(GDateTime) dt = NULL;
		AsRelease *rel = AS_RELEASE (g_ptr_array_index (releases, i));

		/* write version with underline */
		version = g_strdup_printf ("Version %s", as_release_get_version (rel));
		g_string_append_printf (str, "%s\n", version);
		for (guint j = 0; version[j] != '\0'; j++)
			g_string_append (str, "~");
		g_string_append (str, "\n");

		/* write release */
		if (as_release_get_timestamp (rel) > 0) {
			dt = g_date_time_new_from_unix_utc ((gint64) as_release_get_timestamp (rel));
			date = g_date_time_format (dt, "%F");
			g_string_append_printf (str, "Released: %s\n\n", date);
		}

		/* transform description */
		tmp = as_release_get_description (rel);
		if (tmp != NULL) {
			g_autofree gchar *md = NULL;
			md = as_description_markup_convert (tmp,
							    AS_MARKUP_KIND_MARKDOWN,
							    NULL);
			if (md == NULL)
				return FALSE;
			g_string_append_printf (str, "%s\n", md);
		}
		g_string_append (str, "\n");
	}
	if (str->len > 1)
		g_string_truncate (str, str->len - 1);

	*md_data = g_string_free (str, FALSE);
	str = NULL;
	return TRUE;
}

/**
 * as_news_to_releases_from_data:
 *
 * Convert NEWS data to a list of AsRelease elements.
 */
GPtrArray*
as_news_to_releases_from_data (const gchar *data,
			       AsNewsFormatKind kind,
			       gint entry_limit,
			       gint translatable_limit,
			       GError **error)
{
	GPtrArray *releases = NULL;

	if (kind == AS_NEWS_FORMAT_KIND_YAML)
		releases = as_news_yaml_to_releases (data, entry_limit, error);
	if (kind == AS_NEWS_FORMAT_KIND_TEXT)
		releases = as_news_text_to_releases (data, entry_limit, error);

	if (releases == NULL) {
		/* if no error was set, we simply had no idea about the format.
		 * Otherwise, parsing must have failed. */
		if (error == NULL)
			g_set_error (error,
					AS_METADATA_ERROR,
					AS_METADATA_ERROR_FAILED,
					"Unable to detect input data format.");
		return NULL;
	}

	/* trim release entries to the desired size */
	if (entry_limit > 0 && (guint) entry_limit < releases->len)
		g_ptr_array_remove_range (releases, entry_limit, releases->len - entry_limit);

	/* mark only the desired amount of stuff as translatable */
	if (translatable_limit >= 0) {
		for (guint i = 0; i < releases->len; i++) {
			AsRelease *release = AS_RELEASE (g_ptr_array_index (releases, i));
			if (i >= (guint) translatable_limit)
				as_release_set_description_translatable (release, FALSE);
		}
	}

	return releases;
}

/**
 * as_news_to_releases_from_filename:
 *
 * Convert NEWS file to a list of AsRelease elements.
 */
GPtrArray*
as_news_to_releases_from_filename (const gchar *fname,
				   AsNewsFormatKind kind,
				   gint entry_limit,
				   gint translatable_limit,
				   GError **error)
{
	g_autofree gchar *data = NULL;

	/* try to guess what kind of file we are dealing with, assume YAML if detection fails */
	if (kind == AS_NEWS_FORMAT_KIND_UNKNOWN) {
		if (g_str_has_suffix (fname, ".yml") || g_str_has_suffix (fname, ".yaml"))
			kind = AS_NEWS_FORMAT_KIND_YAML;
		else if (g_str_has_suffix (fname, "NEWS") || g_str_has_suffix (fname, ".txt") || g_str_has_suffix (fname, "news"))
			kind = AS_NEWS_FORMAT_KIND_TEXT;
		else
			kind = AS_NEWS_FORMAT_KIND_YAML;
	}

	/* load data from file */
	if (!g_file_get_contents (fname, &data, NULL, error))
		return NULL;

	return as_news_to_releases_from_data (data,
					      kind,
					      entry_limit,
					      translatable_limit,
					      error);
}

/**
 * as_releases_to_news_data:
 *
 * Convert a list of releases to a text representation.
 */
gboolean
as_releases_to_news_data (GPtrArray *releases, AsNewsFormatKind kind, gchar **news_data, GError **error)
{
	if (kind == AS_NEWS_FORMAT_KIND_YAML)
		return as_news_releases_to_yaml (releases, news_data);

	if (kind == AS_NEWS_FORMAT_KIND_TEXT)
		return as_news_releases_to_text (releases, news_data);

	g_set_error (error,
			AS_METADATA_ERROR,
			AS_METADATA_ERROR_FAILED,
			"Unable to detect input data format.");
	return FALSE;
}

/**
 * as_releases_to_news_file:
 *
 * Convert a list of releases to a text representation and save it to a file.
 */
gboolean
as_releases_to_news_file (GPtrArray *releases, const gchar *fname, AsNewsFormatKind kind, GError **error)
{
	g_autofree gchar *data = NULL;

	/* try to guess what kind of file we are supposed to be writing */
	if (kind == AS_NEWS_FORMAT_KIND_UNKNOWN) {
		if (g_str_has_suffix (fname, ".yml") || g_str_has_suffix (fname, ".yaml"))
			kind = AS_NEWS_FORMAT_KIND_YAML;
		else if (g_str_has_suffix (fname, "NEWS") || g_str_has_suffix (fname, ".txt") || g_str_has_suffix (fname, "news"))
			kind = AS_NEWS_FORMAT_KIND_TEXT;
		else
			kind = AS_NEWS_FORMAT_KIND_YAML;
	}

	if (!as_releases_to_news_data (releases, kind, &data, error))
		return FALSE;

	return g_file_set_contents (fname, data, -1, error);
}
