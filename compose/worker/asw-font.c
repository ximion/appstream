/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:asw-font
 * @short_description: Font handling functions of the AppStream media worker.
 */

#include "config.h"
#include "asw-font-private.h"

#include <glib/gstdio.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include <pango/pango-language.h>

#include "as-utils-private.h"

#include "asc-globals.h"
#include "asw-canvas.h"
#include "asw-resources.h"

/* Fontconfig is not threadsafe, so we use this mutex to guard any section
 * using it either directly or indirectly. This variable is exported to
 * other units as well. */
GMutex fontconfig_mutex;

struct _AswFont {
	GObject parent_instance;
};

typedef struct {
	FT_Library library;
	FT_Face fface;

	GHashTable *languages;

	gchar *preferred_lang;
	gchar *sample_text;
	gchar *sample_icon_text;

	gchar *style;
	gchar *fullname;
	gchar *id;

	gchar *description;
	gchar *designer_name;
	gchar *homepage;

	gchar *file_basename;
} AswFontPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AswFont, asw_font, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asw_font_get_instance_private (o))

/**
 * asw_font_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
asw_font_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AswFontError");
	return quark;
}

static void
asw_font_finalize (GObject *object)
{
	AswFont *font = ASW_FONT (object);
	AswFontPrivate *priv = GET_PRIVATE (font);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);

	if (priv->fface != NULL)
		FT_Done_Face (priv->fface);
	if (priv->library != NULL)
		FT_Done_FreeType (priv->library);

	g_hash_table_unref (priv->languages);

	g_free (priv->preferred_lang);
	g_free (priv->sample_text);
	g_free (priv->sample_icon_text);

	g_free (priv->style);
	g_free (priv->fullname);
	g_free (priv->id);

	g_free (priv->description);
	g_free (priv->designer_name);
	g_free (priv->homepage);

	g_free (priv->file_basename);

	G_OBJECT_CLASS (asw_font_parent_class)->finalize (object);
}

static void
asw_font_init (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);

	priv->languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
asw_font_class_init (AswFontClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asw_font_finalize;
}

static void
asw_font_read_sfnt_data (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	guint namecount;

	namecount = FT_Get_Sfnt_Name_Count (priv->fface);
	for (guint index = 0; index < namecount; index++) {
		FT_SfntName sname;
		g_autofree gchar *val = NULL;

		if (FT_Get_Sfnt_Name (priv->fface, index, &sname) != 0)
			continue;

		/* only handle unicode names for en_US */
		if (!(sname.platform_id == TT_PLATFORM_MICROSOFT &&
		      sname.encoding_id == TT_MS_ID_UNICODE_CS &&
		      sname.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES))
			continue;

		val = g_convert ((gchar *) sname.string,
				 sname.string_len,
				 "UTF-8",
				 "UTF-16BE",
				 NULL,
				 NULL,
				 NULL);

		switch (sname.name_id) {
		case TT_NAME_ID_SAMPLE_TEXT:
			g_free (g_steal_pointer (&priv->sample_icon_text));
			if (!as_is_empty (val))
				g_strchug (val);
			if (g_utf8_strlen (val, -1) > 3) {
				g_autofree gchar *substr = g_strchomp (
				    g_utf8_substring (val, 0, 3));
				if (!as_is_empty (substr))
					priv->sample_icon_text = g_steal_pointer (&substr);
			}
			if (priv->sample_icon_text == NULL)
				priv->sample_icon_text = g_steal_pointer (&val);
			break;
		case TT_NAME_ID_DESCRIPTION:
			g_free (priv->description);
			priv->description = g_steal_pointer (&val);
			break;
		case TT_NAME_ID_DESIGNER_URL:
			g_free (priv->homepage);
			priv->homepage = g_steal_pointer (&val);
			break;
		case TT_NAME_ID_VENDOR_URL:
			if ((priv->homepage == NULL) || (priv->homepage[0] == '\0')) {
				g_free (priv->homepage);
				priv->homepage = g_steal_pointer (&val);
			}
			break;
		default:
			break;
		}
	}
}

