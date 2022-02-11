/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-utils-private.h"

typedef struct
{
	AsFormatVersion		format_version;
	AsFormatStyle		style;
	GRefString		*locale;
	GRefString		*origin;
	GRefString		*media_baseurl;
	GRefString		*arch;
	GRefString		*fname;
	gint 			priority;

	gboolean		internal_mode;
	gboolean		all_locale;
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
 * Since: 0.10
 **/
const gchar*
as_format_version_to_string (AsFormatVersion version)
{
	if (version == AS_FORMAT_VERSION_V0_6)
		return "0.6";
	if (version == AS_FORMAT_VERSION_V0_7)
		return "0.7";
	if (version == AS_FORMAT_VERSION_V0_8)
		return "0.8";
	if (version == AS_FORMAT_VERSION_V0_9)
		return "0.9";
	if (version == AS_FORMAT_VERSION_V0_10)
		return "0.10";
	if (version == AS_FORMAT_VERSION_V0_11)
		return "0.11";
	if (version == AS_FORMAT_VERSION_V0_12)
		return "0.12";
	if (version == AS_FORMAT_VERSION_V0_13)
		return "0.13";
	if (version == AS_FORMAT_VERSION_V0_14)
		return "0.14";
	return "?.??";
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
 * Since: 0.10
 **/
AsFormatVersion
as_format_version_from_string (const gchar *version_str)
{
	if (g_strcmp0 (version_str, "0.14") == 0)
		return AS_FORMAT_VERSION_V0_14;
	if (g_strcmp0 (version_str, "0.13") == 0)
		return AS_FORMAT_VERSION_V0_13;
	if (g_strcmp0 (version_str, "0.12") == 0)
		return AS_FORMAT_VERSION_V0_12;
	if (g_strcmp0 (version_str, "0.11") == 0)
		return AS_FORMAT_VERSION_V0_11;
	if (g_strcmp0 (version_str, "0.10") == 0)
		return AS_FORMAT_VERSION_V0_10;
	if (g_strcmp0 (version_str, "0.9") == 0)
		return AS_FORMAT_VERSION_V0_9;
	if (g_strcmp0 (version_str, "0.8") == 0)
		return AS_FORMAT_VERSION_V0_8;
	if (g_strcmp0 (version_str, "0.7") == 0)
		return AS_FORMAT_VERSION_V0_7;
	if (g_strcmp0 (version_str, "0.6") == 0)
		return AS_FORMAT_VERSION_V0_6;
	return AS_FORMAT_VERSION_V0_10;
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

	G_OBJECT_CLASS (as_context_parent_class)->finalize (object);
}

static void
as_context_init (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);

	priv->format_version = AS_FORMAT_VERSION_CURRENT;
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
const gchar*
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
 * Returns: The active locale.
 **/
const gchar*
as_context_get_locale (AsContext *ctx)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->locale;
}

/**
 * as_context_set_locale:
 * @ctx: a #AsContext instance.
 * @value: the new value.
 *
 * Sets the active locale.
 **/
void
as_context_set_locale (AsContext *ctx, const gchar *value)
{
	AsContextPrivate *priv = GET_PRIVATE (ctx);

	g_atomic_int_set (&priv->all_locale, FALSE);
	if (g_strcmp0 (value, "ALL") == 0) {
		g_autofree gchar *tmp = as_get_current_locale ();
		g_atomic_int_set (&priv->all_locale, TRUE);
		as_ref_string_assign_safe (&priv->locale, tmp);
	} else {
		as_ref_string_assign_safe (&priv->locale, value);
	}
}

/**
 * as_context_get_locale_all_enabled:
 * @ctx: a #AsContext instance.
 *
 * Returns: %TRUE if all locale should be parsed.
 **/
gboolean
as_context_get_locale_all_enabled (AsContext *ctx)
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
const gchar*
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
const gchar*
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
const gchar*
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
const gchar*
as_context_localized_ht_get (AsContext *ctx, GHashTable *lht, const gchar *locale_override, AsValueFlags value_flags)
{
	const gchar *locale;
	const gchar *msg;

	/* retrieve context locale, if the locale isn't explicitly overridden */
	if ((ctx != NULL) && (locale_override == NULL)) {
		AsContextPrivate *priv = GET_PRIVATE (ctx);
		locale = priv->locale;
	} else {
		locale = locale_override;
	}

	/* NULL is not an acceptable value here and means "C" */
	if (locale == NULL)
		locale = "C";

	msg = g_hash_table_lookup (lht, locale);
	if ((msg == NULL) && (!as_flags_contains (value_flags, AS_VALUE_FLAG_NO_TRANSLATION_FALLBACK))) {
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
 * @locale: (nullable): the locale, or %NULL. e.g. "en_GB".
 *
 * Helper function to set a localized value on a trabslation mapping.
 *
 * This is used by all entities which have a context and have localized strings.
 */
void
as_context_localized_ht_set (AsContext *ctx, GHashTable *lht, const gchar *value, const gchar *locale)
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
	g_hash_table_insert (lht,
			     g_ref_string_new_intern (locale_noenc),
			     g_strdup (value));
}

/**
 * as_context_new:
 *
 * Creates a new #AsContext.
 *
 * Returns: (transfer full): an #AsContext
 **/
AsContext*
as_context_new (void)
{
	AsContext *ctx;
	ctx = g_object_new (AS_TYPE_CONTEXT, NULL);
	return AS_CONTEXT (ctx);
}
