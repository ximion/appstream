/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2019 Matthias Klumpp <matthias@tenstral.net>
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
 * Internally, a cache is backed by an LMDB database.
 *
 * See also: #AsPool
 */

#include "config.h"
#include "as-cache.h"

#include <lmdb.h>
#include <errno.h>
#include <glib/gstdio.h>

#include "as-utils-private.h"
#include "as-context.h"
#include "as-component-private.h"
#include "as-launchable.h"

/*
 * The maximum size of the cache file (512MiB).
 * The file is mmap(2)'d into memory.
 */
static const size_t LMDB_DB_SIZE = 1024 * 1024 * 512;

/* format version of the currently supported cache */
static const gchar *CACHE_FORMAT_VERSION = "1";

typedef struct
{
	MDB_env *db_env;
	MDB_dbi db_cpts;
	MDB_dbi db_cids;
	MDB_dbi db_fts;
	MDB_dbi db_cats;
	MDB_dbi db_launchables;
	MDB_dbi db_provides;
	MDB_dbi db_kinds;
	gchar *fname;
	gchar *volatile_db_fname;
	gsize max_keysize;
	gboolean opened;

	AsContext *context;
	gchar *locale;

	gboolean floating;
	GHashTable *cpt_map;
	GHashTable *cid_set;
} AsCachePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsCache, as_cache, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_cache_get_instance_private (o))

/**
 * as_cache_init:
 **/
static void
as_cache_init (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);

	priv->opened = FALSE;
	priv->max_keysize = 511;
	priv->locale = as_get_current_locale ();

	priv->context = as_context_new ();
	as_context_set_locale (priv->context, priv->locale);
	as_context_set_style (priv->context, AS_FORMAT_STYLE_COLLECTION);

	priv->floating = FALSE;

	/* stores known components in-memory for faster access */
	priv->cpt_map = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						(GDestroyNotify) g_object_unref);

	/* set which stores whether we have seen a component-ID already */
	priv->cid_set = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						NULL);
}

/**
 * as_cache_finalize:
 **/
static void
as_cache_finalize (GObject *object)
{
	AsCache *cache = AS_CACHE (object);
	AsCachePrivate *priv = GET_PRIVATE (cache);

	g_object_unref (priv->context);
	as_cache_close (cache);
	g_free (priv->locale);

	g_hash_table_unref (priv->cpt_map);
	g_hash_table_unref (priv->cid_set);

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
 * as_cache_open_subdb:
 *
 * helper for %as_cache_open()
 */
static gboolean
as_cache_open_subdb (MDB_txn *txn, const gchar *name, MDB_dbi *dbi, GError **error)
{
	gint rc;
	rc = mdb_dbi_open (txn, name, MDB_CREATE, dbi);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to read %s database: %s", name, mdb_strerror (rc));
		return FALSE;
	}
	return TRUE;
}

/**
 * as_cache_transaction_new:
 */
static MDB_txn*
as_cache_transaction_new (AsCache *cache, guint flags, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	gint rc;
	MDB_txn *txn;

	rc = mdb_txn_begin (priv->db_env, NULL, flags, &txn);
        if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to create transaction: %s", mdb_strerror (rc));
		return NULL;
	}

        return txn;
}

/**
 * lmdb_transaction_commit:
 */
static gboolean
lmdb_transaction_commit (MDB_txn *txn, GError **error)
{
	gint rc;
	rc = mdb_txn_commit (txn);
        if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to commit transaction: %s", mdb_strerror (rc));
		return FALSE;
	}

        return TRUE;
}

/**
 * lmdb_transaction_abort:
 */
static void
lmdb_transaction_abort (MDB_txn *txn)
{
	if (txn == NULL)
		return;
	mdb_txn_abort (txn);
}

/**
 * lmdb_str_to_dbval:
 *
 * Get MDB_val for string.
 */
inline static MDB_val
lmdb_str_to_dbval (const gchar *data, gssize len)
{
	MDB_val val;
	if (len < 0)
		len = sizeof(gchar) * strlen (data);

	val.mv_size = len;
	val.mv_data = (void*) data;
	return val;
}

/**
 * lmdb_val_strdup:
 *
 * Get string from database value.
 */
inline static gchar*
lmdb_val_strdup (MDB_val val)
{
	if (val.mv_size == 0)
		return NULL;
	return g_strndup (val.mv_data, val.mv_size);
}

/**
 * as_cache_txn_put_kv:
 *
 * Add key/value pair to the database
 */
inline static gboolean
as_cache_txn_put_kv (AsCache *cache, MDB_txn *txn, MDB_dbi dbi, const gchar *key, MDB_val dval, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_val dkey;
	gint rc;
	gsize key_len;
	g_autofree gchar *key_hash = NULL;

	/* if key is too long, hash it */
	key_len = sizeof(gchar) * strlen (key);
	if (key_len > priv->max_keysize) {
		key_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, key, key_len);
		dkey = lmdb_str_to_dbval (key_hash, -1);
	} else {
		dkey = lmdb_str_to_dbval (key, key_len);
	}

        rc = mdb_put (txn, dbi, &dkey, &dval, 0);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to add data: %s", mdb_strerror (rc));
		return FALSE;
	}

	return TRUE;
}

/**
 * as_cache_txn_put_kv_str:
 *
 * Add key/value pair to the database
 */
inline static gboolean
as_cache_txn_put_kv_str (AsCache *cache, MDB_txn *txn, MDB_dbi dbi, const gchar *key, const gchar *value, GError **error)
{
	MDB_val dvalue;
        dvalue = lmdb_str_to_dbval (value, -1);

	return as_cache_txn_put_kv (cache, txn, dbi, key, dvalue, error);
}

/**
 * as_cache_txn_delete:
 *
 * Remove key from the database.
 */