static void
asw_font_load_fontconfig_data_from_file (AswFont *font, const gchar *fname)
{
	/* open FC font pattern
	 * the count pointer has to be valid, otherwise FcFreeTypeQuery() crashes. */
	int c;
	FcPattern *fpattern;
	gboolean any_lang_added = FALSE;
	gboolean match = FALSE;
	guchar *tmp_val;
	AswFontPrivate *priv = GET_PRIVATE (font);

	fpattern = FcFreeTypeQuery ((const guchar *) fname, 0, NULL, &c);

	/* load information about the font */
	g_hash_table_remove_all (priv->languages);
	match = TRUE;
	for (guint i = 0; match == TRUE; i++) {
		FcLangSet *ls;

		match = FALSE;
		if (FcPatternGetLangSet (fpattern, FC_LANG, i, &ls) == FcResultMatch) {
			FcStrSet *langs;
			FcStrList *list;
			match = TRUE;

			langs = FcLangSetGetLangs (ls);
			list = FcStrListCreate (langs);

			FcStrListFirst (list);
			while ((tmp_val = FcStrListNext (list)) != NULL) {
				g_hash_table_add (priv->languages, g_strdup ((gchar *) tmp_val));
				any_lang_added = TRUE;
			}

			FcStrListDone (list);
			FcStrSetDestroy (langs);
		}
	}

	if (FcPatternGetString (fpattern, FC_FULLNAME, 0, &tmp_val) == FcResultMatch) {
		g_free (priv->fullname);
		priv->fullname = g_strdup ((gchar *) tmp_val);
	}

	if (FcPatternGetString (fpattern, FC_STYLE, 0, &tmp_val) == FcResultMatch) {
		g_free (priv->style);
		priv->style = g_strdup ((gchar *) tmp_val);
	}

	/* assume 'en' is available */
	if (!any_lang_added)
		g_hash_table_add (priv->languages, g_strdup ("en"));

	/* prefer the English language if possible
	 * this is a hack since some people don't set their
	 * <languages> tag properly. */
	if (any_lang_added && g_hash_table_contains (priv->languages, "en")) {
		g_free (priv->preferred_lang);
		priv->preferred_lang = g_strdup ("en");
	}

	/* read font metadata, if any is there */
	asw_font_read_sfnt_data (font);

	/* cleanup */
	FcPatternDestroy (fpattern);
}

static AswFont *
asw_font_new (GError **error)
{
	AswFontPrivate *priv;
	FT_Error err;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);
	g_autoptr(AswFont) font = ASW_FONT (g_object_new (ASW_TYPE_FONT, NULL));
	priv = GET_PRIVATE (font);

	err = FT_Init_FreeType (&priv->library);
	if (err != 0) {
		g_set_error (error,
			     ASW_FONT_ERROR,
			     ASW_FONT_ERROR_FAILED,
			     "Unable to load FreeType. Error code: %i",
			     err);
		return NULL;
	}

	return g_steal_pointer (&font);
}

/**
 * asw_font_new_from_file:
 * @fname: Name of the file to load.
 * @error: A #GError or %NULL
 *
 * Creates a new #AswFont from a file on the filesystem.
 **/
AswFont *
asw_font_new_from_file (const gchar *fname, GError **error)
{
	AswFont *font;
	AswFontPrivate *priv;
	FT_Error err;

	font = asw_font_new (error);
	if (font == NULL)
		return NULL;
	priv = GET_PRIVATE (font);

	err = FT_New_Face (priv->library, fname, 0, &priv->fface);
	if (err != 0) {
		g_set_error (error,
			     ASW_FONT_ERROR,
			     ASW_FONT_ERROR_FAILED,
			     "Unable to load font face from file. Error code: %i",
			     err);
		return NULL;
	}

	asw_font_load_fontconfig_data_from_file (font, fname);
	g_free (priv->file_basename);
	priv->file_basename = g_path_get_basename (fname);

	return font;
}

