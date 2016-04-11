/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-validator
 * @short_description: Validator and report-generator about AppStream XML metadata
 * @include: appstream.h
 *
 * This object is able to validate AppStream XML metadata (distro and upstream)
 * and to generate a report about issues found with it.
 *
 * See also: #AsMetadata
 */

#include <config.h>
#include <glib.h>
#include <gio/gio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <string.h>

#include "as-validator.h"
#include "as-validator-issue.h"

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-xmldata.h"
#include "as-component.h"
#include "as-component-private.h"

typedef struct
{
	GHashTable *issues; /* of utf8:AsValidatorIssue */

	AsComponent *current_cpt;
	gchar *current_fname;
} AsValidatorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsValidator, as_validator, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_validator_get_instance_private (o))

/**
 * as_validator_finalize:
 **/
static void
as_validator_finalize (GObject *object)
{
	AsValidator *validator = AS_VALIDATOR (object);
	AsValidatorPrivate *priv = GET_PRIVATE (validator);

	g_hash_table_unref (priv->issues);
	g_free (priv->current_fname);
	if (priv->current_cpt != NULL)
		g_object_unref (priv->current_cpt);

	G_OBJECT_CLASS (as_validator_parent_class)->finalize (object);
}

/**
 * as_validator_init:
 **/
static void
as_validator_init (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);

	priv->issues = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	priv->current_fname = NULL;
	priv->current_cpt = NULL;
}

/**
 * as_validator_add_issue:
 **/
static void
as_validator_add_issue (AsValidator *validator, AsIssueImportance importance, AsIssueKind kind, const gchar *format, ...)
{
	va_list args;
	gchar *buffer;
	gchar *str;
	gchar *id_str;
	g_autofree gchar *fname = NULL;
	AsValidatorIssue *issue;
	AsValidatorPrivate *priv = GET_PRIVATE (validator);

	va_start (args, format);
	buffer = g_strdup_vprintf (format, args);
	va_end (args);

	issue = as_validator_issue_new ();
	as_validator_issue_set_kind (issue, kind);
	as_validator_issue_set_importance (issue, importance);
	as_validator_issue_set_message (issue, buffer);
	g_free (buffer);

	/* find location */
	if (priv->current_fname == NULL)
		fname = g_strdup ("<unknown>");
	else
		fname = g_strdup (priv->current_fname);

	if (priv->current_cpt == NULL)
		str = g_strdup_printf ("%s:<root>", fname);
	else if (as_str_empty (as_component_get_id (priv->current_cpt)))
		str = g_strdup_printf ("%s:???", fname);
	else
		str = g_strdup_printf ("%s:%s",
					fname,
					as_component_get_id (priv->current_cpt));
	as_validator_issue_set_location (issue, str);
	g_free (str);

	id_str = g_strdup_printf ("%s - %s",
					as_validator_issue_get_location (issue),
					as_validator_issue_get_message (issue));
	/* str ownership is transferred to the hashtable */
	g_hash_table_insert (priv->issues, id_str, issue);
}

/**
 * as_validator_set_current_fname:
 *
 * Sets the name of the file we are currently dealing with.
 **/
static void
as_validator_set_current_fname (AsValidator *validator, const gchar *fname)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	g_free (priv->current_fname);
	priv->current_fname = g_strdup (fname);
}

/**
 * as_validator_clear_current_fname:
 *
 * Clears the current filename.
 **/
static void
as_validator_clear_current_fname (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	g_free (priv->current_fname);
	priv->current_fname = NULL;
}

/**
 * as_validator_set_current_cpt:
 *
 * Sets the #AsComponent we are currently analyzing.
 **/
static void
as_validator_set_current_cpt (AsValidator *validator, AsComponent *cpt)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	if (priv->current_cpt != NULL)
		g_object_unref (priv->current_cpt);
	priv->current_cpt = g_object_ref (cpt);
}

/**
 * as_validator_clear_current_cpt:
 *
 * Clears the current component.
 **/
static void
as_validator_clear_current_cpt (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	if (priv->current_cpt != NULL)
		g_object_unref (priv->current_cpt);
	priv->current_cpt = NULL;
}

/**
 * as_validator_clear_issues:
 * @validator: An instance of #AsValidator.
 *
 * Clears the list of issues
 **/
