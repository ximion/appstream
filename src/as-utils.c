/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C)      2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include "as-utils.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <sys/types.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "as-category.h"

/**
 * SECTION:as-utils
 * @short_description: Helper functions that are used inside libappstream
 * @include: appstream.h
 *
 * Functions which are used in libappstream and might be useful for others
 * as well.
 */

/**
 * as_description_markup_convert_simple:
 * @markup: the text to copy.
 *
 * Converts an XML description markup into a simple printable form.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 **/
gchar*
as_description_markup_convert_simple (const gchar *markup)
{
	xmlDoc *doc = NULL;
	xmlNode *root;
	xmlNode *iter;
	gboolean ret = TRUE;
	gchar *xmldata;
	gchar *formatted = NULL;
	GString *str = NULL;

	/* is this actually markup */
	if (g_strrstr (markup, "<") == NULL) {
		formatted = g_strdup (markup);
		goto out;
	}

	/* make XML parser happy by providing a root element */
	xmldata = g_strdup_printf ("<root>%s</root>", markup);

	doc = xmlParseDoc ((xmlChar*) xmldata);
	if (doc == NULL) {
		ret = FALSE;
		goto out;
	}

	root = xmlDocGetRootElement (doc);
	if (doc == NULL) {
		/* document was empty */
		ret = FALSE;
		goto out;
	}

	str = g_string_new ("");
	for (iter = root->children; iter != NULL; iter = iter->next) {
		gchar *content;
		xmlNode *iter2;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "p") == 0) {
			content = (gchar*) xmlNodeGetContent (iter);
			if (str->len > 0)
				g_string_append (str, "\n");
			g_string_append_printf (str, "%s\n", content);
			g_free (content);
		} else if ((g_strcmp0 ((gchar*) iter->name, "ul") == 0) || (g_strcmp0 ((gchar*) iter->name, "ol") == 0)) {
			/* iterate over itemize contents */
			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (g_strcmp0 ((gchar*) iter2->name, "li") == 0) {
					content = (gchar*) xmlNodeGetContent (iter2);
					g_string_append_printf (str,
								" • %s\n",
								content);
					g_free (content);
				} else {
					/* only <li> is valid in lists */
					ret = FALSE;
					goto out;
				}
			}
		} else {
			/* only <p>, <ul> and <ol> is valid here */
			/* we might catch that as proper GError later */
		}
	}

	/* success */
	if (str->len > 0)
		g_string_truncate (str, str->len - 1);
out:
	if (doc != NULL)
		xmlFreeDoc (doc);
	if (!ret)
		formatted = g_strdup (markup);
	if (str != NULL) {
		if (!ret)
			g_string_free (str, TRUE);
		else
			formatted = g_string_free (str, FALSE);
	}
	return formatted;
}

gboolean
as_str_empty (const gchar* str)
{
	if ((str == NULL) || (g_strcmp0 (str, "") == 0))
		return TRUE;
	return FALSE;
}

gboolean
as_utils_touch_dir (const gchar* dirname)
{
	GFile *d = NULL;
	GError *error = NULL;
	g_return_val_if_fail (dirname != NULL, FALSE);

	d = g_file_new_for_path (dirname);
	if (!g_file_query_exists (d, NULL)) {
		g_file_make_directory_with_parents (d, NULL, &error);
		if (error != NULL) {
			g_critical ("Unable to create directory tree. Error: %s", error->message);
			g_error_free (error);
			return FALSE;
		}
	}
	g_object_unref (d);

	return TRUE;
}

/**
 * as_utils_delete_dir_recursive:
 * @dirname: Directory to remove
 *
 * Remove folder like rm -r does
 *
 * Returns: TRUE if operation was successful
 */
