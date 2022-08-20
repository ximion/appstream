/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
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
#include "as-utils-private.h"

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
#include <sys/utsname.h>
#include <sys/stat.h>
#include <errno.h>

#include "as-version.h"
#include "as-resources.h"
#include "as-category.h"
#include "as-metadata.h"
#include "as-component-private.h"

/**
 * SECTION:as-utils
 * @short_description: Helper functions that are used inside libappstream
 * @include: appstream.h
 *
 * Functions which are used in libappstream and might be useful for others
 * as well.
 */


/**
 * as_get_appstream_version:
 *
 * Get the version of the AppStream library that is currently used
 * as a string.
 *
 * Returns: The AppStream version.
 */
const gchar*
as_get_appstream_version (void)
{
	return as_version_string ();
}

/**
 * as_utils_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.14.0
 **/
G_DEFINE_QUARK (as-utils-error-quark, as_utils_error)

/**
 * as_markup_strsplit_words:
 * @text: the text to split.
 * @line_len: the maximum length of the output line
 *
 * Splits up a long line into an array of smaller strings, each being no longer
 * than @line_len. Words are not split.
 *
 * Returns: (transfer full): lines, or %NULL in event of an error
 *
 * Since: 0.14.0
 **/
gchar**
as_markup_strsplit_words (const gchar *text, guint line_len)
{
	GPtrArray *lines;
	g_autoptr(GString) curline = NULL;
	g_auto(GStrv) tokens = NULL;
	gsize curline_char_count = 0;

	/* sanity check */
	if (text == NULL)
		return NULL;
	if (text[0] == '\0')
		return g_strsplit (text, " ", -1);
	if (line_len == 0)
		return NULL;

	lines = g_ptr_array_new ();
	curline = g_string_new ("");

	/* tokenize the string */
	tokens = g_strsplit (text, " ", -1);
	for (guint i = 0; tokens[i] != NULL; i++) {
		gsize token_unilen = g_utf8_strlen (tokens[i], -1);
		gboolean token_has_linebreak = g_strstr_len (tokens[i], -1, "\n") != NULL;

		/* current line plus new token is okay */
		if (curline_char_count + token_unilen < line_len) {
			/* we can't just check for a suffix \n here, as tokens may contain internal linebreaks */
			if (token_has_linebreak) {
				if (tokens[i][0] == '\0')
					g_string_append_c (curline, ' ');
				else
					g_string_append_printf (curline, "%s ", tokens[i]);
				g_ptr_array_add (lines, g_strdup (curline->str));
				g_string_truncate (curline, 0);
				curline_char_count = 0;
			} else {
				g_string_append_printf (curline, "%s ", tokens[i]);
				curline_char_count += token_unilen + 1;
			}
			continue;
		}

		/* too long, so remove space, add newline and dump */
		if (curline->len > 0) {
			g_string_truncate (curline, curline->len - 1);
			curline_char_count -= 1;
		}

		g_string_append (curline, "\n");
		g_ptr_array_add (lines, g_strdup (curline->str));
		g_string_truncate (curline, 0);
		curline_char_count = 0;
		if (token_has_linebreak) {
			g_ptr_array_add (lines, g_strdup (tokens[i]));
		} else {
			g_string_append_printf (curline, "%s ", tokens[i]);
			curline_char_count += token_unilen + 1;
		}
	}

	/* any incomplete line? */
	if (curline->len > 0) {
		g_string_truncate (curline, curline->len - 1);
		g_string_append (curline, "\n");
		g_ptr_array_add (lines, g_strdup (curline->str));
	}

	/* any superfluous linebreak at the start?
	 * (this may happen if text is just one, very long word with no spaces) */
	if (lines->len > 0) {
		gchar *first_line = (gchar*) g_ptr_array_index (lines, 0);
		if (!g_str_has_prefix (text, "\n") && g_strcmp0 (first_line, "\n") == 0)
			g_ptr_array_remove_index (lines, 0);
	}

	g_ptr_array_add (lines, NULL);
	return (gchar **) g_ptr_array_free (lines, FALSE);
}

/**
 * as_sanitize_text_spaces:
 * @text: The text to sanitize.
 *
 * Sanitize a text string by removing extra whitespaces and all
 * linebreaks. This is ideal to run on a text obtained from XML
 * prior to passing it through to a word-wrapping function.
 *
 * Returns: The sanitized string.
 */
gchar*
as_sanitize_text_spaces (const gchar *text)
{
	g_auto(GStrv) strv = NULL;

	if (text == NULL)
		return NULL;

	strv = g_strsplit (text, "\n", -1);
	for (guint i = 0; strv[i] != NULL; ++i)
		g_strstrip (strv[i]);
	return g_strstrip (g_strjoinv (" ", strv));
}

/**
 * as_description_markup_convert:
 * @markup: the XML markup to transform.
 * @error: A #GError or %NULL.
 *
 * Converts XML description markup into other forms of markup.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string.
 **/
