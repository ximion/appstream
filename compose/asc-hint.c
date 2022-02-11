/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-hint
 * @short_description: A data processing hint.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-hint.h"

#include "as-utils-private.h"
#include "asc-globals-private.h"

typedef struct
{
	GPtrArray		*vars;
	gchar			*tag;
	AsIssueSeverity		severity;
	GRefString		*explanation_tmpl;
} AscHintPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscHint, asc_hint, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_hint_get_instance_private (o))

static void
asc_hint_init (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);

	priv->vars = g_ptr_array_new_with_free_func (g_free);
}

static void
asc_hint_finalize (GObject *object)
{
	AscHint *hint = ASC_HINT (object);
	AscHintPrivate *priv = GET_PRIVATE (hint);

	g_free (priv->tag);
	as_ref_string_release (priv->explanation_tmpl);
	priv->severity = AS_ISSUE_SEVERITY_UNKNOWN;
	if (priv->vars != NULL)
		g_ptr_array_unref (priv->vars);

	G_OBJECT_CLASS (asc_hint_parent_class)->finalize (object);
}

static void
asc_hint_class_init (AscHintClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_hint_finalize;
}

/**
 * asc_hint_get_tag:
 * @hint: an #AscHint instance.
 *
 * Gets the unique tag for the type of this hint.
 **/
const gchar*
asc_hint_get_tag (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	return priv->tag;
}

/**
 * asc_hint_set_tag:
 * @hint: an #AscHint instance.
 *
 * Sets the unique tag for the type of this hint.
 **/
void
asc_hint_set_tag (AscHint *hint, const gchar *tag)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_free (priv->tag);
	priv->tag = g_strdup (tag);
}

/**
 * asc_hint_get_severity:
 * @hint: an #AscHint instance.
 *
 * Gets the issue severity of this hint.
 **/
AsIssueSeverity
asc_hint_get_severity (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	return priv->severity;
}

/**
 * asc_hint_set_severity:
 * @hint: an #AscHint instance.
 *
 * Sets the issue severity of this hint.
 **/
void
asc_hint_set_severity (AscHint *hint, AsIssueSeverity severity)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	priv->severity = severity;
}

/**
 * asc_hint_get_explanation_template:
 * @hint: an #AscHint instance.
 *
 * Gets the explanation template for this hint.
 **/
const gchar*
asc_hint_get_explanation_template (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	return priv->explanation_tmpl;
}

/**
 * asc_hint_set_explanation_template:
 * @hint: an #AscHint instance.
 *
 * Sets the explanation template for this hint.
 **/
void
asc_hint_set_explanation_template (AscHint *hint, const gchar *explanation_tmpl)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	as_ref_string_assign_safe (&priv->explanation_tmpl, explanation_tmpl);
}

/**
 * asc_hint_is_error:
 * @hint: an #AscHint instance.
 *
 * Returns: %TRUE if this hint describes an error.
 **/
gboolean
asc_hint_is_error (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	return priv->severity == AS_ISSUE_SEVERITY_ERROR;
}

/**
 * asc_hint_is_valid:
 * @hint: an #AscHint instance.
 *
 * Check if this hint is valid (it requires at least a tag and a severity
 * in order to be considered valid).
 *
 * Returns: %TRUE if this hint is valid.
 **/
gboolean
asc_hint_is_valid (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	return (priv->severity != AS_ISSUE_SEVERITY_UNKNOWN) && !as_is_empty (priv->tag);
}

/**
 * asc_hint_add_explanation_var:
 * @hint: an #AscHint instance.
 * @var_name: Name of the variable to be replaced.
 * @text: Replacement for the variable name.
 *
 * Add a replacement variable for the explanation text.
 **/
