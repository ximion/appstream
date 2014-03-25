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
#include <appstream_internal.h>
#include <unistd.h>
#include <sys/types.h>
#include <locale.h>

#include "../src/as-database-builder.h"

#define TYPE_AS_CLIENT (as_client_get_type ())
#define AS_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_AS_CLIENT, ASClient))
#define AS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_AS_CLIENT, ASClientClass))
#define IS_AS_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_AS_CLIENT))
#define IS_AS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_AS_CLIENT))
#define AS_CLIENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_AS_CLIENT, ASClientClass))

typedef struct _ASClient ASClient;
typedef struct _ASClientClass ASClientClass;
typedef struct _ASClientPrivate ASClientPrivate;
#define _g_main_loop_unref0(var) ((var == NULL) ? NULL : (var = (g_main_loop_unref (var), NULL)))
#define _g_free0(var) (var = (g_free (var), NULL))
#define _g_error_free0(var) ((var == NULL) ? NULL : (var = (g_error_free (var), NULL)))
#define _g_option_context_free0(var) ((var == NULL) ? NULL : (var = (g_option_context_free (var), NULL)))
#define _g_ptr_array_unref0(var) ((var == NULL) ? NULL : (var = (g_ptr_array_unref (var), NULL)))
#define _g_object_unref0(var) ((var == NULL) ? NULL : (var = (g_object_unref (var), NULL)))

struct _ASClient {
	GObject parent_instance;
	ASClientPrivate * priv;
};

struct _ASClientClass {
	GObjectClass parent_class;
};

struct _ASClientPrivate {
	GMainLoop* loop;
	gint _exit_code;
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
enum  {
	AS_CLIENT_DUMMY_PROPERTY,
	AS_CLIENT_EXIT_CODE
};
ASClient* as_client_new (gchar** args, int args_length1);
ASClient* as_client_construct (GType object_type, gchar** args, int args_length1);
void as_client_set_exit_code (ASClient* self, gint value);
static void as_client_quit_loop (ASClient* self);
static void as_client_print_key_value (ASClient* self, const gchar* key, const gchar* val, gboolean highlight);
static void as_client_print_separator (ASClient* self);
void as_client_run (ASClient* self);
gint as_client_get_exit_code (ASClient* self);
static gint as_client_main (gchar** args, int args_length1);
static void as_client_finalize (GObject* obj);
static void _vala_as_client_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void _vala_as_client_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);
static void _vala_array_destroy (gpointer array, gint array_length, GDestroyNotify destroy_func);
static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func);
static gint _vala_array_length (gpointer array);

static const GOptionEntry AS_CLIENT_options[7] = {{"version", 'v', 0, G_OPTION_ARG_NONE, &as_client_o_show_version, "Show the application's version", NULL}, {"verbose", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_verbose_mode, "Enable verbose mode", NULL}, {"no-color", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_no_color, "Don't show colored output", NULL}, {"refresh", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_refresh, "Rebuild the application information cache", NULL}, {"force", (gchar) 0, 0, G_OPTION_ARG_NONE, &as_client_o_force, "Enforce a cache refresh", NULL}, {"search", 's', 0, G_OPTION_ARG_STRING, &as_client_o_search, "Search the application database", NULL}, {NULL}};

