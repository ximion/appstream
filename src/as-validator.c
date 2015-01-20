/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-metadata.h"
#include "as-metadata-private.h"

#include "as-utils.h"
#include "as-component.h"
#include "as-component-private.h"

typedef struct _AsValidatorPrivate	AsValidatorPrivate;
struct _AsValidatorPrivate
{
	GHashTable *issues; /* of utf8 string->AsValidatorIssue objects */
};

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
}

/**
 * as_validator_add_issue:
 **/
static void
as_validator_add_issue (AsValidator *validator, AsComponent *cpt, AsIssueImportance importance, AsIssueKind kind, const gchar *format, ...)
{
	va_list args;
	gchar *buffer;
	gchar *str;
	AsValidatorIssue *issue;
	AsValidatorPrivate *priv = GET_PRIVATE (validator);

	va_start (args, format);
	buffer = g_strdup_vprintf (format, args);
	va_end (args);

	if (cpt == NULL)
		str = g_strdup_printf ("[<root>] %s", buffer);
	else if (as_str_empty (as_component_get_id (cpt)))
		str = g_strdup_printf ("[???] %s", buffer);
	else
		str = g_strdup_printf ("[%s] %s", as_component_get_id (cpt), buffer);
	g_free (buffer);

	issue = as_validator_issue_new ();
	as_validator_issue_set_kind (issue, kind);
	as_validator_issue_set_importance (issue, importance);
	as_validator_issue_set_message (issue, str);

	/* str ownership is transferred to the hashtable */
	g_hash_table_insert (priv->issues, str, issue);
}

/**
 * as_validator_clear_issues:
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
			cpt,
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
				cpt,
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
				cpt,
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
				cpt,
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

		as_validator_check_content_empty (validator,
								node_content,
								node_name,
								AS_ISSUE_IMPORTANCE_WARNING,
								cpt);

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
					cpt,
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
									"description/ul",
									cpt,
									"The '%s' tag should not be localized in distro metadata. Localize the whole 'description' tag instead.");
			}
			as_validator_check_children_quick (validator, iter, "li", cpt);
		} else {
			as_validator_add_issue (validator,
				cpt,
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
				cpt,
				AS_ISSUE_IMPORTANCE_ERROR,
				AS_ISSUE_KIND_TAG_DUPLICATED,
				"The tag '%s' appears multiple times, while it should only be defined once per component.",
				node_name);
	}
}

/**
 * as_validator_validate_component_node:
 **/
static void
as_validator_validate_component_node (AsValidator *validator, xmlNode *root, AsParserMode mode)
{
	gchar *cpttype;
	xmlNode *iter;
	AsMetadata *metad;
	AsComponent *cpt;
	gchar *metadata_license = NULL;
	gboolean provides_found = FALSE;
	GHashTable *found_tags;
	/* AsValidatorPrivate *priv = GET_PRIVATE (validator); */

	found_tags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* validate the resulting AsComponent for sanity */
	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "C");
	as_metadata_set_parser_mode (metad, mode);

	cpt = as_metadata_parse_component_node (metad, root, TRUE, NULL);
	g_object_unref (metad);
	g_assert (cpt != NULL);

	/* check if component type is valid */
	cpttype = (gchar*) xmlGetProp (root, (xmlChar*) "type");
	if (cpttype != NULL) {
		if (as_component_kind_from_string (cpttype) == AS_COMPONENT_KIND_UNKNOWN) {
			as_validator_add_issue (validator,
					cpt,
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
					cpt,
					AS_ISSUE_IMPORTANCE_INFO,
					AS_ISSUE_KIND_PROPERTY_INVALID,
					"The id tag for \"%s\" still contains a 'type' property, probably from an old conversion.",
					node_content);
			}
			g_free (prop);
			if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
				if (!g_str_has_suffix (node_content, ".desktop"))
					as_validator_add_issue (validator,
						cpt,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_VALUE_WRONG,
						"Component id belongs to a desktop-application, but doesn't resemble the .desktop file name: \"%s\"",
						node_content);
			}
		} else if (g_strcmp0 (node_name, "metadata_license") == 0) {
			metadata_license = g_strdup (node_content);
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "pkgname") == 0) {
			if (g_hash_table_contains (found_tags, node_name)) {
				as_validator_add_issue (validator,
					cpt,
					AS_ISSUE_IMPORTANCE_PEDANTIC,
					AS_ISSUE_KIND_TAG_DUPLICATED,
					"The 'pkgname' tag appears multiple times. You can maybe create a metapackage containing the data in order to get rid of defining multiple package names per component.");
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
						cpt,
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
						cpt,
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
			provides_found = TRUE;
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
		} else if (g_strcmp0 (node_name, "extends") == 0) {
		} else if (g_strcmp0 (node_name, "update_contact") == 0) {
			if (mode == AS_PARSER_MODE_DISTRO) {
				as_validator_add_issue (validator,
					cpt,
					AS_ISSUE_IMPORTANCE_WARNING,
					AS_ISSUE_KIND_TAG_NOT_ALLOWED,
					"The 'update_contact' tag should not be included in distro AppStream XML.");
			} else {
				as_validator_check_appear_once (validator, iter, found_tags, cpt);
			}
		} else if (g_strcmp0 (node_name, "metadata") == 0) {
			as_validator_add_issue (validator,
				cpt,
				AS_ISSUE_IMPORTANCE_PEDANTIC,
				AS_ISSUE_KIND_TAG_UNKNOWN,
				"Found custom metadata in <metadata/> tag. Use of this tag is common, but should be avoided if possible.");
		} else if (!g_str_has_prefix (node_name, "x-")) {
			as_validator_add_issue (validator,
				cpt,
				AS_ISSUE_IMPORTANCE_WARNING,
				AS_ISSUE_KIND_TAG_UNKNOWN,
				"Found invalid tag: '%s'. Non-standard tags have to be prefixed with \"x-\".",
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
				cpt,
				AS_ISSUE_IMPORTANCE_ERROR,
				AS_ISSUE_KIND_TAG_MISSING,
				"The essential tag 'metadata_license' is missing.");
	} else {
		g_free (metadata_license);
	}

	if ((!provides_found) && (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP)) {
		as_validator_add_issue (validator,
					cpt,
					AS_ISSUE_IMPORTANCE_PEDANTIC,
					AS_ISSUE_KIND_TAG_MISSING,
					"Component describes a desktop-application, but has no 'provides' tag. It should at least define a 'binary' as public interface.");
	}

	if (as_component_get_extends (cpt)->len > 0) {
		if (as_component_get_kind (cpt) != AS_COMPONENT_KIND_ADDON)
			as_validator_add_issue (validator,
				cpt,
				AS_ISSUE_IMPORTANCE_ERROR,
				AS_ISSUE_KIND_TAG_NOT_ALLOWED,
				"An 'extends' tag is specified, but the component is not an addon.");
	} else {
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_ADDON)
			as_validator_add_issue (validator,
				cpt,
				AS_ISSUE_IMPORTANCE_ERROR,
				AS_ISSUE_KIND_TAG_MISSING,
				"The component is an addon, but no 'extends' tag was specified.");
	}

	if (cpt != NULL)
		g_object_unref (cpt);
}

