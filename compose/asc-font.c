/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:asc-font
 * @short_description: Font handling functions.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-font-private.h"

#include <glib/gstdio.h>
#include <fontconfig/fontconfig.h>
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include <pango/pango-language.h>

#include "as-utils-private.h"

#include "asc-globals-private.h"

/* Fontconfig is not threadsafe, so we use this mutex to guard any section
 * using it either directly or indirectly. This variable is exported to
 * other units as well. */
GMutex fontconfig_mutex;

struct _AscFont
{
	GObject parent_instance;
};

typedef struct
{
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
} AscFontPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscFont, asc_font, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_font_get_instance_private (o))

/**
 * asc_font_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
asc_font_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AscFontError");
	return quark;
}

static void
asc_font_finalize (GObject *object)
{
	AscFont *font = ASC_FONT (object);
	AscFontPrivate *priv = GET_PRIVATE (font);
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

	G_OBJECT_CLASS (asc_font_parent_class)->finalize (object);
}

static void
asc_font_init (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);

	priv->languages = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 g_free,
						 NULL);
}

static void
asc_font_class_init (AscFontClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_font_finalize;
}

static void
asc_font_read_sfnt_data (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	guint namecount;

	namecount = FT_Get_Sfnt_Name_Count (priv->fface);
	for (guint index = 0; index < namecount; index++) {
		FT_SfntName sname;
		g_autofree gchar *val = NULL;

		if (FT_Get_Sfnt_Name (priv->fface, index, &sname) != 0)
			continue;

		/* only handle unicode names for en_US */
		if (!(sname.platform_id == TT_PLATFORM_MICROSOFT
			&& sname.encoding_id == TT_MS_ID_UNICODE_CS
			&& sname.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES))
			continue;

		val = g_convert((gchar*) sname.string,
				sname.string_len,
				"UTF-8",
				"UTF-16BE",
				NULL,
				NULL,
				NULL);

		switch (sname.name_id) {
			case TT_NAME_ID_SAMPLE_TEXT:
				g_free (priv->sample_icon_text);
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
asc_font_load_fontconfig_data_from_file (AscFont *font, const gchar *fname)
{
	/* open FC font pattern
	 * the count pointer has to be valid, otherwise FcFreeTypeQuery() crashes. */
	int c;
	FcPattern *fpattern;
	gboolean any_lang_added = FALSE;
	gboolean match = FALSE;
	guchar *tmp_val;
	AscFontPrivate *priv = GET_PRIVATE (font);

	fpattern = FcFreeTypeQuery ((const guchar*) fname, 0, NULL, &c);

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
				g_hash_table_add (priv->languages, g_strdup ((gchar*) tmp_val));
				any_lang_added = TRUE;
			}

			FcStrListDone (list);
			FcStrSetDestroy (langs);
		}
	}

	if (FcPatternGetString (fpattern, FC_FULLNAME, 0, &tmp_val) == FcResultMatch) {
		g_free (priv->fullname);
		priv->fullname = g_strdup ((gchar*) tmp_val);
	}

	if (FcPatternGetString (fpattern, FC_STYLE, 0, &tmp_val) == FcResultMatch) {
		g_free (priv->style);
		priv->style = g_strdup ((gchar*) tmp_val);
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
	asc_font_read_sfnt_data (font);

	/* cleanup */
	FcPatternDestroy (fpattern);
}

static AscFont*
asc_font_new (GError **error)
{
	AscFontPrivate *priv;
	FT_Error err;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&fontconfig_mutex);
	g_autoptr(AscFont) font = ASC_FONT (g_object_new (ASC_TYPE_FONT, NULL));
	priv = GET_PRIVATE (font);

	err = FT_Init_FreeType (&priv->library);
	if (err != 0) {
		g_set_error (error,
			     ASC_FONT_ERROR,
			     ASC_FONT_ERROR_FAILED,
			     "Unable to load FreeType. Error code: %i", err);
		return NULL;
	}

	return g_steal_pointer (&font);
}

