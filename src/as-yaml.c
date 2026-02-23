/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-utils.h"
#include "as-utils-private.h"

/**
 * SECTION:as-yaml
 * @short_description: Helper functions to parse AppStream YAML data
 * @include: appstream.h
 */

/**
 * as_yaml_error_diag_create:
 *
 * Helper method to create a fy_diag that is preconfigured
 * for collecting errors.
 *
 * Returns: (transfer full): newly allocated #fy_diag structure
 */
struct fy_diag *
as_yaml_error_diag_create (void)
{
	struct fy_diag_cfg dcfg;
	struct fy_diag *ydiag;

	fy_diag_cfg_default (&dcfg);
	dcfg.colorize = false;
	dcfg.level = FYET_ERROR;

	ydiag = fy_diag_create (&dcfg);
	fy_diag_set_collect_errors (ydiag, true);

	return ydiag;
}

/**
 * as_yaml_make_error_message:
 * @diag: the #fy_diag structure to extract errors from
 *
 * Helper method to create a nice error message from a libfyaml
 * diagnostic structure.
 *
 * Returns: (transfer full): newly allocated error message, or NULL if no error.
 */
gchar *
as_yaml_make_error_message (struct fy_diag *diag)
{
	struct fy_diag_error *err;
	g_autoptr(GString) msg = NULL;
	gpointer iter = NULL;

	if (!fy_diag_got_error (diag))
		return NULL;

	msg = g_string_new ("");
	while ((err = fy_diag_errors_iterate (diag, &iter)) != NULL) {
		g_string_append_printf (msg, "%d:%d %s\n", err->line, err->column, err->msg);
	}

	if (msg->len > 0 && msg->str[msg->len - 1] == '\n')
		g_string_truncate (msg, msg->len - 1);

	return g_string_free (g_steal_pointer (&msg), FALSE);
}

/**
 * as_yaml_node_get_key0:
 *
 * Helper method to get the key of a node as zero-terminated string.
 * If the key is not a scalar, NULL is returned.
 */
const gchar *
as_yaml_node_get_key0 (struct fy_node_pair *ynp)
{
	struct fy_node *key_n = fy_node_pair_key (ynp);
	if (key_n == NULL)
		return NULL;

	return fy_node_get_scalar0 (key_n);
}

/**
 * as_yaml_node_get_key:
 *
 * Helper method to get the key of a node as string+length.
 * The resulting string is not zero-terminated!
 * If the key is not a scalar, NULL is returned.
 */
const gchar *
as_yaml_node_get_key (struct fy_node_pair *ynp, size_t *lenp)
{
	struct fy_node *key_n = fy_node_pair_key (ynp);
	if (key_n == NULL)
		return NULL;

	return fy_node_get_scalar (key_n, lenp);
}

/**
 * as_yaml_node_get_value0:
 *
 * Helper method to get the value of a node as zero-terminated string.
 */
const gchar *
as_yaml_node_get_value0 (struct fy_node_pair *ynp)
{
	struct fy_node *val_n = fy_node_pair_value (ynp);
	if (val_n == NULL)
		return NULL;

	return fy_node_get_scalar0 (val_n);
}

/**
 * as_yaml_node_get_value:
 *
 * Helper method to get the value of a node as string+length.
 * The resulting string is not zero-terminated!
 */
const gchar *
as_yaml_node_get_value (struct fy_node_pair *ynp, size_t *lenp)
{
	struct fy_node *val_n = fy_node_pair_value (ynp);
	if (val_n == NULL)
		return NULL;

	return fy_node_get_scalar (val_n, lenp);
}

/**
 * as_yaml_node_get_key_refstr:
 *
 * Helper method to get the key of a node.
 */
GRefString *
as_yaml_node_get_key_refstr (struct fy_node_pair *ynp)
{
	const gchar *key = as_yaml_node_get_key0 (ynp);
	if (key == NULL)
		return NULL;
	return g_ref_string_new_intern (key);
}

/**
 * as_yaml_node_get_value_refstr:
 *
 * Helper method to get the value of a node.
 */
