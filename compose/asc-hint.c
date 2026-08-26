/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

typedef struct {
	GPtrArray *vars;
	gchar *tag;
	AsIssueSeverity severity;
	GRefString *explanation_tmpl;
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
 *
 * Since: 0.13.0
 **/
const gchar *
asc_hint_get_tag (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_val_if_fail (ASC_IS_HINT (hint), NULL);

	return priv->tag;
}

/**
 * asc_hint_set_tag:
 * @hint: an #AscHint instance.
 *
 * Sets the unique tag for the type of this hint.
 *
 * Since: 0.13.0
 **/
void
asc_hint_set_tag (AscHint *hint, const gchar *tag)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_if_fail (ASC_IS_HINT (hint));

	g_free (priv->tag);
	priv->tag = g_strdup (tag);
}

/**
 * asc_hint_get_severity:
 * @hint: an #AscHint instance.
 *
 * Gets the issue severity of this hint.
 *
 * Since: 0.13.0
 **/
AsIssueSeverity
asc_hint_get_severity (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_val_if_fail (ASC_IS_HINT (hint), AS_ISSUE_SEVERITY_UNKNOWN);

	return priv->severity;
}

/**
 * asc_hint_set_severity:
 * @hint: an #AscHint instance.
 *
 * Sets the issue severity of this hint.
 *
 * Since: 0.13.0
 **/
void
asc_hint_set_severity (AscHint *hint, AsIssueSeverity severity)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_if_fail (ASC_IS_HINT (hint));

	priv->severity = severity;
}

/**
 * asc_hint_get_explanation_template:
 * @hint: an #AscHint instance.
 *
 * Gets the explanation template for this hint.
 *
 * Since: 0.13.0
 **/
const gchar *
asc_hint_get_explanation_template (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_val_if_fail (ASC_IS_HINT (hint), NULL);

	return priv->explanation_tmpl;
}

/**
 * asc_hint_set_explanation_template:
 * @hint: an #AscHint instance.
 *
 * Sets the explanation template for this hint.
 *
 * Since: 0.13.0
 **/
void
asc_hint_set_explanation_template (AscHint *hint, const gchar *explanation_tmpl)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_if_fail (ASC_IS_HINT (hint));

	as_ref_string_assign_safe (&priv->explanation_tmpl, explanation_tmpl);
}

/**
 * asc_hint_is_error:
 * @hint: an #AscHint instance.
 *
 * Returns: %TRUE if this hint describes an error.
 *
 * Since: 0.13.0
 **/
gboolean
asc_hint_is_error (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_val_if_fail (ASC_IS_HINT (hint), FALSE);

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
 *
 * Since: 0.13.0
 **/
gboolean
asc_hint_is_valid (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_val_if_fail (ASC_IS_HINT (hint), FALSE);

	return (priv->severity != AS_ISSUE_SEVERITY_UNKNOWN) && !as_is_empty (priv->tag);
}

/**
 * asc_hint_add_explanation_var:
 * @hint: an #AscHint instance.
 * @var_name: Name of the variable to be replaced.
 * @text: Replacement for the variable name.
 *
 * Add a replacement variable for the explanation text.
 *
 * Since: 0.13.0
 **/
void
asc_hint_add_explanation_var (AscHint *hint, const gchar *var_name, const gchar *text)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_if_fail (ASC_IS_HINT (hint));

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
 *
 * Since: 0.14.0
 **/
GPtrArray *
asc_hint_get_explanation_vars_list (AscHint *hint)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_return_val_if_fail (ASC_IS_HINT (hint), NULL);

	g_assert_cmpint (priv->vars->len % 2, ==, 0);
	return priv->vars;
}

/**
 * asc_hint_plain_state:
 *
 * State that the plain-text conversion has to carry across template chunks,
 * as a tag may well be opened before a placeholder and closed after it.
 */
typedef struct {
	gchar *link_url;       /* URL of the link we are currently inside of, or %NULL */
	gsize link_text_start; /* offset into the output where that link's text begins */
} AscHintPlainState;

