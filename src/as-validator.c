/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-validator-issue-tag.h"

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-curl.h"
#include "as-vercmp.h"
#include "as-spdx.h"
#include "as-component.h"
#include "as-component-private.h"
#include "as-yaml.h"
#include "as-desktop-entry.h"

typedef struct
{
	GHashTable	*issue_tags; /* of utf8:AsValidatorIssueTag */

	GHashTable	*issues; /* of utf8:AsValidatorIssue */
	GHashTable	*issues_per_file; /* of utf8:GPtrArray<AsValidatorIssue> */

	AsComponent	*current_cpt;
	gchar		*current_fname;

	gboolean	check_urls;
	gboolean	strict;
	AsCurl		*acurl;
} AsValidatorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsValidator, as_validator, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_validator_get_instance_private (o))

/**
 * as_validator_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.15.4
 **/
G_DEFINE_QUARK (as-validator-error-quark, as_validator_error)


/**
 * as_validator_init:
 **/
static void
as_validator_init (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);

	priv->issue_tags = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  NULL);
	for (guint i = 0; as_validator_issue_tag_list[i].tag != NULL; i++) {
		gboolean r = g_hash_table_insert (priv->issue_tags,
						  g_strdup (as_validator_issue_tag_list[i].tag),
						  &as_validator_issue_tag_list[i]);
		if (G_UNLIKELY (!r))
			g_critical ("Duplicate issue-tag '%s' found in tag list. This is a bug in the validator.", as_validator_issue_tag_list[i].tag);
	}

	/* set of issues */
	priv->issues = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      g_free,
				              g_object_unref);
	/* filename -> issue list mapping */
	priv->issues_per_file = g_hash_table_new_full (g_str_hash,
							g_str_equal,
							g_free,
							(GDestroyNotify) g_ptr_array_unref);
	priv->current_fname = NULL;
	priv->current_cpt = NULL;
	priv->check_urls = FALSE;
	priv->strict = FALSE;
}

/**
 * as_validator_finalize:
 **/
static void
as_validator_finalize (GObject *object)
{
	AsValidator *validator = AS_VALIDATOR (object);
	AsValidatorPrivate *priv = GET_PRIVATE (validator);

	g_hash_table_unref (priv->issue_tags);

	g_hash_table_unref (priv->issues_per_file);
	g_hash_table_unref (priv->issues);

	g_free (priv->current_fname);
	if (priv->current_cpt != NULL)
		g_object_unref (priv->current_cpt);

	if (priv->acurl != NULL)
		g_object_unref (priv->acurl);

	G_OBJECT_CLASS (as_validator_parent_class)->finalize (object);
}

/**
 * as_validator_add_issue:
 **/
static void
as_validator_add_issue (AsValidator *validator, xmlNode *node, const gchar *tag, const gchar *format, ...)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	va_list args;
	g_autofree gchar *buffer = NULL;
	g_autofree gchar *tag_final = NULL;
	const gchar *explanation;
	AsIssueSeverity severity;
	g_autofree gchar *location = NULL;
	AsValidatorIssue *issue;
	gchar *id_str;
	const AsValidatorIssueTag *tag_data = g_hash_table_lookup (priv->issue_tags, tag);

	if (tag_data == NULL) {
		/* we requested data about an invalid tag */
		g_warning ("Validator invoked invalid issue tag: %s", tag);
		severity = AS_ISSUE_SEVERITY_ERROR;
		explanation =_("The emitted issue tag is unknown in the tag registry of AppStream. "
				"This is a bug in the validator itself, please report this issue in our bugtracker.");
		tag_final = g_strdup_printf ("__error__%s", tag);
	} else {
		tag_final = g_strdup (tag);
		severity = tag_data->severity;
		explanation = tag_data->explanation;
	}

	if (format != NULL) {
		va_start (args, format);
		buffer = g_strdup_vprintf (format, args);
		va_end (args);
	}

	issue = as_validator_issue_new ();
	as_validator_issue_set_tag (issue, tag_final);
	as_validator_issue_set_severity (issue, severity);
	as_validator_issue_set_hint (issue, buffer);
	as_validator_issue_set_explanation (issue, explanation);

	/* update location information */
	if (priv->current_fname != NULL)
		as_validator_issue_set_filename (issue, priv->current_fname);

	if (priv->current_cpt != NULL)
		as_validator_issue_set_cid (issue, as_component_get_id (priv->current_cpt));

	if (node != NULL)
		as_validator_issue_set_line (issue, xmlGetLineNo (node));

	location = as_validator_issue_get_location (issue);
	id_str = g_strdup_printf ("%s/%s/%s",
				  location,
				  tag_final,
				  buffer);
	/* str ownership is transferred to the hashtable */
	if (g_hash_table_insert (priv->issues, id_str, issue)) {
		/* the issue is new, we can add it to our by-file listing */
		const gchar *fname_key = priv->current_fname? priv->current_fname : "";
		GPtrArray *ilist = g_hash_table_lookup (priv->issues_per_file, fname_key);
		if (ilist == NULL) {
			ilist = g_ptr_array_new_with_free_func (g_object_unref);
			g_hash_table_insert (priv->issues_per_file, g_strdup (fname_key), ilist);
		}
		g_ptr_array_add (ilist, g_object_ref (issue));
	}
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
	g_hash_table_remove_all (priv->issues_per_file);
	g_hash_table_remove_all (priv->issues);
}

/**
 * as_validator_check_success:
 *
 * Check if, according to the current recorded issues, the last validated data was valid.
 */
static gboolean
as_validator_check_success (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	GHashTableIter iter;
	gpointer value;

	g_hash_table_iter_init (&iter, priv->issues);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		AsIssueSeverity severity;
		AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (value);
		severity = as_validator_issue_get_severity (issue);

		if (priv->strict) {
			/* in strict mode we fail for anything that's not a pedantic issue */
			if (severity != AS_ISSUE_SEVERITY_PEDANTIC)
				return FALSE;
		} else {
			/* any error or warning means validation has failed */
			if (severity == AS_ISSUE_SEVERITY_ERROR || severity == AS_ISSUE_SEVERITY_WARNING)
				return FALSE;
		}
	}

	return TRUE;
}

/**
 * as_validator_setup_networking:
 */
static gboolean
as_validator_setup_networking (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	g_autoptr(GError) tmp_error = NULL;

	/* check if we are already initialized */
	if (priv->acurl != NULL)
		return TRUE;

	/* don't initialize if no URLs should be checked */
	if (!priv->check_urls)
		return TRUE;

	priv->acurl = as_curl_new (&tmp_error);
	if (priv->acurl == NULL) {
		g_critical ("Failed to set up networking support: %s", tmp_error->message);
		return FALSE;
	}
	return TRUE;
}

/**
 * as_validator_check_web_url:
 *
 * Check if an URL is valid and is reachable, creating a new
 * issue tag of value @tag in case of errors.
 */
static gboolean
as_validator_check_web_url (AsValidator *validator, xmlNode *node, const gchar *url, const gchar *tag)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	g_autoptr(GError) tmp_error = NULL;

	/* we don't check mailto URLs */
	if (g_str_has_prefix (url, "mailto:"))
		return TRUE;

	if (g_str_has_prefix (url, "ftp:")) {
		/* we can't check FTP URLs here, and those shouldn't generally be used in AppStream */
		as_validator_add_issue (validator, node,
					"url-uses-ftp",
					url);
		return FALSE;
	}

	if (!as_curl_is_url (url)) {
		as_validator_add_issue (validator, node, tag,
					"%s - %s",
					url,
					_("URL format is invalid."));
		return FALSE;
	}

	/* do nothing and assume the URL exists if we shouldn't check URLs */
	if (!priv->check_urls)
		return TRUE;

	g_debug ("Checking URL availability: %s", url);

	/* try to download first few bytes of the file, get error if that fails */
	if (!as_curl_check_url_exists (priv->acurl, url, &tmp_error)) {
		as_validator_add_issue (validator, node, tag,
					"%s - %s",
					url,
					tmp_error->message);
		return FALSE;
	}

	/* if we we din't get a zero-length file, we just assume everything is fine here */
	return TRUE;
}

/**
 * as_validator_get_check_urls:
 * @validator: a #AsValidator instance.
 *
 * Returns: %TRUE in case we check if remote URLs exist.
 */
gboolean
as_validator_get_check_urls (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	return priv->check_urls;
}

/**
 * as_validator_set_check_urls:
 * @validator: a #AsValidator instance.
 * @value: %TRUE if remote URLs should be checked for availability.
 *
 * Set this value to make the #AsValidator check whether remote URLs
 * actually exist.
 */
void
as_validator_set_check_urls (AsValidator *validator, gboolean value)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	priv->check_urls = value;

	/* initialize networking, in case URLs should be checked */
	as_validator_setup_networking (validator);
}

/**
 * as_validator_get_strict:
 * @validator: a #AsValidator instance.
 *
 * Returns: %TRUE in case we are in strict mode and consider any issues as fatal.
 *
 * Since: 0.15.4
 */
gboolean
as_validator_get_strict (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	return priv->strict;
}

/**
 * as_validator_set_strict:
 * @validator: a #AsValidator instance.
 * @is_strict: %TRUE to enable strict mode.
 *
 * Enable or disable strict mode. In strict mode, any found issue will result
 * in a failed validation (except for issues of "pedantic" severity).
 * Otherwise, only a "warning" or "error" will cause the validation to fail.
 *
 * Since: 0.15.4
 */
void
as_validator_set_strict (AsValidator *validator, gboolean is_strict)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	priv->strict = is_strict;
}

/**
 * as_validator_add_override:
 * @validator: a #AsValidator instance.
 * @tag: the issue tag to override, e.g. "release-time-missing"
 * @severity_override: the new severity for the tag.
 * @error: a #GError or %NULL
 *
 * Override the severity of a selected tag. For most tags, the severity
 * can not be lowered to a value that makes a validation
 * that would otherwise fail pass (so e.g. an ERROR can not become an INFO).
 * Some tags are explicitly allowed to have their severity lowered to enable
 * validation of e.g. incomplete metadata during development.
 * Increasing the severity of any tag is always allowed.
 *
 * Since: 0.15.4
 */
gboolean
as_validator_add_override (AsValidator *validator,
			   const gchar *tag,
			   AsIssueSeverity severity_override,
			   GError **error)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	AsValidatorIssueTag *tag_data = NULL;
	AsIssueSeverity real_severity;

	/* we only allow some tags to be downgraded in order to keep validation pass/fail consistent
	 * and prevent people from ignoring truly fatal errors instead of fixing them */
	const gchar* demotion_allowlist[] = {
		/* needed for some in-development metainfo which doesn't have a release time yet */
		"release-time-missing",
		/* allowed for apps which intentionally *must* keep the old ID for some reason */
		"cid-desktopapp-is-not-rdns",
		/* in case the empty tag was actually intended */
		"tag-empty",
		/* allow GNOME to validate metadata using its new versioning scheme (until a better solution is found) */
		"releases-not-in-order",
		NULL
	};

	/* sanity checks */
	if (severity_override == AS_ISSUE_SEVERITY_UNKNOWN || severity_override >= AS_ISSUE_SEVERITY_LAST) {
		g_set_error (error,
				AS_VALIDATOR_ERROR,
				AS_VALIDATOR_ERROR_OVERRIDE_INVALID,
				/* TRANSLATORS: The user tried to set an invalid severity for a validator issue tag */
				_("The new issue severity for tag '%s' is invalid."),
				tag);
		return FALSE;
	}

	tag_data = g_hash_table_lookup (priv->issue_tags, tag);
	if (tag_data == NULL) {
		g_set_error (error,
				AS_VALIDATOR_ERROR,
				AS_VALIDATOR_ERROR_OVERRIDE_INVALID,
				/* TRANSLATORS: The user tried to override a validator issue tag that we don't know */
				_("The issue tag '%s' is not recognized."),
				tag);
		return FALSE;
	}

	real_severity = tag_data->severity;

	if (real_severity == AS_ISSUE_SEVERITY_ERROR || real_severity == AS_ISSUE_SEVERITY_WARNING) {
		if (severity_override != AS_ISSUE_SEVERITY_ERROR && severity_override != AS_ISSUE_SEVERITY_WARNING) {
			/* check if we can downgrade the severity of this issue */
			gboolean severity_downgrade_allowed = FALSE;

			for (guint i = 0; demotion_allowlist[i] != NULL; i++) {
				if (g_strcmp0 (demotion_allowlist[i], tag) == 0) {
					severity_downgrade_allowed = TRUE;
					break;
				}
			}

			if (!severity_downgrade_allowed) {
				g_set_error (error,
						AS_VALIDATOR_ERROR,
						AS_VALIDATOR_ERROR_OVERRIDE_INVALID,
						/* TRANSLATORS: The user tried to override an issue tag and make it non-fatal, even though the tag is not
						 * whitelisted for that. */
						_("It is not allowed to downgrade the severity of tag '%s' to one that allows validation to pass."),
						tag);
				return FALSE;
			}
		}
	}

	/* actually apply the override, if we are here everything was fine with it */
	g_debug ("Overriding severity of validator issue tag: %s == %s", tag, as_issue_severity_to_string (severity_override));
	tag_data->severity = severity_override;

	return TRUE;
}

