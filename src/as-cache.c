/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-cache
 * @short_description: On-disk or in-memory cache of components for quick searching
 *
 * Caches are used by #AsPool to quickly search for components while not keeping all
 * component data in memory.
 * This class is threadsafe.
 *
 * See also: #AsPool
 */

#include "config.h"
#include "as-cache.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <xmlb.h>

#include "as-utils-private.h"
#include "as-context-private.h"
#include "as-component-private.h"
#include "as-launchable.h"


typedef struct
{
	gchar			*cache_root_dir;
	gchar			*system_cache_dir;
	GRefString		*locale;
	gboolean		default_paths_changed;

	AsContext		*context;
	GPtrArray		*sections;
	GHashTable		*masked;

	AsCacheDataRefineFn	cpt_refine_func;
	gboolean		prefer_os_metainfo;
	gboolean		auto_resolve_addons;

	GRWLock			rw_lock;
} AsCachePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsCache, as_cache, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_cache_get_instance_private (o))

/* location of the system-wide cache */
static const gchar *AS_APPSTREAM_SYS_CACHE_DIR = "/var/cache/swcatalog/cache";

typedef struct {
	gboolean		is_os_data;
	gboolean		is_mask;
	gchar			*key;
	AsComponentScope	scope;
	AsFormatStyle		format_style;
	XbSilo			*silo;
	gchar			*fname;

	gpointer		refine_func_udata;
} AsCacheSection;

static AsCacheSection*
as_cache_section_new (const gchar *key)
{
	AsCacheSection *csec;
	csec = g_new0 (AsCacheSection, 1);
	csec->format_style = AS_FORMAT_STYLE_COLLECTION;
	csec->key = g_strdup (key);
	return csec;
}

static void
as_cache_section_free (AsCacheSection *csec)
{
	g_free (csec->key);
	g_free (csec->fname);
	if (csec->silo != NULL)
		g_object_unref (csec->silo);
	g_free (csec);
}

static gint
as_cache_section_cmp (gconstpointer a, gconstpointer b)
{
	const AsCacheSection *s1 = *((AsCacheSection **) a);
	const AsCacheSection *s2 = *((AsCacheSection **) b);

	/* sort masking data last */
	if (s1->is_mask && !s2->is_mask)
		return 1;
	if (!s1->is_mask && s2->is_mask)
		return -1;

	/* sort collection metadata before metainfo data */
	if (s1->format_style > s2->format_style)
		return -1;
	if (s1->format_style < s2->format_style)
		return 1;

	/* system scope before user scope */
	if (s1->scope < s2->scope)
		return -1;
	if (s1->scope > s2->scope)
		return 1;

	/* alphabetic sorting if everything else is equal */
	return g_ascii_strcasecmp (s1->key, s2->key);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsCacheSection, as_cache_section_free)

/**
 * as_cache_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_cache_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsCache");
	return quark;
}

/**
 * as_cache_init:
 **/
static void
as_cache_init (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);

	g_rw_lock_init (&priv->rw_lock);

	priv->sections = g_ptr_array_new_with_free_func ((GDestroyNotify) as_cache_section_free);
	priv->masked = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						NULL);


	priv->context = as_context_new ();
	as_context_set_style (priv->context, AS_FORMAT_STYLE_COLLECTION);
	/* we need to write some special tags only used in the cache */
	as_context_set_internal_mode (priv->context, TRUE);

	/* default string for unknown locale - should be replaced before use */
	as_cache_set_locale (cache, "unknown");

	/* we choose the system-wide cache directory if we are root, otherwise we will write to
	 * a user-specific directory. The system cache dir is always considered immutable. */
	priv->system_cache_dir = g_strdup (AS_APPSTREAM_SYS_CACHE_DIR);
	if (as_utils_is_root ()) {
		priv->cache_root_dir = g_strdup (priv->system_cache_dir);
	} else {
		g_autoptr(GError) tmp_error = NULL;
		priv->cache_root_dir = as_get_user_cache_dir (&tmp_error);
		if (priv->cache_root_dir == NULL) {
			g_critical ("Failed to obtain user cache directory: %s", tmp_error->message);
			priv->cache_root_dir = g_strdup ("/tmp");
		}
	}

	priv->default_paths_changed = FALSE;
}

/**
 * as_cache_finalize:
 **/
static void
as_cache_finalize (GObject *object)
{
	AsCache *cache = AS_CACHE (object);
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_rw_lock_writer_lock (&priv->rw_lock);

	g_free (priv->cache_root_dir);
	g_free (priv->system_cache_dir);
	as_ref_string_release (priv->locale);
	g_object_unref (priv->context);
	g_ptr_array_unref (priv->sections);
	g_hash_table_unref (priv->masked);

	g_rw_lock_writer_unlock (&priv->rw_lock);
	g_rw_lock_clear (&priv->rw_lock);
	G_OBJECT_CLASS (as_cache_parent_class)->finalize (object);
}

/**
 * as_cache_class_init:
 **/
static void
as_cache_class_init (AsCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_cache_finalize;
}

/**
 * as_cache_new:
 *
 * Creates a new #AsCache.
 *
 * Returns: (transfer full): a #AsCache
 *
 **/
AsCache*
as_cache_new (void)
{
	AsCache *cache;
	cache = g_object_new (AS_TYPE_CACHE, NULL);
	return AS_CACHE (cache);
}

/**
 * as_cache_get_locale:
 * @cache: an #AsCache instance.
 *
 * Gets the locale this cache is generated for.
 *
 * Returns: locale string, or "unknown" if unset
 *
 * Since: 0.14.0
 */
const gchar*
as_cache_get_locale (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	return priv->locale;
}

/**
 * as_cache_set_locale:
 * @cache: an #AsCache instance.
 * @locale: the locale name.
 *
 * Set the cache locale.
 */
void
as_cache_set_locale (AsCache *cache, const gchar *locale)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);
	as_ref_string_assign_safe (&priv->locale, locale);
	as_context_set_locale (priv->context, priv->locale);
}

/**
 * as_cache_set_locations:
 * @cache: an #AsCache instance.
 * @system_cache_dir: System cache directory
 * @user_cache_dir: User cache directory
 *
 * Override the default cache locations. The system cache is only written to if
 * the application is running with root permissions (in which case it is set equal
 * to the user cache directory), otherwise it is used to load caches that are up-to-date
 * but not available in the user directory yet.
 *
 * The user directory is generally writable and will store arbitrary caches for any applications.
 *
 * You should *not* need to call this function! It should generally only be used for debugging
 * as well as tests.
 */
void
as_cache_set_locations (AsCache *cache, const gchar *system_cache_dir, const gchar *user_cache_dir)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	g_free (priv->cache_root_dir);
	priv->cache_root_dir = g_strdup (user_cache_dir);
	g_free (priv->system_cache_dir);
	priv->system_cache_dir = g_strdup (system_cache_dir);
	priv->default_paths_changed = TRUE;
}

/**
 * as_cache_get_prefer_os_metainfo:
 * @cache: an #AsCache instance.
 *
 * Get whether metainfo data should be preferred over collection data for OS components.
 *
 * Returns: %TRUE if metainfo data is preferred.
 */
gboolean
as_cache_get_prefer_os_metainfo (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	return priv->prefer_os_metainfo;
}

/**
 * as_cache_set_prefer_os_metainfo:
 * @cache: an #AsCache instance.
 * @prefer_os_metainfo: Whether Metainfo data is preferred.
 *
 * Set whether metainfo data should be preferred over collection data for OS components.
 */