/**
 * asc_font_new_from_file:
 * @fname: Name of the file to load.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscFont from a file on the filesystem.
 **/
AscFont*
asc_font_new_from_file (const gchar* fname, GError **error)
{
	AscFont *font;
	AscFontPrivate *priv;
	FT_Error err;

	font = asc_font_new (error);
	if (font == NULL)
		return NULL;
	priv = GET_PRIVATE (font);

	err = FT_New_Face (priv->library, fname, 0, &priv->fface);
	if (err != 0) {
		g_set_error (error,
			     ASC_FONT_ERROR,
			     ASC_FONT_ERROR_FAILED,
			     "Unable to load font face from file. Error code: %i", err);
		return NULL;
	}

	asc_font_load_fontconfig_data_from_file (font, fname);
	g_free (priv->file_basename);
	priv->file_basename = g_path_get_basename (fname);

	return font;
}

/**
 * asc_font_new_from_data:
 * @data: Data to load.
 * @len: Length of the data to load.
 * @file_basename: Font file basename.
 * @error: A #GError or %NULL
 *
 * Creates a new #AscFont from data in memory.
 * The font file basename needs to be supplied as fallback
 * and for heuristics.
 **/
AscFont*
asc_font_new_from_data (const void *data, gssize len, const gchar *file_basename, GError **error)
{
	const gchar *tmp_root;
	gboolean ret;
	g_autofree gchar *fname = NULL;

	/* we unfortunately need to create a stupid temporary file here, otherwise Fontconfig
	 * does not work and we can not determine the right demo strings for this font.
	 * (FreeType itself could load from memory) */
	tmp_root = asc_globals_get_tmp_dir_create ();

        fname = g_build_filename (tmp_root, file_basename, NULL);
#if GLIB_CHECK_VERSION(2,66,0)
	ret = g_file_set_contents_full (fname,
					data,
					len,
					G_FILE_SET_CONTENTS_NONE,
					0666,
					error);
#else
	ret = g_file_set_contents (fname,
				   data,
				   len,
				   error);
	if (ret)
		g_chmod (fname, 0666);
#endif
	if (!ret)
		return NULL;

	return asc_font_new_from_file (fname, error);
}

/**
 * asc_font_get_family:
 * @font: an #AscFont instance.
 *
 * Gets the font family.
 **/
const gchar*
asc_font_get_family (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	return priv->fface->family_name;
}

/**
 * asc_font_get_style:
 * @font: an #AscFont instance.
 *
 * Gets the font style.
 **/
const gchar*
asc_font_get_style (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	return priv->style;
}

/**
 * asc_font_get_fullname:
 * @font: an #AscFont instance.
 *
 * Gets the fonts full name.
 **/
const gchar*
asc_font_get_fullname (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	if (as_is_empty (priv->fullname)) {
		g_free (priv->fullname);
		priv->fullname = g_strdup_printf ("%s %s", asc_font_get_family (font), asc_font_get_style (font));
	}
	return priv->fullname;
}

/**
 * asc_font_get_id:
 * @font: an #AscFont instance.
 *
 * Gets an identifier string for this font.
 **/
const gchar*
asc_font_get_id (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	g_autofree gchar *tmp_family = NULL;
	g_autofree gchar *tmp_style = NULL;
	gchar *tmp;

	if (asc_font_get_family (font) == NULL)
		return priv->file_basename;
	if (asc_font_get_style (font) == NULL)
		return priv->file_basename;
	if (priv->id != NULL)
		return priv->id;

	tmp = g_utf8_strdown (asc_font_get_family (font), -1);
	tmp_family = as_str_replace (tmp, " ", "", 0);
	as_strstripnl (tmp_family);
	g_free (tmp);

	tmp = g_utf8_strdown (asc_font_get_style (font), -1);
	tmp_style = as_str_replace (tmp, " ", "", 0);
	as_strstripnl (tmp_style);
	g_free (tmp);

	g_free (priv->id);
	priv->id = g_strdup_printf ("%s-%s", tmp_family, tmp_style);

	return priv->id;
}