/**
 * as_validator_check_type_property:
 **/
static gchar*
as_validator_check_type_property (AsValidator *validator, AsComponent *cpt, xmlNode *node)
{
	g_autofree gchar *prop = NULL;
	g_autofree gchar *content = NULL;

	prop = as_xml_get_prop_value (node, "type");
	content = (gchar*) xmlNodeGetContent (node);
	if (prop == NULL) {
		as_validator_add_issue (validator, node,
					"type-property-required",
					"%s (%s)",
					(const gchar*) node->name,
					content);
	}

	return g_steal_pointer (&prop);
}

/**
 * as_validator_check_content:
 **/
static void
as_validator_check_content_empty (AsValidator *validator, xmlNode *node, const gchar *tag_path)
{
	g_autofree gchar *node_content = NULL;

	node_content = as_strstripnl ((gchar*) xmlNodeGetContent (node));
	if (!as_is_empty (node_content))
		return;

	as_validator_add_issue (validator, node,
				"tag-empty",
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
	gchar *tmp;
	if (text == NULL)
		return FALSE;

	tmp = g_strstr_len (text, -1, "https://");
	if ((tmp != NULL) && (!g_str_has_prefix (tmp + 8, " ")))
		return TRUE;

	tmp = g_strstr_len (text, -1, "http://");
	if ((tmp != NULL) && (!g_str_has_prefix (tmp + 7, " ")))
		return TRUE;

	tmp = g_strstr_len (text, -1, "ftp://");
	if ((tmp != NULL) && (!g_str_has_prefix (tmp + 6, " ")))
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
 * as_validate_is_secure_url:
 *
 * Check if @str is a secure (HTTPS) URL.
 */
static gboolean
as_validate_is_secure_url (const gchar *str)
{
	if (g_str_has_prefix (str, "https://"))
		return TRUE;
	/* mailto URLs are fine as well */
	if (g_str_has_prefix (str, "mailto:"))
		return TRUE;
	return FALSE;
}

/**
 * as_validator_check_children_quick:
 **/
static void
as_validator_check_children_quick (AsValidator *validator, xmlNode *node, const gchar *allowed_tagname, gboolean allow_empty)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		if (g_strcmp0 (node_name, allowed_tagname) == 0) {
			g_autofree gchar *tag_path = NULL;
			tag_path = g_strdup_printf ("%s/%s", (const gchar*) node->name, node_name);
			if (!allow_empty)
				as_validator_check_content_empty (validator,
								  iter,
								  tag_path);
		} else {
			as_validator_add_issue (validator, node,
						"invalid-child-tag-name",
						/* TRANSLATORS: An invalid XML tag was found, "Found" refers to the tag name found, "Allowed" to the permitted name. */
						_("Found: %s - Allowed: %s"),
						node_name,
						allowed_tagname);
		}
	}
}

/**
 * as_validator_check_nolocalized:
 **/
static void
as_validator_check_nolocalized (AsValidator *validator, xmlNode* node, const gchar *tag, const gchar *format)
{
	g_autofree gchar *lang = NULL;

	lang = as_xml_get_prop_value (node, "lang");
	if (lang != NULL) {
		as_validator_add_issue (validator,
					node,
					tag,
					format);
	}
}

/**
 * as_validator_first_word_capitalized:
 */
static gboolean
as_validator_first_word_capitalized (AsValidator *validator, const gchar *text)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	g_autofree gchar *first_word = NULL;
	gchar *tmp;

	if (text == NULL || text[0] == '\0')
		return TRUE;

	/* text starts with a number, that's fine */
	if (g_ascii_isdigit (text[0]))
		return TRUE;

	/* get the first word */
	first_word = g_strdup (text);
	tmp = g_strstr_len (first_word, -1, " ");
	if (tmp != NULL)
		*tmp = '\0';

	/* we accept a capitalization anywhere in the first word */
	for (guint i = 0; first_word[i] != '\0'; i++) {
		if (first_word[i] >= 'A' && first_word[i] <= 'Z')
			return TRUE;
	}

	/* if the first word is the project's name, we accept whatever spelling they prefer */
	if (g_strcmp0 (first_word, as_component_get_name (priv->current_cpt)) == 0)
		return TRUE;

	return FALSE;
}

/**
 * as_validator_check_description_paragraph:
 **/
static void
as_validator_check_description_paragraph (AsValidator *validator, xmlNode *node)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		if ((g_strcmp0 (node_name, "em") == 0) || (g_strcmp0 (node_name, "code") == 0))
			continue;

		as_validator_add_issue (validator,
					iter,
					"description-para-markup-invalid",
					node_name);
	}
}

/**
 * as_validator_check_description_enumeration:
 **/
static void
as_validator_check_description_enumeration (AsValidator *validator, xmlNode *node)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		if (g_strcmp0 (node_name, "li") == 0) {
			g_autofree gchar *tag_path = NULL;
			tag_path = g_strdup_printf ("%s/%s", (const gchar*) node->name, node_name);
			as_validator_check_content_empty (validator,
							  iter,
							  tag_path);
			as_validator_check_description_paragraph (validator, iter);
		} else {
			as_validator_add_issue (validator, iter,
						"description-enum-item-invalid",
						node_name);
		}
	}
}

/**
 * as_validator_check_description_tag:
 **/
static void
as_validator_check_description_tag (AsValidator *validator, xmlNode* node, AsFormatStyle mode, gboolean main_description)
{
	gboolean first_paragraph = TRUE;
	gboolean is_localized = FALSE;

	if (mode == AS_FORMAT_STYLE_METAINFO) {
		as_validator_check_nolocalized (validator,
						node,
						"metainfo-localized-description-tag",
						(const gchar*) node->name);
	} else {
		g_autofree gchar *lang = as_xml_get_prop_value (node, "lang");
		is_localized = lang != NULL;
	}

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name = (gchar*) iter->name;
		g_autofree gchar *node_content = (gchar*) xmlNodeGetContent (iter);

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if ((g_strcmp0 (node_name, "ul") != 0) && (g_strcmp0 (node_name, "ol") != 0)) {
			as_validator_check_content_empty (validator,
							  iter,
							  node_name);
		}

		if (g_strcmp0 (node_name, "p") == 0) {
			g_autofree gchar *p_content = as_strstripnl ((gchar*) xmlNodeGetContent (iter));

			if (mode == AS_FORMAT_STYLE_COLLECTION) {
				as_validator_check_nolocalized (validator,
								iter,
								"collection-localized-description-section",
								"description/p");
			}
			if (main_description) {
				if (node_content != NULL)
					g_strstrip (node_content);
				if ((first_paragraph) && (strlen (node_content) < 80)) {
					as_validator_add_issue (validator, iter,
								"description-first-para-too-short",
								node_content);
				}
			}
			first_paragraph = FALSE;

			/* in metainfo mode, we need to check every node for localization,
			 * otherwise we just honor the is_localized var */
			if (mode == AS_FORMAT_STYLE_METAINFO) {
				g_autofree gchar *lang = as_xml_get_prop_value (iter, "lang");
				is_localized = lang != NULL;
			}

			/* validate spelling */
			if (!is_localized && !as_validator_first_word_capitalized (validator, p_content))
				as_validator_add_issue (validator, node,
							"description-first-word-not-capitalized",
							NULL);

			/* validate common stuff */
			as_validator_check_description_paragraph (validator, iter);
		} else if (g_strcmp0 (node_name, "ul") == 0) {
			if (mode == AS_FORMAT_STYLE_COLLECTION) {
				as_validator_check_nolocalized (validator,
								iter,
								"collection-localized-description-section",
								"description/ul");
			}
			as_validator_check_description_enumeration (validator, iter);
		} else if (g_strcmp0 (node_name, "ol") == 0) {
			if (mode == AS_FORMAT_STYLE_COLLECTION) {
				as_validator_check_nolocalized (validator,
								iter,
								"collection-localized-description-section",
								"description/ol");
			}
			as_validator_check_description_enumeration (validator, iter);
		} else {
			as_validator_add_issue (validator, iter,
						"description-markup-invalid",
						node_name);
		}

		if (as_validate_has_hyperlink (node_content)) {
			as_validator_add_issue (validator, iter,
						"description-has-plaintext-url",
						node_name);
		}
	}
}

/**
 * as_validator_check_appear_once:
 **/
static void
as_validator_check_appear_once (AsValidator *validator, xmlNode *node, GHashTable *known_tags, gboolean translatable)
{
	g_autofree gchar *lang = NULL;
	gchar *tag_id;
	const gchar *node_name;

	/* generate tag-id to make a unique identifier for localized and unlocalized tags */
	node_name = (const gchar*) node->name;
	lang = as_xml_get_prop_value (node, "lang");
	if (lang == NULL) {
		tag_id = g_strdup (node_name);
	} else {
		if (translatable) {
    		tag_id = g_strdup_printf ("%s (lang=%s)", node_name, lang);
		} else {
    		tag_id = g_strdup (node_name);
    		as_validator_add_issue (validator, node, "tag-not-translatable", node_name);
		}
	}

	if (g_hash_table_contains (known_tags, tag_id)) {
		as_validator_add_issue (validator, node,
					"tag-duplicated",
					tag_id);
	}

	/* add to list of known tags (takes ownership/frees tag_id) */
	g_hash_table_add (known_tags, tag_id);
}