gchar*
as_description_markup_convert (const gchar *markup, AsMarkupKind to_kind, GError **error)
{
	xmlDoc *doc = NULL;
	xmlNode *root;
	xmlNode *iter;
	gboolean ret = TRUE;
	gchar *formatted = NULL;
	GString *str = NULL;
	g_autofree gchar *xmldata = NULL;

	if (markup == NULL)
		return NULL;

	/* is this actually markup */
	if (g_strrstr (markup, "<") == NULL) {
		formatted = g_strdup (markup);
		goto out;
	}

	/* if we already have XML, do nothing */
	if (to_kind == AS_MARKUP_KIND_XML)
		goto out;

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
		xmlNode *iter2;
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "p") == 0) {
			g_autofree gchar *clean_text = NULL;
			g_autofree gchar *text_content = (gchar*) xmlNodeGetContent (iter);

			/* Apparently the element is empty, which is odd. But we better add it instead
			 * of completely ignoring it. */
			if (text_content == NULL)
				text_content = g_strdup ("");

			/* remove extra whitespaces and linebreaks */
			clean_text = as_sanitize_text_spaces (text_content);

			if (str->len > 0)
				g_string_append (str, "\n");

			if (to_kind == AS_MARKUP_KIND_MARKDOWN) {
				g_auto(GStrv) spl = as_markup_strsplit_words (clean_text, 100);
				if (spl != NULL) {
					for (guint i = 0; spl[i] != NULL; i++)
						g_string_append (str, spl[i]);
				}
			} else {
				g_string_append_printf (str, "%s\n", clean_text);
			}
		} else if ((g_strcmp0 ((gchar*) iter->name, "ul") == 0) || (g_strcmp0 ((gchar*) iter->name, "ol") == 0)) {
			g_autofree gchar *item_c = NULL;
			gboolean is_ordered_list = g_strcmp0 ((gchar*) iter->name, "ol") == 0;
			guint entry_no = 0;

			/* set item style for unordered lists */
			if (!is_ordered_list) {
				if (to_kind == AS_MARKUP_KIND_MARKDOWN)
					item_c = g_strdup ("*");
				else
					item_c = g_strdup ("•");
			}

			/* iterate over itemize contents */
			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				if (iter2->type != XML_ELEMENT_NODE)
					continue;
				if (g_strcmp0 ((gchar*) iter2->name, "li") == 0) {
					g_auto(GStrv) spl = NULL;
					g_autofree gchar *clean_item = NULL;
					g_autofree gchar *item_content = (gchar*) xmlNodeGetContent (iter2);
					entry_no++;

					/* Apparently the item text is empty, which is odd.
					 * Let's add an empty entry, instead of ignoring it entirely. */
					if (item_content == NULL)
						item_content = g_strdup ("");

					/* remove extra whitespaces and linebreaks */
					clean_item = as_sanitize_text_spaces (item_content);

					/* set item number for ordered list */
					if (is_ordered_list) {
						g_free (item_c);
						item_c = g_strdup_printf ("%u.", entry_no);
					}

					/* break to 100 chars, leaving room for the dot/indent */
					spl = as_markup_strsplit_words (clean_item, 100 - 4);
					if (spl != NULL) {
						g_string_append_printf (str, " %s %s", item_c, spl[0]);
						for (guint i = 1; spl[i] != NULL; i++)
							g_string_append_printf (str, "   %s", spl[i]);
					}
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
 * as_description_markup_convert_simple:
 * @markup: the XML markup to transform.
 * @error: A #GError or %NULL.
 *
 * Converts an XML description markup into a simple printable form.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string.
 **/
gchar*
as_markup_convert_simple (const gchar *markup, GError **error)
{
	return as_description_markup_convert (markup, AS_MARKUP_KIND_TEXT, error);
}

/**
 * as_is_empty:
 * @str: The string to test.
 *
 * Test if a C string is %NULL or empty (contains only terminating null character).
 *
 * Returns: %TRUE if string was empty.
 */
gboolean
as_is_empty (const gchar *str)
{
	if ((str == NULL) || (str[0] == '\0'))
		return TRUE;
	return FALSE;
}

/**
 * as_iso8601_to_datetime:
 *
 * Helper function to work around a bug in g_date_time_new_from_iso8601.
 * Can be dropped when the bug gets resolved upstream:
 * https://bugzilla.gnome.org/show_bug.cgi?id=760983
 **/
GDateTime*
as_iso8601_to_datetime (const gchar *iso_date)
{
	guint dmy[] = {0, 0, 0};

	/* nothing set */
	if (iso_date == NULL || iso_date[0] == '\0')
		return NULL;

	/* try to parse complete ISO8601 date */
	if (g_strstr_len (iso_date, -1, "T") != NULL) {
		g_autoptr(GTimeZone) tz_utc = g_time_zone_new_utc ();
		GDateTime *res = g_date_time_new_from_iso8601 (iso_date, tz_utc);
		if (res != NULL)
			return res;
	}

	/* g_date_time_new_from_iso8601() blows goats and won't
	 * accept a valid ISO8601 formatted date without a
	 * time value - try and parse this case */
	if (sscanf (iso_date, "%u-%u-%u", &dmy[0], &dmy[1], &dmy[2]) != 3)
		return NULL;

	/* create valid object */
	return g_date_time_new_utc (dmy[0], dmy[1], dmy[2], 0, 0, 0);
}

/**
 * as_str_verify_integer:
 * @str: The string to test.
 * @min_value: Minimal value to be considered valid, e.g. %G_MININT64
 * @max_value: Maximum value to be considered valie, e.g. %G_MAXINT64
 *
 * Verify that a string is an integer in the given range.
 * Unlike strtoll(), this function will only pass if the whole string
 * consists of numbers, and will not succeed if the string has a text suffix.
 *
 * Returns: %TRUE if verified correctly according to the set conditions, %FALSE otherwise.
 */
gboolean
as_str_verify_integer (const gchar *str, gint64 min_value, gint64 max_value)
{
	gchar *endptr;
	gint64 res;
	g_return_val_if_fail (min_value < max_value, FALSE);

	if (as_is_empty (str))
		return FALSE;

	res = g_ascii_strtoll (str, &endptr, 10);
	/* check if we used the whole string for conversion */
	if (endptr[0] != '\0')
		return FALSE;

	return res >= min_value && res <= max_value;
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
	g_autoptr(GFileInfo) info = NULL;
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
		g_autofree gchar *path = g_build_filename (dirname, g_file_info_get_name (info), NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR))
			as_utils_delete_dir_recursive (path);
		else
			g_remove (path);
		g_object_unref (info);
		info = g_file_enumerator_next_file (enr, NULL, &error);
		if (error != NULL)
			goto out;
	}
	if (g_file_test (dirname, G_FILE_TEST_EXISTS))
		g_rmdir (dirname);
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
 * as_utils_find_files_matching:
 */
GPtrArray*
as_utils_find_files_matching (const gchar* dir, const gchar* pattern, gboolean recursive, GError **error)
{
	GPtrArray *list;
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
		g_autofree gchar *path = NULL;

		if (tmp_error != NULL) {
			g_object_unref (file_info);
			break;
		}
		if (g_file_info_get_is_hidden (file_info)) {
			g_object_unref (file_info);
			continue;
		}

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
				g_object_unref (file_info);
				break;
			}
			for (i=0; i<subdir_list->len; i++)
				g_ptr_array_add (list,
						 g_strdup ((gchar *) g_ptr_array_index (subdir_list, i)));
			g_ptr_array_unref (subdir_list);
		} else {
			if (!as_is_empty (pattern)) {
				if (!g_pattern_match_simple (pattern, g_file_info_get_name (file_info))) {
					g_object_unref (file_info);
					continue;
				}
			}
			g_ptr_array_add (list, path);
			path = NULL;
		}

		g_object_unref (file_info);
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
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFileInfo) file_info = NULL;

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
 * Returns the current locale string in the format
 * used by the AppStream.
 *
 * Returns: (transfer full): A locale string, free with g_free()
 */
gchar*
as_get_current_locale (void)
{
	const gchar * const *locale_names;
	const gchar *locale = NULL;

	/* use LANGUAGE, LC_ALL, LC_MESSAGES and LANG */
	locale_names = g_get_language_names ();

	if (g_strstr_len (locale_names[0], -1, "_") == NULL) {
		/* The locale doesn't have a region code - see if LANG has more to offer. */
		const gchar *env_lang = g_getenv ("LANG");
		if ((env_lang != NULL) && (g_strstr_len (env_lang, -1, "_") != NULL))
			locale = env_lang;
	}
	if (locale == NULL)
		locale = locale_names[0];
	if (locale == NULL)
		locale = g_strdup ("C");

	/* return active locale without UTF-8 suffix, UTF-8 is default in AppStream */
	return as_locale_strip_encoding (locale);
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
	value = g_new0 (gchar*, array->len + 1);
	for (i = 0; i < array->len; i++) {
		value_temp = (const gchar*) g_ptr_array_index (array, i);
		value[i] = g_strdup (value_temp);
	}

	return value;
}

/**
 * as_gstring_replace2:
 * @string: a #GString
 * @find: the string to find in @string
 * @replace: the string to insert in place of @find
 * @limit: the maximum instances of @find to replace with @replace, or `0` for no limit
 *
 * Replaces the string @find with the string @replace in a #GString up to
 * @limit times. If the number of instances of @find in the #GString is
 * less than @limit, all instances are replaced. If @limit is `0`,
 * all instances of @find are replaced.
 *
 * Returns: the number of find and replace operations performed.
 **/
guint
as_gstring_replace2 (GString *string, const gchar *find, const gchar *replace, guint limit)
{
#if GLIB_CHECK_VERSION(2,68,0)
	return g_string_replace (string, find, replace, limit);
#else
	/* note: This is a direct copy from GLib upstream (with whitespace
	 * fixed spaces to tabs and with style fixed). Once we can depend on
	 * GLib 2.68, this copy should be dropped and g_string_replace() used
	 * instead.
	 *
	 * GLib is licensed under the LGPL-2.1+.
	 */
	gsize f_len, r_len, pos;
	gchar *cur, *next;
	guint n = 0;

	g_return_val_if_fail (string != NULL, 0);
	g_return_val_if_fail (find != NULL, 0);
	g_return_val_if_fail (replace != NULL, 0);

	f_len = strlen (find);
	r_len = strlen (replace);
	cur = string->str;

	while ((next = strstr (cur, find)) != NULL) {
		pos = next - string->str;
		g_string_erase (string, pos, f_len);
		g_string_insert (string, pos, replace);
		cur = string->str + pos + r_len;
		n++;
	}

	return n;
#endif /* !GLIB_CHECK_VERSION(2,68.0) */
}

/**
 * as_gstring_replace:
 * @string: a #GString
 * @find: the string to find in @string
 * @replace: the string to insert in place of @find
 *
 * Replaces all occurences of @find with the string @replace in a #GString.
 *
 * Returns: the number of find and replace operations performed.
 **/
