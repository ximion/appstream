/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-desktop-entry.h"

/**
 * SECTION:as-desktop-entry
 * @short_description: Parser for XDG Desktop Entry data.
 * @include: appstream.h
 *
 */

#include <string.h>

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-metadata.h"
#include "as-component.h"
#include "as-component-private.h"
#include "as-validator-issue.h"

#define DESKTOP_GROUP G_KEY_FILE_DESKTOP_GROUP

/**
 * as_strequal_casefold:
 */
static gboolean
as_strequal_casefold (const gchar *a, const gchar *b)
{
	g_autofree gchar *str1 = NULL;
	g_autofree gchar *str2 = NULL;

	if (a != NULL)
		str1 = g_utf8_casefold (a, -1);
	if (b != NULL)
		str2 = g_utf8_casefold (b, -1);
	return g_strcmp0 (str1, str2) == 0;
}

/**
 * as_desktop_entry_add_issue:
 */
static void
as_desktop_entry_add_issue (GPtrArray *issues, const gchar *tag, const gchar *format, ...)
{
	va_list args;
	g_autofree gchar *buffer = NULL;
	AsValidatorIssue *issue = NULL;

	/* do nothing if we shouldn't record issues */
	if (issues == NULL)
		return;

	if (format != NULL) {
		va_start (args, format);
		buffer = g_strdup_vprintf (format, args);
		va_end (args);
	}

	issue = as_validator_issue_new ();
	as_validator_issue_set_tag (issue, tag);
	as_validator_issue_set_hint (issue, buffer);
	g_ptr_array_add (issues, issue);
}

/**
 * as_get_locale_from_key:
 */
static gchar*
as_get_locale_from_key (const gchar *key)
{
	gchar *tmp1;
	gchar *tmp2;
	gchar *delim;
	g_autofree gchar *locale = NULL;

	tmp1 = g_strstr_len (key, -1, "[");
	if (tmp1 == NULL)
		return g_strdup ("C");
	tmp2 = g_strstr_len (tmp1, -1, "]");
	/* this is a bug in the file */
	if (tmp2 == NULL)
		return g_strdup ("C");
	locale = g_strdup (tmp1 + 1);
	locale[tmp2 - tmp1 - 1] = '\0';

	/* drop UTF-8 suffixes */
	if (g_str_has_suffix (locale, ".utf-8") ||
	    g_str_has_suffix (locale, ".UTF-8"))
		locale[strlen (locale)-6] = '\0';

	/* filter out cruft */
	if (as_is_cruft_locale (locale))
		return NULL;

	delim = g_strrstr (locale, ".");
	if (delim != NULL) {
		gchar *tmp;
		g_autofree gchar *enc = NULL;
		/* looks like we need to drop another encoding suffix
		 * (but we need to make sure it actually is one) */
		tmp = delim + 1;
		if (tmp != NULL)
			enc = g_utf8_strdown (tmp, -1);
		if ((enc != NULL) && (g_str_has_prefix (enc, "iso"))) {
			delim[0] = '\0';
		}
	}

	return g_steal_pointer (&locale);
}

/**
 * as_add_filtered_categories:
 *
 * Filter out some useless categories which we don't want to have in the
 * AppStream metadata.
 * Add the remaining ones to the new #AsComponent.
 */
static void
as_add_filtered_categories (gchar **cats, AsComponent *cpt, GPtrArray *issues)
{
	guint i;

	for (i = 0; cats[i] != NULL; i++) {
		const gchar *cat = cats[i];

		if (g_strcmp0 (cat, "GTK") == 0)
			continue;
		if (g_strcmp0 (cat, "Qt") == 0)
			continue;
		if (g_strcmp0 (cat, "GNOME") == 0)
			continue;
		if (g_strcmp0 (cat, "KDE") == 0)
			continue;
		if (g_strcmp0 (cat, "GUI") == 0)
			continue;
		if (g_strcmp0 (cat, "Application") == 0)
			continue;

		/* custom categories are ignored */
		if (g_str_has_prefix (cat, "X-"))
			continue;
		if (g_str_has_prefix (cat, "x-"))
			continue;

		/* check for invalid */
		if (g_strcmp0 (cat, "") == 0)
			continue;

		/* add the category if it is valid */
		if (as_utils_is_category_name (cat))
			as_component_add_category (cpt, cat);
		else
			as_desktop_entry_add_issue (issues,
						    "desktop-entry-category-invalid",
						    cat);
	}
}

/**
 * as_get_desktop_entry_value:
 */