gboolean
as_utils_delete_dir_recursive (const gchar* dirname)
{
	GError *error = NULL;
	gboolean ret = FALSE;
	GFile *dir;
	GFileEnumerator *enr;
	GFileInfo *info;
	g_return_val_if_fail (dirname != NULL, FALSE);

	if (!g_file_test (dirname, G_FILE_TEST_IS_DIR))
		return TRUE;

	dir = g_file_new_for_path (dirname);
	enr = g_file_enumerate_children (dir, "standard::name", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
	if (error != NULL)
		goto out;

	if (enr == NULL)
		goto out;
	info = g_file_enumerator_next_file (enr, NULL, &error);
	if (error != NULL)
		goto out;
	while (info != NULL) {
		gchar *path;
		path = g_build_filename (dirname, g_file_info_get_name (info), NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			as_utils_delete_dir_recursive (path);
		} else {
			g_remove (path);
		}
		g_object_unref (info);
		info = g_file_enumerator_next_file (enr, NULL, &error);
		if (error != NULL)
			goto out;
	}
	if (g_file_test (dirname, G_FILE_TEST_EXISTS)) {
		g_rmdir (dirname);
	}
	ret = TRUE;

out:
	g_object_unref (dir);
	if (enr != NULL)
		g_object_unref (enr);
	if (error != NULL) {
		g_critical ("Could not remove directory: %s", error->message);
		g_error_free (error);
	}
	return ret;
}

/**
 * as_utils_categories_from_strv:
 * @categories_strv: a string array
 * @system_categories: list of #AsCategory objects available on this system
 *
 * Create a list of categories from string array
 *
 * Returns: (element-type AsCategory) (transfer full): #GPtrArray of #AsCategory objcts matching the strings in the array
 */
GPtrArray*
as_utils_categories_from_strv (gchar** categories_strv, GPtrArray* system_categories)
{
	GPtrArray *cat_list;
	guint i;

	g_return_val_if_fail (categories_strv != NULL, NULL);
	g_return_val_if_fail (system_categories != NULL, NULL);

	/* This should be done way smarter... */
	cat_list = g_ptr_array_new_with_free_func (g_object_unref);
	for (i = 0; categories_strv[i] != NULL; i++) {
		gchar *idstr;
		guint j;
		idstr = categories_strv[i];
		for (j = 0; j < system_categories->len; j++) {
			AsCategory *sys_cat;
			gchar *catname1;
			gchar *catname2;
			gchar *str;
			sys_cat = (AsCategory*) g_ptr_array_index (system_categories, j);
			catname1 = g_strdup (as_category_get_name (sys_cat));
			if (catname1 == NULL)
				continue;
			str = g_utf8_strdown (catname1, -1);
			g_free (catname1);
			catname1 = str;
			catname2 = g_utf8_strdown (idstr, -1);
			if (g_strcmp0 (catname1, catname2) == 0) {
				g_free (catname1);
				g_free (catname2);
				g_ptr_array_add (cat_list, g_object_ref (sys_cat));
				break;
			}
			g_free (catname1);
			g_free (catname2);
		}
	}

	return cat_list;
}

/**
 * as_utils_categories_from_str:
 * @categories_str: string with semicolon-separated categories
 * @system_categories: list of #AsCategory objects available on this system
 *
 * Create a list of categories from semicolon-separated string
 *
 * Returns: (element-type AsCategory) (transfer full): #GPtrArray of #AsCategory objcts matching the strings in the array
 */
GPtrArray*
as_utils_categories_from_str (const gchar* categories_str, GPtrArray* system_categories)
{
	gchar **cats;
	GPtrArray *cat_list;

	g_return_val_if_fail (categories_str != NULL, NULL);

	cats = g_strsplit (categories_str, ";", 0);
	cat_list = as_utils_categories_from_strv (cats, system_categories);
	g_strfreev (cats);

	return cat_list;
}

GPtrArray*
as_utils_find_files_matching (const gchar* dir, const gchar* pattern, gboolean recursive)
{
	GPtrArray* list;
	GError *error = NULL;
	GFileInfo *file_info;
	GFileEnumerator *enumerator = NULL;
	GFile *fdir;
	g_return_val_if_fail (dir != NULL, NULL);
	g_return_val_if_fail (pattern != NULL, NULL);

	list = g_ptr_array_new_with_free_func (g_free);
	fdir =  g_file_new_for_path (dir);
	enumerator = g_file_enumerate_children (fdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
	if (error != NULL)
		goto out;

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL) {
		gchar *path;
		if (g_file_info_get_is_hidden (file_info))
			continue;
		path = g_build_filename (dir,
								 g_file_info_get_name (file_info),
								 NULL);
		if ((!g_file_test (path, G_FILE_TEST_IS_REGULAR)) && (recursive)) {
			GPtrArray *subdir_list;
			guint i;
			subdir_list = as_utils_find_files_matching (path, pattern, recursive);
			/* if there was an error, exit */
			if (subdir_list == NULL) {
				g_ptr_array_unref (list);
				list = NULL;
				g_free (path);
				goto out;
			}
			for (i=0; i<subdir_list->len; i++)
				g_ptr_array_add (list,
								 g_strdup ((gchar *) g_ptr_array_index (subdir_list, i)));
			g_ptr_array_unref (subdir_list);
		} else {
			if (!as_str_empty (pattern)) {
				if (!g_pattern_match_simple (pattern, g_file_info_get_name (file_info))) {
					g_free (path);
					continue;
				}
			}
			g_ptr_array_add (list, path);
		}
	}
	if (error != NULL)
		goto out;

out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
	if (error != NULL) {
		fprintf (stderr, "Error while finding files in directory %s: %s\n", dir, error->message);
		g_ptr_array_unref (list);
		return NULL;
	}

	return list;
}

GPtrArray*
as_utils_find_files (const gchar* dir, gboolean recursive)
{
	GPtrArray* res = NULL;
	g_return_val_if_fail (dir != NULL, NULL);

	res = as_utils_find_files_matching (dir, "", recursive);
	return res;
}


gboolean
as_utils_is_root (void)
{
	uid_t vuid;
	vuid = getuid ();
	return (vuid == ((uid_t) 0));
}

gchar*
as_string_strip (const gchar* str)
{
	gchar* result = NULL;
	gchar* _tmp0_ = NULL;
	g_return_val_if_fail (str != NULL, NULL);
	_tmp0_ = g_strdup (str);
	result = _tmp0_;
	g_strstrip (result);
	return result;
}

gchar**
as_ptr_array_to_strv (GPtrArray *array)
{
	gchar **value;
	const gchar *value_temp;
	guint i;

	g_return_val_if_fail (array != NULL, NULL);

	/* copy the array to a strv */
	value = g_new0 (gchar *, array->len + 1);
	for (i=0; i<array->len; i++) {
		value_temp = (const gchar *) g_ptr_array_index (array, i);
		value[i] = g_strdup (value_temp);
	}

	return value;
}

gchar*
as_str_replace (const gchar *str, const gchar *old, const gchar *new)
{
	gchar *ret, *r;
	const gchar *p, *q;
	size_t oldlen = strlen(old);
	size_t count, retlen, newlen = strlen(new);

	if (oldlen != newlen) {
		for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
			count++;
		/* this is undefined if p - str > PTRDIFF_MAX */
		retlen = p - str + strlen(p) + count * (newlen - oldlen);
	} else
		retlen = strlen(str);

	if ((ret = malloc(retlen + 1)) == NULL)
		return NULL;

	for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
		/* this is undefined if q - p > PTRDIFF_MAX */
		ptrdiff_t l = q - p;
		memcpy(r, p, l);
		r += l;
		memcpy(r, new, newlen);
		r += newlen;
	}
	strcpy(r, p);

	return ret;
}