guint
as_gstring_replace (GString *string, const gchar *find, const gchar *replace)
{
	return as_gstring_replace2 (string, find, replace, 0);
}

/**
 * as_str_replace:
 * @str: The string to operate on
 * @old_str: The old value to replace.
 * @new_str: The new value to replace @old_str with.
 * @limit: the maximum instances of @find to replace with @new_str, or `0` for
 * no limit
 *
 * Performs search & replace on the given string.
 *
 * Returns: A new string with the characters replaced.
 */
gchar*
as_str_replace (const gchar *str, const gchar *old_str, const gchar *new_str, guint limit)
{
	GString *gstr;

	gstr = g_string_new (str);
	as_gstring_replace2 (gstr, old_str, new_str, limit);
	return g_string_free (gstr, FALSE);
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

/**
 * as_reset_umask:
 *
 * Reset umask potentially set by the user to a default value,
 * so we can create files with the correct permissions.
 */
void
as_reset_umask (void)
{
	umask (0022);
}

/**
 * as_copy_file:
 *
 * Copy a file.
 */
gboolean
as_copy_file (const gchar *source, const gchar *destination, GError **error)
{
	FILE *fsrc, *fdest;
	int a;

	fsrc = fopen (source, "rb");
	if (fsrc == NULL) {
		g_set_error (error,
				G_FILE_ERROR,
				G_FILE_ERROR_FAILED,
				"Could not copy file: %s", g_strerror (errno));
		return FALSE;
	}

	fdest = fopen (destination, "wb");
	if (fdest == NULL) {
		g_set_error (error,
				G_FILE_ERROR,
				G_FILE_ERROR_FAILED,
				"Could not copy file: %s", g_strerror (errno));
		fclose (fsrc);
		return FALSE;
	}

	while (TRUE) {
		a = fgetc (fsrc);

		if (!feof (fsrc))
			fputc (a, fdest);
		else
			break;
	}

	fclose (fdest);
	fclose (fsrc);
	return TRUE;
}

/**
 * as_is_cruft_locale:
 *
 * Checks whether the given locale is valid or a cruft or dummy
 * locale.
 */
gboolean
as_is_cruft_locale (const gchar *locale)
{
	if (locale == NULL)
		return FALSE;
	if (g_strcmp0 (locale, "x-test") == 0)
		return TRUE;
	if (g_strcmp0 (locale, "xx") == 0)
		return TRUE;
	return FALSE;
}

/**
 * as_locale_strip_encoding:
 *
 * Remove the encoding from a locale string.
 * The function returns a newly allocated string.
 */
gchar*
as_locale_strip_encoding (const gchar *locale)
{
	return as_str_replace (locale, ".UTF-8", "", 1);
}

/**
 * as_get_current_arch:
 *
 * Get the current architecture as vendor strings
 * (e.g. "amd64" instead of "x86_64").
 *
 * Returns: (transfer full): The current OS architecture as string
 */
gchar*
as_get_current_arch (void)
{
	gchar *arch;
	struct utsname uts;

	uname (&uts);

	if (g_strcmp0 (uts.machine, "x86_64") == 0) {
		arch = g_strdup ("amd64");
	} else if (g_pattern_match_simple ("i?86", uts.machine)) {
		arch = g_strdup ("i386");
	} else if (g_strcmp0 (uts.machine, "aarch64")) {
		arch = g_strdup ("arm64");
	} else {
		arch = g_strdup (uts.machine);
	}

	return arch;
}

/**
 * as_arch_compatible:
 * @arch1: Architecture 1
 * @arch2: Architecture 2
 *
 * Compares two architectures and returns %TRUE if they are compatible.
 */
gboolean
as_arch_compatible (const gchar *arch1, const gchar *arch2)
{
	if (g_strcmp0 (arch1, arch2) == 0)
		return TRUE;
	if (g_strcmp0 (arch1, "all") == 0)
		return TRUE;
	if (g_strcmp0 (arch2, "all") == 0)
		return TRUE;
	return FALSE;
}

/**
 * as_utils_locale_to_language:
 * @locale: The locale string.
 *
 * Get language part from a locale string.
 */
gchar*
as_utils_locale_to_language (const gchar *locale)
{
	gchar *tmp;
	gchar *country_code;

	/* invalid */
	if (locale == NULL)
		return NULL;

	/* return the part before the _ (not always 2 chars!) */
	country_code = g_strdup (locale);
	tmp = g_strstr_len (country_code, -1, "_");
	if (tmp != NULL)
		*tmp = '\0';

	/* return the part before any "@" for locale with modifiers like "ca@valencia" */
	tmp = g_strstr_len (country_code, -1, "@");
	if (tmp != NULL)
		*tmp = '\0';

	return country_code;
}

/**
 * as_ptr_array_find_string:
 * @array: gchar* array
 * @value: string to find
 *
 * Finds a string in a pointer array.
 *
 * Returns: (nullable): the const string, or %NULL if not found
 **/
const gchar*
as_ptr_array_find_string (GPtrArray *array, const gchar *value)
{
	for (guint i = 0; i < array->len; i++) {
		const gchar *tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (tmp, value) == 0)
			return tmp;
	}
	return NULL;
}


/**
 * as_hash_table_keys_to_array:
 * @table: The hash table.
 * @array: The pointer array.
 *
 * Add keys of a hash table to a pointer array.
 * The hash table keys must be strings.
 */
void
as_hash_table_string_keys_to_array (GHashTable *table, GPtrArray *array)
{
	GHashTableIter iter;
	gpointer value;

	g_hash_table_iter_init (&iter, table);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		const gchar *str = (const gchar*) value;
		g_ptr_array_add (array, g_strdup (str));
	}
}

/**
 * as_utils_locale_is_compatible:
 * @locale1: a locale string, or %NULL
 * @locale2: a locale string, or %NULL
 *
 * Calculates if one locale is compatible with another.
 * When doing the calculation the locale and language code is taken into
 * account if possible.
 *
 * Returns: %TRUE if the locale is compatible.
 *
 * Since: 0.9.5
 **/
gboolean
as_utils_locale_is_compatible (const gchar *locale1, const gchar *locale2)
{
	g_autofree gchar *lang1 = as_utils_locale_to_language (locale1);
	g_autofree gchar *lang2 = as_utils_locale_to_language (locale2);

	/* we've specified "don't care" and locale unspecified */
	if (locale1 == NULL && locale2 == NULL)
		return TRUE;

	/* forward */
	if (locale1 == NULL && locale2 != NULL) {
		const gchar *const *locales = g_get_language_names ();
		return g_strv_contains (locales, locale2) ||
		       g_strv_contains (locales, lang2);
	}

	/* backwards */
	if (locale1 != NULL && locale2 == NULL) {
		const gchar *const *locales = g_get_language_names ();
		return g_strv_contains (locales, locale1) ||
		       g_strv_contains (locales, lang1);
	}

	/* both specified */
	if (g_strcmp0 (locale1, locale2) == 0)
		return TRUE;
	if (g_strcmp0 (locale1, lang2) == 0)
		return TRUE;
	if (g_strcmp0 (lang1, locale2) == 0)
		return TRUE;
	return FALSE;
}

/**
 * as_utils_search_token_valid:
 * @token: the search token
 *
 * Checks the search token if it is valid. Valid tokens are at least 3 chars in
 * length and do not contain markup.
 *
 * Returns: %TRUE is the search token was valid.
 **/
gboolean
as_utils_search_token_valid (const gchar *token)
{
	guint i;
	for (i = 0; token[i] != '\0'; i++) {
		if (token[i] == '<' ||
		    token[i] == '>' ||
		    token[i] == '(' ||
		    token[i] == ')')
			return FALSE;
	}
	if (i < 3)
		return FALSE;
	return TRUE;
}

/**
 * as_utils_ensure_resources:
 *
 * Perform a sanity check to ensure GResource can be loaded.
 */
