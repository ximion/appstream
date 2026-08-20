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
 * SECTION:asc-globals
 * @short_description: Set and read a bunch of global settings used across appstream-compose
 * @include: appstream-compose.h
 *
 * These settings are global to the process, and are shared by every #AscCompose
 * and #AscMedia instance in it. They are not synchronized against concurrent
 * write access, so an application has to apply its configuration before it starts
 * composing metadata, and must not change it while a compose run is in progress.
 */

#define _GNU_SOURCE

#include "config.h"
#include "asc-globals-private.h"

#ifdef HAVE_DLADDR
#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#endif

#include "as-utils-private.h"
#include "as-validator-issue-tag.h"

#ifdef ASC_MEDIAWORKER_USE_LIBEXEC
#define ASC_MEDIAWORKER_EXE LIBEXECDIR "/asc-mediaworker"
#else
#define ASC_MEDIAWORKER_EXE LIBDIR "/ascompose/asc-mediaworker"
#endif

#define ASC_TYPE_GLOBALS (asc_globals_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscGlobals, asc_globals, ASC, GLOBALS, GObject)

struct _AscGlobalsClass {
	GObjectClass parent_class;
};

typedef struct {
	gboolean use_optipng;
	gchar *optipng_bin;
	gchar *ffprobe_bin;
	gchar *mediaworker_bin;
	gchar *tmp_dir;

	GMutex hint_tags_mutex;
	GHashTable *hint_tags;
} AscGlobalsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscGlobals, asc_globals, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_globals_get_instance_private (o))

static AscGlobals *g_globals = NULL;
static GMutex g_globals_mutex;

#ifdef HAVE_DLADDR
/* address anchor used to locate the DSO we are part of */
__attribute__((used)) static gchar asc_globals_dso_anchor;
#endif

/**
 * asc_compose_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.13.0
 **/
GQuark
asc_compose_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AscComposeError");
	return quark;
}

static GObject *
asc_globals_constructor (GType type,
			 guint n_construct_properties,
			 GObjectConstructParam *construct_properties)
{
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&g_globals_mutex);
	if (g_globals != NULL)
		return G_OBJECT (g_globals);
	else
		return G_OBJECT_CLASS (asc_globals_parent_class)
		    ->constructor (type, n_construct_properties, construct_properties);
}

static void
asc_globals_finalize (GObject *object)
{
	AscGlobals *globals = ASC_GLOBALS (object);
	AscGlobalsPrivate *priv = GET_PRIVATE (globals);

	g_free (priv->tmp_dir);
	g_free (priv->optipng_bin);
	g_free (priv->ffprobe_bin);
	g_free (priv->mediaworker_bin);

	g_mutex_clear (&priv->hint_tags_mutex);
	if (priv->hint_tags != NULL)
		g_hash_table_unref (priv->hint_tags);

	G_OBJECT_CLASS (asc_globals_parent_class)->finalize (object);
}

/**
 * asc_globals_find_local_mediaworker:
 *
 * Locate the media worker that was built together with this copy of
 * libappstream-compose, in case we are running uninstalled from a build tree.
 * That way an uninstalled library never spawns a system-wide worker binary
 * of a possibly different version.
 *
 * Returns: (transfer full): The worker path, or %NULL if we are not in a build tree.
 */
static gchar *
asc_globals_find_local_mediaworker (void)
{
#ifdef HAVE_DLADDR
	Dl_info dl_info;
	gchar self_path[PATH_MAX];
	g_autofree gchar *self_dir = NULL;
	g_autofree gchar *worker_fname = NULL;

	/* find the library (or executable) this code is a part of */
	if (dladdr (&asc_globals_dso_anchor, &dl_info) == 0)
		return NULL;

	/* we never resolve anything relative to the current working directory,
	 * as appstream-compose runs with its CWD in untrusted data */
	if (dl_info.dli_fname == NULL || !g_path_is_absolute (dl_info.dli_fname))
		return NULL;
	if (realpath (dl_info.dli_fname, self_path) == NULL)
		return NULL;

	/* in a build tree, the worker sits in a "worker" directory right next to
	 * the library - no installed layout ever looks like that */
	self_dir = g_path_get_dirname (self_path);
	worker_fname = g_build_filename (self_dir, "worker", "asc-mediaworker", NULL);
	if (g_file_test (worker_fname, G_FILE_TEST_IS_EXECUTABLE))
		return g_steal_pointer (&worker_fname);
#endif
	return NULL;
}

