/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "config.h"
#include "as-locale-private.h"

#include <string.h>

/**
 * SECTION:as-locale
 * @short_description: Helpers to compare and convert POSIX and BCP47 locales
 * @include: appstream.h
 *
 * Helpers for everything related to locales: Converting locales to their
 * canonical BCP47 form, working out which translations can be shown for a
 * locale, comparing locales with each other, etc.
 */

/**
 * as_locale_get_current_posix:
 *
 * Returns the current locale string in the POSIX format.
 *
 * Returns: (transfer full): A locale string, free with g_free()
 */
gchar *
as_locale_get_current_posix (void)
{
	const gchar *const *locale_names;
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
 * as_locale_get_current_bcp47:
 *
 * Returns the current locale string in the IETF BCP 47 format
 * for use in AppStream XML.
 *
 * Returns: (transfer full): A locale string, free with g_free()
 */
gchar *
as_locale_get_current_bcp47 (void)
{
	g_autofree gchar *locale = as_locale_get_current_posix ();
	return as_utils_posix_locale_to_bcp47 (locale);
}

/* POSIX locale modifiers that name a script, as glibc uses them */
static const struct {
	const gchar *modifier;
	const gchar *script;
} as_locale_script_modifiers[] = {
	{ "cyrillic",	"Cyrl" },
	{ "devanagari", "Deva" },
	{ "latin",	"Latn" },
};

/* The script a language is written in if a locale doesn't name one, for languages that are
 * commonly written in more than one. Where glibc and CLDR disagree, we follow glibc, because
 * locales and translations without a script mostly come from the POSIX world.
 * The entry without a region is the default for a language, the others override it. */
/* clang-format off */
static const struct {
	const gchar *lang;
	const gchar *region;
	const gchar *script;
} as_locale_default_scripts[] = {
	{ "az",  NULL, "Latn" },
	{ "az",  "IR", "Arab" },
	{ "be",  NULL, "Cyrl" },
	{ "bs",  NULL, "Latn" },
	{ "iu",  NULL, "Cans" },
	{ "kk",  NULL, "Cyrl" },
	{ "kok", NULL, "Deva" },
	{ "ks",  NULL, "Arab" },
	{ "mn",  NULL, "Cyrl" },
	{ "mni", NULL, "Beng" },
	{ "ms",  NULL, "Latn" },
	{ "pa",  NULL, "Guru" },
	{ "pa",  "PK", "Arab" },
	{ "sat", NULL, "Deva" },
	{ "sd",  NULL, "Arab" },
	{ "sr",  NULL, "Cyrl" },
	{ "sr",  "ME", "Latn" },
	{ "uz",  NULL, "Latn" },
	{ "zh",  NULL, "Hans" },
	{ "zh",  "HK", "Hant" },
	{ "zh",  "MO", "Hant" },
	{ "zh",  "TW", "Hant" },
};
/* clang-format on */

/* the region that translations for just a language and script are usually made for */
static const struct {
	const gchar *lang;
	const gchar *script;
	const gchar *region;
} as_locale_likely_regions[] = {
	{ "zh", "Hans", "CN" },
	{ "zh", "Hant", "TW" },
};

static gboolean
as_ascii_is_alpha_n (const gchar *str, gsize len)
{
	for (gsize i = 0; i < len; i++) {
		if (!g_ascii_isalpha (str[i]))
			return FALSE;
	}
	return TRUE;
}

static gboolean
as_ascii_is_digit_n (const gchar *str, gsize len)
{
	for (gsize i = 0; i < len; i++) {
		if (!g_ascii_isdigit (str[i]))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_str_equal_n:
 *
 * Returns: TRUE if the first @len bytes of @str are exactly @fixed
 */
static inline gboolean
as_str_equal_n (const gchar *str, gsize len, const gchar *fixed)
{
	return strncmp (str, fixed, len) == 0 && fixed[len] == '\0';
}

typedef struct {
	gchar *dest;
	gsize size;
	gsize pos;
} AsLocaleWriter;

typedef enum {
	AS_LOCALE_CASE_KEEP,
	AS_LOCALE_CASE_LOWER,
	AS_LOCALE_CASE_UPPER,
	AS_LOCALE_CASE_TITLE,
} AsLocaleCase;

static void
as_locale_writer_add (AsLocaleWriter *w, const gchar *subtag, gsize len, AsLocaleCase lcase)
{
	/* the canonical form is never longer than the original, so this only
	 * protects us from mistakes */
	if (w->pos + (w->pos > 0 ? 1 : 0) + len + 1 > w->size)
		return;
	if (w->pos > 0)
		w->dest[w->pos++] = '-';
	for (gsize i = 0; i < len; i++) {
		gchar c = subtag[i];
		if (lcase == AS_LOCALE_CASE_UPPER || (lcase == AS_LOCALE_CASE_TITLE && i == 0))
			c = g_ascii_toupper (c);
		else if (lcase != AS_LOCALE_CASE_KEEP)
			c = g_ascii_tolower (c);
		w->dest[w->pos++] = c;
	}
}

/**
 * as_locale_canonicalize_buf:
 * @locale: a POSIX or BCP47 locale
 * @dest: buffer for the result
 * @dest_size: size of @dest, must be larger than the length of @locale
 *
 * Writes the canonical BCP47 form of @locale to @dest, e.g. "sr-Latn-RS" for
 * "sr_RS.UTF-8@latin" or "zh-Hans" for "zh_hans". The result is never longer
 * than @locale, and "C" stays "C".
 *
 * Returns: %FALSE if @dest is too small.
 */
gboolean
as_locale_canonicalize_buf (const gchar *locale, gchar *dest, gsize dest_size)
{
	AsLocaleWriter w = { dest, dest_size, 0 };
	const gchar *main_end;
	const gchar *modifier = NULL;
	gsize modifier_len = 0;
	const gchar *lang = NULL;
	const gchar *extlang = NULL;
	const gchar *script = NULL;
	const gchar *script_canonical = NULL;
	const gchar *region = NULL;
	gsize lang_len = 0;
	gsize region_len = 0;
	gboolean opaque = FALSE;
	struct {
		const gchar *str;
		gsize len;
	} rest[8];
	guint n_rest = 0;

	/* the untranslated locale is by far the most common one */
	if (locale[0] == 'C' && locale[1] == '\0' && dest_size >= 2) {
		memcpy (dest, "C", 2);
		return TRUE;
	}

	if (strlen (locale) >= dest_size)
		return FALSE;

	/* POSIX locales look like language_REGION.codeset@modifier, we ignore the codeset */
	main_end = locale + strcspn (locale, ".@");
	if (*main_end != '\0') {
		const gchar *at = strchr (main_end, '@');
		if (at != NULL) {
			modifier = at + 1;
			modifier_len = strcspn (modifier, ".");
		}
	}

	/* the untranslated locale */
	if (as_str_equal_n (locale, main_end - locale, "C") ||
	    as_str_equal_n (locale, main_end - locale, "POSIX")) {
		memcpy (dest, "C", 2);
		return TRUE;
	}

	for (const gchar *p = locale; p < main_end;) {
		const gchar *subtag = p;
		gsize len;

		while (p < main_end && *p != '-' && *p != '_')
			p++;
		len = p - subtag;
		if (p < main_end)
			p++;
		if (len == 0)
			continue;

		if (lang == NULL) {
			lang = subtag;
			lang_len = len;
			/* "x-..." (private use) and "i-..." (grandfathered) tags have no structure we know */
			opaque = len == 1;
			continue;
		}

		/* a singleton starts an extension or the private use part, which we keep as it is */
		if (len == 1)
			opaque = TRUE;

		if (!opaque && n_rest == 0 && region == NULL) {
			if (script == NULL && extlang == NULL && len == 3 &&
			    as_ascii_is_alpha_n (subtag, 3)) {
				extlang = subtag;
				continue;
			}
			if (script == NULL && len == 4 && as_ascii_is_alpha_n (subtag, 4)) {
				script = subtag;
				continue;
			}
			if ((len == 2 && as_ascii_is_alpha_n (subtag, 2)) ||
			    (len == 3 && as_ascii_is_digit_n (subtag, 3))) {
				region = subtag;
				region_len = len;
				continue;
			}
		}

		if (n_rest < G_N_ELEMENTS (rest)) {
			rest[n_rest].str = subtag;
			rest[n_rest].len = len;
			n_rest++;
		}
	}

	if (modifier_len > 0 &&
	    !(modifier_len == 4 && g_ascii_strncasecmp (modifier, "euro", 4) == 0)) {
		/* the modifier usually names a script, and if it doesn't, it is a variant */
		for (guint i = 0; i < G_N_ELEMENTS (as_locale_script_modifiers); i++) {
			const gchar *name = as_locale_script_modifiers[i].modifier;
			if (strlen (name) == modifier_len &&
			    g_ascii_strncasecmp (modifier, name, modifier_len) == 0) {
				script_canonical = as_locale_script_modifiers[i].script;
				break;
			}
		}

		if (opaque || script != NULL) {
			script_canonical = NULL;
		} else if (script_canonical == NULL && modifier_len == 4 &&
			   as_ascii_is_alpha_n (modifier, 4)) {
			script = modifier;
		}

		if (script_canonical == NULL && script != modifier &&
		    n_rest < G_N_ELEMENTS (rest)) {
			rest[n_rest].str = modifier;
			rest[n_rest].len = modifier_len;
			n_rest++;
		}
	}

	/* language-extlang-Script-REGION-variants */
	if (lang != NULL)
		as_locale_writer_add (&w, lang, lang_len, AS_LOCALE_CASE_LOWER);
	if (extlang != NULL)
		as_locale_writer_add (&w, extlang, 3, AS_LOCALE_CASE_LOWER);
	if (script_canonical != NULL)
		as_locale_writer_add (&w, script_canonical, 4, AS_LOCALE_CASE_KEEP);
	else if (script != NULL)
		as_locale_writer_add (&w, script, 4, AS_LOCALE_CASE_TITLE);
	if (region != NULL)
		as_locale_writer_add (&w, region, region_len, AS_LOCALE_CASE_UPPER);
	for (guint i = 0; i < n_rest; i++)
		as_locale_writer_add (&w, rest[i].str, rest[i].len, AS_LOCALE_CASE_LOWER);
	dest[w.pos] = '\0';

	return TRUE;
}

/**
 * as_locale_canonicalize:
 * @locale: a POSIX or BCP47 locale
 *
 * Returns: (transfer full): The canonical BCP47 form of @locale.
 */
gchar *
as_locale_canonicalize (const gchar *locale)
{
	gsize size = strlen (locale) + 1;
	gchar *result = g_malloc (size);

	/* this can't fail, as the canonical form is never longer than @locale - but if it
	 * ever does, we would rather return the locale unchanged than uninitialized memory */
	if (G_UNLIKELY (!as_locale_canonicalize_buf (locale, result, size)))
		memcpy (result, locale, size);
	return result;
}

/**
 * as_locale_canonicalize_inplace:
 * @locale: a POSIX or BCP47 locale
 *
 * Replace @locale with its canonical BCP47 form, which is never longer.
 */
void
as_locale_canonicalize_inplace (gchar *locale)
{
	gchar buf[AS_LOCALE_BUF_SIZE];
	g_autofree gchar *tmp = NULL;

	if (as_locale_canonicalize_buf (locale, buf, sizeof (buf))) {
		strcpy (locale, buf);
	} else {
		tmp = as_locale_canonicalize (locale);
		strcpy (locale, tmp);
	}
}

typedef struct {
	const gchar *lang; /* including any extlang */
	gsize lang_len;
	const gchar *script;
	const gchar *region;
	gsize region_len;
	const gchar *rest; /* variants, extensions and private use */
	gboolean opaque;
} AsLocaleParts;

static void
as_locale_parts_parse (const gchar *canonical, AsLocaleParts *parts)
{
	const gchar *p;

	memset (parts, 0, sizeof (*parts));
	parts->lang = canonical;
	parts->lang_len = strcspn (canonical, "-");
	parts->opaque = parts->lang_len <= 1;
	if (parts->opaque)
		return;

	for (p = canonical + parts->lang_len; *p == '-';) {
		const gchar *subtag = p + 1;
		gsize len = strcspn (subtag, "-");

		if (parts->script == NULL && parts->region == NULL) {
			if (len == 3 && subtag == canonical + parts->lang_len + 1 &&
			    as_ascii_is_alpha_n (subtag, 3)) {
				parts->lang_len += 4;
				p = subtag + len;
				continue;
			}
			if (len == 4 && as_ascii_is_alpha_n (subtag, 4)) {
				parts->script = subtag;
				p = subtag + len;
				continue;
			}
		}
		if (parts->region == NULL && ((len == 2 && as_ascii_is_alpha_n (subtag, 2)) ||
					      (len == 3 && as_ascii_is_digit_n (subtag, 3)))) {
			parts->region = subtag;
			parts->region_len = len;
			p = subtag + len;
			continue;
		}

		parts->rest = subtag;
		break;
	}
}

static const gchar *
as_locale_default_script (const gchar *lang, gsize lang_len, const gchar *region, gsize region_len)
{
	const gchar *script = NULL;

	for (guint i = 0; i < G_N_ELEMENTS (as_locale_default_scripts); i++) {
		if (!as_str_equal_n (lang, lang_len, as_locale_default_scripts[i].lang))
			continue;
		if (as_locale_default_scripts[i].region == NULL)
			script = as_locale_default_scripts[i].script;
		else if (region != NULL &&
			 as_str_equal_n (region, region_len, as_locale_default_scripts[i].region))
			return as_locale_default_scripts[i].script;
	}

	return script;
}

static void
as_locale_chain_add (AsLocaleChain *chain,
		     const AsLocaleParts *parts,
		     const gchar *script,
		     const gchar *region,
		     gsize region_len,
		     const gchar *rest)
{
	gchar tmp[AS_LOCALE_CHAIN_BUF_SIZE];
	AsLocaleWriter w = { tmp, sizeof (tmp), 0 };

	if (parts->lang_len + 1 > sizeof (tmp))
		return;
	memcpy (tmp, parts->lang, parts->lang_len);
	w.pos = parts->lang_len;
	if (script != NULL)
		as_locale_writer_add (&w, script, 4, AS_LOCALE_CASE_KEEP);
	if (region != NULL)
		as_locale_writer_add (&w, region, region_len, AS_LOCALE_CASE_KEEP);
	if (rest != NULL)
		as_locale_writer_add (&w, rest, strlen (rest), AS_LOCALE_CASE_KEEP);
	tmp[w.pos] = '\0';

	if (as_locale_chain_contains (chain, tmp))
		return;
	if (chain->n_entries == AS_LOCALE_CHAIN_MAX_ENTRIES ||
	    chain->buf_used + w.pos + 1 > AS_LOCALE_CHAIN_BUF_SIZE)
		return;

	memcpy (chain->buf + chain->buf_used, tmp, w.pos + 1);
	chain->offsets[chain->n_entries++] = chain->buf_used;
	chain->buf_used += w.pos + 1;
}

/**
 * as_locale_chain_init:
 * @chain: the chain to fill
 * @canonical_locale: a locale in canonical BCP47 form
 *
 * Fill @chain with the locales whose translations can be used for @canonical_locale,
 * best match first. For "zh-CN" that is "zh-CN", "zh-Hans-CN", "zh-Hans" and "zh",
 * and for "sr-Latn-RS" it is "sr-Latn-RS", "sr-Latn" and "sr".
 */
void
as_locale_chain_init (AsLocaleChain *chain, const gchar *canonical_locale)
{
	AsLocaleParts p;
	const gchar *default_script;
	const gchar *script;

	chain->n_entries = 0;
	chain->n_exact = 0;
	chain->buf_used = 0;

	as_locale_parts_parse (canonical_locale, &p);
	if (p.opaque) {
		AsLocaleParts whole = { .lang = canonical_locale,
					.lang_len = strlen (canonical_locale) };
		as_locale_chain_add (chain, &whole, NULL, NULL, 0, NULL);
		chain->n_exact = chain->n_entries;
		return;
	}

	default_script = as_locale_default_script (p.lang, p.lang_len, p.region, p.region_len);
	script = p.script != NULL ? p.script : default_script;

	/* the locale itself, and the same locale with its script spelled out, or left out if
	 * the region implies it */
	as_locale_chain_add (chain, &p, p.script, p.region, p.region_len, p.rest);
	if (p.rest == NULL && p.region != NULL) {
		if (script != NULL)
			as_locale_chain_add (chain, &p, script, p.region, p.region_len, NULL);
		if (p.script != NULL && default_script != NULL &&
		    strncmp (p.script, default_script, 4) == 0)
			as_locale_chain_add (chain, &p, NULL, p.region, p.region_len, NULL);
	}
	chain->n_exact = chain->n_entries;

	/* variants are more important than the region, so "ca-ES-valencia" falls back
	 * to "ca-valencia" before "ca-ES" */
	if (p.rest != NULL) {
		if (p.region != NULL)
			as_locale_chain_add (chain, &p, p.script, NULL, 0, p.rest);
		if (p.region != NULL || p.script != NULL)
			as_locale_chain_add (chain, &p, p.script, p.region, p.region_len, NULL);
		if (p.region != NULL && script != NULL)
			as_locale_chain_add (chain, &p, script, p.region, p.region_len, NULL);
	}

	if (script != NULL) {
		as_locale_chain_add (chain, &p, script, NULL, 0, NULL);

		/* "zh-Hans" is also what "zh-CN" is written in */
		if (p.region == NULL) {
			for (guint i = 0; i < G_N_ELEMENTS (as_locale_likely_regions); i++) {
				const gchar *region = as_locale_likely_regions[i].region;
				if (!as_str_equal_n (p.lang,
						     p.lang_len,
						     as_locale_likely_regions[i].lang) ||
				    strncmp (script, as_locale_likely_regions[i].script, 4) != 0)
					continue;
				as_locale_chain_add (chain,
						     &p,
						     script,
						     region,
						     strlen (region),
						     NULL);
				as_locale_chain_add (chain,
						     &p,
						     NULL,
						     region,
						     strlen (region),
						     NULL);
			}
		}
	}

	/* We always fall back to just the language, even if it is usually written in a different
	 * script, as gettext does. "sr" is often the only Serbian translation there is. */
	as_locale_chain_add (chain, &p, NULL, NULL, 0, NULL);
}

/**
 * as_locale_chain_contains:
 * @chain: an #AsLocaleChain
 * @canonical_locale: a locale in canonical BCP47 form
 *
 * Returns: %TRUE if translations for @canonical_locale can be used for the locale of @chain.
 */
gboolean
as_locale_chain_contains (const AsLocaleChain *chain, const gchar *canonical_locale)
{
	for (guint i = 0; i < chain->n_entries; i++) {
		if (strcmp (as_locale_chain_get (chain, i), canonical_locale) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * as_utils_posix_locale_to_bcp47:
 *
 * Converts a POSIX locale string to the corresponding IETF BCP47 format.
 * If the given locale is already in BCP47 format, it is brought into its
 * canonical form, e.g. "zh-Hans" for "zh-hans".
 *
 * Returns: (transfer full): A locale string, free with g_free()
 */
gchar *
as_utils_posix_locale_to_bcp47 (const gchar *locale)
{
	if (locale == NULL)
		return NULL;

	/* not a locale, but the magic value to read all of them, which is passed through here */
	if (g_strcmp0 (locale, "ALL") == 0)
		return g_strdup (locale);

	return as_locale_canonicalize (locale);
}

/**
 * as_locale_is_posix:
 * @locale: (nullable): The locale to test.
 *
 * Guess if the given locale is in POSIX format, or whether it is
 * in another format, e.g. BCP47.
 *
 * Returns: %TRUE if we have a POSIX locale, %FALSE otherwise.
 */
gboolean
as_locale_is_posix (const gchar *locale)
{
	/* we assume NULL is valid, for compatibility */
	if (locale == NULL)
		return TRUE;

	for (guint i = 0; locale[i] != '\0'; i++) {
		if (locale[i] == '-')
			return FALSE;
		if (locale[i] == '_')
			return TRUE;
		if (locale[i] == '@')
			return TRUE;
	}

	/* if we are here, the string contained no hyphens (which would indicate BCP47),
	 * but also no underscores or @ (which would indicate POSIX), so it may be something
	 * like "de", for which we simply assume it is a POSIX locale. */
	return TRUE;
}

/**
 * as_locale_is_bcp47:
 * @locale: (nullable): The locale to test.
 *
 * Guess if the given locale is in BCP47 format.
 *
 * Returns: %TRUE if we likely have a BCP47 locale, %FALSE otherwise.
 */
gboolean
as_locale_is_bcp47 (const gchar *locale)
{
	/* we assume NULL is valid, for compatibility */
	if (locale == NULL)
		return TRUE;

	for (guint i = 0; locale[i] != '\0'; i++) {
		if (locale[i] == '-')
			return TRUE;
		if (locale[i] == '_')
			return FALSE;
		if (locale[i] == '@')
			return FALSE;
	}

	/* if we are here, the string contained no hyphens (which would indicate BCP47),
	 * but also no underscores or @ (which would indicate POSIX), so it may be something
	 * like "de", for which we simply assume it is a BCP47 locale. */
	return TRUE;
}

/**
 * as_locale_is_cruft:
 *
 * Checks whether the given locale is valid or a cruft or dummy
 * locale.
 */
gboolean
as_locale_is_cruft (const gchar *locale)
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
gchar *
as_locale_strip_encoding (const gchar *locale)
{
	const gchar *codeset;
	const gchar *modifier;

	if (locale == NULL)
		return NULL;

	/* the codeset comes before any modifier, e.g. "sr_RS.UTF-8@latin" */
	codeset = strchr (locale, '.');
	if (codeset == NULL)
		return g_strdup (locale);
	modifier = strchr (codeset, '@');
	if (modifier == NULL)
		return g_strndup (locale, codeset - locale);
	return g_strdup_printf ("%.*s%s", (gint) (codeset - locale), locale, modifier);
}

/**
 * as_locale_language_len:
 * @locale: the BCP47 or POSIX locale string.
 *
 * Get the length of the language part at the start of a locale string.
 */
static gsize
as_locale_language_len (const gchar *locale)
{
	const gchar *sep;
	const gchar *modifier;
	gsize len;

	/* the language ends at the - for BCP47 locales, or else at the _ for POSIX
	 * locales (the language is not always 2 chars!) */
	sep = strchr (locale, '-');
	if (sep == NULL)
		sep = strchr (locale, '_');
	len = sep != NULL ? (gsize) (sep - locale) : strlen (locale);

	/* ...or before any "@" for locales with modifiers like "ca@valencia" */
	modifier = memchr (locale, '@', len);
	if (modifier != NULL)
		len = modifier - locale;

	return len;
}

/**
 * as_locale_to_language:
 * @locale: (nullable): the BCP47 or POSIX locale string.
 *
 * Get language part from a locale string in either BCP47 or POSIX format.
 */
gchar *
as_locale_to_language (const gchar *locale)
{
	/* invalid */
	if (locale == NULL)
		return NULL;

	return g_strndup (locale, as_locale_language_len (locale));
}

/**
 * as_utils_locale_is_compatible:
 * @locale1: (nullable): a BCP47 or POSIX locale string, or %NULL
 * @locale2: (nullable): a BCP47 or POSIX locale string, or %NULL
 *
 * Calculates if one locale is compatible with another, meaning that translations
 * for one of them can be shown for the other.
 * Locales may be in POSIX or BCP47 format, in any spelling, and scripts implied by
 * a region are taken into account, so "zh_CN" is compatible with "zh-Hans" and
 * "sr_RS@latin" with "sr-Latn". Any locale is compatible with its plain language.
 *
 * Returns: %TRUE if the locale is compatible.
 *
 * Since: 0.9.5
 **/
gboolean
as_utils_locale_is_compatible (const gchar *locale1, const gchar *locale2)
{
	gchar buf1[AS_LOCALE_BUF_SIZE];
	gchar buf2[AS_LOCALE_BUF_SIZE];
	g_autofree gchar *tmp1 = NULL;
	g_autofree gchar *tmp2 = NULL;
	const gchar *canon1 = buf1;
	const gchar *canon2 = buf2;
	AsLocaleChain chain;

	/* we've specified "don't care" and locale unspecified */
	if (locale1 == NULL && locale2 == NULL)
		return TRUE;

	/* only one specified, so we check it against the locales of the current user */
	if (locale1 == NULL || locale2 == NULL) {
		const gchar *locale = locale1 != NULL ? locale1 : locale2;
		const gchar *const *locales = g_get_language_names ();

		for (guint i = 0; locales[i] != NULL; i++) {
			if (as_utils_locale_is_compatible (locales[i], locale))
				return TRUE;
		}
		return FALSE;
	}

	/* This function is called a lot while reading metadata, and most of the time
	 * the locales are either the same or for different languages, so we check
	 * that before doing anything else. */
	if (strcmp (locale1, locale2) == 0)
		return TRUE;
	if (!as_locale_same_language (locale1, locale2))
		return FALSE;

	if (!as_locale_canonicalize_buf (locale1, buf1, sizeof (buf1)))
		canon1 = tmp1 = as_locale_canonicalize (locale1);
	if (!as_locale_canonicalize_buf (locale2, buf2, sizeof (buf2)))
		canon2 = tmp2 = as_locale_canonicalize (locale2);
	if (g_strcmp0 (canon1, canon2) == 0)
		return TRUE;

	/* translations for one of them can be used for the other */
	as_locale_chain_init (&chain, canon1);
	if (as_locale_chain_contains (&chain, canon2))
		return TRUE;
	as_locale_chain_init (&chain, canon2);
	return as_locale_chain_contains (&chain, canon1);
}
