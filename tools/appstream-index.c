/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include <glib.h>
#include <glib-object.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>
#include <config.h>
#include <locale.h>

#include "appstream.h"
#include "as-database-builder.h"

#define TYPE_AS_CLIENT (as_client_get_type ())
#define AS_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_AS_CLIENT, ASClient))
#define AS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_AS_CLIENT, ASClientClass))
#define IS_AS_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_AS_CLIENT))
#define IS_AS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_AS_CLIENT))
#define AS_CLIENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_AS_CLIENT, ASClientClass))

typedef struct _ASClient ASClient;
typedef struct _ASClientClass ASClientClass;
typedef struct _ASClientPrivate ASClientPrivate;

struct _ASClient {
	GObject parent_instance;
	ASClientPrivate * priv;
};

struct _ASClientClass {
	GObjectClass parent_class;
};

struct _ASClientPrivate {
	GMainLoop* loop;
	gint exit_code;
};


static gpointer as_client_parent_class = NULL;
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

GType as_client_get_type (void) G_GNUC_CONST;
#define AS_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_AS_CLIENT, ASClientPrivate))

ASClient* as_client_new (gchar** args, int argc);
ASClient* as_client_construct (GType object_type, gchar** args, int argc);
void as_client_set_exit_code (ASClient* self, gint value);
void as_client_run (ASClient* self);
gint as_client_get_exit_code (ASClient* self);

static void as_client_finalize (GObject* obj);

ASClient*
as_client_construct (GType object_type, gchar** args, int argc)
{
	ASClient * self;
	GOptionContext* opt_context;
	GError * error = NULL;

	const GOptionEntry AS_CLIENT_options[] = {
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

	self = (ASClient*) g_object_new (object_type, NULL);
	as_client_set_exit_code (self, 0);

	opt_context = g_option_context_new ("- Appstream-Index client tool.");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, AS_CLIENT_options, NULL);

	g_option_context_parse (opt_context, &argc, &args, &error);
	if (error != NULL) {
		gchar *msg;
		msg = g_strconcat (error->message, "\n", NULL);
		fprintf (stdout, "%s", msg);
		g_free (msg);
		fprintf (stdout, _("Run '%s --help' to see a full list of available command line options.\n"), args[0]);
		as_client_set_exit_code (self, 1);
		g_error_free (error);
		goto out;
	}

out:
	g_option_context_free (opt_context);
	return self;
}


ASClient*
as_client_new (gchar** args, int argc)
{
	return as_client_construct (TYPE_AS_CLIENT, args, argc);
}

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
			str = g_strjoinv ("; ", strv);
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

		/* Mimetypes */
		strv = as_component_get_mimetypes (cpt);
		if (strv != NULL) {
			str = g_strjoinv ("; ", strv);
			as_print_key_value (_("Mimetypes"), str, FALSE);
			g_free (str);
		}
	}
}

void
as_client_run (ASClient* self)
{
	AsDatabase* db = NULL;
	guint i;

	g_return_if_fail (self != NULL);

	if (self->priv->exit_code > 0) {
		return;
	}
	if (as_client_o_show_version) {
		fprintf (stdout, "Appstream-Index client tool version: %s\n", VERSION);
		return;
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
			as_client_set_exit_code (self, 4);
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
			as_client_set_exit_code (self, 2);
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
			as_client_set_exit_code (self, 4);
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
			as_client_set_exit_code (self, 1);
			goto out;
		}

		kind = as_provides_kind_from_string (as_client_o_type);
		if (kind == AS_PROVIDES_KIND_UNKNOWN) {
			uint i;
			fprintf (stderr, "Invalid type for provides-item selected. Valid values are:\n");
			for (i = 1; i < AS_PROVIDES_KIND_LAST; i++)
				fprintf (stdout, " * %s\n", as_provides_kind_to_string (i));
			as_client_set_exit_code (self, 5);
			goto out;
		}

		as_database_open (db);
		cpt_list = as_database_get_components_by_provides (db, kind, as_client_o_value, "");
		if (cpt_list == NULL) {
			fprintf (stderr, "Unable to find component providing '%s:%s'!\n", as_client_o_type, as_client_o_value);
			as_client_set_exit_code (self, 4);
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
}

int
main (int argc, char ** argv)
{
	ASClient* client;
	gint code = 0;

	/* bind locale */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* run the application */
	client = as_client_new (argv, argc);
	as_client_run (client);

	code = client->priv->exit_code;
	g_object_unref (client);
	return code;
}


gint
as_client_get_exit_code (ASClient* self)
{
	g_return_val_if_fail (self != NULL, 0);
	return self->priv->exit_code;
}


void
as_client_set_exit_code (ASClient* self, gint value)
{
	g_return_if_fail (self != NULL);
	self->priv->exit_code = value;
}


static void
as_client_class_init (ASClientClass * klass)
{
	as_client_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (ASClientPrivate));
	G_OBJECT_CLASS (klass)->finalize = as_client_finalize;
}


static void
as_client_instance_init (ASClient * self)
{
	self->priv = AS_CLIENT_GET_PRIVATE (self);
}


static void as_client_finalize (GObject* obj)
{
	G_OBJECT_CLASS (as_client_parent_class)->finalize (obj);
}


GType
as_client_get_type (void)
{
	static volatile gsize as_client_type_id__volatile = 0;
	if (g_once_init_enter (&as_client_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
					sizeof (ASClientClass),
					(GBaseInitFunc) NULL,
					(GBaseFinalizeFunc) NULL,
					(GClassInitFunc) as_client_class_init,
					(GClassFinalizeFunc) NULL,
					NULL,
					sizeof (ASClient),
					0,
					(GInstanceInitFunc) as_client_instance_init,
					NULL
		};
		GType as_client_type_id;
		as_client_type_id = g_type_register_static (G_TYPE_OBJECT, "ASClient", &g_define_type_info, 0);
		g_once_init_leave (&as_client_type_id__volatile, as_client_type_id);
	}
	return as_client_type_id__volatile;
}
