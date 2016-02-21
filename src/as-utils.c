/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C)      2014 Richard Hughes <richard@hughsie.com>
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

#include "as-utils.h"

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <sys/types.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>

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
	if (root == NULL) {
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
				if (iter2->type != XML_ELEMENT_NODE)
					continue;
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

/**
 * as_str_empty:
 * @str: The string to test.
 *
 * Test if a C string is NULL or empty (containing only spaces).
 */
gboolean
as_str_empty (const gchar* str)
{
	if ((str == NULL) || (g_strcmp0 (str, "") == 0))
		return TRUE;
	return FALSE;
}

/**
 * as_iso8601_to_datetime:
 *
 * Helper function to work around a bug in g_time_val_from_iso8601.
 * Can be dropped when the bug gets resolved upstream:
 * https://bugzilla.gnome.org/show_bug.cgi?id=760983
 **/
GDateTime*
as_iso8601_to_datetime (const gchar *iso_date)
{
	GTimeVal tv;
	guint dmy[] = {0, 0, 0};

	/* nothing set */
	if (iso_date == NULL || iso_date[0] == '\0')
		return NULL;

	/* try to parse complete ISO8601 date */
	if (g_strstr_len (iso_date, -1, " ") != NULL) {
		if (g_time_val_from_iso8601 (iso_date, &tv) && tv.tv_sec != 0)
			return g_date_time_new_from_timeval_utc (&tv);
	}

	/* g_time_val_from_iso8601() blows goats and won't
	 * accept a valid ISO8601 formatted date without a
	 * time value - try and parse this case */
	if (sscanf (iso_date, "%u-%u-%u", &dmy[0], &dmy[1], &dmy[2]) != 3)
		return NULL;

	/* create valid object */
	return g_date_time_new_utc (dmy[0], dmy[1], dmy[2], 0, 0, 0);
}

/**
 * as_utils_delete_dir_recursive:
 * @dirname: Directory to remove
 *
 * Remove directory and all its children (like rm -r does).
 *
 * Returns: %TRUE if operation was successful
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
 * Returns: (element-type AsCategory) (transfer full): #GPtrArray of #AsCategory objects matching the strings in the array
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

/**
 * as_utils_find_files_matching:
 */
GPtrArray*
as_utils_find_files_matching (const gchar* dir, const gchar* pattern, gboolean recursive, GError **error)
{
	GPtrArray* list;
	GFileInfo *file_info;
	GFileEnumerator *enumerator = NULL;
	GFile *fdir;
	GError *tmp_error = NULL;
	g_return_val_if_fail (dir != NULL, NULL);
	g_return_val_if_fail (pattern != NULL, NULL);

	list = g_ptr_array_new_with_free_func (g_free);
	fdir =  g_file_new_for_path (dir);
	enumerator = g_file_enumerate_children (fdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &tmp_error);
	if (tmp_error != NULL)
		goto out;

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &tmp_error)) != NULL) {
		gchar *path;

		if (tmp_error != NULL)
			goto out;
		if (g_file_info_get_is_hidden (file_info))
			continue;

		path = g_build_filename (dir,
					 g_file_info_get_name (file_info),
					 NULL);

		if ((!g_file_test (path, G_FILE_TEST_IS_REGULAR)) && (recursive)) {
			GPtrArray *subdir_list;
			guint i;
			subdir_list = as_utils_find_files_matching (path, pattern, recursive, &tmp_error);
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


out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
	if (tmp_error != NULL) {
		if (error == NULL)
			g_debug ("Error while searching for files in %s: %s", dir, tmp_error->message);
		else
			g_propagate_error (error, tmp_error);
		g_ptr_array_unref (list);
		return NULL;
	}

	return list;
}

/**
 * as_utils_find_files:
 */
GPtrArray*
as_utils_find_files (const gchar *dir, gboolean recursive, GError **error)
{
	GPtrArray* res = NULL;
	g_return_val_if_fail (dir != NULL, NULL);

	res = as_utils_find_files_matching (dir, "", recursive, error);
	return res;
}

/**
 * as_utils_is_root:
 */
gboolean
as_utils_is_root (void)
{
	uid_t vuid;
	vuid = getuid ();
	return (vuid == ((uid_t) 0));
}

/**
 * as_utils_is_writable:
 * @path: the path to check.
 *
 * Checks if a path is writable.
 */
gboolean
as_utils_is_writable (const gchar *path)
{
	g_autoptr(GFile) file;
	g_autoptr(GFileInfo) file_info;

	file = g_file_new_for_path (path);

	file_info = g_file_query_info (
		file,
		G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		NULL);

	if (file_info && g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
		return g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

	return FALSE;
}


/**
 * as_get_current_locale:
 *
 * Returns a locale string as used in the AppStream specification.
 *
 * Returns: (transfer full): A locale string, free with g_free()
 */
gchar*
as_get_current_locale (void)
{
	const gchar * const *locale_names;
	gchar *tmp;
	gchar *locale;

	locale_names = g_get_language_names ();
	/* set active locale without UTF-8 suffix */
	locale = g_strdup (locale_names[0]);
	tmp = g_strstr_len (locale, -1, ".UTF-8");
	if (tmp != NULL)
		*tmp = '\0';

	return locale;
}

/**
 * as_ptr_array_to_strv:
 * @array: (element-type utf8)
 *
 * Returns: (transfer full): strv of the string array
 */
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

/**
 * as_str_replace:
 */
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

/**
 * as_touch_location:
 * @fname: The file or directory to touch.
 *
 * Change mtime of a filesystem location.
 */
gboolean
as_touch_location (const gchar *fname)
{
	struct stat sb;
	struct utimbuf new_times;

	if (stat (fname, &sb) < 0)
		return FALSE;

	new_times.actime = sb.st_atime;
	new_times.modtime = time (NULL);
	if (utime (fname, &new_times) < 0)
		return FALSE;

	return TRUE;
}