void
as_validator_clear_issues (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	g_hash_table_remove_all (priv->issues);
}

/**
 * as_validator_check_type_property:
 **/
static gchar*
as_validator_check_type_property (AsValidator *validator, AsComponent *cpt, xmlNode *node)
{
	gchar *prop;
	gchar *content;
	prop = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	content = (gchar*) xmlNodeGetContent (node);
	if (prop == NULL) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_PROPERTY_MISSING,
					"'%s' tag has no 'type' property: %s",
					(const gchar*) node->name,
					content);
	}
	g_free (content);

	return prop;
}

/**
 * as_validator_check_content:
 **/
static void
as_validator_check_content_empty (AsValidator *validator, const gchar *content, const gchar *tag_name, AsIssueImportance importance, AsComponent *cpt)
{
	gchar *tmp;
	tmp = g_strdup (content);
	g_strstrip (tmp);
	if (!as_str_empty (tmp))
		goto out;

	/* release tags are allowed to be empty */
	if (g_str_has_prefix (tag_name, "release"))
		goto out;

	as_validator_add_issue (validator,
				importance,
				AS_ISSUE_KIND_VALUE_WRONG,
				"Found empty '%s' tag.",
				tag_name);
out:
	g_free (tmp);
}

/**
 * as_validator_check_children_quick:
 **/
static void
as_validator_check_children_quick (AsValidator *validator, xmlNode *node, const gchar *allowed_tagname, AsComponent *cpt)
{
	xmlNode *iter;

	for (iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		gchar *node_content;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;
		node_content = (gchar*) xmlNodeGetContent (iter);

		if (g_strcmp0 (node_name, allowed_tagname) == 0) {
			gchar *tag_path;
			tag_path = g_strdup_printf ("%s/%s", (const gchar*) node->name, node_name);
			as_validator_check_content_empty (validator,
								node_content,
								tag_path,
								AS_ISSUE_IMPORTANCE_WARNING,
								cpt);
			g_free (tag_path);
		} else {
			as_validator_add_issue (validator,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_TAG_UNKNOWN,
						"Found tag '%s' in section '%s'. Only '%s' tags are allowed.",
						node_name,
						(const gchar*) node->name,
						allowed_tagname);
		}

		g_free (node_content);
	}
}

/**
 * as_validator_check_nolocalized:
 **/
static void
as_validator_check_nolocalized (AsValidator *validator, xmlNode* node, const gchar *node_path, AsComponent *cpt, const gchar *format)
{
	gchar *lang;

	lang = (gchar*) xmlGetProp (node, (xmlChar*) "lang");
	if (lang != NULL) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_PROPERTY_INVALID,
					format,
					node_path);
	}
	g_free (lang);
}

/**
 * as_validator_check_description_tag:
 **/
static void
as_validator_check_description_tag (AsValidator *validator, xmlNode* node, AsComponent *cpt, AsParserMode mode)
{
	xmlNode *iter;
	gchar *node_content;
	gchar *node_name;
	gboolean first_paragraph = TRUE;

	if (mode == AS_PARSER_MODE_UPSTREAM) {
		as_validator_check_nolocalized (validator,
						node,
						(const gchar*) node->name,
						cpt,
						"The '%s' tag should not be localized in upstream metadata. Localize the individual paragraphs instead.");
	}

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (gchar*) iter->name;
		node_content = (gchar*) xmlNodeGetContent (iter);

		if ((g_strcmp0 (node_name, "ul") != 0) && (g_strcmp0 (node_name, "ol") != 0)) {
			as_validator_check_content_empty (validator,
								node_content,
								node_name,
								AS_ISSUE_IMPORTANCE_WARNING,
								cpt);
		}

		if (g_strcmp0 (node_name, "p") == 0) {
			if (mode == AS_PARSER_MODE_DISTRO) {
				as_validator_check_nolocalized (validator,
									iter,
									"description/p",
									cpt,
									"The '%s' tag should not be localized in distro metadata. Localize the whole 'description' tag instead.");
			}
			if ((first_paragraph) && (strlen (node_content) < 100)) {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_INFO,
							AS_ISSUE_KIND_VALUE_ISSUE,
							"First 'description/p' paragraph might be too short.",
							node_content);
			}
			first_paragraph = FALSE;
		} else if (g_strcmp0 (node_name, "ul") == 0) {
			if (mode == AS_PARSER_MODE_DISTRO) {
				as_validator_check_nolocalized (validator,
								iter,
								"description/ul",
								cpt,
								"The '%s' tag should not be localized in distro metadata. Localize the whole 'description' tag instead.");
			}
			as_validator_check_children_quick (validator, iter, "li", cpt);
		} else if (g_strcmp0 (node_name, "ol") == 0) {
			if (mode == AS_PARSER_MODE_DISTRO) {
				as_validator_check_nolocalized (validator,
								iter,
								"description/ol",
								cpt,
								"The '%s' tag should not be localized in distro metadata. Localize the whole 'description' tag instead.");
			}
			as_validator_check_children_quick (validator, iter, "li", cpt);
		} else {
			as_validator_add_issue (validator,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_TAG_UNKNOWN,
						"Found tag '%s' in 'description' section. Only 'p', 'ul' and 'ol' are allowed.",
						node_name);
		}

		g_free (node_content);
	}
}