ASClient* as_client_construct (GType object_type, gchar** args, int args_length1) {
	ASClient * self = NULL;
	GOptionContext* opt_context = NULL;
	GOptionContext* _tmp0_ = NULL;
	GOptionContext* _tmp1_ = NULL;
	GOptionContext* _tmp2_ = NULL;
	GMainLoop* _tmp13_ = NULL;
	GError * _inner_error_ = NULL;
	self = (ASClient*) g_object_new (object_type, NULL);
	as_client_set_exit_code (self, 0);
	_tmp0_ = g_option_context_new ("- Appstream-Index client tool.");
	opt_context = _tmp0_;
	_tmp1_ = opt_context;
	g_option_context_set_help_enabled (_tmp1_, TRUE);
	_tmp2_ = opt_context;
	g_option_context_add_main_entries (_tmp2_, AS_CLIENT_options, NULL);
	{
		GOptionContext* _tmp3_ = NULL;
		_tmp3_ = opt_context;
		g_option_context_parse (_tmp3_, &args_length1, &args, &_inner_error_);
		if (_inner_error_ != NULL) {
			goto __catch0_g_error;
		}
	}
	goto __finally0;
	__catch0_g_error:
	{
		GError* e = NULL;
		FILE* _tmp4_ = NULL;
		GError* _tmp5_ = NULL;
		const gchar* _tmp6_ = NULL;
		gchar* _tmp7_ = NULL;
		gchar* _tmp8_ = NULL;
		FILE* _tmp9_ = NULL;
		const gchar* _tmp10_ = NULL;
		gchar** _tmp11_ = NULL;
		gint _tmp11__length1 = 0;
		const gchar* _tmp12_ = NULL;
		e = _inner_error_;
		_inner_error_ = NULL;
		_tmp4_ = stdout;
		_tmp5_ = e;
		_tmp6_ = _tmp5_->message;
		_tmp7_ = g_strconcat (_tmp6_, "\n", NULL);
		_tmp8_ = _tmp7_;
		fprintf (_tmp4_, "%s", _tmp8_);
		_g_free0 (_tmp8_);
		_tmp9_ = stdout;
		_tmp10_ = _ ("Run '%s --help' to see a full list of available command line options.\n");
		_tmp11_ = args;
		_tmp11__length1 = args_length1;
		_tmp12_ = _tmp11_[0];
		fprintf (_tmp9_, _tmp10_, _tmp12_);
		as_client_set_exit_code (self, 1);
		_g_error_free0 (e);
		_g_option_context_free0 (opt_context);
		return self;
	}
	__finally0:
	if (_inner_error_ != NULL) {
		_g_option_context_free0 (opt_context);
		g_critical ("file %s: line %d: uncaught error: %s (%s, %d)", __FILE__, __LINE__, _inner_error_->message, g_quark_to_string (_inner_error_->domain), _inner_error_->code);
		g_clear_error (&_inner_error_);
		return NULL;
	}
	_tmp13_ = g_main_loop_new (NULL, FALSE);
	_g_main_loop_unref0 (self->priv->loop);
	self->priv->loop = _tmp13_;
	_g_option_context_free0 (opt_context);
	return self;
}


ASClient* as_client_new (gchar** args, int args_length1) {
	return as_client_construct (TYPE_AS_CLIENT, args, args_length1);
}


static void as_client_quit_loop (ASClient* self) {
	GMainLoop* _tmp0_ = NULL;
	gboolean _tmp1_ = FALSE;
	g_return_if_fail (self != NULL);
	_tmp0_ = self->priv->loop;
	_tmp1_ = g_main_loop_is_running (_tmp0_);
	if (_tmp1_) {
		GMainLoop* _tmp2_ = NULL;
		_tmp2_ = self->priv->loop;
		g_main_loop_quit (_tmp2_);
	}
}