void
asc_hint_add_explanation_var (AscHint *hint, const gchar *var_name, const gchar *text)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_assert_cmpint (priv->vars->len % 2, ==, 0);

	/* check if we can replace an existing value */
	for (guint i = 0; i < priv->vars->len; i += 2) {
		if (g_strcmp0 (g_ptr_array_index (priv->vars, i), var_name) == 0) {
			g_free (g_ptr_array_index (priv->vars, i + 1));
			g_ptr_array_index (priv->vars, i + 1) = g_strdup (text);
			return;
		}
	}

	/* add new key-value pair */
	g_ptr_array_add (priv->vars, g_strdup (var_name));
	g_ptr_array_add (priv->vars, g_strdup (text));
}

/**
 * asc_hint_get_explanation_vars_list:
 * @hint: an #AscHint instance.
 *
 * Returns a list with the flattened key/value pairs for this hint.
 * Values are located in uneven list entries, following their keys in even list entries.
 *
 * Returns: (transfer none) (element-type utf8): A flattened #GPtrArray with the key/value pairs.
 **/
GPtrArray*
asc_hint_get_explanation_vars_list (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_assert_cmpint (priv->vars->len % 2, ==, 0);
	return priv->vars;
}

/**
 * asc_hint_format_explanation:
 * @hint: an #AscHint instance.
 *
 * Formats the explanation template to return a human-redable issue hint
 * explanation, with all placeholder variables replaced.
 *
 * Returns: (transfer full): Explanation text for this hint, with variables replaced.
 **/
gchar*
asc_hint_format_explanation (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_auto(GStrv) parts = NULL;

	g_assert_cmpint (priv->vars->len % 2, ==, 0);
	if (priv->explanation_tmpl == NULL)
		return NULL;

	parts = g_strsplit (priv->explanation_tmpl, "{{", -1);
	for (guint i = 0; parts[i] != NULL; i++) {
		gboolean replaced = FALSE;

		for (guint j = 0; j < priv->vars->len; j += 2) {
			g_autofree gchar *tmp = g_strconcat (g_ptr_array_index (priv->vars, j), "}}", NULL);
			g_autofree gchar *tmp2 = NULL;
			if (!g_str_has_prefix (parts[i], tmp))
				continue;

			/* replace string */
			tmp2 = parts[i];
			parts[i] = parts[i] + strlen (tmp);
			parts[i] = g_strconcat (g_ptr_array_index (priv->vars, j + 1), parts[i], NULL);
			replaced = TRUE;
			break;
		}

		if (!replaced && (i != 0)) {
			g_autofree gchar *tmp = NULL;

			/* keep the placeholder in place */
			tmp = parts[i];
			parts[i] = g_strconcat ("{{", parts[i], NULL);
		}
	}

	return g_strjoinv ("", parts);
}

/**
 * asc_hint_new:
 *
 * Creates a new #AscHint.
 **/
AscHint*
asc_hint_new (void)
{
	AscHint *hint;
	hint = g_object_new (ASC_TYPE_HINT, NULL);
	return ASC_HINT (hint);
}

/**
 * asc_hint_new_for_tag:
 * @tag: The tag ID to construct this hint for.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscHint with the given tag. If the selected tag was not registered+
 * with the global tag registry, %NULL is returned and an error is set.
 **/
AscHint*
asc_hint_new_for_tag (const gchar *tag, GError **error)
{
	AscHintTag *htag;
	g_autoptr(AscHint) hint = asc_hint_new ();

	htag = asc_globals_get_hint_tag_details (tag);
	if (htag == NULL || htag->severity == AS_ISSUE_SEVERITY_UNKNOWN) {
		g_set_error (error,
			     ASC_COMPOSE_ERROR,
			     ASC_COMPOSE_ERROR_FAILED,
			     "The selected hint tag '%s' could not be found. Unable to create hint object.", tag);
		return NULL;
	}

	asc_hint_set_tag (hint, htag->tag);
	asc_hint_set_severity (hint, htag->severity);
	asc_hint_set_explanation_template (hint, htag->explanation);
	return g_steal_pointer (&hint);
}
