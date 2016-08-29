/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "ascli-actions-mdata.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <glib/gstdio.h>

#include "ascli-utils.h"
#include "as-utils-private.h"

/**
 * ascli_refresh_cache:
 */
int
ascli_refresh_cache (const gchar *cachepath, const gchar *datapath, gboolean forced)
{
	g_autoptr(AsPool) dpool = NULL;
	g_autoptr(GError) error = NULL;
	gboolean ret;

	dpool = as_pool_new ();
	if (datapath != NULL) {
		/* the user wants data from a different path to be used */
		as_pool_clear_metadata_locations (dpool);
		as_pool_add_metadata_location (dpool, datapath);

		/* disable loading data from cache */
		as_pool_set_cache_flags (dpool, AS_CACHE_FLAG_NONE);
	}

	if (cachepath == NULL) {
		ret = as_pool_refresh_cache (dpool, forced, &error);
	} else {
		as_pool_load (dpool, NULL, &error);
		if (error == NULL)
			as_pool_save_cache_file (dpool, cachepath, &error);
	}

	if (error != NULL) {
		if (g_error_matches (error, AS_POOL_ERROR, AS_POOL_ERROR_TARGET_NOT_WRITABLE))
			/* TRANSLATORS: In ascli: The requested action needs higher permissions. */
			g_printerr ("%s\n%s\n", error->message, _("You might need superuser permissions to perform this action."));
		else
			g_printerr ("%s\n", error->message);
		return 2;
	}

	if (ret) {
		/* we performed a cache refresh, check if we had errors */
		if (error == NULL) {
			/* TRANSLATORS: Updating the metadata cache succeeded */
			g_print ("%s\n", _("AppStream cache update completed successfully."));
		} else {
			g_printerr ("%s\n", error->message);
		}

		/* no > 0 error code, since we updated something */
		return 0;
	} else {
		/* cache wasn't updated, so the update wasn't necessary, or we have a fatal error */
		if (error == NULL) {
			g_print ("%s\n", _("AppStream cache update is not necessary."));
			return 0;
		} else {
			g_printerr ("%s\n", error->message);
			return 6;
		}
	}
}

/**
 * ascli_data_pool_new_and_open:
 */
static AsPool*
ascli_data_pool_new_and_open (const gchar *cachepath, gboolean no_cache, GError **error)
{
	AsPool *dpool;

	dpool = as_pool_new ();
	if (cachepath == NULL) {
		/* no cache object to load, we can use a normal pool - unless caching
		 * is generally disallowed. */
		if (no_cache)
			as_pool_set_cache_flags (dpool, AS_CACHE_FLAG_NONE);

		as_pool_load (dpool, NULL, error);
	} else {
		/* use an exported cache object */
		as_pool_load_cache_file (dpool, cachepath, error);
	}

	return dpool;
}

/**
 * ascli_get_component:
 *
 * Get component by id
 */
int
ascli_get_component (const gchar *cachepath, const gchar *identifier, gboolean detailed, gboolean no_cache)
{
	g_autoptr(AsPool) dpool = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) result = NULL;

	if (identifier == NULL) {
		/* TRANSLATORS: An AppStream component-id is missing in the command-line arguments */
		fprintf (stderr, "%s\n", _("You need to specify a component-ID."));
		return 2;
	}

	dpool = ascli_data_pool_new_and_open (cachepath, no_cache, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	result = as_pool_get_component_by_id (dpool, identifier);
	if (result->len == 0) {
		ascli_print_stderr (_("Unable to find component with ID '%s'!"), identifier);
		return 4;
	}

	ascli_print_components (result, detailed);

	return 0;
}

/**
 * ascli_search_component:
 */
