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

#ifndef __AS_DATAPROVIDERAPPINSTALL_H
#define __AS_DATAPROVIDERAPPINSTALL_H

#include <glib-object.h>
#include "appstream_internal.h"
#include "../as-data-provider.h"

#define AS_PROVIDER_TYPE_UBUNTU_APPINSTALL (as_provider_ubuntu_appinstall_get_type ())
#define AS_PROVIDER_UBUNTU_APPINSTALL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS_PROVIDER_TYPE_UBUNTU_APPINSTALL, AsProviderUbuntuAppinstall))
#define AS_PROVIDER_UBUNTU_APPINSTALL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AS_PROVIDER_TYPE_UBUNTU_APPINSTALL, AsProviderUbuntuAppinstallClass))
#define AS_PROVIDER_IS_UBUNTU_APPINSTALL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS_PROVIDER_TYPE_UBUNTU_APPINSTALL))
#define AS_PROVIDER_IS_UBUNTU_APPINSTALL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AS_PROVIDER_TYPE_UBUNTU_APPINSTALL))
#define AS_PROVIDER_UBUNTU_APPINSTALL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), AS_PROVIDER_TYPE_UBUNTU_APPINSTALL, AsProviderUbuntuAppinstallClass))

G_BEGIN_DECLS

typedef struct _AsProviderUbuntuAppinstall AsProviderUbuntuAppinstall;
typedef struct _AsProviderUbuntuAppinstallClass AsProviderUbuntuAppinstallClass;
typedef struct _AsProviderUbuntuAppinstallPrivate AsProviderUbuntuAppinstallPrivate;

struct _AsProviderUbuntuAppinstall {
	AsDataProvider parent_instance;
	AsProviderUbuntuAppinstallPrivate * priv;
};

struct _AsProviderUbuntuAppinstallClass {
	AsDataProviderClass parent_class;
};

AsProviderUbuntuAppinstall*				as_provider_ubuntu_appinstall_new (void);
AsProviderUbuntuAppinstall*				as_provider_ubuntu_appinstall_construct (GType object_type);

G_END_DECLS

#endif /* __AS_DATAPROVIDERAPPINSTALL_H */