static void as_client_print_key_value (ASClient* self, const gchar* key, const gchar* val, gboolean highlight) {
	gboolean _tmp0_ = FALSE;
	const gchar* _tmp1_ = NULL;
	gboolean _tmp3_ = FALSE;
	FILE* _tmp4_ = NULL;
	const gchar* _tmp5_ = NULL;
	gchar* _tmp6_ = NULL;
	gchar* _tmp7_ = NULL;
	const gchar* _tmp8_ = NULL;
	g_return_if_fail (self != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (val != NULL);
	_tmp1_ = val;
	if (_tmp1_ == NULL) {
		_tmp0_ = TRUE;
	} else {
		const gchar* _tmp2_ = NULL;
		_tmp2_ = val;
		_tmp0_ = g_strcmp0 (_tmp2_, "") == 0;
	}
	_tmp3_ = _tmp0_;
	if (_tmp3_) {
		return;
	}
	_tmp4_ = stdout;
	_tmp5_ = key;
	_tmp6_ = g_strdup_printf ("%s: ", _tmp5_);
	_tmp7_ = _tmp6_;
	_tmp8_ = val;
	fprintf (_tmp4_, "%c[%dm%s%c[%dm%s\n", 0x1B, 1, _tmp7_, 0x1B, 0, _tmp8_);
	_g_free0 (_tmp7_);
}


static void as_client_print_separator (ASClient* self) {
	FILE* _tmp0_ = NULL;
	g_return_if_fail (self != NULL);
	_tmp0_ = stdout;
	fprintf (_tmp0_, "%c[%dm%s\n%c[%dm", 0x1B, 36, "----", 0x1B, 0);
}


static gpointer _g_object_ref0 (gpointer self) {
	return self ? g_object_ref (self) : NULL;
}


void as_client_run (ASClient* self) {
	gint _tmp0_ = 0;
	gboolean _tmp1_ = FALSE;
	gboolean _tmp3_ = FALSE;
	AsDatabase* db = NULL;
	AsDatabase* _tmp4_ = NULL;
	const gchar* _tmp5_ = NULL;
	g_return_if_fail (self != NULL);
	_tmp0_ = self->priv->_exit_code;
	if (_tmp0_ > 0) {
		return;
	}
	_tmp1_ = as_client_o_show_version;
	if (_tmp1_) {
		FILE* _tmp2_ = NULL;
		_tmp2_ = stdout;
		fprintf (_tmp2_, "Appstream-Index client tool version: %s\n", VERSION);
		return;
	}
	_tmp3_ = as_client_o_verbose_mode;
	if (_tmp3_) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}
	_tmp4_ = as_database_new ();
	db = _tmp4_;
	_tmp5_ = as_client_o_search;
	if (_tmp5_ != NULL) {
		AsDatabase* _tmp6_ = NULL;
		GPtrArray* app_list = NULL;
		AsDatabase* _tmp7_ = NULL;
		const gchar* _tmp8_ = NULL;
		GPtrArray* _tmp9_ = NULL;
		GPtrArray* _tmp10_ = NULL;
		GPtrArray* _tmp13_ = NULL;
		guint _tmp14_ = 0U;
		_tmp6_ = db;
		as_database_open (_tmp6_);
		_tmp7_ = db;
		_tmp8_ = as_client_o_search;
		_tmp9_ = as_database_find_applications_by_str (_tmp7_, _tmp8_, NULL);
		_g_ptr_array_unref0 (app_list);
		app_list = _tmp9_;
		_tmp10_ = app_list;
		if (_tmp10_ == NULL) {
			FILE* _tmp11_ = NULL;
			const gchar* _tmp12_ = NULL;
			_tmp11_ = stdout;
			_tmp12_ = as_client_o_search;
			fprintf (_tmp11_, "Unable to find application matching %s!\n", _tmp12_);
			as_client_set_exit_code (self, 4);
			_g_ptr_array_unref0 (app_list);
			_g_object_unref0 (db);
			return;
		}
		_tmp13_ = app_list;
		_tmp14_ = _tmp13_->len;
		if (_tmp14_ == ((guint) 0)) {
			FILE* _tmp15_ = NULL;
			const gchar* _tmp16_ = NULL;
			_tmp15_ = stdout;
			_tmp16_ = as_client_o_search;
			fprintf (_tmp15_, "No application matching '%s' found.\n", _tmp16_);
			_g_ptr_array_unref0 (app_list);
			_g_object_unref0 (db);
			return;
		}
		{
			guint i = 0U;
			i = (guint) 0;
			{
				gboolean _tmp17_ = FALSE;
				_tmp17_ = TRUE;
				while (TRUE) {
					gboolean _tmp18_ = FALSE;
					guint _tmp20_ = 0U;
					GPtrArray* _tmp21_ = NULL;
					guint _tmp22_ = 0U;
					AsComponent* app = NULL;
					GPtrArray* _tmp23_ = NULL;
					guint _tmp24_ = 0U;
					void* _tmp25_ = NULL;
					AsComponent* _tmp26_ = NULL;
					AsComponent* _tmp27_ = NULL;
					const gchar* _tmp28_ = NULL;
					const gchar* _tmp29_ = NULL;
					AsComponent* _tmp30_ = NULL;
					const gchar* _tmp31_ = NULL;
					const gchar* _tmp32_ = NULL;
					AsComponent* _tmp33_ = NULL;
					const gchar* _tmp34_ = NULL;
					const gchar* _tmp35_ = NULL;
					AsComponent* _tmp36_ = NULL;
					const gchar* _tmp37_ = NULL;
					const gchar* _tmp38_ = NULL;
					AsComponent* _tmp39_ = NULL;
					const gchar* _tmp40_ = NULL;
					const gchar* _tmp41_ = NULL;
					AsComponent* _tmp42_ = NULL;
					const gchar* _tmp43_ = NULL;
					const gchar* _tmp44_ = NULL;
					_tmp18_ = _tmp17_;
					if (!_tmp18_) {
						guint _tmp19_ = 0U;
						_tmp19_ = i;
						i = _tmp19_ + 1;
					}
					_tmp17_ = FALSE;
					_tmp20_ = i;
					_tmp21_ = app_list;
					_tmp22_ = _tmp21_->len;
					if (!(_tmp20_ < _tmp22_)) {
						break;
					}
					_tmp23_ = app_list;
					_tmp24_ = i;
					_tmp25_ = g_ptr_array_index (_tmp23_, _tmp24_);
					_tmp26_ = _g_object_ref0 (G_TYPE_CHECK_INSTANCE_CAST (_tmp25_, AS_TYPE_COMPONENT, AsComponent));
					app = _tmp26_;
					_tmp27_ = app;
					_tmp28_ = as_component_get_name (_tmp27_);
					_tmp29_ = _tmp28_;
					as_client_print_key_value (self, "Application", _tmp29_, FALSE);
					_tmp30_ = app;
					_tmp31_ = as_component_get_summary (_tmp30_);
					_tmp32_ = _tmp31_;
					as_client_print_key_value (self, "Summary", _tmp32_, FALSE);
					_tmp33_ = app;
					_tmp34_ = as_component_get_pkgname (_tmp33_);
					_tmp35_ = _tmp34_;
					as_client_print_key_value (self, "Package", _tmp35_, FALSE);
					_tmp36_ = app;
					_tmp37_ = as_component_get_homepage (_tmp36_);
					_tmp38_ = _tmp37_;
					as_client_print_key_value (self, "Homepage", _tmp38_, FALSE);
					_tmp39_ = app;
					_tmp40_ = as_component_get_desktop_file (_tmp39_);
					_tmp41_ = _tmp40_;
					as_client_print_key_value (self, "Desktop-File", _tmp41_, FALSE);
					_tmp42_ = app;
					_tmp43_ = as_component_get_icon_url (_tmp42_);
					_tmp44_ = _tmp43_;
					as_client_print_key_value (self, "Icon", _tmp44_, FALSE);
					{
						guint j = 0U;
						j = (guint) 0;
						{
							gboolean _tmp45_ = FALSE;
							_tmp45_ = TRUE;
							while (TRUE) {
								gboolean _tmp46_ = FALSE;
								guint _tmp48_ = 0U;
								AsComponent* _tmp49_ = NULL;
								GPtrArray* _tmp50_ = NULL;
								GPtrArray* _tmp51_ = NULL;
								guint _tmp52_ = 0U;
								AsScreenshot* sshot = NULL;
								AsComponent* _tmp53_ = NULL;
								GPtrArray* _tmp54_ = NULL;
								GPtrArray* _tmp55_ = NULL;
								guint _tmp56_ = 0U;
								void* _tmp57_ = NULL;
								AsScreenshot* _tmp58_ = NULL;
								AsScreenshot* _tmp59_ = NULL;
								gboolean _tmp60_ = FALSE;
								_tmp46_ = _tmp45_;
								if (!_tmp46_) {
									guint _tmp47_ = 0U;
									_tmp47_ = j;
									j = _tmp47_ + 1;
								}
								_tmp45_ = FALSE;
								_tmp48_ = j;
								_tmp49_ = app;
								_tmp50_ = as_component_get_screenshots (_tmp49_);
								_tmp51_ = _tmp50_;
								_tmp52_ = _tmp51_->len;
								if (!(_tmp48_ < _tmp52_)) {
									break;
								}
								_tmp53_ = app;
								_tmp54_ = as_component_get_screenshots (_tmp53_);
								_tmp55_ = _tmp54_;
								_tmp56_ = j;
								_tmp57_ = g_ptr_array_index (_tmp55_, _tmp56_);
								_tmp58_ = _g_object_ref0 (G_TYPE_CHECK_INSTANCE_CAST (_tmp57_, AS_TYPE_SCREENSHOT, AsScreenshot));
								sshot = _tmp58_;
								_tmp59_ = sshot;
								_tmp60_ = as_screenshot_is_default (_tmp59_);
								if (_tmp60_) {
									gchar** sizes = NULL;
									AsScreenshot* _tmp61_ = NULL;
									gchar** _tmp62_ = NULL;
									gchar** _tmp63_ = NULL;
									gint sizes_length1 = 0;
									gint _sizes_size_ = 0;
									gchar** _tmp64_ = NULL;
									gint _tmp64__length1 = 0;
									const gchar* _tmp65_ = NULL;
									gchar* sshot_url = NULL;
									AsScreenshot* _tmp66_ = NULL;
									gchar** _tmp67_ = NULL;
									gint _tmp67__length1 = 0;
									const gchar* _tmp68_ = NULL;
									gchar* _tmp69_ = NULL;
									gchar* caption = NULL;
									AsScreenshot* _tmp70_ = NULL;
									const gchar* _tmp71_ = NULL;
									const gchar* _tmp72_ = NULL;
									gchar* _tmp73_ = NULL;
									const gchar* _tmp74_ = NULL;
									_tmp61_ = sshot;
									_tmp63_ = _tmp62_ = as_screenshot_get_available_sizes (_tmp61_);
									sizes = _tmp63_;
									sizes_length1 = _vala_array_length (_tmp62_);
									_sizes_size_ = sizes_length1;
									_tmp64_ = sizes;
									_tmp64__length1 = sizes_length1;
									_tmp65_ = _tmp64_[0];
									if (_tmp65_ == NULL) {
										sizes = (_vala_array_free (sizes, sizes_length1, (GDestroyNotify) g_free), NULL);
										_g_object_unref0 (sshot);
										continue;
									}
									_tmp66_ = sshot;
									_tmp67_ = sizes;
									_tmp67__length1 = sizes_length1;
									_tmp68_ = _tmp67_[0];
									_tmp69_ = as_screenshot_get_url_for_size (_tmp66_, _tmp68_);
									sshot_url = _tmp69_;
									_tmp70_ = sshot;
									_tmp71_ = as_screenshot_get_caption (_tmp70_);
									_tmp72_ = _tmp71_;
									_tmp73_ = g_strdup (_tmp72_);
									caption = _tmp73_;
									_tmp74_ = caption;
									if (g_strcmp0 (_tmp74_, "") == 0) {
										const gchar* _tmp75_ = NULL;
										_tmp75_ = sshot_url;
										as_client_print_key_value (self, "Screenshot", _tmp75_, FALSE);
									} else {
										const gchar* _tmp76_ = NULL;
										const gchar* _tmp77_ = NULL;
										gchar* _tmp78_ = NULL;
										gchar* _tmp79_ = NULL;
										_tmp76_ = sshot_url;
										_tmp77_ = caption;
										_tmp78_ = g_strdup_printf ("%s (%s)", _tmp76_, _tmp77_);
										_tmp79_ = _tmp78_;
										as_client_print_key_value (self, "Screenshot", _tmp79_, FALSE);
										_g_free0 (_tmp79_);
									}
									_g_free0 (caption);
									_g_free0 (sshot_url);
									sizes = (_vala_array_free (sizes, sizes_length1, (GDestroyNotify) g_free), NULL);
									_g_object_unref0 (sshot);
									break;
								}
								_g_object_unref0 (sshot);
							}
						}
					}
					as_client_print_separator (self);
					_g_object_unref0 (app);
				}
			}
		}
		_g_ptr_array_unref0 (app_list);
	} else {
		gboolean _tmp80_ = FALSE;
		_tmp80_ = as_client_o_refresh;
		if (_tmp80_) {
			uid_t _tmp81_ = {0};
			AsBuilder* builder = NULL;
			AsBuilder* _tmp83_ = NULL;
			AsBuilder* _tmp84_ = NULL;
			AsBuilder* _tmp85_ = NULL;
			gboolean _tmp86_ = FALSE;
			_tmp81_ = getuid ();
			if (_tmp81_ != ((uid_t) 0)) {
				FILE* _tmp82_ = NULL;
				_tmp82_ = stdout;
				fprintf (_tmp82_, "You need to run this command with superuser permissions!\n");
				as_client_set_exit_code (self, 2);
				_g_object_unref0 (db);
				return;
			}
			_tmp83_ = as_builder_new ();
			builder = _tmp83_;
			_tmp84_ = builder;
			as_builder_initialize (_tmp84_);
			_tmp85_ = builder;
			_tmp86_ = as_client_o_force;
			as_builder_refresh_cache (_tmp85_, _tmp86_);
			_g_object_unref0 (builder);
		} else {
			FILE* _tmp87_ = NULL;
			_tmp87_ = stderr;
			fprintf (_tmp87_, "No command specified.\n");
			_g_object_unref0 (db);
			return;
		}
	}
	_g_object_unref0 (db);
}