/**
 * asw_font_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @file_basename: Font file basename.
 * @error: A #GError or %NULL
 *
 * Creates a new #AswFont from data in memory.
 * The font file basename needs to be supplied as fallback
 * and for heuristics.
 **/
AswFont *
asw_font_new_from_data (const void *data, gssize len, const gchar *file_basename, GError **error)
{
	const gchar *tmp_root;
	gboolean ret;
	g_autofree gchar *fname = NULL;

	/* we unfortunately need to create a stupid temporary file here, otherwise Fontconfig
	 * does not work and we can not determine the right demo strings for this font.
	 * (FreeType itself could load from memory) */
	tmp_root = asc_globals_get_tmp_dir_create ();

	fname = g_build_filename (tmp_root, file_basename, NULL);
#if GLIB_CHECK_VERSION(2, 66, 0)
	ret = g_file_set_contents_full (fname, data, len, G_FILE_SET_CONTENTS_NONE, 0666, error);
#else
	ret = g_file_set_contents (fname, data, len, error);
	if (ret)
		g_chmod (fname, 0666);
#endif
	if (!ret)
		return NULL;

	return asw_font_new_from_file (fname, error);
}

/**
 * asw_font_new_from_fd:
 * @fd: A (sealed) file descriptor containing the font data.
 * @file_basename: Font file basename.
 * @error: A #GError or %NULL
 *
 * Creates a new #AswFont from an open file descriptor, without
 * requiring any filesystem access.
 * The font file basename needs to be supplied as fallback
 * and for heuristics.
 **/
AswFont *
asw_font_new_from_fd (gint fd, const gchar *file_basename, GError **error)
{
	AswFont *font;
	AswFontPrivate *priv;
	g_autofree gchar *fd_path = g_strdup_printf ("/proc/self/fd/%d", fd);

	font = asw_font_new_from_file (fd_path, error);
	if (font == NULL)
		return NULL;

	/* the procfs path is useless as basename, override it with the real name */
	priv = GET_PRIVATE (font);
	g_free (priv->file_basename);
	priv->file_basename = g_strdup (file_basename);

	return font;
}

/**
 * asw_font_get_family:
 * @font: an #AswFont instance.
 *
 * Gets the font family.
 **/
const gchar *
asw_font_get_family (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	return priv->fface->family_name;
}

/**
 * asw_font_get_style:
 * @font: an #AswFont instance.
 *
 * Gets the font style.
 **/
const gchar *
asw_font_get_style (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	return priv->style;
}

/**
 * asw_font_get_fullname:
 * @font: an #AswFont instance.
 *
 * Gets the fonts full name.
 **/
const gchar *
asw_font_get_fullname (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	if (as_is_empty (priv->fullname)) {
		g_free (priv->fullname);
		priv->fullname = g_strdup_printf ("%s %s",
						  asw_font_get_family (font),
						  asw_font_get_style (font));
	}
	return priv->fullname;
}

/**
 * asw_font_get_id:
 * @font: an #AswFont instance.
 *
 * Gets an identifier string for this font.
 **/
const gchar *
asw_font_get_id (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	g_autofree gchar *tmp_family = NULL;
	g_autofree gchar *tmp_style = NULL;
	gchar *tmp;

	if (asw_font_get_family (font) == NULL)
		return priv->file_basename;
	if (asw_font_get_style (font) == NULL)
		return priv->file_basename;
	if (priv->id != NULL)
		return priv->id;

	tmp = g_utf8_strdown (asw_font_get_family (font), -1);
	tmp_family = as_str_replace (tmp, " ", "", 0);
	as_strstripnl (tmp_family);
	g_free (tmp);

	tmp = g_utf8_strdown (asw_font_get_style (font), -1);
	tmp_style = as_str_replace (tmp, " ", "", 0);
	as_strstripnl (tmp_style);
	g_free (tmp);

	g_free (priv->id);
	priv->id = g_strdup_printf ("%s-%s", tmp_family, tmp_style);

	return priv->id;
}

/**
 * asw_font_get_charset:
 * @font: an #AswFont instance.
 *
 * Gets the primary/first character set for this font.
 **/
