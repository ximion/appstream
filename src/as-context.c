/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-context
 * @short_description: Context of an AppStream metadata document
 * @include: appstream.h
 *
 * Contains information about the context of AppStream metadata, from the
 * root node of the document.
 * Instances of #AsContext may be shared between #AsComponent instances.
 *
 * You usually do not want to use this directly, but use the more convenient
 * #AsMetadata instead.
 *
 * See also: #AsComponent, #AsMetadata
 */

#include "config.h"
#include "as-context.h"
#include "as-context-private.h"

#include "as-utils-private.h"

typedef struct {
	AsFormatVersion format_version;
	AsFormatStyle style;
	AsValueFlags value_flags;
	GRefString *locale;
	GRefString *origin;
	GRefString *media_baseurl;
	GRefString *arch;
	GRefString *fname;
	gint priority;

	gboolean internal_mode;
	gboolean all_locale;

	gchar **free_origin_globs;
	AsCurl *curl;
	GMutex mutex;
} AsContextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsContext, as_context, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_context_get_instance_private (o))

/**
 * as_format_version_to_string:
 * @version: the #AsFormatKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @version
 *
 * Since: 0.10.0
 **/
const gchar *
as_format_version_to_string (AsFormatVersion version)
{
	if (version == AS_FORMAT_VERSION_V1_0)
		return "1.0";
	return "x.xx";
}

/**
 * as_format_version_from_string:
 * @version_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsFormatVersion. For unknown, the highest version
 * number is assumed.
 *
 * Since: 0.10.0
 **/
AsFormatVersion
as_format_version_from_string (const gchar *version_str)
{
	if (g_strcmp0 (version_str, "1.0") == 0)
		return AS_FORMAT_VERSION_V1_0;
	return AS_FORMAT_VERSION_UNKNOWN;
}

/**
 * as_format_kind_to_string:
 * @kind: the #AsFormatKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.10.0
 **/
const gchar *
as_format_kind_to_string (AsFormatKind kind)
{
	if (kind == AS_FORMAT_KIND_XML)
		return "xml";
	if (kind == AS_FORMAT_KIND_YAML)
		return "yaml";
	return "unknown";
}

/**
 * as_format_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsFormatKind or %AS_FORMAT_KIND_UNKNOWN for unknown
 *
 * Since: 0.10.0
 **/
AsFormatKind
as_format_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "xml") == 0)
		return AS_FORMAT_KIND_XML;
	if (g_strcmp0 (kind_str, "yaml") == 0)
		return AS_FORMAT_KIND_YAML;
	return AS_FORMAT_KIND_UNKNOWN;
}

static void
as_context_finalize (GObject *object)
{
	AsContext *ctx = AS_CONTEXT (object);
	AsContextPrivate *priv = GET_PRIVATE (ctx);

	as_ref_string_release (priv->locale);
	as_ref_string_release (priv->origin);
	as_ref_string_release (priv->media_baseurl);
	as_ref_string_release (priv->arch);
	as_ref_string_release (priv->fname);
	g_mutex_clear (&priv->mutex);

	if (priv->free_origin_globs != NULL)
		g_strfreev (priv->free_origin_globs);
	if (priv->curl != NULL)
		g_object_unref (priv->curl);

	G_OBJECT_CLASS (as_context_parent_class)->finalize (object);
}

static void
as_context_init (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);

	g_mutex_init (&priv->mutex);
	priv->format_version = AS_FORMAT_VERSION_LATEST;
	priv->style = AS_FORMAT_STYLE_UNKNOWN;
	priv->priority = 0;
	priv->internal_mode = FALSE;
}

static void
as_context_class_init (AsContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_context_finalize;
}

/**
 * as_context_get_format_version:
 * @ctx: a #AsContext instance.
 *
 * Returns: The AppStream format version.
 **/
AsFormatVersion
as_context_get_format_version (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return g_atomic_int_get (&priv->format_version);
}

/**
 * as_context_set_format_version:
 * @ctx: a #AsContext instance.
 * @ver: the new format version.
 *
 * Sets the AppStream format version.
 **/