static gint as_client_main (gchar** args, int args_length1) {
	gint result = 0;
	ASClient* main = NULL;
	gchar** _tmp0_ = NULL;
	gint _tmp0__length1 = 0;
	ASClient* _tmp1_ = NULL;
	gint code = 0;
	gint _tmp2_ = 0;
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	_tmp0_ = args;
	_tmp0__length1 = args_length1;
	_tmp1_ = as_client_new (_tmp0_, _tmp0__length1);
	main = _tmp1_;
	as_client_run (main);
	_tmp2_ = main->priv->_exit_code;
	code = _tmp2_;
	result = code;
	_g_object_unref0 (main);
	return result;
}


int main (int argc, char ** argv) {
	return as_client_main (argv, argc);
}


gint as_client_get_exit_code (ASClient* self) {
	gint result;
	gint _tmp0_ = 0;
	g_return_val_if_fail (self != NULL, 0);
	_tmp0_ = self->priv->_exit_code;
	result = _tmp0_;
	return result;
}


void as_client_set_exit_code (ASClient* self, gint value) {
	gint _tmp0_ = 0;
	g_return_if_fail (self != NULL);
	_tmp0_ = value;
	self->priv->_exit_code = _tmp0_;
	g_object_notify ((GObject *) self, "exit-code");
}