static void
asc_globals_init (AscGlobals *globals)
{
	AscGlobalsPrivate *priv = GET_PRIVATE (globals);
	g_autofree gchar *tmp_str1 = NULL;
	g_autofree gchar *tmp_str2 = NULL;
	g_assert (g_globals == NULL);
	g_globals = globals;

	tmp_str1 = as_random_alnum_string (6);
	tmp_str2 = g_strconcat ("as-compose_", tmp_str1, NULL);
	priv->tmp_dir = g_build_filename (g_get_tmp_dir (), tmp_str2, NULL);

	priv->optipng_bin = g_find_program_in_path ("optipng");
	if (priv->optipng_bin != NULL)
		priv->use_optipng = TRUE;

	priv->ffprobe_bin = g_find_program_in_path ("ffprobe");

	/* find the media worker: an explicit override for development and test
	 * purposes wins, then a worker built alongside us in a build tree,
	 * then the installed helper, then whatever is in PATH */
	priv->mediaworker_bin = g_strdup (g_getenv ("ASC_MEDIAWORKER"));
	if (as_is_empty (priv->mediaworker_bin)) {
		g_free (priv->mediaworker_bin);
		priv->mediaworker_bin = asc_globals_find_local_mediaworker ();
		if (priv->mediaworker_bin != NULL)
			g_debug ("Using local media worker: %s", priv->mediaworker_bin);
	}
	if (priv->mediaworker_bin == NULL) {
		if (g_file_test (ASC_MEDIAWORKER_EXE, G_FILE_TEST_IS_EXECUTABLE))
			priv->mediaworker_bin = g_strdup (ASC_MEDIAWORKER_EXE);
		else
			priv->mediaworker_bin = g_find_program_in_path ("asc-mediaworker");
	}

	g_mutex_init (&priv->hint_tags_mutex);
}

static void
asc_globals_class_init (AscGlobalsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->constructor = asc_globals_constructor;
	object_class->finalize = asc_globals_finalize;
}

static AscGlobalsPrivate *
asc_globals_get_priv (void)
{
	return GET_PRIVATE (g_object_new (ASC_TYPE_GLOBALS, NULL));
}

/**
 * asc_globals_clear:
 *
 * Clear all global state and restore defaults.
 *
 * Since: 0.14.6
 */
void
asc_globals_clear (void)
{
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&g_globals_mutex);
	if (g_globals == NULL)
		return;

	g_object_unref (g_globals);
	g_globals = NULL;
}

/**
 * asc_globals_get_tmp_dir:
 *
 * Get temporary directory used by appstream-compose.
 *
 * Since: 0.13.0
 **/
const gchar *
asc_globals_get_tmp_dir (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	return priv->tmp_dir;
}

/**
 * asc_globals_get_tmp_dir_create:
 *
 * Get temporary directory used by appstream-compose
 * and try to create it if it does not exist.
 *
 * Since: 0.13.0
 **/
const gchar *
asc_globals_get_tmp_dir_create (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	g_mkdir_with_parents (priv->tmp_dir, 0700);
	return priv->tmp_dir;
}

/**
 * asc_globals_set_tmp_dir:
 *
 * Set temporary directory used by appstream-compose.
 *
 * Since: 0.13.0
 **/
void
asc_globals_set_tmp_dir (const gchar *path)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	g_free (priv->tmp_dir);
	priv->tmp_dir = g_strdup (path);
}

/**
 * asc_globals_get_use_optipng:
 *
 * Get whether images should be optimized using optipng.
 *
 * Since: 0.13.0
 **/
gboolean
asc_globals_get_use_optipng (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	return priv->use_optipng;
}

/**
 * asc_globals_set_use_optipng:
 *
 * Set whether images should be optimized using optipng.
 *
 * Since: 0.13.0
 **/
void
asc_globals_set_use_optipng (gboolean enabled)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	if (enabled && priv->optipng_bin == NULL) {
		g_critical ("Refusing to enable optipng: not found in $PATH");
		priv->use_optipng = FALSE;
		return;
	}
	priv->use_optipng = enabled;
}

