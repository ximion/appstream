/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "ascli-actions-misc.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include "as-utils-private.h"
#include "as-settings-private.h"
#include "ascli-utils.h"

/**
 * ascli_show_status:
 *
 * Print various interesting status information.
 */
int
ascli_show_status (void)
{
	guint i;
	g_autoptr(AsDatabase) cache = NULL;
	const gchar *metainfo_path = "/usr/share/appdata";

	ascli_print_highlight (_("AppStream Status:"));
	ascli_print_stdout ("Version: %s", VERSION);
	g_print ("\n");

	ascli_print_highlight (_("Distribution metadata:"));
	for (i = 0; AS_APPSTREAM_METADATA_PATHS[i] != NULL; i++) {
		g_autofree gchar *xml_path = NULL;
		g_autofree gchar *yaml_path = NULL;
		g_autofree gchar *icons_path = NULL;
		gboolean found = FALSE;

		xml_path = g_build_filename (AS_APPSTREAM_METADATA_PATHS[i], "xmls", NULL);
		yaml_path = g_build_filename (AS_APPSTREAM_METADATA_PATHS[i], "yaml", NULL);
		icons_path = g_build_filename (AS_APPSTREAM_METADATA_PATHS[i], "icons", NULL);

		g_print (" %s\n", AS_APPSTREAM_METADATA_PATHS[i]);

		/* display XML data count */
		if (g_file_test (xml_path, G_FILE_TEST_IS_DIR)) {
			g_autoptr(GPtrArray) xmls = NULL;

			xmls = as_utils_find_files_matching (xml_path, "*.xml*", FALSE, NULL);
			if (xmls != NULL) {
				ascli_print_stdout ("  - XML:  %i", xmls->len);
				found = TRUE;
			}
		}

		/* display YAML data count */
		if (g_file_test (yaml_path, G_FILE_TEST_IS_DIR)) {
			g_autoptr(GPtrArray) yaml = NULL;

			yaml = as_utils_find_files_matching (yaml_path, "*.yml*", FALSE, NULL);
			if (yaml != NULL) {
				ascli_print_stdout ("  - YAML: %i", yaml->len);
				found = TRUE;
			}
		}

		/* display icon information data count */
		if (g_file_test (icons_path, G_FILE_TEST_IS_DIR)) {
			guint j;
			g_autoptr(GPtrArray) icon_dirs = NULL;
			icon_dirs = as_utils_find_files_matching (icons_path, "*", FALSE, NULL);
			if (icon_dirs != NULL) {
				found = TRUE;
				ascli_print_stdout ("  - %s:", _("Iconsets"));
				for (j = 0; j < icon_dirs->len; j++) {
					const gchar *ipath;
					g_autofree gchar *dname = NULL;
					ipath = (const gchar *) g_ptr_array_index (icon_dirs, j);

					dname = g_path_get_basename (ipath);
					g_print ("     %s\n", dname);
				}
			}
		} else if (found) {
			ascli_print_stdout ("  - %s", _("No icons."));
		}

		if (!found) {
			ascli_print_stdout ("  - %s", _("Empty."));
		}

		g_print ("\n");
	}

	ascli_print_highlight (_("Upstream metadata:"));
	if (g_file_test (metainfo_path, G_FILE_TEST_IS_DIR)) {
		g_autoptr(GPtrArray) xmls = NULL;
		g_autofree gchar *msg = NULL;

		xmls = as_utils_find_files_matching (metainfo_path, "*.xml", FALSE, NULL);
		if (xmls != NULL) {
			msg = g_strdup_printf (_("Found %i components."), xmls->len);
			ascli_print_stdout ("  - %s", msg);
		}
	} else {
		ascli_print_stdout ("  - %s", _("Empty."));
	}
	g_print ("\n");

	ascli_print_highlight (_("Summary:"));
	cache = as_database_new ();
	if (g_file_test (as_database_get_location (cache), G_FILE_TEST_IS_DIR)) {
		g_autoptr(GPtrArray) cpts = NULL;
		g_autoptr(GError) error = NULL;

		ascli_print_stdout (_("The system metadata cache exists."));
		as_database_open (cache, NULL);
		cpts = as_database_get_all_components (cache, &error);
		if (error != NULL) {
			ascli_print_stdout (_("There was an error while trying to find additional information: %s"), error->message);
		} else {
			ascli_print_stdout (_("We have information on %i software components."), cpts->len);
		}
	} else {
		ascli_print_stdout (_("The system metadata cache does not exist."));
	}

	return 0;
}