void
as_cache_set_prefer_os_metainfo (AsCache *cache, gboolean prefer_os_metainfo)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	priv->prefer_os_metainfo = prefer_os_metainfo;
}

/**
 * as_cache_set_resolve_addons:
 * @cache: an #AsCache instance.
 * @resolve_addons: Whether addon links should be auto-resolved.
 */
void
as_cache_set_resolve_addons (AsCache *cache, gboolean resolve_addons)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	priv->auto_resolve_addons = resolve_addons;
}

/**
 * as_cache_delete_file_if_old:
 */
static void
as_delete_cache_file_if_old (AsCache *cache, const gchar *fname, time_t min_atime)
{
	struct stat sbuf;

	/* safeguard so we will only delete cache files */
	if (!g_str_has_suffix (fname, ".xb") &&
	    !g_str_has_suffix (fname, ".cache"))
		return;

	if (stat (fname, &sbuf) < 0)
		return;

	if (sbuf.st_atime < min_atime) {
		g_autofree gchar *basename = g_path_get_basename (fname);
		g_debug ("Deleting old cache file: %s", basename);
		g_unlink (fname);
	}
}

/**
 * as_cache_remove_old_data_from_dir:
 */
static void
as_cache_remove_old_data_from_dir (AsCache *cache, const gchar *cache_dir)
{
	g_autoptr(GFileEnumerator) direnum = NULL;
	g_autoptr(GFile) cdir = NULL;
	g_autoptr(GError) error = NULL;
	time_t min_last_atime;

	cdir =  g_file_new_for_path (cache_dir);
	direnum = g_file_enumerate_children (cdir,
					     G_FILE_ATTRIBUTE_STANDARD_NAME,
					     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					     NULL,
					     &error);
	if (error != NULL) {
		g_debug ("Unable to clean cache directory '%s': %s",
			 cache_dir, error->message);
		return;
	}

	min_last_atime = time (NULL) - (3 * 30 * 24 * 60 * 60);
	while (TRUE) {
		GFileInfo *finfo = NULL;
		g_autofree gchar *fname_full = NULL;

		if (!g_file_enumerator_iterate (direnum, &finfo, NULL, NULL, NULL))
			return;
		if (finfo == NULL)
			break;

		/* generate full filename */
		fname_full = g_build_filename (cache_dir,
					       g_file_info_get_name (finfo),
					       NULL);

		/* jump one directory deeper */
		if (g_file_info_get_file_type (finfo) == G_FILE_TYPE_DIRECTORY) {
			g_autoptr(GFileEnumerator) sd_direnum = NULL;
			g_autoptr(GFile) sd_cdir = NULL;

			sd_cdir =  g_file_new_for_path (fname_full);
			sd_direnum = g_file_enumerate_children (sd_cdir,
								G_FILE_ATTRIBUTE_STANDARD_NAME,
								G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
								NULL, NULL);
			if (sd_direnum == NULL) {
				g_debug ("Unable to scan directory '%s'.", fname_full);
				continue;
			}

			while (TRUE) {
				GFileInfo *sd_finfo = NULL;
				g_autofree gchar *sd_fname_full = NULL;
				if (!g_file_enumerator_iterate (sd_direnum, &sd_finfo, NULL, NULL, NULL))
					break;
				if (sd_finfo == NULL)
					break;
				if (g_file_info_get_file_type (sd_finfo) != G_FILE_TYPE_REGULAR)
					continue;

				sd_fname_full = g_build_filename (fname_full,
								  g_file_info_get_name (sd_finfo),
								  NULL);
				as_delete_cache_file_if_old (cache, sd_fname_full, min_last_atime);
			}

			/* just try to delete the directory - if it's not yet empty, nothing will happen */
			g_remove (fname_full);
		}
		if (g_file_info_get_file_type (finfo) != G_FILE_TYPE_REGULAR)
			continue;
		as_delete_cache_file_if_old (cache, fname_full, min_last_atime);
	}
}

/**
 * as_cache_prune_data:
 * @cache: an #AsCache instance.
 *
 * Delete all cache data that has not been used in a long time.
 */
void
as_cache_prune_data (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);

	if (priv->default_paths_changed) {
		/* we will not delete stuff from non-default cache locations, to prevent accidental data loss */
		g_debug ("Not pruning cache: Default paths were changed.");
		return;
	}

	g_debug ("Pruning old cache data.");
	as_cache_remove_old_data_from_dir (cache, priv->cache_root_dir);
	if (as_utils_is_writable (priv->system_cache_dir))
		as_cache_remove_old_data_from_dir (cache, priv->system_cache_dir);
}

/**
 * as_cache_clear:
 * @cache: an #AsCache instance.
 *
 * Close cache and reset all opened cache sections.
 * Also forget all masked components, but keep any settings.
 */
void
as_cache_clear (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	g_ptr_array_set_size (priv->sections, 0);

	g_hash_table_unref (priv->masked);
	priv->masked = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 g_free,
						 NULL);
}

/**
 * as_cache_get_root_dir_with_scope:
 */
static gchar *
as_cache_get_root_dir_with_scope (AsCache *cache, AsCacheScope cache_scope, AsComponentScope scope)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	gchar *path;
	const gchar *cache_root;

	if (cache_scope == AS_CACHE_SCOPE_SYSTEM)
		cache_root = priv->system_cache_dir;
	else
		cache_root = priv->cache_root_dir;

	if (scope == AS_COMPONENT_SCOPE_SYSTEM)
		path = g_build_filename (cache_root, NULL);
	else
		path = g_build_filename (cache_root, "user", NULL);

	g_mkdir_with_parents (path, 0755);
	return path;
}

/**
 * as_transmogrify_xmlnode_to_xbuildernode:
 */