/**
 * asc_hint_append_entity_plain:
 *
 * Append the entity starting at @p in its resolved form, and return a pointer to the
 * first character after it.
 */
static const gchar *
asc_hint_append_entity_plain (GString *out, const gchar *p)
{
	/* clang-format off */
	const struct {
		const gchar *entity;
		const gchar *replacement;
	} entities[] = {
		{ "&lt;",   "<"  },
		{ "&gt;",   ">"  },
		{ "&amp;",  "&"  },
		{ "&quot;", "\"" },
		{ "&apos;", "'"  },
		{ "&#39;",  "'"  },
		{ "&#34;",  "\"" },
		{ "&nbsp;", " "  },
	};
	/* clang-format on */

	for (guint i = 0; i < G_N_ELEMENTS (entities); i++) {
		if (g_str_has_prefix (p, entities[i].entity)) {
			g_string_append (out, entities[i].replacement);
			return p + strlen (entities[i].entity);
		}
	}

	g_string_append_c (out, '&');
	return p + 1;
}

/**
 * asc_hint_append_template_plain:
 *
 * Append a chunk of an explanation template to @out with all markup resolved into
 * plain text.
 */
static void
asc_hint_append_template_plain (GString *out, const gchar *text, AscHintPlainState *state)
{
	const gchar *p = text;

	while (*p != '\0') {
		const gchar *tag_end;

		if (*p == '&') {
			p = asc_hint_append_entity_plain (out, p);
			continue;
		}
		if (*p != '<') {
			g_string_append_c (out, *p);
			p++;
			continue;
		}

		/* we are at the start of a tag - if it never ends, treat it as plain text */
		tag_end = strchr (p, '>');
		if (tag_end == NULL) {
			g_string_append (out, p);
			return;
		}

		if (g_str_has_prefix (p, "<code>") || g_str_has_prefix (p, "</code>")) {
			g_string_append_c (out, '`');
		} else if (g_str_has_prefix (p, "<br/>") || g_str_has_prefix (p, "<br>")) {
			g_string_append_c (out, '\n');
		} else if (g_str_has_prefix (p, "<a ")) {
			const gchar *href = strstr (p, "href=\"");
			g_clear_pointer (&state->link_url, g_free);
			if (href != NULL && href < tag_end) {
				const gchar *url_start = href + strlen ("href=\"");
				const gchar *url_end = strchr (url_start, '"');
				if (url_end != NULL && url_end < tag_end)
					state->link_url = g_strndup (url_start,
								     url_end - url_start);
			}
			state->link_text_start = out->len;
		} else if (g_str_has_prefix (p, "</a>")) {
			/* only mention the URL if the link text is not the URL already */
			if (state->link_url != NULL &&
			    g_strcmp0 (out->str + state->link_text_start, state->link_url) != 0)
				g_string_append_printf (out, " (%s)", state->link_url);
			g_clear_pointer (&state->link_url, g_free);
		}
		/* any other tag (<em/> & co.) is simply dropped */

		p = tag_end + 1;
	}
}

/**
 * asc_hint_format_explanation_internal:
 *
 * Replace all placeholder variables in the explanation template.
 *
 * The template is ours - it is compiled into AppStream or was registered by whoever
 * added the hint tag - and legitimately contains markup. The values we fill it with are
 * read from metadata that we do not control and never may, so in @markup mode they are
 * escaped, and in plain-text mode they are inserted verbatim while the markup of the
 * template around them is resolved into text.
 */