FT_Encoding
asw_font_get_charset (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	if (priv->fface->num_charmaps == 0)
		return FT_ENCODING_NONE;
	return priv->fface->charmaps[0]->encoding;
}

/**
 * asw_font_get_ftface:
 * @font: an #AswFont instance.
 *
 * Gets the FreeType font face for this font.
 **/
FT_Face
asw_font_get_ftface (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	return priv->fface;
}

/**
 * asw_font_get_language_list:
 * @font: an #AswFont instance.
 *
 * Gets the list of languages supported by this font.
 *
 * Returns: (transfer container) (element-type utf8): Sorted list of languages
 **/
GList *
asw_font_get_language_list (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	GList *list = g_hash_table_get_keys (priv->languages);
	list = g_list_sort (list, (GCompareFunc) g_strcmp0);
	return list;
}

/**
 * asw_font_add_language:
 * @font: an #AswFont instance.
 *
 * Add language to the language list of this font.
 **/
void
asw_font_add_language (AswFont *font, const gchar *lang)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	g_hash_table_add (priv->languages, g_strdup (lang));
}

/**
 * asw_font_get_preferred_language:
 * @font: an #AswFont instance.
 *
 * Gets the fonts preferred language.
 **/
const gchar *
asw_font_get_preferred_language (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	return priv->preferred_lang;
}

/**
 * asw_font_set_preferred_language:
 * @font: an #AswFont instance.
 *
 * Sets the fonts preferred language.
 **/
void
asw_font_set_preferred_language (AswFont *font, const gchar *lang)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	g_free (priv->preferred_lang);
	priv->preferred_lang = g_strdup (lang);
}

/**
 * asw_font_get_description:
 * @font: an #AswFont instance.
 *
 * Gets the font description.
 **/
const gchar *
asw_font_get_description (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	return priv->description;
}

/**
 * asw_font_get_homepage:
 * @font: an #AswFont instance.
 *
 * Gets the font homepage.
 **/
const gchar *
asw_font_get_homepage (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	return priv->homepage;
}

/**
 * asw_get_pangrams_en:
 *
 * Obtain the list of English pangrams from the worker's built-in resources.
 *
 * Returns: (transfer none): List of pangrams, or %NULL on failure.
 */
static GPtrArray *
asw_get_pangrams_en (void)
{
	static GPtrArray *pangrams_en = NULL;
	static GMutex mutex;
	g_autoptr(GBytes) data = NULL;
	g_auto(GStrv) strv = NULL;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&mutex);

	/* return cached value if possible */
	if (pangrams_en != NULL)
		return pangrams_en;

	/* load array from resources */
	data = g_resource_lookup_data (asw_get_resource (),
				       "/org/freedesktop/appstream-compose/pangrams/en.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return NULL;

	strv = g_strsplit (g_bytes_get_data (data, NULL), "\n", -1);
	if (strv == NULL)
		return NULL;

	pangrams_en = g_ptr_array_new_full (g_strv_length (strv), g_free);
	for (guint i = 0; strv[i] != NULL; i++)
		g_ptr_array_add (pangrams_en, g_strdup (strv[i]));

	return pangrams_en;
}

/**
 * asw_font_find_pangram:
 *
 * Find a pangram for the given language, making a random but
 * predictable selection.
 *
 * Returns: A representative text for the language, or %NULL if no specific one was found.
 */
const gchar *
asw_font_find_pangram (AswFont *font, const gchar *lang, const gchar *rand_id)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	GPtrArray *pangrams;
	PangoLanguage *plang;
	const gchar *text;

	if (g_strcmp0 (lang, "en") == 0) {
		guint idx;

		/* we ideally want to have fonts of the same family to share the same pangram */
		if (rand_id == NULL) {
			rand_id = asw_font_get_family (font);
			if (as_is_empty (rand_id))
				rand_id = priv->file_basename;
			if (as_is_empty (rand_id))
				rand_id = asw_font_get_id (font);
		}

		pangrams = asw_get_pangrams_en ();
		if (pangrams == NULL) {
			g_warning ("No pangrams found for the english language, even though we "
				   "should have some available.");
		} else {
			/* select an English pangram */
			idx = g_str_hash (rand_id) % pangrams->len;
			return (const gchar *) g_ptr_array_index (pangrams, idx);
		}
	}

	/* The returned pointer will be valid forever after, and should not be freed. */
	plang = pango_language_from_string (lang);
	text = pango_language_get_sample_string (plang);
	if (g_strcmp0 (text,
		       pango_language_get_sample_string (pango_language_from_string ("xx"))) == 0)
		return NULL;
	return text;
}