static gboolean
as_validate_string_lowercase (const gchar *str)
{
	for (guint i = 0; str[i] != '\0'; i++) {
		if (g_ascii_isalpha (str[i]) && g_ascii_isupper (str[i]))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_validator_validate_component_id:
 *
 * Validate the component-ID.
 */
static void
as_validator_validate_component_id (AsValidator *validator, xmlNode *idnode, AsComponent *cpt)
{
	g_auto(GStrv) cid_parts = NULL;
	gboolean hyphen_found = FALSE;
	g_autofree gchar *cid = (gchar*) xmlNodeGetContent (idnode);
	g_return_if_fail (cid != NULL);

	if (cid[0] != '\0' && g_ascii_ispunct (cid[0]))
		as_validator_add_issue (validator, idnode, "cid-punctuation-prefix", cid);

	cid_parts = g_strsplit (cid, ".", -1);
	if (g_strv_length (cid_parts) < 3) {
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
			/* since the ID and .desktop-file-id are tied together, we can't make this an error for desktop apps */
			as_validator_add_issue (validator, idnode, "cid-desktopapp-is-not-rdns", cid);

		} else {
			/* anything which isn't a .desktop app must follow the schema though */
			as_validator_add_issue (validator, idnode, "cid-is-not-rdns", cid);
		}
	} else {
		/* some people just add random dots to their ID - check if we have an actual known TLD as first part, to be more certain that this is a reverse domain name
		 * (this issue happens quite often with old .desktop files) */
		if (!as_utils_is_tld (cid_parts[0])) {
			as_validator_add_issue (validator, idnode, "cid-maybe-not-rdns", cid);
		}

		/* ensure first parts of the rDNS ID are always lowercase */
		if (!as_validate_string_lowercase (cid_parts[0]))
			as_validator_add_issue (validator, idnode, "cid-domain-not-lowercase", cid);
		if (!as_validate_string_lowercase (cid_parts[1]))
			as_validator_add_issue (validator, idnode, "cid-domain-not-lowercase", cid);
	}

	/* validate characters in AppStream ID */
	for (guint i = 0; cid[i] != '\0'; i++) {
		/* check if we have a printable, alphanumeric ASCII character or a dot, hyphen or underscore */
		if ((!g_ascii_isalnum (cid[i])) &&
		    (cid[i] != '.') &&
		    (cid[i] != '-') &&
		    (cid[i] != '_')) {
			g_autofree gchar *c = NULL;
			c = g_utf8_substring (cid, i, i + 1);
			as_validator_add_issue (validator, idnode,
						"cid-invalid-character",
						"%s: '%s'", cid, c);
		}

		if (!hyphen_found && cid[i] == '-') {
			hyphen_found = TRUE;
			/* a hyphen in the ID is bad news, because we can't use the ID on DBus and it also clashes with other naming schemes */
			as_validator_add_issue (validator, idnode, "cid-contains-hyphen", cid);
		}

		if (g_ascii_isalpha (cid[i]) && g_ascii_isupper (cid[i]))
			as_validator_add_issue (validator, idnode, "cid-contains-uppercase-letter", cid);
	}

	/* check if any segment starts with a number */
	for (guint i = 0; cid_parts[i] != NULL; i++) {
		if (g_ascii_isdigit (cid_parts[i][0])) {
			as_validator_add_issue (validator, idnode,
						"cid-has-number-prefix",
						"%s: %s → _%s", cid, cid_parts[i], cid_parts[i]);
			break;
		}
	}

	/* project-group specific constraints on the ID */
	if ((g_strcmp0 (as_component_get_project_group (cpt), "Freedesktop") == 0) ||
	    (g_strcmp0 (as_component_get_project_group (cpt), "FreeDesktop") == 0)) {
		if (!g_str_has_prefix (cid, "org.freedesktop."))
			as_validator_add_issue (validator, idnode,
						"cid-missing-affiliation-freedesktop", cid);
	} else if (g_strcmp0 (as_component_get_project_group (cpt), "KDE") == 0) {
		if (!g_str_has_prefix (cid, "org.kde."))
			as_validator_add_issue (validator, idnode,
						"cid-missing-affiliation-kde", cid);
	} else if (g_strcmp0 (as_component_get_project_group (cpt), "GNOME") == 0) {
		if (!g_str_has_prefix (cid, "org.gnome."))
			as_validator_add_issue (validator, idnode,
						"cid-missing-affiliation-gnome", cid);
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
					"spdx-expression-invalid",
					license_id);
		return;
	}
	for (i = 0; licenses[i] != NULL; i++) {
		g_strstrip (licenses[i]);
		if (g_strcmp0 (licenses[i], "&") == 0 ||
		    g_strcmp0 (licenses[i], "|") == 0 ||
		    g_strcmp0 (licenses[i], "+") == 0 ||
		    g_strcmp0 (licenses[i], "(") == 0 ||
		    g_strcmp0 (licenses[i], ")") == 0 ||
		    g_strcmp0 (licenses[i], "^") == 0)
			continue;

		if (licenses[i][0] != '@') {
			if (!as_is_spdx_license_id (licenses[i] + 1) && !as_is_spdx_license_exception_id (licenses[i] + 1)) {
				as_validator_add_issue (validator, license_node,
						"spdx-license-unknown",
						licenses[i]);
				return;
			}
		}
	}
}

/**
 * as_validator_validate_metadata_license:
 */
static void
as_validator_validate_metadata_license (AsValidator *validator, xmlNode *license_node)
{
	gboolean requires_all_tokens = TRUE;
	guint license_bad_cnt = 0;
	guint license_good_cnt = 0;
	g_auto(GStrv) tokens = NULL;
	g_autofree gchar *license_expression = (gchar*) xmlNodeGetContent (license_node);

	tokens = as_spdx_license_tokenize (license_expression);
	if (tokens == NULL) {
		as_validator_add_issue (validator, license_node,
					"spdx-expression-invalid",
					license_expression);
		return;
	}

	/* this is too complicated to process */
	for (guint i = 0; tokens[i] != NULL; i++) {
		if (g_strcmp0 (tokens[i], "(") == 0 ||
		    g_strcmp0 (tokens[i], ")") == 0) {
			as_validator_add_issue (validator, license_node,
						"metadata-license-too-complex",
						license_expression);
			return;
		}
	}

	/* this is a simple expression parser and can be easily tricked */
	for (guint i = 0; tokens[i] != NULL; i++) {
		if (g_strcmp0 (tokens[i], "+") == 0)
			continue;
		if (g_strcmp0 (tokens[i], "|") == 0) {
			requires_all_tokens = FALSE;
			continue;
		}
		if (g_strcmp0 (tokens[i], "&") == 0) {
			requires_all_tokens = TRUE;
			continue;
		}
		if (as_license_is_metadata_license_id (tokens[i])) {
			license_good_cnt++;
		} else {
			license_bad_cnt++;
		}
	}

	/* any valid token makes this valid */
	if (!requires_all_tokens && license_good_cnt > 0)
		return;

	/* all tokens are required to be valid */
	if (requires_all_tokens && license_bad_cnt == 0)
		return;

	/* looks like the license was bad */
	as_validator_add_issue (validator, license_node,
				"metadata-license-invalid",
				license_expression);
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
						"update-contact-no-mail",
						text);
		}
	}
}

/**
 * as_id_string_valid:
 */
gboolean
as_id_string_valid (const gchar *str, gboolean allow_uppercase)
{
	if (str == NULL)
		return FALSE;

	for (guint i = 0; str[i] != '\0'; i++) {
		/* check if we have a printable, alphanumeric ASCII character or a dot, hyphen or underscore */
		if ((!g_ascii_isalnum (str[i])) &&
		    (str[i] != '.') &&
		    (str[i] != '-') &&
		    (str[i] != '_')) {
			/* found a character that is not whitelisted */
			return FALSE;
		}

		if (!allow_uppercase && g_ascii_isalpha (str[i]) && g_ascii_isupper (str[i]))
			return FALSE;
	}

	return TRUE;
}

/**
 * as_validator_check_tags:
 **/
static void
as_validator_check_tags (AsValidator *validator, xmlNode *node)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		g_autofree gchar *ns = NULL;
		g_autofree gchar *value = NULL;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		if (g_strcmp0 (node_name, "tag") != 0) {
			as_validator_add_issue (validator, node,
						"invalid-child-tag-name",
						/* TRANSLATORS: An invalid XML tag was found, "Found" refers to the tag name found, "Allowed" to the permitted name. */
						_("Found: %s - Allowed: %s"),
						(const gchar*) node->name,
						"tag");
			continue;
		}
		as_validator_check_content_empty (validator, iter, "tags/tag");

		ns = as_xml_get_prop_value (iter, "namespace");
		if (ns == NULL) {
			as_validator_add_issue (validator, iter, "component-tag-missing-namespace", NULL);
			continue;
		}

		if (!as_id_string_valid (ns, FALSE)) {
			as_validator_add_issue (validator, iter, "component-tag-invalid", ns);
			continue;
		}

		value = as_xml_get_node_value (iter);
		if (!as_id_string_valid (value, FALSE))
			as_validator_add_issue (validator, iter, "component-tag-invalid", value);
	}
}

/**
 * as_validator_check_screenshots:
 *
 * Validate a "screenshots" tag.
 **/
static void
as_validator_check_screenshots (AsValidator *validator, xmlNode *node, AsComponent *cpt)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		xmlNode *iter2;
		gboolean image_found = FALSE;
		gboolean video_found = FALSE;
		gboolean caption_found = FALSE;
		gboolean default_screenshot = FALSE;
		g_autofree gchar *scr_kind_str = NULL;

		if (iter->type != XML_ELEMENT_NODE)
			continue;

		scr_kind_str = as_xml_get_prop_value (iter, "type");
		if (g_strcmp0 (scr_kind_str, "default") == 0)
			default_screenshot = TRUE;

		if (g_strcmp0 ((const gchar*) iter->name, "screenshot") != 0) {
			as_validator_add_issue (validator, iter,
						"invalid-child-tag-name",
						/* TRANSLATORS: An invalid XML tag was found, "Found" refers to the tag name found, "Allowed" to the permitted name. */
						_("Found: %s - Allowed: %s"),
						(const gchar*) iter->name,
						"screenshot");
		}

		for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
			const gchar *node_name = (const gchar*) iter2->name;
			if (iter2->type != XML_ELEMENT_NODE)
				continue;

			if (g_strcmp0 (node_name, "image") == 0) {
				g_autofree gchar *image_url = as_strstripnl ((gchar*) xmlNodeGetContent (iter2));

				image_found = TRUE;
				if (!as_validate_is_url (image_url)) {
					as_validator_add_issue (validator, iter2,
								"web-url-expected",
								image_url);
					continue;
				}
				if (!as_validate_is_secure_url (image_url)) {
					as_validator_add_issue (validator, iter2,
								"screenshot-media-url-not-secure",
								image_url);
				}

				/* check if we can reach the URL */
				as_validator_check_web_url (validator,
							    iter2,
							    image_url,
							    "screenshot-image-not-found");
			} else if (g_strcmp0 (node_name, "video") == 0) {
				g_autofree gchar *codec_str = NULL;
				g_autofree gchar *container_str = NULL;
				g_autofree gchar *video_url_basename = NULL;
				g_autofree gchar *video_url_base_lower = NULL;
				g_autofree gchar *video_url = as_strstripnl ((gchar*) xmlNodeGetContent (iter2));

				video_found = TRUE;

				/* the default screenshot must not be a video */
				if (default_screenshot)
					as_validator_add_issue (validator, iter, "screenshot-default-contains-video", NULL);

				as_validator_check_web_url (validator,
							    iter2,
							    video_url,
							    "screenshot-video-not-found");

				if (!as_validate_is_url (video_url)) {
					as_validator_add_issue (validator, iter2,
								"web-url-expected",
								video_url);
					continue;
				}
				if (!as_validate_is_secure_url (video_url)) {
					as_validator_add_issue (validator, iter2,
								"screenshot-media-url-not-secure",
								video_url);
				}

				codec_str = as_xml_get_prop_value (iter2, "codec");
				if (codec_str == NULL) {
					as_validator_add_issue (validator, iter2, "screenshot-video-codec-missing", NULL);
				} else {
					AsVideoCodecKind codec = as_video_codec_kind_from_string (codec_str);
					if (codec == AS_VIDEO_CODEC_KIND_UNKNOWN)
						as_validator_add_issue (validator, iter2, "screenshot-video-codec-invalid", codec_str);
				}

				container_str = as_xml_get_prop_value (iter2, "container");
				if (container_str == NULL) {
					as_validator_add_issue (validator, iter2, "screenshot-video-container-missing", NULL);
				} else {
					AsVideoContainerKind container = as_video_container_kind_from_string (container_str);
					if (container == AS_VIDEO_CONTAINER_KIND_UNKNOWN)
						as_validator_add_issue (validator, iter2, "screenshot-video-container-invalid", container_str);
				}

				video_url_basename = as_filebasename_from_uri (video_url);
				video_url_base_lower = g_utf8_strdown (video_url_basename, -1);
				if (g_strstr_len (video_url_base_lower, -1, ".") != NULL) {
					if (!g_str_has_suffix (video_url_base_lower, ".mkv") &&
					    !g_str_has_suffix (video_url_base_lower, ".webm")) {
						as_validator_add_issue (validator, iter2, "screenshot-video-file-wrong-container", video_url_basename);
					}
				}

			} else if (g_strcmp0 (node_name, "caption") == 0) {
				caption_found = TRUE;
			} else {
				as_validator_add_issue (validator, iter2,
							"invalid-child-tag-name",
							/* TRANSLATORS: An invalid XML tag was found, "Found" refers to the tag name found, "Allowed" to the permitted name. */
							_("Found: %s - Allowed: %s"),
							(const gchar*) iter2->name,
							"caption; image; video");
			}
		}

		if (!image_found && !video_found)
			as_validator_add_issue (validator, iter, "screenshot-no-media", NULL);
		else if (image_found && video_found)
			as_validator_add_issue (validator, iter, "screenshot-mixed-images-videos", NULL);

		if (!caption_found)
			as_validator_add_issue (validator, iter, "screenshot-no-caption", NULL);
	}
}