inline static gboolean
lmdb_txn_delete (MDB_txn *txn, MDB_dbi dbi, const gchar *key, GError **error)
{
	MDB_val dkey;
	gint rc;
        dkey = lmdb_str_to_dbval (key, -1);

	rc = mdb_del (txn, dbi, &dkey, NULL);
        if ((rc != MDB_NOTFOUND) && (rc != MDB_SUCCESS)) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to remove data: %s", mdb_strerror (rc));
		return FALSE;
	}

	return rc != MDB_NOTFOUND;
}

/**
 * as_cache_put_kv:
 *
 * Add key/value pair to the database
 */
static gboolean
as_cache_put_kv (AsCache *cache, MDB_dbi dbi, const gchar *key, const gchar *value, GError **error)
{
	MDB_txn *txn = as_cache_transaction_new (cache, 0, error);
	if (txn == NULL)
		return FALSE;

	if (!as_cache_txn_put_kv_str (cache, txn, dbi, key, value, error)) {
		lmdb_transaction_abort (txn);
		return FALSE;
	}

	return lmdb_transaction_commit (txn, error);
}

/**
 * as_cache_txn_get_value:
 */
inline static MDB_val
as_cache_txn_get_value (AsCache *cache, MDB_txn *txn, MDB_dbi dbi, const gchar *key, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_cursor *cur;
	MDB_val dkey;
	MDB_val dval = {0, NULL};
	gsize key_len;
	g_autofree gchar *key_hash = NULL;
	gint rc;

	if ((key == NULL) || (key[0] == '\0'))
		return dval;

        /* if key is too long, we added it as hash. So look for the hash instead. */
	key_len = sizeof(gchar) * strlen (key);
	if (key_len > priv->max_keysize) {
		key_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, key, key_len);
		dkey = lmdb_str_to_dbval (key_hash, -1);
	} else {
		dkey = lmdb_str_to_dbval (key, key_len);
	}

	rc = mdb_cursor_open (txn, dbi, &cur);
        if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to read data (no cursor): %s", mdb_strerror (rc));
		return dval;
	}

        rc = mdb_cursor_get (cur, &dkey, &dval, MDB_SET);
        if (rc == MDB_NOTFOUND) {
		mdb_cursor_close(cur);
		return dval;
	}
        if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to read data: %s", mdb_strerror (rc));
		mdb_cursor_close(cur);
		return dval;
	}

	mdb_cursor_close(cur);
	return dval;
}

/**
 * as_cache_get_value:
 */
inline static MDB_val
as_cache_get_value (AsCache *cache, MDB_dbi dbi, const gchar *key, GError **error)
{
	MDB_txn *txn;
	GError *tmp_error = NULL;
	MDB_val data = {0, NULL};

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return data;

	data = as_cache_txn_get_value (cache, txn, dbi, key, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return data;
	}

	lmdb_transaction_commit (txn, NULL);
	return data;
}

/**
 * as_cache_check_opened:
 */
inline static gboolean
as_cache_check_opened (AsCache *cache, gboolean allow_floating, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);

	if (!allow_floating && priv->floating) {
		g_set_error (error,
		     AS_CACHE_ERROR,
		     AS_CACHE_ERROR_FLOATING,
		     "Can not perform this action on a floating cache.");
		return FALSE;
	}

	if (priv->opened)
		return TRUE;

	g_set_error (error,
		     AS_CACHE_ERROR,
		     AS_CACHE_ERROR_NOT_OPEN,
		     "Can not perform this action on an unopened cache.");
	return FALSE;
}

/**
 * as_cache_component_to_xml:
 */
static gchar*
as_cache_component_to_xml (AsCache *cache, AsComponent *cpt)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	xmlDoc *doc;
	xmlNode *node;
	xmlBufferPtr buf;
	xmlSaveCtxtPtr sctx;
	gchar *res;

	node = as_component_to_xml_node (cpt, priv->context, NULL);
	if (node == NULL)
		return NULL;

	doc = xmlNewDoc ((xmlChar*) NULL);
	xmlDocSetRootElement (doc, node);

	buf = xmlBufferCreate ();
	sctx = xmlSaveToBuffer (buf, "utf-8", XML_SAVE_FORMAT | XML_SAVE_NO_DECL);
	xmlSaveDoc (sctx, doc);
	xmlSaveClose (sctx);

	res = g_strdup ((const gchar*) xmlBufferContent (buf));
	xmlBufferFree (buf);
	xmlFreeDoc (doc);

	return res;
}

/**
 * as_cache_open:
 * @cache: An instance of #AsCache.
 * @fname: The filename of the cache file (or ":memory" or ":temporary" for an in-memory or temporary cache)
 * @locale: The locale with which the cache should be created, if a new cache was created.
 * @error: A #GError or %NULL.
 *
 * Open/create an AppStream cache file.
 *
 * Returns: %TRUE if cache was opened successfully.
 **/
