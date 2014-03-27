/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef __AS_COMPONENTPRIVATE_H
#define __AS_COMPONENTPRIVATE_H

#include <glib-object.h>
#include "as-component.h"

G_BEGIN_DECLS

gchar*				as_component_dump_screenshot_data_xml (AsComponent* self);
void				as_component_load_screenshots_from_internal_xml (AsComponent* self, const gchar* xmldata);

G_END_DECLS

#endif /* __AS_COMPONENTPRIVATE_H */