/**
 * as_validator_check_appear_once:
 **/
static void
as_validator_check_appear_once (AsValidator *validator, xmlNode *node, GHashTable *known_tags, AsComponent *cpt)
{
	gchar *lang;
	const gchar *node_name;

	/* localized tags may appear more than once, we only test the unlocalized versions */
	lang = (gchar*) xmlGetProp (node, (xmlChar*) "lang");
	if (lang != NULL) {
		g_free (lang);
		return;
	}
	node_name = (const gchar*) node->name;

	if (g_hash_table_contains (known_tags, node_name)) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_DUPLICATED,
					"The tag '%s' appears multiple times, while it should only be defined once per component.",
					node_name);
	}
}

/**
 * as_validator_validate_component_node:
 **/
static AsComponent*
as_validator_validate_component_node (AsValidator *validator, xmlNode *root, AsParserMode mode)
{
	gchar *cpttype;
	xmlNode *iter;
	AsXMLData *xdt;
	AsComponent *cpt;
	gchar *metadata_license = NULL;
	GHashTable *found_tags;
	const gchar *summary;

	found_tags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* validate the resulting AsComponent for sanity */
	xdt = as_xmldata_new ();
	as_xmldata_initialize (xdt, "C", NULL, NULL, 0);
	as_xmldata_set_parser_mode (xdt, mode);

	cpt = as_component_new ();
	as_xmldata_parse_component_node (xdt, root, cpt, NULL);
	g_object_unref (xdt);

	as_validator_set_current_cpt (validator, cpt);

	/* check if component type is valid */
	cpttype = (gchar*) xmlGetProp (root, (xmlChar*) "type");
	if (cpttype != NULL) {
		if (as_component_kind_from_string (cpttype) == AS_COMPONENT_KIND_UNKNOWN) {
			as_validator_add_issue (validator,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_VALUE_WRONG,
						"Invalid component type found: %s",
						cpttype);
		}
	}
	g_free (cpttype);

	for (iter = root->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		gchar *node_content;
		gboolean tag_valid = TRUE;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;
		node_content = (gchar*) xmlNodeGetContent (iter);

		if (g_strcmp0 (node_name, "id") == 0) {
			gchar *prop;
			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (prop != NULL) {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_INFO,
							AS_ISSUE_KIND_PROPERTY_INVALID,
							"The id tag for \"%s\" still contains a 'type' property, probably from an old conversion.",
							node_content);
			}
			g_free (prop);
			if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
				if (!g_str_has_suffix (node_content, ".desktop"))
					as_validator_add_issue (validator,
								AS_ISSUE_IMPORTANCE_WARNING,
								AS_ISSUE_KIND_VALUE_WRONG,
								"Component id belongs to a desktop-application, but does not resemble the .desktop file name: \"%s\"",
								node_content);
			}
		} else if (g_strcmp0 (node_name, "metadata_license") == 0) {
			metadata_license = g_strdup (node_content);
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "pkgname") == 0) {
			if (g_hash_table_contains (found_tags, node_name)) {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_PEDANTIC,
							AS_ISSUE_KIND_TAG_DUPLICATED,
							"The 'pkgname' tag appears multiple times. You should evaluate creating a metapackage containing the data in order to avoid defining multiple package names per component.");
			}
		} else if (g_strcmp0 (node_name, "source_pkgname") == 0) {
			if (g_hash_table_contains (found_tags, node_name)) {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_TAG_DUPLICATED,
							"The 'source_pkgname' tag appears multiple times.");
			}
		} else if (g_strcmp0 (node_name, "name") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "summary") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "description") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			as_validator_check_description_tag (validator, iter, cpt, mode);
		} else if (g_strcmp0 (node_name, "icon") == 0) {
			gchar *prop;
			prop = as_validator_check_type_property (validator, cpt, iter);
			if ((g_strcmp0 (prop, "cached") == 0) || (g_strcmp0 (prop, "stock") == 0)) {
				if (g_strrstr (node_content, "/") != NULL)
					as_validator_add_issue (validator,
								AS_ISSUE_IMPORTANCE_ERROR,
								AS_ISSUE_KIND_VALUE_WRONG,
								"Icons of type 'stock' or 'cached' must not contain a full or relative path to the icon.");
			}
			g_free (prop);
		} else if (g_strcmp0 (node_name, "url") == 0) {
			gchar *prop;
			prop = as_validator_check_type_property (validator, cpt, iter);
			if (as_url_kind_from_string (prop) == AS_URL_KIND_UNKNOWN) {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_PROPERTY_INVALID,
							"Invalid property for 'url' tag: \"%s\"",
							prop);
			}
			g_free (prop);
		} else if (g_strcmp0 (node_name, "categories") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			as_validator_check_children_quick (validator, iter, "category", cpt);
		} else if (g_strcmp0 (node_name, "keywords") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			as_validator_check_children_quick (validator, iter, "keyword", cpt);
		} else if (g_strcmp0 (node_name, "mimetypes") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			as_validator_check_children_quick (validator, iter, "mimetype", cpt);
		} else if (g_strcmp0 (node_name, "provides") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "screenshots") == 0) {
			as_validator_check_children_quick (validator, iter, "screenshot", cpt);
		} else if (g_strcmp0 (node_name, "project_license") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "project_group") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "developer_name") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "compulsory_for_desktop") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "releases") == 0) {
			as_validator_check_children_quick (validator, iter, "release", cpt);
		} else if ((g_strcmp0 (node_name, "languages") == 0) && (mode == AS_PARSER_MODE_DISTRO)) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			as_validator_check_children_quick (validator, iter, "lang", cpt);
		} else if ((g_strcmp0 (node_name, "translation") == 0) && (mode == AS_PARSER_MODE_UPSTREAM)) {
			g_autofree gchar *prop = NULL;
			AsTranslationKind trkind;
			prop = as_validator_check_type_property (validator, cpt, iter);
			trkind = as_translation_kind_from_string (prop);
			if (trkind == AS_TRANSLATION_KIND_UNKNOWN) {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_WRONG,
							"Unknown type '%s' for <translation/> tag.", prop);
			}
		} else if (g_strcmp0 (node_name, "extends") == 0) {
		} else if (g_strcmp0 (node_name, "bundle") == 0) {
			g_autofree gchar *prop = NULL;
			prop = as_validator_check_type_property (validator, cpt, iter);
			if ((g_strcmp0 (prop, "limba") != 0) && (g_strcmp0 (prop, "xdg-app") != 0)) {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_WRONG,
							"Unknown type '%s' for <bundle/> tag.", prop);
			}
		} else if (g_strcmp0 (node_name, "update_contact") == 0) {
			if (mode == AS_PARSER_MODE_DISTRO) {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_WARNING,
							AS_ISSUE_KIND_TAG_NOT_ALLOWED,
							"The 'update_contact' tag should not be included in distro AppStream XML.");
			} else {
				as_validator_check_appear_once (validator, iter, found_tags, cpt);
			}
		} else if (g_strcmp0 (node_name, "metadata") == 0) {
			as_validator_add_issue (validator,
						AS_ISSUE_IMPORTANCE_PEDANTIC,
						AS_ISSUE_KIND_TAG_UNKNOWN,
						"Found custom metadata in <metadata/> tag. Use of this tag is common, but should be avoided if possible.");
			tag_valid = FALSE;
		} else if (!g_str_has_prefix (node_name, "x-")) {
			as_validator_add_issue (validator,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_TAG_UNKNOWN,
						"Found invalid tag: '%s'. Non-standard tags must be prefixed with \"x-\".",
				node_name);
			tag_valid = FALSE;
		}

		if (tag_valid) {
			as_validator_check_content_empty (validator,
							node_content,
							node_name,
							AS_ISSUE_IMPORTANCE_WARNING,
							cpt);
		}

		g_free (node_content);

		g_hash_table_add (found_tags, g_strdup (node_name));
	}

	g_hash_table_unref (found_tags);

	if (metadata_license == NULL) {
		if (mode == AS_PARSER_MODE_UPSTREAM)
			as_validator_add_issue (validator,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_TAG_MISSING,
						"The essential tag 'metadata_license' is missing.");
	} else {
		g_free (metadata_license);
	}

	/* check if the summary is sane */
	summary = as_component_get_summary (cpt);
	if ((summary != NULL) && ((strstr (summary, "\n") != NULL) || (strstr (summary, "\t") != NULL))) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_WRONG,
					"The summary tag must not contain tabs or linebreaks.");
	}

	/* check if we have a description */
	if (as_str_empty (as_component_get_description (cpt))) {
		AsComponentKind cpt_kind;
		cpt_kind = as_component_get_kind (cpt);

		if ((cpt_kind == AS_COMPONENT_KIND_DESKTOP_APP) ||
			(cpt_kind == AS_COMPONENT_KIND_FONT)) {
			as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_MISSING,
					"The component is missing a long description. Components of this type must have a long description.");
		} else {
			as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_INFO,
					AS_ISSUE_KIND_TAG_MISSING,
					"The component is missing a long description. It is recommended to add one.");
		}
	}

	/* validate font specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_FONT) {
		if (!g_str_has_suffix (as_component_get_id (cpt), ".font"))
			as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_WRONG,
					"Components of type 'font' must have an AppStream ID with a '.font' suffix.");
		if (as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_FONT) == NULL)
			as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_WARNING,
					AS_ISSUE_KIND_TAG_MISSING,
					"Type 'font' component, but no font information was provided via a provides/font tag.");
	}

	/* validate addon specific stuff */
	if (as_component_get_extends (cpt)->len > 0) {
		if (as_component_get_kind (cpt) != AS_COMPONENT_KIND_ADDON)
			as_validator_add_issue (validator,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_TAG_NOT_ALLOWED,
						"An 'extends' tag is specified, but the component is not an addon.");
	} else {
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_ADDON)
			as_validator_add_issue (validator,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_TAG_MISSING,
						"The component is an addon, but no 'extends' tag was specified.");
	}

	as_validator_clear_current_cpt (validator);
	return cpt;
}

