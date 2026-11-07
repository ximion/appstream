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

#include "as-stemmer.h"

#include <config.h>
#include <glib.h>
#include <string.h>
#ifdef HAVE_STEMMING
#ifdef HAVE_LIBSTEMMER_SUBDIR
#include <libstemmer/libstemmer.h>
#else
#include <libstemmer.h>
#endif
#endif

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-locale-private.h"

/**
 * SECTION:as-stemmer
 * @short_description: Stemming helper singleton for AppStream searches.
 */

struct _AsStemmer {
	GObject parent_instance;

	struct sb_stemmer *sb;
	gchar *current_lang;
	GMutex mutex;

	GHashTable *stem_cache;
	guint cache_holds;
};

G_DEFINE_TYPE (AsStemmer, as_stemmer, G_TYPE_OBJECT)

static gpointer as_stemmer_object = NULL;

static void as_stemmer_reload_internal (AsStemmer *stemmer, const gchar *locale, gboolean force);

/**
 * as_stemmer_finalize:
 **/
static void
as_stemmer_finalize (GObject *object)
{
#ifdef HAVE_STEMMING
	AsStemmer *stemmer = AS_STEMMER (object);

	{
		/* we still lock here to create an easier-to-investigate crash in case the
		 * object is ever destroyed while something else is using it in parallel. */
		g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&stemmer->mutex);
		sb_stemmer_delete (g_steal_pointer (&stemmer->sb));
		g_clear_pointer (&stemmer->stem_cache, g_hash_table_unref);
	}
	g_mutex_clear (&stemmer->mutex);
#endif

	G_OBJECT_CLASS (as_stemmer_parent_class)->finalize (object);
}

/**
 * as_stemmer_init:
 **/
static void
as_stemmer_init (AsStemmer *stemmer)
{
#ifdef HAVE_STEMMING
	g_autofree gchar *locale = NULL;

	g_mutex_init (&stemmer->mutex);

	/* we don't use the locale in XML, so it can be POSIX */
	locale = as_locale_get_current_posix ();

	/* force a reload for initialization */
	as_stemmer_reload_internal (stemmer, locale, TRUE);
#endif
}

static void
as_stemmer_reload_internal (AsStemmer *stemmer, const gchar *locale, gboolean force)
{
#ifdef HAVE_STEMMING
	g_autofree gchar *lang = NULL;
	GMutexLocker *locker = NULL;

	/* check if we need to reload */
	lang = as_locale_to_language (locale);
	locker = g_mutex_locker_new (&stemmer->mutex);
	if (!force && as_str_equal0 (lang, stemmer->current_lang)) {
		g_mutex_locker_free (locker);
		return;
	}

	g_free (stemmer->current_lang);
	stemmer->current_lang = g_steal_pointer (&lang);

	/* the stems we have cached are for the previous language */
	if (stemmer->stem_cache != NULL)
		g_hash_table_remove_all (stemmer->stem_cache);

	/* reload stemmer */
	sb_stemmer_delete (stemmer->sb);
	stemmer->sb = sb_stemmer_new (stemmer->current_lang, NULL);
	if (stemmer->sb == NULL)
		g_debug ("Language %s can not be stemmed.", stemmer->current_lang);
	else
		g_debug ("Stemming language is now: %s", stemmer->current_lang);

	g_mutex_locker_free (locker);
#endif
}

/**
 * as_stemmer_reload:
 * @stemmer: A #AsStemmer
 * @locale: The stemming language as POSIX locale.
 *
 * Allows realoading the #AsStemmer with a different language.
 * Does nothing if the stemmer is already using the selected language.
 */
void
as_stemmer_reload (AsStemmer *stemmer, const gchar *locale)
{
	as_stemmer_reload_internal (stemmer, locale, FALSE);
}

#ifdef HAVE_STEMMING
/**
 * as_stemmer_cache_add:
 *
 * Add a term and its stem to the cache. The mutex must be held.
 *
 * Returns: The cached stem.
 */
static const gchar *
as_stemmer_cache_add (AsStemmer *stemmer, const gchar *term, const gchar *stem)
{
	gsize term_len = strlen (term);
	gsize stem_len = strlen (stem);
	gchar *entry;

	/* We store the term and its stem in one allocation, which saves memory as both are
	 * usually short. The term is the key, and the value points to the stem behind it. */
	entry = g_malloc (term_len + stem_len + 2);
	memcpy (entry, term, term_len + 1);
	memcpy (entry + term_len + 1, stem, stem_len + 1);
	g_hash_table_insert (stemmer->stem_cache, entry, entry + term_len + 1);

	return entry + term_len + 1;
}
#endif

