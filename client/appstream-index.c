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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>
#include <config.h>
#include <unistd.h>
#include <sys/types.h>
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
static gboolean as_client_o_show_version;
static gboolean as_client_o_show_version = FALSE;
static gboolean as_client_o_verbose_mode;
static gboolean as_client_o_verbose_mode = FALSE;
static gboolean as_client_o_no_color;
static gboolean as_client_o_no_color = FALSE;
static gboolean as_client_o_refresh;
static gboolean as_client_o_refresh = FALSE;
static gboolean as_client_o_force;
static gboolean as_client_o_force = FALSE;
static gchar* as_client_o_search;
static gchar* as_client_o_search = NULL;

GType as_client_get_type (void) G_GNUC_CONST;
#define AS_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_AS_CLIENT, ASClientPrivate))

ASClient* as_client_new (gchar** args, int argc);
ASClient* as_client_construct (GType object_type, gchar** args, int argc);
void as_client_set_exit_code (ASClient* self, gint value);
void as_client_run (ASClient* self);
gint as_client_get_exit_code (ASClient* self);

static void as_client_quit_loop (ASClient* self);
static void as_client_finalize (GObject* obj);

static const GOptionEntry AS_CLIENT_options[7] = {
	{"version", 'v', 0, G_OPTION_ARG_NONE, &as_client_o_show_version, "Show the application's version", NULL},
	{"verbose", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_verbose_mode, "Enable verbose mode", NULL},
	{"no-color", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_no_color, "Don't show colored output", NULL},
	{"refresh", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_refresh, "Rebuild the application information cache", NULL},
	{"force", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_force, "Enforce a cache refresh", NULL},
	{"search", 's', 0, G_OPTION_ARG_STRING, &as_client_o_search, "Search the application database", NULL},
	{NULL}
};

ASClient*
as_client_construct (GType object_type, gchar** args, int argc)
{
	ASClient * self;
	GOptionContext* opt_context;
	GError * error = NULL;

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

	self->priv->loop = g_main_loop_new (NULL, FALSE);

out:
	g_option_context_free (opt_context);
	return self;
}


ASClient*
as_client_new (gchar** args, int argc)
{
	return as_client_construct (TYPE_AS_CLIENT, args, argc);
}


static void
as_client_quit_loop (ASClient* self)
{
	g_return_if_fail (self != NULL);

	if (g_main_loop_is_running (self->priv->loop)) {
		g_main_loop_quit (self->priv->loop);
	}
}

static void
as_client_print_key_value (ASClient* self, const gchar* key, const gchar* val, gboolean highlight)
{
	gchar *str;
	g_return_if_fail (self != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (val != NULL);

	if ((val == NULL) || (g_strcmp0 (val, "") == 0))
		return;

	str = g_strdup_printf ("%s: ", key);
	fprintf (stdout, "%c[%dm%s%c[%dm%s\n", 0x1B, 1, str, 0x1B, 0, val);
	g_free (str);
}


static void
as_client_print_separator (ASClient* self)
{
	g_return_if_fail (self != NULL);
	fprintf (stdout, "%c[%dm%s\n%c[%dm", 0x1B, 36, "----", 0x1B, 0);
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

		as_database_open (db);
		cpt_list = as_database_find_components_by_str (db, as_client_o_search, NULL);
		if (cpt_list == NULL) {
			fprintf (stderr, "Unable to find application matching %s!\n", as_client_o_search);
			as_client_set_exit_code (self, 4);
			goto out;
		}

		if (cpt_list->len == 0) {
			fprintf (stdout, "No application matching '%s' found.\n", as_client_o_search);
			g_ptr_array_unref (cpt_list);
			goto out;
		}

		for (i = 0; i < cpt_list->len; i++) {
			AsComponent *cpt;
			cpt = (AsComponent*) g_ptr_array_index (cpt_list, i);

			as_client_print_key_value (self, "Application", as_component_get_name (cpt), FALSE);
			as_client_print_key_value (self, "Summary", as_component_get_summary (cpt), FALSE);
			as_client_print_key_value (self, "Package", as_component_get_pkgname (cpt), FALSE);
			as_client_print_key_value (self, "Homepage", as_component_get_homepage (cpt), FALSE);
			as_client_print_key_value (self, "Desktop-File", as_component_get_desktop_file (cpt), FALSE);
			as_client_print_key_value (self, "Icon", as_component_get_icon_url (cpt), FALSE);
			as_client_print_separator (self);
		}
		g_ptr_array_unref (cpt_list);
	} else if (as_client_o_refresh) {
		AsBuilder *builder;
		if (getuid () != ((uid_t) 0)) {
			fprintf (stdout, "You need to run this command with superuser permissions!\n");
			as_client_set_exit_code (self, 2);
			goto out;
		}

		as_builder_initialize (builder);
		as_builder_refresh_cache (builder, as_client_o_force);
		g_object_unref (builder);
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
	ASClient* main;
	gint code = 0;

	/* bind locale */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* run the application */
	main = as_client_new (argv, argc);
	as_client_run (main);

	code = main->priv->exit_code;
	g_object_unref (main);
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
	g_object_notify ((GObject *) self, "exit-code");
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
	ASClient * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, TYPE_AS_CLIENT, ASClient);
	g_main_loop_unref (self->priv->loop);
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
