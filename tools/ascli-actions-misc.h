/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __ASCLI_ACTIONS_MISC_H
#define __ASCLI_ACTIONS_MISC_H

#include <glib-object.h>
#include "appstream.h"

G_BEGIN_DECLS

gint		ascli_show_status (void);

gint		ascli_make_desktop_entry_file (const gchar *mi_fname,
					       const gchar *de_fname,
					       const gchar *exec_line);

gint		ascli_news_to_metainfo (const gchar *news_fname,
					const gchar *mi_fname,
					const gchar *out_fname,
					guint entry_limit,
					guint translate_limit,
					const gchar *format_str);
gint		ascli_metainfo_to_news (const gchar *mi_fname,
					const gchar *news_fname,
					const gchar *format_str);

gint		ascli_check_license (const gchar *license);

G_END_DECLS

#endif /* __ASCLI_ACTIONS_MISC_H */
