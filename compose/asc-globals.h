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

/**
 * AscComposeError:
 * @ASC_COMPOSE_ERROR_FAILED:	Generic failure.
 *
 * A metadata composition error.
 **/
typedef enum {
	ASC_COMPOSE_ERROR_FAILED,
	/*< private >*/
	ASC_COMPOSE_ERROR_LAST
} AscComposeError;

#define	ASC_COMPOSE_ERROR	asc_compose_error_quark ()
GQuark				asc_compose_error_quark (void);

void				asc_globals_clear (void);

const gchar			*asc_globals_get_tmp_dir (void);
const gchar			*asc_globals_get_tmp_dir_create (void);
void				asc_globals_set_tmp_dir (const gchar *path);

gboolean			asc_globals_get_use_optipng (void);
void				asc_globals_set_use_optipng (gboolean enabled);

const gchar			*asc_globals_get_optipng_binary (void);
void				asc_globals_set_optipng_binary (const gchar *path);

const gchar			*asc_globals_get_ffprobe_binary (void);
void				asc_globals_set_ffprobe_binary (const gchar *path);

gboolean			asc_globals_add_hint_tag (const gchar *tag,
							  AsIssueSeverity severity,
							  const gchar *explanation,
							  gboolean overrideExisting);
gchar				**asc_globals_get_hint_tags ();
AsIssueSeverity			asc_globals_hint_tag_severity (const gchar *tag);
const gchar			*asc_globals_hint_tag_explanation (const gchar *tag);

G_END_DECLS
