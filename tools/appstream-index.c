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
#include <locale.h>

#include "appstream.h"
#include "as-cache-builder.h"

static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;
static gboolean optn_no_color = FALSE;
static gboolean optn_force = FALSE;
static gboolean optn_details = FALSE;
static gchar *optn_dbpath = NULL;
static gchar *optn_datapath = NULL;

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

	if (optn_no_color) {
		fprintf (stdout, "%s%s\n", str, fmtval);
	} else {
		fprintf (stdout, "%c[%dm%s%c[%dm%s\n", 0x1B, 1, str, 0x1B, 0, fmtval);
	}

	g_free (str);
	g_free (fmtval);
}


static void
as_print_separator ()
{
	if (optn_no_color) {
		fprintf (stdout, "----\n");
	} else {
		fprintf (stdout, "%c[%dm%s\n%c[%dm", 0x1B, 36, "----", 0x1B, 0);
	}
}

static void
as_print_stderr (const gchar *format, ...)
{
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_printerr ("%s\n", str);

	g_free (str);
}

static void
as_print_stdout (const gchar *format, ...)
{
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_print ("%s\n", str);

	g_free (str);
}

static void
as_print_component (AsComponent *cpt)
{
	gchar *short_idline;
	gchar *pkgs_str;
	guint j;

	short_idline = g_strdup_printf ("%s [%s]",
							as_component_get_id (cpt),
							as_component_kind_to_string (as_component_get_kind (cpt)));
	pkgs_str = g_strjoinv (", ", as_component_get_pkgnames (cpt));

	as_print_key_value (_("Identifier"), short_idline, FALSE);
	as_print_key_value (_("Name"), as_component_get_name (cpt), FALSE);
	as_print_key_value (_("Summary"), as_component_get_summary (cpt), FALSE);
	as_print_key_value (_("Package"), pkgs_str, FALSE);
	as_print_key_value (_("Homepage"), as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), FALSE);
	as_print_key_value (_("Icon"), as_component_get_icon_url (cpt), FALSE);
	g_free (short_idline);
	g_free (pkgs_str);
	short_idline = NULL;

	if (optn_details) {
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
					as_print_key_value (_("Sample Screenshot URL"), as_image_get_url (img), FALSE);
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

/**
 * as_client_get_summary:
 **/
static gchar *
as_client_get_summary ()
{
	GString *string;
	string = g_string_new ("");

	/* TRANSLATORS: This is the header to the --help menu */
	g_string_append_printf (string, "%s\n\n%s\n", _("AppStream-Index Client Tool"),
				/* these are commands we can use with appstream-index */
				_("Subcommands:"));

	g_string_append_printf (string, "  %s - %s\n", "search [TERM]", _("Search the component database"));
	g_string_append_printf (string, "  %s - %s\n", "get [COMPONENT-ID]", _("Get information about a component by its id"));
	g_string_append_printf (string, "  %s - %s\n", "what-provides [TYPE] [VALUE]", _("Get components which provide the given item"));
	g_string_append_printf (string, "    %s - %s\n", "[TYPE]", _("A provides-item type (e.g. lib, bin, python3, ...)"));
	g_string_append_printf (string, "    %s - %s\n", "[VALUE]", _("Select a value for the provides-item which needs to be found"));
	g_string_append_printf (string, "  %s - %s\n", "refresh", _("Rebuild the component information cache"));

	return g_string_free (string, FALSE);
}

/**
 * as_client_refresh_cache:
 */
int
as_client_refresh_cache (const gchar *dbpath, const gchar *datapath, gboolean forced)
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

AsDatabase*
as_client_database_new_path (const gchar *dbpath)
{
	AsDatabase *db;
	db = as_database_new ();
	if (dbpath != NULL)
		as_database_set_database_path (db, dbpath);
	return db;
}

/**
 * as_client_get_component:
 *
 * Get component by id
 */
int
as_client_get_component (const gchar *dbpath, const gchar *identifier)
{
	AsDatabase *db;
	AsComponent *cpt;
	int exit_code;

	if (identifier == NULL) {
		fprintf (stderr, "%s\n", _("You need to specify a component-id."));
		return 2;
	}

	db = as_client_database_new_path (dbpath);
	as_database_open (db);

	cpt = as_database_get_component_by_id (db, identifier);
	if (cpt == NULL) {
		as_print_stderr (_("Unable to find component with id '%s'!"), identifier);
		exit_code = 4;
		goto out;
	}
	as_print_component (cpt);

out:
	g_object_unref (db);
	if (cpt != NULL)
		g_object_unref (cpt);

	return exit_code;
}

/**
 * as_client_search_component:
 */
int
as_client_search_component (const gchar *dbpath, const gchar *search_term)
{
	AsDatabase *db;
	GPtrArray* cpt_list = NULL;
	guint i;
	int exit_code = 0;

	db = as_client_database_new_path (dbpath);

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
		as_print_stderr (_("Unable to find component matching %s!"), search_term);
		exit_code = 4;
		goto out;
	}

	if (cpt_list->len == 0) {
		as_print_stdout (_("No component matching '%s' found."), search_term);
		g_ptr_array_unref (cpt_list);
		goto out;
	}

	for (i = 0; i < cpt_list->len; i++) {
		AsComponent *cpt;
		cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

		as_print_component (cpt);

		as_print_separator ();
	}

out:
	if (cpt_list != NULL)
		g_ptr_array_unref (cpt_list);
	g_object_unref (db);

	return exit_code;
}