/**
 * as_validator_validate_file:
 * @validator: An instance of #AsValidator.
 * @metadata_file: An AppStream XML file.
 *
 * Validate an AppStream XML file
 **/
gboolean
as_validator_validate_file (AsValidator *validator, GFile *metadata_file)
{
	gboolean ret;
	gchar* xml_doc;
	GFileInputStream* fistream;
	GFileInfo *info = NULL;
	const gchar *content_type = NULL;
	g_autofree gchar *fname = NULL;

	info = g_file_query_info (metadata_file,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
	if (info != NULL)
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	fname = g_file_get_basename (metadata_file);
	as_validator_set_current_fname (validator, fname);

	if ((g_strcmp0 (content_type, "application/gzip") == 0) || (g_strcmp0 (content_type, "application/x-gzip") == 0)) {
		GMemoryOutputStream *mem_os;
		GInputStream *conv_stream;
		GZlibDecompressor *zdecomp;
		guint8 *data;

		/* load a GZip compressed file */
		fistream = g_file_read (metadata_file, NULL, NULL);
		mem_os = (GMemoryOutputStream*) g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
		zdecomp = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
		conv_stream = g_converter_input_stream_new (G_INPUT_STREAM (fistream), G_CONVERTER (zdecomp));
		g_object_unref (zdecomp);

		g_output_stream_splice (G_OUTPUT_STREAM (mem_os), conv_stream, 0, NULL, NULL);
		data = g_memory_output_stream_get_data (mem_os);

		xml_doc = g_strdup ((const gchar*) data);

		g_object_unref (conv_stream);
		g_object_unref (mem_os);
		g_object_unref (fistream);
	} else {
		gchar *line = NULL;
		GString *str;
		GDataInputStream *dis;

		/* load a plaintext file */
		str = g_string_new ("");
		fistream = g_file_read (metadata_file, NULL, NULL);
		dis = g_data_input_stream_new ((GInputStream*) fistream);
		g_object_unref (fistream);

		while (TRUE) {
			line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
			if (line == NULL) {
				break;
			}

			g_string_append_printf (str, "%s\n", line);
		}

		xml_doc = g_string_free (str, FALSE);
		g_object_unref (dis);
	}

	ret = as_validator_validate_data (validator, xml_doc);
	g_free (xml_doc);

	as_validator_clear_current_fname (validator);

	return ret;
}

/**
 * as_validator_open_xml_document:
 */
static xmlNode*
as_validator_open_xml_document (AsValidator *validator, const gchar *xmldata, xmlDoc **xdoc)
{
	xmlDoc *doc;
	xmlNode *root = NULL;

	doc = xmlParseDoc ((xmlChar*) xmldata);
	if (doc == NULL) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_MARKUP_INVALID,
					"Could not parse XML data.");
		return NULL;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_MARKUP_INVALID,
					"The XML document is empty.");
		goto out;
	}