void
as_utils_ensure_resources ()
{
	static GMutex mutex;
	GResource *resource = NULL;

	g_mutex_lock (&mutex);
	resource = as_get_resource ();
	if (resource == NULL)
		g_error ("Failed to load internal resources: as_get_resource() returned NULL!");
	g_mutex_unlock (&mutex);
}

/**
 * as_utils_is_category_id:
 * @category_name: a XDG category name, e.g. "ProjectManagement"
 *
 * Searches the known list of registered XDG category names.
 * See https://specifications.freedesktop.org/menu-spec/menu-spec-1.0.html#category-registry
 * for a reference.
 *
 * Returns: %TRUE if the category name is valid
 *
 * Since: 0.9.7
 **/
gboolean
as_utils_is_category_name (const gchar *category_name)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;
	GResource *resource = as_get_resource ();
	g_assert (resource != NULL);

	/* custom spec-extensions are generally valid if prefixed correctly */
	if (g_str_has_prefix (category_name, "X-"))
		return TRUE;

	/* load the readonly data section and look for the category name */
	data = g_resource_lookup_data (resource,
				       "/org/freedesktop/appstream/xdg-category-names.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", category_name);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_tld:
 * @tld: a top-level domain without dot, e.g. "de", "org", "name"
 *
 * Searches the known list of TLDs we allow for AppStream IDs.
 * This excludes internationalized names.
 *
 * Returns: %TRUE if the TLD is valid
 *
 * Since: 0.9.8
 **/
gboolean
as_utils_is_tld (const gchar *tld)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;
	GResource *resource = as_get_resource ();
	g_assert (resource != NULL);

	/* load the readonly data section and look for the TLD */
	data = g_resource_lookup_data (resource,
				       "/org/freedesktop/appstream/iana-filtered-tld-list.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", tld);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_desktop_environment:
 * @desktop: a desktop environment id.
 *
 * Searches the known list of desktop environments AppStream
 * knows about.
 *
 * Returns: %TRUE if the desktop-id is valid
 *
 * Since: 0.10.0
 **/
gboolean
as_utils_is_desktop_environment (const gchar *desktop)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;
	GResource *resource = as_get_resource ();
	g_assert (resource != NULL);

	/* load the readonly data section and look for the desktop environment name */
	data = g_resource_lookup_data (resource,
				       "/org/freedesktop/appstream/desktop-environments.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", desktop);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_platform_triplet_arch:
 * @arch: an architecture ID.
 *
 * Check if the given string is a valid architecture part
 * of a platform triplet.
 *
 * Returns: %TRUE if architecture is valid
 *
 * Since: 0.14.0
 **/
gboolean
as_utils_is_platform_triplet_arch (const gchar *arch)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;
	GResource *resource = NULL;

	if (arch == NULL)
		return FALSE;

	/* "any" is always a valid value */
	if (g_strcmp0 (arch, "any") == 0)
		return TRUE;

	resource = as_get_resource ();
	g_assert (resource != NULL);

	/* load the readonly data section */
	data = g_resource_lookup_data (resource,
				       "/org/freedesktop/appstream/platform_arch.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", arch);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_platform_triplet_oskernel:
 * @os: an OS/kernel ID.
 *
 * Check if the given string is a valid OS/kernel part
 * of a platform triplet.
 *
 * Returns: %TRUE if kernel ID is valid
 *
 * Since: 0.14.0
 **/
gboolean
as_utils_is_platform_triplet_oskernel (const gchar *os)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;
	GResource *resource;

	if (os == NULL)
		return FALSE;

	/* "any" is always a valid value */
	if (g_strcmp0 (os, "any") == 0)
		return TRUE;

	resource = as_get_resource ();
	g_assert (resource != NULL);

	/* load the readonly data section */
	data = g_resource_lookup_data (resource,
				       "/org/freedesktop/appstream/platform_os.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", os);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_platform_triplet_osenv:
 * @env: an OS/environment ID.
 *
 * Check if the given string is a valid OS/environment part
 * of a platform triplet.
 *
 * Returns: %TRUE if environment ID is valid
 *
 * Since: 0.14.0
 **/
gboolean
as_utils_is_platform_triplet_osenv (const gchar *env)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;
	GResource *resource;

	if (env == NULL)
		return FALSE;

	/* "any" is always a valid value */
	if (g_strcmp0 (env, "any") == 0)
		return TRUE;

	resource = as_get_resource ();
	g_assert (resource != NULL);

	/* load the readonly data section */
	data = g_resource_lookup_data (resource,
				       "/org/freedesktop/appstream/platform_env.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", env);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_platform_triplet:
 * @triplet: a platform triplet.
 *
 * Test if the given string is a valid platform triplet recognized by
 * AppStream.
 *
 * Returns: %TRUE if triplet is valid.
 *
 * Since: 0.14.0
 **/
gboolean
as_utils_is_platform_triplet (const gchar *triplet)
{
	g_auto(GStrv) parts = NULL;

	if (triplet == NULL)
		return FALSE;

	parts = g_strsplit (triplet, "-", 3);
	if (g_strv_length (parts) != 3)
		return FALSE;
	if (!as_utils_is_platform_triplet_arch (parts[0]))
		return FALSE;
	if (!as_utils_is_platform_triplet_oskernel (parts[1]))
		return FALSE;
	if (!as_utils_is_platform_triplet_osenv (parts[2]))
		return FALSE;
	return TRUE;
}

/**
 * as_utils_sort_components_into_categories:
 * @cpts: (element-type AsComponent): List of components.
 * @categories: (element-type AsCategory): List of categories to sort components into.
 * @check_duplicates: Whether to check for duplicates.
 *
 * Sorts all components in @cpts into the #AsCategory categories listed in @categories.
 */
void
as_utils_sort_components_into_categories (GPtrArray *cpts, GPtrArray *categories, gboolean check_duplicates)
{
	guint i;

	for (i = 0; i < cpts->len; i++) {
		guint j;
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));

		for (j = 0; j < categories->len; j++) {
			guint k;
			GPtrArray *children;
			gboolean added_to_main = FALSE;
			AsCategory *main_cat = AS_CATEGORY (g_ptr_array_index (categories, j));

			if (as_component_is_member_of_category (cpt, main_cat)) {
				if (!check_duplicates || !as_category_has_component (main_cat, cpt)) {
					as_category_add_component (main_cat, cpt);
					added_to_main = TRUE;
				}
			}

			/* fortunately, categories are only nested one level deep in all known cases.
			 * if this will ever change, we will need to adjust this code to go through
			 * a whole tree of categories, eww... */
			children = as_category_get_children (main_cat);
			for (k = 0; k < children->len; k++) {
				AsCategory *subcat = AS_CATEGORY (g_ptr_array_index (children, k));

				/* skip duplicates */
				if (check_duplicates && as_category_has_component (subcat, cpt))
					continue;

				if (as_component_is_member_of_category (cpt, subcat)) {
					as_category_add_component (subcat, cpt);
					if (!added_to_main) {
						if (!check_duplicates || !as_category_has_component (main_cat, cpt)) {
							as_category_add_component (main_cat, cpt);
						}
					}
				}
			}
		}
	}
}

static inline const gchar*
_as_fix_data_id_part (const gchar *tmp)
{
	if (tmp == NULL || tmp[0] == '\0')
		return AS_DATA_ID_WILDCARD;
	return tmp;
}

/**
 * as_utils_build_data_id:
 * @scope: Scope of the metadata as #AsComponentScope e.g. %AS_COMPONENT_SCOPE_SYSTEM
 * @bundle_kind: Bundling system providing this data, e.g. 'package' or 'flatpak'
 * @origin: Origin string, e.g. 'os' or 'gnome-apps-nightly'
 * @cid: AppStream component ID, e.g. 'org.freedesktop.appstream.cli'
 * @branch: Branch, e.g. '3-20' or 'master'
 *
 * Builds an identifier string unique to the individual dataset using the supplied information.
 *
 * Since: 0.14.0
 */