/**
 * asw_font_get_icon_text_for_lang:
 *
 * Obtain a font "icon" text for the given language, or
 * return %NULL in case we do not have one explicitly set.
 */
static const gchar *
asw_font_get_icon_text_for_lang (const gchar *lang)
{
	/* clang-format off */
	struct {
		const gchar	*lang;
		const gchar	*value;
	} text_icon[] =  {
		{ "en",		"Aa" },
		{ "ar",		"أب" },
		{ "as",		 "অআই" },
		{ "bn",		 "অআই" },
		{ "be",		"Аа" },
		{ "bg",		"Аа" },
		{ "cs",		"Aa" },
		{ "da",		"Aa" },
		{ "de",		"Aa" },
		{ "es",		"Aa" },
		{ "fr",		"Aa" },
		{ "gu",		"અબક" },
		{ "hi",		 "अआइ" },
		{ "he",		"אב" },
		{ "it",		"Aa" },
		{ "kn",		 "ಅಆಇ" },
		{ "ml",		"ആഇ" },
		{ "ne",		 "अआइ" },
		{ "nl",		"Aa" },
		{ "or",		 "ଅଆଇ" },
		{ "pa",		 "ਅਆਇ" },
		{ "pl",		"ĄĘ" },
		{ "pt",		"Aa" },
		{ "ru",		"Аа" },
		{ "sv",		"Åäö" },
		{ "ta",		 "அஆஇ" },
		{ "te",		 "అఆఇ" },
		{ "ua",		"Аа" },
		{ "und-zsye",   "😀" },
		{ "zh-tw",	"漢" },
		{ NULL, NULL } };
	/* clang-format on */

	for (guint i = 0; text_icon[i].lang != NULL; i++) {
		if (g_strcmp0 (text_icon[i].lang, lang) == 0)
			return text_icon[i].value;
	}

	return NULL;
}

/**
 * asw_font_set_fallback_sample_texts_if_needed:
 */
static void
asw_font_set_fallback_sample_texts_if_needed (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);

	if (as_is_empty (priv->sample_text)) {
		g_free (priv->sample_text);
		priv->sample_text = g_strdup (
		    "Lorem ipsum dolor sit amet, consetetur sadipscing elitr.");
	}

	if (as_is_empty (priv->sample_icon_text)) {
		g_free (priv->sample_icon_text);
		if (g_utf8_strlen (priv->sample_text, 8) > 3)
			priv->sample_icon_text = g_utf8_substring (priv->sample_text, 0, 3);
		else
			priv->sample_icon_text = g_strdup ("Aa");
	}
}

/**
 * asw_font_determine_sample_texts:
 *
 * Set example texts to display this font.
 */
