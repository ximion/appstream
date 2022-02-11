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

#if !defined (__APPSTREAM_COMPOSE_H) && !defined (ASC_COMPILATION)
#error "Only <appstream-compose.h> can be included directly."
#endif
#pragma once

#include <glib-object.h>
#include <appstream.h>

G_BEGIN_DECLS

#define ASC_TYPE_HINT (asc_hint_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscHint, asc_hint, ASC, HINT, GObject)

struct _AscHintClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
};

AscHint			*asc_hint_new (void);
AscHint			*asc_hint_new_for_tag (const gchar *tag,
						GError **error);

const gchar		*asc_hint_get_tag (AscHint *hint);
void			asc_hint_set_tag (AscHint *hint,
					  const gchar *tag);

AsIssueSeverity		asc_hint_get_severity (AscHint *hint);
void			asc_hint_set_severity (AscHint *hint,
					       AsIssueSeverity severity);

const gchar		*asc_hint_get_explanation_template (AscHint *hint);
void			asc_hint_set_explanation_template (AscHint *hint,
							   const gchar *explanation_tmpl);

gboolean		asc_hint_is_error (AscHint *hint);
gboolean		asc_hint_is_valid (AscHint *hint);

void			asc_hint_add_explanation_var (AscHint *hint,
						      const gchar *var_name,
						      const gchar *text);
GPtrArray		*asc_hint_get_explanation_vars_list (AscHint *hint);

gchar			*asc_hint_format_explanation (AscHint *hint);

G_END_DECLS
