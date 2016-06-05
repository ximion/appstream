/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_XML_BASE_H
#define __AS_XML_BASE_H

#include <glib.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

gchar*		as_xml_get_node_value (xmlNode *node);
gchar*		as_xml_dump_node_children (xmlNode *node);
gchar**		as_xml_get_children_as_strv (xmlNode* node, const gchar* element_name);
xmlDoc*		as_xml_parse_document (const gchar *data, GError **error);

G_END_DECLS

#endif /* __AS_XML_BASE_H */