static gchar *
as_get_desktop_entry_value (GKeyFile *df, GPtrArray *issues, const gchar *key)
{
	g_autofree gchar *str = NULL;
	const gchar *str_iter;
	GString *sane_str;
	gboolean has_invalid_chars = FALSE;
	g_autoptr(GError) error = NULL;

	str = g_key_file_get_string (df, DESKTOP_GROUP, key, &error);
	if (error != NULL)
		as_desktop_entry_add_issue (issues,
					    "desktop-entry-bad-data",
					    error->message);
	if (str == NULL)
		return NULL;

	/* some dumb .desktop files contain non-printable characters. If we are in XML mode,
	 * this will hard-break some XML readers at a later point, so we need to clean this up
	 * and replace these characters with a nice questionmark, so someone will notice and hopefully
	 * fix the issue at the source */
	sane_str = g_string_new ("");
	str_iter = str;
	while (*str_iter != '\0') {
		gunichar c = g_utf8_get_char (str_iter);
		if (as_unichar_accepted (c))
			g_string_append_unichar (sane_str, c);
		else {
			g_string_append (sane_str, "�");
			has_invalid_chars = TRUE;
		}

		str_iter = g_utf8_next_char (str_iter);
	}

	if (has_invalid_chars)
		as_desktop_entry_add_issue (issues,
					    "desktop-entry-value-invalid-chars",
					    key);
	return g_string_free (sane_str, FALSE);
}

/**
 * as_check_desktop_string:
 */
void
as_check_desktop_string (GPtrArray *issues, const gchar *field, const gchar *str)
{
	if (issues == NULL)
		return;
	if ((g_str_has_prefix (str, "\"") && g_str_has_suffix (str, "\"")) ||
	    (g_str_has_prefix (str, "'") && g_str_has_suffix (str, "'")))
		as_desktop_entry_add_issue (issues,
					    "desktop-entry-value-quoted",
					    "%s: %s", field, str);
}

/**
 * as_get_external_desktop_translations:
 */
static GPtrArray*
as_get_external_desktop_translations (GKeyFile *kf, const gchar *text, const gchar *locale,
				      AsTranslateDesktopTextFn de_l10n_fn, gpointer user_data)
{
	GPtrArray *l10n;
	if (de_l10n_fn == NULL)
		return NULL;
	if (g_strcmp0 (locale, "C") != 0)
		return NULL;

	l10n = de_l10n_fn (kf, text, user_data);
	if (G_UNLIKELY (l10n->len % 2 != 0)) {
		/* NOTE: We could use g_return_val_if_fail here, but we could just as well write a more descriptive message */
		g_critical ("Invalid amount of list entries in external desktop translation l10n listing. "
			    "Make sure you return locale names in even, and translations in odd indices. This is a programmer error.");
		return NULL;
	}
	return l10n;
}

/**
 * as_desktop_entry_parse_data:
 */
