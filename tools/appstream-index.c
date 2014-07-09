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

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>
#include <config.h>
#include <locale.h>

#include "appstream.h"
#include "as-cache-builder.h"

static gboolean as_client_o_show_version = FALSE;
static gboolean as_client_o_verbose_mode = FALSE;
static gboolean as_client_o_no_color = FALSE;
static gboolean as_client_o_refresh = FALSE;
static gboolean as_client_o_force = FALSE;
static gchar* as_client_o_search = NULL;
static gboolean as_client_o_details = FALSE;
static gchar* as_client_o_get_id = NULL;
static gboolean as_client_o_what_provides = FALSE;
static gchar* as_client_o_type = NULL;
static gchar* as_client_o_value = NULL;

static gchar*
format_long_output (const gchar *str)
{
	gchar *res;
	gchar *str2;
	gchar **strv;
	guint i;
	gboolean do_linebreak = FALSE;

	str2 = g_strdup (str);
	for (i = 0; str2[i] != '\0'; ++i) {
		if ((i != 0) && ((i % 80) == 0))
			do_linebreak = TRUE;
		if ((do_linebreak) && (str2[i] == ' ')) {
			do_linebreak = FALSE;
			str2[i] = '\n';
		}
	}

	strv = g_strsplit (str2, "\n", -1);
	g_free (str2);

	res = g_strjoinv ("\n  ", strv);
	g_strfreev (strv);

	return res;
}

static void
as_print_key_value (const gchar* key, const gchar* val, gboolean highlight)
{
	gchar *str;
	gchar *fmtval;
	g_return_if_fail (key != NULL);

	if (as_str_empty (val))
		return;

	if (strlen (val) > 120) {
		/* only produces slightly better output (indented).
		 * we need word-wrapping in future
		 */
		fmtval = format_long_output (val);
	} else {
		fmtval = g_strdup (val);
	}

	str = g_strdup_printf ("%s: ", key);

	fprintf (stdout, "%c[%dm%s%c[%dm%s\n", 0x1B, 1, str, 0x1B, 0, fmtval);

	g_free (str);
	g_free (fmtval);
}


static void
as_print_separator ()
{
	fprintf (stdout, "%c[%dm%s\n%c[%dm", 0x1B, 36, "----", 0x1B, 0);
}

static void
as_print_component (AsComponent *cpt)
{
	gchar *short_idline;
	guint j;

	short_idline = g_strdup_printf ("%s [%s]",
							as_component_get_id (cpt),
							as_component_kind_to_string (as_component_get_kind (cpt)));

	as_print_key_value (_("Identifier"), short_idline, FALSE);
	as_print_key_value (_("Name"), as_component_get_name (cpt), FALSE);
	as_print_key_value (_("Summary"), as_component_get_summary (cpt), FALSE);
	as_print_key_value (_("Package"), as_component_get_pkgname (cpt), FALSE);
	as_print_key_value (_("Homepage"), as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), FALSE);
	as_print_key_value (_("Icon"), as_component_get_icon_url (cpt), FALSE);
	g_free (short_idline);
	short_idline = NULL;

	if (as_client_o_details) {
		GPtrArray *sshot_array;
		GPtrArray *imgs = NULL;
		GPtrArray *provided_items;
		AsScreenshot *sshot;
		AsImage *img;
		gchar *str;
		gchar **strv;

		/* long description */
		str = as_description_markup_convert_simple (as_component_get_description (cpt));
		as_print_key_value (_("Description"), str, FALSE);
		g_free (str);

		/* some simple screenshot information */
		sshot_array = as_component_get_screenshots (cpt);

		/* find default screenshot, if possible */
		sshot = NULL;
		for (j = 0; j < sshot_array->len; j++) {
			sshot = (AsScreenshot*) g_ptr_array_index (sshot_array, j);
			if (as_screenshot_get_kind (sshot) == AS_SCREENSHOT_KIND_DEFAULT)
				break;
		}

		if (sshot != NULL) {
			/* get the first source image and display it's url */
			imgs = as_screenshot_get_images (sshot);
			for (j = 0; j < imgs->len; j++) {
				img = (AsImage*) g_ptr_array_index (imgs, j);
				if (as_image_get_kind (img) == AS_IMAGE_KIND_SOURCE) {
					as_print_key_value ("Sample Screenshot URL", as_image_get_url (img), FALSE);
					break;
				}
			}
		}

		/* project group */
		as_print_key_value (_("Project Group"), as_component_get_project_group (cpt), FALSE);

		/* license */
		as_print_key_value (_("License"), as_component_get_project_license (cpt), FALSE);

		/* Categories */
		strv = as_component_get_categories (cpt);
		if (strv != NULL) {
			str = g_strjoinv (", ", strv);
			as_print_key_value (_("Categories"), str, FALSE);
			g_free (str);
		}

		/* desktop-compulsority */
		strv = as_component_get_compulsory_for_desktops (cpt);
		if (strv != NULL) {
			str = g_strjoinv (", ", strv);
			as_print_key_value (_("Compulsory for"), str, FALSE);
			g_free (str);
		}

		/* Provided Items */
		provided_items = as_component_get_provided_items (cpt);
		strv = as_ptr_array_to_strv (provided_items);
		if (strv != NULL) {
			str = g_strjoinv (" ", strv);
			as_print_key_value (_("Provided Items"), str, FALSE);
			g_free (str);
		}
		g_strfreev (strv);
	}
}