/**
 * as_validator_check_relations:
 **/
static void
as_validator_check_relations (AsValidator *validator,
			      xmlNode *node,
			      AsComponent *cpt,
			      GHashTable *known_entries,
			      AsRelationKind kind)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		const gchar *rel_dupe_type = NULL;
		g_autofree gchar *content = NULL;
		g_autofree gchar *version = NULL;
		g_autofree gchar *compare_str = NULL;
		g_autofree gchar *rel_item_id = NULL;
		gboolean can_have_version = FALSE;
		gboolean can_have_compare = FALSE;
		AsRelationItemKind item_kind;
		AsRelationCompare compare;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;
		content = as_xml_get_node_value (iter);
		as_strstripnl (content);

		item_kind = as_relation_item_kind_from_string (node_name);
		if (item_kind == AS_RELATION_ITEM_KIND_UNKNOWN) {
			as_validator_add_issue (validator, iter,
						"relation-invalid-tag",
						node_name);
			continue;
		}

		if (g_strcmp0 (content, "") == 0) {
			/* only firmware relations are permitted to be empty, and only if the component is of type=firmware */
			if ((as_component_get_kind (cpt) != AS_COMPONENT_KIND_FIRMWARE) && (item_kind != AS_RELATION_ITEM_KIND_FIRMWARE)) {
				as_validator_add_issue (validator, iter,
							"relation-item-no-value",
							NULL);
				continue;
			}
		}

		/* check for circular relation */
		if (g_strcmp0 (content, as_component_get_id (cpt)) == 0) {
			as_validator_add_issue (validator, iter,
						"circular-component-relation",
						NULL);
		}

		switch (item_kind) {
		case AS_RELATION_ITEM_KIND_MODALIAS:
		case AS_RELATION_ITEM_KIND_CONTROL:
		case AS_RELATION_ITEM_KIND_HARDWARE:
		case AS_RELATION_ITEM_KIND_INTERNET:
			can_have_version = FALSE;
			can_have_compare = FALSE;
			break;
		case AS_RELATION_ITEM_KIND_MEMORY:
		case AS_RELATION_ITEM_KIND_DISPLAY_LENGTH:
			can_have_version = FALSE;
			can_have_compare = TRUE;
			break;
		default:
			can_have_version = TRUE;
			can_have_compare = TRUE;
		}

		version = as_xml_get_prop_value (iter, "version");
		compare_str = as_xml_get_prop_value (iter, "compare");
		compare = as_relation_compare_from_string (compare_str);
		if (version != NULL) {
			if (!can_have_version) {
				as_validator_add_issue (validator, iter,
						"relation-item-has-version",
						as_relation_item_kind_to_string (item_kind));
				continue;
			}

			if (compare_str == NULL) {
				as_validator_add_issue (validator, iter, "relation-item-missing-compare", NULL);
				continue;
			}
		}

		if (can_have_compare && compare == AS_RELATION_COMPARE_UNKNOWN) {
			as_validator_add_issue (validator, iter,
						"relation-item-invalid-vercmp",
						compare_str);
		} else if (!can_have_compare && compare_str != NULL) {
			as_validator_add_issue (validator, iter,
						"relation-item-has-vercmp",
						compare_str);
		}

		if (kind == AS_RELATION_KIND_REQUIRES) {
			if (item_kind == AS_RELATION_ITEM_KIND_MEMORY)
				as_validator_add_issue (validator, iter, "relation-memory-in-requires", NULL);
			else if (item_kind == AS_RELATION_ITEM_KIND_CONTROL)
				as_validator_add_issue (validator, iter, "relation-control-in-requires", NULL);
		}

		/* check input control names for sanity */
		if (item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
			if (as_control_kind_from_string (content) == AS_CONTROL_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "relation-control-value-invalid", content);
		}

		/* check display length for sanity */
		if (item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
			g_autofree gchar *side_str = NULL;
			if (as_display_length_kind_from_string (content) == AS_DISPLAY_LENGTH_KIND_UNKNOWN) {
				/* no text name, but we still may have an integer */
				if (!as_str_verify_integer (content, 1, G_MAXINT64))
					as_validator_add_issue (validator, iter, "relation-display-length-value-invalid", content);
			}

			side_str = as_xml_get_prop_value (iter, "side");
			if (as_display_side_kind_from_string (side_str) == AS_DISPLAY_SIDE_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "relation-display-length-side-property-invalid", side_str);
		}

		/* check hardware for sanity */
		if (item_kind == AS_RELATION_ITEM_KIND_HARDWARE) {
			guint dash_count = 0;
			for (guint i = 0; content[i] != '\0'; i++)
			     if (content[i] == '-')
				     dash_count++;

			if (g_str_has_prefix (content, "{") || g_str_has_suffix (content, "}"))
				as_validator_add_issue (validator, iter, "relation-hardware-value-invalid", content);
			else if (dash_count != 4)
				as_validator_add_issue (validator, iter, "relation-hardware-value-invalid", content);
		}

		/* check memory for sanity */
		if (item_kind == AS_RELATION_ITEM_KIND_MEMORY)
			if (!as_str_verify_integer (content, 1, G_MAXINT64))
				as_validator_add_issue (validator, iter, "relation-memory-value-invalid", content);

		/* check internet for sanity */
		if (item_kind == AS_RELATION_ITEM_KIND_INTERNET) {
			g_autofree gchar *bandwidth_str = as_xml_get_prop_value (iter, "bandwidth_mbitps");
			g_autofree gchar *internet_tag_id = g_strdup_printf ("rel::%s/internet", as_relation_kind_to_string (kind));
			if (as_internet_kind_from_string (content) == AS_INTERNET_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "relation-internet-value-invalid", content);
			if (bandwidth_str != NULL && !as_str_verify_integer (bandwidth_str, 1, G_MAXINT64))
				as_validator_add_issue (validator, iter, "relation-internet-bandwidth-value-invalid", bandwidth_str);

			/* the internet item must only appear once per relation kind */
			if (g_hash_table_lookup (known_entries, internet_tag_id) == NULL)
				g_hash_table_insert (known_entries,
							g_steal_pointer (&internet_tag_id),
							g_strdup (content));
			else
				as_validator_add_issue (validator, iter, "tag-duplicated", "internet");
		}

		/* check for redefinition */
		rel_item_id = g_strdup_printf ("rel::%s/%s%s%s", node_name, content, compare_str, version);
		rel_dupe_type = g_hash_table_lookup (known_entries, rel_item_id);
		if (rel_dupe_type == NULL) {
			g_hash_table_insert (known_entries,
					     g_steal_pointer (&rel_item_id),
					     g_strdup (as_relation_kind_to_string (kind)));
		} else {
			as_validator_add_issue (validator,
						iter, "relation-item-redefined",
						"%s & %s", rel_dupe_type, as_relation_kind_to_string (kind));
		}
	}
}

/**
 * as_validator_check_provides:
 **/
static void
as_validator_check_provides (AsValidator *validator, xmlNode *node, AsComponent *cpt)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		g_autofree gchar *node_content = NULL;
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;
		node_content = as_xml_get_node_value (iter);
		g_strstrip (node_content);
		if (as_is_empty (node_content)) {
			as_validator_add_issue (validator, iter,
						"tag-empty",
						"%s", node_name);
		}

		if (g_strcmp0 (node_name, "library") == 0) {
		} else if (g_strcmp0 (node_name, "binary") == 0) {
		} else if (g_strcmp0 (node_name, "font") == 0) {
		} else if (g_strcmp0 (node_name, "modalias") == 0) {
		} else if (g_strcmp0 (node_name, "firmware") == 0) {
		} else if (g_strcmp0 (node_name, "python2") == 0) {
		} else if (g_strcmp0 (node_name, "python3") == 0) {
		} else if (g_strcmp0 (node_name, "dbus") == 0) {
		} else if (g_strcmp0 (node_name, "mediatype") == 0) {
		} else if (g_strcmp0 (node_name, "id") == 0) {
			/* check for circular relation */
			if (g_strcmp0 (node_content, as_component_get_id (cpt)) == 0) {
				as_validator_add_issue (validator, iter,
							"circular-component-relation",
							NULL);
			}
		} else {
			as_validator_add_issue (validator, iter,
						"unknown-provides-item-type",
						"%s", node_name);
		}
	}
}

/**
 * as_validator_validate_iso8601_complete_date:
 */
static void
as_validator_validate_iso8601_complete_date (AsValidator *validator, xmlNode *node, const gchar *date)
{
	g_autoptr(GDateTime) time = as_iso8601_to_datetime (date);
	if (time == NULL)
		as_validator_add_issue (validator, node, "invalid-iso8601-date", "%s", date, NULL);
}

/**
 * as_validator_check_platform_triplet:
 **/
static void
as_validator_check_platform_triplet (AsValidator *validator, xmlNode *node, const gchar *triplet)
{
	g_auto(GStrv) parts = g_strsplit (triplet, "-", 3);
	if (g_strv_length (parts) != 3) {
		as_validator_add_issue (validator, node,
					"artifact-invalid-platform-triplet",
					"%u != 3", g_strv_length (parts));
		return;
	}
	if (!as_utils_is_platform_triplet_arch (parts[0])) {
		as_validator_add_issue (validator, node,
					"artifact-invalid-platform-triplet",
					"Architecture invalid: %s", parts[0]);
		return;
	}
	if (!as_utils_is_platform_triplet_oskernel (parts[1])) {
		as_validator_add_issue (validator, node,
					"artifact-invalid-platform-triplet",
					"OS/Kernel invalid: %s", parts[1]);
		return;
	}
	if (!as_utils_is_platform_triplet_osenv (parts[2])) {
		as_validator_add_issue (validator, node,
					"artifact-invalid-platform-triplet",
					"OS/Environment invalid: %s", parts[2]);
		return;
	}
}

/**
 * as_validator_check_artifact:
 **/
