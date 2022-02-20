/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __ASCLI_ACTIONS_PKGMGR_H
#define __ASCLI_ACTIONS_PKGMGR_H

#include <glib-object.h>
#include "appstream.h"

G_BEGIN_DECLS

int ascli_install_component (const gchar *identifier,
			     AsBundleKind bundle_kind,
			     gboolean choose_first);
int ascli_remove_component (const gchar *identifier,
			    AsBundleKind bundle_kind,
			    gboolean choose_first);

G_END_DECLS

#endif /* __ASCLI_ACTIONS_PKGMGR_H */
