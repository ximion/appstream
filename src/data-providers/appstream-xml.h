/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_DATAPROVIDERXML_H
#define __AS_DATAPROVIDERXML_H

#include <glib-object.h>
#include <gio/gio.h>
#include "../as-data-provider.h"

#define AS_PROVIDER_TYPE_XML (as_provider_xml_get_type ())
#define AS_PROVIDER_XML(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_PROVIDER_TYPE_XML, AsProviderXML))
#define AS_PROVIDER_XML_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_PROVIDER_TYPE_XML, AsProviderXMLClass))
#define AS_PROVIDER_IS_APPSTREAM_XML(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_PROVIDER_TYPE_XML))
#define AS_PROVIDER_IS_APPSTREAM_XML_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_PROVIDER_TYPE_XML))
#define AS_PROVIDER_XML_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_PROVIDER_TYPE_XML, AsProviderXMLClass))

G_BEGIN_DECLS

typedef struct _AsProviderXML AsProviderXML;
typedef struct _AsProviderXMLClass AsProviderXMLClass;
typedef struct _AsProviderXMLPrivate AsProviderXMLPrivate;

struct _AsProviderXML {
	AsDataProvider parent_instance;
	AsProviderXMLPrivate * priv;
};

struct _AsProviderXMLClass {
	AsDataProviderClass parent_class;
};

GType as_provider_xml_get_type (void) G_GNUC_CONST;

AsProviderXML* as_provider_xml_new (void);
AsProviderXML* as_provider_xml_construct (GType object_type);
gboolean as_provider_xml_process_compressed_file (AsProviderXML *self,
												GFile *infile);
gboolean as_provider_xml_process_file (AsProviderXML *self,
									   GFile *infile);

G_END_DECLS

#endif /* __AS_DATAPROVIDERXML_H */
