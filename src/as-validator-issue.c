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
 * SECTION:as-validator-issue
 * @short_description: Object representing an issue found in AppStream metadata
 * @include: appstream.h
 *
 * See also: #AsValidator
 */

#include "config.h"

#include "as-validator-issue.h"

typedef struct
{
	gchar		*tag;
	AsIssueSeverity	severity;

	gchar		*hint;
	gchar		*explanation;

	gchar		*fname;
	gchar		*cid;
	glong		line;
} AsValidatorIssuePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsValidatorIssue, as_validator_issue, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_validator_issue_get_instance_private (o))

/**
 * as_issue_severity_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsIssueSeverity, or %AS_ISSUE_SEVERITY_UNKNOWN for unknown.
 *
 **/
AsIssueSeverity
as_issue_severity_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "error") == 0)
		return AS_ISSUE_SEVERITY_ERROR;
	if (g_strcmp0 (str, "warning") == 0)
		return AS_ISSUE_SEVERITY_WARNING;
	if (g_strcmp0 (str, "info") == 0)
		return AS_ISSUE_SEVERITY_INFO;
	if (g_strcmp0 (str, "pedantic") == 0)
		return AS_ISSUE_SEVERITY_PEDANTIC;
	return AS_ISSUE_SEVERITY_UNKNOWN;
}

/**
 * as_issue_severity_to_string:
 * @severity: the #AsIssueSeverity.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @severity
 *
 **/
const gchar*
as_issue_severity_to_string (AsIssueSeverity severity)
{
	if (severity == AS_ISSUE_SEVERITY_ERROR)
		return "error";
	if (severity == AS_ISSUE_SEVERITY_WARNING)
		return "warning";
	if (severity == AS_ISSUE_SEVERITY_INFO)
		return "info";
	if (severity == AS_ISSUE_SEVERITY_PEDANTIC)
		return "pedantic";
	return NULL;
}

/**
 * as_validator_issue_finalize:
 **/
static void
as_validator_issue_finalize (GObject *object)
{
	AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (object);
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);

	g_free (priv->tag);
	g_free (priv->hint);
	g_free (priv->explanation);

	g_free (priv->fname);
	g_free (priv->cid);

	G_OBJECT_CLASS (as_validator_issue_parent_class)->finalize (object);
}

/**
 * as_validator_issue_init:
 **/
static void
as_validator_issue_init (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	priv->severity = AS_ISSUE_SEVERITY_UNKNOWN;
	priv->line = -1;
}

/**
 * as_validator_issue_class_init:
 **/
static void
as_validator_issue_class_init (AsValidatorIssueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_validator_issue_finalize;
}

/**
 * as_validator_issue_get_tag:
 * @issue: a #AsValidatorIssue instance.
 *
 * Gets the issue tag string for this issue.
 *
 * Returns: the tag
 *
 * Since: 0.12.8
 **/
const gchar*
as_validator_issue_get_tag (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->tag;
}

/**
 * as_validator_issue_set_tag:
 * @issue: a #AsValidatorIssue instance.
 * @tag: the tag.
 *
 * Sets the issue tag.
 *
 * Since: 0.12.8
 **/
void
as_validator_issue_set_tag (AsValidatorIssue *issue, const gchar *tag)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->tag);
	priv->tag = g_strdup (tag);
}

/**
 * as_validator_issue_get_severity:
 * @issue: a #AsValidatorIssue instance.
 *
 * Gets the severity of this issue.
 *
 * Returns: a #AsIssueSeverity
 **/
AsIssueSeverity
as_validator_issue_get_severity (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->severity;
}

/**
 * as_validator_issue_set_severity:
 * @issue: a #AsValidatorIssue instance.
 * @severity: the #AsIssueSeverity.
 *
 * Sets the severity for this issue.
 **/
void
as_validator_issue_set_severity (AsValidatorIssue *issue, AsIssueSeverity severity)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	priv->severity = severity;
}

/**
 * as_validator_issue_get_hint:
 * @issue: a #AsValidatorIssue instance.
 *
 * Get a short context hint for this issue.
 *
 * Returns: the hint
 *
 * Since: 0.12.8
 **/
const gchar*
as_validator_issue_get_hint (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->hint;
}

/**
 * as_validator_issue_set_hint:
 * @issue: a #AsValidatorIssue instance.
 * @hint: the hint.
 *
 * Sets short issue hint.
 *
 * Since: 0.12.8
 **/
void
as_validator_issue_set_hint (AsValidatorIssue *issue, const gchar *hint)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->hint);
	priv->hint = g_strdup (hint);
}

/**
 * as_validator_issue_get_explanation:
 * @issue: a #AsValidatorIssue instance.
 *
 * Get an extended explanation on this issue, or return %NULL
 * if none is available.
 *
 * Returns: the explanation
 *
 * Since: 0.12.8
 **/
