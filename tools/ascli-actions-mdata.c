/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ascli-actions-mdata.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <glib/gstdio.h>

#include "ascli-utils.h"
#include "as-utils-private.h"
#include "as-pool-private.h"

/**
 * ascli_refresh_cache:
 */
int
ascli_refresh_cache (const gchar *cachepath,
		     const gchar *datapath,
		     const gchar *const *sources_str,
		     gboolean forced)
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(GError) error = NULL;
	gboolean cache_updated;
	gboolean ret = FALSE;

	if (!as_utils_is_root ())
		/* TRANSLATORS: In ascli: Status information during a "refresh" action. */
		g_print ("• %s\n",
			 _("Only refreshing metadata cache specific to the current user."));

	pool = as_pool_new ();
	if (sources_str != NULL) {
		as_pool_set_load_std_data_locations (pool, FALSE);

		for (guint i = 0; sources_str[i] != NULL; i++) {
			if (g_strcmp0 (sources_str[i], "os") == 0) {
				as_pool_add_flags (pool,
						   AS_POOL_FLAG_LOAD_OS_CATALOG |
						       AS_POOL_FLAG_LOAD_OS_METAINFO |
						       AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES);
				g_print ("• %s\n",
					 _("Updating software metadata cache for the operating system."));
			} else if (g_strcmp0 (sources_str[i], "flatpak") == 0) {
				as_pool_add_flags (pool, AS_POOL_FLAG_LOAD_FLATPAK);
				g_print ("• %s\n",
					 _("Updating software metadata cache for Flatpak."));
			} else {
				ascli_print_stderr (_("A metadata source group with the name '%s' does not exist!"),
				    sources_str[i]);
				return 1;
			}
		}
	}

	if (datapath != NULL) {
		/* we auto-disable loading data from sources that are not in datapath for now */
		as_pool_set_load_std_data_locations (pool, FALSE);

		/* the user wants data from a different path to be used */
		as_pool_add_extra_data_location (pool, datapath, AS_FORMAT_STYLE_CATALOG);
	}

	if (cachepath == NULL) {
		ret = as_pool_refresh_system_cache (pool, forced, &cache_updated, &error);
	} else {
		as_pool_override_cache_locations (pool, cachepath, NULL);
		ret = as_pool_load (pool, NULL, &error);
		cache_updated = TRUE;
	}

	if (!ret) {
		if (g_error_matches (error, AS_POOL_ERROR, AS_POOL_ERROR_CACHE_WRITE_FAILED))
			/* TRANSLATORS: In ascli: The requested action needs higher permissions. */
			g_printerr (
			    "✘ %s\n  %s\n",
			    error->message,
			    _("You might need superuser permissions to perform this action."));
		else
			g_printerr ("✘ %s\n", error->message);
		return 2;
	}

	if (cache_updated) {
		/* TRANSLATORS: Updating the metadata cache succeeded */
		g_print ("✔ %s\n", _("Metadata cache was updated successfully."));
	} else {
		/* TRANSLATORS: Metadata cache was not updated, likely because it was recent enough */
		g_print ("✔ %s\n", _("Metadata cache update is not necessary."));
	}

	return 0;
}

/**
 * ascli_get_component:
 *
 * Get component by id
 */
