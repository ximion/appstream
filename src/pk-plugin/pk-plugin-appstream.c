/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2013 Matthias Klumpp <matthias@tenstral.net>
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

#include <packagekit-glib2/packagekit.h>
#include <plugin/packagekit-plugin.h>

#include "../as-cache-builder.h"

struct PkPluginPrivate {
	guint		dummy;
};

/**
 * pk_plugin_get_description:
 */
const gchar *
pk_plugin_get_description (void)
{
	return "Refreshes the Appstream database of available applications";
}

/**
 * pk_plugin_initialize:
 */
void
pk_plugin_initialize (PkPlugin *plugin)
{
	/* create private area */
	plugin->priv = PK_TRANSACTION_PLUGIN_GET_PRIVATE (PkPluginPrivate);
}

/**
 * pk_plugin_transaction_finished_end:
 */
void
pk_plugin_transaction_finished_end (PkPlugin *plugin,
				    PkTransaction *transaction)
{
	AsBuilder *builder = NULL;
	PkRoleEnum role;

	/* skip simulate actions */
	if (pk_bitfield_contain (pk_transaction_get_transaction_flags (transaction),
				 PK_TRANSACTION_FLAG_ENUM_SIMULATE)) {
		goto out;
	}

	/* skip only-download */
	if (pk_bitfield_contain (pk_transaction_get_transaction_flags (transaction),
				 PK_TRANSACTION_FLAG_ENUM_ONLY_DOWNLOAD)) {
		goto out;
	}

	/* check the role */
	role = pk_transaction_get_role (transaction);
	if (role != PK_ROLE_ENUM_REFRESH_CACHE)
		goto out;

	/* use a local backend instance */
	pk_backend_reset_job (plugin->backend, plugin->job);
	pk_backend_job_set_status (plugin->job,
				   PK_STATUS_ENUM_SCAN_APPLICATIONS);

	/* refresh the AppStream cache using the database builder */
	builder = as_builder_new ();
	as_builder_initialize (builder);
	as_builder_refresh_cache (builder, FALSE);

	pk_backend_job_set_percentage (plugin->job, 100);
	pk_backend_job_set_status (plugin->job, PK_STATUS_ENUM_FINISHED);
out:
	if (builder != NULL)
		g_object_unref (builder);
}