static void
as_transmogrify_xmlnode_to_xbuildernode (xmlNode *lxn, XbBuilderNode *xbn)
{
	/* text node */
	if (lxn->children != NULL && xmlNodeIsText (lxn->children)) {
		if (lxn->children->next == NULL) {
			/* pure text node */
			g_autofree gchar *node_content = NULL;
			node_content = (gchar*) xmlNodeGetContent (lxn);
			xb_builder_node_set_text (xbn, node_content, -1);
		} else {
			/* other inline nodes follow after the text */
			xb_builder_node_set_text (xbn,
						  (gchar*) lxn->children->content,
						  -1);
		}
	}

	/* tail, in case text follows */
	if (xmlNodeIsText (lxn->next)) {
		const gchar *content = (gchar*) lxn->next->content;
		if (content != NULL)
			xb_builder_node_set_tail (xbn,
						  content,
						  -1);
	}

	/* attributes */
	for (xmlAttr *iter = lxn->properties; iter != NULL; iter = iter->next) {
		g_autofree gchar *attr_value = NULL;

		/* ignore attributes that are set to NULL */
		if (iter->children == NULL)
			continue;
		attr_value = (gchar*) xmlNodeGetContent (iter->children);
		xb_builder_node_set_attr (xbn,
					  (gchar*) iter->name,
					  attr_value);
	}

	/* children */
	for (xmlNode *iter = lxn->children; iter != NULL; iter = iter->next) {
		g_autoptr(XbBuilderNode) child = NULL;

		/* discard text and spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;
		child = xb_builder_node_new ((gchar*) iter->name);
		xb_builder_node_add_flag (child, XB_BUILDER_NODE_FLAG_LITERAL_TEXT);
		as_transmogrify_xmlnode_to_xbuildernode (iter, child);
		xb_builder_node_add_child (xbn, child);
	}
}

/**
 * as_cache_builder_add_simple_tokens:
 */
static void
as_cache_builder_add_simple_tokens (XbBuilderNode *root,
				    AsComponent *cpt,
				    const gchar *element,
				    AsSearchTokenMatch token_kind)
{
	g_autoptr(XbBuilderNode) token_node = NULL;
	g_autoptr(GPtrArray) token_array = NULL;

	token_node = xb_builder_node_get_child (root, element, NULL);
	if (token_node == NULL)
		return;

	token_array = as_component_generate_tokens_for (cpt, token_kind);
	/* libxmlb only allows a maximum of 32 tokens per tag, so we clamp this here to
	 * avoid cache corruption with older xmlb versions. See XB_OPCODE_TOKEN_MAX */
	for (guint i = 0; i < MIN(token_array->len, 32); i++)
		xb_builder_node_add_token (token_node,
					   g_ptr_array_index (token_array, i));
}

/**
 * as_cache_builder_add_manifold_tokens:
 */
static void
as_cache_builder_add_manifold_tokens (XbBuilderNode *root, AsComponent *cpt)
{
	g_autoptr(XbBuilderNode) tokens_node = NULL;
	g_autoptr(GPtrArray) token_array = NULL;

	tokens_node = xb_builder_node_new ("_asi_tokens");

	token_array = as_component_generate_tokens_for (cpt, AS_SEARCH_TOKEN_MATCH_KEYWORD);
	for (guint i = 0; i < token_array->len; i++) {
		g_autoptr(XbBuilderNode) child = xb_builder_node_new ("t");
		xb_builder_node_set_text (child, g_ptr_array_index (token_array, i), -1);
		xb_builder_node_add_child (tokens_node, child);
	}
	g_clear_pointer (&token_array, g_ptr_array_unref);

	token_array = as_component_generate_tokens_for (cpt, AS_SEARCH_TOKEN_MATCH_DESCRIPTION);
	for (guint i = 0; i < token_array->len; i++) {
		g_autoptr(XbBuilderNode) child = xb_builder_node_new ("t");
		xb_builder_node_set_text (child, g_ptr_array_index (token_array, i), -1);
		xb_builder_node_add_child (tokens_node, child);
	}
	g_clear_pointer (&token_array, g_ptr_array_unref);

	xb_builder_node_add_child (root, tokens_node);
}

/**
 * as_cache_components_to_internal_xml:
 *
 * Convert a list of components into internal binary XML that we will store
 * on disk for fast querying.
 *
 * Returns: Valid collection XML metadata for internal cache use.
 */
static XbSilo*
as_cache_components_to_internal_xb (AsCache *cache,
				    GPtrArray *cpts,
				    gboolean refine,
				    gpointer refine_func_udata,
				    GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(XbBuilder) builder = NULL;
	g_autoptr(XbBuilderNode) bnode_root = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autoptr(XbSilo) silo = NULL;

	/* NOTE: This function is already write-lock protected by its callers */

	bnode_root = xb_builder_node_new ("components");
	for (guint i = 0; i < cpts->len; i++) {
		xmlNode *cnode;
		g_autoptr(XbBuilderNode) xbnode = NULL;
		g_autoptr(XbBuilderNode) tmp_node = NULL;
		AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));

		/* refine component data */
		if (refine && priv->cpt_refine_func != NULL)
			(*priv->cpt_refine_func) (cpt, TRUE, refine_func_udata);

		/* serialize to node */
		cnode = as_component_to_xml_node (cpt, priv->context, NULL);
		if (cnode == NULL)
			continue;

		/* convert component node to builder node */
		xbnode = xb_builder_node_new ("component");
		as_transmogrify_xmlnode_to_xbuildernode (cnode, xbnode);
		xmlFreeNode (cnode);

		/* add tokens */
		as_cache_builder_add_simple_tokens (xbnode, cpt, "summary", AS_SEARCH_TOKEN_MATCH_SUMMARY);
		as_cache_builder_add_simple_tokens (xbnode, cpt, "_asi_origin", AS_SEARCH_TOKEN_MATCH_ORIGIN);
		as_cache_builder_add_simple_tokens (xbnode, cpt, "pkgname", AS_SEARCH_TOKEN_MATCH_PKGNAME);
		as_cache_builder_add_manifold_tokens (xbnode, cpt);

		/* add component to tree */
		xb_builder_node_add_child (bnode_root, xbnode);
	}

	builder = xb_builder_new ();
	/* add our version to the correctness hash */
	xb_builder_append_guid (builder, PACKAGE_VERSION);
	xb_builder_import_node (builder, bnode_root);

	silo = xb_builder_compile (builder,
				   XB_BUILDER_COMPILE_FLAG_NONE,
				   NULL,
				   &tmp_error);
	if (silo == NULL) {
		g_propagate_prefixed_error (error,
					    tmp_error,
					    "Unable to compile binary XML for caching:");
		return NULL;
	} else {
		return g_steal_pointer (&silo);
	}
}

/**
 * as_cache_register_addons_for_component:
 *
 * Associate available addons with this component.
 */
static gboolean
as_cache_register_addons_for_component (AsCache *cache, AsComponent *cpt, GError **error)
{
	g_autoptr(GPtrArray) addons = NULL;

	addons = as_cache_get_components_by_extends (cache,
						     as_component_get_id (cpt),
						     error);
	if (addons == NULL)
		return FALSE;

	for (guint i = 0; i < addons->len; i++)
		as_component_add_addon (cpt, g_ptr_array_index (addons, i));

	return TRUE;
}

/**
 * as_transmogrify_xbnode_to_xmlnode:
 */