const gchar*
as_validator_issue_get_explanation (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->explanation;
}

/**
 * as_validator_issue_set_explanation:
 * @issue: a #AsValidatorIssue instance.
 * @explanation: the explanation.
 *
 * Set explanatory text for this issue.
 *
 * Since: 0.12.8
 **/
void
as_validator_issue_set_explanation (AsValidatorIssue *issue, const gchar *explanation)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->explanation);
	priv->explanation = g_strdup (explanation);
}

/**
 * as_validator_issue_get_cid:
 * @issue: a #AsValidatorIssue instance.
 *
 * The component-id this issue is about.
 *
 * Returns: a component-id.
 **/
const gchar*
as_validator_issue_get_cid (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->cid;
}

/**
 * as_validator_issue_set_cid:
 * @issue: a #AsValidatorIssue instance.
 * @cid: a component-id.
 *
 * Sets the component-id this issue is about.
 **/
void
as_validator_issue_set_cid (AsValidatorIssue *issue, const gchar *cid)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->cid);
	priv->cid = g_strdup (cid);
}

/**
 * as_validator_issue_get_line:
 * @issue: a #AsValidatorIssue instance.
 *
 * Gets the line number where this issue was found.
 *
 * Returns: the line number where this issue occured, or -1 if unknown.
 **/
glong
as_validator_issue_get_line (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->line;
}

/**
 * as_validator_issue_set_line:
 * @issue: a #AsValidatorIssue instance.
 * @line: the line number.
 *
 * Sets the line number where this issue was found.
 **/
void
as_validator_issue_set_line (AsValidatorIssue *issue, glong line)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	priv->line = line;
}

/**
 * as_validator_issue_get_filename:
 * @issue: a #AsValidatorIssue instance.
 *
 * The name of the file this issue was found in.
 *
 * Returns: the filename
 **/
const gchar*
as_validator_issue_get_filename (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->fname;
}

/**
 * as_validator_issue_set_filename:
 * @issue: a #AsValidatorIssue instance.
 * @fname: the filename.
 *
 * Sets the name of the file the issue was found in.
 **/
void
as_validator_issue_set_filename (AsValidatorIssue *issue, const gchar *fname)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->fname);
	priv->fname = g_strdup (fname);
}

/**
 * as_validator_issue_get_location:
 * @issue: a #AsValidatorIssue instance.
 *
 * Builds a string containing all information about the location
 * where this issue occured that we know about.
 *
 * Returns: (transfer full): the location hint as string.
 **/
gchar*
as_validator_issue_get_location (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	GString *location;

	location = g_string_new ("");

	if (priv->fname == NULL)
		g_string_append (location, "~");
	else
		g_string_append (location, priv->fname);

	if (priv->cid == NULL)
		g_string_append (location, ":~");
	else
		g_string_append_printf (location, ":%s", priv->cid);

	if (priv->line >= 0) {
		g_string_append_printf (location, ":%li", priv->line);
	}

	return g_string_free (location, FALSE);
}

/**
 * as_validator_issue_get_importance:
 * @issue: a #AsValidatorIssue instance.
 *
 * This function is deprecated and should not be used in new code.
 *
 * Returns: a #AsIssueSeverity
 **/
AsIssueSeverity
as_validator_issue_get_importance (AsValidatorIssue *issue)
{
	return as_validator_issue_get_severity (issue);
}

/**
 * as_validator_issue_set_importance:
 * @issue: a #AsValidatorIssue instance.
 * @importance: the #AsIssueSeverity.
 *
 * This function is deprecated and should not be used in new code.
 **/
void
as_validator_issue_set_importance (AsValidatorIssue *issue, AsIssueSeverity importance)
{
	as_validator_issue_set_severity (issue, importance);
}

/**
 * as_validator_issue_get_message:
 * @issue: a #AsValidatorIssue instance.
 *
 * This function is deprecated.
 *
 * Returns: the message
 **/
const gchar*
as_validator_issue_get_message (AsValidatorIssue *issue)
{
	return as_validator_issue_get_hint (issue);
}

/**
 * as_validator_issue_set_message:
 * @issue: a #AsValidatorIssue instance.
 * @message: the message text.
 *
 * This function is deprecated.
 **/
void
as_validator_issue_set_message (AsValidatorIssue *issue, const gchar *message)
{
	as_validator_issue_set_hint (issue, message);
}

/**
 * as_validator_issue_new:
 *
 * Creates a new #AsValidatorIssue.
 *
 * Returns: (transfer full): a #AsValidatorIssue
 **/
AsValidatorIssue *
as_validator_issue_new (void)
{
	AsValidatorIssue *issue;
	issue = g_object_new (AS_TYPE_VALIDATOR_ISSUE, NULL);
	return AS_VALIDATOR_ISSUE (issue);
}
