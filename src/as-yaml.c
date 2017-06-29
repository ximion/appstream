/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2017 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-yaml.h"

/**
 * SECTION:as-yaml
 * @short_description: Helper functions to parse AppStream YAML data
 * @include: appstream.h
 */

/**
 * as_str_is_numeric:
 *
 * Check if string is a number.
 */
static gboolean
as_str_is_numeric (const gchar *s)
{
	gchar *p;

	if (s == NULL || *s == '\0' || g_ascii_isspace (*s))
		return FALSE;
	strtod (s, &p);
	return *p == '\0';
}

/**
 * as_yaml_node_get_key:
 *
 * Helper method to get the key of a node.
 */
const gchar*
as_yaml_node_get_key (GNode *n)
{
	return (const gchar*) n->data;
}

/**
 * as_yaml_node_get_value:
 *
 * Helper method to get the value of a node.
 */
const gchar*
as_yaml_node_get_value (GNode *n)
{
	if (n->children)
		return (const gchar*) n->children->data;
	else
		return NULL;
}

/**
 * as_yaml_print_unknown:
 */
void
as_yaml_print_unknown (const gchar *root, const gchar *key)
{
	g_debug ("YAML: Unknown field '%s/%s' found.", root, key);
}

/**
 * as_yaml_mapping_start:
 */
void
as_yaml_mapping_start (yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_mapping_start_event_initialize (&event, NULL, NULL, 1, YAML_ANY_MAPPING_STYLE);
	g_assert (yaml_emitter_emit (emitter, &event));
}

/**
 * as_yaml_mapping_end:
 */
void
as_yaml_mapping_end (yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_mapping_end_event_initialize (&event);
	g_assert (yaml_emitter_emit (emitter, &event));
}

/**
 * as_yaml_sequence_start:
 */
void
as_yaml_sequence_start (yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_sequence_start_event_initialize (&event, NULL, NULL, 1, YAML_ANY_SEQUENCE_STYLE);
	g_assert (yaml_emitter_emit (emitter, &event));
}

/**
 * as_yaml_sequence_end:
 */
void
as_yaml_sequence_end (yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_sequence_end_event_initialize (&event);
	g_assert (yaml_emitter_emit (emitter, &event));
}

/**
 * as_yaml_emit_scalar:
 */
void
as_yaml_emit_scalar (yaml_emitter_t *emitter, const gchar *value)
{
	gint ret;
	yaml_event_t event;
	yaml_scalar_style_t style;
	g_assert (value != NULL);

	/* we always want the values to be represented as strings, and not have e.g. Python recognize them as ints later */
	style = YAML_ANY_SCALAR_STYLE;
	if (as_str_is_numeric (value))
		style = YAML_SINGLE_QUOTED_SCALAR_STYLE;

	yaml_scalar_event_initialize (&event,
					NULL,
					NULL,
					(yaml_char_t*) value,
					strlen (value),
					TRUE,
					TRUE,
					style);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);
}

/**
 * as_yaml_emit_scalar_uint:
 */
void
as_yaml_emit_scalar_uint (yaml_emitter_t *emitter, guint value)
{
	gint ret;
	yaml_event_t event;
	g_autofree gchar *value_str = NULL;

	value_str = g_strdup_printf("%i", value);
	yaml_scalar_event_initialize (&event,
					NULL,
					NULL,
					(yaml_char_t*) value_str,
					strlen (value_str),
					TRUE,
					TRUE,
					YAML_ANY_SCALAR_STYLE);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);
}

/**
 * as_yaml_emit_scalar_key:
 */
void
as_yaml_emit_scalar_key (yaml_emitter_t *emitter, const gchar *key)
{
	yaml_scalar_style_t keystyle;
	yaml_event_t event;
	gint ret;

	/* Some locale are "no", which - if unquoted - are interpreted as booleans.
	 * Since we hever have boolean keys, we can disallow creating bool keys for all keys. */
	keystyle = YAML_ANY_SCALAR_STYLE;
	if (g_strcmp0 (key, "no") == 0)
		keystyle = YAML_SINGLE_QUOTED_SCALAR_STYLE;
	if (g_strcmp0 (key, "yes") == 0)
		keystyle = YAML_SINGLE_QUOTED_SCALAR_STYLE;

	yaml_scalar_event_initialize (&event,
					NULL,
					NULL,
					(yaml_char_t*) key,
					strlen (key),
					TRUE,
					TRUE,
					keystyle);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);
}

/**
 * as_yaml_emit_entry:
 */
void
as_yaml_emit_entry (yaml_emitter_t *emitter, const gchar *key, const gchar *value)
{
	if (value == NULL)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	as_yaml_emit_scalar (emitter, value);
}

/**
 * as_yaml_emit_entry_uint:
 */
void
as_yaml_emit_entry_uint (yaml_emitter_t *emitter, const gchar *key, guint value)
{
	as_yaml_emit_scalar_key (emitter, key);
	as_yaml_emit_scalar_uint (emitter, value);
}

/**
 * as_yaml_emit_entry_timestamp:
 */
void
as_yaml_emit_entry_timestamp (yaml_emitter_t *emitter, const gchar *key, guint64 unixtime)
{
	g_autofree gchar *time_str = NULL;
	yaml_event_t event;
	gint ret;

	as_yaml_emit_scalar_key (emitter, key);

	time_str = g_strdup_printf ("%" G_GUINT64_FORMAT, unixtime);
	yaml_scalar_event_initialize (&event,
					NULL,
					NULL,
					(yaml_char_t*) time_str,
					strlen (time_str),
					TRUE,
					TRUE,
					YAML_ANY_SCALAR_STYLE);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);

}

/**
 * as_yaml_emit_long_entry:
 */
void
as_yaml_emit_long_entry (yaml_emitter_t *emitter, const gchar *key, const gchar *value)
{
	yaml_event_t event;
	gint ret;

	if (value == NULL)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	yaml_scalar_event_initialize (&event,
					NULL,
					NULL,
					(yaml_char_t*) value,
					strlen (value),
					TRUE,
					TRUE,
					YAML_FOLDED_SCALAR_STYLE);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);
}