static gboolean
as_transmogrify_xbnode_to_xmlnode (XbNode *xbn, xmlNode *lxn)
{
	XbNodeAttrIter attr_iter;
	const gchar *attr_name;
	const gchar *attr_value;
	const gchar *tail;
	XbNodeChildIter child_iter;
	XbNode *child = NULL;
	g_return_val_if_fail (XB_IS_NODE (xbn), FALSE);

	xmlNodeSetName (lxn,
			(xmlChar*) xb_node_get_element (xbn));
	xmlNodeAddContent (lxn,
			   (xmlChar*) xb_node_get_text (xbn));

	tail = xb_node_get_tail (xbn);
	if (tail != NULL) {
		xmlNode *tn = xmlNewText ((xmlChar*) tail);
		xmlAddNextSibling (lxn, tn);
	}

	/* attributes */
	xb_node_attr_iter_init (&attr_iter, xbn);
	while (xb_node_attr_iter_next (&attr_iter, &attr_name, &attr_value))
		xmlNewProp (lxn, (xmlChar*) attr_name, (xmlChar*) attr_value);

	/* children */
	xb_node_child_iter_init (&child_iter, xbn);
	while (xb_node_child_iter_loop (&child_iter, &child)) {
		xmlNode *lc = xmlNewChild (lxn, NULL, (xmlChar*) "", NULL);
		if (!as_transmogrify_xbnode_to_xmlnode (child, lc)) {
			g_clear_pointer (&child, g_object_unref);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * as_cache_component_from_node:
 *
 * Get component from an xmlb node.
 */
static AsComponent*
as_cache_component_from_node (AsCache *cache, AsCacheSection *csec, XbNode *node, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(AsComponent) cpt = NULL;
	xmlNode *root;

	root = xmlNewNode (NULL, (xmlChar*) "");
	as_transmogrify_xbnode_to_xmlnode (node, root);

	cpt = as_component_new ();
	if (!as_component_load_from_xml (cpt, priv->context, root, error)) {
		xmlFreeNode (root);
		return NULL;
	}
	xmlFreeNode (root);

	/* find addons (if there are any) - ensure addons don't have addons themselves */
	if (priv->auto_resolve_addons &&
	    (as_component_get_kind (cpt) != AS_COMPONENT_KIND_ADDON) &&
	    !as_cache_register_addons_for_component (cache, cpt, error)) {
		return NULL;
	}

	/* refine for deserialization */
	if (priv->cpt_refine_func != NULL && !csec->is_mask)
		(*priv->cpt_refine_func) (cpt, FALSE, csec->refine_func_udata);

	return g_steal_pointer (&cpt);
}

/**
 * as_cache_remove_section_file:
 *
 * Safely remove a cache section file from disk.
 */
static void
as_cache_remove_section_file (AsCache *cache, AsCacheSection *csec)
{
	g_autofree gchar *rnd_suffix = NULL;
	g_autofree gchar *fname_old = NULL;

	/* do nothing if file is already gone */
	if (!g_file_test (csec->fname, G_FILE_TEST_EXISTS))
		return;

	/* random temporary name, in case multiple processes try to delete this at the same time */
	rnd_suffix = as_random_alnum_string (6);
	fname_old = g_strconcat (csec->fname, rnd_suffix, ".old", NULL);

	/* just in case the file exists by any chance, we drop it */
	g_unlink (fname_old);

	if (g_rename (csec->fname, fname_old) == -1) {
		/* some other process may have done this already */
		g_debug ("Unable to rename stale cache file '%s': %s", csec->fname, g_strerror (errno));
		g_unlink (csec->fname);
		return;
	}

	/* we should be able to remove the file at this point */
	if (g_unlink (fname_old) == -1)
		g_warning ("Unable to unlink old cache file '%s': %s", fname_old, g_strerror (errno));
}

/**
 * as_cache_build_section_key:
 *
 * Generate a cache key that we can use in filenames from an
 * arbitrary string that the user may have control over.
 * This will also prepend any locale information.
 */
static gchar*
as_cache_build_section_key (AsCache *cache, const gchar *str)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);

	if (g_strstr_len (str, -1, "/") == NULL) {
		/* the string is fine as-is, we just need to add the locale prefix to it */
		return g_strconcat (priv->locale,
				    "-",
				    str,
				    NULL);
	} else {
		/* we need to create a unique key from the filename-ish string */
		g_autoptr(GChecksum) cs = g_checksum_new (G_CHECKSUM_MD5);
		g_checksum_update (cs,
				   (const guchar *) str,
				   -1);

		return g_strconcat (priv->locale,
				"-",
				g_checksum_get_string (cs),
				NULL);
	}
}

/**
 * as_cache_get_section_fname_for:
 */
static gchar*
as_cache_get_section_fname_for (AsCache *cache,
				AsCacheScope cache_scope,
				AsComponentScope scope,
				const gchar *section_key)
{
	g_autofree gchar *cache_full_dir = NULL;
	g_autofree gchar *cache_basename = NULL;

	cache_full_dir = as_cache_get_root_dir_with_scope (cache,
							   cache_scope,
							   scope);
	cache_basename = g_strconcat (section_key, ".xb", NULL);

	return g_build_filename (cache_full_dir, cache_basename, NULL);
}

/**
 * as_cache_set_contents_internal:
 */
static gboolean
as_cache_set_contents_internal (AsCache *cache,
			        AsComponentScope scope,
			        AsFormatStyle source_format_style,
				gboolean is_os_data,
			        GPtrArray *cpts,
			        const gchar *cache_key,
				gpointer refine_user_data,
			        GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autofree gchar *internal_section_key = NULL;
	g_autofree gchar *section_key = NULL;
	g_autofree gchar *cache_full_dir = NULL;
	g_autoptr(AsCacheSection) csec = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autoptr(GRWLockWriterLocker) locker = NULL;
	gboolean ret = TRUE;

	section_key = as_cache_build_section_key (cache, cache_key);
	internal_section_key = g_strconcat (as_component_scope_to_string (scope), ":", section_key, NULL);

	locker = g_rw_lock_writer_locker_new (&priv->rw_lock);
	g_debug ("Storing cache data for section: %s", internal_section_key);

	/* ensure we can write to the cache location */
	cache_full_dir = as_cache_get_root_dir_with_scope (cache,
							   AS_CACHE_SCOPE_WRITABLE, /* only the "user" scope is ever writable */
							   scope);
	if (!as_utils_is_writable (cache_full_dir)) {
		g_set_error (error,
				AS_CACHE_ERROR,
				AS_CACHE_ERROR_PERMISSIONS,
				_("Cache location '%s' is not writable."), cache_full_dir);
		return FALSE;
	}

	/* replace old section, in case one with the same key exists */
	for (guint i = 0; i < priv->sections->len; i++) {
		AsCacheSection *csec_entry = g_ptr_array_index (priv->sections, i);
		if (g_strcmp0 (csec_entry->key, internal_section_key) == 0) {
			as_cache_remove_section_file (cache, csec_entry);
			g_ptr_array_remove_index_fast (priv->sections, i);
			break;
		}
	}

	/* create new section */
	csec = as_cache_section_new (internal_section_key);
	csec->is_os_data = is_os_data && scope == AS_COMPONENT_SCOPE_SYSTEM;
	csec->scope = scope;
	csec->format_style = source_format_style;
	csec->fname = as_cache_get_section_fname_for (cache, AS_CACHE_SCOPE_WRITABLE, scope, section_key);
	csec->refine_func_udata = refine_user_data;

	csec->silo = as_cache_components_to_internal_xb (cache,
							 cpts,
							 TRUE, /* refine */
							 csec->refine_func_udata,
							 error);
	if (csec->silo == NULL)
		return FALSE;

	/* write data to cache directory - XbSilo will do an atomic write, so this is safe */
	g_debug ("Writing cache file: %s", csec->fname);
	file = g_file_new_for_path (csec->fname);
	if (!xb_silo_save_to_file (csec->silo, file, NULL, &tmp_error)) {
		g_propagate_prefixed_error (error,
					    tmp_error,
					    "Unable to write cache file:");
		ret = FALSE;
	}

	/* register the new section */
	g_ptr_array_add (priv->sections,
			 g_steal_pointer (&csec));

	/* fix up section ordering */
	g_ptr_array_sort (priv->sections, as_cache_section_cmp);

	return ret;
}

/**
 * as_cache_set_contents_for_section:
 * @cache: an #AsCache instance.
 *
 * Set cache contents for components belonging to the operating system,
 * or other well-defined modules that AppStream can add automatically
 * and that it knows a sensible sectoon key for.
 * All previous contents of the cache section will be removed.
 */
gboolean
as_cache_set_contents_for_section (AsCache *cache,
				   AsComponentScope scope,
				   AsFormatStyle source_format_style,
				   gboolean is_os_data,
				   GPtrArray *cpts,
				   const gchar *cache_key,
				   gpointer refinefn_user_data,
				   GError **error)
{
	return as_cache_set_contents_internal (cache,
						scope,
						source_format_style,
						is_os_data,
						cpts,
						cache_key,
						refinefn_user_data,
						error);
}

/**
 * as_cache_set_contents_for_path:
 * @cache: an #AsCache instance.
 *
 * Set cache contents for components added by the API user, which may be in
 * completely arbitrary paths.
 * All previous contents of the cache section will be removed.
 */
gboolean
as_cache_set_contents_for_path (AsCache *cache,
				GPtrArray *cpts,
				const gchar *filename,
				gpointer refinefn_user_data,
				GError **error)
{
	if (g_strcmp0 (filename, "os-catalog") == 0 || g_strcmp0 (filename, "flatpak") == 0 || g_strcmp0 (filename, "metainfo") == 0) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_BAD_VALUE,
			     "Can not add extra repository data with reserved cache key name '%s'.", filename);
		return FALSE;

	}

	return as_cache_set_contents_internal (cache,
						as_utils_guess_scope_from_path (filename),
						AS_FORMAT_STYLE_COLLECTION,
						FALSE, /* no OS data */
						cpts,
						filename,
						refinefn_user_data,
						error);
}