gboolean
as_cache_open (AsCache *cache, const gchar *fname, const gchar *locale, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	GError *tmp_error = NULL;
	gint rc;
	const gchar *volatile_dir = NULL;
	MDB_txn *txn = NULL;
	MDB_dbi db_config;
	g_autofree gchar *cache_format;

	rc = mdb_env_create (&priv->db_env);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to initialize an MDB environment: %s", mdb_strerror (rc));
		goto fail;
	}

	rc = mdb_env_set_maxdbs (priv->db_env, 8);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to set MDB max DB count: %s", mdb_strerror (rc));
		goto fail;
	}

	rc = mdb_env_set_mapsize (priv->db_env, LMDB_DB_SIZE);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to set DB mapsize: %s", mdb_strerror (rc));
		goto fail;
	}

	/* determine where to put a volatile database */
	if (g_strcmp0 (fname, ":temporary") == 0) {
		if (as_utils_is_root ())
			volatile_dir = g_get_tmp_dir ();
		else
			volatile_dir = g_get_user_cache_dir ();
	} else if (g_strcmp0 (fname, ":memory") == 0) {
		volatile_dir = g_get_user_runtime_dir ();
	}

	g_free (priv->fname);
	if (volatile_dir != NULL) {
		gint fd;

		priv->fname = g_build_filename (volatile_dir, "appstream-cache-XXXXXX.mdb", NULL);
		fd = g_mkstemp (priv->fname);
		if (fd < 0) {
			g_set_error (error,
				AS_CACHE_ERROR,
				AS_CACHE_ERROR_FAILED,
				"Unable to open temporary cache file: %s", g_strerror (errno));
			goto fail;
		} else {
			close (fd);
		}

		priv->volatile_db_fname = g_strdup (priv->fname);
	} else {
		priv->fname = g_strdup (fname);
	}

	g_debug ("Opening cache file: %s", priv->fname);
	rc = mdb_env_open (priv->db_env,
			   priv->fname,
			   MDB_NOSUBDIR | MDB_NOMETASYNC | MDB_NOLOCK,
			   0755);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to open cache: %s", mdb_strerror (rc));
		goto fail;
	}

	/* unlink the file, so it gets removed as soon as we don't need it anymore */
	if (priv->volatile_db_fname != NULL)
		g_unlink (priv->volatile_db_fname);

	/* retrieve the maximum keysize allowed for this database */
	priv->max_keysize = mdb_env_get_maxkeysize (priv->db_env);

	txn = as_cache_transaction_new (cache, 0, &tmp_error);
	if (txn == NULL) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	/* cache settings */
	if (!as_cache_open_subdb (txn, "config", &db_config, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	/* component data as XML */
	if (!as_cache_open_subdb (txn, "components", &priv->db_cpts, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	/* component-ID mapping */
	if (!as_cache_open_subdb (txn, "cpt_ids", &priv->db_cids, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	/* full-text search index */
	if (!as_cache_open_subdb (txn, "fts", &priv->db_fts, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	/* categories */
	if (!as_cache_open_subdb (txn, "categories", &priv->db_cats, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	/* launchables */
	if (!as_cache_open_subdb (txn, "launchables", &priv->db_launchables, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	/* provides */
	if (!as_cache_open_subdb (txn, "provides", &priv->db_provides, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	/* kinds */
	if (!as_cache_open_subdb (txn, "kinds", &priv->db_kinds, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

        if (!lmdb_transaction_commit (txn, &tmp_error)) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	cache_format = lmdb_val_strdup (as_cache_get_value (cache, db_config, "format", &tmp_error));
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}
	if (cache_format == NULL) {
		/* the value was empty, we most likely have a new cache file */
		if (!as_cache_put_kv (cache, db_config, "format", CACHE_FORMAT_VERSION, error))
			goto fail;
	} else if (g_strcmp0 (cache_format, CACHE_FORMAT_VERSION) != 0) {
		/* we try to load an incompatible cache version - emit an error, so it can be recreated */
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_WRONG_FORMAT,
			     "The cache format version is unsupported.");
		goto fail;
	}

	g_free (priv->locale);
	priv->locale = lmdb_val_strdup (as_cache_get_value (cache, db_config, "locale", &tmp_error));
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}
	if (priv->locale == NULL) {
		/* the value was empty, we most likely have a new cache file - so set a locale for it */
		if (!as_cache_put_kv (cache, db_config, "locale", locale, error))
			goto fail;
		priv->locale = g_strdup (locale);
	} else if (g_strcmp0 (priv->locale, locale) != 0) {
		/* we expected a different locale than this cache has - throw an error, just to be safe */
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_LOCALE_MISMATCH,
			     "We expected locale '%s', but the cache was build for '%s'.", locale, priv->locale);
		goto fail;
	}

	/* set locale for XML serialization */
	as_context_set_locale (priv->context, priv->locale);

	priv->opened = TRUE;
	return TRUE;
fail:
	lmdb_transaction_abort (txn);
	if (priv->db_env != NULL)
		mdb_env_close (priv->db_env);
	return  FALSE;
}

/**
 * as_cache_close:
 * @cache: An instance of #AsCache.
 *
 * Close an AppStream cache file.
 * This function can be called after the cache has been opened to
 * explicitly close it and reuse the #AsCache instance.
 * It will also be called when the object is freed.
 *
 * Returns: %TRUE if cache was opened successfully.
 **/
gboolean
as_cache_close (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);

	if (!priv->opened)
		return FALSE;

	mdb_env_close (priv->db_env);

	/* ensure any temporary file is gone, in case we used an in-memory database */
	if (priv->volatile_db_fname != NULL) {
		g_remove (priv->volatile_db_fname);
		g_free (priv->volatile_db_fname);
		priv->volatile_db_fname = NULL;
	}
	g_free (priv->fname);
	priv->fname = NULL;

	priv->opened = FALSE;
	return TRUE;
}

/**
 * as_str_check_append:
 *
 * Helper function for as_cache_insert()
 */
static gchar*
as_str_check_append (const gchar *setstr, const gchar *value)
{
	g_autofree gchar *new_list = NULL;

	/* if the given value is already contained, don't do anything */
	if ((setstr != NULL) && (g_strstr_len (setstr, -1, value) != NULL))
		return NULL;

	if (setstr == NULL)
		return g_strdup (value);
	else
		return g_strconcat (setstr, value, NULL);
}

/**
 * as_cache_insert:
 * @cache: An instance of #AsCache.
 * @cpt: The #AsComponent to insert.
 * @error: A #GError or %NULL.
 *
 * Insert a new component into the cache.
 *
 * Returns: %TRUE if there was no error.
 **/
gboolean
as_cache_insert (AsCache *cache, AsComponent *cpt, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn = NULL;
	g_autofree gchar *xml_data = NULL;
	g_autofree gchar *cpt_checksum = NULL;
	g_autofree gchar *cpt_checksum_nl = NULL;
	GError *tmp_error = NULL;

	GHashTable *token_cache;
	GHashTableIter tc_iter;
	gpointer tc_key, tc_value;

	GPtrArray *categories;
	GPtrArray *launchables;
	GPtrArray *provides;

	if (!as_cache_check_opened (cache, TRUE, error))
		return FALSE;

	if (priv->floating) {
		/* floating cache, don't really add this component yet but stage it in the internal map */
		g_hash_table_insert (priv->cpt_map,
				     g_strdup (as_component_get_data_id (cpt)),
				     g_object_ref (cpt));
		g_hash_table_add (priv->cid_set,
				  g_strdup (as_component_get_id (cpt)));
		return TRUE;
	}

	txn = as_cache_transaction_new (cache, 0, error);
	if (txn == NULL)
		return FALSE;

	/* add the component data itself to the cache */
	xml_data = as_cache_component_to_xml (cache, cpt);
	cpt_checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5,
						      as_component_get_data_id (cpt),
						      -1);
	cpt_checksum_nl = g_strconcat (cpt_checksum, "\n", NULL);

	if (!as_cache_txn_put_kv_str (cache,
				  txn,
				  priv->db_cpts,
				  cpt_checksum,
				  xml_data,
				  error)) {
		goto fail;
	}

	/* populate full-text search index */
	as_component_create_token_cache (cpt);
	token_cache = as_component_get_token_cache_table (cpt);

	g_hash_table_iter_init (&tc_iter, token_cache);
	while (g_hash_table_iter_next (&tc_iter, &tc_key, &tc_value)) {
		g_autoptr(GPtrArray) entries = NULL;
		g_autoptr(GVariant) nvar = NULL;
		g_autofree gpointer entry_data = NULL;
		const gchar *token_str;
		AsTokenType *match_pval;
		MDB_val dval;

		/* we ignore tokens which are too long to be database keys */
		token_str = (const gchar*) tc_key;
		if ((sizeof(gchar) * strlen (token_str)) > priv->max_keysize) {
			g_warning ("Ignored search token '%s': Too long to be stored in the cache.", token_str);
			continue;
		}

		match_pval = (AsTokenType *) tc_value;
		dval = as_cache_txn_get_value (cache, txn, priv->db_fts, token_str, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto fail;
		}

		entries = g_ptr_array_new_with_free_func ((GDestroyNotify) g_variant_unref);
		if (dval.mv_size > 0) {
			g_autoptr(GVariant) var = NULL;
			GVariantIter iter;
			GVariant *v_entry;

			/* GVariant doesn't like the mmap, so we need to copy the data here. This should be fixed in GLib >= 2.60 */
			entry_data = g_memdup (dval.mv_data, dval.mv_size);
			var = g_variant_new_from_data (G_VARIANT_TYPE ("a{sq}"),
							entry_data,
							dval.mv_size,
							TRUE, /* trusted */
							NULL,
							NULL);

			g_variant_iter_init (&iter, var);
			while ((v_entry = g_variant_iter_next_value (&iter)))
				/* the array takes ownership of the value here */
				g_ptr_array_add (entries, v_entry);
		}

		/* create new value and have the array take ownership */
		g_ptr_array_add (entries,
				 g_variant_ref_sink (g_variant_new_dict_entry (g_variant_new_string (cpt_checksum),
										g_variant_new_uint16 (*match_pval))));

		nvar = g_variant_new_array (G_VARIANT_TYPE ("{sq}"),
					    (GVariant *const *)entries->pdata,
					    entries->len);

		MDB_val ndval;
		ndval.mv_size = g_variant_get_size (nvar);
		ndval.mv_data = (void*) g_variant_get_data (nvar);

		if (!as_cache_txn_put_kv (cache,
					  txn,
					  priv->db_fts,
					  token_str,
					  ndval,
					  error)) {
			goto fail;
		}
	}

	/* register this component with the CID mapping */
	{
		g_autofree gchar *existing_db_cids = NULL;
		g_autofree gchar *new_list = NULL;
		existing_db_cids = lmdb_val_strdup (as_cache_txn_get_value (cache,
									txn,
									priv->db_cids,
									as_component_get_id (cpt),
									&tmp_error));
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto fail;
		}

		new_list = as_str_check_append (existing_db_cids, cpt_checksum_nl);
		if (new_list != NULL) {
			/* our checksum was not in the list yet, so add it */
			if (!as_cache_txn_put_kv_str (cache,
							txn,
							priv->db_cids,
							as_component_get_id (cpt),
							new_list,
							error)) {
				goto fail;
			}
		}
	}

	/* add category mapping */
	categories = as_component_get_categories (cpt);
	for (guint i = 0; i < categories->len; i++) {
		g_autofree gchar *ecpt_str = NULL;
		g_autofree gchar *new_list = NULL;
		const gchar *category = (const gchar*) g_ptr_array_index (categories, i);

		ecpt_str = lmdb_val_strdup (as_cache_txn_get_value (cache,
								    txn,
								    priv->db_cats,
								    category,
								    &tmp_error));
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto fail;
		}

		new_list = as_str_check_append (ecpt_str, cpt_checksum_nl);
		if (new_list != NULL) {
			/* this component was not yet registered with the category */
			if (!as_cache_txn_put_kv_str (cache,
							txn,
							priv->db_cats,
							category,
							new_list,
							error)) {
				goto fail;
			}
		}
	}

	/* add launchables mapping */
	launchables = as_component_get_launchables (cpt);
	for (guint i = 0; i < launchables->len; i++) {
		GPtrArray *entries;
		AsLaunchable *launchable = AS_LAUNCHABLE (g_ptr_array_index (launchables, i));

		entries = as_launchable_get_entries (launchable);
		for (guint j = 0; j < entries->len; j++) {
			g_autofree gchar *entry_key = NULL;
			g_autofree gchar *ecpt_str = NULL;
			g_autofree gchar *new_list = NULL;
			const gchar *entry = (const gchar*) g_ptr_array_index (entries, j);

			entry_key = g_strconcat (as_launchable_kind_to_string (as_launchable_get_kind (launchable)), entry, NULL);
			ecpt_str = lmdb_val_strdup (as_cache_txn_get_value (cache,
									txn,
									priv->db_launchables,
									entry_key,
									&tmp_error));
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto fail;
			}

			new_list = as_str_check_append (ecpt_str, cpt_checksum_nl);
			if (new_list != NULL) {
				/* add component checksum to launchable list */
				if (!as_cache_txn_put_kv_str (cache,
								txn,
								priv->db_launchables,
								entry_key,
								new_list,
								error)) {
					goto fail;
				}
			}
		}
	}

	/* add provides mapping */
	provides = as_component_get_provided (cpt);
	for (guint i = 0; i < provides->len; i++) {
		GPtrArray *items;
		AsProvided *prov = AS_PROVIDED (g_ptr_array_index (provides, i));

		items = as_provided_get_items (prov);
		for (guint j = 0; j < items->len; j++) {
			g_autofree gchar *item_key = NULL;
			g_autofree gchar *ecpt_str = NULL;
			g_autofree gchar *new_list = NULL;
			const gchar *item = (const gchar*) g_ptr_array_index (items, j);

			item_key = g_strconcat (as_provided_kind_to_string (as_provided_get_kind (prov)), item, NULL);
			ecpt_str = lmdb_val_strdup (as_cache_txn_get_value (cache,
									txn,
									priv->db_provides,
									item_key,
									&tmp_error));
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto fail;
			}

			new_list = as_str_check_append (ecpt_str, cpt_checksum_nl);
			if (new_list != NULL) {
				/* add component checksum to launchable list */
				if (!as_cache_txn_put_kv_str (cache,
								txn,
								priv->db_provides,
								item_key,
								new_list,
								error)) {
					goto fail;
				}
			}
		}
	}

	/* add kinds mapping */
	{
		const gchar *kind_str = as_component_kind_to_string (as_component_get_kind (cpt));
		g_autofree gchar *ecpt_list_str = NULL;
		g_autofree gchar *new_list = NULL;

		ecpt_list_str = lmdb_val_strdup (as_cache_txn_get_value (cache,
									 txn,
									 priv->db_kinds,
									 kind_str,
									 &tmp_error));
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto fail;
		}

		new_list = as_str_check_append (ecpt_list_str, cpt_checksum_nl);
		if (new_list != NULL) {
			/* add component checksum to component-kind mapping */
			if (!as_cache_txn_put_kv_str (cache,
							txn,
							priv->db_kinds,
							kind_str,
							new_list,
							error)) {
				goto fail;
			}
		}
	}

	return lmdb_transaction_commit (txn, error);
fail:
	lmdb_transaction_abort (txn);
	return FALSE;
}

/**
 * as_cache_remove_by_data_id:
 * @cache: An instance of #AsCache.
 * @cdid: Component data ID.
 * @error: A #GError or %NULL.
 *
 * Remove a component from the cache.
 *
 * Returns: %TRUE if component was removed.
 **/
gboolean
as_cache_remove_by_data_id (AsCache *cache, const gchar *cdid, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn = NULL;
	g_autofree gchar *cpt_checksum = NULL;
	GError *tmp_error = NULL;
	gboolean ret;

	if (!as_cache_check_opened (cache, TRUE, error))
		return FALSE;

	if (priv->floating) {
		/* floating cache, remove only from the internal map */
		return g_hash_table_remove (priv->cpt_map, cdid);
	}

	txn = as_cache_transaction_new (cache, 0, error);
	if (txn == NULL)
		return FALSE;

	/* remove component itself from the cache */
	cpt_checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5,
						      cdid,
						      -1);

	ret = lmdb_txn_delete (txn, priv->db_cpts, cpt_checksum, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	if (ret) {
		/* TODO: We could remove all references to the removed component from the cache here.
		 * Currently those are not an issue though, so we may keep them (as removing references is expensive) */
	}

	lmdb_transaction_commit (txn, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto fail;
	}

	return ret;
fail:
	lmdb_transaction_abort (txn);
	return FALSE;
}

/**
 * as_cache_component_from_dval:
 *
 * Get component from database entry.
 */
static inline AsComponent*
as_cache_component_from_dval (AsCache *cache, MDB_val dval, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(AsComponent) cpt = NULL;
	xmlDoc *doc;
	xmlNode *root;

	if (dval.mv_size <= 0)
		return NULL;

	doc = as_xml_parse_document (dval.mv_data, dval.mv_size, error);
	if (doc == NULL)
		return NULL;
	root = xmlDocGetRootElement (doc);

	cpt = as_component_new ();
	if (!as_component_load_from_xml (cpt, priv->context, root, error)) {
		xmlFreeDoc (doc);
		return NULL;
	}

	xmlFreeDoc (doc);
	return g_steal_pointer (&cpt);
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
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	MDB_cursor *cur;
	gint rc;
	MDB_val dval;
	g_autoptr(GPtrArray) results = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	results = g_ptr_array_new_with_free_func (g_object_unref);

	rc = mdb_cursor_open (txn, priv->db_cpts, &cur);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
				AS_CACHE_ERROR,
				AS_CACHE_ERROR_FAILED,
				"Unable to iterate cache (no cursor): %s", mdb_strerror (rc));
		lmdb_transaction_abort (txn);
		return NULL;
	}

	rc = mdb_cursor_get (cur, NULL, &dval, MDB_SET_RANGE);
	while (rc == 0) {
		AsComponent *cpt;

		if (dval.mv_size <= 0)
			continue;

		cpt = as_cache_component_from_dval (cache, dval, error);
		if (cpt == NULL)
			return NULL; /* error */
		g_ptr_array_add (results, cpt);

		rc = mdb_cursor_get(cur, NULL, &dval, MDB_NEXT);
	}
	mdb_cursor_close (cur);

	lmdb_transaction_commit (txn, NULL);
	return g_steal_pointer (&results);
}

/**
 * as_cache_component_by_hash:
 *
 * Retrieve a component using its internal hash.
 */
static AsComponent*
as_cache_component_by_hash (AsCache *cache, MDB_txn *txn, const gchar *cpt_hash, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_val dval;
	g_autoptr(AsComponent) cpt = NULL;
	GError *tmp_error = NULL;

	dval = as_cache_txn_get_value (cache, txn, priv->db_cpts, cpt_hash, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}
	if (dval.mv_size <= 0)
		return NULL;

	cpt = as_cache_component_from_dval (cache, dval, error);
	if (cpt == NULL)
		return NULL; /* error */

	return g_steal_pointer (&cpt);
}

/**
 * as_cache_components_by_hash_list:
 *
 * Retrieve components using an internal hash list.
 */
static GPtrArray*
as_cache_components_by_hash_list (AsCache *cache, MDB_txn *txn, const gchar *hash_list_str, GError **error)
{
	g_autoptr(GPtrArray) result = NULL;
	g_auto(GStrv) strv = NULL;
	GError *tmp_error = NULL;

	result = g_ptr_array_new_with_free_func (g_object_unref);
	if (hash_list_str == NULL)
		goto out;

	strv = g_strsplit (hash_list_str, "\n", -1);
	for (guint i = 0; strv[i] != NULL; i++) {
		AsComponent *cpt;
		if (strv[i][0] == '\0')
			continue;

		cpt = as_cache_component_by_hash (cache, txn, strv[i], &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_prefixed_error (error, tmp_error, "Failed to retrieve component data: ");
			return NULL;
		}
		if (cpt != NULL)
			g_ptr_array_add (result, cpt);
	}

out:
	return g_steal_pointer (&result);
}

/**
 * as_cache_get_component_by_cid:
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
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	GError *tmp_error = NULL;
	g_autofree gchar *data = NULL;
	GPtrArray *result = NULL;

	if (!as_cache_check_opened (cache, TRUE, error))
		return NULL;

	if (priv->floating) {
		/* floating cache, check only the internal map */

		GHashTableIter iter;
		gpointer value;

		result = g_ptr_array_new_with_free_func (g_object_unref);
		if (id == NULL)
			return result;

		g_hash_table_iter_init (&iter, priv->cpt_map);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			AsComponent *cpt = AS_COMPONENT (value);
			if (g_strcmp0 (as_component_get_id (cpt), id) == 0)
				g_ptr_array_add (result, g_object_ref (cpt));
		}
		return result;
	}

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	data = lmdb_val_strdup (as_cache_txn_get_value (cache, txn, priv->db_cids, id, &tmp_error));
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

	result = as_cache_components_by_hash_list (cache, txn, data, error);
	if (result == NULL) {
		lmdb_transaction_abort (txn);
		return NULL;
	} else {
		lmdb_transaction_commit (txn, NULL);
		return result;
	}
}