/**
 * as_validator_validate_file:
 *
 * Validate an AppStream XML file
 **/
gboolean
as_validator_validate_file (AsValidator *validator,
							GFile* metadata_file)
{
	gboolean ret;
	gchar* xml_doc;
	gchar* line = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;

	xml_doc = g_strdup ("");
	ir = g_file_read (metadata_file, NULL, NULL);
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

	ret = as_validator_validate_data (validator, xml_doc);
	g_object_unref (dis);
	g_free (xml_doc);

	return ret;
}

/**
 * as_validator_validate_data:
 *
 * Validate AppStream XML data
 **/
gboolean
as_validator_validate_data (AsValidator *validator,
							const gchar *metadata)
{
	gboolean ret;
	xmlDoc* doc;
	xmlNode* root;

	/* cleanup */
	as_validator_clear_issues (validator);

	doc = xmlParseDoc ((xmlChar*) metadata);
	if (doc == NULL) {
		as_validator_add_issue (validator,
			NULL,
			AS_ISSUE_IMPORTANCE_ERROR,
			AS_ISSUE_KIND_MARKUP_INVALID,
			"Could not parse XML data.");
		return FALSE;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL) {
		as_validator_add_issue (validator,
			NULL,
			AS_ISSUE_IMPORTANCE_ERROR,
			AS_ISSUE_KIND_MARKUP_INVALID,
			"The XML document is empty.");
		return FALSE;
	}

	ret = TRUE;

	if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
		as_validator_validate_component_node (validator,
										root,
										AS_PARSER_MODE_UPSTREAM);
	} else if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
		xmlNode *iter;
		const gchar *node_name;
		for (iter = root->children; iter != NULL; iter = iter->next) {
			/* discard spaces */
			if (iter->type != XML_ELEMENT_NODE)
				continue;
			node_name = (const gchar*) iter->name;
			if (g_strcmp0 (node_name, "component") == 0) {
				as_validator_validate_component_node (validator,
												iter,
												AS_PARSER_MODE_DISTRO);
			} else {
				as_validator_add_issue (validator,
					NULL,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_UNKNOWN,
					"Unknown tag found: %s",
					node_name);
				ret = FALSE;
			}
		}
	} else if (g_str_has_prefix ((gchar*) root->name, "application")) {
		as_validator_add_issue (validator,
				NULL,
				AS_ISSUE_IMPORTANCE_ERROR,
				AS_ISSUE_KIND_LEGACY,
				"Your file is in a legacy AppStream format, which can not be validated. Please migrate it to spec version 0.6 or above.");
		ret = FALSE;
	}

	xmlFreeDoc (doc);
	return ret;
}

/**
 * as_validator_get_issues:
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