gboolean
as_desktop_entry_parse_data (AsComponent *cpt,
			     const gchar *data, gssize data_len,
			     AsFormatVersion fversion,
			     gboolean ignore_nodisplay,
			     GPtrArray *issues,
			     AsTranslateDesktopTextFn de_l10n_fn,
			     gpointer user_data,
			     GError **error)
{
	g_autoptr(GKeyFile) df = NULL;
	g_auto(GStrv) keys = NULL;
	gboolean ignore_cpt = FALSE;
	GError *tmp_error = NULL;
	gchar *tmp;
	gboolean had_name, had_summary, had_categories, had_mimetypes;
	g_autofree gchar *desktop_basename = g_strdup (as_component_get_id (cpt));

	if (desktop_basename == NULL) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_PARSE,
				     "Unable to determine component-id for component from desktop-entry data.");
		return FALSE;
	}

	df = g_key_file_new ();
	g_key_file_load_from_data (df,
				   data,
				   data_len,
				   G_KEY_FILE_KEEP_TRANSLATIONS,
				   &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* check this is a valid desktop file */
	if (!g_key_file_has_group (df, DESKTOP_GROUP)) {
		g_set_error (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_PARSE,
				"Data in '%s' does not contain a valid Desktop Entry.", as_component_get_id (cpt));
		return FALSE;
	}

	/* Type */
	tmp = g_key_file_get_string (df,
				     DESKTOP_GROUP,
				     "Type",
				     NULL);
	if (!as_strequal_casefold (tmp, "application")) {
		g_free (tmp);
		/* not an application, so we can't proceed, but also no error */
		return FALSE;
	}
	g_free (tmp);

	/* NoDisplay */
	tmp = g_key_file_get_string (df,
				     DESKTOP_GROUP,
				     "NoDisplay",
				     NULL);
	if (as_strequal_casefold (tmp, "true")) {
		/* we may read the application data, but it will be ignored in its current form */
		ignore_cpt = TRUE;
		if (!ignore_nodisplay) {
			g_free (tmp);
			return FALSE;
		}
	}
	g_free (tmp);

	/* X-AppStream-Ignore */
	tmp = g_key_file_get_string (df,
				     DESKTOP_GROUP,
				     "X-AppStream-Ignore",
				     NULL);
	if (as_strequal_casefold (tmp, "true")) {
		g_free (tmp);
		/* this file should be ignored, we can't return a component (but this is also no error) */
		return FALSE;
	}
	g_free (tmp);

	/* Hidden */
	tmp = g_key_file_get_string (df,
				     DESKTOP_GROUP,
				     "Hidden",
				     NULL);
	if (as_strequal_casefold (tmp, "true")) {
		ignore_cpt = TRUE;
		as_desktop_entry_add_issue (issues,
					    "desktop-entry-hidden-set",
					    NULL);
		if (!ignore_nodisplay) {
			g_free (tmp);
			return FALSE;
		}
	}
	g_free (tmp);

	/* OnlyShowIn */
	tmp = g_key_file_get_string (df,
				     DESKTOP_GROUP,
				     "OnlyShowIn",
				     NULL);
	if (tmp != NULL) {
		if (as_is_empty (tmp))
			as_desktop_entry_add_issue (issues,
						    "desktop-entry-empty-onlyshowin",
						    NULL);
		/* We want to ignore all desktop-entry files which were made desktop-exclusive
		 * via OnlyShowIn (those are usually configuration apps and control center modules)
		 * Only exception is if a metainfo file was present. */
		g_free (tmp);
		ignore_cpt = TRUE;
		if (!ignore_nodisplay)
			return FALSE;
	}

	/* create the new component we synthesize for this desktop entry */
	as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);
	as_component_set_ignored (cpt, ignore_cpt);
	as_component_set_origin_kind (cpt, AS_ORIGIN_KIND_DESKTOP_ENTRY);

	/* strip .desktop suffix if the reverse-domain-name scheme is followed and we build for
         * a recent AppStream version */
        if (fversion >= AS_FORMAT_VERSION_V0_10) {
		g_auto(GStrv) parts = g_strsplit (desktop_basename, ".", 3);
		if (g_strv_length (parts) == 3) {
			if (as_utils_is_tld (parts[0]) && g_str_has_suffix (desktop_basename, ".desktop")) {
				g_autofree gchar *id_raw = NULL;
				/* remove .desktop suffix */
				id_raw = g_strdup (desktop_basename);
				id_raw[strlen (id_raw)-8] = '\0';

				as_component_set_id (cpt, id_raw);
			}
		}
	}

	had_name = !as_is_empty (as_component_get_name (cpt));
	had_summary = !as_is_empty (as_component_get_name (cpt));
	had_categories = as_component_get_categories (cpt)->len > 0;
	had_mimetypes = as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_MEDIATYPE) != NULL;

	keys = g_key_file_get_keys (df, DESKTOP_GROUP, NULL, NULL);
	for (guint i = 0; keys[i] != NULL; i++) {
		g_autofree gchar *locale = NULL;
		g_autofree gchar *val = NULL;
		gchar *key = keys[i];

		if (g_strcmp0 (key, "Type") == 0)
			continue;

		g_strstrip (key);
		locale = as_get_locale_from_key (key);

		/* skip invalid stuff */
		if (locale == NULL)
			continue;

		val = as_get_desktop_entry_value (df, issues, key);
		if (val == NULL)
			continue;
		if (g_str_has_prefix (key, "Name")) {
			g_autoptr(GPtrArray) l10n_data = NULL;
			if (had_name)
				continue;

			as_component_set_name (cpt, val, locale);
			as_check_desktop_string (issues, key, val);
			l10n_data = as_get_external_desktop_translations (df, val, locale,
									  de_l10n_fn, user_data);
			if (l10n_data != NULL) {
				for (guint j = 0; j < l10n_data->len; j += 2)
					as_component_set_name (cpt,
							       g_ptr_array_index (l10n_data, j),
							       g_ptr_array_index (l10n_data, j + 1));
			}
		} else if (g_str_has_prefix (key, "Comment")) {
			g_autoptr(GPtrArray) l10n_data = NULL;
			if (had_summary)
				continue;

			as_component_set_summary (cpt, val, locale);
			as_check_desktop_string (issues, key, val);
			l10n_data = as_get_external_desktop_translations (df, val, locale,
									  de_l10n_fn, user_data);
			if (l10n_data != NULL) {
				for (guint j = 0; j < l10n_data->len; j += 2)
					as_component_set_name (cpt,
							       g_ptr_array_index (l10n_data, j),
							       g_ptr_array_index (l10n_data, j + 1));
			}
		} else if (g_strcmp0 (key, "Categories") == 0) {
			g_auto(GStrv) cats = NULL;
			if (had_categories)
				continue;

			cats = g_strsplit (val, ";", -1);
			as_add_filtered_categories (cats, cpt, issues);
		} else if (g_str_has_prefix (key, "Keywords")) {
			g_auto(GStrv) kws = NULL;
			g_autoptr(GPtrArray) l10n_data = NULL;

			kws = g_strsplit (val, ";", -1);
			as_component_set_keywords (cpt, kws, locale);

			l10n_data = as_get_external_desktop_translations (df, val, locale,
									  de_l10n_fn, user_data);
			if (l10n_data != NULL) {
				for (guint j = 0; j < l10n_data->len; j += 2) {
					g_auto(GStrv) e_kws = NULL;
					const gchar *e_locale = g_ptr_array_index (l10n_data, j);
					gchar *e_value = g_ptr_array_index (l10n_data, j + 1);

					e_kws = g_strsplit (e_value, ";", -1);
					as_component_set_keywords (cpt, e_kws, e_locale);
				}
			}
		} else if (g_strcmp0 (key, "MimeType") == 0) {
			g_auto(GStrv) mts = NULL;
			g_autoptr(AsProvided) prov = NULL;
			if (had_mimetypes)
				continue;

			mts = g_strsplit (val, ";", -1);
			if (mts == NULL)
				continue;

			prov = as_component_get_provided_for_kind (cpt, AS_PROVIDED_KIND_MEDIATYPE);
			if (prov == NULL) {
				prov = as_provided_new ();
				as_provided_set_kind (prov, AS_PROVIDED_KIND_MEDIATYPE);
			} else {
				g_object_ref (prov);
			}

			for (guint j = 0; mts[j] != NULL; j++) {
				if (g_strcmp0 (mts[j], "") == 0)
					continue;
				as_provided_add_item (prov, mts[j]);
			}

			as_component_add_provided (cpt, prov);
		} else if (g_strcmp0 (key, "Icon") == 0) {
			g_autoptr(AsIcon) icon = NULL;

			icon = as_icon_new ();
			if (g_str_has_prefix (val, "/")) {
				as_icon_set_kind (icon, AS_ICON_KIND_LOCAL);
				as_icon_set_filename (icon, val);
			} else {
				gchar *dot;
				as_icon_set_kind (icon, AS_ICON_KIND_STOCK);

				/* work around stock icons being suffixed */
				dot = g_strstr_len (val, -1, ".");
				if (dot != NULL &&
				    (g_strcmp0 (dot, ".png") == 0 ||
				     g_strcmp0 (dot, ".xpm") == 0 ||
				     g_strcmp0 (dot, ".svg") == 0 ||
				     g_strcmp0 (dot, ".svgz") == 0)) {
					*dot = '\0';
				}

				as_icon_set_name (icon, val);
			}

			as_component_add_icon (cpt, icon);
		}
	}

	/* add this .desktop file as launchable entry, if we don't have one set already */
	if (as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID) == NULL) {
		g_autoptr(AsLaunchable) launch = as_launchable_new ();
		as_launchable_set_kind (launch, AS_LAUNCHABLE_KIND_DESKTOP_ID);
		as_launchable_add_entry (launch, desktop_basename);
		as_component_add_launchable (cpt, launch);

		/* we have the lowest priority */
		as_component_set_priority (cpt, -G_MAXINT);
	}

	return TRUE;
}

