/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __ASCLI_VALIDATE_ACTIONS_H
#define __ASCLI_VALIDATE_ACTIONS_H

#include <glib-object.h>

G_BEGIN_DECLS

gboolean		ascli_validate_file (gchar *fname,
						gboolean pretty,
						gboolean pedantic);

gint			ascli_validate_files (char **argv,
						int argc,
						gboolean no_color,
						gboolean pedantic);

G_END_DECLS

#endif /* __ASCLI_VALIDATE_ACTIONS_H */
