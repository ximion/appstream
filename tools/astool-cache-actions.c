/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
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

#include "astool-cache-actions.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include "astool-utils.h"
#include "as-cache-builder.h"

/**
 * astool_refresh_cache:
 */
int
astool_refresh_cache (const gchar *dbpath, const gchar *datapath, gboolean forced)
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
 * astool_database_new_path:
 */
static AsDatabase*
astool_database_new_path (const gchar *dbpath)
{
	AsDatabase *db;
	db = as_database_new ();
	if (dbpath != NULL)
		as_database_set_database_path (db, dbpath);
	return db;
}

/**
 * astool_get_component:
 *
 * Get component by id
 */
int
astool_get_component (const gchar *dbpath, const gchar *identifier, gboolean detailed)
{
	AsDatabase *db;
	AsComponent *cpt;
	gint exit_code = 0;

	if (identifier == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify a component-id."));
		return 2;
	}

	db = astool_database_new_path (dbpath);
	as_database_open (db);

	cpt = as_database_get_component_by_id (db, identifier);
	if (cpt == NULL) {
		astool_print_stderr (_("Unable to find component with id '%s'!"), identifier);
		exit_code = 4;
		goto out;
	}
	astool_print_component (cpt, detailed);

out:
	g_object_unref (db);
	if (cpt != NULL)
		g_object_unref (cpt);

	return exit_code;
}

/**
 * astool_search_component:
 */
int
astool_search_component (const gchar *dbpath, const gchar *search_term, gboolean detailed)
{
	AsDatabase *db;
	GPtrArray* cpt_list = NULL;
	guint i;
	gint exit_code = 0;

	db = astool_database_new_path (dbpath);

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
		astool_print_stderr (_("Unable to find component matching %s!"), search_term);
		exit_code = 4;
		goto out;
	}

	if (cpt_list->len == 0) {
		astool_print_stdout (_("No component matching '%s' found."), search_term);
		g_ptr_array_unref (cpt_list);
		goto out;
	}

	for (i = 0; i < cpt_list->len; i++) {
		AsComponent *cpt;
		cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

		astool_print_component (cpt, detailed);

		astool_print_separator ();
	}

out:
	if (cpt_list != NULL)
		g_ptr_array_unref (cpt_list);
	g_object_unref (db);

	return exit_code;
}

/**
 * astool_what_provides:
 *
 * Get component providing an item
 */
int
astool_what_provides (const gchar *dbpath, const gchar *kind_str, const gchar *value, const gchar *data, gboolean detailed)
{
	AsDatabase *db = NULL;
	GPtrArray* cpt_list = NULL;
	AsProvidesKind kind;
	guint i;
	gint exit_code = 0;

	if (value == NULL) {
		g_printerr ("%s\n", _("No value for the provides-item to search for defined."));
		exit_code = 1;
		goto out;
	}

	kind = as_provides_kind_from_string (kind_str);
	if (kind == AS_PROVIDES_KIND_UNKNOWN) {
		uint i;
		g_printerr ("%s\n", _("Invalid type for provides-item selected. Valid values are:"));
		for (i = 1; i < AS_PROVIDES_KIND_LAST; i++)
			fprintf (stdout, " * %s\n", as_provides_kind_to_string (i));
		exit_code = 5;
		goto out;
	}

	db = astool_database_new_path (dbpath);
	as_database_open (db);

	cpt_list = as_database_get_components_by_provides (db, kind, value, data);
	if (cpt_list == NULL) {
		astool_print_stderr (_("Unable to find component providing '%s:%s:%s'!"), kind_str, value, data);
		exit_code = 4;
		goto out;
	}

	if (cpt_list->len == 0) {
		astool_print_stdout (_("No component providing '%s:%s:%s' found."), kind_str, value, data);
		goto out;
	}

	for (i = 0; i < cpt_list->len; i++) {
		AsComponent *cpt;
		cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

		astool_print_component (cpt, detailed);
		astool_print_separator ();
	}

out:
	if (db != NULL)
		g_object_unref (db);
	if (cpt_list != NULL)
		g_ptr_array_unref (cpt_list);

	return exit_code;
}

/**
 * astool_dump_component:
 *
 * Dump the raw upstream XML for a component.
 */
int
astool_dump_component (const gchar *dbpath, const gchar *identifier)
{
	AsDatabase *db;
	AsComponent *cpt;
	AsMetadata *metad;
	gchar *tmp;
	gint exit_code = 0;

	if (identifier == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify a component-id."));
		return 2;
	}

	db = astool_database_new_path (dbpath);
	as_database_open (db);

	cpt = as_database_get_component_by_id (db, identifier);
	if (cpt == NULL) {
		astool_print_stderr (_("Unable to find component with id '%s'!"), identifier);
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
	g_object_unref (db);
	if (cpt != NULL)
		g_object_unref (cpt);

	return exit_code;
}
