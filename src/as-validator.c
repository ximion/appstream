/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Matthias Klumpp <matthias@tenstral.net>
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
 * This object is able to validate AppStream XML metadata (collection and metainfo)
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
#include "as-spdx.h"
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
as_validator_add_issue (AsValidator *validator, xmlNode *node, AsIssueImportance importance, AsIssueKind kind, const gchar *format, ...)
{
	va_list args;
	gchar *buffer;
	gchar *id_str;
	g_autofree gchar *location = NULL;
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

	/* update location information */
	if (priv->current_fname != NULL)
		as_validator_issue_set_filename (issue, priv->current_fname);

	if (priv->current_cpt != NULL)
		as_validator_issue_set_cid (issue, as_component_get_id (priv->current_cpt));

	if (node != NULL)
		as_validator_issue_set_line (issue, node->line);

	location = as_validator_issue_get_location (issue);
	id_str = g_strdup_printf ("%s - %s",
					location,
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
		as_validator_add_issue (validator, node,
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
as_validator_check_content_empty (AsValidator *validator, xmlNode *node, const gchar *tag_path, AsIssueImportance importance, AsComponent *cpt)
{
	g_autofree gchar *node_content = NULL;

	node_content = (gchar*) xmlNodeGetContent (node);
	g_strstrip (node_content);
	if (!as_str_empty (node_content))
		return;

	/* release tags are allowed to be empty */
	if (g_str_has_prefix (tag_path, "release"))
		return;

	as_validator_add_issue (validator, node,
				importance,
				AS_ISSUE_KIND_VALUE_WRONG,
				"Found empty '%s' tag.",
				tag_path);
}

/**
 * as_validate_has_hyperlink:
 *
 * Check if @text contains a hyperlink.
 */
static gboolean
as_validate_has_hyperlink (const gchar *text)
{
	if (text == NULL)
		return FALSE;
	if (g_strstr_len (text, -1, "http://") != NULL)
		return TRUE;
	if (g_strstr_len (text, -1, "https://") != NULL)
		return TRUE;
	if (g_strstr_len (text, -1, "ftp://") != NULL)
		return TRUE;
	return FALSE;
}

/**
 * as_validate_is_url:
 *
 * Check if @str is an URL.
 */
static gboolean
as_validate_is_url (const gchar *str)
{
	if (str == NULL)
		return FALSE;
	if (g_str_has_prefix (str, "http://"))
		return TRUE;
	if (g_str_has_prefix (str, "https://"))
		return TRUE;
	if (g_str_has_prefix (str, "ftp://"))
		return TRUE;
	return FALSE;
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
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		if (g_strcmp0 (node_name, allowed_tagname) == 0) {
			g_autofree gchar *tag_path = NULL;
			tag_path = g_strdup_printf ("%s/%s", (const gchar*) node->name, node_name);
			as_validator_check_content_empty (validator,
								iter,
								tag_path,
								AS_ISSUE_IMPORTANCE_WARNING,
								cpt);
		} else {
			as_validator_add_issue (validator, node,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_TAG_UNKNOWN,
						"Found tag '%s' in section '%s'. Only '%s' tags are allowed.",
						node_name,
						(const gchar*) node->name,
						allowed_tagname);
		}
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
		as_validator_add_issue (validator, node,
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
as_validator_check_description_tag (AsValidator *validator, xmlNode* node, AsComponent *cpt, AsFormatStyle mode)
{
	xmlNode *iter;
	gboolean first_paragraph = TRUE;

	if (mode == AS_FORMAT_STYLE_METAINFO) {
		as_validator_check_nolocalized (validator,
						node,
						(const gchar*) node->name,
						cpt,
						"The '%s' tag should not be localized in upstream metadata. Localize the individual paragraphs instead.");
	}

	for (iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name = (gchar*) iter->name;
		g_autofree gchar *node_content = (gchar*) xmlNodeGetContent (iter);

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if ((g_strcmp0 (node_name, "ul") != 0) && (g_strcmp0 (node_name, "ol") != 0)) {
			as_validator_check_content_empty (validator,
							  node,
							  node_name,
							  AS_ISSUE_IMPORTANCE_WARNING,
							  cpt);
		}

		if (g_strcmp0 (node_name, "p") == 0) {
			if (mode == AS_FORMAT_STYLE_COLLECTION) {
				as_validator_check_nolocalized (validator,
								iter,
								"description/p",
								cpt,
								"The '%s' tag should not be localized in collection metadata. Localize the whole 'description' tag instead.");
			}
			if ((first_paragraph) && (strlen (node_content) < 80)) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_INFO,
							AS_ISSUE_KIND_VALUE_ISSUE,
							"First 'description/p' paragraph might be too short (< 80 characters).",
							node_content);
			}
			first_paragraph = FALSE;
		} else if (g_strcmp0 (node_name, "ul") == 0) {
			if (mode == AS_FORMAT_STYLE_COLLECTION) {
				as_validator_check_nolocalized (validator,
								iter,
								"description/ul",
								cpt,
								"The '%s' tag should not be localized in collection metadata. Localize the whole 'description' tag instead.");
			}
			as_validator_check_children_quick (validator, iter, "li", cpt);
		} else if (g_strcmp0 (node_name, "ol") == 0) {
			if (mode == AS_FORMAT_STYLE_COLLECTION) {
				as_validator_check_nolocalized (validator,
								iter,
								"description/ol",
								cpt,
								"The '%s' tag should not be localized in collection metadata. Localize the whole 'description' tag instead.");
			}
			as_validator_check_children_quick (validator, iter, "li", cpt);
		} else {
			as_validator_add_issue (validator, iter,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_TAG_UNKNOWN,
						"Found tag '%s' in 'description' section. Only 'p', 'ul' and 'ol' are allowed.",
						node_name);
		}

		if (as_validate_has_hyperlink (node_content)) {
			as_validator_add_issue (validator, iter,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_VALUE_WRONG,
						"The description contains an URL. This is not allowed, please use the <url/> tag to share links.",
						node_name);
		}
	}
}

/**
 * as_validator_check_appear_once:
 **/
static void
as_validator_check_appear_once (AsValidator *validator, xmlNode *node, GHashTable *known_tags, AsComponent *cpt)
{
	g_autofree gchar *lang = NULL;
	gchar *tag_id;
	const gchar *node_name;

	/* generate tag-id to make a unique identifier for localized and unlocalized tags */
	node_name = (const gchar*) node->name;
	lang = (gchar*) xmlGetProp (node, (xmlChar*) "lang");
	if (lang == NULL)
		tag_id = g_strdup (node_name);
	else
		tag_id = g_strdup_printf ("%s (lang=%s)", node_name, lang);

	if (g_hash_table_contains (known_tags, tag_id)) {
		as_validator_add_issue (validator, node,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_DUPLICATED,
					"The tag '%s' appears multiple times, while it should only be defined once per component.",
					tag_id);
	}

	/* add to list of known tags (takes ownership/frees tag_id) */
	g_hash_table_add (known_tags, tag_id);
}

/**
 * as_validator_validate_component_id:
 *
 * Validate the component-ID.
 */
static void
as_validator_validate_component_id (AsValidator *validator, xmlNode *idnode, AsComponent *cpt)
{
	guint i;
	g_auto(GStrv) cid_parts = NULL;
	g_autofree gchar *cid = (gchar*) xmlNodeGetContent (idnode);

	cid_parts = g_strsplit (cid, ".", 3);
	if (g_strv_length (cid_parts) != 3) {
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
			/* since the ID and .desktop-file-id are tied together, we can't make this an error for desktop apps */
			as_validator_add_issue (validator, idnode,
					AS_ISSUE_IMPORTANCE_WARNING,
					AS_ISSUE_KIND_VALUE_WRONG,
					"The component ID is not a reverse domain-name. Please update the ID and that of the accompanying .desktop file to follow the latest version of the Desktop-Entry and AppStream specifications and avoid future issues.");
		} else {
			/* anything which isn't a .desktop app must follow the schema though */
			as_validator_add_issue (validator, idnode,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_WRONG,
					"The component ID is no reverse domain-name.");
		}
	} else {
		/* some people just add random dots to their ID - check if we have an actual known TLD as first part, to be more certain that this is a reverse domain name
		 * (this issue happens quite often with old .desktop files) */
		if (!as_utils_is_tld (cid_parts[0])) {
			as_validator_add_issue (validator, idnode,
						AS_ISSUE_IMPORTANCE_INFO,
						AS_ISSUE_KIND_VALUE_WRONG,
						"The component ID might not follow the reverse domain-name schema (we do not know about the TLD '%s').", cid_parts[0]);
		}
	}

	/* validate characters in AppStream ID */
	for (i = 0; cid[i] != '\0'; i++) {
		/* check if we have a printable, alphanumeric ASCII character or a dot, hyphen or underscore */
		if ((!g_ascii_isalnum (cid[i])) &&
		    (cid[i] != '.') &&
		    (cid[i] != '-') &&
		    (cid[i] != '_')) {
			g_autofree gchar *c = NULL;
			c = g_utf8_substring (cid, i, i + 1);
			as_validator_add_issue (validator, idnode,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_WRONG,
					"The component ID [%s] contains an invalid character: '%s'", cid, c);
		}
	}

	/* project-group specific constraints on the ID */
	if ((g_strcmp0 (as_component_get_project_group (cpt), "Freedesktop") == 0) ||
	    (g_strcmp0 (as_component_get_project_group (cpt), "FreeDesktop") == 0)) {
		if (!g_str_has_prefix (cid, "org.freedesktop."))
			as_validator_add_issue (validator, idnode,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_VALUE_WRONG,
						"The component is part of the Freedesktop project, but its id does not start with fd.o's reverse-DNS name (\"org.freedesktop\").");
	} else if (g_strcmp0 (as_component_get_project_group (cpt), "KDE") == 0) {
		if (!g_str_has_prefix (cid, "org.kde."))
			as_validator_add_issue (validator, idnode,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_VALUE_WRONG,
						"The component is part of the KDE project, but its id does not start with KDEs reverse-DNS name (\"org.kde\").");
	} else if (g_strcmp0 (as_component_get_project_group (cpt), "GNOME") == 0) {
		if (!g_str_has_prefix (cid, "org.gnome."))
			as_validator_add_issue (validator, idnode,
						AS_ISSUE_IMPORTANCE_PEDANTIC,
						AS_ISSUE_KIND_VALUE_WRONG,
						"The component is part of the GNOME project, but its id does not start with GNOMEs reverse-DNS name (\"org.gnome\").");
	}
}

/**
 * as_validator_validate_project_license:
 */
static void
as_validator_validate_project_license (AsValidator *validator, xmlNode *license_node)
{
	guint i;
	g_auto(GStrv) licenses = NULL;
	g_autofree gchar *license_id = (gchar*) xmlNodeGetContent (license_node);

	licenses = as_spdx_license_tokenize (license_id);
	if (licenses == NULL) {
		as_validator_add_issue (validator, license_node,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_WRONG,
					"SPDX license expression '%s' could not be parsed.",
					license_id);
		return;
	}
	for (i = 0; licenses[i] != NULL; i++) {
		if (g_strcmp0 (licenses[i], "&") == 0 ||
		    g_strcmp0 (licenses[i], "|") == 0 ||
		    g_strcmp0 (licenses[i], "+") == 0 ||
		    g_strcmp0 (licenses[i], "(") == 0 ||
		    g_strcmp0 (licenses[i], ")") == 0)
			continue;
		if (licenses[i][0] != '@' ||
		    !as_is_spdx_license_id (licenses[i] + 1)) {
			as_validator_add_issue (validator, license_node,
					AS_ISSUE_IMPORTANCE_WARNING,
					AS_ISSUE_KIND_VALUE_WRONG,
					"SPDX license ID '%s' is unknown.",
					licenses[i]);
			return;
		}
	}
}

/**
 * as_validator_validate_update_contact:
 */
static void
as_validator_validate_update_contact (AsValidator *validator, xmlNode *uc_node)
{
	g_autofree gchar *text = (gchar*) xmlNodeGetContent (uc_node);

	if ((g_strstr_len (text, -1, "@") == NULL) &&
	    (g_strstr_len (text, -1, "_at_") == NULL) &&
	    (g_strstr_len (text, -1, "_AT_") == NULL)) {
		if (g_strstr_len (text, -1, ".") == NULL) {
			as_validator_add_issue (validator, uc_node,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_VALUE_WRONG,
						"The update-contact '%s' does not appear to be a valid email address.",
						text);
		}
	}
}

/**
 * as_validator_validate_component_node:
 **/
static AsComponent*
as_validator_validate_component_node (AsValidator *validator, AsXMLData *xdt, xmlNode *root)
{
	xmlNode *iter;
	AsComponent *cpt;
	g_autofree gchar *cpttype = NULL;
	g_autoptr(GHashTable) found_tags = NULL;

	AsFormatStyle mode;
	gboolean has_metadata_license = FALSE;

	found_tags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	mode = as_xmldata_get_format_style (xdt);

	/* validate the resulting AsComponent for sanity */
	cpt = as_component_new ();
	as_xmldata_parse_component_node (xdt, root, cpt, NULL);
	as_validator_set_current_cpt (validator, cpt);

	/* check if component type is valid */
	cpttype = (gchar*) xmlGetProp (root, (xmlChar*) "type");
	if (cpttype != NULL) {
		if (as_component_kind_from_string (cpttype) == AS_COMPONENT_KIND_UNKNOWN) {
			as_validator_add_issue (validator, root,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_VALUE_WRONG,
						"Invalid component type found: %s",
						cpttype);
		}
	}

	if ((as_component_get_priority (cpt) != 0) && (mode == AS_FORMAT_STYLE_METAINFO)) {
		as_validator_add_issue (validator, root,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_WRONG,
					"The component has a priority value set. This is not allowed in metainfo files.");
	}

	if ((as_component_get_merge_kind (cpt) != AS_MERGE_KIND_NONE) && (mode == AS_FORMAT_STYLE_METAINFO)) {
		as_validator_add_issue (validator, root,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_WRONG,
					"The component has a 'merge' method defined. This is not allowed in metainfo files.");
	}

	/* the component must have a name */
	if (as_str_empty (as_component_get_name (cpt))) {
		/* we don't have a summary */
		as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_MISSING,
					"The component is missing a name (<name/> tag).");
		}

	/* the component must have a summary */
	if (as_str_empty (as_component_get_summary (cpt))) {
		/* we don't have a summary */
		as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_VALUE_MISSING,
					"The component is missing a summary (<summary/> tag).");
	}

	for (iter = root->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		g_autofree gchar *node_content = NULL;
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
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_INFO,
							AS_ISSUE_KIND_PROPERTY_INVALID,
							"The id tag for \"%s\" still contains a 'type' property, probably from an old conversion.",
							node_content);
			}
			g_free (prop);

			/* validate the AppStream ID */
			as_validator_validate_component_id (validator, iter, cpt);
		} else if (g_strcmp0 (node_name, "metadata_license") == 0) {
			has_metadata_license = TRUE;
			as_validator_check_appear_once (validator, iter, found_tags, cpt);

			/* the license must allow easy mixing of metadata in metainfo files */
			if (mode == AS_FORMAT_STYLE_METAINFO) {
				if (!as_license_is_metadata_license (node_content)) {
					as_validator_add_issue (validator, iter,
								AS_ISSUE_IMPORTANCE_WARNING,
								AS_ISSUE_KIND_VALUE_WRONG,
								"The metadata itself does not seem to be licensed under a permissive license. Please license the data under a permissive license, like FSFAP, CC-0-1.0 or MIT "
								"to allow distributors to include it in mixed data collections without the risk of license violations due to mutually incompatible licenses.");
				}
			}
		} else if (g_strcmp0 (node_name, "pkgname") == 0) {
			if (g_hash_table_contains (found_tags, node_name)) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_PEDANTIC,
							AS_ISSUE_KIND_TAG_DUPLICATED,
							"The tag 'pkgname' appears multiple times. You should evaluate creating a metapackage containing the data in order to avoid defining multiple package names per component.");
			}
		} else if (g_strcmp0 (node_name, "source_pkgname") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "name") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			if (g_str_has_suffix (node_content, ".")) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_INFO,
							AS_ISSUE_KIND_VALUE_ISSUE,
							"The component name should not end with a \".\" [%s]",
							node_content);
			}

		} else if (g_strcmp0 (node_name, "summary") == 0) {
			const gchar *summary = node_content;

			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			if (g_str_has_suffix (summary, "."))
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_INFO,
							AS_ISSUE_KIND_VALUE_ISSUE,
							"The component summary should not end with a \".\" [%s]",
							summary);

			if ((summary != NULL) && ((strstr (summary, "\n") != NULL) || (strstr (summary, "\t") != NULL))) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_WRONG,
							"The summary tag must not contain tabs or linebreaks.");
			}

			if (as_validate_has_hyperlink (summary)) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_WRONG,
							"The summary must not contain any URL.",
							node_name);
			}

		} else if (g_strcmp0 (node_name, "description") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			as_validator_check_description_tag (validator, iter, cpt, mode);
		} else if (g_strcmp0 (node_name, "icon") == 0) {
			gchar *prop;
			prop = as_validator_check_type_property (validator, cpt, iter);
			if ((g_strcmp0 (prop, "cached") == 0) || (g_strcmp0 (prop, "stock") == 0)) {
				if ((g_strrstr (node_content, "/") != NULL) || (as_validate_is_url (node_content)))
					as_validator_add_issue (validator, iter,
								AS_ISSUE_IMPORTANCE_ERROR,
								AS_ISSUE_KIND_VALUE_WRONG,
								"Icons of type 'stock' or 'cached' must not contain an URL or a full or relative path to the icon.");
			}

			if (g_strcmp0 (prop, "remote") == 0) {
				if (!as_validate_is_url (node_content))
					as_validator_add_issue (validator, iter,
								AS_ISSUE_IMPORTANCE_ERROR,
								AS_ISSUE_KIND_VALUE_WRONG,
								"Icons of type 'remote' must contain an URL to the referenced icon.");
			}

			if (mode == AS_FORMAT_STYLE_METAINFO) {
				if ((prop != NULL) && (g_strcmp0 (prop, "stock") != 0)) {
					as_validator_add_issue (validator, iter,
								AS_ISSUE_IMPORTANCE_ERROR,
								AS_ISSUE_KIND_VALUE_WRONG,
								"Metainfo files may only contain 'stock' icons, icons of kind '%s' are not allowed.", prop);
				}
			}
			g_free (prop);
		} else if (g_strcmp0 (node_name, "url") == 0) {
			gchar *prop;
			prop = as_validator_check_type_property (validator, cpt, iter);
			if (as_url_kind_from_string (prop) == AS_URL_KIND_UNKNOWN) {
				as_validator_add_issue (validator, iter,
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
			as_validator_validate_project_license (validator, iter);
		} else if (g_strcmp0 (node_name, "project_group") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
		} else if (g_strcmp0 (node_name, "developer_name") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);

			if (as_validate_has_hyperlink (node_content)) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_WARNING,
							AS_ISSUE_KIND_VALUE_ISSUE,
							"The <developer_name/> can not contain a hyperlink.");
			}
		} else if (g_strcmp0 (node_name, "compulsory_for_desktop") == 0) {
			if (!as_utils_is_desktop_environment (node_content)) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_WRONG,
							"Unknown desktop-id '%s'.", node_content);
			}
		} else if (g_strcmp0 (node_name, "releases") == 0) {
			as_validator_check_children_quick (validator, iter, "release", cpt);
		} else if (g_strcmp0 (node_name, "languages") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			as_validator_check_children_quick (validator, iter, "lang", cpt);
		} else if ((g_strcmp0 (node_name, "translation") == 0) && (mode == AS_FORMAT_STYLE_METAINFO)) {
			g_autofree gchar *prop = NULL;
			AsTranslationKind trkind;
			prop = as_validator_check_type_property (validator, cpt, iter);
			trkind = as_translation_kind_from_string (prop);
			if (prop != NULL && trkind == AS_TRANSLATION_KIND_UNKNOWN) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_WRONG,
							"Unknown type '%s' for <translation/> tag.", prop);
			}
		} else if (g_strcmp0 (node_name, "launch") == 0) {
		} else if (g_strcmp0 (node_name, "extends") == 0) {
		} else if (g_strcmp0 (node_name, "bundle") == 0) {
			g_autofree gchar *prop = NULL;
			prop = as_validator_check_type_property (validator, cpt, iter);
			if (prop != NULL && as_bundle_kind_from_string (prop) == AS_BUNDLE_KIND_UNKNOWN) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_WRONG,
							"Unknown type '%s' for <bundle/> tag.", prop);
			}
		} else if (g_strcmp0 (node_name, "update_contact") == 0) {
			if (mode == AS_FORMAT_STYLE_COLLECTION) {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_WARNING,
							AS_ISSUE_KIND_TAG_NOT_ALLOWED,
							"The 'update_contact' tag should not be included in collection AppStream XML.");
			} else {
				as_validator_check_appear_once (validator, iter, found_tags, cpt);
				as_validator_validate_update_contact (validator, iter);
			}
		} else if (g_strcmp0 (node_name, "suggests") == 0) {
			as_validator_check_children_quick (validator, iter, "id", cpt);
		} else if (g_strcmp0 (node_name, "content_rating") == 0) {
			as_validator_check_children_quick (validator, iter, "content_attribute", cpt);
		} else if (g_strcmp0 (node_name, "custom") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, cpt);
			as_validator_check_children_quick (validator, iter, "value", cpt);
		} else if ((g_strcmp0 (node_name, "metadata") == 0) || (g_strcmp0 (node_name, "kudos") == 0)) {
			/* these tags are GNOME / Fedora specific extensions and are therefore quite common. They shouldn't make the validation fail,
			 * especially if we might standardize at leat the <kudos/> tag one day, but we should still complain about those tags to make
			 * it obvious that they are not supported by all implementations */
			as_validator_add_issue (validator, iter,
						AS_ISSUE_IMPORTANCE_INFO,
						AS_ISSUE_KIND_TAG_UNKNOWN,
						"Found invalid tag: '%s'. This tag is a GNOME-specific extension to AppStream and is not supported by all implementations.",
						node_name);
			tag_valid = FALSE;
		} else if (!g_str_has_prefix (node_name, "x-")) {
			as_validator_add_issue (validator, iter,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_TAG_UNKNOWN,
						"Found invalid tag: '%s'. Non-standard tags must be prefixed with \"x-\".",
						node_name);
			tag_valid = FALSE;
		}

		if (tag_valid) {
			as_validator_check_content_empty (validator,
							  iter,
							  node_name,
							  AS_ISSUE_IMPORTANCE_WARNING,
							  cpt);
		}
	}

	/* emit an error if we are missing the metadata license in metainfo files */
	if ((!has_metadata_license) && (mode == AS_FORMAT_STYLE_METAINFO)) {
		as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_MISSING,
					"The essential tag 'metadata_license' is missing.");
	}

	/* check if we have a description */
	if (as_str_empty (as_component_get_description (cpt))) {
		AsComponentKind cpt_kind;
		cpt_kind = as_component_get_kind (cpt);

		if ((cpt_kind == AS_COMPONENT_KIND_DESKTOP_APP) ||
		    (cpt_kind == AS_COMPONENT_KIND_CONSOLE_APP) ||
		    (cpt_kind == AS_COMPONENT_KIND_WEB_APP)) {
			as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_MISSING,
					"The component is missing a long description. Components of this type must have a long description.");
		} else if (cpt_kind == AS_COMPONENT_KIND_FONT) {
			as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_PEDANTIC,
					AS_ISSUE_KIND_TAG_MISSING,
					"It would be useful for add a long description to this font to present it better to users.");
		} else if ((cpt_kind == AS_COMPONENT_KIND_DRIVER) || (cpt_kind == AS_COMPONENT_KIND_FIRMWARE)) {
			as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_INFO,
					AS_ISSUE_KIND_TAG_MISSING,
					"It is recommended to add a long description to this component to present it better to users.");
		} else if (cpt_kind != AS_COMPONENT_KIND_GENERIC) {
			as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_PEDANTIC,
					AS_ISSUE_KIND_TAG_MISSING,
					"The component is missing a long description. It is recommended to add one.");
		}
	}

	/* validate console-app specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_CONSOLE_APP) {
		if (as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_BINARY) == NULL)
			as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_WARNING,
					AS_ISSUE_KIND_TAG_MISSING,
					"Type 'console-application' component, but no information about binaries in $PATH was provided via a provides/binary tag.");
	}

	/* validate font specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_FONT) {
		if (as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_FONT) == NULL)
			as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_MISSING,
					"Type 'font' component, but no font information was provided via a provides/font tag.");
	}

	/* validate driver specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DRIVER) {
		if (as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_MODALIAS) == NULL)
			as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_WARNING,
					AS_ISSUE_KIND_TAG_MISSING,
					"Type 'driver' component, but no modalias information was provided via a provides/modalias tag.");
	}

	/* validate addon specific stuff */
	if (as_component_get_extends (cpt)->len > 0) {
		AsComponentKind kind = as_component_get_kind (cpt);
		if ((kind != AS_COMPONENT_KIND_ADDON) && (kind != AS_COMPONENT_KIND_LOCALIZATION))
			as_validator_add_issue (validator, NULL,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_TAG_NOT_ALLOWED,
						"An 'extends' tag is specified, but the component is not of type 'addon' or 'localization'.");
	} else {
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_ADDON)
			as_validator_add_issue (validator, NULL,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_TAG_MISSING,
						"The component is an addon, but no 'extends' tag was specified.");
	}

	/* validate l10n specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_LOCALIZATION) {
		if (as_component_get_extends (cpt)->len == 0) {
			as_validator_add_issue (validator, NULL,
						AS_ISSUE_IMPORTANCE_WARNING,
						AS_ISSUE_KIND_TAG_MISSING,
						"This 'localization' component is missing an An 'extends' tag, to specify the components it adds localization to.");
		}
		if (g_hash_table_size (as_component_get_languages_table (cpt)) == 0) {
			as_validator_add_issue (validator, NULL,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_TAG_MISSING,
						"This 'localization' component does not define any languages this localization is for.");
		}
	}

	/* validate suggestions */
	if (as_component_get_suggested (cpt)->len > 0) {
		guint j;
		GPtrArray *sug_array;

		sug_array = as_component_get_suggested (cpt);
		for (j = 0; j < sug_array->len; j++) {
			AsSuggested *prov = AS_SUGGESTED (g_ptr_array_index (sug_array, j));
			if (mode == AS_FORMAT_STYLE_METAINFO) {
				if (as_suggested_get_kind (prov) != AS_SUGGESTED_KIND_UPSTREAM)
					as_validator_add_issue (validator, NULL,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_VALUE_WRONG,
							"Suggestions of any type other than 'upstream' are not allowed in metainfo files (type was '%s')", as_suggested_kind_to_string (as_suggested_get_kind (prov)));
			}
		}
	}

	/* validate categories */
	if (as_component_get_categories (cpt)->len > 0) {
		guint j;
		GPtrArray *cat_array;

		cat_array = as_component_get_categories (cpt);
		for (j = 0; j < cat_array->len; j++) {
			const gchar *category_name = (const gchar*) g_ptr_array_index (cat_array, j);

			if (!as_utils_is_category_name (category_name)) {
				as_validator_add_issue (validator, NULL,
							AS_ISSUE_IMPORTANCE_WARNING,
							AS_ISSUE_KIND_VALUE_WRONG,
							"The category '%s' defined is not valid. Refer to the Freedesktop menu specification for a list of valid categories.", category_name);
			}
		}
	}

	/* validate screenshots */
	if (as_component_get_screenshots (cpt)->len > 0) {
		guint j;
		GPtrArray *scr_array;

		scr_array = as_component_get_screenshots (cpt);
		for (j = 0; j < scr_array->len; j++) {
			AsScreenshot *scr = AS_SCREENSHOT (g_ptr_array_index (scr_array, j));
			const gchar *scr_caption = as_screenshot_get_caption (scr);

			if ((scr_caption != NULL) && (strlen (scr_caption) > 80)) {
				as_validator_add_issue (validator, NULL,
							AS_ISSUE_IMPORTANCE_PEDANTIC,
							AS_ISSUE_KIND_VALUE_ISSUE,
							"The screenshot caption '%s' is too long (should be <= 80 characters)",
							scr_caption);
			}

			if (as_screenshot_get_images (scr)->len <= 0) {
				as_validator_add_issue (validator, NULL,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_TAG_MISSING,
							"The component contains a screenshot without any images.");
			}
		}
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
	g_autoptr(GFileInfo) info = NULL;
	g_autoptr(GInputStream) file_stream = NULL;
	g_autoptr(GInputStream) stream_data = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GString) asxmldata = NULL;
	g_autofree gchar *fname = NULL;
	gssize len;
	const gsize buffer_size = 1024 * 32;
	g_autofree gchar *buffer = NULL;
	const gchar *content_type = NULL;
	g_autoptr(GError) tmp_error = NULL;
	gboolean ret;

	info = g_file_query_info (metadata_file,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
	if (info != NULL)
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	fname = g_file_get_basename (metadata_file);
	as_validator_set_current_fname (validator, fname);

	file_stream = G_INPUT_STREAM (g_file_read (metadata_file, NULL, &tmp_error));
	if (tmp_error != NULL) {
		as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_READ_ERROR,
					"Unable to read file: %s", tmp_error->message);
		return FALSE;
	}
	if (file_stream == NULL)
		return FALSE;

	if ((g_strcmp0 (content_type, "application/gzip") == 0) || (g_strcmp0 (content_type, "application/x-gzip") == 0)) {
		/* decompress the GZip stream */
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		stream_data = g_converter_input_stream_new (file_stream, conv);
	} else {
		stream_data = g_object_ref (file_stream);
	}

	asxmldata = g_string_new ("");
	buffer = g_malloc (buffer_size);
	while ((len = g_input_stream_read (stream_data, buffer, buffer_size, NULL, &tmp_error)) > 0) {
		g_string_append_len (asxmldata, buffer, len);
	}
	if (tmp_error != NULL) {
		as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_READ_ERROR,
					"Unable to read file: %s", tmp_error->message);
		return FALSE;
	}
	/* check if there was an error */
	if (len < 0)
		return FALSE;

	ret = as_validator_validate_data (validator, asxmldata->str);
	as_validator_clear_current_fname (validator);

	return ret;
}