out:
	if (root != NULL)
		*xdoc = doc;
	else
		xmlFreeDoc (doc);
	return root;
}

/**
 * as_validator_validate_data:
 * @validator: An instance of #AsValidator.
 * @metadata: XML metadata.
 *
 * Validate AppStream XML data
 **/
gboolean
as_validator_validate_data (AsValidator *validator, const gchar *metadata)
{
	gboolean ret;
	xmlNode* root;
	xmlDoc *doc;
	AsComponent *cpt;

	root = as_validator_open_xml_document (validator, metadata, &doc);
	if (!root)
		return FALSE;


	ret = TRUE;

	if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
		cpt = as_validator_validate_component_node (validator,
							root,
							AS_PARSER_MODE_UPSTREAM);
		if (cpt != NULL)
			g_object_unref (cpt);
	} else if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
		xmlNode *iter;
		const gchar *node_name;
		for (iter = root->children; iter != NULL; iter = iter->next) {
			/* discard spaces */
			if (iter->type != XML_ELEMENT_NODE)
				continue;
			node_name = (const gchar*) iter->name;
			if (g_strcmp0 (node_name, "component") == 0) {
				cpt = as_validator_validate_component_node (validator,
									iter,
									AS_PARSER_MODE_DISTRO);
				if (cpt != NULL)
					g_object_unref (cpt);
			} else {
				as_validator_add_issue (validator,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_TAG_UNKNOWN,
							"Unknown tag found: %s",
							node_name);
				ret = FALSE;
			}
		}
	} else if (g_str_has_prefix ((gchar*) root->name, "application")) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_LEGACY,
					"The metainfo file uses an ancient version of the AppStream specification, which can not be validated. Please migrate it to version 0.6 (or higher).");
		ret = FALSE;
	}

	xmlFreeDoc (doc);
	return ret;
}

