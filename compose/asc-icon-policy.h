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

#define ASC_TYPE_ICON_POLICY (asc_icon_policy_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscIconPolicy, asc_icon_policy, ASC, ICON_POLICY, GObject)

struct _AscIconPolicyClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
};

typedef struct
{
  /*< private >*/
  gpointer	dummy1;
  guint		dummy2;
  gpointer	dummy3;
  gpointer	dummy4;
  gpointer	dummy5;
  gpointer	dummy6;
} AscIconPolicyIter;

/**
 * AscIconState:
 * @ASC_ICON_STATE_IGNORED:		Ignore icons of this size.
 * @ASC_ICON_STATE_CACHED_REMOTE:	Create cache for the icon, and provide remote link as well.
 * @ASC_ICON_STATE_CACHED_ONLY:		Set if the icon should be stored in an icon tarball and be cached locally.
 * @ASC_ICON_STATE_REMOTE_ONLY:		Set if this icon should be stored remotely and fetched on demand.
 *
 * Designated state for an icon of a given size.
 **/
typedef enum {
	ASC_ICON_STATE_IGNORED,
	ASC_ICON_STATE_CACHED_REMOTE,
	ASC_ICON_STATE_CACHED_ONLY,
	ASC_ICON_STATE_REMOTE_ONLY
} AscIconState;

AscIconPolicy			*asc_icon_policy_new (void);

void				asc_icon_policy_set_policy (AscIconPolicy *ipolicy,
							    guint icon_size,
							    guint icon_scale,
							    AscIconState state);

void				asc_icon_policy_iter_init (AscIconPolicyIter *iter,
							   AscIconPolicy *ipolicy);
gboolean			asc_icon_policy_iter_next (AscIconPolicyIter *iter,
							   guint *size,
							   guint *scale,
							   AscIconState *state);

G_END_DECLS