/**
 * as_client_what_provides:
 *
 * Get component providing an item
 */
int
as_client_what_provides (const gchar *dbpath, const gchar *kind_str, const gchar *value, const gchar *data)
{
	AsDatabase *db = NULL;
	GPtrArray* cpt_list = NULL;
	AsProvidesKind kind;
	guint i;
	int exit_code;

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

	db = as_client_database_new_path (dbpath);
	as_database_open (db);

	cpt_list = as_database_get_components_by_provides (db, kind, value, data);
	if (cpt_list == NULL) {
		as_print_stderr (_("Unable to find component providing '%s:%s:%s'!"), kind_str, value, data);
		exit_code = 4;
		goto out;
	}

	if (cpt_list->len == 0) {
		as_print_stdout (_("No component providing '%s:%s:%s' found."), kind_str, value, data);
		goto out;
	}

	for (i = 0; i < cpt_list->len; i++) {
		AsComponent *cpt;
		cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

		as_print_component (cpt);
		as_print_separator ();
	}

out:
	if (db != NULL)
		g_object_unref (db);
	if (cpt_list != NULL)
		g_ptr_array_unref (cpt_list);

	return exit_code;
}

/**
 * as_client_run:
 */
int
as_client_run (char **argv, int argc)
{
	GOptionContext *opt_context;
	GError *error = NULL;

	int exit_code = 0;
	gchar *command = NULL;
	gchar *value1 = NULL;
	gchar *value2 = NULL;
	gchar *value3 = NULL;

	gchar *summary;
	gchar *options_help = NULL;

	const GOptionEntry client_options[] = {
		{ "version", 0, 0, G_OPTION_ARG_NONE, &optn_show_version, _("Show the program version"), NULL },
		{ "verbose", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_verbose_mode, _("Show extra debugging information"), NULL },
		{ "no-color", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_no_color, _("Don't show colored output"), NULL },
		{ "force", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_force, _("Enforce a cache refresh"), NULL },
		{ "details", 0, 0, G_OPTION_ARG_NONE, &optn_details, _("Print detailed output about found components"), NULL },
		{ "dbpath", 0, 0, G_OPTION_ARG_STRING, &optn_dbpath, _("Manually set the location of the AppStream cache"), NULL },
		{ "datapath", 0, 0, G_OPTION_ARG_STRING, &optn_datapath, _("Manually set the location of AppStream metadata for cache regeneration"), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- Appstream-Index Client Tool.");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, client_options, NULL);

	/* set the summary text */
	summary = as_client_get_summary ();
	g_option_context_set_summary (opt_context, summary) ;
	options_help = g_option_context_get_help (opt_context, TRUE, NULL);
	g_free (summary);

	g_option_context_parse (opt_context, &argc, &argv, &error);
	if (error != NULL) {
		gchar *msg;
		msg = g_strconcat (error->message, "\n", NULL);
		fprintf (stdout, "%s", msg);
		g_free (msg);
		as_print_stderr (_("Run '%s --help' to see a full list of available command line options."), argv[0]);
		exit_code = 1;
		g_error_free (error);
		goto out;
	}

	if (optn_show_version) {
		as_print_stdout (_("Appstream-Index client tool version: %s"), VERSION);
		goto out;
	}

	/* just a hack, we might need proper message handling later */
	if (optn_verbose_mode) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}

	if (argc < 2) {
		g_printerr ("%s\n", _("You need to specify a command."));
		as_print_stderr (_("Run '%s --help' to see a full list of available command line options."), argv[0]);
		exit_code = 1;
		goto out;
	}

	command = argv[1];
	if (argc > 2)
		value1 = argv[2];
	if (argc > 3)
		value2 = argv[3];
	if (argc > 4)
		value3 = argv[4];

	if ((g_strcmp0 (command, "search") == 0) || (g_strcmp0 (command, "s") == 0)) {
		exit_code = as_client_search_component (optn_dbpath, value1);
	} else if (g_strcmp0 (command, "refresh") == 0) {
		exit_code = as_client_refresh_cache (optn_dbpath, optn_datapath, optn_force);
	} else if (g_strcmp0 (command, "get") == 0) {
		exit_code = as_client_get_component (optn_dbpath, value1);
	} else if (g_strcmp0 (command, "what-provides") == 0) {
		exit_code = as_client_what_provides (optn_dbpath, value1, value2, value3);
	} else {
		as_print_stderr (_("Command '%s' is unknown."), command);
		exit_code = 1;
		goto out;
	}

out:
	g_option_context_free (opt_context);
	if (options_help != NULL)
		g_free (options_help);

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