GRefString *
as_yaml_node_get_value_refstr (struct fy_node_pair *ynp)
{
	const gchar *value = as_yaml_node_get_value0 (ynp);
	if (value == NULL)
		return NULL;
	return g_ref_string_new_intern (value);
}

/**
 * as_yaml_get_localized_node:
 * @ctx: AsContext
 * @node: A YAML node
 * @locale_override: Locale override or %NULL
 *
 * Returns: A localized node if found, %NULL otherwise
 */
struct fy_node *
as_yaml_get_localized_node (AsContext *ctx, struct fy_node *node, const gchar *locale_override)
{
	struct fy_node_pair *fynp;
	void *iter = NULL;
	const gchar *target_locale;

	if (node == NULL || !fy_node_is_mapping (node))
		return NULL;

	target_locale = locale_override ? locale_override : as_context_get_locale (ctx);

	/* Iterate through the mapping to find the locale */
	while ((fynp = fy_node_mapping_iterate (node, &iter)) != NULL) {
		struct fy_node *key_node = fy_node_pair_key (fynp);
		const gchar *locale = NULL;

		if (fy_node_is_scalar (key_node))
			locale = fy_node_get_scalar0 (key_node);

		if (locale != NULL) {
			if (as_context_get_locale_use_all (ctx) || g_strcmp0 (locale, "C") == 0 ||
			    as_utils_locale_is_compatible (target_locale, locale)) {
				return fy_node_pair_value (fynp);
			}
		}
	}

	return NULL;
}

/**
 * as_yaml_get_node_locale:
 * @ctx: AsContext
 * @node_pair: A YAML node pair
 *
 * Returns: The locale of a node, if the node should be considered for inclusion.
 * %NULL if the node should be ignored due to a not-matching locale.
 */
const gchar *
as_yaml_get_node_locale (AsContext *ctx, struct fy_node_pair *node_pair)
{
	const gchar *key = as_yaml_node_get_key0 (node_pair);

	if (as_context_get_locale_use_all (ctx)) {
		/* we should read all languages */
		return key;
	}

	/* we always include the untranslated strings */
	if (g_strcmp0 (key, "C") == 0)
		return key;

	if (as_utils_locale_is_compatible (as_context_get_locale (ctx), key)) {
		return key;
	} else {
		/* If we are here, we haven't found a matching locale.
		 * In that case, we return %NULL to indicate that this element should not be added.
		 */
		return NULL;
	}
}

/**
 * as_yaml_set_localized_table:
 *
 * Apply node values to a hash table holding the l10n data.
 */
void
as_yaml_set_localized_table (AsContext *ctx, struct fy_node *node, GHashTable *l10n_table)
{
	struct fy_node_pair *fynp;
	void *iter = NULL;

	if (node == NULL || !fy_node_is_mapping (node))
		return;

	while ((fynp = fy_node_mapping_iterate (node, &iter)) != NULL) {
		const gchar *locale = as_yaml_get_node_locale (ctx, fynp);

		if (locale != NULL) {
			size_t value_len;
			g_autofree gchar *locale_noenc = as_locale_strip_encoding (locale);
			const gchar *value = as_yaml_node_get_value (fynp, &value_len);
			if (value != NULL) {
				g_hash_table_insert (l10n_table,
						     g_ref_string_new_intern (locale_noenc),
						     g_strndup (value, value_len));
			}
		}
	}
}

/**
 * as_yaml_list_to_str_array:
 */
void
as_yaml_list_to_str_array (struct fy_node *node, GPtrArray *array)
{
	struct fy_node *item_node;
	void *iter = NULL;

	if (node == NULL || !fy_node_is_sequence (node))
		return;

	while ((item_node = fy_node_sequence_iterate (node, &iter)) != NULL) {
		size_t val_len;
		const gchar *val = fy_node_get_scalar (item_node, &val_len);
		if (val != NULL)
			g_ptr_array_add (array, g_strndup (val, val_len));
	}
}

/**
 * as_yaml_print_unknown:
 */
void
as_yaml_print_unknown (const gchar *root, const gchar *key, ssize_t key_len)
{
	if (key_len < 0)
		g_debug ("YAML: Unknown field '%s/%s' found.", root, key);
	else
		g_debug ("YAML: Unknown field '%s/%.*s' found.", root, (int) key_len, key);
}