/**
 * as_cache_get_component_by_data_id:
 * @cache: An instance of #AsCache.
 * @cdid: The component data ID.
 * @error: A #GError or %NULL.
 *
 * Retrieve component with the given data ID.
 *
 * Returns: (transfer full): An #AsComponent
 */
AsComponent*
as_cache_get_component_by_data_id (AsCache *cache, const gchar *cdid, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	MDB_val dval;
	GError *tmp_error = NULL;
	AsComponent *cpt;

	if (!as_cache_check_opened (cache, TRUE, error))
		return NULL;

	if (priv->floating) {
		/* floating cache, check only the internal map */
		cpt = g_hash_table_lookup (priv->cpt_map, cdid);
		return cpt? g_object_ref (cpt) : NULL;
	}

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	dval = as_cache_txn_get_value (cache, txn, priv->db_cpts, cdid, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}
	cpt = as_cache_component_from_dval (cache, dval, error);
	if (cpt == NULL)
		return NULL; /* error */

	lmdb_transaction_commit (txn, NULL);
	return cpt;
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
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	GError *tmp_error = NULL;
	g_autofree gchar *data = NULL;
	GPtrArray *result = NULL;
	const gchar *kind_str = as_component_kind_to_string (kind);

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	data = lmdb_val_strdup (as_cache_txn_get_value (cache, txn, priv->db_kinds, kind_str, &tmp_error));
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

	result = as_cache_components_by_hash_list (cache, txn, data, error);
	if (result == NULL) {
		lmdb_transaction_abort (txn);
		return NULL;
	} else {
		lmdb_transaction_commit (txn, NULL);
		return result;
	}
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
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	GError *tmp_error = NULL;
	g_autofree gchar *data = NULL;
	g_autofree gchar *item_key = NULL;
	GPtrArray *result = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;

	item_key = g_strconcat (as_provided_kind_to_string (kind), item, NULL);
	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	data = lmdb_val_strdup (as_cache_txn_get_value (cache, txn, priv->db_provides, item_key, &tmp_error));
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

	result = as_cache_components_by_hash_list (cache, txn, data, error);
	if (result == NULL) {
		lmdb_transaction_abort (txn);
		return NULL;
	} else {
		lmdb_transaction_commit (txn, NULL);
		return result;
	}
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
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	GError *tmp_error = NULL;
	g_autoptr(GPtrArray) result = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	result = g_ptr_array_new_with_free_func (g_object_unref);
	for (guint i = 0; categories[i] != NULL; i++) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		g_autofree gchar *data = NULL;

		data = lmdb_val_strdup (as_cache_txn_get_value (cache, txn, priv->db_cats, categories[i], &tmp_error));
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			lmdb_transaction_abort (txn);
			return NULL;
		}
		if (data == NULL)
			continue;

		tmp_res = as_cache_components_by_hash_list (cache, txn, data, error);
		if (tmp_res == NULL) {
			lmdb_transaction_abort (txn);
			return NULL;
		}
		for (guint j = 0; j < tmp_res->len; j++)
			g_ptr_array_add (result, g_ptr_array_steal_index_fast (tmp_res, 0));
	}

	if (result == NULL) {
		lmdb_transaction_abort (txn);
		return NULL;
	} else {
		lmdb_transaction_commit (txn, NULL);
		return g_steal_pointer (&result);
	}
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
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	GError *tmp_error = NULL;
	g_autofree gchar *data = NULL;
	g_autofree gchar *entry_key = NULL;
	GPtrArray *result = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;

	entry_key = g_strconcat (as_launchable_kind_to_string (kind), id, NULL);
	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	data = lmdb_val_strdup (as_cache_txn_get_value (cache, txn, priv->db_provides, entry_key, &tmp_error));
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

	result = as_cache_components_by_hash_list (cache, txn, data, error);
	if (result == NULL) {
		lmdb_transaction_abort (txn);
		return NULL;
	} else {
		lmdb_transaction_commit (txn, NULL);
		return result;
	}
}