static gchar *
asc_hint_format_explanation_internal (AscHint *hint, gboolean markup)
{
	AscHintPrivate *priv = GET_PRIVATE (hint);
	g_auto(GStrv) parts = NULL;
	g_autoptr(GString) result = NULL;
	AscHintPlainState state = { NULL, 0 };

	g_return_val_if_fail (ASC_IS_HINT (hint), NULL);

	g_assert_cmpint (priv->vars->len % 2, ==, 0);
	if (priv->explanation_tmpl == NULL)
		return NULL;

	result = g_string_sized_new (strlen (priv->explanation_tmpl));
	parts = g_strsplit (priv->explanation_tmpl, "{{", -1);
	for (guint i = 0; parts[i] != NULL; i++) {
		const gchar *tmpl_text = parts[i];

		/* every chunk but the first one starts with a variable to substitute */
		if (i != 0) {
			gboolean replaced = FALSE;

			for (guint j = 0; j < priv->vars->len; j += 2) {
				const gchar *var_name = g_ptr_array_index (priv->vars, j);
				const gchar *value = g_ptr_array_index (priv->vars, j + 1);
				gsize var_name_len = strlen (var_name);

				if (strncmp (parts[i], var_name, var_name_len) != 0)
					continue;
				if (strncmp (parts[i] + var_name_len, "}}", 2) != 0)
					continue;

				if (markup) {
					/* the value may be arbitrary data read from a metadata
					 * file, so it may not even be valid UTF-8 */
					g_autofree gchar *value_valid = g_utf8_make_valid (value,
											   -1);
					g_autofree gchar *value_esc = g_markup_escape_text (
					    value_valid,
					    -1);
					g_string_append (result, value_esc);
				} else {
					g_string_append (result, value);
				}

				tmpl_text = parts[i] + var_name_len + 2;
				replaced = TRUE;
				break;
			}

			/* keep the placeholder in place if we have no value for it */
			if (!replaced)
				g_string_append (result, "{{");
		}

		if (markup)
			g_string_append (result, tmpl_text);
		else
			asc_hint_append_template_plain (result, tmpl_text, &state);
	}

	g_free (state.link_url);
	return g_string_free (g_steal_pointer (&result), FALSE);
}

/**
 * asc_hint_format_explanation:
 * @hint: an #AscHint instance.
 *
 * Formats the explanation template to return a human-readable issue hint explanation,
 * with all placeholder variables replaced.
 *
 * Explanation templates contain markup, so the result of this function is a snippet of
 * HTML, ready to be embedded into a generated report.
 *
 * Use %asc_hint_format_explanation_plain if you need text to display without HTML
 * markup.
 *
 * Returns: (transfer full): Explanation text for this hint as HTML.
 *
 * Since: 0.13.0
 **/
gchar *
asc_hint_format_explanation (AscHint *hint)
{
	return asc_hint_format_explanation_internal (hint, TRUE);
}

/**
 * asc_hint_format_explanation_plain:
 * @hint: an #AscHint instance.
 *
 * Formats the explanation template like %asc_hint_format_explanation does, but resolves
 * all markup of the template into plain text: emphasis is dropped, code spans are marked
 * the way Markdown does, line breaks become actual line breaks, links are reduced to
 * their text followed by the URL they point to, and escaped characters are unescaped.
 *
 * Returns: (transfer full): Explanation text for this hint as plain text.
 *
 * Since: 1.2.0
 **/
gchar *
asc_hint_format_explanation_plain (AscHint *hint)
{
	return asc_hint_format_explanation_internal (hint, FALSE);
}

/**
 * asc_hint_new:
 *
 * Creates a new, empty #AscHint.
 *
 * Returns: (transfer full): The new #AscHint.
 **/
static AscHint *
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
 *
 * Since: 0.14.0
 **/
AscHint *
asc_hint_new_for_tag (const gchar *tag, GError **error)
{
	AscHintTag *htag;
	g_autoptr(AscHint) hint = asc_hint_new ();

	htag = asc_globals_get_hint_tag_details (tag);
	if (htag == NULL || htag->severity == AS_ISSUE_SEVERITY_UNKNOWN) {
		g_set_error (
		    error,
		    ASC_COMPOSE_ERROR,
		    ASC_COMPOSE_ERROR_FAILED,
		    "The selected hint tag '%s' could not be found. Unable to create hint object.",
		    tag);
		return NULL;
	}

	asc_hint_set_tag (hint, htag->tag);
	asc_hint_set_severity (hint, htag->severity);
	asc_hint_set_explanation_template (hint, htag->explanation);
	return g_steal_pointer (&hint);
}