static void
as_validator_check_artifact (AsValidator *validator, xmlNode *node)
{
	gchar *prop;

	/* validate type */
	prop = as_xml_get_prop_value (node, "type");
	if (prop != NULL) {
		if (as_artifact_kind_from_string (prop) == AS_ARTIFACT_KIND_UNKNOWN)
			as_validator_add_issue (validator, node, "artifact-type-invalid", prop);
		g_free (prop);
	}

	/* validate platform */
	prop = as_xml_get_prop_value (node, "platform");
	if (prop != NULL) {
		as_validator_check_platform_triplet (validator, node, prop);
		g_free (prop);
	}

	/* validate bundle type */
	prop = as_xml_get_prop_value (node, "bundle");
	if (prop != NULL) {
		if (as_bundle_kind_from_string (prop) == AS_BUNDLE_KIND_UNKNOWN)
			as_validator_add_issue (validator, node, "artifact-bundle-type-invalid", prop);
		g_free (prop);
	}

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		if (g_strcmp0 (node_name, "location") == 0) {
			g_autofree gchar *url = as_xml_get_node_value (iter);
			if (!as_validate_is_url (url)) {
				as_validator_add_issue (validator, iter, "web-url-expected", url);
				continue;
			}
			if (!as_validate_is_secure_url (url))
				as_validator_add_issue (validator, iter, "url-not-secure", url);
			continue;
		} else if (g_strcmp0 (node_name, "checksum") == 0) {
			g_autofree gchar *tmp = as_xml_get_prop_value (iter, "type");
			if (as_checksum_kind_from_string (tmp) == AS_CHECKSUM_KIND_NONE)
				as_validator_add_issue (validator, iter, "artifact-checksum-type-invalid", tmp);
			continue;
		} else if (g_strcmp0 (node_name, "size") == 0) {
			g_autofree gchar *tmp = as_xml_get_prop_value (iter, "type");
			if (as_size_kind_from_string (tmp) == AS_SIZE_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "artifact-size-type-invalid", tmp);
			continue;
		} else if (g_strcmp0 (node_name, "filename") == 0) {
			g_autofree gchar *fname = as_xml_get_node_value (iter);
			if (g_path_is_absolute (fname) || g_str_has_prefix (fname, "."))
				as_validator_add_issue (validator, iter, "artifact-filename-not-basename", fname);
			continue;
		} else {
			/* unknown tag */
			as_validator_add_issue (validator, iter, "unknown-tag", node_name);
		}
	}
}

/**
 * as_validator_check_release_issue:
 **/
static void
as_validator_check_release_issue (AsValidator *validator, xmlNode *node)
{
	gchar *prop;
	AsIssueKind kind = AS_ISSUE_KIND_UNKNOWN;

	/* validate type */
	prop = as_xml_get_prop_value (node, "type");
	kind = as_issue_kind_from_string (prop);
	if (prop != NULL) {
		if (kind == AS_ISSUE_KIND_UNKNOWN)
			as_validator_add_issue (validator, node, "release-issue-type-invalid", prop);
		g_free (prop);
	}

	/* validate url */
	prop = as_xml_get_prop_value (node, "url");
	if (prop != NULL) {
		if (!as_validate_is_url (prop))
			as_validator_add_issue (validator, node, "web-url-expected", prop);
		else if (!as_validate_is_secure_url (prop))
			as_validator_add_issue (validator, node, "url-not-secure", prop);
		g_free (prop);
	}

	/* validate value */
	if (kind == AS_ISSUE_KIND_CVE) {
		g_autofree gchar *value = as_xml_get_node_value (node);
		if (!g_str_has_prefix (value, "CVE-"))
			as_validator_add_issue (validator, node, "release-issue-is-cve-but-no-cve-id", value);
	}
}

/**
 * as_validator_check_release:
 **/
static void
as_validator_check_release (AsValidator *validator, xmlNode *node, AsFormatStyle mode)
{
	gchar *prop;
	AsReleaseKind rel_kind = AS_RELEASE_KIND_UNKNOWN;

	/* validate presence of version property */
	prop = as_xml_get_prop_value (node, "version");
	if (prop == NULL)
		as_validator_add_issue (validator, node, "release-version-missing", "version");
	g_free (prop);

	/* validate type */
	prop = as_xml_get_prop_value (node, "type");
	if (prop != NULL) {
		rel_kind = as_release_kind_from_string (prop);
		if (rel_kind == AS_RELEASE_KIND_UNKNOWN)
			as_validator_add_issue (validator, node, "release-type-invalid", prop);
		g_free (prop);
	}

	/* validate date strings */
	prop = as_xml_get_prop_value (node, "date");
	if (prop != NULL) {
		as_validator_validate_iso8601_complete_date (validator, node, prop);
		g_free (prop);
	} else {
		g_autofree gchar *timestamp = as_xml_get_prop_value (node, "timestamp");
		if (timestamp == NULL) {
			/* Neither timestamp, nor date property exists */
			as_validator_add_issue (validator, node, "release-time-missing", "date");
		} else {
			if (as_str_verify_integer (timestamp, 3000, G_MAXINT64)) {
				/* check if the timestamp is both a number and higher than 3000. The 3000 is used to check that it is not a year */
				as_validator_add_issue (validator, node, "release-timestamp-invalid", timestamp);
			}
		}
	}

	prop = as_xml_get_prop_value (node, "date_eol");
	if (prop != NULL) {
		as_validator_validate_iso8601_complete_date (validator, node, prop);
		g_free (prop);
	}

	/* validate urgency */
	prop = as_xml_get_prop_value (node, "urgency");
	if (prop != NULL) {
		if (as_urgency_kind_from_string (prop) == AS_URGENCY_KIND_UNKNOWN)
			as_validator_add_issue (validator, node, "release-urgency-invalid", prop);
		g_free (prop);
	}

	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		/* validate description */
		if (g_strcmp0 (node_name, "description") == 0) {
			as_validator_check_description_tag (validator, iter, mode, FALSE);
			continue;
		}

		/* validate url */
		if (g_strcmp0 (node_name, "url") == 0) {
			g_autofree gchar *url = as_xml_get_node_value (iter);
			if (!as_validate_is_url (url)) {
				as_validator_add_issue (validator, iter, "web-url-expected", url);
				continue;
			}
			if (!as_validate_is_secure_url (url))
				as_validator_add_issue (validator, iter, "url-not-secure", url);
			continue;
		}

		/* validate artifacts */
		if (g_strcmp0 (node_name, "artifacts") == 0) {
			for (xmlNode *iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (iter2->type != XML_ELEMENT_NODE)
					continue;
				if (g_strcmp0 ((const gchar*) iter2->name, "artifact") != 0) {
					as_validator_add_issue (validator, iter2,
								"invalid-child-tag-name",
								/* TRANSLATORS: An invalid XML tag was found, "Found" refers to the tag name found, "Allowed" to the permitted name. */
								_("Found: %s - Allowed: %s"),
								(const gchar*) iter2->name,
								"artifact");
					continue;
				}
				/* validate artifact */
				as_validator_check_artifact (validator, iter2);
			}
			continue;
		}

		/* validate issues */
		if (g_strcmp0 (node_name, "issues") == 0) {
			for (xmlNode *iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (iter2->type != XML_ELEMENT_NODE)
					continue;
				if (g_strcmp0 ((const gchar*) iter2->name, "issue") != 0) {
					as_validator_add_issue (validator, iter2,
								"invalid-child-tag-name",
								/* TRANSLATORS: An invalid XML tag was found, "Found" refers to the tag name found, "Allowed" to the permitted name. */
								_("Found: %s - Allowed: %s"),
								(const gchar*) iter2->name,
								"issue");
					continue;
				}
				/* validate issue */
				as_validator_check_release_issue (validator, iter2);
			}
			continue;
		}

		/* if we are here, we have an unknown tag */
		as_validator_add_issue (validator, iter, "unknown-tag", node_name);
	}
}

/**
 * as_validator_check_releases:
 **/
static void
as_validator_check_releases (AsValidator *validator, xmlNode *node, AsFormatStyle mode)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;

		if (g_strcmp0 (node_name, "release") != 0) {
			as_validator_add_issue (validator, iter,
						"invalid-child-tag-name",
						/* TRANSLATORS: An invalid XML tag was found, "Found" refers to the tag name found, "Allowed" to the permitted name. */
						_("Found: %s - Allowed: %s"),
						(const gchar*) iter->name,
						"release");
			continue;
		}

		/* validate the individual release */
		as_validator_check_release (validator, iter, mode);
	}
}

/**
 * as_validator_check_branding:
 **/
static void
as_validator_check_branding (AsValidator *validator, xmlNode *node)
{
	for (xmlNode *iter = node->children; iter != NULL; iter = iter->next) {
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "color") == 0) {
			gchar *tmp;
			guint len;

			tmp = as_xml_get_prop_value (iter, "type");
			if (as_color_kind_from_string (tmp) == AS_COLOR_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "branding-color-type-invalid", tmp);
			g_free (tmp);

			tmp = as_xml_get_prop_value (iter, "scheme_preference");
			if (tmp != NULL && as_color_scheme_kind_from_string (tmp) == AS_COLOR_SCHEME_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "branding-color-scheme-type-invalid", tmp);
			g_free (tmp);

			tmp = as_xml_get_node_value (iter);
			len = strlen (tmp);
			if (!g_str_has_prefix (tmp, "#") || (len != 7 && len != 9))
				as_validator_add_issue (validator, iter, "branding-color-invalid", tmp);
		} else {
			as_validator_add_issue (validator, iter,
						"invalid-child-tag-name",
						/* TRANSLATORS: An invalid XML tag was found, "Found" refers to the tag name found, "Allowed" to the permitted names. */
						_("Found: %s - Allowed: %s"),
						(const gchar*) iter->name,
						"color");
		}
	}
}

/**
 * as_validator_validate_component_node:
 **/