/**
 * as_cache_get_ctime_with_section_key:
 * @cache: An instance of #AsCache.
 * @scope: Component scope
 * @section_key: An internal cache section key.
 * @cache_scope: Returns which scope the cache that was most recent is in.
 *
 * Returns: ctime of the cache section.
 */
static time_t
as_cache_get_ctime_with_section_key (AsCache *cache, AsComponentScope scope, const gchar *section_key, AsCacheScope *cache_scope)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	struct stat cache_sbuf;
	g_autofree gchar *xb_fname_user = NULL;
	g_autofree gchar *xb_fname_sys = NULL;
	time_t t_sys, t_user;

	xb_fname_user = as_cache_get_section_fname_for (cache,
							AS_CACHE_SCOPE_WRITABLE,
							scope,
							section_key);
	if (g_strcmp0 (priv->cache_root_dir, priv->system_cache_dir) != 0) {
		xb_fname_sys = as_cache_get_section_fname_for (cache,
							AS_CACHE_SCOPE_SYSTEM,
							scope,
							section_key);
	}

	if (cache_scope != NULL)
		*cache_scope = AS_CACHE_SCOPE_WRITABLE;

	t_user = 0;
	t_sys = 0;
	if (stat (xb_fname_user, &cache_sbuf) == 0)
		t_user = cache_sbuf.st_ctime;
	if (xb_fname_sys == NULL) {
		return t_user;
	} else {
		if (stat (xb_fname_sys, &cache_sbuf) == 0)
			t_sys = cache_sbuf.st_ctime;
		if (t_sys > t_user) {
			if (cache_scope != NULL)
				*cache_scope = AS_CACHE_SCOPE_SYSTEM;
			return t_sys;
		}
		return t_user;
	}
}

/**
 * as_cache_get_ctime:
 * @cache: An instance of #AsCache.
 * @scope: Component scope
 * @cache_key: A user-provided cache key.
 * @cache_scope: returns which scope the cache that was most recent is in.
 *
 * Returns: ctime of the cache section.
 */
time_t
as_cache_get_ctime (AsCache *cache, AsComponentScope scope, const gchar *cache_key, AsCacheScope *cache_scope)
{
	g_autofree gchar *section_key = NULL;

	if (scope == AS_COMPONENT_SCOPE_UNKNOWN) {
		scope = AS_COMPONENT_SCOPE_SYSTEM;
		if (g_str_has_prefix (cache_key, "/home") || g_str_has_prefix (cache_key, g_get_home_dir ()))
			scope = AS_COMPONENT_SCOPE_USER;
	}

	section_key = as_cache_build_section_key (cache, cache_key);
	return as_cache_get_ctime_with_section_key (cache, scope, section_key, cache_scope);
}

/**
 * as_cache_check_section_outdated:
 *
 * Returns: %TRUE if ctime of @origin_path is newer than the ctime on the cache section.
 */
static gboolean
as_cache_check_section_outdated (AsCache *cache, AsComponentScope scope, const gchar *cache_key, const gchar *origin_path)
{
	struct stat sb;
	time_t cache_ctime;

	if (stat (origin_path, &sb) < 0)
		return TRUE;

	cache_ctime = as_cache_get_ctime (cache, scope, cache_key, NULL);
	if (sb.st_ctime > cache_ctime)
		return TRUE;

	return FALSE;
}

/**
 * as_cache_load_section_internal:
 *
 * Load cache section.
 */
static void
as_cache_load_section_internal (AsCache *cache,
				AsComponentScope scope,
				const gchar *cache_key,
				AsFormatStyle source_format_style,
				gboolean is_os_data,
				gboolean *is_outdated,
				gpointer refine_user_data)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	AsCacheScope cache_scope;
	g_autofree gchar *section_key = NULL;
	g_autofree gchar *internal_section_key = NULL;
	g_autofree gchar *xb_fname = NULL;
	g_autoptr(AsCacheSection) csec = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	section_key = as_cache_build_section_key (cache, cache_key);
	internal_section_key = g_strconcat (as_component_scope_to_string (scope), ":", section_key, NULL);

	/* check which available cache is the most recent one */
	as_cache_get_ctime_with_section_key (cache,
					     scope,
					     section_key,
					     &cache_scope);

	xb_fname = as_cache_get_section_fname_for (cache,
						   cache_scope,
						   scope,
						   section_key);

	if (!g_file_test (xb_fname, G_FILE_TEST_EXISTS)) {
		/* nothing to do if no cache file exists that we can load */
		if (is_outdated != NULL)
			*is_outdated = TRUE;
		return;
	}

	csec = as_cache_section_new (internal_section_key);
	csec->is_os_data = is_os_data && scope == AS_COMPONENT_SCOPE_SYSTEM;
	csec->scope = scope;
	csec->format_style = source_format_style;
	csec->fname = g_strdup (xb_fname);
	csec->refine_func_udata = refine_user_data;

	csec->silo = xb_silo_new ();
	file = g_file_new_for_path (csec->fname);
	if (!xb_silo_load_from_file (csec->silo,
				     file,
				     XB_SILO_LOAD_FLAG_NONE,
				     NULL,
				     &tmp_error)) {
		g_debug ("Failed to load AppStream cache section '%s' - marking cache as outdated. Issue: %s",
			 internal_section_key, tmp_error->message);
		if (is_outdated != NULL)
			*is_outdated = TRUE;
		return;

	}

	/* register the new section, replacing any old data */
	for (guint i = 0; i < priv->sections->len; i++) {
		AsCacheSection *csec_entry = g_ptr_array_index (priv->sections, i);
		if (g_strcmp0 (csec_entry->key, internal_section_key) == 0) {
			g_ptr_array_remove_index_fast (priv->sections, i);
			break;
		}
	}
	g_ptr_array_add (priv->sections,
			 g_steal_pointer (&csec));
	g_debug ("Using cache file: %s", xb_fname);

	/* fix up section ordering */
	g_ptr_array_sort (priv->sections, as_cache_section_cmp);
}

/**
 * as_cache_load_section_for_key:
 * @cache: an #AsCache instance.
 *
 * Load cache section that was created using a custom, special key.
 */
void
as_cache_load_section_for_key (AsCache *cache,
			       AsComponentScope scope,
				AsFormatStyle source_format_style,
				gboolean is_os_data,
				const gchar *cache_key,
				gboolean *is_outdated,
			        gpointer refinefn_user_data)
{
	as_cache_load_section_internal (cache,
					scope,
					cache_key,
					source_format_style,
					is_os_data,
					is_outdated,
					refinefn_user_data);
}

/**
 * as_cache_load_section_for_path:
 * @cache: an #AsCache instance.
 *
 * Try to load possibly cached data for a generic, custom path.
 */