/**
 * as_cache_update_results_with_fts_variant:
 *
 * Update results table using the full-text search scoring data from a GVariant dict
 */
static gboolean
as_cache_update_results_with_fts_variant (AsCache *cache, MDB_txn *txn, MDB_val var_dbval, GHashTable *results_ht, gboolean exact_match, GError **error)
{
	GError *tmp_error = NULL;
	g_autofree void *entry_data = NULL;
	g_autoptr(GVariant) var = NULL;
	GVariantIter iter;
	GVariant *v_entry;

	/* GVariant doesn't like the mmap alignment, so we need to copy the data here. GLib >= 2.60 resolves this */
	entry_data = g_memdup (var_dbval.mv_data, var_dbval.mv_size);
	var = g_variant_new_from_data (G_VARIANT_TYPE_VARDICT,
					entry_data,
					var_dbval.mv_size,
					TRUE, /* trusted */
					NULL,
					NULL);

	g_variant_iter_init (&iter, var);
	while ((v_entry = g_variant_iter_next_value (&iter))) {
		AsTokenType match_pval;
		guint sort_score;
		g_autofree gchar *cpt_hash = NULL;
		AsComponent *cpt;

		/* unpack variant */
		g_variant_get (v_entry, "{sq}", &cpt_hash, &match_pval);

		/* retrieve component woth this hash */
		cpt = g_hash_table_lookup (results_ht, cpt_hash);
		if (cpt == NULL) {
			cpt = as_cache_component_by_hash (cache, txn, cpt_hash, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_prefixed_error (error, tmp_error, "Failed to retrieve component data: ");
				return FALSE;
			}
			if (cpt != NULL) {
				sort_score = 0;
				if (exact_match)
					sort_score |= match_pval << 2;
				else
					sort_score |= match_pval;
				as_component_set_sort_score (cpt, sort_score);

				g_hash_table_insert (results_ht,
						     g_strdup (cpt_hash),
						     cpt);
			}
		} else {
			sort_score = as_component_get_sort_score (cpt);
			if (exact_match)
				sort_score |= match_pval << 2;
			else
				sort_score |= match_pval;
			as_component_set_sort_score (cpt, sort_score);
		}

		g_variant_unref (v_entry);
	}

	return TRUE;
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
 * as_cache_search:
 * @cache: An instance of #AsCache.
 * @terms: List of search terms
 * @sort: %TRUE if results should be sorted by score.
 * @error: A #GError or %NULL.
 *
 * Perform a search for the given terms.
 *
 * Returns: (transfer full): An array of #AsComponent
 */
GPtrArray*
as_cache_search (AsCache *cache, gchar **terms, gboolean sort, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	GError *tmp_error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GHashTable) results_ht = NULL;
	GHashTableIter ht_iter;
	gpointer ht_value;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	results = g_ptr_array_new_with_free_func (g_object_unref);
	results_ht = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    g_free,
					    (GDestroyNotify) g_object_unref);

	/* search by using exact matches first */
	for (guint i = 0; terms[i] != NULL; i++) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		MDB_val dval;

		dval = as_cache_txn_get_value (cache, txn, priv->db_fts, terms[i], &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			lmdb_transaction_abort (txn);
			return NULL;
		}
		if (dval.mv_size <= 0)
			continue;

		if (!as_cache_update_results_with_fts_variant (cache, txn, dval, results_ht, TRUE, error)) {
			lmdb_transaction_abort (txn);
			return NULL;
		}
	}

	/* if we got no results by exact matches, try partial matches (which is slower) */
	if (g_hash_table_size (results_ht) <= 0) {
		MDB_cursor *cur;
		gint rc;
		MDB_val dkey;
		MDB_val dval;

		rc = mdb_cursor_open (txn, priv->db_fts, &cur);
		if (rc != MDB_SUCCESS) {
			g_set_error (error,
				     AS_CACHE_ERROR,
				     AS_CACHE_ERROR_FAILED,
				     "Unable to iterate cache (no cursor): %s", mdb_strerror (rc));
			lmdb_transaction_abort (txn);
			return NULL;
		}

		rc = mdb_cursor_get (cur, &dkey, &dval, MDB_SET_RANGE);
		while (rc == 0) {
			gboolean match = FALSE;

			for (guint i = 0; terms[i] != NULL; i++) {
				gsize term_len = strlen (terms[i]);
				/* if term length is bigger than the key, it will never match.
				 * if it is equal, we already matches it and shouldn't be here */
				if (term_len >= dkey.mv_size)
					continue;
				if (strncmp (dkey.mv_data, terms[i], term_len) == 0) {
					/* prefix match was successful */
					match = TRUE;
					break;
				}
			}
			if (!match) {
				rc = mdb_cursor_get(cur, &dkey, &dval, MDB_NEXT);
				continue;
			}

			/* we got a prefix match, so add the components to our search result */
			if (dval.mv_size <= 0)
				continue;
			if (!as_cache_update_results_with_fts_variant (cache, txn, dval, results_ht, FALSE, error)) {
				mdb_cursor_close (cur);
				lmdb_transaction_abort (txn);
				return NULL;
			}

			rc = mdb_cursor_get(cur, &dkey, &dval, MDB_NEXT);
		}
		mdb_cursor_close (cur);
	}

	/* compile our result */
	g_hash_table_iter_init (&ht_iter, results_ht);
	while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value))
		g_ptr_array_add (results, g_object_ref (AS_COMPONENT (ht_value)));

	/* sort the results by their priority */
	if (sort)
		g_ptr_array_sort (results, as_sort_components_by_score_cb);

	if (results == NULL) {
		lmdb_transaction_abort (txn);
		return NULL;
	} else {
		lmdb_transaction_commit (txn, NULL);
		return g_steal_pointer (&results);
	}
}