static AsComponent*
as_validator_validate_component_node (AsValidator *validator, AsContext *ctx, xmlNode *root)
{
	AsComponent *cpt;
	g_autofree gchar *cpttype = NULL;
	g_autoptr(GHashTable) found_tags = NULL;
	g_autoptr(GHashTable) known_relation_items = NULL;
	g_autofree gchar *date_eol_str = NULL;
	guint64 known_url_kinds = 0;

	AsFormatStyle mode;
	gboolean has_metadata_license = FALSE;

	/* hash tables for finding duplicates */
	found_tags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	known_relation_items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	mode = as_context_get_style (ctx);

	/* validate the resulting AsComponent for sanity */
	cpt = as_component_new ();
	as_component_load_from_xml (cpt, ctx, root, NULL);
	as_component_set_active_locale (cpt, "C");
	as_validator_set_current_cpt (validator, cpt);

	/* check if component type is valid */
	cpttype = as_xml_get_prop_value (root, (xmlChar*) "type");
	if (cpttype != NULL) {
		if (as_component_kind_from_string (cpttype) == AS_COMPONENT_KIND_UNKNOWN) {
			as_validator_add_issue (validator, root,
						"component-type-invalid",
						cpttype);
		}
	}

	/* validate EOL date format */
	date_eol_str = as_xml_get_prop_value (root, "date_eol");
	if (date_eol_str != NULL)
		as_validator_validate_iso8601_complete_date (validator, root, date_eol_str);

	/* validate other component properties */
	if ((as_component_get_priority (cpt) != 0) && (mode == AS_FORMAT_STYLE_METAINFO))
		as_validator_add_issue (validator, root, "component-priority-in-metainfo", NULL);

	if ((as_component_get_merge_kind (cpt) != AS_MERGE_KIND_NONE) && (mode == AS_FORMAT_STYLE_METAINFO))
		as_validator_add_issue (validator, root, "component-merge-in-metainfo", NULL);

	/* the component must have an id */
	if (as_is_empty (as_component_get_id (cpt))) {
		/* we don't have an id */
		as_validator_add_issue (validator, NULL, "component-id-missing", NULL);
	}

	/* the component must have a name */
	if (as_is_empty (as_component_get_name (cpt))) {
		/* we don't have a name */
		as_validator_add_issue (validator, NULL, "component-name-missing", NULL);
	}

	/* the component must have a summary */
	if (as_is_empty (as_component_get_summary (cpt))) {
		/* we don't have a summary */
		as_validator_add_issue (validator, NULL, "component-summary-missing", NULL);
	}

	for (xmlNode *iter = root->children; iter != NULL; iter = iter->next) {
		const gchar *node_name;
		g_autofree gchar *node_content = NULL;
		gboolean tag_valid = TRUE;
		gboolean can_be_empty = FALSE;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		node_name = (const gchar*) iter->name;
		node_content = (gchar*) xmlNodeGetContent (iter);
		if (node_content != NULL)
			node_content = as_strstripnl (node_content);

		if (g_strcmp0 (node_name, "id") == 0) {
			g_autofree gchar *prop = as_xml_get_prop_value (iter, (xmlChar*) "type");
			if (prop != NULL) {
				as_validator_add_issue (validator, iter, "id-tag-has-type", node_content);
			}

			/* validate the AppStream ID */
			as_validator_validate_component_id (validator, iter, cpt);
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
		} else if (g_strcmp0 (node_name, "metadata_license") == 0) {
			has_metadata_license = TRUE;
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);

			/* the license must allow easy mixing of metadata in metainfo files */
			if (mode == AS_FORMAT_STYLE_METAINFO)
				as_validator_validate_metadata_license (validator, iter);
		} else if (g_strcmp0 (node_name, "pkgname") == 0) {
			if (g_hash_table_contains (found_tags, node_name))
				as_validator_add_issue (validator, iter, "multiple-pkgname", NULL);
		} else if (g_strcmp0 (node_name, "source_pkgname") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
		} else if (g_strcmp0 (node_name, "name") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, TRUE);
			if (g_str_has_suffix (node_content, "."))
				as_validator_add_issue (validator, iter, "name-has-dot-suffix", node_content);

		} else if (g_strcmp0 (node_name, "summary") == 0) {
			g_autofree gchar *lang = NULL;
			const gchar *summary = node_content;

			as_validator_check_appear_once (validator, iter, found_tags, TRUE);
			if (g_str_has_suffix (summary, "."))
				as_validator_add_issue (validator, iter,
							"summary-has-dot-suffix",
							summary);

			if ((summary != NULL) && ((strstr (summary, "\n") != NULL) || (strstr (summary, "\t") != NULL))) {
				as_validator_add_issue (validator, iter, "summary-has-tabs-or-linebreaks", NULL);
			}

			if (as_validate_has_hyperlink (summary)) {
				as_validator_add_issue (validator, iter,
							"summary-has-url",
							node_name);
			}

			lang = as_xml_get_prop_value (iter, "lang");
			if (lang == NULL && !as_validator_first_word_capitalized (validator, summary))
				as_validator_add_issue (validator, iter,
							"summary-first-word-not-capitalized",
							NULL);

		} else if (g_strcmp0 (node_name, "description") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, TRUE);
			as_validator_check_description_tag (validator, iter, mode, TRUE);
		} else if (g_strcmp0 (node_name, "icon") == 0) {
			g_autofree gchar *prop = as_validator_check_type_property (validator, cpt, iter);
			if ((g_strcmp0 (prop, "cached") == 0) || (g_strcmp0 (prop, "stock") == 0)) {
				if ((g_strrstr (node_content, "/") != NULL) || (as_validate_is_url (node_content)))
					as_validator_add_issue (validator, iter, "icon-stock-cached-has-url", NULL);
			}

			if (g_strcmp0 (prop, "remote") == 0) {
				if (as_validate_is_url (node_content)) {
					if (!as_validate_is_secure_url (node_content))
						as_validator_add_issue (validator, iter, "icon-remote-not-secure", node_content);

					as_validator_check_web_url (validator,
								    iter,
								    node_content,
								    "icon-remote-not-found");
				} else {
					as_validator_add_issue (validator, iter, "icon-remote-no-url", node_content);
				}
			}

			if (mode == AS_FORMAT_STYLE_METAINFO) {
				if ((prop != NULL) && (g_strcmp0 (prop, "stock") != 0) && (g_strcmp0 (prop, "remote") != 0))
					as_validator_add_issue (validator, iter, "metainfo-invalid-icon-type", prop);
			}

		} else if (g_strcmp0 (node_name, "url") == 0) {
			AsUrlKind url_kind;
			g_autofree gchar *prop = as_validator_check_type_property (validator, cpt, iter);

			url_kind = as_url_kind_from_string (prop);
			if (url_kind == AS_URL_KIND_UNKNOWN) {
				as_validator_add_issue (validator, iter, "url-invalid-type", prop);
			} else {
				/* check for URL kind duplicates */
				guint64 url_kind_flag = 1 << url_kind;
				if (as_flags_contains (known_url_kinds, url_kind_flag))
					as_validator_add_issue (validator,
								iter,
								"url-redefined",
								as_url_kind_to_string (url_kind));
				else
					as_flags_add (known_url_kinds, url_kind_flag);
			}

			if (!as_validate_is_url (node_content)) {
				as_validator_add_issue (validator, iter, "web-url-expected", node_content);
				continue;
			}

			if (!as_validate_is_secure_url (node_content))
				as_validator_add_issue (validator, iter, "url-not-secure", node_content);

			as_validator_check_web_url (validator,
						    iter,
						    node_content,
						    "url-not-reachable");

		} else if (g_strcmp0 (node_name, "categories") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_children_quick (validator, iter, "category", FALSE);
		} else if (g_strcmp0 (node_name, "keywords") == 0) {
			as_validator_check_children_quick (validator, iter, "keyword", FALSE);

			if (mode == AS_FORMAT_STYLE_METAINFO) {
				as_validator_check_appear_once (validator, iter, found_tags, TRUE);
				as_validator_check_nolocalized (validator,
								iter,
								"metainfo-localized-keywords-tag",
								(const gchar*) iter->name);
			}
		} else if (g_strcmp0 (node_name, "mimetypes") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_children_quick (validator, iter, "mimetype", FALSE);
			as_validator_add_issue (validator, iter,
						"mimetypes-tag-deprecated",
						NULL);
		} else if (g_strcmp0 (node_name, "provides") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_provides (validator, iter, cpt);
		} else if (g_strcmp0 (node_name, "screenshots") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_screenshots (validator, iter, cpt);
		} else if (g_strcmp0 (node_name, "project_license") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_validate_project_license (validator, iter);
		} else if (g_strcmp0 (node_name, "project_group") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
		} else if (g_strcmp0 (node_name, "developer_name") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, TRUE);

			if (as_validate_has_hyperlink (node_content))
				as_validator_add_issue (validator, iter, "developer-name-has-url", NULL);

		} else if (g_strcmp0 (node_name, "compulsory_for_desktop") == 0) {
			if (!as_utils_is_desktop_environment (node_content)) {
				as_validator_add_issue (validator, iter,
							"unknown-desktop-id",
							node_content);
			}
		} else if (g_strcmp0 (node_name, "releases") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_releases (validator, iter, mode);
			can_be_empty = TRUE;
		} else if (g_strcmp0 (node_name, "languages") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_children_quick (validator, iter, "lang", FALSE);
		} else if ((g_strcmp0 (node_name, "translation") == 0) && (mode == AS_FORMAT_STYLE_METAINFO)) {
			g_autofree gchar *prop = NULL;
			AsTranslationKind trkind;
			prop = as_validator_check_type_property (validator, cpt, iter);
			trkind = as_translation_kind_from_string (prop);
			if (prop != NULL && trkind == AS_TRANSLATION_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "unknown-desktop-id", prop);

		} else if (g_strcmp0 (node_name, "launchable") == 0) {
			g_autofree gchar *prop = as_validator_check_type_property (validator, cpt, iter);
			if (as_launchable_kind_from_string (prop) == AS_LAUNCHABLE_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "launchable-unknown-type", prop);

		} else if (g_strcmp0 (node_name, "extends") == 0) {
			/* check for circular extends */
			if (g_strcmp0 (node_content, as_component_get_id (cpt)) == 0) {
				as_validator_add_issue (validator, iter,
							"circular-component-relation",
							NULL);
			}

		} else if (g_strcmp0 (node_name, "bundle") == 0) {
			g_autofree gchar *prop = as_validator_check_type_property (validator, cpt, iter);
			if (prop != NULL && as_bundle_kind_from_string (prop) == AS_BUNDLE_KIND_UNKNOWN)
				as_validator_add_issue (validator, iter, "bundle-unknown-type", prop);

		} else if (g_strcmp0 (node_name, "update_contact") == 0) {
			if (mode == AS_FORMAT_STYLE_COLLECTION) {
				as_validator_add_issue (validator, iter, "update-contact-in-collection-data", NULL);
			} else {
				as_validator_check_appear_once (validator, iter, found_tags, FALSE);
				as_validator_validate_update_contact (validator, iter);
			}
		} else if (g_strcmp0 (node_name, "suggests") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_children_quick (validator, iter, "id", FALSE);
		} else if (g_strcmp0 (node_name, "content_rating") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_children_quick (validator, iter, "content_attribute", TRUE);
			can_be_empty = TRUE;
		} else if (g_strcmp0 (node_name, "replaces") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_children_quick (validator, iter, "id", FALSE);
		} else if (g_strcmp0 (node_name, "requires") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_relations (validator,
						      iter,
						      cpt,
						      known_relation_items,
						      AS_RELATION_KIND_REQUIRES);
		} else if (g_strcmp0 (node_name, "recommends") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_relations (validator,
						      iter,
						      cpt,
						      known_relation_items,
						      AS_RELATION_KIND_RECOMMENDS);
		} else if (g_strcmp0 (node_name, "supports") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_relations (validator,
						      iter,
						      cpt,
						      known_relation_items,
						      AS_RELATION_KIND_SUPPORTS);
		} else if (g_strcmp0 (node_name, "agreement") == 0) {
			as_validator_check_children_quick (validator, iter, "agreement_section", FALSE);
		} else if (g_strcmp0 (node_name, "branding") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_branding (validator, iter);
		} else if (g_strcmp0 (node_name, "tags") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_tags (validator, iter);
		} else if (g_strcmp0 (node_name, "name_variant_suffix") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
		} else if (g_strcmp0 (node_name, "custom") == 0) {
			as_validator_check_appear_once (validator, iter, found_tags, FALSE);
			as_validator_check_children_quick (validator, iter, "value", FALSE);
		} else if ((g_strcmp0 (node_name, "metadata") == 0) || (g_strcmp0 (node_name, "kudos") == 0)) {
			/* these tags are GNOME / Fedora specific extensions and are therefore quite common. They shouldn't make the validation fail,
			 * especially if we might standardize at leat the <kudos/> tag one day, but we should still complain about those tags to make
			 * it obvious that they are not supported by all implementations */
			as_validator_add_issue (validator, iter, "nonstandard-gnome-extension", node_name);
			tag_valid = FALSE;
		} else if (!g_str_has_prefix (node_name, "x-")) {
			as_validator_add_issue (validator, iter, "unknown-tag", node_name);
			tag_valid = FALSE;
		}

		if (tag_valid && !can_be_empty) {
			as_validator_check_content_empty (validator,
							  iter,
							  node_name);
		}
	}

	/* emit an error if we are missing the metadata license in metainfo files */
	if ((!has_metadata_license) && (mode == AS_FORMAT_STYLE_METAINFO))
		as_validator_add_issue (validator, NULL, "metadata-license-missing", NULL);

	/* check if we have a description */
	if (as_is_empty (as_component_get_description (cpt))) {
		AsComponentKind cpt_kind;
		cpt_kind = as_component_get_kind (cpt);

		if ((cpt_kind == AS_COMPONENT_KIND_DESKTOP_APP) ||
		    (cpt_kind == AS_COMPONENT_KIND_CONSOLE_APP) ||
		    (cpt_kind == AS_COMPONENT_KIND_WEB_APP)) {
			as_validator_add_issue (validator, NULL, "app-description-required", NULL);
		} else if (cpt_kind == AS_COMPONENT_KIND_FONT) {
			as_validator_add_issue (validator, NULL, "font-description-missing", NULL);
		} else if ((cpt_kind == AS_COMPONENT_KIND_DRIVER) || (cpt_kind == AS_COMPONENT_KIND_FIRMWARE)) {
			as_validator_add_issue (validator, NULL, "driver-firmware-description-missing", NULL);
		} else if (cpt_kind != AS_COMPONENT_KIND_GENERIC) {
			as_validator_add_issue (validator, NULL, "generic-description-missing", NULL);
		}
	}

	/* validate GUI app specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
		AsIcon *stock_icon = NULL;
		AsLaunchable *launchable = NULL;

		/* try to find desktop-id launchable */
		launchable = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);

		/* try to find a stock icon for this component */
		stock_icon = as_component_get_icon_stock (cpt);
		if (stock_icon == NULL) {
			/* no stock icon, we require a desktop-entry file association */
			if (launchable == NULL || as_launchable_get_entries (launchable)->len == 0) {
				/* legacy compatibility: this is only an error if the component ID doesn't have a .desktop suffix */
				if (g_str_has_suffix (as_component_get_id (cpt), ".desktop"))
					as_validator_add_issue (validator, NULL, "desktop-app-launchable-omitted", NULL);
				else
					as_validator_add_issue (validator, NULL, "desktop-app-launchable-missing", NULL);
			}
		} else {
			/* this app has a stock icon, so we may not need a desktop-entry file */

			/* check if we have any categories */
			if (as_component_get_categories (cpt)->len == 0) {
				as_validator_add_issue (validator, NULL,
							"app-categories-missing",
							NULL);
			}

			/* check for launchable, but this time the omission may actually be intended */
			if (launchable == NULL || as_launchable_get_entries (launchable)->len == 0)
				as_validator_add_issue (validator, NULL, "desktop-app-launchable-omitted", NULL);
		}
	}

	/* validate console-app specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_CONSOLE_APP) {
		if (as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_BINARY) == NULL)
			as_validator_add_issue (validator, NULL, "console-app-no-binary", NULL);
	}

	/* validate webapp specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_WEB_APP) {
		AsLaunchable *launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_URL);
		if (launch == NULL || as_launchable_get_entries (launch)->len == 0)
			as_validator_add_issue (validator, NULL, "web-app-no-url-launchable", NULL);

		if (as_component_get_icons (cpt)->len <= 0)
			as_validator_add_issue (validator, NULL, "web-app-no-icon", NULL);

		if (as_component_get_categories (cpt)->len <= 0) {
			as_validator_add_issue (validator, NULL, "web-app-no-category", NULL);
		}
	}

	/* validate font specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_FONT) {
		if (as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_FONT) == NULL)
			as_validator_add_issue (validator, NULL, "font-no-font-data", NULL);
	}

	/* validate driver specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DRIVER) {
		if (as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_MODALIAS) == NULL)
			as_validator_add_issue (validator, NULL, "driver-no-modalias", NULL);
	}

	/* validate addon specific stuff */
	if (as_component_get_extends (cpt)->len > 0) {
		AsComponentKind kind = as_component_get_kind (cpt);
		if ((kind != AS_COMPONENT_KIND_ADDON) &&
		    (kind != AS_COMPONENT_KIND_LOCALIZATION) &&
		    (kind != AS_COMPONENT_KIND_REPOSITORY))
			as_validator_add_issue (validator, NULL, "extends-not-allowed", NULL);
	} else {
		if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_ADDON)
			as_validator_add_issue (validator, NULL, "addon-extends-missing", NULL);
	}

	/* validate l10n specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_LOCALIZATION) {
		if (as_component_get_extends (cpt)->len == 0) {
			as_validator_add_issue (validator, NULL, "localization-extends-missing", NULL);
		}
		if (g_hash_table_size (as_component_get_languages_table (cpt)) == 0) {
			as_validator_add_issue (validator, NULL, "localization-no-languages", NULL);
		}
	}

	/* validate service specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_SERVICE) {
		AsLaunchable *launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_SERVICE);
		if (launch == NULL || as_launchable_get_entries (launch)->len == 0)
			as_validator_add_issue (validator, NULL, "service-no-service-launchable", NULL);
	}

	/* validate runtime specific stuff */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_RUNTIME) {
		const gchar *project_license = as_component_get_project_license (cpt);
		if ((project_license == NULL) || (!g_str_has_prefix (project_license, "LicenseRef")))
			as_validator_add_issue (validator, NULL, "runtime-project-license-no-ref", NULL);

		if (as_component_get_provided (cpt)->len == 0)
			as_validator_add_issue (validator, NULL, "runtime-no-provides", NULL);
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
								"metainfo-suggestion-type-invalid",
								as_suggested_kind_to_string (as_suggested_get_kind (prov)));
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
							"category-invalid",
							category_name);
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

			if ((scr_caption != NULL) && (strlen (scr_caption) > 100)) {
				as_validator_add_issue (validator, NULL,
							"screenshot-caption-too-long",
							scr_caption);
			}
		}
	}

	/* validate releases */
	if (as_component_get_releases (cpt)->len > 0) {
		GPtrArray *releases = as_component_get_releases (cpt);
		AsRelease *release_prev = g_ptr_array_index (releases, 0);

		for (guint i = 1; i < releases->len; i++) {
			AsRelease *release = g_ptr_array_index (releases, i);
			const gchar *version = as_release_get_version (release);
			const gchar *version_prev = as_release_get_version (release_prev);
			if (version == NULL || version_prev == NULL)
				continue;
			if (as_vercmp_simple (version_prev, version) < 0) {
				as_validator_add_issue (validator, NULL,
							"releases-not-in-order",
							"%s << %s",
							version_prev,
							version);
			}
			release_prev = release;
		}
	} else {
		/* we have no release information! */
		AsComponentKind kind = as_component_get_kind (cpt);
		if (kind == AS_COMPONENT_KIND_DESKTOP_APP ||
		    kind == AS_COMPONENT_KIND_CONSOLE_APP) {
			/* show an info about missing age rating for application-type components */
			as_validator_add_issue (validator, NULL, "releases-info-missing", NULL);
		}
	}

	/* check content_rating */
	if (as_component_get_content_ratings (cpt)->len == 0) {
		AsComponentKind kind = as_component_get_kind (cpt);
		if (kind == AS_COMPONENT_KIND_DESKTOP_APP ||
		    kind == AS_COMPONENT_KIND_CONSOLE_APP ||
		    kind == AS_COMPONENT_KIND_WEB_APP) {
			/* show an info about missing age rating for application-type components */
			as_validator_add_issue (validator, NULL, "content-rating-missing", NULL);
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
 * Validate an AppStream XML file.
 * Remember to run %as_validator_clear_issues if you do not want previous
 * validation runs to affect the outcome of this validation.
 *
 * Returns: %TRUE if file validated successfully.
 **/
gboolean
as_validator_validate_file (AsValidator *validator, GFile *metadata_file)
{
	g_autoptr(GFileInfo) info = NULL;
	g_autoptr(GInputStream) file_stream = NULL;
	g_autoptr(GInputStream) stream_data = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GString) asxmldata = NULL;
	g_autoptr(GBytes) bytes = NULL;
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
					"file-read-failed",
					tmp_error->message);
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
					"file-read-failed",
					tmp_error->message);
		return FALSE;
	}
	/* check if there was an error */
	if (len < 0)
		return FALSE;

	bytes = g_bytes_new_static (asxmldata->str, asxmldata->len);
	ret = as_validator_validate_bytes (validator, bytes);
	as_validator_clear_current_fname (validator);

	return ret;
}

