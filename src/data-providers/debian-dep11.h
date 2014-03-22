/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#ifndef __AS_DATAPROVIDERDEP11_H
#define __AS_DATAPROVIDERDEP11_H

#include <glib-object.h>
#include "appstream_internal.h"

#define AS_PROVIDER_TYPE_DEP11 (as_provider_dep11_get_type ())
#define AS_PROVIDER_DE_P11(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_PROVIDER_TYPE_DEP11, AsProviderDEP11))
#define AS_PROVIDER_DE_P11_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_PROVIDER_TYPE_DEP11, AsProviderDEP11Class))
#define AS_PROVIDER_IS_DEP11(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_PROVIDER_TYPE_DEP11))
#define AS_PROVIDER_IS_DEP11_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_PROVIDER_TYPE_DEP11))
#define AS_PROVIDER_DEP11_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_PROVIDER_TYPE_DEP11, AsProviderDEP11Class))

G_BEGIN_DECLS

typedef struct _AsProviderDEP11 AsProviderDEP11;
typedef struct _AsProviderDEP11Class AsProviderDEP11Class;
typedef struct _AsProviderDEP11Private AsProviderDEP11Private;

struct _AsProviderDEP11 {
	AsDataProvider parent_instance;
	AsProviderDEP11Private * priv;
};

struct _AsProviderDEP11Class {
	AsDataProviderClass parent_class;
};

AsProviderDEP11*		as_provider_dep11_new (void);
AsProviderDEP11*		as_provider_dep11_construct (GType object_type);

G_END_DECLS

#endif /* __AS_DATAPROVIDERDEP11_H */