int
ascli_search_component (const gchar *cachepath, const gchar *search_term, gboolean detailed, gboolean no_cache)
{
	g_autoptr(AsPool) dpool = NULL;
	g_autoptr(GPtrArray) result = NULL;
	g_autoptr(GError) error = NULL;

	if (search_term == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify a term to search for."));
		return 2;
	}

	dpool = ascli_data_pool_new_and_open (cachepath, no_cache, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	result = as_pool_search (dpool, search_term);
	if (result->len == 0) {
		/* TRANSLATORS: We failed to find any component in the database, likely due to an error */
		ascli_print_stderr (_("Unable to find component matching %s!"), search_term);
		return 4;
	}

	if (result->len == 0) {
		ascli_print_stdout (_("No component matching '%s' found."), search_term);
		return 0;
	}

	/* show the result */
	ascli_print_components (result, detailed);

	return 0;
}

/**
 * ascli_what_provides:
 *
 * Get component providing an item
 */
int
ascli_what_provides (const gchar *cachepath, const gchar *kind_str, const gchar *item, gboolean detailed)
{
	g_autoptr(AsPool) dpool = NULL;
	g_autoptr(GPtrArray) result = NULL;
	AsProvidedKind kind;
	g_autoptr(GError) error = NULL;

	if (item == NULL) {
		g_printerr ("%s\n", _("No value for the item to search for defined."));
		return 1;
	}

	kind = as_provided_kind_from_string (kind_str);
	if (kind == AS_PROVIDED_KIND_UNKNOWN) {
		uint i;
		g_printerr ("%s\n", _("Invalid type for provided item selected. Valid values are:"));
		for (i = 1; i < AS_PROVIDED_KIND_LAST; i++)
			fprintf (stdout, " * %s\n", as_provided_kind_to_string (i));
		return 3;
	}

	dpool = ascli_data_pool_new_and_open (cachepath, FALSE, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	result = as_pool_get_components_by_provided_item (dpool, kind, item, &error);
	if (error != NULL) {
		/* TRANSLATORS: There was an error when trying to search for provided items (e.g. mimetypes) */
		ascli_print_stderr (_("Unable to find component providing '%s::%s': %s"),
				    kind_str, item, error->message);
		return 2;
	}

	if (result->len == 0) {
		/* TRANSLATORS: Search for provided items (e.g. mimetypes, modaliases, ..) yielded no results */
		ascli_print_stdout (_("No component providing '%s::%s' found."), kind_str, item);
		return 0;
	}

	/* show results */
	ascli_print_components (result, detailed);

	return 0;
}

/**
 * ascli_dump_component:
 *
 * Dump the raw upstream XML for a component.
 */
int
ascli_dump_component (const gchar *cachepath, const gchar *identifier, AsDataFormat format, gboolean no_cache)
{
	g_autoptr(AsPool) dpool = NULL;
	g_autoptr(GPtrArray) result = NULL;
	g_autoptr(AsMetadata) metad = NULL;
	guint i;
	g_autoptr(GError) error = NULL;

	if (identifier == NULL) {
		/* TRANSLATORS: ascli was told to find a software component by its ID, but no component-id was specified. */
		fprintf (stderr, "%s\n", _("You need to specify a component-ID."));
		return 2;
	}

	dpool = ascli_data_pool_new_and_open (cachepath, no_cache, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	result = as_pool_get_component_by_id (dpool, identifier);
	if (result->len == 0) {
		ascli_print_stderr (_("Unable to find component with ID '%s'!"), identifier);
		return 4;
	}

	/* default to XML if we don't know the format */
	if (format == AS_DATA_FORMAT_UNKNOWN)
		format = AS_DATA_FORMAT_XML;

	/* convert the obtained component to XML */
	metad = as_metadata_new ();
	for (i = 0; i < result->len; i++) {
		g_autofree gchar *metadata = NULL;
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (result, i));

		as_metadata_clear_components (metad);
		as_metadata_add_component (metad, cpt);
		if (format == AS_DATA_FORMAT_YAML) {
			/* we allow YAML serialization just this once */
			metadata = as_metadata_components_to_collection (metad, AS_DATA_FORMAT_YAML, NULL);
		} else {
			metadata = as_metadata_component_to_metainfo (metad, format, NULL);
		}
		g_print ("%s\n", metadata);
	}

	return 0;
}

/**
 * ascli_put_metainfo:
 *
 * Install a metainfo file.
 */
int
ascli_put_metainfo (const gchar *fname)
{
	g_autofree gchar *tmp = NULL;
	g_autofree gchar *dest = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *metainfo_target;
	const gchar *root_dir;

	if (fname == NULL) {
		ascli_print_stderr (_("You need to specify a metadata file."));
		return 2;
	}

	/* determine our root directory */
	root_dir = g_getenv ("DESTDIR");
	if (root_dir == NULL)
		metainfo_target = g_strdup ("/usr/share/metainfo");
	else
		metainfo_target = g_build_filename (root_dir, "share", "metainfo", NULL);

	g_mkdir_with_parents (metainfo_target, 0755);
	if (!as_utils_is_writable (metainfo_target)) {
		ascli_print_stderr (_("Unable to write to '%s', can not install metainfo file."), metainfo_target);
		return 1;
	}

	if ((!g_str_has_suffix (fname, ".metainfo.xml")) && (!g_str_has_suffix (fname, ".appdata.xml"))) {
		ascli_print_stderr (_("Can not copy '%s': File does not have a '.metainfo.xml' or '.appdata.xml' suffix."));
		return 2;
	}

	tmp = g_path_get_basename (fname);
	dest = g_build_filename (metainfo_target, tmp, NULL);

	as_copy_file (fname, dest, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 3;
	}
	g_chmod (dest, 0755);

	return 0;
}

/**
 * ascli_convert_data:
 *
 * Convert data from YAML to XML and vice versa.
 */
int
ascli_convert_data (const gchar *in_fname, const gchar *out_fname, AsDataFormat format)
{
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GFile) infile = NULL;
	GError *error = NULL;

	if (in_fname == NULL || out_fname == NULL) {
		ascli_print_stderr (_("You need to specify an input and output file."));
		return 3;
	}

	/* load input file */
	infile = g_file_new_for_path (in_fname);
	if (!g_file_query_exists (infile, NULL)) {
		ascli_print_stderr (_("Metadata file '%s' does not exist."), in_fname);
		return 4;
	}

	metad = as_metadata_new ();
	/* since YAML files are always collection-YAMLs, we will always run in collection mode */
	as_metadata_set_parser_mode (metad, AS_PARSER_MODE_COLLECTION);

	as_metadata_parse_file (metad,
				infile,
				AS_DATA_FORMAT_UNKNOWN,
				&error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	if (format == AS_DATA_FORMAT_UNKNOWN) {
		if (g_str_has_suffix (in_fname, ".xml") || g_str_has_suffix (in_fname, ".xml.gz"))
			format = AS_DATA_FORMAT_YAML;
		else if (g_str_has_suffix (in_fname, ".yml") || g_str_has_suffix (in_fname, ".yml.gz") ||
			 g_str_has_suffix (in_fname, ".yaml") || g_str_has_suffix (in_fname, ".yaml.gz"))
			format = AS_DATA_FORMAT_XML;

		if (format == AS_DATA_FORMAT_UNKNOWN) {
			/* TRANSLATORS: User is trying to convert a file in ascli */
			ascli_print_stderr (_("Unable to convert file: Could not determine output format, please set it explicitly using '--format='."));
			return 3;
		}
	}

	if (g_strcmp0 (out_fname, "-") == 0) {
		g_autofree gchar *data = NULL;

		/* print to stdout */
		data = as_metadata_components_to_collection (metad, format, &error);
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			return 1;
		}
		g_print ("%s\n", data);
	} else {
		/* save to file */

		as_metadata_save_collection (metad, out_fname, format, &error);
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			return 1;
		}
	}

	return 0;
}