static void as_client_class_init (ASClientClass * klass) {
	as_client_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (ASClientPrivate));
	G_OBJECT_CLASS (klass)->get_property = _vala_as_client_get_property;
	G_OBJECT_CLASS (klass)->set_property = _vala_as_client_set_property;
	G_OBJECT_CLASS (klass)->finalize = as_client_finalize;
	g_object_class_install_property (G_OBJECT_CLASS (klass), AS_CLIENT_EXIT_CODE, g_param_spec_int ("exit-code", "exit-code", "exit-code", G_MININT, G_MAXINT, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
}


static void as_client_instance_init (ASClient * self) {
	self->priv = AS_CLIENT_GET_PRIVATE (self);
}


static void as_client_finalize (GObject* obj) {
	ASClient * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, TYPE_AS_CLIENT, ASClient);
	_g_main_loop_unref0 (self->priv->loop);
	G_OBJECT_CLASS (as_client_parent_class)->finalize (obj);
}


GType as_client_get_type (void) {
	static volatile gsize as_client_type_id__volatile = 0;
	if (g_once_init_enter (&as_client_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = { sizeof (ASClientClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) as_client_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (ASClient), 0, (GInstanceInitFunc) as_client_instance_init, NULL };
		GType as_client_type_id;
		as_client_type_id = g_type_register_static (G_TYPE_OBJECT, "ASClient", &g_define_type_info, 0);
		g_once_init_leave (&as_client_type_id__volatile, as_client_type_id);
	}
	return as_client_type_id__volatile;
}


static void _vala_as_client_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	ASClient * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, TYPE_AS_CLIENT, ASClient);
	switch (property_id) {
		case AS_CLIENT_EXIT_CODE:
		g_value_set_int (value, as_client_get_exit_code (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void _vala_as_client_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	ASClient * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, TYPE_AS_CLIENT, ASClient);
	switch (property_id) {
		case AS_CLIENT_EXIT_CODE:
		as_client_set_exit_code (self, g_value_get_int (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void _vala_array_destroy (gpointer array, gint array_length, GDestroyNotify destroy_func) {
	if ((array != NULL) && (destroy_func != NULL)) {
		int i;
		for (i = 0; i < array_length; i = i + 1) {
			if (((gpointer*) array)[i] != NULL) {
				destroy_func (((gpointer*) array)[i]);
			}
		}
	}
}


static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func) {
	_vala_array_destroy (array, array_length, destroy_func);
	g_free (array);
}


static gint _vala_array_length (gpointer array) {
	int length;
	length = 0;
	if (array) {
		while (((gpointer*) array)[length]) {
			length++;
		}
	}
	return length;
}



