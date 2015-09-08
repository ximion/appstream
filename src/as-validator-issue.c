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
 * SECTION:as-validator-issue
 * @short_description: Object representing an issue found in AppStream metadata
 * @include: appstream.h
 *
 * See also: #AsValidator
 */

#include "config.h"

#include "as-validator-issue.h"

typedef struct _AsValidatorIssuePrivate	AsValidatorIssuePrivate;
struct _AsValidatorIssuePrivate
{
	AsIssueKind		kind;
	AsIssueImportance	importance;
	gchar			*message;
	gchar			*location;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsValidatorIssue, as_validator_issue, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_validator_issue_get_instance_private (o))

/**
 * as_validator_issue_finalize:
 **/
static void
as_validator_issue_finalize (GObject *object)
{
	AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (object);
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);

	g_free (priv->message);
	g_free (priv->location);

	G_OBJECT_CLASS (as_validator_issue_parent_class)->finalize (object);
}

/**
 * as_validator_issue_init:
 **/
static void
as_validator_issue_init (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	priv->kind = AS_ISSUE_KIND_UNKNOWN;
	priv->importance = AS_ISSUE_IMPORTANCE_UNKNOWN;
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
 * as_validator_issue_get_kind:
 * @issue: a #AsValidatorIssue instance.
 *
 * Gets the issue kind enum, if available.
 *
 * Returns: a #AsIssueKind
 **/
AsIssueKind
as_validator_issue_get_kind (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->kind;
}

/**
 * as_validator_issue_set_kind:
 * @issue: a #AsValidatorIssue instance.
 * @kind: the #AsIssueKind.
 *
 * Sets the kind enum for this issue, if known.
 **/
void
as_validator_issue_set_kind (AsValidatorIssue *issue, AsIssueKind kind)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	priv->kind = kind;
}

/**
 * as_validator_issue_get_importance:
 * @issue: a #AsValidatorIssue instance.
 *
 * Gets the importance of this issue.
 *
 * Returns: a #AsIssueImportance
 **/
AsIssueImportance
as_validator_issue_get_importance (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->importance;
}

/**
 * as_validator_issue_set_importance:
 * @issue: a #AsValidatorIssue instance.
 * @importance: the #AsIssueImportance.
 *
 * Sets the importance for this issue.
 **/
void
as_validator_issue_set_importance (AsValidatorIssue *issue, AsIssueImportance importance)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	priv->importance = importance;
}

/**
 * as_validator_issue_get_message:
 * @issue: a #AsValidatorIssue instance.
 *
 * Gets the message for the issue.
 *
 * Returns: the message
 **/
const gchar*
as_validator_issue_get_message (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->message;
}

/**
 * as_validator_issue_set_message:
 * @issue: a #AsValidatorIssue instance.
 * @message: the message text.
 *
 * Sets a message on the issue.
 **/
void
as_validator_issue_set_message (AsValidatorIssue *issue, const gchar *message)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->message);
	priv->message = g_strdup (message);
}

/**
 * as_validator_issue_get_location:
 * @issue: a #AsValidatorIssue instance.
 *
 * Gets a location hint for the issue.
 *
 * Returns: the location hint
 **/
const gchar*
as_validator_issue_get_location (AsValidatorIssue *issue)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->location;
}

/**
 * as_validator_issue_set_location:
 * @issue: a #AsValidatorIssue instance.
 * @location: a location hint.
 *
 * Sets a location hint for this issue.
 **/
void
as_validator_issue_set_location (AsValidatorIssue *issue, const gchar *location)
{
	AsValidatorIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->location);
	priv->location = g_strdup (location);
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