void
as_cache_load_section_for_path (AsCache *cache,
				const gchar *filename,
				gboolean *is_outdated,
				gpointer refinefn_user_data)
{
	AsComponentScope scope;

	scope = as_utils_guess_scope_from_path (filename);
	if (is_outdated != NULL)
		*is_outdated = as_cache_check_section_outdated (cache, scope, filename, filename);

	as_cache_load_section_internal (cache,
					scope,
					filename,
					AS_FORMAT_STYLE_COLLECTION,
					FALSE,
					is_outdated,
					refinefn_user_data);
}

/**
 * as_cache_mask_by_data_id:
 * @cache: an #AsCache instance.
 * @cdid: Data-ID of an #AsComponent
 *
 * Mark a component as "deleted" from the cache.
 * The component will no longer show up in search results or be returned
 * by other queries.
 */
void
as_cache_mask_by_data_id (AsCache *cache, const gchar *cdid)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);
	g_hash_table_insert (priv->masked,
			     g_strdup (cdid),
			     GINT_TO_POINTER (TRUE));
}

/**
 * as_cache_add_masking_component:
 * @cache: an #AsCache instance.
 * @cpts: An array of #AsComponent to add as override
 *
 * Add a masking component to the cache that is returned in place of
 * existing components in the cache.
 */
gboolean
as_cache_add_masking_components (AsCache *cache, GPtrArray *cpts, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(AsCacheSection) old_mcsec = NULL;
	g_autoptr(AsCacheSection) mcsec = NULL;
	g_autoptr(GPtrArray) cpts_final = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autofree gchar *volatile_fname = NULL;
	gint fd;
	g_autoptr(GRWLockWriterLocker) locker = g_rw_lock_writer_locker_new (&priv->rw_lock);

	/* find old masking section */
	for (guint i = 0; i < priv->sections->len; i++) {
		AsCacheSection *csec_entry = g_ptr_array_index (priv->sections, i);
		if (csec_entry->is_mask) {
			old_mcsec = g_ptr_array_steal_index_fast (priv->sections, i);
			break;
		}
	}

	cpts_final = g_ptr_array_new_with_free_func (g_object_unref);
	if (old_mcsec != NULL) {
		g_autoptr(GPtrArray) array = NULL;

		/* retrieve the old data */
		array = xb_silo_query (old_mcsec->silo,
					"components/component", 0,
					NULL);
		if (array != NULL) {
			for (guint j = 0; j < array->len; j++) {
				g_autoptr (AsComponent) cpt = NULL;
				gpointer iptr;
				XbNode *node = g_ptr_array_index (array, j);

				cpt = as_cache_component_from_node (cache,
								    old_mcsec,
								    node,
								    NULL);
				if (cpt == NULL)
					continue;

				/* just delete masked components */
				iptr = g_hash_table_lookup (priv->masked, as_component_get_data_id (cpt));
				if (iptr != NULL && GPOINTER_TO_INT (iptr) == TRUE)
					continue;

				g_ptr_array_add (cpts_final,
						 g_steal_pointer (&cpt));
				g_hash_table_insert (priv->masked,
							g_strdup (as_component_get_data_id (cpt)),
							GINT_TO_POINTER (FALSE));
			}
		}

		/* drop the old data */
		as_cache_remove_section_file (cache, old_mcsec);
	}

	/* generate filename for the volatile section in memory */
	volatile_fname = g_build_filename (g_get_user_runtime_dir (), "appstream-extra-XXXXXX.mdb", NULL);
	fd = g_mkstemp (volatile_fname);
	if (fd > 0)
		close (fd);

	/* create new section */
	mcsec = as_cache_section_new ("memory:volatile_mask");
	mcsec->scope = AS_COMPONENT_SCOPE_USER;
	mcsec->format_style = AS_FORMAT_STYLE_COLLECTION;
	mcsec->fname = g_steal_pointer (&volatile_fname);
	mcsec->is_mask = TRUE;

	/* create final component set */
	for (guint i = 0; i < cpts->len; i++) {
		AsComponent *cpt = g_ptr_array_index (cpts, i);
		g_ptr_array_add (cpts_final,
				 g_object_ref (cpt));
		g_hash_table_insert (priv->masked,
					g_strdup (as_component_get_data_id (cpt)),
					GINT_TO_POINTER (FALSE));
	}

	mcsec->silo = as_cache_components_to_internal_xb (cache,
							  cpts_final,
							  FALSE, /* do not refine */
							  NULL,
							  &tmp_error);
	if (mcsec->silo == NULL) {
		g_propagate_prefixed_error (error,
					    g_steal_pointer (&tmp_error),
					    "Unable to add masking components to cache: Silo build failed. ");
		return FALSE;
	}

	/* write data */
	file = g_file_new_for_path (mcsec->fname);
	if (!xb_silo_save_to_file (mcsec->silo, file, NULL, &tmp_error)) {
		g_propagate_prefixed_error (error,
					    g_steal_pointer (&tmp_error),
					    "Unable to add masking components to cache: Failed to store silo. ");
		return FALSE;
	}

	/* register the new section */
	g_ptr_array_add (priv->sections,
			 g_steal_pointer (&mcsec));

	/* fix up section ordering */
	g_ptr_array_sort (priv->sections, as_cache_section_cmp);

	return TRUE;
}

/**
 * as_cache_set_refine_func:
 * @cache: An instance of #AsCache.
 *
 * Set function to be called on a component after it was deserialized.
 */
void
as_cache_set_refine_func (AsCache *cache, AsCacheDataRefineFn func)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	priv->cpt_refine_func = func;
}

typedef struct {
	GHashTable *results_map;
	GHashTable *known_os_cids;
} AsQueryContext;

static AsQueryContext*
as_query_context_new ()
{
	AsQueryContext *ctx;
	ctx = g_new0 (AsQueryContext, 1);
	ctx->results_map = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_object_unref);
	ctx->known_os_cids = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    NULL);
	return ctx;
}

static void
as_query_context_free (AsQueryContext *ctx)
{
	g_hash_table_unref (ctx->results_map);
	g_hash_table_unref (ctx->known_os_cids);
	g_free (ctx);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsQueryContext, as_query_context_free)

static gboolean
as_query_context_add_component_from_node (AsQueryContext *ctx,
					  AsCache *cache,
					  AsCacheSection *csec,
					  XbNode *cpt_node,
					  AsTokenType match_value,
					  GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *data_id;

	if (csec->is_os_data && csec->format_style == AS_FORMAT_STYLE_METAINFO) {
		const gchar *cid = xb_node_query_text (cpt_node, "id", NULL);
		if (g_hash_table_contains (ctx->known_os_cids, cid) &&
			!priv->prefer_os_metainfo)
			return TRUE;
	}

	cpt = as_cache_component_from_node (cache,
						csec,
						cpt_node,
						error);
	if (cpt == NULL)
		return FALSE;
	if (csec->format_style == AS_FORMAT_STYLE_METAINFO)
		as_component_set_origin_kind (cpt, AS_ORIGIN_KIND_METAINFO);

	/* don't display masked components */
	if (!csec->is_mask && g_hash_table_contains (priv->masked, as_component_get_data_id (cpt)))
		return TRUE;

	/* add match score (in case we are full-text searching) */
	if (match_value != 0)
		as_component_set_sort_score (cpt, match_value);

	/* register */
	if (csec->is_os_data)
		g_hash_table_add (ctx->known_os_cids,
					g_strdup (as_component_get_id (cpt)));

	data_id = as_component_get_data_id (cpt);
	g_hash_table_insert (ctx->results_map,
				g_strdup (data_id),
				g_steal_pointer (&cpt));

	return TRUE;
}