static void
asw_font_determine_sample_texts (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	g_autoptr(GList) tmp_lang_list = NULL;

	/* If we only have to set the icon text, try to do it!
	 * Otherwise keep cached values and do nothing */
	if (!as_is_empty (priv->sample_text)) {
		asw_font_set_fallback_sample_texts_if_needed (font);
		if (!as_is_empty (priv->sample_icon_text))
			return;
	}

	/* always prefer English (even if not alphabetically first) */
	if (g_hash_table_contains (priv->languages, "en")) {
		g_free (priv->preferred_lang);
		priv->preferred_lang = g_strdup ("en");
	}

	/* ensure we try the preferred language first */
	tmp_lang_list = asw_font_get_language_list (font);
	if (!as_is_empty (priv->preferred_lang))
		tmp_lang_list = g_list_prepend (tmp_lang_list, priv->preferred_lang);

	/* determine our sample texts */
	for (GList *l = tmp_lang_list; l != NULL; l = l->next) {
		const gchar *text = asw_font_find_pangram (font, l->data, NULL);
		if (text == NULL)
			continue;

		g_free (priv->sample_text);
		priv->sample_text = g_strdup (text);

		g_free (priv->sample_icon_text);
		priv->sample_icon_text = g_strdup (asw_font_get_icon_text_for_lang (l->data));
		break;
	}

	/* set some default values if we have been unable to find any texts */
	asw_font_set_fallback_sample_texts_if_needed (font);

	/* check if we have a font that can actually display the characters we picked - in case
	 * it doesn't, we just select random chars. */
	if (FT_Get_Char_Index (priv->fface,
			       g_utf8_get_char_validated (priv->sample_icon_text, -1)) != 0)
		return;

	if (FT_Get_Char_Index (priv->fface, g_utf8_get_char_validated ("☃", -1)) != 0) {
		/* maybe we have a symbols-only font? */
		g_free (priv->sample_text);
		g_free (priv->sample_icon_text);
		priv->sample_text = g_strdup ("☃❤✓☀★☂♞☯☢∞❄♫↺");
		priv->sample_icon_text = g_strdup ("☃❤");
	} else {
		GString *sample_text;
		guint count;

		sample_text = g_string_new ("");
		count = 0;
		for (gint map = 0; map < priv->fface->num_charmaps; map++) {
			FT_ULong charcode;
			FT_UInt gindex;
			FT_CharMap charmap = priv->fface->charmaps[map];
			FT_Set_Charmap (priv->fface, charmap);

			charcode = FT_Get_First_Char (priv->fface, &gindex);
			while (gindex != 0) {
				if (g_unichar_isgraph (charcode) && !g_unichar_ispunct (charcode)) {
					count++;
					g_string_append_unichar (sample_text, charcode);
				}

				if (count >= 24)
					break;
				charcode = FT_Get_Next_Char (priv->fface, charcode, &gindex);
			}

			if (count >= 24)
				break;
		}

		g_free (priv->sample_text);
		priv->sample_text = g_string_free (sample_text, FALSE);
		g_strstrip (priv->sample_text);

		g_free (g_steal_pointer (&priv->sample_icon_text));

		/* if we were unsuccessful at adding chars, set fallback again
		 * (and in this case, also set the icon text to something useful again) */
		asw_font_set_fallback_sample_texts_if_needed (font);
	}
}

/**
 * asw_font_get_sample_text:
 * @font: an #AswFont instance.
 *
 * Gets the sample text for this font.
 **/
const gchar *
asw_font_get_sample_text (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	if (as_is_empty (priv->sample_text))
		asw_font_determine_sample_texts (font);
	return priv->sample_text;
}

/**
 * asw_font_set_sample_text:
 * @font: an #AswFont instance.
 *
 * Sets the sample text for this font.
 **/
void
asw_font_set_sample_text (AswFont *font, const gchar *text)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	g_free (priv->sample_text);
	priv->sample_text = g_strdup (text);
}

/**
 * asw_font_get_sample_icon_text:
 * @font: an #AswFont instance.
 *
 * Gets the sample icon text fragment for this font.
 **/
const gchar *
asw_font_get_sample_icon_text (AswFont *font)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	if (as_is_empty (priv->sample_icon_text))
		asw_font_determine_sample_texts (font);
	return priv->sample_icon_text;
}

/**
 * asw_font_set_sample_icon_text:
 * @font: an #AswFont instance.
 *
 * Sets the sample icon text fragment for this font - must not
 * contain more than 3 characters to be valid.
 **/
void
asw_font_set_sample_icon_text (AswFont *font, const gchar *text)
{
	AswFontPrivate *priv = GET_PRIVATE (font);
	glong len = g_utf8_strlen (text, -1);
	if (len > 3)
		return;
	g_free (priv->sample_icon_text);
	priv->sample_icon_text = g_strdup (text);
}