/**
 * as_cache_has_component_id:
 * @cache: An instance of #AsCache.
 * @id: The component ID to search for.
 * @error: A #GError or %NULL.
 *
 * Check if there is any component(s) with the given ID in the cache.
 *
 * Returns: %TRUE if the ID is known.
 */
gboolean
as_cache_has_component_id (AsCache *cache, const gchar *id, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	GError *tmp_error = NULL;
	MDB_val dval;
	gboolean found;

	if (!as_cache_check_opened (cache, TRUE, error))
		return FALSE;

	if (priv->floating) {
		/* floating cache, check only the internal map */
		return g_hash_table_contains (priv->cid_set, id);
	}

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return FALSE;

	dval = as_cache_txn_get_value (cache, txn, priv->db_cids, id, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return FALSE;
	}

	found = dval.mv_size > 0;

	lmdb_transaction_commit (txn, NULL);
	return found;
}

/**
 * as_cache_count_components:
 * @cache: An instance of #AsCache.
 * @error: A #GError or %NULL.
 *
 * Get the amount of components the cache holds.
 *
 * Returns: Components count in the database.
 */
gsize
as_cache_count_components (AsCache *cache, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	MDB_stat stats;
	gint rc;
	gsize count;

	if (!as_cache_check_opened (cache, FALSE, error))
		return 0;

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return 0;

	rc = mdb_stat(txn, priv->db_cpts, &stats);
	if (rc == MDB_SUCCESS) {
		count = stats.ms_entries;
	} else {
		g_set_error (error,
				AS_CACHE_ERROR,
				AS_CACHE_ERROR_FAILED,
				"Unable to retrieve cache statistics: %s", mdb_strerror (rc));
	}

	lmdb_transaction_commit (txn, NULL);
	return count;
}