static gboolean
as_query_context_add_component_from_nodes (AsQueryContext *ctx,
					   AsCache *cache,
					   AsCacheSection *csec,
					   GPtrArray *nodes,
					   GError **error)
{
	for (guint i = 0; i < nodes->len; i++) {
		XbNode *qnode = XB_NODE (g_ptr_array_index (nodes, i));
		if (!as_query_context_add_component_from_node (ctx, cache, csec, qnode, 0, error))
			return FALSE;
	}
	return TRUE;
}

static GPtrArray*
as_query_context_retrieve_components (AsQueryContext *ctx)
{
	GHashTableIter ht_iter;
	gpointer ht_value;
	GPtrArray *results = g_ptr_array_new_with_free_func (g_object_unref);

	g_hash_table_iter_init (&ht_iter, ctx->results_map);
	while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value))
		g_ptr_array_add (results, g_object_ref (ht_value));

	return results;
}

/**
 * as_cache_query_components_internal:
 */
static GPtrArray*
as_cache_query_components (AsCache *cache,
			   const gchar *xpath,
			   XbQueryContext *context,
			   guint limit,
			   gboolean is_fts,
			   GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(AsQueryContext) qctx = NULL;
	g_autoptr(GRWLockReaderLocker) locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

	qctx = as_query_context_new ();
	for (guint i = 0; i < priv->sections->len; i++) {
		g_autoptr(GPtrArray) array = NULL;
		g_autoptr(GError) tmp_error = NULL;
		g_autoptr(XbQuery) query = NULL;
		AsCacheSection *csec = (AsCacheSection*) g_ptr_array_index (priv->sections, i);

		g_debug ("Querying `%s` in %s", xpath, csec->key);
		query = xb_query_new (csec->silo, xpath, &tmp_error);
		if (query == NULL) {
			g_propagate_prefixed_error (error,
							g_steal_pointer (&tmp_error),
							"Unable to construct query: ");
			return NULL;
		}

		array = xb_silo_query_with_context (csec->silo,
						    query,
						    context,
						    &tmp_error);
		if (array == NULL) {
			if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
				continue;
			if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT))
				continue;
			g_propagate_prefixed_error (error,
							g_steal_pointer (&tmp_error),
							"Unable to run query: ");
			return NULL;
		}

		if (!as_query_context_add_component_from_nodes (qctx, cache, csec, array, error))
			return NULL;

	}

	return as_query_context_retrieve_components (qctx);
}

/**
 * as_cache_get_components_all:
 * @cache: An instance of #AsCache.
 * @error: A #GError or %NULL.
 *
 * Retrieve all components this cache contains.
 *
 * Returns: (transfer full): An array of #AsComponent
 */
GPtrArray*
as_cache_get_components_all (AsCache *cache, GError **error)
{
	return as_cache_query_components (cache,
					  "components/component",
					  NULL,
					  0,
					  FALSE,
					  error);
}

/**
 * as_cache_get_components_by_id:
 * @cache: An instance of #AsCache.
 * @id: The component ID to search for.
 * @error: A #GError or %NULL.
 *
 * Retrieve components with the selected ID.
 *
 * Returns: (transfer full): An array of #AsComponent
 */
GPtrArray*
as_cache_get_components_by_id (AsCache *cache, const gchar *id, GError **error)
{
	GPtrArray *results = NULL;
	g_autofree gchar *id_lower = NULL;
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();

	id_lower = g_utf8_strdown (id, -1);
	xb_value_bindings_bind_str (xb_query_context_get_bindings (&context), 0, id_lower, NULL);
	results = as_cache_query_components (cache,
					     "components/component/id[lower-case(text())=?]/..",
					     &context,
					     0,
					     FALSE,
					     error);

	/* don't continue if we have an error */
	if (results == NULL)
		return results;

	if (results->len == 0) {
		/* we found no exact matches, try components providing this ID */
		g_ptr_array_unref (results);
		results = as_cache_query_components (cache,
						     "components/component/provides/id[lower-case(text())=?]/../..",
						     &context,
						     0,
						     FALSE,
						     error);
	}

	return results;
}

/**
 * as_cache_get_components_by_extends:
 * @cache: An instance of #AsCache.
 * @extends_id: The component ID to find extensions for.
 * @error: A #GError or %NULL.
 *
 * Retrieve components extending a component with the selected ID.
 *
 * Returns: (transfer full): An array of #AsComponent
 */
GPtrArray*
as_cache_get_components_by_extends (AsCache *cache, const gchar *extends_id, GError **error)
{
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();
	xb_value_bindings_bind_str (xb_query_context_get_bindings (&context), 0, extends_id, NULL);
	return as_cache_query_components (cache,
					  "components/component/extends[text()=?]/..",
					  &context,
					  0,
					  FALSE,
					  error);
}

/**
 * as_cache_get_components_by_kind:
 * @cache: An instance of #AsCache.
 * @kind: The #AsComponentKind to retrieve.
 * @error: A #GError or %NULL.
 *
 * Retrieve components of a specific kind.
 *
 * Returns: (transfer full): An array of #AsComponent
 */
GPtrArray*
as_cache_get_components_by_kind (AsCache *cache, AsComponentKind kind, GError **error)
{
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();
	xb_value_bindings_bind_str (xb_query_context_get_bindings (&context),
				    0,
				    as_component_kind_to_string (kind),
				    NULL);
	return as_cache_query_components (cache,
					  "components/component[@type=?]",
					  &context,
					  0,
					  FALSE,
					  error);
}

/**
 * as_cache_get_components_by_provided_item:
 * @cache: An instance of #AsCache.
 * @kind: Kind of the provided item.
 * @item: Name of the item.
 * @error: A #GError or %NULL.
 *
 * Retrieve a list of components that provide a certain item.
 *
 * Returns: (transfer full): An array of #AsComponent
 */
GPtrArray*
as_cache_get_components_by_provided_item (AsCache *cache, AsProvidedKind kind, const gchar *item, GError **error)
{
	const gchar *kind_node_name = NULL;
	const gchar *xpath_query_tmpl = NULL;
	const gchar *xpath_query_type_tmpl = NULL;
	g_autofree gchar *xpath_query = NULL;
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();
	XbValueBindings *vbindings = xb_query_context_get_bindings (&context);

	xpath_query_tmpl = "components/component/provides/%s[text()=?]/../..";
	xpath_query_type_tmpl = "components/component/provides/%s[text()=?][@type='%s']/../..";

	if (kind == AS_PROVIDED_KIND_LIBRARY)
		kind_node_name = "library";
	else if (kind == AS_PROVIDED_KIND_BINARY)
		kind_node_name = "binary";
	else if (kind == AS_PROVIDED_KIND_DBUS_SYSTEM)
		xpath_query = g_strdup_printf (xpath_query_type_tmpl, "dbus", "system");
	else if (kind == AS_PROVIDED_KIND_DBUS_USER)
		xpath_query = g_strdup_printf (xpath_query_type_tmpl, "dbus", "user");
	else if (kind == AS_PROVIDED_KIND_FIRMWARE_RUNTIME)
		xpath_query = g_strdup_printf (xpath_query_type_tmpl, "firmware", "runtime");
	else if (kind == AS_PROVIDED_KIND_FIRMWARE_FLASHED)
		xpath_query = g_strdup_printf (xpath_query_type_tmpl, "firmware", "flashed");
	else
		kind_node_name = as_provided_kind_to_string (kind);

	if (xpath_query == NULL)
		xpath_query = g_strdup_printf (xpath_query_tmpl, kind_node_name);
	xb_value_bindings_bind_str (vbindings, 0,
				    item,
				    NULL);
	return as_cache_query_components (cache,
					  xpath_query,
					  &context,
					  0,
					  FALSE,
					  error);
}

