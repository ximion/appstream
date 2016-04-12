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

#ifndef __AS_XMLDATA_H
#define __AS_XMLDATA_H

#include <glib-object.h>
#include <libxml/tree.h>

#include "as-component.h"
#include "as-metadata.h"

G_BEGIN_DECLS

#define AS_TYPE_XMLDATA (as_xmldata_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsXMLData, as_xmldata, AS, XMLDATA, GObject)

struct _AsXMLDataClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

AsXMLData		*as_xmldata_new (void);

void			as_xmldata_initialize (AsXMLData *xdt,
						const gchar *locale,
						const gchar *origin,
						const gchar *media_baseurl,
						const gchar *arch,
						gint priority);

AsComponent		*as_xmldata_parse_upstream_data (AsXMLData *xdt,
							const gchar *data,
							GError **error);
GPtrArray		*as_xmldata_parse_distro_data (AsXMLData *xdt,
							const gchar *data,
							GError **error);
gboolean		as_xmldata_update_cpt_with_upstream_data (AsXMLData *xdt,
								const gchar *data,
								AsComponent *cpt,
								GError **error);

gchar			*as_xmldata_serialize_to_upstream (AsXMLData *xdt,
								AsComponent *cpt);
gchar			*as_xmldata_serialize_to_distro (AsXMLData *xdt,
							 GPtrArray *cpts,
							 gboolean write_header);

void			as_xmldata_set_parser_mode (AsXMLData *xdt,
							AsParserMode mode);
void			as_xmldata_parse_component_node (AsXMLData *metad,
								xmlNode *node,
								AsComponent *cpt,
								GError **error);


G_END_DECLS

#endif /* __AS_XMLDATA_H */