int
ascli_get_component (const gchar *cachepath,
		     const gchar *identifier,
		     gboolean detailed,
		     gboolean no_cache)
{
	g_autoptr(AsPool) dpool = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsComponentBox) result = NULL;

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

	result = as_pool_get_components_by_id (dpool, identifier);
	if (as_component_box_is_empty (result)) {
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
ascli_search_component (const gchar *cachepath,
			const gchar *search_term,
			gboolean detailed,
			gboolean no_cache)
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(AsComponentBox) result = NULL;
	g_autoptr(GError) error = NULL;

	if (search_term == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify a term to search for."));
		return 2;
	}

	pool = ascli_data_pool_new_and_open (cachepath, no_cache, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	result = as_pool_search (pool, search_term);
	if (as_component_box_is_empty (result)) {
		/* TRANSLATORS: We got no full-text search results */
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
ascli_what_provides (const gchar *cachepath,
		     const gchar *kind_str,
		     const gchar *item,
		     gboolean detailed)
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(AsComponentBox) result = NULL;
	AsProvidedKind kind;
	g_autoptr(GError) error = NULL;

	if (item == NULL) {
		g_printerr ("%s\n", _("No value for the item to search for was defined."));
		return 1;
	}

	kind = as_provided_kind_from_string (kind_str);
	if (kind == AS_PROVIDED_KIND_UNKNOWN) {
		g_printerr ("%s\n",
			    _("Invalid type for provided item selected. Valid values are:"));
		for (guint i = 1; i < AS_PROVIDED_KIND_LAST; i++)
			g_printerr (" • %s\n", as_provided_kind_to_string (i));
		return 3;
	}

	pool = ascli_data_pool_new_and_open (cachepath, FALSE, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	result = as_pool_get_components_by_provided_item (pool, kind, item);
	if (as_component_box_len (result) == 0) {
		/* TRANSLATORS: Search for provided items (e.g. mimetypes, modaliases, ..) yielded no results */
		ascli_print_stdout (
		    _("Could not find component providing '%s::%s'."), kind_str, item);
		return 0;
	}

	/* show results */
	ascli_print_components (result, detailed);

	return 0;
}

/**
 * ascli_list_categories:
 *
 * List components that match the selected categories.
 */
int
ascli_list_categories (const gchar *cachepath,
		       gchar **categories,
		       gboolean detailed,
		       gboolean no_cache)
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(AsComponentBox) result = NULL;
	g_autoptr(GError) error = NULL;

	if (categories == NULL || categories[0] == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify categories to list."));
		return 2;
	}

	pool = ascli_data_pool_new_and_open (cachepath, FALSE, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	result = as_pool_get_components_by_categories (pool, categories);
	if (as_component_box_is_empty (result)) {
		g_autofree gchar *cats_str = g_strjoinv (", ", categories);
		/* TRANSLATORS: We got no category list results */
		ascli_print_stdout (_("No component found in categories %s."), cats_str);
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
ascli_dump_component (const gchar *cachepath,
		      const gchar *identifier,
		      AsFormatKind mformat,
		      gboolean no_cache)
{
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(AsComponentBox) result = NULL;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;

	if (identifier == NULL) {
		/* TRANSLATORS: ascli was told to find a software component by its ID, but no component-id was specified. */
		fprintf (stderr, "%s\n", _("You need to specify a component-ID."));
		return 2;
	}

	pool = ascli_data_pool_new_and_open (cachepath, no_cache, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	result = as_pool_get_components_by_id (pool, identifier);
	if (as_component_box_is_empty (result)) {
		ascli_print_stderr (_("Unable to find component with ID '%s'!"), identifier);
		return 4;
	}

	/* default to XML if we don't know the format */
	if (mformat == AS_FORMAT_KIND_UNKNOWN)
		mformat = AS_FORMAT_KIND_XML;

	/* convert the obtained component to XML */
	metad = as_metadata_new ();
	for (guint i = 0; i < as_component_box_len (result); i++) {
		g_autofree gchar *metadata = NULL;
		AsComponent *cpt = as_component_box_index (result, i);

		as_metadata_clear_components (metad);
		as_metadata_add_component (metad, cpt);
		if (mformat == AS_FORMAT_KIND_YAML) {
			/* we allow YAML serialization just this once */
			metadata = as_metadata_components_to_catalog (metad,
								      AS_FORMAT_KIND_YAML,
								      NULL);
		} else {
			metadata = as_metadata_component_to_metainfo (metad, mformat, NULL);
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
ascli_put_metainfo (const gchar *fname, const gchar *origin, gboolean for_user)
{
	AsMetadataLocation location;
	const gchar *dest_dir;
	g_autoptr(GError) error = NULL;

	if (fname == NULL) {
		ascli_print_stderr (_("You need to specify a metadata file."));
		return 2;
	}

	/* determine our root directory */
	dest_dir = g_getenv ("DESTDIR");

	location = AS_METADATA_LOCATION_CACHE;
	if (g_str_has_suffix (fname, ".metainfo.xml") || g_str_has_suffix (fname, ".appdata.xml"))
		location = AS_METADATA_LOCATION_SHARED;
	if (for_user)
		location = AS_METADATA_LOCATION_USER;

	if (!as_utils_install_metadata_file (location, fname, origin, dest_dir, &error)) {
		ascli_print_stderr (_("Unable to install metadata file: %s"), error->message);
		return 3;
	}

	return 0;
}

/**
 * ascli_convert_data:
 *
 * Convert data from YAML to XML and vice versa.
 */
int
ascli_convert_data (const gchar *in_fname, const gchar *out_fname, AsFormatKind mformat)
{
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GFile) infile = NULL;
	g_autoptr(GError) error = NULL;

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
	as_metadata_set_locale (metad, "ALL");

	if ((g_str_has_suffix (in_fname, ".yml.gz")) || (g_str_has_suffix (in_fname, ".yaml.gz")) ||
	    (g_str_has_suffix (in_fname, ".yml")) || (g_str_has_suffix (in_fname, ".yaml"))) {
		/* if we have YAML, we also automatically assume a catalog style */
		as_metadata_set_format_style (metad, AS_FORMAT_STYLE_CATALOG);
	} else if (g_str_has_suffix (in_fname, ".metainfo.xml") ||
		   g_str_has_suffix (in_fname, ".appdata.xml")) {
		as_metadata_set_format_style (metad, AS_FORMAT_STYLE_METAINFO);
	} else {
		as_metadata_set_format_style (metad, AS_FORMAT_STYLE_CATALOG);
	}

	as_metadata_parse_file (metad, infile, AS_FORMAT_KIND_UNKNOWN, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	/* since YAML files are always catalog-YAMLs, we will always run in catalog mode */
	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_CATALOG);

	if (mformat == AS_FORMAT_KIND_UNKNOWN) {
		if (g_str_has_suffix (in_fname, ".xml") || g_str_has_suffix (in_fname, ".xml.gz"))
			mformat = AS_FORMAT_KIND_YAML;
		else if (g_str_has_suffix (in_fname, ".yml") ||
			 g_str_has_suffix (in_fname, ".yml.gz") ||
			 g_str_has_suffix (in_fname, ".yaml") ||
			 g_str_has_suffix (in_fname, ".yaml.gz"))
			mformat = AS_FORMAT_KIND_XML;

		if (mformat == AS_FORMAT_KIND_UNKNOWN) {
			/* TRANSLATORS: User is trying to convert a file in ascli */
			ascli_print_stderr (
			    _("Unable to convert file: Could not determine output format, please set it explicitly using '--format='."));
			return 3;
		}
	}

	if (g_strcmp0 (out_fname, "-") == 0) {
		g_autofree gchar *data = NULL;

		/* print to stdout */
		data = as_metadata_components_to_catalog (metad, mformat, &error);
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			return 1;
		}
		g_print ("%s\n", data);
	} else {
		/* save to file */

		as_metadata_save_catalog (metad, out_fname, mformat, &error);
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			return 1;
		}
	}

	return 0;
}

/**
 * ascli_create_metainfo_template:
 *
 * Create a metainfo file template to be filed out by the user.
 */
int
ascli_create_metainfo_template (const gchar *out_fname,
				const gchar *cpt_kind_str,
				const gchar *desktop_file)
{
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(GError) error = NULL;
	AsComponentKind cpt_kind = AS_COMPONENT_KIND_UNKNOWN;

	/* check if we have a component-kind set */
	cpt_kind = as_component_kind_from_string (cpt_kind_str);
	if (cpt_kind == AS_COMPONENT_KIND_UNKNOWN) {
		if (cpt_kind_str == NULL) {
			/* TRANSLATORS: The user tried to create a new template, but supplied a wrong component-type string */
			ascli_print_stderr (
			    _("You need to give an AppStream software component type to generate a template. Possible values are:"));
		} else {
			/* TRANSLATORS: The user tried to create a new template, but supplied a wrong component-type string */
			ascli_print_stderr (_("The software component type '%s' is not valid in AppStream. Possible values are:"),
					       cpt_kind_str);
		}
		for (guint i = 1; i < AS_COMPONENT_KIND_LAST; i++)
			ascli_print_stderr (" • %s", as_component_kind_to_string (i));
		return 3;
	}

	/* new metadata parser, limited to one locale */
	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "C");

	if (desktop_file == NULL) {
		cpt = as_component_new ();
		as_metadata_add_component (metad, cpt);
	} else {
		g_autoptr(GFile) defile = NULL;

		defile = g_file_new_for_path (desktop_file);
		if (!g_file_query_exists (defile, NULL)) {
			ascli_print_stderr (_("The .desktop file '%s' does not exist."),
					       desktop_file);
			return 4;
		}

		as_metadata_parse_file (metad, defile, AS_FORMAT_KIND_DESKTOP_ENTRY, &error);
		if (error != NULL) {
			ascli_print_stderr (_("Unable to read the .desktop file: %s"),
					       error->message);
			return 1;
		}

		cpt = g_object_ref (as_metadata_get_component (metad));
	}
	as_component_set_context_locale (cpt, "C");

	as_component_set_kind (cpt, cpt_kind);
	if (cpt_kind == AS_COMPONENT_KIND_FONT) {
		as_component_set_id (cpt, "org.example.FontPackageName");
	} else if (cpt_kind == AS_COMPONENT_KIND_ADDON) {
		as_component_set_id (cpt, "org.example.FooBar.my-addon");
	} else {
		if (as_component_get_id (cpt) == NULL)
			as_component_set_id (cpt, "org.example.SoftwareName");
	}

	if (as_component_get_name (cpt) == NULL)
		as_component_set_name (cpt, "The human-readable name of this software", "C");

	if (as_component_get_summary (cpt) == NULL)
		as_component_set_summary (cpt,
					  "A short summary describing what this software is about",
					  "C");

	if (as_component_get_description (cpt) == NULL)
		as_component_set_description (cpt,
					      "<p>Multiple paragraphs of long description, "
					      "describing this software component.</p>"
					      "<p>You can also use ordered and unordered lists:</p>"
					      "<ul><li>Feature 1</li><li>Feature 2</li></ul>"
					      "<p>Keep in mind to XML-escape characters, and that "
					      "this is not HTML markup.</p>",
					      "C");

	as_component_set_metadata_license (
	    cpt,
	    "A permissive license for this metadata, e.g. \"FSFAP\"");
	as_component_set_project_license (
	    cpt,
	    "The license of this software as SPDX string, e.g. \"GPL-3+\"");

	as_developer_set_name (as_component_get_developer (cpt),
			       "The software vendor name, e.g. \"ACME Corporation\"",
			       "C");

	/* console-app specific */
	if (cpt_kind == AS_COMPONENT_KIND_CONSOLE_APP) {
		g_autoptr(AsProvided) prov = as_provided_new ();
		as_provided_set_kind (prov, AS_PROVIDED_KIND_BINARY);
		as_provided_add_item (prov, "The binary name of this software in PATH");
		as_component_add_provided (cpt, prov);
	}

	/* addon specific */
	if (cpt_kind == AS_COMPONENT_KIND_ADDON)
		as_component_add_extends (cpt,
					  "The component-id of the software that is extended by "
					  "this addon, e.g. \"org.example.FooBar\"");

	/* font specific */
	if (cpt_kind == AS_COMPONENT_KIND_FONT) {
		g_autoptr(AsProvided) prov = as_provided_new ();

		as_provided_set_kind (prov, AS_PROVIDED_KIND_FONT);
		as_provided_add_item (prov,
				      "A full font name, consisting of the fonts family and style, "
				      "e.g. \"Lato Heavy Italic\"");
		as_provided_add_item (prov, "Liberation Serif Bold Italic");
		as_component_add_provided (cpt, prov);
	}

	/* driver specific */
	if (cpt_kind == AS_COMPONENT_KIND_DRIVER) {
		g_autoptr(AsProvided) prov = as_provided_new ();
		as_provided_set_kind (prov, AS_PROVIDED_KIND_MODALIAS);
		as_provided_add_item (prov, "Modalias of the hardware this software handles");
		as_component_add_provided (cpt, prov);
	}

	/* print to console or save to file */
	if ((out_fname == NULL) || (g_strcmp0 (out_fname, "-") == 0)) {
		g_autofree gchar *metainfo_xml = NULL;

		metainfo_xml = as_metadata_component_to_metainfo (metad,
								  AS_FORMAT_KIND_XML,
								  &error);
		if (error != NULL) {
			ascli_print_stderr (_("Unable to build the template metainfo file: %s"),
					       error->message);
			return 1;
		}

		g_print ("%s\n", metainfo_xml);
	} else {
		as_metadata_save_metainfo (metad, out_fname, AS_FORMAT_KIND_XML, &error);
		if (error != NULL) {
			ascli_print_stderr (_("Unable to save the template metainfo file: %s"),
					       error->message);
			return 1;
		}
	}

	return 0;
}

/**
 * ascli_print_satisfy_check_results:
 *
 * Helper for %ascli_check_is_satisfied
 */
static gboolean
ascli_print_satisfy_check_results (GPtrArray *rc_results, AsSystemInfo *sysinfo)
{
	gboolean res = TRUE;
	const gchar *fail_char;

	for (guint i = 0; i < rc_results->len; i++) {
		AsRelationCheckResult *rcr = AS_RELATION_CHECK_RESULT (
		    g_ptr_array_index (rc_results, i));
		AsRelation *relation;
		AsRelationStatus status;

		status = as_relation_check_result_get_status (rcr);
		relation = as_relation_check_result_get_relation (rcr);

		if (as_relation_get_kind (relation) == AS_RELATION_KIND_SUPPORTS)
			fail_char = "•";
		else
			fail_char = ASCLI_CHAR_FAIL;

		if (status == AS_RELATION_STATUS_ERROR) {
			if (as_relation_get_item_kind (relation) ==
				AS_RELATION_ITEM_KIND_DISPLAY_LENGTH &&
			    as_system_info_get_display_length (sysinfo,
							       AS_DISPLAY_SIDE_KIND_LONGEST) == 0) {
				g_print (" • %s\n",
					 _("Unable to check display size: Can not read information without GUI toolkit access."));
			} else if (as_relation_check_result_get_error_code (rcr) ==
				   AS_RELATION_ERROR_NOT_IMPLEMENTED) {
				g_print (" • %s\n", as_relation_check_result_get_message (rcr));
				res = FALSE;
			} else {
				g_print (" %s %s: %s\n",
					 fail_char,
					 _("ERROR"), as_relation_check_result_get_message (rcr));
				res = FALSE;
			}
		} else {
			const gchar *message = as_relation_check_result_get_message (rcr);

			if (as_relation_check_result_get_status (rcr) ==
			    AS_RELATION_STATUS_SATISFIED) {
				g_print (" %s %s\n", ASCLI_CHAR_SUCCESS, message);
			} else {
				g_print (" %s %s\n", fail_char, message);
				res = FALSE;
			}
		}
	}

	return res;
}

/**
 * ascli_check_is_satisfied:
 *
 * Verify if the selected component is satisfied on the current system.
 */
gint
ascli_check_is_satisfied (const gchar *fname_or_cid, const gchar *cachepath, gboolean no_cache)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(AsSystemInfo) sysinfo = NULL;
	g_autoptr(GPtrArray) requires_rcr = NULL;
	g_autoptr(GPtrArray) recommends_rcr = NULL;
	g_autoptr(GPtrArray) supports_rcr = NULL;
	gboolean res = TRUE;

	if (fname_or_cid == NULL) {
		ascli_print_stderr (_("You need to specify a MetaInfo filename or component ID."));
		return 2;
	}

	/* open the metadata pool with default options */
	pool = ascli_data_pool_new_and_open (cachepath, no_cache, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return ASCLI_EXIT_CODE_FAILED;
	}

	/* get the current component, either from file or from the pool */
	if (g_strstr_len (fname_or_cid, -1, "/") != NULL ||
	    g_file_test (fname_or_cid, G_FILE_TEST_EXISTS)) {
		g_autoptr(GFile) mi_file = NULL;
		g_autoptr(AsMetadata) mdata = NULL;

		mi_file = g_file_new_for_path (fname_or_cid);
		if (!g_file_query_exists (mi_file, NULL)) {
			ascli_print_stderr (_("Metainfo file '%s' does not exist."), fname_or_cid);
			return 4;
		}

		/* read the metainfo file */
		mdata = as_metadata_new ();
		as_metadata_set_locale (mdata, "ALL");

		as_metadata_parse_file (mdata, mi_file, AS_FORMAT_KIND_XML, &error);
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			return 1;
		}

		cpt = g_object_ref (as_metadata_get_component (mdata));
	} else {
		g_autoptr(AsComponentBox) cbox = NULL;

		cbox = as_pool_get_components_by_id (pool, fname_or_cid);
		if (as_component_box_is_empty (cbox)) {
			ascli_print_stderr (_("Unable to find component with ID '%s'!"),
					       fname_or_cid);
			return ASCLI_EXIT_CODE_NO_RESULT;
		}

		cpt = g_object_ref (as_component_box_index (cbox, 0));
	}

	/* TRANSLATORS: We are testing the relations (requires, recommends & supports data) for being satisfied on the current system. */
	ascli_print_stdout (_("Relation check for: %s"), as_component_get_data_id (cpt));
	g_print ("\n");

	sysinfo = as_system_info_new ();
	requires_rcr = as_component_check_relations (cpt, sysinfo, pool, AS_RELATION_KIND_REQUIRES);
	recommends_rcr = as_component_check_relations (cpt,
						       sysinfo,
						       pool,
						       AS_RELATION_KIND_RECOMMENDS);
	supports_rcr = as_component_check_relations (cpt, sysinfo, pool, AS_RELATION_KIND_SUPPORTS);

	ascli_print_highlight (_("Requirements:"));
	if (requires_rcr->len == 0) {
		g_print (" • %s\n", _("No required items are set for this software."));
	} else {
		res = ascli_print_satisfy_check_results (requires_rcr, sysinfo) ? res : FALSE;
	}

	ascli_print_highlight (_("Recommendations:"));
	if (recommends_rcr->len == 0) {
		g_print (" • %s\n", _("No recommended items are set for this software."));
	} else {
		res = ascli_print_satisfy_check_results (recommends_rcr, sysinfo) ? res : FALSE;
	}

	ascli_print_highlight (_("Supported:"));
	if (supports_rcr->len == 0) {
		g_print (" • %s\n", _("No supported items are set for this software."));
	} else {
		ascli_print_satisfy_check_results (supports_rcr, sysinfo);
	}

	return res ? ASCLI_EXIT_CODE_SUCCESS : ASCLI_EXIT_CODE_FAILED;
}

/**
 * as_chassis_kind_to_header:
 * @kind: the #AsChassisKind.
 *
 * Converts the enumerated value to a human-readble
 * and translated text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 1.0.0
 **/
static const gchar *
as_chassis_kind_to_header (AsChassisKind kind)
{
	if (kind == AS_CHASSIS_KIND_DESKTOP)
		/* TRANSLATORS: This represents a computer chassis type. */
		return _("Desktop");
	if (kind == AS_CHASSIS_KIND_LAPTOP)
		/* TRANSLATORS: This represents a computer chassis type. */
		return _("Laptop");
	if (kind == AS_CHASSIS_KIND_SERVER)
		/* TRANSLATORS: This represents a computer chassis type. */
		return _("Server");
	if (kind == AS_CHASSIS_KIND_TABLET)
		/* TRANSLATORS: This represents a computer chassis type. */
		return _("Tablet");
	if (kind == AS_CHASSIS_KIND_HANDSET)
		/* TRANSLATORS: This represents a computer chassis type. */
		return _("Handset");

	return _("Unknown");
}

/**
 * ascli_check_syscompat:
 *
 * Verify if the selected component is satisfied on a bunch of mock configurations.
 */
gint
ascli_check_syscompat (const gchar *fname_or_cid,
		       const gchar *cachepath,
		       gboolean no_cache,
		       gboolean show_details)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(GError) error = NULL;

	if (fname_or_cid == NULL) {
		ascli_print_stderr (_("You need to specify a MetaInfo filename or component ID."));
		return 2;
	}

	/* get the current component, either from file or from the pool */
	if (g_strstr_len (fname_or_cid, -1, "/") != NULL ||
	    g_file_test (fname_or_cid, G_FILE_TEST_EXISTS)) {
		g_autoptr(GFile) mi_file = NULL;
		g_autoptr(AsMetadata) mdata = NULL;

		mi_file = g_file_new_for_path (fname_or_cid);
		if (!g_file_query_exists (mi_file, NULL)) {
			ascli_print_stderr (_("Metainfo file '%s' does not exist."), fname_or_cid);
			return 4;
		}

		/* read the metainfo file */
		mdata = as_metadata_new ();
		as_metadata_set_locale (mdata, "ALL");

		as_metadata_parse_file (mdata, mi_file, AS_FORMAT_KIND_XML, &error);
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			return 1;
		}

		cpt = g_object_ref (as_metadata_get_component (mdata));
	} else {
		g_autoptr(AsPool) pool = NULL;
		g_autoptr(AsComponentBox) cbox = NULL;

		/* open the metadata pool with default options */
		pool = ascli_data_pool_new_and_open (cachepath, no_cache, &error);
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			return ASCLI_EXIT_CODE_FAILED;
		}

		cbox = as_pool_get_components_by_id (pool, fname_or_cid);
		if (as_component_box_is_empty (cbox)) {
			ascli_print_stderr (_("Unable to find component with ID '%s'!"),
					       fname_or_cid);
			return ASCLI_EXIT_CODE_NO_RESULT;
		}

		cpt = g_object_ref (as_component_box_index (cbox, 0));
	}

	/* TRANSLATORS: We are testing compatibility of a component with a common representation of hardware for a specific chassis (phone/tablet/desktop, etc.) */
	ascli_print_stdout (_("Chassis compatibility check for: %s"),
			       as_component_get_data_id (cpt));

	for (guint chassis = AS_CHASSIS_KIND_DESKTOP; chassis < AS_CHASSIS_KIND_LAST; chassis++) {
		g_autoptr(AsSystemInfo) sysinfo = NULL;
		g_autoptr(GPtrArray) rc_results = NULL;
		gint score;

		sysinfo = as_system_info_new_template_for_chassis (chassis, NULL);
		if (sysinfo == NULL)
			continue;
		g_print ("\n");
		ascli_print_highlight ("%s:", as_chassis_kind_to_header (chassis));

		score = as_component_get_system_compatibility_score (cpt,
								     sysinfo,
								     TRUE,
								     &rc_results);
		if (score < 50) {
			g_print (" %s %s\n", ASCLI_CHAR_FAIL, _("Incompatible"));

			if (!show_details)
				continue;

			for (guint i = 0; i < rc_results->len; i++) {
				AsRelationCheckResult *rcr = AS_RELATION_CHECK_RESULT (
				    g_ptr_array_index (rc_results, i));
				AsRelation *relation;
				AsRelationStatus status;
				AsRelationKind rel_kind;
				g_autofree gchar *prefix = NULL;

				status = as_relation_check_result_get_status (rcr);
				relation = as_relation_check_result_get_relation (rcr);
				rel_kind = as_relation_get_kind (relation);

				if (status == AS_RELATION_STATUS_ERROR) {
					g_print (" %s %s: %s\n",
						 "•",
						 _("ERROR"),
						    as_relation_check_result_get_message (rcr));
					continue;
				}

				/* ignore any successes */
				if (status == AS_RELATION_STATUS_SATISFIED)
					continue;

				g_print (" %s %s: %s\n",
					 "•",
					 as_relation_kind_to_string (rel_kind),
					 as_relation_check_result_get_message (rcr));
			}
		} else {
			g_print (" %s %s (%d%%)\n", ASCLI_CHAR_SUCCESS, _("Compatible"), score);
		}
	}

	return ASCLI_EXIT_CODE_SUCCESS;
}
