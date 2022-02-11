/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-issue
 * @short_description: An issue resolved in a release.
 * @include: appstream.h
 *
 * Information about an issue that was resolved in a release.
 *
 * See also: #AsRelease
 */

#include "config.h"
#include "as-issue-private.h"

typedef struct
{
	AsIssueKind		 kind;
	gchar			*id;
	gchar			*url;
} AsIssuePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsIssue, as_issue, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_issue_get_instance_private (o))

/**
 * as_issue_kind_to_string:
 * @kind: the %AsIssueKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar*
as_issue_kind_to_string (AsIssueKind kind)
{
	if (kind == AS_ISSUE_KIND_GENERIC)
		return "generic";
	if (kind == AS_ISSUE_KIND_CVE)
		return "cve";
	return "unknown";
}

/**
 * as_issue_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsIssueKind or %AS_ISSUE_KIND_UNKNOWN for unknown
 **/
AsIssueKind
as_issue_kind_from_string (const gchar *kind_str)
{
	if (kind_str == NULL)
		return AS_ISSUE_KIND_GENERIC;
	if (g_strcmp0 (kind_str, "") == 0)
		return AS_ISSUE_KIND_GENERIC;
	if (g_strcmp0 (kind_str, "cve") == 0)
		return AS_ISSUE_KIND_CVE;
	return AS_ISSUE_KIND_UNKNOWN;
}

static void
as_issue_finalize (GObject *object)
{
	AsIssue *issue = AS_ISSUE (object);
	AsIssuePrivate *priv = GET_PRIVATE (issue);

	g_free (priv->id);
	g_free (priv->url);

	G_OBJECT_CLASS (as_issue_parent_class)->finalize (object);
}

static void
as_issue_init (AsIssue *issue)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);
	priv->kind = AS_ISSUE_KIND_GENERIC;
}

static void
as_issue_class_init (AsIssueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_issue_finalize;
}

/**
 * as_issue_get_kind:
 * @issue: a #AsIssue instance.
 *
 * Gets the issue type.
 *
 * Returns: the #AsIssueKind
 **/
AsIssueKind
as_issue_get_kind (AsIssue *issue)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->kind;
}

/**
 * as_issue_set_kind:
 * @issue: a #AsIssue instance.
 * @kind: the #AsIssueKind, e.g. %AS_ISSUE_KIND_SHA256.
 *
 * Sets the issue type.
 **/
void
as_issue_set_kind (AsIssue *issue, AsIssueKind kind)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);
	priv->kind = kind;
}

/**
 * as_issue_get_id:
 * @issue: a #AsIssue instance.
 *
 * Gets the issue ID (usually a bug number or CVE ID)
 *
 * Returns: the ID.
 **/
const gchar*
as_issue_get_id (AsIssue *issue)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);
	return priv->id;
}

/**
 * as_issue_set_id:
 * @issue: a #AsIssue instance.
 * @id: the new ID.
 *
 * Sets the issue ID.
 **/
void
as_issue_set_id (AsIssue *issue, const gchar *id)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->id);
	priv->id = g_strdup (id);
}

/**
 * as_issue_get_url:
 * @issue: a #AsIssue instance.
 *
 * Gets the URL associacted with this issue, usually
 * referencing a bug report or issue description.
 *
 * Returns: the URL.
 **/
const gchar*
as_issue_get_url (AsIssue *issue)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);

	/* we can synthesize an URL if the issue type is a CVE entry */
	if ((priv->url == NULL) && (priv->kind == AS_ISSUE_KIND_CVE) && (priv->id != NULL))
		priv->url = g_strdup_printf ("https://cve.mitre.org/cgi-bin/cvename.cgi?name=%s", priv->id);

	return priv->url;
}

/**
 * as_issue_set_url:
 * @issue: a #AsIssue instance.
 * @url: the new URL.
 *
 * Sets an URL describing this issue.
 **/
void
as_issue_set_url (AsIssue *issue, const gchar *url)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);
	g_free (priv->url);
	priv->url = g_strdup (url);
}

