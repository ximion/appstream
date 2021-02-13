/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2015 Richard Hughes <richard@hughsie.com>
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

#if !defined (AS_COMPILATION)
#error "Can not use internal AppStream API from external project."
#endif

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_PROFILE	(as_profile_get_type ())

G_DECLARE_FINAL_TYPE (AsProfile, as_profile, AS, PROFILE, GObject)

typedef struct _AsProfileTask AsProfileTask;

AsProfile	*as_profile_new			(void);
AsProfileTask	*as_profile_start_literal	(AsProfile	*profile,
						 const gchar	*id)
						 G_GNUC_WARN_UNUSED_RESULT;
AsProfileTask	*as_profile_start		(AsProfile	*profile,
						 const gchar	*fmt,
						 ...)
						 G_GNUC_PRINTF (2, 3)
						 G_GNUC_WARN_UNUSED_RESULT;
void		 as_profile_clear		(AsProfile	*profile);
void		 as_profile_prune		(AsProfile	*profile,
						 guint		 duration);
void		 as_profile_dump		(AsProfile	*profile);
void		 as_profile_set_autodump	(AsProfile	*profile,
						 guint		 delay);
void		 as_profile_set_autoprune	(AsProfile	*profile,
						 guint		 duration);
void		 as_profile_set_duration_min	(AsProfile	*profile,
						 guint		 duration_min);
void		 as_profile_task_set_threaded	(AsProfileTask	*ptask,
						 gboolean	 threaded);
void		 as_profile_task_free		(AsProfileTask	*ptask);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsProfileTask, as_profile_task_free)

G_END_DECLS