/**
 * as_validator_open_xml_document:
 */
static xmlDoc*
as_validator_open_xml_document (AsValidator *validator, AsXMLData *xdt, const gchar *xmldata)
{
	xmlDoc *doc;
	g_autoptr(GError) error = NULL;

	doc = as_xmldata_parse_document (xdt, xmldata, &error);
	if (doc == NULL) {
		if (error != NULL) {
			as_validator_add_issue (validator, NULL,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_MARKUP_INVALID,
						error->message);
		}

		return NULL;
	}

	return doc;
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
	g_autoptr(AsXMLData) xdt = NULL;
	AsComponent *cpt;

	/* load the XML data */
	xdt = as_xmldata_new ();
	as_xmldata_initialize (xdt, AS_CURRENT_FORMAT_VERSION,
			       "C",
				NULL,
				NULL,
				NULL,
				0);

	doc = as_validator_open_xml_document (validator, xdt, metadata);
	if (doc == NULL)
		return FALSE;
	root = xmlDocGetRootElement (doc);

	ret = TRUE;
	if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
		as_xmldata_set_format_style (xdt, AS_FORMAT_STYLE_METAINFO);
		cpt = as_validator_validate_component_node (validator, xdt, root);
		if (cpt != NULL)
			g_object_unref (cpt);
	} else if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
		xmlNode *iter;
		const gchar *node_name;

		as_xmldata_set_format_style (xdt, AS_FORMAT_STYLE_COLLECTION);
		for (iter = root->children; iter != NULL; iter = iter->next) {
			/* discard spaces */
			if (iter->type != XML_ELEMENT_NODE)
				continue;
			node_name = (const gchar*) iter->name;
			if (g_strcmp0 (node_name, "component") == 0) {
				cpt = as_validator_validate_component_node (validator, xdt, iter);
				if (cpt != NULL)
					g_object_unref (cpt);
			} else {
				as_validator_add_issue (validator, iter,
							AS_ISSUE_IMPORTANCE_ERROR,
							AS_ISSUE_KIND_TAG_UNKNOWN,
							"Unknown tag found: %s",
							node_name);
				ret = FALSE;
			}
		}
	} else if (g_str_has_prefix ((gchar*) root->name, "application")) {
		as_validator_add_issue (validator, root,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_LEGACY,
					"The metainfo file uses an ancient version of the AppStream specification, which can not be validated. Please migrate it to version 0.6 (or higher).");
		ret = FALSE;
	} else {
		as_validator_add_issue (validator, root,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_UNKNOWN,
					"Unknown root tag found: '%s' - maybe not a metainfo document?",
					(gchar*) root->name);
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
	g_autofree gchar *cid_base = NULL;

	/* if we have no component-id, we can't check anything */
	if (as_component_get_id (cpt) == NULL)
		return;

	as_validator_set_current_cpt (data->validator, cpt);
	as_validator_set_current_fname (data->validator, fname);

	/* check if the fname and the component-id match */
	if (g_str_has_suffix (as_component_get_id (cpt), ".desktop")) {
		cid_base = g_strndup (as_component_get_id (cpt),
					g_strrstr (as_component_get_id (cpt), ".") - as_component_get_id (cpt));
	} else {
		cid_base = g_strdup (as_component_get_id (cpt));
	}
	if (!as_matches_metainfo (fname, cid_base)) {
		/* the name-without-type didn't match - check for the full id in the component name */
		if (!as_matches_metainfo (fname, as_component_get_id (cpt))) {
			as_validator_add_issue (data->validator, NULL,
					AS_ISSUE_IMPORTANCE_WARNING,
					AS_ISSUE_KIND_WRONG_NAME,
					"The metainfo filename does not match the component ID.");
		}
	}

	/* check if the referenced .desktop file exists */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
		AsLaunchable *de_launchable = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
		if ((de_launchable != NULL) && (as_launchable_get_entries (de_launchable)->len > 0)) {
			const gchar *desktop_id = g_ptr_array_index (as_launchable_get_entries (de_launchable), 0);

			if (g_hash_table_contains (data->desktop_fnames, desktop_id)) {
				g_autofree gchar *desktop_fname_full = NULL;
				g_autoptr(GKeyFile) dfile = NULL;
				GError *tmp_error = NULL;

				desktop_fname_full = g_build_filename (data->apps_dir, desktop_id, NULL);
				dfile = g_key_file_new ();

				g_key_file_load_from_file (dfile, desktop_fname_full, G_KEY_FILE_NONE, &tmp_error);
				if (tmp_error != NULL) {
					as_validator_add_issue (data->validator, NULL,
							AS_ISSUE_IMPORTANCE_WARNING,
							AS_ISSUE_KIND_READ_ERROR,
							"Unable to read associated .desktop file: %s", tmp_error->message);
					g_error_free (tmp_error);
					tmp_error = NULL;
				} else {
					/* we successfully opened the .desktop file, now perform some checks */

					/* categories */
					if (g_key_file_has_key (dfile, G_KEY_FILE_DESKTOP_GROUP,
									G_KEY_FILE_DESKTOP_KEY_CATEGORIES, NULL)) {
						g_autofree gchar *cats_str = NULL;
						g_auto(GStrv) cats = NULL;
						guint i;

						cats_str = g_key_file_get_string (dfile, G_KEY_FILE_DESKTOP_GROUP,
											G_KEY_FILE_DESKTOP_KEY_CATEGORIES, NULL);
						cats = g_strsplit (cats_str, ";", -1);
						for (i = 0; cats[i] != NULL; i++) {
							if (as_str_empty (cats[i]))
								continue;
							if (!as_utils_is_category_name (cats[i])) {
								as_validator_add_issue (data->validator, NULL,
											AS_ISSUE_IMPORTANCE_WARNING,
											AS_ISSUE_KIND_VALUE_WRONG,
											"The category '%s' defined in the .desktop file does not exist.", cats[i]);
							}
						}
					}

				}
			} else {
				as_validator_add_issue (data->validator, NULL,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_FILE_MISSING,
						"Component metadata refers to a non-existing .desktop file.");
			}
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
	g_autofree gchar *legacy_metainfo_dir = NULL;
	g_autofree gchar *apps_dir = NULL;
	g_autoptr(GPtrArray) mfiles = NULL;
	g_autoptr(GPtrArray) mfiles_legacy = NULL;
	g_autoptr(GPtrArray) dfiles = NULL;
	GHashTable *dfilenames = NULL;
	GHashTable *validated_cpts = NULL;
	guint i;
	gboolean ret = TRUE;
	g_autoptr(AsXMLData) xdt = NULL;
	struct MInfoCheckData ht_helper;

	/* cleanup */
	as_validator_clear_issues (validator);

	metainfo_dir = g_build_filename (root_dir, "usr", "share", "metainfo", NULL);
	legacy_metainfo_dir = g_build_filename (root_dir, "usr", "share", "appdata", NULL);
	apps_dir = g_build_filename (root_dir, "usr", "share", "applications", NULL);

	/* check if we actually have a directory which could hold metadata */
	if ((!g_file_test (metainfo_dir, G_FILE_TEST_IS_DIR)) &&
	    (!g_file_test (legacy_metainfo_dir, G_FILE_TEST_IS_DIR))) {
		as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_INFO,
					AS_ISSUE_KIND_FILE_MISSING,
					"No AppStream metadata was found.");
		goto out;
	}

	/* check if we actually have a directory which could hold application information */
	if (!g_file_test (apps_dir, G_FILE_TEST_IS_DIR)) {
		as_validator_add_issue (validator, NULL,
					AS_ISSUE_IMPORTANCE_PEDANTIC, /* pedantic because not everything which has metadata is an application */
					AS_ISSUE_KIND_FILE_MISSING,
					"No XDG applications directory found.");
	}

	/* holds a filename -> component mapping */
	validated_cpts = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						g_object_unref);

	/* set up XML parser */
	xdt = as_xmldata_new ();
	as_xmldata_initialize (xdt, AS_CURRENT_FORMAT_VERSION,
			       "C",
				NULL,
				NULL,
				NULL,
				0);
	as_xmldata_set_format_style (xdt, AS_FORMAT_STYLE_METAINFO);

	/* validate all metainfo files */
	mfiles = as_utils_find_files_matching (metainfo_dir, "*.xml", FALSE, NULL);
	mfiles_legacy = as_utils_find_files_matching (legacy_metainfo_dir, "*.xml", FALSE, NULL);

	/* in case we only have legacy files */
	if (mfiles == NULL)
		mfiles = g_ptr_array_new_with_free_func (g_free);

	if (mfiles_legacy != NULL) {
		for (i = 0; i < mfiles_legacy->len; i++) {
			const gchar *fname;
			g_autofree gchar *fname_basename = NULL;

			/* process metainfo files in legacy paths */
			fname = (const gchar*) g_ptr_array_index (mfiles_legacy, i);
			fname_basename = g_path_get_basename (fname);
			as_validator_set_current_fname (validator, fname_basename);

			as_validator_add_issue (validator, NULL,
						AS_ISSUE_IMPORTANCE_INFO,
						AS_ISSUE_KIND_LEGACY,
						"The metainfo file is stored in a legacy path. Please place it in '/usr/share/metainfo'.");

			g_ptr_array_add (mfiles, g_strdup (fname));
		}
	}

	for (i = 0; i < mfiles->len; i++) {
		const gchar *fname;
		g_autoptr(GFile) file = NULL;
		g_autoptr(GInputStream) file_stream = NULL;
		g_autoptr(GError) tmp_error = NULL;
		g_autoptr(GString) asdata = NULL;
		gssize len;
		const gsize buffer_size = 1024 * 24;
		g_autofree gchar *buffer = NULL;
		xmlNode *root;
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
		file_stream = G_INPUT_STREAM (g_file_read (file, NULL, &tmp_error));
		if (tmp_error != NULL) {
			as_validator_add_issue (validator, NULL,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_READ_ERROR,
						"Unable to read file: %s", tmp_error->message);
			continue;
		}

		asdata = g_string_new ("");
		buffer = g_malloc (buffer_size);
		while ((len = g_input_stream_read (file_stream, buffer, buffer_size, NULL, &tmp_error)) > 0) {
			g_string_append_len (asdata, buffer, len);
		}
		/* check if there was an error */
		if (tmp_error != NULL) {
			as_validator_add_issue (validator, NULL,
						AS_ISSUE_IMPORTANCE_ERROR,
						AS_ISSUE_KIND_READ_ERROR,
						"Unable to read file: %s", tmp_error->message);
			continue;
		}

		/* now read the XML */
		doc = as_validator_open_xml_document (validator, xdt, asdata->str);
		if (doc == NULL) {
			as_validator_clear_current_fname (validator);
			continue;
		}
		root = xmlDocGetRootElement (doc);

		if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
			AsComponent *cpt;
			cpt = as_validator_validate_component_node (validator,
								    xdt,
								    root);
			if (cpt != NULL)
				g_hash_table_insert (validated_cpts,
							g_strdup (fname_basename),
							cpt);
		} else if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
			as_validator_add_issue (validator, root,
					AS_ISSUE_IMPORTANCE_ERROR,
					AS_ISSUE_KIND_TAG_NOT_ALLOWED,
					"The metainfo file specifies multiple components. This is not allowed.");
			ret = FALSE;
		} else if (g_str_has_prefix ((gchar*) root->name, "application")) {
			as_validator_add_issue (validator, root,
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