/**
 * MInfoCheckData:
 *
 * Helper for HashTable iteration
 */
struct MInfoCheckData {
	AsValidator *validator;
	GHashTable *desktop_fnames;
	gchar *apps_dir;
};

/**
 * as_matches_metainfo:
 *
 * Check if filname matches %basename.(appdata|metainfo).xml
 */
static gboolean
as_matches_metainfo (const gchar *fname, const gchar *basename)
{
	g_autofree gchar *tmp = NULL;

	tmp = g_strdup_printf ("%s.metainfo.xml", basename);
	if (g_strcmp0 (fname, tmp) == 0)
		return TRUE;
	g_free (tmp);
	tmp = g_strdup_printf ("%s.appdata.xml", basename);
	if (g_strcmp0 (fname, tmp) == 0)
		return TRUE;

	return FALSE;
}

/**
 * as_validator_analyze_component_metainfo_relation_cb:
 *
 * Helper function for GHashTable foreach iteration.
 */
static void
as_validator_analyze_component_metainfo_relation_cb (const gchar *fname, AsComponent *cpt, struct MInfoCheckData *data)
{
	gchar *tmp;

	/* if we have no component-id, we can't check anything */
	if (as_component_get_id (cpt) == NULL)
		return;

	as_validator_set_current_cpt (data->validator, cpt);
	as_validator_set_current_fname (data->validator, fname);

	/* check if the fname and the component-id match */
	tmp = g_strndup (as_component_get_id (cpt),
				g_strrstr (as_component_get_id (cpt), ".") - as_component_get_id (cpt));
	if (!as_matches_metainfo (fname, tmp)) {
		/* the name-without-type didn't match - check for the full id in the component name */
		if (!as_matches_metainfo (fname, as_component_get_id (cpt))) {
			as_validator_add_issue (data->validator,
					AS_ISSUE_IMPORTANCE_WARNING,
					AS_ISSUE_KIND_WRONG_NAME,
					"The metainfo filename does not match the component ID.");
		}
	}
	g_free (tmp);

	/* check if the referenced .desktop file exists */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
		if (g_hash_table_contains (data->desktop_fnames, as_component_get_id (cpt))) {
			g_autofree gchar *desktop_fname_full = NULL;
			g_autoptr(GKeyFile) dfile = NULL;
			GError *tmp_error = NULL;

			desktop_fname_full = g_build_filename (data->apps_dir, as_component_get_id (cpt), NULL);
			dfile = g_key_file_new ();

			g_key_file_load_from_file (dfile, desktop_fname_full, G_KEY_FILE_NONE, &tmp_error);
			if (tmp_error != NULL) {
				as_validator_add_issue (data->validator,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_READ_ERROR,
						"Unable to read associated .desktop file: %s", tmp_error->message);
				g_error_free (tmp_error);
				tmp_error = NULL;
			} else {
				/* we successfully opened the .desktop file, now perform some checks */

				/* name */
				if ((g_strcmp0 (as_component_get_name (cpt), "") == 0) &&
				    (!g_key_file_has_key (dfile, "Desktop Entry", "Name", NULL))) {
					/* we don't have a summary, and there is also none in the .desktop file - this is bad. */
					as_validator_add_issue (data->validator,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_MISSING,
							"The component is missing a name (none found in its metainfo or .desktop file)");
				}

				/* summary */
				if ((g_strcmp0 (as_component_get_summary (cpt), "") == 0) &&
				    (!g_key_file_has_key (dfile, "Desktop Entry", "Comment", NULL))) {
					/* we don't have a summary, and there is also none in the .desktop file - this is bad. */
					as_validator_add_issue (data->validator,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_MISSING,
							"The component is missing a summary (none found in its metainfo or .desktop file)");
				}
			}
		} else {
			as_validator_add_issue (data->validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_FILE_MISSING,
					"Component metadata refers to a non-existing .desktop file.");
		}
	}

	as_validator_clear_current_cpt (data->validator);
	as_validator_clear_current_fname (data->validator);
}