/**
 * asc_font_get_charset:
 * @font: an #AscFont instance.
 *
 * Gets the primary/first character set for this font.
 **/
FT_Encoding
asc_font_get_charset (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	if (priv->fface->num_charmaps == 0)
		return FT_ENCODING_NONE;
	return priv->fface->charmaps[0]->encoding;
}

/**
 * asc_font_get_ftface:
 * @font: an #AscFont instance.
 *
 * Gets the FreeType font face for this font.
 **/
FT_Face
asc_font_get_ftface (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	return priv->fface;
}

/**
 * asc_font_get_language_list:
 * @font: an #AscFont instance.
 *
 * Gets the list of languages supported by this font.
 *
 * Returns: (transfer container) (element-type utf8): Sorted list of languages
 **/
GList*
asc_font_get_language_list (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	GList *list = g_hash_table_get_keys (priv->languages);
	list = g_list_sort (list, (GCompareFunc) g_strcmp0);
	return list;
}

/**
 * asc_font_add_language:
 * @font: an #AscFont instance.
 *
 * Add language to the language list of this font.
 **/
void
asc_font_add_language (AscFont *font, const gchar *lang)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	g_hash_table_add (priv->languages, g_strdup (lang));
}

/**
 * asc_font_get_preferred_language:
 * @font: an #AscFont instance.
 *
 * Gets the fonts preferred language.
 **/
const gchar*
asc_font_get_preferred_language (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	return priv->preferred_lang;
}

/**
 * asc_font_set_preferred_language:
 * @font: an #AscFont instance.
 *
 * Sets the fonts preferred language.
 **/
void
asc_font_set_preferred_language (AscFont *font, const gchar *lang)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	g_free (priv->preferred_lang);
	priv->preferred_lang = g_strdup (lang);
}

/**
 * asc_font_get_description:
 * @font: an #AscFont instance.
 *
 * Gets the font description.
 **/
const gchar*
asc_font_get_description (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	return priv->description;
}

/**
 * asc_font_get_homepage:
 * @font: an #AscFont instance.
 *
 * Gets the font homepage.
 **/
const gchar*
asc_font_get_homepage (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	return priv->homepage;
}

/**
 * asc_font_find_pangram:
 *
 * Find a pangram for the given language, making a random but
 * predictable selection.
 *
 * Returns: A representative text for the language, or %NULL if no specific one was found.
 */
const gchar*
asc_font_find_pangram (AscFont *font, const gchar *lang, const gchar *rand_id)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	GPtrArray *pangrams;
	PangoLanguage *plang;
	const gchar *text;

	if (g_strcmp0 (lang, "en") == 0) {
		guint idx;

		/* we ideally want to have fonts of the same family to share the same pangram */
		if (rand_id == NULL) {
			rand_id = asc_font_get_family (font);
			if (as_is_empty (rand_id))
				rand_id = priv->file_basename;
			if (as_is_empty (rand_id))
				rand_id = asc_font_get_id (font);
		}

		pangrams = asc_globals_get_pangrams_for ("en");
		if (pangrams == NULL) {
			g_warning ("No pangrams found for the english language, even though we should have some available.");
		} else {
			/* select an English pangram */
			idx = g_str_hash (rand_id) % pangrams->len;
			return (const gchar*) g_ptr_array_index (pangrams, idx);
		}
	}

	/* The returned pointer will be valid forever after, and should not be freed. */
	plang = pango_language_from_string (lang);
	text = pango_language_get_sample_string (plang);
	if (g_strcmp0 (text, pango_language_get_sample_string (pango_language_from_string ("xx"))) == 0)
		return NULL;
	return text;
}

/**
 * asc_font_get_icon_text_for_lang:
 *
 * Obtain a font "icon" text for the given language, or
 * return %NULL in case we do not have one explicitly set.
 */
static const gchar*
asc_font_get_icon_text_for_lang (const gchar *lang)
{
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

	for (guint i = 0; text_icon[i].lang != NULL; i++) {
		if (g_strcmp0 (text_icon[i].value, lang) == 0)
			return text_icon[i].value;
	}

	return NULL;
}