/**
 * as_validator_open_xml_document:
 */
static xmlDoc*
as_validator_open_xml_document (AsValidator *validator, const gchar *xmldata, gssize len)
{
	xmlDoc *doc;
	g_autoptr(GError) error = NULL;

	doc = as_xml_parse_document (xmldata, len, &error);
	if (doc == NULL) {
		if (error != NULL) {
			as_validator_add_issue (validator, NULL,
						"xml-markup-invalid",
						error->message);
		}

		return NULL;
	}

	return doc;
}

/**
 * as_validator_validate_bytes:
 * @validator: An instance of #AsValidator.
 * @metadata: XML metadata as #GBytes.
 *
 * Validate AppStream XML data from a byte array.
 * Remember to run %as_validator_clear_issues if you do not want previous
 * validation runs to affect the outcome of this validation.
 *
 * Returns: %TRUE if bytes validated successfully.
 *
 * Since: 0.14.0
 **/
gboolean
as_validator_validate_bytes (AsValidator *validator, GBytes *metadata)
{
	gboolean ret;
	xmlNode* root;
	xmlDoc *doc;
	g_autoptr(AsContext) ctx = NULL;
	const gchar *data;
	gsize data_len;
	AsComponent *cpt;

	/* setup networking, in case we want to check URLs */
	as_validator_setup_networking (validator);

	/* load the XML data */
	ctx = as_context_new ();
	as_context_set_locale (ctx, "C");

	data = g_bytes_get_data (metadata, &data_len);
	doc = as_validator_open_xml_document (validator, data, data_len);
	if (doc == NULL)
		return FALSE;
	root = xmlDocGetRootElement (doc);

	ret = TRUE;
	if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
		as_context_set_style (ctx, AS_FORMAT_STYLE_METAINFO);
		cpt = as_validator_validate_component_node (validator, ctx, root);
		if (cpt != NULL)
			g_object_unref (cpt);
	} else if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
		const gchar *node_name;

		as_context_set_style (ctx, AS_FORMAT_STYLE_COLLECTION);
		for (xmlNode *iter = root->children; iter != NULL; iter = iter->next) {
			/* discard spaces */
			if (iter->type != XML_ELEMENT_NODE)
				continue;
			node_name = (const gchar*) iter->name;
			if (g_strcmp0 (node_name, "component") == 0) {
				cpt = as_validator_validate_component_node (validator, ctx, iter);
				if (cpt != NULL)
					g_object_unref (cpt);
			} else {
				as_validator_add_issue (validator, iter,
							"component-collection-tag-invalid",
							node_name);
				ret = FALSE;
			}
		}
	} else if (g_str_has_prefix ((gchar*) root->name, "application")) {
		as_validator_add_issue (validator, root,
					"metainfo-ancient",
					NULL);
		ret = FALSE;
	} else {
		as_validator_add_issue (validator, root,
					"root-tag-unknown",
					(gchar*) root->name);
		ret = FALSE;
	}

	xmlFreeDoc (doc);
	return ret && as_validator_check_success (validator);
}

/**
 * as_validator_validate_data:
 * @validator: An instance of #AsValidator.
 * @metadata: XML metadata.
 *
 * Validate AppStream XML data.
 * Remember to run %as_validator_clear_issues if you do not want previous
 * validation runs to affect the outcome of this validation.
 *
 * Returns: %TRUE if data validated successfully.
 **/