/**
 * as_issue_load_from_xml:
 * @issue: a #AsIssue instance.
 * @ctx: the AppStream document context.
 * @node: the XML node.
 * @error: a #GError.
 *
 * Loads data from an XML node.
 **/
gboolean
as_issue_load_from_xml (AsIssue *issue, AsContext *ctx, xmlNode *node, GError **error)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);
	g_autofree gchar *prop = NULL;

	prop = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	priv->kind = as_issue_kind_from_string (prop);
	if (priv->kind == AS_ISSUE_KIND_UNKNOWN)
		return FALSE;

	g_free (priv->id);
	priv->id = as_xml_get_node_value (node);

	g_free (priv->url);
	priv->url = (gchar*) xmlGetProp (node, (xmlChar*) "url");

	return TRUE;
}

/**
 * as_issue_to_xml_node:
 * @issue: a #AsIssue instance.
 * @ctx: the AppStream document context.
 * @root: XML node to attach the new nodes to.
 *
 * Serializes the data to an XML node.
 **/
void
as_issue_to_xml_node (AsIssue *issue, AsContext *ctx, xmlNode *root)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);
	xmlNode *n;

	if (priv->kind == AS_ISSUE_KIND_UNKNOWN)
		return;
	if (priv->id == NULL)
		return;

	n = xmlNewTextChild (root, NULL,
			     (xmlChar*) "issue",
			     (xmlChar*) priv->id);

	if (priv->kind != AS_ISSUE_KIND_GENERIC)
		xmlNewProp (n,
			    (xmlChar*) "type",
			    (xmlChar*) as_issue_kind_to_string (priv->kind));

	if (priv->url != NULL) {
		g_strstrip (priv->url);
		xmlNewProp (n,
			    (xmlChar*) "url",
			    (xmlChar*) priv->url);
	}
}

/**
 * as_issue_load_from_yaml:
 * @issue: a #AsIssue instance.
 * @ctx: the AppStream document context.
 * @node: the YAML node.
 * @error: a #GError.
 *
 * Loads data from a YAML field.
 **/
gboolean
as_issue_load_from_yaml (AsIssue *issue, AsContext *ctx, GNode *node, GError **error)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);

	for (GNode *n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);
		const gchar *value = as_yaml_node_get_value (n);

		if (G_UNLIKELY (value == NULL))
			continue; /* there should be no key without value */

		if (g_strcmp0 (key, "type") == 0) {
			priv->kind = as_issue_kind_from_string (value);

		} else if (g_strcmp0 (key, "id") == 0) {
			g_free (priv->id);
			priv->id = g_strdup (value);

		} else if (g_strcmp0 (key, "url") == 0) {
			g_free (priv->url);
			priv->url = g_strdup (value);

		} else {
			as_yaml_print_unknown ("issue", key);
		}
	}

	return TRUE;
}

/**
 * as_issue_emit_yaml:
 * @issue: a #AsIssue instance.
 * @ctx: the AppStream document context.
 * @emitter: The YAML emitter to emit data on.
 *
 * Emit YAML data for this object.
 **/
void
as_issue_emit_yaml (AsIssue *issue, AsContext *ctx, yaml_emitter_t *emitter)
{
	AsIssuePrivate *priv = GET_PRIVATE (issue);

	if (priv->kind == AS_ISSUE_KIND_UNKNOWN)
		return;
	if (priv->id == NULL)
		return;

	as_yaml_mapping_start (emitter);

	if (priv->kind != AS_ISSUE_KIND_GENERIC)
		as_yaml_emit_entry (emitter, "type", as_issue_kind_to_string (priv->kind));

	if (priv->url != NULL)
		g_strstrip (priv->url);

	as_yaml_emit_entry (emitter, "id", priv->id);
	as_yaml_emit_entry (emitter, "url", priv->url);

	as_yaml_mapping_end (emitter);
}

/**
 * as_issue_new:
 *
 * Creates a new #AsIssue.
 *
 * Returns: (transfer full): an #AsIssue
 **/
AsIssue*
as_issue_new (void)
{
	AsIssue *issue;
	issue = g_object_new (AS_TYPE_ISSUE, NULL);
	return AS_ISSUE (issue);
}