/**
 * as_stemmer_stem:
 * @stemmer: A #AsStemmer
 * @term: The input term to stem.
 *
 * Stems a string using Snowball.
 *
 * Returns: A stemmed string.
 **/
gchar *
as_stemmer_stem (AsStemmer *stemmer, const gchar *term)
{
#ifdef HAVE_STEMMING
	gchar *result;
	GMutexLocker *locker = g_mutex_locker_new (&stemmer->mutex);
	if (stemmer->sb == NULL) {
		g_mutex_locker_free (locker);
		return g_strdup (term);
	}

	if (stemmer->stem_cache != NULL) {
		const gchar *stem = g_hash_table_lookup (stemmer->stem_cache, term);
		if (stem == NULL) {
			stem = (const gchar *) sb_stemmer_stem (stemmer->sb,
								(unsigned char *) term,
								strlen (term));
			if (stem != NULL)
				stem = as_stemmer_cache_add (stemmer, term, stem);
		}
		result = g_strdup (stem);
	} else {
		result = g_strdup (
		    (gchar *) sb_stemmer_stem (stemmer->sb, (unsigned char *) term, strlen (term)));
	}

	g_mutex_locker_free (locker);

	/* Snowball sometimes stems tokens to an empty string,
	 * for example the Turkish "leri" token. See issue #264
	 * In this case, we currently just filter out the token,
	 * as this sort of stemming seems to generally indicate an
	 * unsuitable search token. */
	if ((result != NULL) && (result[0] == '\0')) {
		g_free (result);
		return NULL;
	}
	return result;
#else
	return g_strdup (term);
#endif
}

/**
 * as_stemmer_cache_hold:
 * @stemmer: A #AsStemmer
 *
 * Remember the stem of every term until as_stemmer_cache_release() is called,
 * so a word that comes up many times is only stemmed once. This is useful when
 * we stem a lot of text in one go, like when writing the cache.
 *
 * Holds may overlap, e.g. when two pools write their caches at the same time.
 * The cache is freed when the last hold is released.
 */
void
as_stemmer_cache_hold (AsStemmer *stemmer)
{
#ifdef HAVE_STEMMING
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&stemmer->mutex);

	if (stemmer->cache_holds++ == 0)
		stemmer->stem_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
#endif
}

/**
 * as_stemmer_cache_release:
 * @stemmer: A #AsStemmer
 *
 * Release a hold on the stem cache that was taken with as_stemmer_cache_hold(),
 * and free the cache if nothing else holds it.
 */
void
as_stemmer_cache_release (AsStemmer *stemmer)
{
#ifdef HAVE_STEMMING
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&stemmer->mutex);

	g_return_if_fail (stemmer->cache_holds > 0);
	if (--stemmer->cache_holds == 0)
		g_clear_pointer (&stemmer->stem_cache, g_hash_table_unref);
#endif
}

/**
 * as_stemmer_class_init:
 **/
static void
as_stemmer_class_init (AsStemmerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_stemmer_finalize;
}

/**
 * as_stemmer_get:
 * @locale: The stemming language as POSIX locale.
 *
 * Gets the global #AsStemmer instance.
 *
 * Returns: (transfer none): an #AsStemmer
 **/
AsStemmer *
as_stemmer_get (const gchar *locale)
{
	AsStemmer *stemmer;
	if (as_stemmer_object == NULL) {
		as_stemmer_object = g_object_new (AS_TYPE_STEMMER, NULL);
		g_object_add_weak_pointer (as_stemmer_object, &as_stemmer_object);
	}
	stemmer = AS_STEMMER (as_stemmer_object);

	/* load current locale if locale was NULL */
	if (locale == NULL) {
		g_autofree gchar *sys_locale = as_locale_get_current_posix ();
		as_stemmer_reload (stemmer, sys_locale);
		return stemmer;
	}

	/* load English for standard C locale */
	if (g_str_has_prefix (locale, "C"))
		as_stemmer_reload (stemmer, "en");
	else
		as_stemmer_reload (stemmer, locale);
	return stemmer;
}