gchar*
as_utils_build_data_id (AsComponentScope scope,
			AsBundleKind bundle_kind,
			const gchar *origin,
			const gchar *cid,
			const gchar *branch)
{
	const gchar *scope_str = NULL;
	const gchar *bundle_str = NULL;

	/* if we have a package in system scope, the origin is "os", as they share the same namespace
	 * and we can not have multiple versions of the same software installed on the system.
	 * The data ID is needed to deduplicate entries */
	if (scope == AS_COMPONENT_SCOPE_SYSTEM && bundle_kind == AS_BUNDLE_KIND_PACKAGE)
		origin = "os";

	if (scope != AS_COMPONENT_SCOPE_UNKNOWN)
		scope_str = as_component_scope_to_string (scope);
	if (bundle_kind != AS_BUNDLE_KIND_UNKNOWN)
		bundle_str = as_bundle_kind_to_string (bundle_kind);

	/* build the data-id */
	return g_strdup_printf ("%s/%s/%s/%s/%s",
				_as_fix_data_id_part (scope_str),
				_as_fix_data_id_part (bundle_str),
				_as_fix_data_id_part (origin),
				_as_fix_data_id_part (cid),
				_as_fix_data_id_part (branch));
}

/**
 * as_utils_data_id_valid:
 * @data_id: a component data ID
 *
 * Checks if a data ID is valid i.e. has the correct number of
 * sections.
 *
 * Returns: %TRUE if the ID is valid
 *
 * Since: 0.14.0
 */
gboolean
as_utils_data_id_valid (const gchar *data_id)
{
	guint i;
	guint sections = 1;
	if (data_id == NULL)
		return FALSE;
	for (i = 0; data_id[i] != '\0'; i++) {
		if (data_id[i] == '/')
			sections++;
	}
	return sections == AS_DATA_ID_PARTS_COUNT;
}

/**
 * as_utils_data_id_get_cid:
 * @data_id: The data-id.
 *
 * Get the component-id part of the data-id.
 */
gchar*
as_utils_data_id_get_cid (const gchar *data_id)
{
	g_auto(GStrv) parts = NULL;

	parts = g_strsplit (data_id, "/", 5);
	if (g_strv_length (parts) != 5)
		return NULL;
	return g_strdup (parts[3]);
}

static inline guint
_as_utils_data_id_find_part (const gchar *str)
{
	guint i;
	for (i = 0; str[i] != '/' && str[i] != '\0'; i++);
	return i;
}

static inline gboolean
_as_utils_data_id_is_wildcard_part (const gchar *str, guint len)
{
	return len == 1 && str[0] == '*';
}

/**
 * as_utils_data_id_match:
 * @data_id1: a data ID
 * @data_id2: another data ID
 * @match_flags: a #AsDataIdMatchFlags bitfield, e.g. %AS_DATA_ID_MATCH_FLAG_ID
 *
 * Checks two data IDs for equality allowing globs to match, whilst also
 * allowing clients to whitelist sections that have to match.
 *
 * Returns: %TRUE if the IDs should be considered equal.
 *
 * Since: 0.14.0
 */
gboolean
as_utils_data_id_match (const gchar *data_id1,
			const gchar *data_id2,
			AsDataIdMatchFlags match_flags)
{
	guint last1 = 0;
	guint last2 = 0;
	guint len1;
	guint len2;

	/* trivial */
	if (data_id1 == data_id2)
		return TRUE;

	/* invalid */
	if (!as_utils_data_id_valid (data_id1) ||
	    !as_utils_data_id_valid (data_id2))
		return g_strcmp0 (data_id1, data_id2) == 0;

	/* look at each part */
	for (guint i = 0; i < AS_DATA_ID_PARTS_COUNT; i++) {
		const gchar *tmp1 = data_id1 + last1;
		const gchar *tmp2 = data_id2 + last2;

		/* find the slash or the end of the string */
		len1 = _as_utils_data_id_find_part (tmp1);
		len2 = _as_utils_data_id_find_part (tmp2);

		/* either string was a wildcard */
		if (match_flags & (1 << i) &&
		    !_as_utils_data_id_is_wildcard_part (tmp1, len1) &&
		    !_as_utils_data_id_is_wildcard_part (tmp2, len2)) {
			/* are substrings the same */
			if (len1 != len2)
				return FALSE;
			if (memcmp (tmp1, tmp2, len1) != 0)
				return FALSE;
		}

		/* advance to next section */
		last1 += len1 + 1;
		last2 += len2 + 1;
	}
	return TRUE;
}

/**
 * as_utils_data_id_equal:
 * @data_id1: a data ID
 * @data_id2: another data ID
 *
 * Checks two component data IDs for equality allowing globs to match.
 *
 * Returns: %TRUE if the ID's should be considered equal.
 *
 * Since: 0.14.0
 */
gboolean
as_utils_data_id_equal (const gchar *data_id1, const gchar *data_id2)
{
	return as_utils_data_id_match (data_id1,
				       data_id2,
				       AS_DATA_ID_MATCH_FLAG_SCOPE |
				       AS_DATA_ID_MATCH_FLAG_BUNDLE_KIND |
				       AS_DATA_ID_MATCH_FLAG_ORIGIN |
				       AS_DATA_ID_MATCH_FLAG_ID |
				       AS_DATA_ID_MATCH_FLAG_BRANCH);
}

/**
 * as_utils_data_id_hash:
 * @data_id: a data ID
 *
 * Converts a data-id to a hash value.
 *
 * This function implements the widely used DJB hash on the ID subset of the
 * data-id string.
 *
 * It can be passed to g_hash_table_new() as the hash_func parameter,
 * when using non-NULL strings or unique_ids as keys in a GHashTable.
 *
 * Returns: a hash value corresponding to the key
 *
 * Since: 0.14.0
 */
guint
as_utils_data_id_hash (const gchar *data_id)
{
	gsize i;
	guint hash = 5381;
	guint section_cnt = 0;

	/* not a unique ID */
	if (!as_utils_data_id_valid (data_id))
		return g_str_hash (data_id);

	/* only include the component-id */
	for (i = 0; data_id[i] != '\0'; i++) {
		if (data_id[i] == '/') {
			if (++section_cnt > 3)
				break;
			continue;
		}
		if (section_cnt < 3)
			continue;
		hash = (guint) ((hash << 5) + hash) + (guint) (data_id[i]);
	}
	return hash;
}

/**
 * as_utils_get_component_bundle_kind:
 *
 * Check which bundling system the component uses.
 */
AsBundleKind
as_utils_get_component_bundle_kind (AsComponent *cpt)
{
	GPtrArray *bundles;
	AsBundleKind bundle_kind = AS_BUNDLE_KIND_UNKNOWN;

	/* determine bundle - what should we do if there are multiple bundles of different types
	 * defined for one component? */
	if (as_component_has_package (cpt) ||
	    as_component_get_kind (cpt) == AS_COMPONENT_KIND_OPERATING_SYSTEM)
		bundle_kind = AS_BUNDLE_KIND_PACKAGE;
	bundles = as_component_get_bundles (cpt);
	if (bundles->len > 0)
		bundle_kind = as_bundle_get_kind (AS_BUNDLE (g_ptr_array_index (bundles, 0)));

	/* assume "package" for system-wide components from metainfo files */
	if (bundle_kind == AS_BUNDLE_KIND_UNKNOWN &&
	    as_component_get_scope (cpt) == AS_COMPONENT_SCOPE_SYSTEM &&
	    as_component_get_origin_kind (cpt) == AS_ORIGIN_KIND_METAINFO)
		return AS_BUNDLE_KIND_PACKAGE;

	return bundle_kind;
}

/**
 * as_utils_build_data_id_for_cpt:
 * @cpt: The component to build the ID for.
 *
 * Builds the unique metadata ID for component @cpt.
 */