void
as_context_set_format_version (AsContext *ctx, AsFormatVersion ver)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	g_atomic_int_set (&priv->format_version, ver);
}

/**
 * as_context_get_style:
 * @ctx: a #AsContext instance.
 *
 * Returns: The document style.
 **/
AsFormatStyle
as_context_get_style (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return g_atomic_int_get (&priv->style);
}

/**
 * as_context_set_style:
 * @ctx: a #AsContext instance.
 * @style: the new document style.
 *
 * Sets the AppStream document style.
 **/
void
as_context_set_style (AsContext *ctx, AsFormatStyle style)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	g_atomic_int_set (&priv->style, style);
}

/**
 * as_context_get_priority:
 * @ctx: a #AsContext instance.
 *
 * Returns: The data priority.
 **/
gint
as_context_get_priority (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return g_atomic_int_get (&priv->priority);
}

/**
 * as_context_set_priority:
 * @ctx: a #AsContext instance.
 * @priority: the new priority.
 *
 * Sets the data priority.
 **/
void
as_context_set_priority (AsContext *ctx, gint priority)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	g_atomic_int_set (&priv->priority, priority);
}

/**
 * as_context_get_origin:
 * @ctx: a #AsContext instance.
 *
 * Returns: The data origin.
 **/
const gchar *
as_context_get_origin (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->origin;
}

/**
 * as_context_set_origin:
 * @ctx: a #AsContext instance.
 * @value: the new value.
 *
 * Sets the data origin.
 **/
void
as_context_set_origin (AsContext *ctx, const gchar *value)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	as_ref_string_assign_safe (&priv->origin, value);
}

/**
 * as_context_get_locale:
 * @ctx: a #AsContext instance.
 *
 * Returns: The active locale in BCP47 format.
 **/
const gchar *
as_context_get_locale (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->locale;
}

/**
 * as_context_set_locale:
 * @ctx: a #AsContext instance.
 * @locale: (nullable): a POSIX or BCP47 locale, or %NULL. e.g. "en_GB"
 *
 * Sets the active locale.
 * If the magic value "ALL" is used, the current system locale will be used
 * for data reading, but when writing data all locale will be written.
 **/
void
as_context_set_locale (AsContext *ctx, const gchar *locale)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);

	g_atomic_int_set (&priv->all_locale, FALSE);
	if (g_strcmp0 (locale, "ALL") == 0) {
		g_autofree gchar *tmp = as_get_current_locale_bcp47 ();
		g_atomic_int_set (&priv->all_locale, TRUE);
		as_ref_string_assign_safe (&priv->locale, tmp);
	} else {
		g_autofree gchar *bcp47 = as_utils_posix_locale_to_bcp47 (locale);
		as_ref_string_assign_safe (&priv->locale, bcp47);
	}
}

/**
 * as_context_get_locale_use_all:
 * @ctx: a #AsContext instance.
 *
 * Returns: %TRUE if all locale should be parsed.
 **/
gboolean
as_context_get_locale_use_all (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return g_atomic_int_get (&priv->all_locale);
}

/**
 * as_context_has_media_baseurl:
 * @ctx: a #AsContext instance.
 *
 * Returns: %TRUE if a media base URL is set.
 **/
gboolean
as_context_has_media_baseurl (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->media_baseurl != NULL;
}

/**
 * as_context_get_media_baseurl:
 * @ctx: a #AsContext instance.
 *
 * Returns: The media base URL.
 **/
const gchar *
as_context_get_media_baseurl (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->media_baseurl;
}

/**
 * as_context_set_media_baseurl:
 * @ctx: a #AsContext instance.
 * @value: the new value.
 *
 * Sets the media base URL.
 **/
void
as_context_set_media_baseurl (AsContext *ctx, const gchar *value)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	as_ref_string_assign_safe (&priv->media_baseurl, value);
}

/**
 * as_context_get_architecture:
 * @ctx: a #AsContext instance.
 *
 * Returns: The current architecture for the document.
 **/
const gchar *
as_context_get_architecture (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->arch;
}

/**
 * as_context_set_architecture:
 * @ctx: a #AsContext instance.
 * @value: the new value.
 *
 * Sets the current architecture for this document.
 **/