gboolean
as_validator_validate_data (AsValidator *validator, const gchar *metadata)
{
	g_autoptr(GBytes) bytes = g_bytes_new_static (metadata, strlen (metadata));
	return as_validator_validate_bytes (validator, bytes);
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
	AsValidatorPrivate *priv = GET_PRIVATE (data->validator);
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
		/* the name-without-type didn't match - check for the full ID in the component name */
		if (!as_matches_metainfo (fname, as_component_get_id (cpt))) {
			as_validator_add_issue (data->validator, NULL,
						"metainfo-filename-cid-mismatch",
						NULL);
		}
	}

	/* check if the referenced .desktop file exists */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) {
		AsLaunchable *de_launchable = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
		if ((de_launchable != NULL) && (as_launchable_get_entries (de_launchable)->len > 0)) {
			const gchar *desktop_id = g_ptr_array_index (as_launchable_get_entries (de_launchable), 0);

			if (g_hash_table_contains (data->desktop_fnames, desktop_id)) {
				g_autofree gchar *desktop_fname_full = NULL;
				g_autoptr(GFile) dfile = NULL;
				g_autoptr(GPtrArray) de_issues = NULL;
				g_autoptr(GError) tmp_error = NULL;

				desktop_fname_full = g_build_filename (data->apps_dir, desktop_id, NULL);
				dfile = g_file_new_for_path (desktop_fname_full);
				de_issues = g_ptr_array_new_with_free_func (g_object_unref);
				as_desktop_entry_parse_file (cpt,
							     dfile,
							     AS_FORMAT_VERSION_CURRENT,
							     TRUE, /* ignore NoDisplay */
							     de_issues,
							     NULL, NULL,
							     &tmp_error);
				if (tmp_error != NULL) {
					as_validator_add_issue (data->validator, NULL,
								"desktop-file-load-failed",
								tmp_error->message);
				} else {
					/* we successfully loaded the desktop-entry data into the component, now perform some checks */

					/* pass on the found desktop-entry issues */
					for (guint i = 0; i < de_issues->len; i++) {
						AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (g_ptr_array_index (de_issues, i));
						g_autofree gchar *issue_id_str = NULL;

						as_validator_issue_set_filename (issue, desktop_id);
						issue_id_str = g_strdup_printf ("%s/%s/%s",
										desktop_id,
										as_validator_issue_get_tag (issue),
										as_validator_issue_get_hint (issue));

						/* str ownership is transferred to the hashtable */
						if (g_hash_table_insert (priv->issues, g_steal_pointer (&issue_id_str), issue)) {
							/* the issue is new, we can add it to our by-file listing */
							GPtrArray *ilist = g_hash_table_lookup (priv->issues_per_file, desktop_id);
							if (ilist == NULL) {
								ilist = g_ptr_array_new_with_free_func (g_object_unref);
								g_hash_table_insert (priv->issues_per_file, g_strdup (desktop_id), ilist);
							}
							g_ptr_array_add (ilist, g_object_ref (issue));
						}
					}

					/* check if we have any categories */
					if (as_component_get_categories (cpt)->len == 0) {
						as_validator_add_issue (data->validator, NULL,
									"app-categories-missing",
									NULL);
					}
				}
			} else {
				as_validator_add_issue (data->validator, NULL, "desktop-file-not-found", NULL);
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
 *
 * Returns: %TRUE if file validated successfully.
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
	g_autoptr(AsContext) ctx = NULL;
	struct MInfoCheckData ht_helper;

	/* cleanup */
	as_validator_clear_issues (validator);

	metainfo_dir = g_build_filename (root_dir, "usr", "share", "metainfo", NULL);
	legacy_metainfo_dir = g_build_filename (root_dir, "usr", "share", "appdata", NULL);
	apps_dir = g_build_filename (root_dir, "usr", "share", "applications", NULL);

	/* check if we actually have a directory which could hold metadata */
	if ((!g_file_test (metainfo_dir, G_FILE_TEST_IS_DIR)) &&
	    (!g_file_test (legacy_metainfo_dir, G_FILE_TEST_IS_DIR))) {
		g_clear_pointer (&metainfo_dir, g_free);
		metainfo_dir = g_build_filename (root_dir, "share", "metainfo", NULL);
		if (g_file_test (metainfo_dir, G_FILE_TEST_IS_DIR)) {
			/* success, we found some metadata without /usr prefix! */
			g_clear_pointer (&legacy_metainfo_dir, g_free);
			g_clear_pointer (&apps_dir, g_free);
			legacy_metainfo_dir = g_build_filename (root_dir, "share", "appdata", NULL);
			apps_dir = g_build_filename (root_dir, "share", "applications", NULL);
		} else {
			/* no metadata directory */
			as_validator_add_issue (validator, NULL,
						"dir-no-metadata-found",
						NULL);
			goto out;
		}
	}

	g_debug ("Looking for metadata in %s", metainfo_dir);

	/* check if we actually have a directory which could hold application information */
	if (!g_file_test (apps_dir, G_FILE_TEST_IS_DIR)) {
		as_validator_add_issue (validator, NULL,
					"dir-applications-not-found",
					NULL);
	}

	/* initialize networking (only actually happens if URLs should be checked) */
	as_validator_setup_networking (validator);

	/* holds a filename -> component mapping */
	validated_cpts = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						g_object_unref);

	/* set up XML parser */
	ctx = as_context_new ();
	as_context_set_locale (ctx, "C");
	as_context_set_style (ctx, AS_FORMAT_STYLE_METAINFO);

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

			as_validator_add_issue (validator, NULL, "metainfo-legacy-path", NULL);

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
						"file-read-failed",
						tmp_error->message);
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
						"file-read-failed",
						tmp_error->message);
			continue;
		}

		/* now read the XML */
		doc = as_validator_open_xml_document (validator, asdata->str, asdata->len);
		if (doc == NULL) {
			as_validator_clear_current_fname (validator);
			continue;
		}
		root = xmlDocGetRootElement (doc);

		if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
			AsComponent *cpt;
			cpt = as_validator_validate_component_node (validator,
								    ctx,
								    root);
			if (cpt != NULL)
				g_hash_table_insert (validated_cpts,
							g_strdup (fname_basename),
							cpt);
		} else if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
			as_validator_add_issue (validator, root,
						"metainfo-multiple-components",
						NULL);
			ret = FALSE;
		} else if (g_str_has_prefix ((gchar*) root->name, "application")) {
			as_validator_add_issue (validator, root, "metainfo-ancient", NULL);
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

	return ret && as_validator_check_success (validator);
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
 * as_validator_get_issues_per_file:
 * @validator: An instance of #AsValidator.
 *
 * Get a hash table of filenames mapped to lists of issues.
 * This is useful if validation was requested for multiple files and
 * a list of issues per-file is desired without prior explicit sorting.
 *
 * Returns: (element-type utf8 GPtrArray(AsValidatorIssue)) (transfer none): a file to issue list mapping
 *
 * Since: 0.12.8
 */
GHashTable*
as_validator_get_issues_per_file (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	return priv->issues_per_file;
}

/**
 * as_validator_yaml_write_handler_cb:
 *
 * Helper function to store the emitted YAML document.
 */
static int
as_validator_yaml_write_handler_cb (void *ptr, unsigned char *buffer, size_t size)
{
	GString *str;
	str = (GString*) ptr;
	g_string_append_len (str, (const gchar*) buffer, size);

	return 1;
}

/**
 * as_validator_get_issues_as_yaml:
 * @validator: An instance of #AsValidator.
 * @yaml_report: (out): The generated YAML report
 *
 * Get an issue report as a YAML document.
 *
 * Returns: %TRUE if validation was successful. A YAML report is generated in any case.
 *
 * Since: 0.12.8
 */
gboolean
as_validator_get_report_yaml (AsValidator *validator, gchar **yaml_report)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	GHashTableIter ht_iter;
	gpointer ht_key, ht_value;
	yaml_emitter_t emitter;
	yaml_event_t event;
	gboolean res = FALSE;
	gboolean report_validation_passed = TRUE;
	GString *yaml_result = g_string_new ("");

	if (yaml_report == NULL)
		return FALSE;
	*yaml_report = NULL;

	yaml_emitter_initialize (&emitter);
	yaml_emitter_set_indent (&emitter, 2);
	yaml_emitter_set_unicode (&emitter, TRUE);
	yaml_emitter_set_width (&emitter, 100);
	yaml_emitter_set_output (&emitter, as_validator_yaml_write_handler_cb, yaml_result);

	/* emit start event */
	yaml_stream_start_event_initialize (&event, YAML_UTF8_ENCODING);
	if (!yaml_emitter_emit (&emitter, &event)) {
		g_critical ("Failed to initialize YAML emitter.");
		g_string_free (yaml_result, TRUE);
		yaml_emitter_delete (&emitter);
		return FALSE;
	}

	g_hash_table_iter_init (&ht_iter, priv->issues_per_file);
	while (g_hash_table_iter_next (&ht_iter, &ht_key, &ht_value)) {
		const gchar *filename = (const gchar*) ht_key;
		GPtrArray *issues = (GPtrArray*) ht_value;
		gboolean validation_passed = TRUE;

		/* new document for this file */
		yaml_document_start_event_initialize (&event, NULL, NULL, NULL, FALSE);
		res = yaml_emitter_emit (&emitter, &event);
		g_assert (res);

		/* main dict start */
		as_yaml_mapping_start (&emitter);

		as_yaml_emit_entry (&emitter, "File", filename);
		as_yaml_emit_entry (&emitter, "Validator", PACKAGE_VERSION);

		as_yaml_emit_scalar (&emitter, "Issues");
		as_yaml_sequence_start (&emitter);

		for (guint i = 0; i < issues->len; i++) {
			AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (g_ptr_array_index (issues, i));
			gint line = as_validator_issue_get_line (issue);
			const gchar *hint = as_validator_issue_get_hint (issue);
			const gchar *cid = as_validator_issue_get_cid (issue);
			AsIssueSeverity severity = as_validator_issue_get_severity (issue);
			as_yaml_mapping_start (&emitter);

			as_yaml_emit_entry (&emitter,
					    "tag",
					    as_validator_issue_get_tag (issue));
			as_yaml_emit_entry (&emitter,
					    "severity",
					    as_issue_severity_to_string (severity));

			if (cid != NULL)
				as_yaml_emit_entry (&emitter, "component", cid);
			if (line > 0)
				as_yaml_emit_entry_uint64 (&emitter, "line", (guint) line);
			if (hint != NULL)
				as_yaml_emit_entry (&emitter, "hint", hint);
			as_yaml_emit_long_entry (&emitter,
						 "explanation",
						 as_validator_issue_get_explanation (issue));

			if ((severity == AS_ISSUE_SEVERITY_WARNING) || (severity == AS_ISSUE_SEVERITY_ERROR))
				validation_passed = FALSE;

			as_yaml_mapping_end (&emitter);
		}

		/* end "Issues" sequence */
		as_yaml_sequence_end (&emitter);

		as_yaml_emit_entry (&emitter, "Passed", validation_passed? "yes" : "no");
		if (!validation_passed)
			report_validation_passed = FALSE;

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
	*yaml_report = g_string_free (yaml_result, FALSE);
	return report_validation_passed;
}

/**
 * as_validator_get_tag_explanation:
 * @validator: An instance of #AsValidator.
 *
 * Get the explanatory text for a given issue tag.
 *
 * Returns: Explanation text.
 */
const gchar*
as_validator_get_tag_explanation (AsValidator *validator, const gchar *tag)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	const AsValidatorIssueTag *tag_data = g_hash_table_lookup (priv->issue_tags, tag);
	if (tag_data == NULL)
		return NULL;
	return tag_data->explanation;
}

/**
 * as_validator_get_tag_severity:
 * @validator: An instance of #AsValidator.
 *
 * Get the severity for a given issue tag.
 *
 * Returns: The #AsIssueSeverity
 */
AsIssueSeverity
as_validator_get_tag_severity (AsValidator *validator, const gchar *tag)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	const AsValidatorIssueTag *tag_data = g_hash_table_lookup (priv->issue_tags, tag);
	if (tag_data == NULL)
		return AS_ISSUE_SEVERITY_UNKNOWN;
	return tag_data->severity;
}

/**
 * as_validator_get_tags:
 * @validator: An instance of #AsValidator.
 *
 * Get an array of all tags known to the validator.
 *
 * Returns: (transfer full): A string array of tags
 */
gchar**
as_validator_get_tags (AsValidator *validator)
{
	AsValidatorPrivate *priv = GET_PRIVATE (validator);
	GHashTableIter iter;
	gpointer ht_key;
	guint i = 0;
	gchar **result;

	result = g_new0 (gchar*, g_hash_table_size (priv->issue_tags) + 1);
	g_hash_table_iter_init (&iter, priv->issue_tags);
	while (g_hash_table_iter_next (&iter, &ht_key, NULL)) {
		result[i++] = g_strdup ((const gchar*) ht_key);
	}

	return result;
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
