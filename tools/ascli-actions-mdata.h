/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ASCLI_ACTIONS_MDATA_H
#define __ASCLI_ACTIONS_MDATA_H

#include <glib-object.h>

G_BEGIN_DECLS

int		ascli_what_provides (const gchar *dbpath,
					const gchar *kind_str,
					const gchar *item,
					gboolean detailed);

int		ascli_search_component (const gchar *dbpath,
					const gchar *search_term,
					gboolean detailed);

int		ascli_get_component (const gchar *dbpath,
					const gchar *identifier,
					gboolean detailed,
					gboolean no_cache);

int		ascli_refresh_cache (const gchar *dbpath,
					const gchar *datapath,
					gboolean forced);

int		ascli_dump_component (const gchar *dbpath,
					const gchar *identifier,
					gboolean no_cache);

int		ascli_put_metainfo (const gchar *fname);


G_END_DECLS

#endif /* __ASCLI_ACTIONS_MDATA_H */