/**
 * asc_globals_get_optipng_binary:
 *
 * Get path to the "optipng" binary we should use.
 *
 * Since: 0.13.0
 **/
const gchar *
asc_globals_get_optipng_binary (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	return priv->optipng_bin;
}

/**
 * asc_globals_set_optipng_binary:
 *
 * Set path to the "optipng" binary we should use.
 *
 * Since: 0.13.0
 **/
void
asc_globals_set_optipng_binary (const gchar *path)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	g_free (priv->optipng_bin);
	priv->optipng_bin = g_strdup (path);
	if (priv->optipng_bin == NULL)
		priv->use_optipng = FALSE;
}

/**
 * asc_globals_get_ffprobe_binary:
 *
 * Get path to the "ffprobe" binary we should use.
 *
 * Since: 0.14.6
 **/
const gchar *
asc_globals_get_ffprobe_binary (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	return priv->ffprobe_bin;
}

/**
 * asc_globals_set_ffprobe_binary:
 *
 * Set path to the "ffprobe" binary we should use.
 *
 * Since: 0.14.6
 **/
void
asc_globals_set_ffprobe_binary (const gchar *path)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	g_free (priv->ffprobe_bin);
	priv->ffprobe_bin = g_strdup (path);
}

/**
 * asc_globals_get_mediaworker_binary:
 *
 * Get path to the "asc-mediaworker" binary that #AscMedia
 * spawns for media processing.
 **/
const gchar *
asc_globals_get_mediaworker_binary (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	return priv->mediaworker_bin;
}

/**
 * asc_globals_set_mediaworker_binary:
 *
 * Set path to the "asc-mediaworker" binary that #AscMedia
 * spawns for media processing.
 **/
void
asc_globals_set_mediaworker_binary (const gchar *path)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	g_free (priv->mediaworker_bin);
	priv->mediaworker_bin = g_strdup (path);
}

/**
 * asc_globals_create_hint_tag_table:
 *
 * Create hint tag cache.
 * IMPORTANT: This function may only be called while a lock is held
 * on the hint tag table mutex.
 */
static void
asc_globals_create_hint_tag_table (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	g_return_if_fail (priv->hint_tags == NULL);

	priv->hint_tags = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 (GDestroyNotify) as_ref_string_release,
						 (GDestroyNotify) asc_hint_tag_free);

	/* add compose issue hint tags */
	for (guint i = 0; asc_hint_tag_list[i].tag != NULL; i++) {
		AscHintTag *htag;
		gboolean r;
		const AscHintTagStatic s = asc_hint_tag_list[i];
		htag = asc_hint_tag_new (s.tag, s.severity, s.explanation);
		r = g_hash_table_insert (priv->hint_tags,
					 g_ref_string_new_intern (asc_hint_tag_list[i].tag),
					 htag);
		if (G_UNLIKELY (!r))
			g_critical ("Duplicate compose-hint tag '%s' found in tag list. This is a "
				    "bug in appstream-compose.",
				    asc_hint_tag_list[i].tag);
	}

	/* add validator issue hint tags */
	for (guint i = 0; as_validator_issue_tag_list[i].tag != NULL; i++) {
		AscHintTag *htag;
		gboolean r;
		AsIssueSeverity severity;
		g_autofree gchar *compose_tag = g_strconcat ("asv-",
							     as_validator_issue_tag_list[i].tag,
							     NULL);
		g_autofree gchar *explanation = g_markup_printf_escaped (
		    "<code>{{location}}</code> - <em>{{hint}}</em><br/>%s",
		    as_validator_issue_tag_list[i].explanation);

		/* any validator issue can not be of type error in as-compose - if the validation issue
		 * is so severe that it renders the compose process impossible, we will throw another issue
		 * of type "error" which will immediately terminate the data genertaion. */
		severity = as_validator_issue_tag_list[i].severity;
		if (severity == AS_ISSUE_SEVERITY_ERROR)
			severity = AS_ISSUE_SEVERITY_WARNING;

		htag = asc_hint_tag_new (compose_tag, severity, explanation);
		r = g_hash_table_insert (priv->hint_tags,
					 g_ref_string_new_intern (compose_tag),
					 htag);
		if (G_UNLIKELY (!r))
			g_critical ("Duplicate issue-tag '%s' found in tag list. This is a bug in "
				    "appstream-compose.",
				    as_validator_issue_tag_list[i].tag);
	}
}