/**
 * as_cache_get_ctime:
 * @cache: An instance of #AsCache.
 *
 * Returns: ctime of the cache database.
 */
time_t
as_cache_get_ctime (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	struct stat cache_sbuf;

	if (priv->fname == NULL)
		return 0;

	if (stat (priv->fname, &cache_sbuf) < 0)
		return 0;
	else
		return cache_sbuf.st_ctime;
}

/**
 * as_cache_get_ctime:
 * @cache: An instance of #AsCache.
 *
 * Returns: %TRUE if the cache is open.
 */
gboolean
as_cache_is_open (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	return priv->opened;
}

/**
 * as_cache_set_floating:
 * @cache: An instance of #AsCache.
 *
 * Make cache "floating": Only some actions are permitted and nothing
 * is written to disk until the floating state is changed.
 */
gboolean
as_cache_set_floating (AsCache *cache, gboolean floating, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	GHashTableIter iter;
	gpointer ht_value;

	if (!priv->floating && !floating)
		return TRUE;
	if (priv->floating && floating)
		return TRUE;
	if (!priv->floating && floating) {
		priv->floating = floating;
		g_hash_table_remove_all (priv->cpt_map);
		g_hash_table_remove_all (priv->cid_set);
		g_debug ("Cache set to floating mode.");
		return TRUE;
	}
	g_assert (priv->floating && !floating);

	/* if we are here, we switch from a floating cache to a non-floating one,
	 * so we need to persist all changes */

	priv->floating = floating;

	g_hash_table_iter_init (&iter, priv->cpt_map);
	while (g_hash_table_iter_next (&iter, NULL, &ht_value)) {
		if (!as_cache_insert (cache, AS_COMPONENT (ht_value), error))
			return FALSE;
	}

	g_hash_table_remove_all (priv->cid_set);
	g_hash_table_remove_all (priv->cpt_map);

	g_debug ("Cache returned from floating mode (all changes are now persistent)");

	return TRUE;
}

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