/**
 * as_validator_validate_tree:
 * @validator: An instance of #AsValidator.
 * @root_dir: The root directory of the filesystem tree that should be validated.
 *
 * Validate a full directory tree for issues in AppStream metadata.
 **/
gboolean
as_validator_validate_tree (AsValidator *validator, const gchar *root_dir)
{
	g_autofree gchar *metainfo_dir = NULL;
	g_autofree gchar *apps_dir = NULL;
	GPtrArray *mfiles = NULL;
	GPtrArray *dfiles = NULL;
	GHashTable *dfilenames = NULL;
	GHashTable *validated_cpts = NULL;
	guint i;
	gboolean ret = TRUE;
	struct MInfoCheckData ht_helper;

	/* cleanup */
	as_validator_clear_issues (validator);

	metainfo_dir = g_build_filename (root_dir, "usr", "share", "metainfo", NULL);
	apps_dir = g_build_filename (root_dir, "usr", "share", "applications", NULL);

	/* check if we actually have a directory which could hold metadata */
	if (!g_file_test (metainfo_dir, G_FILE_TEST_IS_DIR)) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_INFO,
					AS_ISSUE_KIND_FILE_MISSING,
					"No AppStream metadata was found.");
		goto out;
	}

	/* check if we actually have a directory which could hold application information */
	if (!g_file_test (apps_dir, G_FILE_TEST_IS_DIR)) {
		as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_PEDANTIC, /* pedantic because not everything which has metadata is an application */
					AS_ISSUE_KIND_FILE_MISSING,
					"No XDG applications directory found.");
	}

	/* holds a filename -> component mapping */
	validated_cpts = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						g_object_unref);

	/* validate all metainfo files */
	mfiles = as_utils_find_files_matching (metainfo_dir, "*.xml", FALSE, NULL);
	for (i = 0; i < mfiles->len; i++) {
		const gchar *fname;
		GFile *file;
		gchar *line = NULL;
		GString *str;
		GDataInputStream *dis;
		GFileInputStream *fistream;
		xmlNode* root;
		xmlDoc *doc;
		g_autofree gchar *fname_basename = NULL;

		fname = (const gchar*) g_ptr_array_index (mfiles, i);
		file = g_file_new_for_path (fname);
		if (!g_file_query_exists (file, NULL)) {
			g_warning ("File '%s' suddenly vanished.", fname);
			g_object_unref (file);
			continue;
		}

		fname_basename = g_path_get_basename (fname);
		as_validator_set_current_fname (validator, fname_basename);

		/* load a plaintext file */
		str = g_string_new ("");
		fistream = g_file_read (file, NULL, NULL);
		dis = g_data_input_stream_new ((GInputStream*) fistream);
		g_object_unref (fistream);

		while (TRUE) {
			line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
			if (line == NULL) {
				break;
			}

			g_string_append_printf (str, "%s\n", line);
		}
		g_object_unref (dis);
		g_object_unref (file);

		/* now read the XML */
		root = as_validator_open_xml_document (validator, str->str, &doc);
		g_string_free (str, TRUE);
		if (!root) {
			as_validator_clear_current_fname (validator);
			continue;
		}

		if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
			AsComponent *cpt;
			cpt = as_validator_validate_component_node (validator,
							root,
							AS_PARSER_MODE_UPSTREAM);
			if (cpt != NULL)
				g_hash_table_insert (validated_cpts,
							g_strdup (fname_basename),
							cpt);
		} else if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
			as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_NOT_ALLOWED,
					"The metainfo file specifies multiple components. This is not allowed.");
			ret = FALSE;
		} else if (g_str_has_prefix ((gchar*) root->name, "application")) {
			as_validator_add_issue (validator,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_LEGACY,
					"The metainfo file uses an ancient version of the AppStream specification, which can not be validated. Please migrate it to version 0.6 (or higher).");
			ret = FALSE;
		}

		as_validator_clear_current_fname (validator);
		xmlFreeDoc (doc);
	}

	/* check if we have matching .desktop files */
	dfilenames = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						NULL);
	dfiles = as_utils_find_files_matching (apps_dir, "*.desktop", FALSE, NULL);
	if (dfiles != NULL) {
		for (i = 0; i < dfiles->len; i++) {
			const gchar *fname;
			fname = (const gchar*) g_ptr_array_index (dfiles, i);
			g_hash_table_add (dfilenames,
						g_path_get_basename (fname));
		}
	}

	/* validate the component-id <-> filename relations and availability of other metadata */
	ht_helper.validator = validator;
	ht_helper.desktop_fnames = dfilenames;
	ht_helper.apps_dir = apps_dir;
	g_hash_table_foreach (validated_cpts,
				(GHFunc) as_validator_analyze_component_metainfo_relation_cb,
				&ht_helper);

out:
	if (mfiles != NULL)
		g_ptr_array_unref (mfiles);
	if (dfiles != NULL)
		g_ptr_array_unref (dfiles);
	if (dfilenames != NULL)
		g_hash_table_unref (dfilenames);
	if (validated_cpts != NULL)
		g_hash_table_unref (validated_cpts);

	return ret;
}

/**
 * as_validator_get_issues:
 * @validator: An instance of #AsValidator.
 *
 * Get a list of found metadata format issues.
 *
 * Returns: (element-type AsValidatorIssue) (transfer container): a list of #AsValidatorIssue instances, free with g_list_free()
 */
GList*
as_validator_get_issues (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	return g_hash_table_get_values (priv->issues);
}

/**
 * as_validator_class_init:
 **/
static void
as_validator_class_init (AsValidatorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_validator_finalize;
}

/**
 * as_validator_new:
 *
 * Creates a new #AsValidator.
 *
 * Returns: (transfer full): an #AsValidator
 **/
AsValidator*
as_validator_new (void)
{
	AsValidator *validator;
	validator = g_object_new (AS_TYPE_VALIDATOR, NULL);
	return AS_VALIDATOR (validator);
}
