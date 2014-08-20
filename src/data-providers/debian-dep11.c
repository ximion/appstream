/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "debian-dep11.h"

#include <glib.h>
#include <glib-object.h>
#include <yaml.h>

static gpointer as_provider_dep11_parent_class = NULL;

/**
 * as_provider_dep11_construct:
 */
AsProviderDEP11*
as_provider_dep11_construct (GType object_type)
{
	AsProviderDEP11 * self = NULL;
	self = (AsProviderDEP11*) as_data_provider_construct (object_type);
	return self;
}

/**
 * as_provider_dep11_new:
 */
AsProviderDEP11*
as_provider_dep11_new (void) {
	return as_provider_dep11_construct (AS_PROVIDER_TYPE_DEP11);
}

/**
 * as_provider_dep11_process_data:
 */
gboolean
as_provider_dep11_process_data (AsProviderDEP11 *dprov, const gchar *data)
{
	gboolean ret;

	ret = TRUE;

	/* TODO */

	return ret;
}

/**
 * as_provider_dep11_process_compressed_file:
 */
gboolean
as_provider_dep11_process_compressed_file (AsProviderDEP11 *dprov, GFile *infile)
{
	GFileInputStream* src_stream;
	GMemoryOutputStream* mem_os;
	GInputStream *conv_stream;
	GZlibDecompressor* zdecomp;
	guint8* data;
	gboolean ret;

	g_return_val_if_fail (dprov != NULL, FALSE);
	g_return_val_if_fail (infile != NULL, FALSE);

	src_stream = g_file_read (infile, NULL, NULL);
	mem_os = (GMemoryOutputStream*) g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
	zdecomp = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
	conv_stream = g_converter_input_stream_new (G_INPUT_STREAM (src_stream), G_CONVERTER (zdecomp));
	g_object_unref (zdecomp);

	g_output_stream_splice ((GOutputStream*) mem_os, conv_stream, 0, NULL, NULL);
	data = g_memory_output_stream_get_data (mem_os);
	ret = as_provider_dep11_process_data (dprov, (const gchar*) data);

	g_object_unref (conv_stream);
	g_object_unref (mem_os);
	g_object_unref (src_stream);

	return ret;
}

/**
 * as_provider_dep11_process_file:
 */
gboolean
as_provider_dep11_process_file (AsProviderDEP11 *dprov, GFile *infile)
{
	gboolean ret;
	gchar *yml_data;
	gchar *line = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;
	GString *str = NULL;

	g_return_val_if_fail (infile != NULL, FALSE);

	ir = g_file_read (infile, NULL, NULL);
	dis = g_data_input_stream_new ((GInputStream*) ir);
	g_object_unref (ir);

	str = g_string_new ("");
	while (TRUE) {
		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		if (str->len > 0)
			g_string_append (str, "\n");
		g_string_append_printf (str, "%s\n", line);
		g_free (line);
	}
	yml_data = g_string_free (str, FALSE);

	ret = as_provider_dep11_process_data (dprov, yml_data);
	g_object_unref (dis);
	g_free (yml_data);

	return ret;
}

/**
 * as_provider_dep11_real_execute:
 */
static gboolean
as_provider_dep11_real_execute (AsDataProvider* base)
{
	AsProviderDEP11 *dprov;
	GPtrArray* yaml_files;
	guint i;
	GFile *infile;
	gchar **paths;
	gboolean ret = TRUE;
	const gchar *content_type;

	dprov = AS_PROVIDER_DEP11 (base);
	yaml_files = g_ptr_array_new_with_free_func (g_free);

	paths = as_data_provider_get_watch_files (base);
	if (paths == NULL)
		return TRUE;
	for (i = 0; paths[i] != NULL; i++) {
		gchar *path;
		GPtrArray *yamls;
		guint j;
		path = paths[i];

		if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
			continue;
		}
		yamls = as_utils_find_files_matching (path, "*.yml*", FALSE);
		if (yamls == NULL)
			continue;
		for (j = 0; j < yamls->len; j++) {
			const gchar *val;
			val = (const gchar *) g_ptr_array_index (yamls, j);
			g_ptr_array_add (yaml_files, g_strdup (val));
		}

		g_ptr_array_unref (yamls);
	}

	if (yaml_files->len == 0) {
		g_ptr_array_unref (yaml_files);
		return FALSE;
	}

	ret = TRUE;
	for (i = 0; i < yaml_files->len; i++) {
		gchar *fname;
		GFileInfo *info = NULL;
		fname = (gchar*) g_ptr_array_index (yaml_files, i);
		infile = g_file_new_for_path (fname);
		if (!g_file_query_exists (infile, NULL)) {
			g_warning ("File '%s' does not exist.", fname);
			g_object_unref (infile);
			continue;
		}

		info = g_file_query_info (infile,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
		if (info == NULL) {
			g_debug ("No info for file '%s' found, file was skipped.", fname);
			g_object_unref (infile);
			continue;
		}
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
		if (g_strcmp0 (content_type, "application/x-yaml") == 0) {
			ret = as_provider_dep11_process_file (dprov, infile);
		} else if (g_strcmp0 (content_type, "application/gzip") == 0 ||
				g_strcmp0 (content_type, "application/x-gzip") == 0) {
			ret = as_provider_dep11_process_compressed_file (dprov, infile);
		} else {
			g_warning ("Invalid file of type '%s' found. File '%s' was skipped.", content_type, fname);
		}
		g_object_unref (info);
		g_object_unref (infile);

		if (!ret)
			break;
	}
	g_ptr_array_unref (yaml_files);

	return ret;
}

static void
as_provider_dep11_class_init (AsProviderDEP11Class * klass)
{
	as_provider_dep11_parent_class = g_type_class_peek_parent (klass);
	AS_DATA_PROVIDER_CLASS (klass)->execute = as_provider_dep11_real_execute;
}

static void
as_provider_dep11_instance_init (AsProviderDEP11 *dprov)
{
}

/**
 * as_provider_dep11_get_type:
 */
GType
as_provider_dep11_get_type (void)
{
	static volatile gsize as_provider_dep11_type_id__volatile = 0;
	if (g_once_init_enter (&as_provider_dep11_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
					sizeof (AsProviderDEP11Class),
					(GBaseInitFunc) NULL,
					(GBaseFinalizeFunc) NULL,
					(GClassInitFunc) as_provider_dep11_class_init,
					(GClassFinalizeFunc) NULL,
					NULL,
					sizeof (AsProviderDEP11),
					0,
					(GInstanceInitFunc) as_provider_dep11_instance_init,
					NULL
		};
		GType as_provider_dep11_type_id;
		as_provider_dep11_type_id = g_type_register_static (AS_TYPE_DATA_PROVIDER, "AsProviderDEP11", &g_define_type_info, 0);
		g_once_init_leave (&as_provider_dep11_type_id__volatile, as_provider_dep11_type_id);
	}
	return as_provider_dep11_type_id__volatile;
}
