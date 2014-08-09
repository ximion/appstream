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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_DATAPROVIDER_H
#define __AS_DATAPROVIDER_H

#include <glib-object.h>
#include "as-component.h"

#define AS_TYPE_DATA_PROVIDER (as_data_provider_get_type ())
#define AS_DATA_PROVIDER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_TYPE_DATA_PROVIDER, AsDataProvider))
#define AS_DATA_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_TYPE_DATA_PROVIDER, AsDataProviderClass))
#define AS_IS_DATA_PROVIDER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_TYPE_DATA_PROVIDER))
#define AS_IS_DATA_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_TYPE_DATA_PROVIDER))
#define AS_DATA_PROVIDER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_TYPE_DATA_PROVIDER, AsDataProviderClass))

G_BEGIN_DECLS

typedef struct _AsDataProvider AsDataProvider;
typedef struct _AsDataProviderClass AsDataProviderClass;
typedef struct _AsDataProviderPrivate AsDataProviderPrivate;

struct _AsDataProvider
{
	GObject parent_instance;
	AsDataProviderPrivate * priv;
};

struct _AsDataProviderClass
{
	GObjectClass parent_class;
	gboolean (*execute) (AsDataProvider* self);
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

GType as_data_provider_get_type (void) G_GNUC_CONST;

AsDataProvider*		as_data_provider_construct (GType object_type);

void				as_data_provider_emit_application (AsDataProvider* self,
													   AsComponent* cpt);
gboolean			as_data_provider_execute (AsDataProvider* self);

void				as_data_provider_log_error (AsDataProvider* self,
												const gchar* msg);
void				as_data_provider_log_warning (AsDataProvider* self,
												  const gchar* msg);

const gchar*		as_data_provider_get_locale (AsDataProvider *dpool);
void				as_data_provider_set_locale (AsDataProvider *dprov,
												 const gchar *locale);

gchar**				as_data_provider_get_watch_files (AsDataProvider* self);
void				as_data_provider_set_watch_files (AsDataProvider* self,
													  gchar** value);

G_END_DECLS

#endif /* __AS_DATAPROVIDER_H */