gchar*
as_utils_build_data_id_for_cpt (AsComponent *cpt)
{
	AsBundleKind bundle_kind;

	/* determine bundle - what should we do if there are multiple bundles of different types
	 * defined for one component? */
	bundle_kind = as_utils_get_component_bundle_kind (cpt);

	/* build the data-id */
	return as_utils_build_data_id (as_component_get_scope (cpt),
				       bundle_kind,
				       as_component_get_origin (cpt),
				       as_component_get_id (cpt),
				       as_component_get_branch (cpt));
}

/**
 * as_utils_dns_to_rdns:
 *
 * Create a reverse-DNS ID based on a preexisting URL.
 */
gchar*
as_utils_dns_to_rdns (const gchar *url, const gchar *suffix)
{
	g_autofree gchar *tmp = NULL;
	gchar *pos = NULL;
	GString *new_cid = NULL;
	g_auto(GStrv) parts = NULL;
	guint i;

	tmp = g_strstr_len (url, -1, "://");
	if (tmp == NULL)
		tmp = g_strdup (url);
	else
		tmp = g_strdup (tmp + 3);

	pos = g_strstr_len (tmp, -1, "/");
	if (pos != NULL)
		pos[0] = '\0';

	parts = g_strsplit (tmp, ".", -1);
	if (parts == NULL)
		return NULL;

	new_cid = g_string_new (suffix);
	for (i = 0; parts[i] != NULL; i++) {
		if (g_strcmp0 (parts[i], "www") != 0) {
			g_string_prepend_c (new_cid, '.');
			g_string_prepend (new_cid, parts[i]);
		}
	}

	if (suffix == NULL)
		g_string_truncate (new_cid, new_cid->len - 1);

	return g_string_free (new_cid, FALSE);
}

/**
 * as_sort_components_by_score_cb:
 *
 * Helper method to sort result arrays by the #AsComponent match score
 * with higher scores appearing higher in the list.
 */
static gint
as_sort_components_by_score_cb (gconstpointer a, gconstpointer b)
{
	guint s1, s2;
	AsComponent *cpt1 = *((AsComponent **) a);
	AsComponent *cpt2 = *((AsComponent **) b);
	s1 = as_component_get_sort_score (cpt1);
	s2 = as_component_get_sort_score (cpt2);

	if (s1 > s2)
		return -1;
	if (s1 < s2)
		return 1;
	return 0;
}

/**
 * as_sort_components_by_score:
 *
 * Sort components by their (search) match score.
 */
void
as_sort_components_by_score (GPtrArray *cpts)
{
	g_ptr_array_sort (cpts, as_sort_components_by_score_cb);
}

/**
 * as_object_ptr_array_absorb:
 *
 * Append contents from source array of GObjects to destination array,
 * transferring ownership to the destination and removing values
 * from the source (effectively moving the data).
 * The source array will be empty afterwards.
 *
 * This function assumes that a GDestroyNotify function is set on the
 * GPtrArray if GLib < 2.58.
 */
void
as_object_ptr_array_absorb (GPtrArray *dest, GPtrArray *src)
{
	while (src->len != 0)
		g_ptr_array_add (dest, g_ptr_array_steal_index_fast (src, 0));
}

/**
 * as_ptr_array_to_str:
 *
 * Convert a string GPtrArray to a single string with
 * the array values separated by a separator.
 */
gchar*
as_ptr_array_to_str (GPtrArray *array, const gchar *separator)
{
	GString *str;
	if ((array == NULL) || (array->len == 0))
		return NULL;

	str = g_string_new ("");
	for (guint i = 0; i < array->len; i++) {
		g_string_append_printf (str,
					"%s%s",
					(const gchar*) g_ptr_array_index (array, i),
					separator);
	}
	if (str->len >= 1)
		g_string_truncate (str, str->len -1);

	return g_string_free (str, FALSE);
}

/**
 * as_filebasename_from_uri:
 *
 * Get the file basename from an URI.
 * This is the last component of the path, with any query or fragment
 * stripped off.
 *
 * Returns: The filename.
 */
gchar*
as_filebasename_from_uri (const gchar *uri)
{
	gchar *tmp;
	gchar *bname;

	if (uri == NULL)
		return NULL;
	bname = g_path_get_basename (uri);

	tmp = g_strstr_len (bname, -1, "?");
	if (tmp != NULL)
		tmp[0] = '\0';
	tmp = g_strstr_len (bname, -1, "#");
	if (tmp != NULL)
		tmp[0] = '\0';

	return bname;
}

/**
 * as_strstripnl:
 * @string: a string to remove surrounding whitespaces and newlines
 *
 * Removes newlines and whitespaces surrounding a string.
 *
 * This function doesn't allocate or reallocate any memory;
 * it modifies @string in place.
 *
 * As opposed to g_strstrip() this function also removes newlines
 * from the start and end of strings.
 *
 * Returns: @string
 */
gchar*
as_strstripnl (gchar *string)
{
	gsize len;
	guchar *start;
	if (string == NULL)
		return NULL;

	/* remove trailing whitespaces/newlines */
	len = strlen (string);
	while (len--) {
		const guchar c = string[len];
		if (g_ascii_isspace (c) || (c == '\n'))
			string[len] = '\0';
		else
			break;
	}

	/* remove leading whitespaces/newlines */
	for (start = (guchar*) string;
	     *start && (g_ascii_isspace (*start) || ((*start) == '\n'));
	     start++)
	;

	memmove (string, start, strlen ((gchar *) start) + 1);
	return string;
}

/**
 * as_ref_string_release:
 * @rstr: a #GRefString to release.
 *
 * This function works exactly like %g_ref_string_release, except
 * that it does not throw an error if %NULL is passed to it.
 */
void
as_ref_string_release (GRefString *rstr)
{
	if (rstr == NULL)
		return;
	g_ref_string_release (rstr);
}

/**
 * as_ref_string_assign_safe:
 * @rstr_ptr: (out): a #AsRefString
 * @str: a string, or a #AsRefString
 *
 * This function unrefs and clears @rstr_ptr if set, then sets @rstr if
 * non-NULL. If @rstr and @rstr_ptr are the same string the action is ignored.
 *
 * This function should be used when @str cannot be guaranteed to be a
 * refcounted string and is suitable for use in existing object setters.
 */
void
as_ref_string_assign_safe (GRefString **rstr_ptr, const gchar *str)
{
	g_return_if_fail (rstr_ptr != NULL);
	if (*rstr_ptr != NULL) {
		g_ref_string_release (*rstr_ptr);
		*rstr_ptr = NULL;
	}
	if (str != NULL)
		*rstr_ptr = g_ref_string_new_intern (str);
}

/**
 * as_ref_string_assign_transfer:
 * @rstr_ptr: (out): a #AsRefString
 * @new_rstr: a #AsRefString
 *
 * Clear the previous refstring in @rstr_ptr and move the new string @new_rstr in its place,
 * without increasing its reference count again.
 */
void
as_ref_string_assign_transfer (GRefString **rstr_ptr, GRefString *new_rstr)
{
	g_return_if_fail (rstr_ptr != NULL);
	if (*rstr_ptr != NULL) {
		g_ref_string_release (*rstr_ptr);
		*rstr_ptr = NULL;
	}
	if (new_rstr != NULL)
		*rstr_ptr = new_rstr;
}

/**
 * as_utils_extract_tarball:
 *
 * Internal helper function to extract a tarball with tar.
 */