void
as_context_set_architecture (AsContext *ctx, const gchar *value)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	as_ref_string_assign_safe (&priv->arch, value);
}

/**
 * as_context_get_filename:
 * @ctx: a #AsContext instance.
 *
 * Returns: The name of the file the data originates from.
 **/
const gchar *
as_context_get_filename (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->fname;
}

/**
 * as_context_set_filename:
 * @ctx: a #AsContext instance.
 * @fname: the new file name.
 *
 * Sets the file name we are loading data from.
 **/
void
as_context_set_filename (AsContext *ctx, const gchar *fname)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	as_ref_string_assign_safe (&priv->fname, fname);
}

/**
 * as_context_set_value_flags:
 * @ctx: a #AsContext instance.
 * @flags: #AsValueFlags to set on @cpt.
 *
 */
void
as_context_set_value_flags (AsContext *ctx, AsValueFlags flags)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	priv->value_flags = flags;
}

/**
 * as_context_get_value_flags:
 * @ctx: a #AsContext instance.
 *
 * Returns: The #AsValueFlags that are set on @cpt.
 *
 */
AsValueFlags
as_context_get_value_flags (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->value_flags;
}

/**
 * as_context_get_internal_mode:
 * @ctx: a #AsContext instance.
 *
 * Returns: %TRUE if internal-mode XML is generated.
 **/
gboolean
as_context_get_internal_mode (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return g_atomic_int_get (&priv->internal_mode);
}

/**
 * as_context_set_internal_mode:
 * @ctx: a #AsContext instance.
 * @enabled: %TRUE if enabled.
 *
 * In internal mode, serializers will generate
 * a bit of additional XML used internally by AppStream
 * (e.g. for database serialization).
 **/
void
as_context_set_internal_mode (AsContext *ctx, gboolean enabled)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	g_atomic_int_set (&priv->internal_mode, enabled);
}

/**
 * as_context_localized_ht_get:
 * @ctx: a #AsContext instance, or %NULL
 * @lht: (element-type utf8 utf8): the #GHashTable from which the value will be retreived.
 * @locale_override: Override the default locale defined by @ctx, or %NULL
 *
 * Helper function to get a value for the current locale from a localization
 * hash table (which maps locale to localized strings).
 *
 * This is used by all entities which have a context and have localized strings.
 *
 * Returns: The localized string in the best matching localization.
 */
const gchar *
as_context_localized_ht_get (AsContext *ctx, GHashTable *lht, const gchar *locale_override)
{
	const gchar *locale;
	const gchar *msg;
	AsContextPrivate *priv = NULL;
	AsValueFlags value_flags = AS_VALUE_FLAG_NONE;

	if (ctx != NULL) {
		priv = GET_PRIVATE (ctx);
		value_flags = priv->value_flags;
	}

	/* retrieve context locale, if the locale isn't explicitly overridden */
	if (priv != NULL && locale_override == NULL) {
		locale = priv->locale;
	} else {
		locale = locale_override;
	}

	/* NULL is not an acceptable value here and means "C" */
	if (locale == NULL)
		locale = "C";

	msg = g_hash_table_lookup (lht, locale);
	if ((msg == NULL) &&
	    (!as_flags_contains (value_flags, AS_VALUE_FLAG_NO_TRANSLATION_FALLBACK))) {
		g_autofree gchar *lang = as_utils_locale_to_language (locale);
		/* fall back to language string */
		msg = g_hash_table_lookup (lht, lang);
		if (msg == NULL) {
			/* fall back to untranslated / default */
			msg = g_hash_table_lookup (lht, "C");
		}
	}

	return msg;
}

/**
 * as_context_localized_ht_set:
 * @ctx: a #AsContext instance, or %NULL
 * @lht: (element-type utf8 utf8): the #GHashTable to which the value will be added.
 * @value: the value to add.
 * @locale: (nullable): the BCP47 locale, or %NULL. e.g. "en-GB".
 *
 * Helper function to set a localized value on a trabslation mapping.
 *
 * This is used by all entities which have a context and have localized strings.
 */