/**
 * as_cache_get_components_by_categories:
 * @cache: An instance of #AsCache.
 * @categories: List of category names.
 * @error: A #GError or %NULL.
 *
 * get a list of components in the selected categories.
 *
 * Returns: (transfer full): An array of #AsComponent
 */
GPtrArray*
as_cache_get_components_by_categories (AsCache *cache, gchar **categories, GError **error)
{
	g_autoptr(GString) xpath = NULL;
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();
	XbValueBindings *vbindings = xb_query_context_get_bindings (&context);

	if (categories == NULL || categories[0] == NULL)
		return g_ptr_array_new_with_free_func (g_object_unref);

	xpath = g_string_new ("components/component/categories");
	for (guint i = 0; categories[i] != NULL; i++) {
		g_string_append (xpath, "/category[text()=?]/..");
		xb_value_bindings_bind_str (vbindings, i,
					    categories[i],
					    NULL);
	}
	g_string_append (xpath, "/..");

	return as_cache_query_components (cache,
					  xpath->str,
					  &context,
					  0,
					  FALSE,
					  error);
}

/**
 * as_cache_get_components_by_launchable:
 * @cache: An instance of #AsCache.
 * @kind: Type of the launchable.
 * @id: ID of the launchable.
 * @error: A #GError or %NULL.
 *
 * Get components which provide a certain launchable.
 *
 * Returns: (transfer full): An array of #AsComponent
 */
GPtrArray*
as_cache_get_components_by_launchable (AsCache *cache, AsLaunchableKind kind, const gchar *id, GError **error)
{
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();
	g_autofree gchar *xpath = NULL;
	xb_value_bindings_bind_str (xb_query_context_get_bindings (&context),
				    0, id, NULL);
	xpath = g_strdup_printf ("components/component/launchable[@type='%s'][text()=?]/..", as_launchable_kind_to_string (kind));
	return as_cache_query_components (cache,
					  xpath,
					  &context,
					  0,
					  FALSE,
					  error);
}

typedef struct {
	AsSearchTokenMatch	match_value;
	XbQuery			*query;
} AsFTSearchHelper;

static void
as_ftsearch_helper_free (AsFTSearchHelper *helper)
{
	g_object_unref (helper->query);
	g_free (helper);
}

static AsTokenType
as_cache_search_component_node_term (GPtrArray *array, XbNode *cpt_node, const gchar *term)
{
	AsTokenType match_value = 0;

	/* run all queries on the component node */
	for (guint i = 0; i < array->len; i++) {
		g_autoptr(GPtrArray) n = NULL;
		g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();
		AsFTSearchHelper *helper = g_ptr_array_index (array, i);

		xb_value_bindings_bind_str (xb_query_context_get_bindings (&context), 0, term, NULL);
		n = xb_node_query_with_context (cpt_node, helper->query, &context, NULL);
		if (n != NULL)
			match_value |= helper->match_value;
	}

	return match_value;
}

static AsTokenType
as_cache_search_component_node_terms (GPtrArray *array, XbNode *cpt_node, const gchar * const *terms)
{
	AsTokenType matches_sum = 0;

	/* match all search terms */
	for (guint i = 0; terms[i] != NULL; i++) {
		AsTokenType tmp = as_cache_search_component_node_term (array, cpt_node, terms[i]);
		if (tmp == 0)
			return 0;
		matches_sum |= tmp;
	}
	return matches_sum;
}

/**
 * as_cache_search:
 * @cache: An instance of #AsCache.
 * @terms: List of search terms
 * @sort: %TRUE if results should be sorted by score.
 * @error: A #GError or %NULL.
 *
 * Perform a search for the given pre-stemmed terms.
 *
 * Returns: (transfer container): An array of #AsComponent
 */
GPtrArray*
as_cache_search (AsCache *cache, const gchar * const *terms, gboolean sort, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(AsQueryContext) qctx = NULL;
	g_autoptr(GRWLockReaderLocker) locker = NULL;

	const struct {
		AsSearchTokenMatch	match_value;
		const gchar		*xpath;
	} queries[] = {
		{ AS_SEARCH_TOKEN_MATCH_MEDIATYPE,	"provides/mediatype[text()~=?]" },
		{ AS_SEARCH_TOKEN_MATCH_PKGNAME,	"pkgname[text()~=?]" },
		{ AS_SEARCH_TOKEN_MATCH_SUMMARY,	"summary[text()~=?]" },
		{ AS_SEARCH_TOKEN_MATCH_NAME,		"name[text()~=?]" },
		{ AS_SEARCH_TOKEN_MATCH_DESCRIPTION,	"_asi_tokens/t[text()~=?]" },
		{ AS_SEARCH_TOKEN_MATCH_ID,		"id[text()~=?]" },
		{ AS_SEARCH_TOKEN_MATCH_ORIGIN,		"_asi_origin[text()~=?]" },
		{ AS_SEARCH_TOKEN_MATCH_NONE,		NULL }
	};

	if (terms == NULL || terms[0] == NULL)
		return g_ptr_array_new_with_free_func (g_object_unref);

	/* lock for reading */
	locker = g_rw_lock_reader_locker_new (&priv->rw_lock);

	qctx = as_query_context_new ();
	for (guint i = 0; i < priv->sections->len; i++) {
		g_autoptr(GPtrArray) array = NULL;
		g_autoptr(GPtrArray) cpt_nodes = NULL;
		g_autoptr(GError) tmp_error = NULL;
		AsCacheSection *csec = (AsCacheSection*) g_ptr_array_index (priv->sections, i);

		g_debug ("Full text search in %s", csec->key);

		/* add weighted queries */
		array = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ftsearch_helper_free);
		for (guint i = 0; queries[i].xpath != NULL; i++) {
			g_autoptr(GError) error_query = NULL;
			g_autoptr(XbQuery) query = xb_query_new (csec->silo, queries[i].xpath, &error_query);
			if (query != NULL) {
				AsFTSearchHelper *helper = g_new0 (AsFTSearchHelper, 1);
				helper->match_value = queries[i].match_value;
				helper->query = g_steal_pointer (&query);
				g_ptr_array_add (array, helper);
			} else {
				g_debug ("Unable to create query (ignoring it): %s", error_query->message);
			}
		}

		/* get nodes for all components */
		cpt_nodes = xb_silo_query (csec->silo, "components/component", 0, &tmp_error);
		if (cpt_nodes == NULL) {
			if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
				continue;
			if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT))
				continue;
			g_propagate_prefixed_error (error,
							g_steal_pointer (&tmp_error),
							"Unable to run query: ");
			return NULL;
		}

		for (guint i = 0; i < cpt_nodes->len; i++) {
			XbNode *cpt_node = g_ptr_array_index (cpt_nodes, i);
			AsTokenType match_value = as_cache_search_component_node_terms (array, cpt_node, terms);
			if (match_value != 0) {
				if (!as_query_context_add_component_from_node (qctx,
									       cache,
									       csec,
									       cpt_node,
									       match_value,
									       error))
					return NULL;
			}
		}

	}

	results = as_query_context_retrieve_components (qctx);

	/* sort the results by their priority */
	if (sort)
		as_sort_components_by_score (results);

	return g_steal_pointer (&results);
}
