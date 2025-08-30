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

#if !defined(__APPSTREAM_H) && !defined(AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_YAML_H
#define __AS_YAML_H

#include <libfyaml.h>

#include "config.h"
#include "as-context.h"
#include "as-metadata.h"
#include "as-macros-private.h"
#include "as-tag.h"

AS_BEGIN_PRIVATE_DECLS

/**
 * AS_FYAML_CHECK_VERSION:
 * @major: major version number
 * @minor: minor version number
 *
 * Check whether a libfyaml version equal to or greater than
 * major.minor is present.
 */
#define AS_FYAML_CHECK_VERSION(major, minor) \
    (_FYAML_MAJOR_VERSION > (major) || \
     (_FYAML_MAJOR_VERSION == (major) && _FYAML_MINOR_VERSION > (minor)) || \
     (_FYAML_MAJOR_VERSION == (major) && _FYAML_MINOR_VERSION == (minor)))

/**
 * Helper macro for iterating through YAML sequences more compactly - we do this a lot!
 * The macro ensures all variables are only visible within the loop body.
 */
#define AS_YAML_SEQUENCE_FOREACH(node_var, seq_node)                                        \
	for (gpointer _iter_##seq_node = NULL; _iter_##seq_node == NULL;                    \
	     _iter_##seq_node	       = GINT_TO_POINTER (1))                               \
		 for (struct fy_node *node_var =                                    \
			  fy_node_sequence_iterate ((seq_node), &_iter_##seq_node); \
		      (node_var) != NULL;                                           \
		      (node_var) = fy_node_sequence_iterate ((seq_node), &_iter_##seq_node))

/**
 * Helper macro for iterating through YAML mappings in a compact way, ensuring all variables
 * are only visible within the loop body.
 */
#define AS_YAML_MAPPING_FOREACH(pair, map_node)                                            \
	for (gpointer _iter_##map_node = NULL; _iter_##map_node == NULL;                   \
	     _iter_##map_node	       = GINT_TO_POINTER (1))                              \
		 for (struct fy_node_pair *pair =                                  \
			  fy_node_mapping_iterate ((map_node), &_iter_##map_node); \
		      (pair) != NULL;                                              \
		      (pair) = fy_node_mapping_iterate ((map_node), &_iter_##map_node))

struct fy_diag	      *as_yaml_error_diag_create (void);
gchar		      *as_yaml_make_error_message (struct fy_diag *diag);

const gchar	      *as_yaml_node_get_key0 (struct fy_node_pair *ynp);
const gchar	      *as_yaml_node_get_key (struct fy_node_pair *ynp, size_t *lenp);
const gchar	      *as_yaml_node_get_value (struct fy_node_pair *ynp);

GRefString	      *as_yaml_node_get_key_refstr (struct fy_node_pair *ynp);
GRefString	      *as_yaml_node_get_value_refstr (struct fy_node_pair *ynp);

void		       as_yaml_print_unknown (const gchar *root, const gchar *key, ssize_t key_len);

/* these functions have internal visibility, so appstream-compose can write YAML data */
#pragma GCC visibility push(default)
void		       as_yaml_mapping_start (struct fy_emitter *emitter);
void		       as_yaml_mapping_end (struct fy_emitter *emitter);

void		       as_yaml_sequence_start (struct fy_emitter *emitter);
void		       as_yaml_sequence_end (struct fy_emitter *emitter);

void		       as_yaml_emit_long_entry_literal (struct fy_emitter *emitter,
							const gchar	  *key,
							const gchar	  *value);
void		       as_yaml_emit_scalar_raw (struct fy_emitter *emitter, const gchar *value);
void		       as_yaml_emit_scalar (struct fy_emitter *emitter, const gchar *value);
void		       as_yaml_emit_scalar_uint64 (struct fy_emitter *emitter, guint64 value);
void		       as_yaml_emit_scalar_key (struct fy_emitter *emitter, const gchar *key);
void as_yaml_emit_entry (struct fy_emitter *emitter, const gchar *key, const gchar *value);
void as_yaml_emit_entry_uint64 (struct fy_emitter *emitter, const gchar *key, guint64 value);
void as_yaml_emit_entry_timestamp (struct fy_emitter *emitter, const gchar *key, guint64 unixtime);
void as_yaml_emit_long_entry (struct fy_emitter *emitter, const gchar *key, const gchar *value);
void as_yaml_emit_sequence (struct fy_emitter *emitter, const gchar *key, GPtrArray *list);
#pragma GCC visibility pop
void as_yaml_emit_sequence_from_str_array (struct fy_emitter *emitter,
					   const gchar	     *key,
					   GPtrArray	     *array);
void as_yaml_emit_localized_strv (struct fy_emitter *emitter, const gchar *key, GHashTable *ltab);
void as_yaml_emit_localized_str_array (struct fy_emitter *emitter,
				       const gchar	 *key,
				       GHashTable	 *ltab);

struct fy_node *as_yaml_get_localized_node (AsContext	   *ctx,
					    struct fy_node *node,
					    const gchar	   *locale_override);
const gchar    *as_yaml_get_node_locale (AsContext *ctx, struct fy_node_pair *node_pair);
void as_yaml_set_localized_table (AsContext *ctx, struct fy_node *node, GHashTable *l10n_table);

void as_yaml_emit_localized_entry (struct fy_emitter *emitter, const gchar *key, GHashTable *ltab);
void as_yaml_emit_long_localized_entry (struct fy_emitter *emitter,
					const gchar	  *key,
					GHashTable	  *ltab);

void as_yaml_list_to_str_array (struct fy_node *node, GPtrArray *array);

AS_END_PRIVATE_DECLS

#endif /* __AS_YAML_H */