gboolean
as_utils_extract_tarball (const gchar *filename, const gchar *target_dir, GError **error)
{
	g_autofree gchar *wdir = NULL;
	gboolean ret;
	gint exit_status;
	const gchar *argv[] = { "/bin/tar",
				"-xzf",
				filename,
				"-C",
				target_dir,
				NULL };

	g_return_val_if_fail (filename != NULL, FALSE);

	if (!as_utils_is_writable (target_dir)) {
		g_set_error_literal (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Can not extract tarball: target directory is not writable.");
		return FALSE;
	}

	wdir = g_path_get_dirname (filename);
	if (g_strcmp0 (wdir, ".") == 0)
		g_clear_pointer (&wdir, g_free);

	ret = g_spawn_sync (wdir,
			    (gchar**) argv,
			    NULL, /* envp */
			    G_SPAWN_CLOEXEC_PIPES,
			    NULL, /* child_setup */
			    NULL, /* child_setup udata */
			    NULL, /* stdout */
			    NULL, /* stderr */
			    &exit_status,
			    error);
	if (!ret) {
		g_prefix_error (error, "Unable to run tar: ");
		return FALSE;
	}
	if (exit_status == 0)
		return TRUE;

	g_set_error (error,
		     AS_UTILS_ERROR,
		     AS_UTILS_ERROR_FAILED,
		     "Tarball extraction failed with 'tar' exit-code %i.",
		     exit_status);
	return FALSE;
}

/**
 * as_metadata_location_get_prefix:
 */
static const gchar*
as_metadata_location_get_prefix (AsMetadataLocation location)
{
	if (location == AS_METADATA_LOCATION_SHARED)
		return "/usr/share";
	if (location == AS_METADATA_LOCATION_CACHE)
		return "/var/cache";
	if (location == AS_METADATA_LOCATION_STATE)
		return "/var/lib";
	if (location == AS_METADATA_LOCATION_USER)
		return g_get_user_data_dir ();
	return NULL;
}

/**
 * as_utils_install_metadata_file_internal:
 */
static gboolean
as_utils_install_metadata_file_internal (const gchar *filename,
					 const gchar *origin,
					 const gchar *dir,
					 const gchar *destdir,
					 gboolean is_yaml,
					 GError **error)
{
	gchar *tmp;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *path_dest = NULL;
	g_autofree gchar *path_parent = NULL;
	g_autoptr(GFile) file_dest = NULL;
	g_autoptr(GFile) file_src = NULL;

	/* create directory structure */
	path_parent = g_strdup_printf ("%s%s", destdir, dir);
	if (g_mkdir_with_parents (path_parent, 0755) != 0) {
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "Failed to create %s", path_parent);
		return FALSE;
	}

	/* calculate the new destination */
	file_src = g_file_new_for_path (filename);
	basename = g_path_get_basename (filename);
	if (origin != NULL) {
		g_autofree gchar *basename_new = NULL;
		tmp = g_strstr_len (basename, -1, ".");
		if (tmp == NULL) {
			g_set_error (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Name of metadata collection file is invalid %s",
				     basename);
			return FALSE;
		}
		basename_new = g_strdup_printf ("%s%s", origin, tmp);
		/* replace the fedora.xml.gz into %{origin}.xml.gz */
		path_dest = g_build_filename (path_parent, basename_new, NULL);
	} else {
		path_dest = g_build_filename (path_parent, basename, NULL);
	}

	/* actually copy file */
	file_dest = g_file_new_for_path (path_dest);
	if (!g_file_copy (file_src, file_dest,
			  G_FILE_COPY_OVERWRITE,
			  NULL, NULL, NULL, error))
		return FALSE;

	/* update the origin for XML files */
	if (origin != NULL && !is_yaml) {
		g_autoptr(AsMetadata) mdata = as_metadata_new ();
		as_metadata_set_locale (mdata, "ALL");
		if (!as_metadata_parse_file (mdata, file_dest, AS_FORMAT_KIND_XML, error))
			return FALSE;
		as_metadata_set_origin (mdata, origin);
		if (!as_metadata_save_collection (mdata, path_dest, AS_FORMAT_KIND_XML, error))
			return FALSE;
	}

	g_chmod (path_dest, 0755);
	return TRUE;
}

/**
 * as_utils_install_icon_tarball:
 */
static gboolean
as_utils_install_icon_tarball (AsMetadataLocation location,
				const gchar *filename,
				const gchar *origin,
				const gchar *size_id,
				const gchar *destdir,
				GError **error)
{
	g_autofree gchar *dir = NULL;
	dir = g_strdup_printf ("%s%s/swcatalog/icons/%s/%s",
			       destdir,
			       as_metadata_location_get_prefix (location),
			       origin,
			       size_id);
	if (g_mkdir_with_parents (dir, 0755) != 0) {
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "Failed to create %s", dir);
		return FALSE;
	}

	if (!as_utils_extract_tarball (filename, dir, error))
		return FALSE;
	return TRUE;
}

/**
 * as_utils_install_metadata_file:
 * @location: the #AsMetadataLocation, e.g. %AS_METADATA_LOCATION_CACHE
 * @filename: the full path of the file to install
 * @origin: the origin to use for the installation, or %NULL
 * @destdir: the destdir to use, or %NULL
 * @error: A #GError or %NULL
 *
 * Installs an AppStream MetaInfo, AppStream Metadata Collection or AppStream Icon tarball file
 * to the right place on the filesystem.
 * Please note that this function does almost no validation and may guess missing values such
 * as icon sizes and origin names.
 * Ensure your metadata is good before installing it.
 *
 * Returns: %TRUE for success, %FALSE if error is set
 *
 * Since: 0.14.0
 **/