/**
 * as_desktop_entry_parse_file:
 *
 * Parse a .desktop file.
 */
gboolean
as_desktop_entry_parse_file (AsComponent *cpt,
			     GFile *file,
			     AsFormatVersion fversion,
			     gboolean ignore_nodisplay,
			     GPtrArray *issues,
			     AsTranslateDesktopTextFn de_l10n_fn,
			     gpointer user_data,
			     GError **error)
{
	g_autofree gchar *file_basename = NULL;
	g_autoptr(GInputStream) file_stream = NULL;
	g_autoptr(GString) dedata = NULL;
	gssize len;
	const gsize buffer_size = 1024 * 32;
	g_autofree gchar *buffer = NULL;

	file_stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
	if (file_stream == NULL)
		return FALSE;

	file_basename = g_file_get_basename (file);
	dedata = g_string_new ("");
	buffer = g_malloc (buffer_size);
	while ((len = g_input_stream_read (file_stream, buffer, buffer_size, NULL, error)) > 0) {
		g_string_append_len (dedata, buffer, len);
	}
	/* check if there was an error */
	if (len < 0)
		return FALSE;

	/* parse desktop entry */
	as_component_set_id (cpt, file_basename);
	return as_desktop_entry_parse_data (cpt,
					    dedata->str,
					    dedata->len,
					    fversion,
					    ignore_nodisplay,
					    issues,
					    de_l10n_fn,
					    user_data,
					    error);
}