/**
 * asc_font_set_fallback_sample_texts_if_needed:
 */
static void
asc_font_set_fallback_sample_texts_if_needed (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);

	if (as_is_empty (priv->sample_text)) {
		g_free (priv->sample_text);
		priv->sample_text = g_strdup ("Lorem ipsum dolor sit amet, consetetur sadipscing elitr.");
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
 * asc_font_determine_sample_texts:
 *
 * Set example texts to display this font.
 */
static void
asc_font_determine_sample_texts (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	g_autoptr(GList) tmp_lang_list = NULL;

	/* If we only have to set the icon text, try to do it!
	 * Otherwise keep cached values and do nothing */
        if (!as_is_empty (priv->sample_text)) {
		asc_font_set_fallback_sample_texts_if_needed (font);
		if (!as_is_empty (priv->sample_icon_text))
			return;
	}

	/* always prefer English (even if not alphabetically first) */
	if (g_hash_table_contains (priv->languages, "en")) {
		g_free (priv->preferred_lang);
		priv->preferred_lang = g_strdup ("en");
	}

	/* ensure we try the preferred language first */
	tmp_lang_list = asc_font_get_language_list (font);
	if (!as_is_empty (priv->preferred_lang))
		tmp_lang_list = g_list_prepend (tmp_lang_list, priv->preferred_lang);

	/* determine our sample texts */
	for (GList *l = tmp_lang_list; l != NULL; l = l->next) {
		const gchar *text = asc_font_find_pangram (font, l->data, NULL);
		if (text == NULL)
			continue;

		g_free (priv->sample_text);
		priv->sample_text = g_strdup (text);

		g_free (priv->sample_icon_text);
		priv->sample_icon_text = g_strdup (asc_font_get_icon_text_for_lang (l->data));
		break;
	}

	/* set some default values if we have been unable to find any texts */
        asc_font_set_fallback_sample_texts_if_needed (font);

	/* check if we have a font that can actually display the characters we picked - in case
	 * it doesn't, we just select random chars. */
	if (FT_Get_Char_Index (priv->fface, g_utf8_get_char_validated (priv->sample_icon_text, -1)) != 0)
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

		g_free (priv->sample_icon_text);
		priv->sample_icon_text = NULL;

		/* if we were unsuccessful at adding chars, set fallback again
		 * (and in this case, also set the icon text to something useful again) */
		asc_font_set_fallback_sample_texts_if_needed (font);
        }
}

/**
 * asc_font_get_sample_text:
 * @font: an #AscFont instance.
 *
 * Gets the sample text for this font.
 **/
const gchar*
asc_font_get_sample_text (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	if (as_is_empty (priv->sample_text))
		asc_font_determine_sample_texts (font);
	return priv->sample_text;
}

/**
 * asc_font_set_sample_text:
 * @font: an #AscFont instance.
 *
 * Sets the sample text for this font.
 **/
void
asc_font_set_sample_text (AscFont *font, const gchar *text)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	g_free (priv->sample_text);
	priv->sample_text = g_strdup (text);
}

/**
 * asc_font_get_sample_icon_text:
 * @font: an #AscFont instance.
 *
 * Gets the sample icon text fragment for this font.
 **/
const gchar*
asc_font_get_sample_icon_text (AscFont *font)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	if (as_is_empty (priv->sample_icon_text))
		asc_font_determine_sample_texts (font);
	return priv->sample_icon_text;
}

/**
 * asc_font_set_sample_icon_text:
 * @font: an #AscFont instance.
 *
 * Sets the sample icon text fragment for this font - must not
 * contain more than 3 characters to be valid.
 **/
void
asc_font_set_sample_icon_text (AscFont *font, const gchar *text)
{
	AscFontPrivate *priv = GET_PRIVATE (font);
	glong len = g_utf8_strlen (text, -1);
	if (len > 3)
		return;
	g_free (priv->sample_icon_text);
	priv->sample_icon_text = g_strdup (text);
}