/**
 * asc_globals_add_hint_tag:
 * @tag: the tag-ID to add
 * @severity: the tag severity as #AsIssueSeverity
 * @explanation: the tag explanatory message
 * @overrideExisting: whether an existing tag should be replaced
 *
 * Register a new hint tag. If a previous tag with the given name
 * already existed, the existing tag will not be replaced unless
 * @overrideExisting is set to %TRUE.
 * Please be careful when overriding tags! Tag severities can not
 * be lowered by overriding a tag.
 *
 * Returns: %TRUE if the tag was registered and did not exist previously.
 *
 * Since: 0.14.0
 */
gboolean
asc_globals_add_hint_tag (const gchar *tag,
			  AsIssueSeverity severity,
			  const gchar *explanation,
			  gboolean overrideExisting)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	AscHintTag *htag;
	AscHintTag *e_htag;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->hint_tags_mutex);
	g_return_val_if_fail (tag != NULL, FALSE);

	if (priv->hint_tags == NULL)
		asc_globals_create_hint_tag_table ();

	e_htag = g_hash_table_lookup (priv->hint_tags, tag);
	if (e_htag != NULL) {
		if (overrideExisting) {
			/* make sure we don't permit lowering severities */
			if (severity > e_htag->severity)
				severity = e_htag->severity;
		} else {
			/* don't allow the override */
			return FALSE;
		}
	}

	htag = asc_hint_tag_new (tag, severity, explanation);
	g_hash_table_insert (priv->hint_tags, g_ref_string_new_intern (tag), htag);
	return TRUE;
}

/**
 * asc_globals_get_hint_info:
 *
 * Return details for a given hint tag.
 *
 * Returns: (transfer none): Hint tag details.
 */
AscHintTag *
asc_globals_get_hint_tag_details (const gchar *tag)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->hint_tags_mutex);
	g_return_val_if_fail (tag != NULL, NULL);

	if (priv->hint_tags == NULL)
		asc_globals_create_hint_tag_table ();
	return g_hash_table_lookup (priv->hint_tags, tag);
}

/**
 * asc_globals_get_hint_tags:
 *
 * Retrieve all hint tags that we know.
 *
 * Returns: (transfer full): A list of valid hint tags. Free with %g_strfreev
 *
 * Since: 0.14.0
 */
gchar **
asc_globals_get_hint_tags (void)
{
	AscGlobalsPrivate *priv = asc_globals_get_priv ();
	GHashTableIter iter;
	gpointer key;
	gchar **strv;
	guint i = 0;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->hint_tags_mutex);

	if (priv->hint_tags == NULL)
		asc_globals_create_hint_tag_table ();

	/* deep-copy the table keys to a strv */
	strv = g_new0 (gchar *, g_hash_table_size (priv->hint_tags) + 1);
	g_hash_table_iter_init (&iter, priv->hint_tags);
	while (g_hash_table_iter_next (&iter, &key, NULL))
		strv[i++] = g_strdup ((const gchar *) key);

	return strv;
}

/**
 * asc_globals_hint_tag_severity:
 *
 * Retrieve the severity of the given hint tag.
 *
 * Returns: An #AsIssueSeverity or %AS_ISSUE_SEVERITY_UNKNOWN if the tag did not exist
 *          or has an unknown severity.
 *
 * Since: 0.14.0
 */
AsIssueSeverity
asc_globals_hint_tag_severity (const gchar *tag)
{
	AscHintTag *htag = asc_globals_get_hint_tag_details (tag);
	if (htag == NULL)
		return AS_ISSUE_SEVERITY_UNKNOWN;
	return htag->severity;
}

/**
 * asc_globals_hint_tag_explanation:
 *
 * Retrieve the explanation template of the given hint tag.
 *
 * Returns: An explanation template, or %NULL if the tag was not found.
 *
 * Since: 0.14.0
 */
const gchar *
asc_globals_hint_tag_explanation (const gchar *tag)
{
	AscHintTag *htag = asc_globals_get_hint_tag_details (tag);
	if (htag == NULL)
		return NULL;
	return htag->explanation;
}