/**
 * asw_font_render_card_to_file:
 * @font: an #AswFont instance.
 * @png_fname: Filename of the resulting PNG image.
 * @width: Requested card width.
 * @height: Requested card height.
 * @info_label: Short info label for the font, or %NULL for the default.
 * @actual_width: (out) (optional): Actual width of the rendered card.
 * @actual_height: (out) (optional): Actual height of the rendered card.
 * @error: A #GError or %NULL
 *
 * Render a font specimen card for the given font and save it as PNG image.
 * The card render may adjust the canvas height, so the actual dimensions
 * are returned and must be used by the caller.
 *
 * Returns: %TRUE on success.
 **/
gboolean
asw_font_render_card_to_file (AswFont *font,
			      const gchar *png_fname,
			      gint width,
			      gint height,
			      const gchar *info_label,
			      gint *actual_width,
			      gint *actual_height,
			      GError **error)
{
	const gchar *bg_letter = NULL;
	g_autoptr(AswCanvas) cv = NULL;

	if (as_str_equal0 (asw_font_get_preferred_language (font), "en"))
		bg_letter = "a";
	else
		bg_letter = asw_font_get_sample_icon_text (font);

	cv = asw_canvas_new (width, height);
	if (!asw_canvas_draw_font_card (cv,
					font,
					info_label,
					asw_font_get_sample_text (font),
					bg_letter,
					-1, /* default border width */
					error))
		return FALSE;

	if (!asw_canvas_save_png (cv, png_fname, error))
		return FALSE;

	if (actual_width != NULL)
		*actual_width = asw_canvas_get_width (cv);
	if (actual_height != NULL)
		*actual_height = asw_canvas_get_height (cv);
	return TRUE;
}

/**
 * asw_font_render_icon_to_file:
 * @font: an #AswFont instance.
 * @png_fname: Filename of the resulting PNG image.
 * @size: Icon canvas size in pixels (width and height).
 * @actual_width: (out) (optional): Actual width of the rendered icon.
 * @actual_height: (out) (optional): Actual height of the rendered icon.
 * @error: A #GError or %NULL
 *
 * Render an icon for the given font, consisting of a background shape
 * (deterministically selected based on the font ID) with the font's
 * sample icon text drawn on top, and save it as PNG image.
 *
 * Returns: %TRUE on success.
 **/
gboolean
asw_font_render_icon_to_file (AswFont *font,
			      const gchar *png_fname,
			      guint size,
			      gint *actual_width,
			      gint *actual_height,
			      GError **error)
{
	AswCanvasShape bg_shape;
	gint shape_border_width;
	gint text_border_width;
	g_autoptr(AswCanvas) cv = NULL;

	bg_shape = g_str_hash (asw_font_get_id (font)) % ASW_CANVAS_SHAPE_LAST;

	/* we want a small border around our shape */
	shape_border_width = (gint) (size * 0.032);

	/* calculate text border width based on shape type to ensure
	 * text fits properly within the shape */
	text_border_width = asw_calculate_text_border_width_for_icon_shape (bg_shape,
									    size,
									    shape_border_width);

	cv = asw_canvas_new (size, size);
	if (!asw_canvas_draw_shape (cv,
				    bg_shape,
				    shape_border_width,
				    0.84, /* red */
				    0.84, /* green */
				    0.84, /* blue */
				    error))
		return FALSE;

	if (!asw_canvas_draw_text_line (cv,
					font,
					asw_font_get_sample_icon_text (font),
					text_border_width,
					bg_shape == ASW_CANVAS_SHAPE_CVL_TRIANGLE
					    ? (gint) ((size / 2.0 - shape_border_width) * 0.15)
					    : 0,
					error))
		return FALSE;

	if (!asw_canvas_save_png (cv, png_fname, error))
		return FALSE;

	if (actual_width != NULL)
		*actual_width = asw_canvas_get_width (cv);
	if (actual_height != NULL)
		*actual_height = asw_canvas_get_height (cv);
	return TRUE;
}
