/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ASCLI_ACTIONS_VALIDATE_H
#define __ASCLI_ACTIONS_VALIDATE_H

#include <glib-object.h>

G_BEGIN_DECLS

gint			ascli_validate_files (gchar **argv,
						gint argc,
						gboolean explain,
						gboolean pedantic,
						gboolean validate_strict,
						gboolean use_net,
						const gchar *overrides_str);
gint			ascli_validate_files_format (gchar **argv,
							gint argc,
							const gchar *format,
							gboolean validate_strict,
							gboolean use_net,
							const gchar *overrides_str);

gint			ascli_validate_tree (const gchar *root_dir,
						gboolean explain,
						gboolean pedantic,
						gboolean validate_strict,
						gboolean use_net,
						const gchar *overrides_str);
gint			ascli_validate_tree_format (const gchar *root_dir,
						    const gchar *format,
						    gboolean validate_strict,
						    gboolean use_net,
						    const gchar *overrides_str);

gint			ascli_check_license (const gchar *license);

G_END_DECLS

#endif /* __ASCLI_ACTIONS_VALIDATE_H */