int
as_client_run (char **argv, int argc)
{
	GOptionContext *opt_context;
	GError *error = NULL;
	int exit_code = 0;
	AsDatabase *db = NULL;
	guint i;

	const GOptionEntry client_options[] = {
		{ "version", 0, 0, G_OPTION_ARG_NONE, &as_client_o_show_version, _("Show the program version"), NULL },
		{ "verbose", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_verbose_mode, _("Show extra debugging information"), NULL },
		{ "no-color", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_no_color, _("Don't show colored output"), NULL },
		{ "refresh", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_refresh, _("Rebuild the component information cache"), NULL },
		{ "force", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_force, _("Enforce a cache refresh"), NULL },
		{ "search", 's', 0, G_OPTION_ARG_STRING, &as_client_o_search, _("Search the component database"), NULL },
		{ "details", 0, 0, G_OPTION_ARG_NONE, &as_client_o_details, _("Print detailed output about found components"), NULL },
		{ "get", 0, 0, G_OPTION_ARG_STRING, &as_client_o_get_id, _("Get component by id"), NULL },
		{ "what-provides", 0, 0, G_OPTION_ARG_NONE, &as_client_o_what_provides, _("Get components which provide the given item"), NULL },
		{ "type", 't', 0, G_OPTION_ARG_STRING, &as_client_o_type, _("Select a provides type (e.g. lib, bin, python3, ...)"), NULL },
		{ "value", 'v', 0, G_OPTION_ARG_STRING, &as_client_o_value, _("Select a value for the provides-item which needs to be found"), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- Appstream-Index client tool.");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, client_options, NULL);

	g_option_context_parse (opt_context, &argc, &argv, &error);
	if (error != NULL) {
		gchar *msg;
		msg = g_strconcat (error->message, "\n", NULL);
		fprintf (stdout, "%s", msg);
		g_free (msg);
		fprintf (stdout, _("Run '%s --help' to see a full list of available command line options.\n"), argv[0]);
		exit_code = 1;
		g_error_free (error);
		goto out;
	}

	if (as_client_o_show_version) {
		fprintf (stdout, "Appstream-Index client tool version: %s\n", VERSION);
		goto out;
	}

	/* just a hack, we might need proper message handling later */
	if (as_client_o_verbose_mode) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}

	/* prepare the AppStream database connection */
	db = as_database_new ();
	if (as_client_o_search != NULL) {
		GPtrArray* cpt_list = NULL;
		/* search for stuff */

		as_database_open (db);
		cpt_list = as_database_find_components_by_str (db, as_client_o_search, NULL);
		if (cpt_list == NULL) {
			fprintf (stderr, "Unable to find component matching %s!\n", as_client_o_search);
			exit_code = 4;
			goto out;
		}

		if (cpt_list->len == 0) {
			fprintf (stdout, "No component matching '%s' found.\n", as_client_o_search);
			g_ptr_array_unref (cpt_list);
			goto out;
		}

		for (i = 0; i < cpt_list->len; i++) {
			AsComponent *cpt;
			cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

			as_print_component (cpt);

			as_print_separator ();
		}
		g_ptr_array_unref (cpt_list);

	} else if (as_client_o_refresh) {
		AsBuilder *builder;
		if (getuid () != ((uid_t) 0)) {
			fprintf (stdout, "You need to run this command with superuser permissions!\n");
			exit_code = 2;
			goto out;
		}

		builder = as_builder_new ();
		as_builder_initialize (builder);
		as_builder_refresh_cache (builder, as_client_o_force);
		g_object_unref (builder);
	} else if (as_client_o_get_id != NULL) {
		AsComponent *cpt;
		/* get component by id */

		as_database_open (db);
		cpt = as_database_get_component_by_id (db, as_client_o_get_id);
		if (cpt == NULL) {
			fprintf (stderr, "Unable to find component with id %s!\n", as_client_o_get_id);
			exit_code = 4;
			goto out;
		}
		as_print_component (cpt);
		g_object_unref (cpt);
	} else if (as_client_o_what_provides) {
		GPtrArray* cpt_list = NULL;
		AsProvidesKind kind;
		/* get component providing an item */

		if (as_client_o_value == NULL) {
			fprintf (stderr, "No value for the provides-item to search for defined.\n");
			exit_code = 1;
			goto out;
		}

		kind = as_provides_kind_from_string (as_client_o_type);
		if (kind == AS_PROVIDES_KIND_UNKNOWN) {
			uint i;
			fprintf (stderr, "Invalid type for provides-item selected. Valid values are:\n");
			for (i = 1; i < AS_PROVIDES_KIND_LAST; i++)
				fprintf (stdout, " * %s\n", as_provides_kind_to_string (i));
			exit_code = 5;
			goto out;
		}

		as_database_open (db);
		cpt_list = as_database_get_components_by_provides (db, kind, as_client_o_value, "");
		if (cpt_list == NULL) {
			fprintf (stderr, "Unable to find component providing '%s:%s'!\n", as_client_o_type, as_client_o_value);
			exit_code = 4;
			goto out;
		}

		if (cpt_list->len == 0) {
			fprintf (stdout, "No component providing '%s:%s' found.\n", as_client_o_type, as_client_o_value);
			g_ptr_array_unref (cpt_list);
			goto out;
		}

		for (i = 0; i < cpt_list->len; i++) {
			AsComponent *cpt;
			cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

			as_print_component (cpt);

			as_print_separator ();
		}
		g_ptr_array_unref (cpt_list);
	} else {
		fprintf (stderr, "No command specified.\n");
		goto out;
	}

out:
	g_object_unref (db);
	g_option_context_free (opt_context);

	return exit_code;
}

int
main (int argc, char ** argv)
{
	gint code = 0;

	/* bind locale */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* run the application */
	code = as_client_run (argv, argc);

	return code;
}
