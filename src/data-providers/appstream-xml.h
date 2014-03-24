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

#ifndef __AS_DATAPROVIDERXML_H
#define __AS_DATAPROVIDERXML_H

#include <glib-object.h>
#include "appstream_internal.h"
#include "../as-data-provider.h"

#define AS_PROVIDER_TYPE_APPSTREAM_XML (as_provider_appstream_xml_get_type ())
#define AS_PROVIDER_APPSTREAM_XML(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_PROVIDER_TYPE_APPSTREAM_XML, AsProviderAppstreamXML))
#define AS_PROVIDER_APPSTREAM_XML_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_PROVIDER_TYPE_APPSTREAM_XML, AsProviderAppstreamXMLClass))
#define AS_PROVIDER_IS_APPSTREAM_XML(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_PROVIDER_TYPE_APPSTREAM_XML))
#define AS_PROVIDER_IS_APPSTREAM_XML_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_PROVIDER_TYPE_APPSTREAM_XML))
#define AS_PROVIDER_APPSTREAM_XML_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_PROVIDER_TYPE_APPSTREAM_XML, AsProviderAppstreamXMLClass))

G_BEGIN_DECLS

typedef struct _AsProviderAppstreamXML AsProviderAppstreamXML;
typedef struct _AsProviderAppstreamXMLClass AsProviderAppstreamXMLClass;
typedef struct _AsProviderAppstreamXMLPrivate AsProviderAppstreamXMLPrivate;

struct _AsProviderAppstreamXML {
	AsDataProvider parent_instance;
	AsProviderAppstreamXMLPrivate * priv;
};

struct _AsProviderAppstreamXMLClass {
	AsDataProviderClass parent_class;
};

GType as_provider_appstream_xml_get_type (void) G_GNUC_CONST;

AsProviderAppstreamXML* as_provider_appstream_xml_new (void);
AsProviderAppstreamXML* as_provider_appstream_xml_construct (GType object_type);
gboolean as_provider_appstream_xml_process_compressed_file (AsProviderAppstreamXML* self, GFile* infile);
gboolean as_provider_appstream_xml_process_file (AsProviderAppstreamXML* self, GFile* infile);

G_END_DECLS

#endif /* __AS_DATAPROVIDERXML_H */
