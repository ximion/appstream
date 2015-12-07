/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ascli-mdata-actions.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include "ascli-utils.h"
#include "as-cache-builder.h"

/**
 * ascli_refresh_cache:
 */
int
ascli_refresh_cache (const gchar *dbpath, const gchar *datapath, gboolean forced)
{
	AsBuilder *builder;
	GError *error = NULL;
	gboolean ret;

	if (dbpath == NULL) {
		if (getuid () != ((uid_t) 0)) {
			g_print ("%s\n", _("You need to run this command with superuser permissions!"));
			return 2;
		}
	}

	if (dbpath == NULL)
		builder = as_builder_new ();
	else
		builder = as_builder_new_path (dbpath);

	if (datapath != NULL) {
		gchar **strv;
		/* the user wants data from a different path to be used */
		strv = g_new0 (gchar*, 2);
		strv[0] = g_strdup (datapath);
		as_builder_set_data_source_directories (builder, strv);
		g_strfreev (strv);
	}

	as_builder_initialize (builder);
	ret = as_builder_refresh_cache (builder, forced, &error);
	g_object_unref (builder);

	if (error == NULL) {
		g_print ("%s\n", _("AppStream cache update completed successfully."));
	} else {
		g_print ("%s\n", error->message);
		g_error_free (error);
	}

	if (ret)
		return 0;
	else
		return 6;
}

/**
 * ascli_database_new_path:
 */
static AsDatabase*
ascli_database_new_path (const gchar *dbpath)
{
	AsDatabase *db;
	db = as_database_new ();
	if (dbpath != NULL)
		as_database_set_database_path (db, dbpath);
	return db;
}

/**
 * ascli_get_component:
 *
 * Get component by id
 */
int
ascli_get_component (const gchar *dbpath, const gchar *identifier, gboolean detailed, gboolean no_cache)
{
	AsComponent *cpt = NULL;
	gint exit_code = 0;

	if (identifier == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify a component-id."));
		return 2;
	}

	if (no_cache) {
		AsDataPool *dpool;

		dpool = as_data_pool_new ();
		as_data_pool_update (dpool);
		cpt = as_data_pool_get_component_by_id (dpool, identifier);
		g_object_unref (dpool);
	} else {
		AsDatabase *db;

		db = ascli_database_new_path (dbpath);
		as_database_open (db);

		cpt = as_database_get_component_by_id (db, identifier);
		g_object_unref (db);
	}

	if (cpt == NULL) {
		ascli_print_stderr (_("Unable to find component with id '%s'!"), identifier);
		exit_code = 4;
		goto out;
	}
	ascli_print_component (cpt, detailed);

out:
	if (cpt != NULL)
		g_object_unref (cpt);

	return exit_code;
}

/**
 * ascli_search_component:
 */
int
ascli_search_component (const gchar *dbpath, const gchar *search_term, gboolean detailed)
{
	AsDatabase *db;
	GPtrArray* cpt_list = NULL;
	guint i;
	gint exit_code = 0;

	db = ascli_database_new_path (dbpath);

	if (search_term == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify a term to search for."));
		exit_code = 2;
		goto out;
	}

	/* search for stuff */
	as_database_open (db);
	cpt_list = as_database_find_components_by_term (db, search_term, NULL);
	if (cpt_list == NULL) {
		/* TRANSLATORS: We failed to find any component in the database due to an error */
		ascli_print_stderr (_("Unable to find component matching %s!"), search_term);
		exit_code = 4;
		goto out;
	}

	if (cpt_list->len == 0) {
		ascli_print_stdout (_("No component matching '%s' found."), search_term);
		g_ptr_array_unref (cpt_list);
		goto out;
	}

	for (i = 0; i < cpt_list->len; i++) {
		AsComponent *cpt;
		cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

		ascli_print_component (cpt, detailed);

		ascli_print_separator ();
	}

out:
	if (cpt_list != NULL)
		g_ptr_array_unref (cpt_list);
	g_object_unref (db);

	return exit_code;
}

/**
 * ascli_what_provides:
 *
 * Get component providing an item
 */
int
ascli_what_provides (const gchar *dbpath, const gchar *kind_str, const gchar *item, gboolean detailed)
{
	AsDatabase *db = NULL;
	GPtrArray* cpt_list = NULL;
	AsProvidedKind kind;
	guint i;
	gint exit_code = 0;

	if (item == NULL) {
		g_printerr ("%s\n", _("No value for the item to search for defined."));
		exit_code = 1;
		goto out;
	}

	kind = as_provided_kind_from_string (kind_str);
	if (kind == AS_PROVIDED_KIND_UNKNOWN) {
		uint i;
		g_printerr ("%s\n", _("Invalid type for provided item selected. Valid values are:"));
		for (i = 1; i < AS_PROVIDED_KIND_LAST; i++)
			fprintf (stdout, " * %s\n", as_provided_kind_to_string (i));
		exit_code = 5;
		goto out;
	}

	db = ascli_database_new_path (dbpath);
	as_database_open (db);

	cpt_list = as_database_get_components_by_provides (db, kind, item);
	if (cpt_list == NULL) {
		ascli_print_stderr (_("Unable to find component providing '%s;%s'!"), kind_str, item);
		exit_code = 4;
		goto out;
	}

	if (cpt_list->len == 0) {
		ascli_print_stdout (_("No component providing '%s;%s' found."), kind_str, item);
		goto out;
	}

	for (i = 0; i < cpt_list->len; i++) {
		AsComponent *cpt;
		cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

		ascli_print_component (cpt, detailed);
		ascli_print_separator ();
	}

out:
	if (db != NULL)
		g_object_unref (db);
	if (cpt_list != NULL)
		g_ptr_array_unref (cpt_list);

	return exit_code;
}

/**
 * ascli_dump_component:
 *
 * Dump the raw upstream XML for a component.
 */
int
ascli_dump_component (const gchar *dbpath, const gchar *identifier, gboolean no_cache)
{
	AsComponent *cpt = NULL;
	AsMetadata *metad;
	gchar *tmp;
	gint exit_code = 0;

	if (identifier == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify a component-id."));
		return 2;
	}

	if (no_cache) {
		AsDataPool *dpool;

		dpool = as_data_pool_new ();
		as_data_pool_update (dpool);
		cpt = as_data_pool_get_component_by_id (dpool, identifier);
		g_object_unref (dpool);
	} else {
		AsDatabase *db;

		db = ascli_database_new_path (dbpath);
		as_database_open (db);

		cpt = as_database_get_component_by_id (db, identifier);
		g_object_unref (db);
	}

	if (cpt == NULL) {
		ascli_print_stderr (_("Unable to find component with id '%s'!"), identifier);
		exit_code = 4;
		goto out;
	}

	/* convert the obtained component to XML */
	metad = as_metadata_new ();
	as_metadata_add_component (metad, cpt);
	tmp = as_metadata_component_to_upstream_xml (metad);
	g_object_unref (metad);

	g_print ("%s\n", tmp);
	g_free (tmp);

out:
	if (cpt != NULL)
		g_object_unref (cpt);

	return exit_code;
}