gboolean
as_utils_install_metadata_file (AsMetadataLocation location,
				const gchar *filename,
				const gchar *origin,
				const gchar *destdir,
				GError **error)
{
	gboolean ret = FALSE;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *path = NULL;
	const gchar *icons_size_id = NULL;
	const gchar *icons_size_ids[] = { "48x48",
					  "48x48@2",
					  "64x64",
					  "64x64@2",
					  "128x128",
					  "128x128@2",
					  NULL };

	/* default value */
	if (destdir == NULL)
		destdir = "";
	if (location == AS_METADATA_LOCATION_USER)
		destdir = "";

	switch (as_metadata_file_guess_style (filename)) {
	case AS_FORMAT_STYLE_COLLECTION:
		if (g_strstr_len (filename, -1, ".yml.gz") != NULL) {
			path = g_build_filename (as_metadata_location_get_prefix (location),
						 "swcatalog", "yaml", NULL);
			ret = as_utils_install_metadata_file_internal (filename, origin, path, destdir, TRUE, error);
		} else {
			path = g_build_filename (as_metadata_location_get_prefix (location),
						 "swcatalog", "xml", NULL);
			ret = as_utils_install_metadata_file_internal (filename, origin, path, destdir, FALSE, error);
		}
		break;
	case AS_FORMAT_STYLE_METAINFO:
		if (location == AS_METADATA_LOCATION_CACHE || location == AS_METADATA_LOCATION_STATE) {
			g_set_error_literal (error,
					     AS_UTILS_ERROR,
					     AS_UTILS_ERROR_FAILED,
					     "System cache and state locations are unsupported for MetaInfo files");
			return FALSE;
		}
		path = g_build_filename (as_metadata_location_get_prefix (location),
					 "metainfo", NULL);
		ret = as_utils_install_metadata_file_internal (filename, NULL, path, destdir, FALSE, error);
		break;
	default:
		basename = g_path_get_basename (filename);

		if (g_str_has_suffix (basename, ".tar.gz")) {
			gchar *tmp;
			g_autofree gchar *tmp2 = NULL;
			/* we may have an icon tarball */

			/* guess icon size */
			for (guint i = 0; icons_size_ids[i] != NULL; i++) {
				if (g_strstr_len (basename, -1, icons_size_ids[i]) != NULL) {
					icons_size_id = icons_size_ids[i];
					break;
				}
			}

			if (icons_size_id == NULL) {
				g_set_error_literal (error,
					AS_UTILS_ERROR,
					AS_UTILS_ERROR_FAILED,
					"Unable to find valid icon size in icon tarball name.");
				return FALSE;
			}

			/* install icons if we know the origin name */
			if (origin != NULL) {
				ret = as_utils_install_icon_tarball (location, filename, origin, icons_size_id, destdir, error);
				break;
			}

			/* guess origin */
			tmp2 = g_strdup_printf ("_icons-%s.tar.gz", icons_size_id);
			tmp = g_strstr_len (basename, -1, tmp2);
			if (tmp != NULL) {
				*tmp = '\0';
				ret = as_utils_install_icon_tarball (location, filename, basename, icons_size_id, destdir, error);
				break;
			}
		}

		/* unrecognised */
		g_set_error_literal (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Can not process files of this type.");
		break;
	}

	return ret;
}

/**
 * as_get_user_cache_dir:
 *
 * Obtain the user-specific data cache directory for AppStream.
 *
 * Since: 0.14.2
 */
gchar*
as_get_user_cache_dir (GError **error)
{
	const gchar *cache_root = g_get_user_cache_dir ();
	if (cache_root == NULL) {
		gchar *tmp_dir = g_dir_make_tmp ("appstream-XXXXXX", error);
		if (tmp_dir == NULL) {
			/* something went very wrong here, we could neither get a user cache dir, nor
			 * access to a temporary directory in /tmp */
			return NULL;
		}
		return tmp_dir;
	} else {
		gchar *cache_dir = g_build_filename (cache_root, "appstream", NULL);
		g_mkdir_with_parents (cache_dir, 0755);
		return cache_dir;
	}
}

/**
 * as_unichar_accepted:
 *
 * Test if the unicode character is in the accepted set for
 * string values in AppStream.
 *
 * We permit any printable, non-spacing, format or zero-width space characters, as
 * well as enclosing marks and U+00AD SOFT HYPHEN
 */
gboolean
as_unichar_accepted (gunichar c)
{
	return g_unichar_isprint (c) || g_unichar_iszerowidth (c) || c == 173;
}

/**
 * as_random_alnum_string:
 * @len: Length of the generated string.
 *
 * Create a random alphanumeric (only ASCII letters and numbers)
 * string that can be used for tests and filenames.
 *
 * Returns: A random alphanumeric string.
 */
gchar*
as_random_alnum_string (gssize len)
{
	gchar *ret;
	static char alnum_plain_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"1234567890";

	ret = g_new0 (gchar, len + 1);
	for (gssize i = 0; i < len; i++)
		ret[i] = alnum_plain_chars[g_random_int_range(0, strlen (alnum_plain_chars))];

	return ret;
}

/**
 * as_utils_find_stock_icon_filename_full:
 * @root_dir: the directory to search in, including prefix.
 * @icon_name: the stock icon search name, e.g. "microphone.svg" or "kate"
 * @icon_size: the icon color, e.g. 64 or 128. If size is 0, the first found icon is returned.
 * @icon_scale the icon scaling factor, 1 for non HiDPI displays
 * @error: a #GError or %NULL
 *
 * Finds an icon filename in the filesystem that matches the given specifications.
 * This function may return a bigger icon than requested, which is suitable to be scaled down
 * to the actually requested size.
 * If no icon with the right scale factor is found, %NULL is returned.
 *
 * This algorithm does not implement the full Freedesktop icon theme specification,
 * instead is is designed to find 99% of all application icons quickly and
 * efficiently. It is not explicitly designed to find non-application stock
 * icons as well.
 * It also deliberately does not support legacy icon search locations and formats.
 * It will however work on incomplete directory trees with missing icon theme definition files,
 * by using a heuristic to find the right icon.
 * If you need more features, and have a complete icon theme definition installed, use the
 * icon-finding functions provided by GTK+ and Qt instead.
 *
 * If @icon_name is an absolute path, it will be returned unconditionally, as long
 * as the icon it references exists on the filesystem.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 *
 * Since: 0.14.5
 **/
gchar*
as_utils_find_stock_icon_filename_full (const gchar *root_dir,
					const gchar *icon_name,
					guint icon_size,
					guint icon_scale,
					GError **error)
{
	guint min_size_idx = 0;
	const gchar *supported_ext[] = { ".png",
					 ".svg",
					 ".svgz",
					 "",
					 NULL };
	const struct {
		guint size;
		const gchar *size_str;
	} sizes[] =  {
		{ 48,  "48x48" },
		{ 64,  "64x64" },
		{ 96,  "96x96" },
		{ 128, "128x128" },
		{ 256, "256x256" },
		{ 512, "512x512" },
		{ 0,   "scalable" },
		{ 0,   NULL }
	};
	const gchar *types[] = { "actions",
				 "animations",
				 "apps",
				 "categories",
				 "devices",
				 "emblems",
				 "emotes",
				 "filesystems",
				 "intl",
				 "mimetypes",
				 "places",
				 "status",
				 "stock",
				 NULL };
	g_autofree gchar *prefix = NULL;

	g_return_val_if_fail (icon_name != NULL, NULL);

	/* fallbacks & sanitizations */
	if (root_dir == NULL)
		root_dir = "";
	if (icon_scale <= 0)
		icon_scale = 1;
	if (icon_size > 512)
		icon_size = 512;

	/* is this an absolute path */
	if (icon_name[0] == '/') {
		g_autofree gchar *tmp = NULL;
		tmp = g_build_filename (root_dir, icon_name, NULL);
		if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
			g_set_error (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "specified icon '%s' does not exist",
				     icon_name);
			return NULL;
		}
		return g_strdup (tmp);
	}

	/* detect prefix */
	prefix = g_build_filename (root_dir, "usr", NULL);
	if (!g_file_test (prefix, G_FILE_TEST_EXISTS)) {
		g_free (prefix);
		prefix = g_strdup (root_dir);
	}
	if (!g_file_test (prefix, G_FILE_TEST_EXISTS)) {
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "Failed to find icon '%s' in %s", icon_name, prefix);
		return NULL;
	}

	/* select minimum size */
	for (guint i = 0; sizes[i].size_str != NULL; i++) {
		if (sizes[i].size >= icon_size) {
			min_size_idx = i;
			break;
		}
	}

	/* hicolor icon theme search */
	for (guint i = min_size_idx; sizes[i].size_str != NULL; i++) {
		g_autofree gchar *size = NULL;
		if (icon_scale == 1)
			size = g_strdup (sizes[i].size_str);
		else
			size = g_strdup_printf ("%s@%i", sizes[i].size_str, icon_scale);
		for (guint m = 0; types[m] != NULL; m++) {
			for (guint j = 0; supported_ext[j] != NULL; j++) {
				g_autofree gchar *tmp = NULL;
				tmp = g_strdup_printf ("%s/share/icons/"
							"hicolor/%s/%s/%s%s",
							prefix,
							size,
							types[m],
							icon_name,
							supported_ext[j]);
				if (g_file_test (tmp, G_FILE_TEST_EXISTS))
					return g_strdup (tmp);
			}
		}
	}

	/* breeze icon theme search, for KDE Plasma compatibility */
	for (guint i = min_size_idx; sizes[i].size_str != NULL; i++) {
		g_autofree gchar *size = NULL;
		if (icon_scale == 1)
			size = g_strdup (sizes[i].size_str);
		else
			size = g_strdup_printf ("%s@%i", sizes[i].size_str, icon_scale);
		for (guint m = 0; types[m] != NULL; m++) {
			for (guint j = 0; supported_ext[j] != NULL; j++) {
				g_autofree gchar *tmp = NULL;
				tmp = g_strdup_printf ("%s/share/icons/"
							"breeze/%s/%s/%s%s",
							prefix,
							types[m],
							size,
							icon_name,
							supported_ext[j]);
				if (g_file_test (tmp, G_FILE_TEST_EXISTS))
					return g_strdup (tmp);
			}
		}
	}

	/* failed */
	g_set_error (error,
		     AS_UTILS_ERROR,
		     AS_UTILS_ERROR_FAILED,
		     "Failed to find icon %s", icon_name);
	return NULL;
}

/**
 * as_utils_guess_scope_from_path:
 * @path: The filename to test.
 *
 * Guess the #AsComponentScope that applies to a given path.
 *
 * Returns: the #AsComponentScope
 *
 * Since: 0.15.0
 */
AsComponentScope
as_utils_guess_scope_from_path (const gchar *path)
{
	if (g_str_has_prefix (path, "/home") || g_str_has_prefix (path, g_get_home_dir ()))
		return AS_COMPONENT_SCOPE_USER;
	return AS_COMPONENT_SCOPE_SYSTEM;
}
