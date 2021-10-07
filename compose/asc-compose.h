/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "asc-unit.h"

G_BEGIN_DECLS

#define ASC_TYPE_COMPOSE (asc_compose_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscCompose, asc_compose, ASC, COMPOSE, GObject)

struct _AscComposeClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
};

/**
 * AsCacheFlags:
 * @ASC_COMPOSE_FLAG_NONE:		No flags.
 * @ASC_COMPOSE_FLAG_ALLOW_NET:		Allow network access for downloading extra data.
 * @ASC_COMPOSE_FLAG_VALIDATE:		Validate metadata while processing.
 * @ASC_COMPOSE_FLAG_STORE_SCREENSHOTS:	Whether screenshots should be cached in the media directory.
 *
 * Flags on how caching should be used.
 **/
typedef enum {
	ASC_COMPOSE_FLAG_NONE			= 0,
	ASC_COMPOSE_FLAG_ALLOW_NET		= 1 << 0,
	ASC_COMPOSE_FLAG_VALIDATE		= 1 << 1,
	ASC_COMPOSE_FLAG_STORE_SCREENSHOTS	= 1 << 2
} AscComposeFlags;

AscCompose		*asc_compose_new (void);

void			asc_compose_reset (AscCompose *compose);
void			asc_compose_add_unit (AscCompose *compose,
					      AscUnit *unit);

void			asc_compose_add_allowed_cid (AscCompose *compose,
							const gchar *component_id);

const gchar		*asc_compose_get_prefix (AscCompose *compose);
void			asc_compose_set_prefix (AscCompose *compose,
						const gchar *prefix);

const gchar		*asc_compose_get_origin (AscCompose *compose);
void			asc_compose_set_origin (AscCompose *compose,
						const gchar *origin);

AsFormatKind		asc_compose_get_format (AscCompose *compose);
void			asc_compose_set_format (AscCompose *compose,
						AsFormatKind kind);

const gchar		*asc_compose_get_media_baseurl (AscCompose *compose);
void			asc_compose_set_media_baseurl (AscCompose *compose,
						       const gchar *url);

AscComposeFlags		asc_compose_get_flags (AscCompose *compose);
void			asc_compose_set_flags (AscCompose *compose,
					       AscComposeFlags flags);

const gchar		*asc_compose_get_data_result_dir (AscCompose *compose);
void			asc_compose_set_data_result_dir (AscCompose *compose,
							 const gchar *dir);

const gchar		*asc_compose_get_icons_result_dir (AscCompose *compose);
void			asc_compose_set_icons_result_dir (AscCompose *compose,
							  const gchar *dir);

const gchar		*asc_compose_get_media_result_dir (AscCompose *compose);
void			asc_compose_set_media_result_dir (AscCompose *compose,
							  const gchar *dir);

const gchar		*asc_compose_get_hints_result_dir (AscCompose *compose);
void			asc_compose_set_hints_result_dir (AscCompose *compose,
							  const gchar *dir);

GPtrArray		*asc_compose_get_results (AscCompose *compose);
GPtrArray		*asc_compose_fetch_components (AscCompose *compose);
gboolean		asc_compose_has_errors (AscCompose *compose);

GPtrArray		*asc_compose_run (AscCompose *compose,
					  GCancellable *cancellable,
					  GError **error);

G_END_DECLS