void
as_context_localized_ht_set (AsContext *ctx,
			     GHashTable *lht,
			     const gchar *value,
			     const gchar *locale)
{
	const gchar *selected_locale;
	g_autofree gchar *locale_noenc = NULL;

	/* if no locale was specified, we assume the default locale
	 * NOTE: %NULL does NOT necessarily mean lang=C here! */
	if ((ctx != NULL) && (locale == NULL)) {
		AsContextPrivate *priv = GET_PRIVATE (ctx);
		selected_locale = priv->locale;
	} else {
		selected_locale = locale;
	}
	/* if we still have no locale, assume "C" as best option */
	if (selected_locale == NULL)
		selected_locale = "C";

	locale_noenc = as_locale_strip_encoding (selected_locale);
	g_hash_table_insert (lht, g_ref_string_new_intern (locale_noenc), g_strdup (value));
}

/**
 * as_context_get_curl:
 * @ctx: a #AsContext instance, or %NULL
 * @error: a #GError
 *
 * Get an #AsCurl instance.
 *
 * Returns: (transfer full): an #AsCurl reference.
 */
AsCurl *
as_context_get_curl (AsContext *ctx, GError **error)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	if (priv->curl == NULL) {
		priv->curl = as_curl_new (error);
		if (priv->curl == NULL)
			return NULL;
	}
	return g_object_ref (priv->curl);
}

static void
as_context_ensure_os_config_loaded (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	gboolean ret;
	const gchar *as_config_fname = NULL;
	g_autofree gchar *distro_id = NULL;
	g_autoptr(GKeyFile) kf = NULL;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	/* return if we already loaded all data */
	if (priv->free_origin_globs != NULL)
		return;

	/* load data from /etc, but fall back to the default in /usr/share if the override does not exist */
	as_config_fname = SYSCONFDIR "/appstream.conf";
	if (!g_file_test (as_config_fname, G_FILE_TEST_EXISTS))
		as_config_fname = AS_DATADIR "/appstream.conf";
	g_debug ("Loading OS configuration from: %s", as_config_fname);

	kf = g_key_file_new ();
	ret = g_key_file_load_from_file (kf, as_config_fname, G_KEY_FILE_NONE, NULL);
	if (!ret) {
		g_debug ("Unable to read configuration file %s", as_config_fname);
		priv->free_origin_globs = g_new0 (gchar *, 1);
		return;
	}
#if GLIB_CHECK_VERSION(2, 64, 0)
	distro_id = g_get_os_info (G_OS_INFO_KEY_ID);
	if (distro_id == NULL) {
		g_warning ("Unable to determine the ID for this operating system.");
		priv->free_origin_globs = g_new0 (gchar *, 1);
		return;
	}
#else
	distro_id = g_strdup ("general");
#endif
	priv->free_origin_globs = g_key_file_get_string_list (kf,
							      distro_id,
							      "FreeRepos",
							      NULL,
							      NULL);
	if (priv->free_origin_globs == NULL) {
		priv->free_origin_globs = g_new0 (gchar *, 1);
		return;
	}
}

/**
 * as_context_os_origin_is_free:
 * @ctx: a #AsContext instance
 * @error: a #GError
 *
 * Check the local whitelist for whether a component from an
 * OS origin is free software or not, based purely on its origin.
 *
 * Returns: %TRUE if the respective origin contains only free software, %FALSE if not or unknown.
 */
gboolean
as_context_os_origin_is_free (AsContext *ctx, const gchar *origin)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);

	/* load global configuration */
	as_context_ensure_os_config_loaded (ctx);

	/* return a free component if any of the origin wildcards matches */
	for (guint i = 0; priv->free_origin_globs[i] != NULL; i++) {
		if (g_pattern_match_simple (priv->free_origin_globs[i], origin))
			return TRUE;
	}

	return FALSE;
}

/**
 * as_context_new:
 *
 * Creates a new #AsContext.
 *
 * Returns: (transfer full): an #AsContext
 **/
AsContext *
as_context_new (void)
{
	AsContext *ctx;
	ctx = g_object_new (AS_TYPE_CONTEXT, NULL);
	return AS_CONTEXT (ctx);
}