/**
 * as_yaml_mapping_start:
 */
void
as_yaml_mapping_start (struct fy_emitter *emitter)
{
	struct fy_event *fye;

	fye = fy_emit_event_create (emitter, FYET_MAPPING_START, FYNS_BLOCK, NULL, NULL);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_mapping_end:
 */
void
as_yaml_mapping_end (struct fy_emitter *emitter)
{
	struct fy_event *fye;

	fye = fy_emit_event_create (emitter, FYET_MAPPING_END);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_sequence_start:
 */
void
as_yaml_sequence_start (struct fy_emitter *emitter)
{
	struct fy_event *fye;

	fye = fy_emit_event_create (emitter, FYET_SEQUENCE_START, FYNS_BLOCK, NULL, NULL);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_sequence_end:
 */
void
as_yaml_sequence_end (struct fy_emitter *emitter)
{
	struct fy_event *fye;

	fye = fy_emit_event_create (emitter, FYET_SEQUENCE_END);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_emit_scalar:
 */
void
as_yaml_emit_scalar (struct fy_emitter *emitter, const gchar *value)
{
	struct fy_event *fye;
	g_return_if_fail (value != NULL);

	fye = fy_emit_event_create (emitter, FYET_SCALAR, FYSS_ANY, value, FY_NT, NULL, NULL);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_emit_scalar_str:
 * @emitter: the YAML emitter.
 * @value: the string value to emit.
 *
 * Emit a string scalar and ensure it is always quoted.
 */
void
as_yaml_emit_scalar_str (struct fy_emitter *emitter, const gchar *value)
{
	struct fy_event *fye;
	enum fy_scalar_style style = FYSS_PLAIN;
	g_return_if_fail (value != NULL);

	/* Some strings are interpreted as booleans or numbers if unquoted, so we need to quote them.
	 * This function needs to be very fast, so we may quote more than we need this way, but
	 * comparing a bunch of bytes is very fast compared to scanning a string.
	 * We use double-quoting, since English strings are more likely to contain apostrophes, so
	 * double-quotes may lead to less escaping than single-quotes.
	 * The punctuation check is done for numbers like +42, as well as to guard against YAML
	 * weirdness in case any string starts with "#" or "!". */
	if (as_is_empty (value))
		style = FYSS_DOUBLE_QUOTED;
	else if (g_ascii_isdigit (value[0]) || g_ascii_ispunct (value[0]))
		style = FYSS_DOUBLE_QUOTED;
	else if (as_str_equal0 (value, "true") || as_str_equal0 (value, "false"))
		style = FYSS_DOUBLE_QUOTED;

	fye = fy_emit_event_create (emitter, FYET_SCALAR, style, value, FY_NT, NULL, NULL);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_emit_scalar_uint64:
 */
void
as_yaml_emit_scalar_uint64 (struct fy_emitter *emitter, guint64 value)
{
	struct fy_event *fye;
	g_autofree gchar *value_str = NULL;

	value_str = g_strdup_printf ("%" G_GUINT64_FORMAT, value);
	fye = fy_emit_event_create (emitter, FYET_SCALAR, FYSS_PLAIN, value_str, FY_NT, NULL, NULL);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_emit_scalar_key:
 */
void
as_yaml_emit_scalar_key (struct fy_emitter *emitter, const gchar *key)
{
	struct fy_event *fye;
	enum fy_scalar_style keystyle;

	/* Some locale are "no", which - if unquoted - are interpreted as booleans.
	 * Since we never have boolean keys, we can disallow creating bool keys for all keys. */
	keystyle = FYSS_ANY;
	if (g_strcmp0 (key, "no") == 0)
		keystyle = FYSS_SINGLE_QUOTED;
	else if (g_strcmp0 (key, "yes") == 0)
		keystyle = FYSS_SINGLE_QUOTED;

	fye = fy_emit_event_create (emitter, FYET_SCALAR, keystyle, key, FY_NT, NULL, NULL);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_emit_entry:
 */
void
as_yaml_emit_entry (struct fy_emitter *emitter, const gchar *key, const gchar *value)
{
	if (value == NULL)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	as_yaml_emit_scalar (emitter, value);
}

/**
 * as_yaml_emit_entry_str:
 * @emitter: the YAML emitter.
 * @key: the key to emit.
 * @value: the string value to emit.
 *
 * Emit a kv pair, and ensure the string value is always quoted.
 */
void
as_yaml_emit_entry_str (struct fy_emitter *emitter, const gchar *key, const gchar *value)
{
	if (value == NULL)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	as_yaml_emit_scalar_str (emitter, value);
}

/**
 * as_yaml_emit_entry_uint64:
 */
void
as_yaml_emit_entry_uint64 (struct fy_emitter *emitter, const gchar *key, guint64 value)
{
	as_yaml_emit_scalar_key (emitter, key);
	as_yaml_emit_scalar_uint64 (emitter, value);
}

/**
 * as_yaml_emit_entry_timestamp:
 */
void
as_yaml_emit_entry_timestamp (struct fy_emitter *emitter, const gchar *key, guint64 unixtime)
{
	struct fy_event *fye;
	g_autofree gchar *time_str = NULL;

	as_yaml_emit_scalar_key (emitter, key);

	time_str = g_strdup_printf ("%" G_GUINT64_FORMAT, unixtime);
	fye = fy_emit_event_create (emitter, FYET_SCALAR, FYSS_ANY, time_str, FY_NT, NULL, NULL);
	if (fye != NULL) {
		fy_emit_event (emitter, fye);
	}
}

/**
 * as_yaml_emit_long_entry:
 */
void
as_yaml_emit_long_entry (struct fy_emitter *emitter, const gchar *key, const gchar *value)
{
	struct fy_event *fye;

	if (value == NULL)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	fye = fy_emit_event_create (emitter, FYET_SCALAR, FYSS_FOLDED, value, FY_NT, NULL, NULL);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_emit_long_entry_literal:
 */
void
as_yaml_emit_long_entry_literal (struct fy_emitter *emitter, const gchar *key, const gchar *value)
{
	struct fy_event *fye;

	if (value == NULL)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	fye = fy_emit_event_create (emitter, FYET_SCALAR, FYSS_LITERAL, value, FY_NT, NULL, NULL);
	if (fye != NULL)
		fy_emit_event (emitter, fye);
}

/**
 * as_yaml_emit_sequence:
 */
void
as_yaml_emit_sequence (struct fy_emitter *emitter, const gchar *key, GPtrArray *list)
{
	guint i;

	if (list == NULL)
		return;
	if (list->len == 0)
		return;

	as_yaml_emit_scalar_key (emitter, key);

	as_yaml_sequence_start (emitter);
	for (i = 0; i < list->len; i++) {
		const gchar *value = (const gchar *) g_ptr_array_index (list, i);
		as_yaml_emit_scalar (emitter, value);
	}
	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_emit_localized_entry_with_func:
 */
static void
as_yaml_emit_localized_entry_with_func (struct fy_emitter *emitter,
					const gchar *key,
					GHashTable *ltab,
					GHFunc tfunc)
{
	if (ltab == NULL)
		return;
	if (g_hash_table_size (ltab) == 0)
		return;

	as_yaml_emit_scalar (emitter, key);

	/* start mapping for localized entry */
	as_yaml_mapping_start (emitter);
	/* emit entries */
	g_hash_table_foreach (ltab, tfunc, emitter);
	/* finalize */
	as_yaml_mapping_end (emitter);
}

/**
 * as_yaml_emit_lang_hashtable_entries:
 */
static void
as_yaml_emit_lang_hashtable_entries (gchar *key, gchar *value, struct fy_emitter *emitter)
{
	if (as_is_empty (value))
		return;

	/* skip cruft */
	if (as_is_cruft_locale (key))
		return;

	as_yaml_emit_entry_str (emitter, key, as_strstripnl (value));
}

/**
 * as_yaml_emit_localized_entry:
 */
void
as_yaml_emit_localized_entry (struct fy_emitter *emitter, const gchar *key, GHashTable *ltab)
{
	as_yaml_emit_localized_entry_with_func (emitter,
						key,
						ltab,
						(GHFunc) as_yaml_emit_lang_hashtable_entries);
}

/**
 * as_yaml_emit_lang_hashtable_entries_long:
 */
static void
as_yaml_emit_lang_hashtable_entries_long (gchar *key, gchar *value, struct fy_emitter *emitter)
{
	if (as_is_empty (value))
		return;

	/* skip cruft */
	if (as_is_cruft_locale (key))
		return;

	as_yaml_emit_long_entry (emitter, key, as_strstripnl (value));
}

/**
 * as_yaml_emit_long_localized_entry:
 */
void
as_yaml_emit_long_localized_entry (struct fy_emitter *emitter, const gchar *key, GHashTable *ltab)
{
	as_yaml_emit_localized_entry_with_func (emitter,
						key,
						ltab,
						(GHFunc) as_yaml_emit_lang_hashtable_entries_long);
}

/**
 * as_yaml_emit_sequence_from_str_array:
 */
void
as_yaml_emit_sequence_from_str_array (struct fy_emitter *emitter,
				      const gchar *key,
				      GPtrArray *array)
{
	if (array == NULL)
		return;
	if (array->len == 0)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	as_yaml_sequence_start (emitter);

	for (guint i = 0; i < array->len; i++) {
		const gchar *val = (const gchar *) g_ptr_array_index (array, i);
		as_yaml_emit_scalar (emitter, val);
	}

	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_localized_list_helper:
 */
static void
as_yaml_localized_list_helper (gchar *key, gchar **strv, struct fy_emitter *emitter)
{
	g_autofree gchar *locale_noenc = NULL;
	if (strv == NULL)
		return;

	/* skip cruft */
	if (as_is_cruft_locale (key))
		return;

	locale_noenc = as_locale_strip_encoding (key);
	as_yaml_emit_scalar (emitter, locale_noenc);
	as_yaml_sequence_start (emitter);
	for (guint i = 0; strv[i] != NULL; i++) {
		as_yaml_emit_scalar (emitter, strv[i]);
	}
	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_emit_localized_strv:
 * @ltab: Hash table of utf8->strv
 */
void
as_yaml_emit_localized_strv (struct fy_emitter *emitter, const gchar *key, GHashTable *ltab)
{
	if (ltab == NULL)
		return;
	if (g_hash_table_size (ltab) == 0)
		return;

	as_yaml_emit_scalar (emitter, key);

	/* start mapping for localized entry */
	as_yaml_mapping_start (emitter);
	/* emit entries */
	g_hash_table_foreach (ltab, (GHFunc) as_yaml_localized_list_helper, emitter);
	/* finalize */
	as_yaml_mapping_end (emitter);
}

/**
 * as_yaml_emit_localized_str_array:
 * @ltab: Hash table of utf8->GPtrArray[utf8]
 */
void
as_yaml_emit_localized_str_array (struct fy_emitter *emitter, const gchar *key, GHashTable *ltab)
{
	GHashTableIter iter;
	gpointer ht_key, ht_value;

	if (ltab == NULL)
		return;
	if (g_hash_table_size (ltab) == 0)
		return;

	as_yaml_emit_scalar (emitter, key);

	/* start mapping for localized entry */
	as_yaml_mapping_start (emitter);

	/* emit entries */
	g_hash_table_iter_init (&iter, ltab);
	while (g_hash_table_iter_next (&iter, &ht_key, &ht_value)) {
		g_autofree gchar *locale_noenc = NULL;
		GPtrArray *array = ht_value;

		/* skip cruft */
		if (as_is_cruft_locale (ht_key))
			continue;

		locale_noenc = as_locale_strip_encoding (ht_key);
		as_yaml_emit_scalar (emitter, locale_noenc);
		as_yaml_sequence_start (emitter);
		for (guint i = 0; i < array->len; i++)
			as_yaml_emit_scalar_str (emitter, g_ptr_array_index (array, i));
		as_yaml_sequence_end (emitter);
	}

	/* finalize */
	as_yaml_mapping_end (emitter);
}
