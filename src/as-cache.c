/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018-2021 Matthias Klumpp <matthias@tenstral.net>
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
 * This class is threadsafe.
 *
 * See also: #AsPool
 */

#include "config.h"
#include "as-cache.h"

#include <lmdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gstdio.h>

#include "as-utils-private.h"
#include "as-context-private.h"
#include "as-component-private.h"
#include "as-launchable.h"

/*
 * The maximum size of the cache file (128MiB on x86,
 * 256MiB everywhere else).
 * The file is mmap(2)'d into memory.
 */
#ifdef __i386__
static const size_t LMDB_DB_SIZE = 1024 * 1024 * 128;
#else
static const size_t LMDB_DB_SIZE = 1024 * 1024 * 256;
#endif

/* format version of the currently supported cache */
static const gchar *CACHE_FORMAT_VERSION = "1";

/* checksum algorithm used in the cache */
#define AS_CACHE_CHECKSUM G_CHECKSUM_MD5

/* digest length of AS_CACHE_CHECKSUM */
static const gsize AS_CACHE_CHECKSUM_LEN = 16;

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
	MDB_dbi db_addons;

	gchar *fname;
	gchar *volatile_db_fname;
	gsize max_keysize;
	gboolean opened;
	gboolean nosync;
	gboolean readonly;

	AsContext *context;
	gchar *locale;

	gboolean floating;
	GHashTable *cpt_map;
	GHashTable *cid_set;
	GHashTable *ro_removed_set;

	GFunc cpt_refine_func;
	gpointer cpt_refine_func_udata;

	GMutex mutex;
} AsCachePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsCache, as_cache, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_cache_get_instance_private (o))

static gboolean as_cache_register_addons_for_component (AsCache *cache,
							MDB_txn *txn,
							AsComponent *cpt,
							GError **error);

/**
 * as_cache_checksum_hash:
 *
 * Hash cache checksums for use in a hash table.
 */
guint
as_cache_checksum_hash (gconstpointer v)
{
	guint32 h = 5381;
	const guint8 *p = v;

	for (gsize i = 0; i < AS_CACHE_CHECKSUM_LEN; i++)
		h = (h << 5) + h + p[i];

	return h;
}

/**
 * as_cache_checksum_equal:
 *
 * Equality check for two cache checksums.
 */
gboolean
as_cache_checksum_equal (gconstpointer v1, gconstpointer v2)
{
	return memcmp (v1, v2, AS_CACHE_CHECKSUM_LEN) == 0;
}

/**
 * as_cache_init:
 **/
static void
as_cache_init (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);

	g_mutex_init (&priv->mutex);

	priv->opened = FALSE;
	priv->max_keysize = 511;
	priv->cpt_refine_func = NULL;
	priv->locale = as_get_current_locale ();

	priv->context = as_context_new ();
	as_context_set_locale (priv->context, priv->locale);
	as_context_set_style (priv->context, AS_FORMAT_STYLE_COLLECTION);
	as_context_set_internal_mode (priv->context, TRUE); /* we need to write some special tags only used in the cache */

	priv->floating = FALSE;
	priv->readonly = FALSE;

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

	/* set of removed component hashes in a read-only cache */
	priv->ro_removed_set = g_hash_table_new_full (as_cache_checksum_hash,
						      as_cache_checksum_equal,
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

	as_cache_close (cache);
	g_mutex_lock (&priv->mutex);
	g_object_unref (priv->context);
	g_free (priv->locale);
	g_free (priv->fname);

	g_hash_table_unref (priv->cpt_map);
	g_hash_table_unref (priv->cid_set);
	g_hash_table_unref (priv->ro_removed_set);

	g_mutex_unlock (&priv->mutex);
	g_mutex_clear (&priv->mutex);

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
 * as_generate_cache_checksum:
 *
 * Create checksum digest used internally in the database.
 */
inline static void
as_generate_cache_checksum (const gchar *value, gssize value_len, guint8 **result, gsize *result_len)
{
	gsize rlen = AS_CACHE_CHECKSUM_LEN; /* digest length (16 for MD5) */
	g_autoptr(GChecksum) cs = g_checksum_new (AS_CACHE_CHECKSUM);

	*result = g_malloc (sizeof(guint8) * rlen);
	g_checksum_update (cs, (const guchar *) value, value_len);
	g_checksum_get_digest (cs, *result, &rlen);

	if (result_len != NULL)
		*result_len = rlen;
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
 * lmdb_val_memdup:
 *
 * Duplicate memory chunk from database value.
 */
inline static void*
lmdb_val_memdup (MDB_val val, gsize *len)
{
	*len = val.mv_size;
	if (val.mv_size == 0)
		return NULL;
#if GLIB_CHECK_VERSION(2,68,0)
	return g_memdup2 (val.mv_data, val.mv_size);
#else
	return g_memdup (val.mv_data, val.mv_size);
#endif
}

/**
 * as_cache_txn_put_kv:
 *
 * Add key/value pair to the database
 */
static gboolean
as_cache_txn_put_kv (AsCache *cache, MDB_txn *txn, MDB_dbi dbi, const gchar *key, MDB_val dval, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_val dkey;
	gint rc;
	gsize key_len;
	g_autofree gchar *key_hash = NULL;

	/* if key is too short, return error */
	g_assert (key != NULL);
	key_len = sizeof(gchar) * strlen (key);
	if (key_len == 0) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_BAD_DATA,
			     "Can not add an empty (zero-length) key to the cache");
		return FALSE;
	}

	/* if key is too long, hash it */
	if (key_len > priv->max_keysize) {
		key_hash = g_compute_checksum_for_string (AS_CACHE_CHECKSUM, key, key_len);
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
static gboolean
as_cache_txn_put_kv_str (AsCache *cache, MDB_txn *txn, MDB_dbi dbi, const gchar *key, const gchar *value, GError **error)
{
	MDB_val dvalue;
        dvalue = lmdb_str_to_dbval (value, -1);

	return as_cache_txn_put_kv (cache, txn, dbi, key, dvalue, error);
}

/**
 * as_cache_txn_put_kv_bytes:
 *
 * Add string key / byte value pair to the database
 */
static gboolean
as_cache_txn_put_kv_bytes (AsCache *cache, MDB_txn *txn, MDB_dbi dbi, const gchar *key,
			   const guint8 *value, gsize value_len, GError **error)
{
	MDB_val dval;
	dval.mv_data = (guint8*) value;
	dval.mv_size = value_len;

	return as_cache_txn_put_kv (cache, txn, dbi, key, dval, error);
}

/**
 * as_txn_put_hash_kv_str:
 *
 * Add checksum key / string value pair to the database
 */
static gboolean
as_txn_put_hash_kv_str (MDB_txn *txn, MDB_dbi dbi, const guint8 *hash, const gchar *value, GError **error)
{
	MDB_val dkey;
	MDB_val dval;
	gint rc;

	dkey.mv_size = AS_CACHE_CHECKSUM_LEN;
	dkey.mv_data = (guint8*) hash;
	dval = lmdb_str_to_dbval (value, -1);

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
 * lmdb_txn_delete_by_hash:
 *
 * Remove key from the database.
 */
inline static gboolean
lmdb_txn_delete_by_hash (MDB_txn *txn, MDB_dbi dbi, const guint8 *key, GError **error)
{
	MDB_val dkey;
	gint rc;
	dkey.mv_size = AS_CACHE_CHECKSUM_LEN;
	dkey.mv_data = (void*) key;

	rc = mdb_del (txn, dbi, &dkey, NULL);
	if ((rc != MDB_NOTFOUND) && (rc != MDB_SUCCESS)) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to remove data by hash key: %s", mdb_strerror (rc));
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
static MDB_val
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
		key_hash = g_compute_checksum_for_string (AS_CACHE_CHECKSUM, key, key_len);
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
 * as_txn_get_value_by_hash:
 */
static MDB_val
as_txn_get_value_by_hash (AsCache *cache, MDB_txn *txn, MDB_dbi dbi, const guint8 *hash, GError **error)
{
	MDB_cursor *cur;
	MDB_val dkey;
	MDB_val dval = {0, NULL};
	gint rc;

	if (hash == NULL)
		return dval;

	dkey.mv_size = AS_CACHE_CHECKSUM_LEN;
	dkey.mv_data = (void*) hash;

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
			     "Unable to read data (using hash key): %s", mdb_strerror (rc));
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
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

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
	sctx = xmlSaveToBuffer (buf, "utf-8", XML_SAVE_NO_DECL);
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
	MDB_txn *txn = NULL;
	MDB_dbi db_config;
	g_autofree gchar *cache_format = NULL;
	unsigned int db_flags;
	gboolean nosync;
	gboolean readonly;
	g_autoptr(GMutexLocker) locker = NULL;
	int db_fd;
	g_autofree gchar *volatile_fname = NULL;

	/* close cache in case it was open */
	as_cache_close (cache);

	locker = g_mutex_locker_new (&priv->mutex);

	rc = mdb_env_create (&priv->db_env);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to initialize an MDB environment: %s", mdb_strerror (rc));
		goto fail;
	}

	/* we currently have 9 DBs */
	rc = mdb_env_set_maxdbs (priv->db_env, 9);
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

	/* determine database mode */
	db_flags = MDB_NOSUBDIR | MDB_NOMETASYNC | MDB_NOLOCK;
	nosync = priv->nosync;
	readonly = priv->readonly;

	/* determine where to put a volatile database */
	if (g_strcmp0 (fname, ":temporary") == 0) {
		g_autofree gchar *volatile_dir = as_get_user_cache_dir ();
		volatile_fname = g_build_filename (volatile_dir, "appcache-XXXXXX.mdb", NULL);

		if (g_mkdir_with_parents (volatile_dir, 0755) != 0) {
			g_set_error (error,
				     AS_CACHE_ERROR,
				     AS_CACHE_ERROR_FAILED,
				     "Unable to create cache directory (%s)",volatile_dir);
			goto fail;
		}

		/* we can skip syncs in temporary mode - in case of a crash, the data is lost anyway */
		nosync = TRUE;

		/* readonly mode makes no sense if we have a temporary cache */
		readonly = FALSE;

	} else if (g_strcmp0 (fname, ":memory") == 0) {
		volatile_fname = g_build_filename (g_get_user_runtime_dir (), "appstream-cache-XXXXXX.mdb", NULL);
		/* we can skip syncs in in-memory mode - they don't mean anything anyway*/
		nosync = TRUE;

		/* readonly mode makes no sense if we have an in-memory cache */
		readonly = FALSE;
	}

	if (readonly)
		db_flags |= MDB_RDONLY;
	if (nosync)
		db_flags |= MDB_NOSYNC;

	if (volatile_fname != NULL) {
		gint fd;

		g_free (priv->fname);
		priv->fname = g_steal_pointer (&volatile_fname);
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

		g_free (priv->volatile_db_fname);
		priv->volatile_db_fname = g_strdup (priv->fname);
	} else {
		g_free (priv->fname);
		priv->fname = g_strdup (fname);
	}

	if (readonly)
		g_debug ("Opening cache file for reading only: %s", priv->fname);
	else
		g_debug ("Opening cache file: %s", priv->fname);
	rc = mdb_env_open (priv->db_env,
			   priv->fname,
			   db_flags,
			   0755);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_FAILED,
			     "Unable to open cache: %s", mdb_strerror (rc));
		goto fail;
	}

	/* set FD_CLOEXEC manually. LMDB should do that, but it doesn't:
	   https://www.openldap.org/lists/openldap-bugs/201702/msg00003.html */
	if (mdb_env_get_fd (priv->db_env, &db_fd) == MDB_SUCCESS) {
		int db_fd_flags = fcntl (db_fd, F_GETFD);
		if (db_fd_flags != -1)
			fcntl (db_fd, F_SETFD, db_fd_flags | FD_CLOEXEC);
	}

	/* unlink the file, so it gets removed as soon as we don't need it anymore */
	if (priv->volatile_db_fname != NULL)
		g_unlink (priv->volatile_db_fname);

	/* retrieve the maximum keysize allowed for this database */
	priv->max_keysize = mdb_env_get_maxkeysize (priv->db_env);

	txn = as_cache_transaction_new (cache,
					readonly? MDB_RDONLY : 0,
					&tmp_error);
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

	/* addons */
	if (!as_cache_open_subdb (txn, "addons", &priv->db_addons, &tmp_error)) {
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
		if (readonly) {
			g_set_error (error,
					AS_CACHE_ERROR,
					AS_CACHE_ERROR_WRONG_FORMAT,
					"Cache format missing on read-only cache.");
			goto fail;
		} else {
			if (!as_cache_put_kv (cache, db_config, "format", CACHE_FORMAT_VERSION, error))
				goto fail;
		}
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
		/* the value was empty, we most likely have a new cache file - so set a locale for it if we can */
		if (readonly) {
			g_set_error (error,
					AS_CACHE_ERROR,
					AS_CACHE_ERROR_LOCALE_MISMATCH,
					"Locale value missing on read-only cache.");
			goto fail;
		} else {
			if (!as_cache_put_kv (cache, db_config, "locale", locale, error))
				goto fail;
		}
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
 * as_cache_open2:
 * @cache: An instance of #AsCache.
 * @locale: The locale with which the cache should be created, if a new cache was created.
 * @error: A #GError or %NULL.
 *
 * Open/create an AppStream cache file based on the previously set filename.
 * (This is equivalent to reopening the previous database - to create a new temporary cache
 * and discard the old one, use open())
 *
 * Returns: %TRUE if cache was opened successfully.
 **/
gboolean
as_cache_open2 (AsCache *cache, const gchar *locale, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autofree gchar *fname = NULL;

	if (priv->fname == NULL) {
		g_set_error (error,
			     AS_CACHE_ERROR,
			     AS_CACHE_ERROR_NO_FILENAME,
			     "No location was set for this cache.");
		return FALSE;
	}

	/* we need to duplicate this here, as open() frees priv->fname in case
	 * it contains a placeholder like :temporary */
	fname = g_strdup (priv->fname);

	return as_cache_open (cache, fname, locale, error);
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
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	if (!priv->opened)
		return FALSE;

	if (!priv->readonly)
		mdb_env_sync (priv->db_env, 1);
	mdb_env_close (priv->db_env);

	/* ensure any temporary file is gone, in case we used an in-memory database */
	if (priv->volatile_db_fname != NULL) {
		g_remove (priv->volatile_db_fname);
		g_free (priv->volatile_db_fname);
		priv->volatile_db_fname = NULL;
	}

	priv->opened = FALSE;
	return TRUE;
}

/**
 * as_cache_hash_set_append:
 *
 * Helper function for as_cache_insert()
 */
static gboolean
as_cache_hash_set_append (guint8 **list, gsize *list_len, const guint8 *new_hash)
{
	g_assert_cmpint (*list_len % AS_CACHE_CHECKSUM_LEN, ==, 0);

	/* check if the list already contains the hash that should be added */
	for (gsize i = 0; i < *list_len; i += AS_CACHE_CHECKSUM_LEN) {
		if (memcmp ((*list) + i, new_hash, AS_CACHE_CHECKSUM_LEN) == 0)
			return FALSE; /* hash was already in the set */
	}

	/* add new hash to the list */
	*list_len = *list_len + AS_CACHE_CHECKSUM_LEN;
	*list = g_realloc_n (*list, *list_len, sizeof(guint8));

	memcpy ((*list) + (*list_len) - AS_CACHE_CHECKSUM_LEN, new_hash, AS_CACHE_CHECKSUM_LEN);
	return TRUE;
}

/**
 * as_cache_hash_match_dict_insert:
 *
 * Helper function for as_cache_insert() FTS
 */
static gboolean
as_cache_hash_match_dict_insert (guint8 **dict, gsize *dict_len, const guint8 *hash, AsTokenType match_val)
{
	const gsize ENTRY_LEN = sizeof(AsTokenType) + AS_CACHE_CHECKSUM_LEN * sizeof(guint8);
	gsize insert_idx = *dict_len;
	g_assert_cmpint (*dict_len % ENTRY_LEN, ==, 0);

	/* check if the list already contains the hash that should be added */
	for (gsize i = 0; i < *dict_len; i += ENTRY_LEN) {
		if (memcmp ((*dict) + i, hash, AS_CACHE_CHECKSUM_LEN) == 0) {
			if (((AsTokenType) *(*dict + i + AS_CACHE_CHECKSUM_LEN)) == match_val)
				return FALSE; /* the entry is already known, we will add nothing to the dict */
			insert_idx = i; /* change index so we override the existing entry */
			break;
		}
	}

	if (insert_idx >= *dict_len) {
		/* the entry is new, so we need to add space in the list for it */

		*dict_len = *dict_len + ENTRY_LEN;
		*dict = g_realloc (*dict, *dict_len);
	}

	memcpy ((*dict) + insert_idx, hash, AS_CACHE_CHECKSUM_LEN);
	memcpy ((*dict) + insert_idx + AS_CACHE_CHECKSUM_LEN, &match_val, sizeof(AsTokenType));

	return TRUE;
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
	g_autofree guint8 *cpt_checksum = NULL;
	GError *tmp_error = NULL;

	GHashTable *token_cache;
	GHashTableIter tc_iter;
	gpointer tc_key, tc_value;

	GPtrArray *categories;
	GPtrArray *launchables;
	GPtrArray *provides;
	GPtrArray *extends;

	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, TRUE, error))
		return FALSE;
	locker = g_mutex_locker_new (&priv->mutex);

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

	/* add a "fake" launchable entry for desktop-apps that failed to include one.
	 * This is used for legacy compatibility */
	if ((as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP) &&
		(as_component_get_launchables (cpt)->len <= 0)) {
		if (g_str_has_suffix (as_component_get_id (cpt), ".desktop")) {
			g_autoptr(AsLaunchable) launchable = as_launchable_new ();
			as_launchable_set_kind (launchable, AS_LAUNCHABLE_KIND_DESKTOP_ID);
			as_launchable_add_entry (launchable, as_component_get_id (cpt));
			as_component_add_launchable (cpt, launchable);
		}
	}

	/* add the component data itself to the cache */
	xml_data = as_cache_component_to_xml (cache, cpt);
	as_generate_cache_checksum (as_component_get_data_id (cpt),
				    -1,
				    &cpt_checksum,
				    NULL);

	if (!as_txn_put_hash_kv_str (txn,
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
		MDB_val fts_val;
		g_autofree guint8 *match_list = NULL;
		gsize match_list_len;
		const gchar *token_str;
		size_t token_len;
		AsTokenType *match_pval;

		/* we ignore tokens which are too long to be database keys */
		token_str = (const gchar*) tc_key;
		token_len = sizeof(gchar) * strlen (token_str);
		if (token_len == 0) {
			g_warning ("Ignored empty search token for component '%s'", as_component_get_data_id (cpt));
			continue;
		}
		if (token_len > priv->max_keysize) {
			g_debug ("Ignored search token '%s': Too long to be stored in the cache.", token_str);
			continue;
		}

		/* get existing fts match value - we deliberately ignore any errors while reading here */
		fts_val = as_cache_txn_get_value (cache,
						  txn,
						  priv->db_fts,
						  token_str,
						  NULL);

		match_pval = (AsTokenType *) tc_value;
		/* TODO: There is potential to save on allocations here */
#if GLIB_CHECK_VERSION(2,68,0)
		match_list = g_memdup2 (fts_val.mv_data, fts_val.mv_size);
#else
		match_list = g_memdup (fts_val.mv_data, fts_val.mv_size);
#endif
		match_list_len = fts_val.mv_size;

		if (as_cache_hash_match_dict_insert (&match_list, &match_list_len, cpt_checksum, *match_pval)) {
			/* our checksum was not in the list yet, so add it */
			if (!as_cache_txn_put_kv_bytes (cache,
							txn,
							priv->db_fts,
							token_str,
							match_list,
							match_list_len,
							error)) {
				goto fail;
			}
		}
	}

	/* register this component with the CID mapping */
	{
		g_autofree guint8 *hash_list = NULL;
		gsize hash_list_len;
		hash_list = lmdb_val_memdup (as_cache_txn_get_value (cache,
									txn,
									priv->db_cids,
									as_component_get_id (cpt),
									&tmp_error), &hash_list_len);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto fail;
		}

		if (as_cache_hash_set_append (&hash_list, &hash_list_len, cpt_checksum)) {
			/* our checksum was not in the list yet, so add it */
			if (!as_cache_txn_put_kv_bytes (cache,
							txn,
							priv->db_cids,
							as_component_get_id (cpt),
							hash_list,
							hash_list_len,
							error)) {
				goto fail;
			}
		}
	}

	/* add category mapping */
	categories = as_component_get_categories (cpt);
	for (guint i = 0; i < categories->len; i++) {
		g_autofree guint8 *hash_list = NULL;
		gsize hash_list_len;
		const gchar *category = (const gchar*) g_ptr_array_index (categories, i);

		hash_list = lmdb_val_memdup (as_cache_txn_get_value (cache,
								    txn,
								    priv->db_cats,
								    category,
								    &tmp_error), &hash_list_len);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto fail;
		}

		if (as_cache_hash_set_append (&hash_list, &hash_list_len, cpt_checksum)) {
			/* this component was not yet registered with the category */
			if (!as_cache_txn_put_kv_bytes (cache,
							txn,
							priv->db_cats,
							category,
							hash_list,
							hash_list_len,
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
			g_autofree guint8 *hash_list = NULL;
			gsize hash_list_len;
			const gchar *entry = (const gchar*) g_ptr_array_index (entries, j);

			entry_key = g_strconcat (as_launchable_kind_to_string (as_launchable_get_kind (launchable)), entry, NULL);
			hash_list = lmdb_val_memdup (as_cache_txn_get_value (cache,
									txn,
									priv->db_launchables,
									entry_key,
									&tmp_error), &hash_list_len);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto fail;
			}

			if (as_cache_hash_set_append (&hash_list, &hash_list_len, cpt_checksum)) {
				/* add component checksum to launchable list */
				if (!as_cache_txn_put_kv_bytes (cache,
								txn,
								priv->db_launchables,
								entry_key,
								hash_list,
								hash_list_len,
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
			g_autofree guint8 *hash_list = NULL;
			gsize hash_list_len;
			const gchar *item = (const gchar*) g_ptr_array_index (items, j);

			item_key = g_strconcat (as_provided_kind_to_string (as_provided_get_kind (prov)), item, NULL);
			hash_list = lmdb_val_memdup (as_cache_txn_get_value (cache,
										txn,
										priv->db_provides,
										item_key,
										&tmp_error), &hash_list_len);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto fail;
			}

			if (as_cache_hash_set_append (&hash_list, &hash_list_len, cpt_checksum)) {
				/* add component checksum to launchable list */
				if (!as_cache_txn_put_kv_bytes (cache,
								txn,
								priv->db_provides,
								item_key,
								hash_list,
								hash_list_len,
								error)) {
					goto fail;
				}
			}
		}
	}

	/* add kinds mapping */
	{
		const gchar *kind_str = as_component_kind_to_string (as_component_get_kind (cpt));
		g_autofree guint8 *hash_list = NULL;
		gsize hash_list_len;

		hash_list = lmdb_val_memdup (as_cache_txn_get_value (cache,
									txn,
									priv->db_kinds,
									kind_str,
									&tmp_error), &hash_list_len);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto fail;
		}

		if (as_cache_hash_set_append (&hash_list, &hash_list_len, cpt_checksum)) {
			/* add component checksum to component-kind mapping */
			if (!as_cache_txn_put_kv_bytes (cache,
							txn,
							priv->db_kinds,
							kind_str,
							hash_list,
							hash_list_len,
							error)) {
				goto fail;
			}
		}
	}

	/* add addon/extension mapping for quick lookup of addons */
	extends = as_component_get_extends (cpt);
	if ((as_component_get_kind (cpt) == AS_COMPONENT_KIND_ADDON) &&
	    (extends != NULL) &&
	    (extends->len > 0)) {
		for (guint i = 0; i < extends->len; i++) {
			g_autofree gchar *extended_cdid = NULL;
			g_autofree guint8 *hash_list = NULL;
			gsize hash_list_len;
			const gchar *extended_cid = (const gchar*) g_ptr_array_index (extends, i);

			extended_cdid = as_utils_build_data_id (as_component_get_scope (cpt),
								as_utils_get_component_bundle_kind (cpt),
								as_component_get_origin (cpt),
								extended_cid,
								NULL);

			hash_list = lmdb_val_memdup (as_cache_txn_get_value (cache,
									     txn,
									     priv->db_addons,
									     extended_cdid,
									     &tmp_error), &hash_list_len);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto fail;
			}

			if (as_cache_hash_set_append (&hash_list, &hash_list_len, cpt_checksum)) {
				/* our checksum was not in the addon list yet, so add it */
				if (!as_cache_txn_put_kv_bytes (cache,
								txn,
								priv->db_addons,
								extended_cdid,
								hash_list,
								hash_list_len,
								error)) {
					goto fail;
				}
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
	g_autofree guint8 *cpt_checksum = NULL;
	GError *tmp_error = NULL;
	gboolean ret;
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, TRUE, error))
		return FALSE;
	locker = g_mutex_locker_new (&priv->mutex);

	if (priv->floating) {
		/* floating cache, remove only from the internal map */
		return g_hash_table_remove (priv->cpt_map, cdid);
	}

	if (priv->readonly) {
		as_generate_cache_checksum (cdid, -1, &cpt_checksum, NULL);
		g_hash_table_add (priv->ro_removed_set,
				  g_steal_pointer (&cpt_checksum));
		return TRUE;
	}

	txn = as_cache_transaction_new (cache, 0, error);
	if (txn == NULL)
		return FALSE;

	/* remove component itself from the cache */
	as_generate_cache_checksum (cdid, -1, &cpt_checksum, NULL);

	ret = lmdb_txn_delete_by_hash (txn, priv->db_cpts, cpt_checksum, &tmp_error);
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
as_cache_component_from_dval (AsCache *cache, MDB_txn *txn, MDB_val dval, GError **error)
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

	/* find addons (if there are any) - ensure addons don't have addons themselves */
	if ((as_component_get_kind (cpt) != AS_COMPONENT_KIND_ADDON) &&
	    !as_cache_register_addons_for_component (cache, txn, cpt, error)) {
		xmlFreeDoc (doc);
		return NULL;
	}

	if (priv->cpt_refine_func != NULL)
		(*priv->cpt_refine_func) (cpt, priv->cpt_refine_func_udata);

	xmlFreeDoc (doc);
	return g_steal_pointer (&cpt);
}

/**
 * as_cache_component_by_hash:
 *
 * Retrieve a component using its internal hash.
 */
static AsComponent*
as_cache_component_by_hash (AsCache *cache, MDB_txn *txn, const guint8 *cpt_hash, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_val dval;
	g_autoptr(AsComponent) cpt = NULL;
	GError *tmp_error = NULL;

	/* in case we "removed" the component on a readonly cache */
	if (g_hash_table_contains (priv->ro_removed_set, cpt_hash))
		return NULL;

	dval = as_txn_get_value_by_hash (cache, txn, priv->db_cpts, cpt_hash, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}
	if (dval.mv_size <= 0)
		return NULL;

	cpt = as_cache_component_from_dval (cache, txn, dval, error);
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
as_cache_components_by_hash_list (AsCache *cache, MDB_txn *txn, const guint8 *hlist, gsize hlist_len, GError **error)
{
	g_autoptr(GPtrArray) result = NULL;
	GError *tmp_error = NULL;
	g_assert_cmpint (hlist_len % AS_CACHE_CHECKSUM_LEN, ==, 0);

	result = g_ptr_array_new_with_free_func (g_object_unref);
	if (hlist == NULL)
		goto out;

	for (gsize i = 0; i < hlist_len; i += AS_CACHE_CHECKSUM_LEN) {
		AsComponent *cpt;

		cpt = as_cache_component_by_hash (cache, txn, hlist + i, &tmp_error);
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
 * as_cache_register_addons_for_component:
 *
 * Associate available addons with this component.
 */
static gboolean
as_cache_register_addons_for_component (AsCache *cache, MDB_txn *txn, AsComponent *cpt, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_val dval;
	g_autofree guint8 *cpt_checksum = NULL;
	GError *tmp_error = NULL;

	dval = as_cache_txn_get_value (cache,
					txn,
					priv->db_addons,
					as_component_get_data_id (cpt),
					&tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	if (dval.mv_size == 0)
		return TRUE;

	/* retrieve cache checksum of this component */
	as_generate_cache_checksum (as_component_get_data_id (cpt),
				    -1,
				    &cpt_checksum,
				    NULL);

	g_assert_cmpint (dval.mv_size % AS_CACHE_CHECKSUM_LEN, ==, 0);
	for (gsize i = 0; i < dval.mv_size; i += AS_CACHE_CHECKSUM_LEN) {
		const guint8 *chash = dval.mv_data + i;
		g_autoptr(AsComponent) addon = NULL;

		/* ignore addon that extends itself to prevent infinite recursion */
		if (memcmp (chash, cpt_checksum, AS_CACHE_CHECKSUM_LEN) == 0)
			continue;

		addon = as_cache_component_by_hash (cache, txn, chash, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_prefixed_error (error, tmp_error, "Failed to retrieve addon component data: ");
			return FALSE;
		}

		/* ensure we only link addons to the component directly, to prevent loops in refcounting and annoying
		 * dependency graph issues.
		 * Frontends can get their non-addon extensions via the "extends" property of #AsComponent and a pool query instead */
		if ((addon != NULL) && (as_component_get_kind (addon) == AS_COMPONENT_KIND_ADDON))
			as_component_add_addon (cpt, addon);
	}

	return TRUE;
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
	MDB_val dkey;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;
	locker = g_mutex_locker_new (&priv->mutex);

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

	rc = mdb_cursor_get (cur, &dkey, &dval, MDB_FIRST);
	while (rc == 0) {
		AsComponent *cpt;
		if (dval.mv_size <= 0) {
			rc = mdb_cursor_get (cur, NULL, &dval, MDB_NEXT);
			continue;
		}

		/* in case we "removed" the component on a readonly cache */
		if (priv->readonly) {
			g_autofree gchar *cpt_hash = g_strndup (dkey.mv_data, dkey.mv_size);
			if (g_hash_table_contains (priv->ro_removed_set, cpt_hash))
				return NULL;
		}

		cpt = as_cache_component_from_dval (cache, txn, dval, error);
		if (cpt == NULL)
			return NULL; /* error */
		g_ptr_array_add (results, cpt);

		rc = mdb_cursor_get (cur, NULL, &dval, MDB_NEXT);
	}
	mdb_cursor_close (cur);

	lmdb_transaction_commit (txn, NULL);
	return g_steal_pointer (&results);
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
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	GError *tmp_error = NULL;
	MDB_val dval;
	GPtrArray *result = NULL;
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, TRUE, error))
		return NULL;
	locker = g_mutex_locker_new (&priv->mutex);

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

	dval = as_cache_txn_get_value (cache, txn, priv->db_cids, id, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

	result = as_cache_components_by_hash_list (cache, txn, dval.mv_data, dval.mv_size, error);
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
	g_autofree guint8 *cpt_hash = NULL;
	AsComponent *cpt;
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, TRUE, error))
		return NULL;
	locker = g_mutex_locker_new (&priv->mutex);

	if (priv->floating) {
		/* floating cache, check only the internal map */
		cpt = g_hash_table_lookup (priv->cpt_map, cdid);
		return cpt? g_object_ref (cpt) : NULL;
	}

	/* in case we "removed" the component on a readonly cache */
	as_generate_cache_checksum (cdid, -1, &cpt_hash, NULL);
	if (g_hash_table_contains (priv->ro_removed_set, cpt_hash))
		return NULL;

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	dval = as_txn_get_value_by_hash (cache, txn, priv->db_cpts, cpt_hash, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}
	if (dval.mv_size <= 0) {
		/* nothing found? */
		lmdb_transaction_abort (txn);
		return NULL;
	}

	cpt = as_cache_component_from_dval (cache, txn, dval, error);
	if (cpt == NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

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
	MDB_val dval;
	GPtrArray *result = NULL;
	const gchar *kind_str = as_component_kind_to_string (kind);
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;
	locker = g_mutex_locker_new (&priv->mutex);

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	dval = as_cache_txn_get_value (cache, txn, priv->db_kinds, kind_str, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

	result = as_cache_components_by_hash_list (cache, txn, dval.mv_data, dval.mv_size, error);
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
	MDB_val dval;
	g_autofree gchar *item_key = NULL;
	GPtrArray *result = NULL;
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;
	locker = g_mutex_locker_new (&priv->mutex);

	item_key = g_strconcat (as_provided_kind_to_string (kind), item, NULL);
	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	dval = as_cache_txn_get_value (cache, txn, priv->db_provides, item_key, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

	result = as_cache_components_by_hash_list (cache, txn, dval.mv_data, dval.mv_size, error);
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
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;
	locker = g_mutex_locker_new (&priv->mutex);

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	result = g_ptr_array_new_with_free_func (g_object_unref);
	for (guint i = 0; categories[i] != NULL; i++) {
		g_autoptr(GPtrArray) tmp_res = NULL;
		MDB_val dval;

		dval = as_cache_txn_get_value (cache, txn, priv->db_cats, categories[i], &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			lmdb_transaction_abort (txn);
			return NULL;
		}
		if (dval.mv_size == 0)
			continue;

		tmp_res = as_cache_components_by_hash_list (cache, txn, dval.mv_data, dval.mv_size, error);
		if (tmp_res == NULL) {
			lmdb_transaction_abort (txn);
			return NULL;
		}

		as_object_ptr_array_absorb (result, tmp_res);
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
	g_autofree gchar *entry_key = NULL;
	MDB_val dval;
	GPtrArray *result = NULL;
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;
	locker = g_mutex_locker_new (&priv->mutex);

	entry_key = g_strconcat (as_launchable_kind_to_string (kind), id, NULL);
	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	dval = as_cache_txn_get_value (cache, txn, priv->db_launchables, entry_key, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		lmdb_transaction_abort (txn);
		return NULL;
	}

	result = as_cache_components_by_hash_list (cache, txn, dval.mv_data, dval.mv_size, error);
	if (result == NULL) {
		lmdb_transaction_abort (txn);
		return NULL;
	} else {
		lmdb_transaction_commit (txn, NULL);
		return result;
	}
}

typedef struct {
	AsComponent *cpt;
	GPtrArray *matched_terms;
	guint terms_found;
} AsSearchResultItem;

static void
as_search_result_item_free (AsSearchResultItem *item)
{
	if (item->cpt != NULL)
		g_object_unref (item->cpt);
	if (item->matched_terms != NULL)
		g_ptr_array_unref (item->matched_terms);
	g_slice_free (AsSearchResultItem, item);
}

/**
 * as_cache_update_results_with_fts_value:
 *
 * Update results table using the full-text search scoring data from a GVariant dict
 */
static gboolean
as_cache_update_results_with_fts_value (AsCache *cache, MDB_txn *txn,
					MDB_val dval, const gchar *matched_term,
					GHashTable *results_ht,
					gboolean exact_match,
					GError **error)
{
	GError *tmp_error = NULL;
	guint8 *data = NULL;
	gsize data_len = 0;
	const gsize ENTRY_LEN = sizeof(AsTokenType) + AS_CACHE_CHECKSUM_LEN * sizeof(guint8);

	data = dval.mv_data;
	data_len = dval.mv_size;

	g_assert_cmpint (data_len % ENTRY_LEN, ==, 0);
	if (data_len == 0)
		return TRUE;

	for (gsize i = 0; i < data_len; i += ENTRY_LEN) {
		guint sort_score;
		AsSearchResultItem *sitem;
		const guint8 *cpt_hash;
		AsTokenType match_pval;

		cpt_hash = data + i;
		match_pval = (AsTokenType) *(data + i + AS_CACHE_CHECKSUM_LEN);

		/* retrieve component with this hash */
		sitem = g_hash_table_lookup (results_ht, cpt_hash);
		if (sitem == NULL) {
			sitem = g_slice_new0 (AsSearchResultItem);
			sitem->cpt = as_cache_component_by_hash (cache, txn, cpt_hash, &tmp_error);
			sitem->matched_terms = g_ptr_array_new_with_free_func (g_free);
			if (tmp_error != NULL) {
				g_propagate_prefixed_error (error, tmp_error, "Failed to retrieve component data: ");
				as_search_result_item_free (sitem);
				return FALSE;
			}
			if (sitem->cpt == NULL) {
				as_search_result_item_free (sitem);
			} else {
				sort_score = 0;
				if (exact_match)
					sort_score |= match_pval << 2;
				else
					sort_score |= match_pval;

				if ((as_component_get_kind (sitem->cpt) == AS_COMPONENT_KIND_ADDON) && (match_pval > 0))
					sort_score--;

				as_component_set_sort_score (sitem->cpt, sort_score);
				sitem->terms_found = 1;
				g_ptr_array_add (sitem->matched_terms, g_strdup (matched_term));

#if GLIB_CHECK_VERSION(2,68,0)
				g_hash_table_insert (results_ht,
						     g_memdup2 (cpt_hash, AS_CACHE_CHECKSUM_LEN),
						     sitem);
#else
				g_hash_table_insert (results_ht,
						     g_memdup (cpt_hash, AS_CACHE_CHECKSUM_LEN),
						     sitem);
#endif

			}
		} else {
			gboolean term_matched = FALSE;

			sort_score = as_component_get_sort_score (sitem->cpt);
			if (exact_match)
				sort_score |= match_pval << 2;
			else
				sort_score |= match_pval;

			if (as_component_get_sort_score (sitem->cpt) == 0) {
				if ((as_component_get_kind (sitem->cpt) == AS_COMPONENT_KIND_ADDON))
					sort_score--;
			}

			as_component_set_sort_score (sitem->cpt, sort_score);

			/* due to stemming and partial matches, we may match the same term a lot of times,
			 * but we only want to register a specific term match once (while still bumping up the
			 * search result's match score) */
			for (guint j = 0; j < sitem->matched_terms->len; j++) {
				if (g_strcmp0 (matched_term, (const gchar *) g_ptr_array_index (sitem->matched_terms, j)) == 0) {
					term_matched = TRUE;
					break;
				}
			}
			if (!term_matched)
				sitem->terms_found++;
		}
	}

	return TRUE;
}

/**
 * as_cache_search_items_table_to_results:
 *
 * Converts the found items hash table entries to results list of components.
 */
static void
as_cache_search_items_table_to_results (GHashTable *results_ht, GPtrArray *results, guint required_terms_n)
{
	GHashTableIter ht_iter;
	gpointer ht_value;

	if (g_hash_table_size (results_ht) == 0)
		return;

	g_hash_table_iter_init (&ht_iter, results_ht);
	while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value)) {
		AsSearchResultItem *sitem = (AsSearchResultItem*) ht_value;
		if (sitem->terms_found < required_terms_n)
			continue;
		g_ptr_array_add (results, g_steal_pointer (&sitem->cpt));
	}

	/* the items hash table contents are invalid now */
	g_hash_table_remove_all (results_ht);
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
 * Returns: (transfer container): An array of #AsComponent
 */
GPtrArray*
as_cache_search (AsCache *cache, gchar **terms, gboolean sort, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GHashTable) results_ht = NULL;
	g_autoptr(GMutexLocker) locker = NULL;

	MDB_cursor *cur;
	gint rc;
	MDB_val dkey;
	MDB_val dval;
	g_autofree gsize *terms_lens = NULL;
	guint terms_n = 0;

	if (!as_cache_check_opened (cache, FALSE, error))
		return NULL;

	if (terms == NULL) {
		/* if we have no search terms, just return all components we have */
		return as_cache_get_components_all (cache, error);
	}

	locker = g_mutex_locker_new (&priv->mutex);

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return NULL;

	results = g_ptr_array_new_with_free_func (g_object_unref);
	results_ht = g_hash_table_new_full (as_cache_checksum_hash,
					    as_cache_checksum_equal,
					    g_free,
					    (GDestroyNotify) as_search_result_item_free);

	terms_n = g_strv_length (terms);

	/* unconditionally perform partial matching, which is slower, but yields more complete results
	 * closer to what the users seem to expect compared to the more narrow results we get when running
	 * an exact match query on the database */
	rc = mdb_cursor_open (txn, priv->db_fts, &cur);
	if (rc != MDB_SUCCESS) {
		g_set_error (error,
				AS_CACHE_ERROR,
				AS_CACHE_ERROR_FAILED,
				"Unable to iterate cache (no cursor): %s", mdb_strerror (rc));
		lmdb_transaction_abort (txn);
		return NULL;
	}

	/* cache term string lengths */
	terms_lens = g_new0 (gsize, terms_n + 1);
	for (guint i = 0; terms[i] != NULL; i++)
		terms_lens[i] = strlen (terms[i]);

	rc = mdb_cursor_get (cur, &dkey, &dval, MDB_FIRST);
	while (rc == 0) {
		const gchar *matched_term = NULL;
		const gchar *token = dkey.mv_data;
		gsize token_len = dkey.mv_size;

		for (guint i = 0; terms[i] != NULL; i++) {
			gsize term_len = terms_lens[i];
			/* if term length is bigger than the key, it will never match */
			if (term_len > dkey.mv_size)
				continue;

			for (guint j = 0; j < token_len - term_len + 1; j++) {
				if (strncmp (token + j, terms[i], term_len) == 0) {
					/* partial match was successful */
					matched_term = terms[i];
					break;
				}
			}
			if (matched_term != NULL)
				break;
		}
		if (matched_term == NULL) {
			rc = mdb_cursor_get(cur, &dkey, &dval, MDB_NEXT);
			continue;
		}

		/* we got a partial match, so add the components to our search result */
		if (dval.mv_size <= 0)
			continue;
		if (!as_cache_update_results_with_fts_value (cache, txn,
								dval, matched_term,
								results_ht,
								FALSE,
								error)) {
			mdb_cursor_close (cur);
			lmdb_transaction_abort (txn);
			return NULL;
		}

		rc = mdb_cursor_get(cur, &dkey, &dval, MDB_NEXT);
	}
	mdb_cursor_close (cur);

	/* compile our result */
	as_cache_search_items_table_to_results (results_ht,
						results,
						terms_n);

	/* we don't need the mutex anymore, no class member access here */
	g_clear_pointer (&locker, g_mutex_locker_free);

	/* sort the results by their priority */
	if (sort)
		as_sort_components_by_score (results);

	lmdb_transaction_commit (txn, NULL);
	return g_steal_pointer (&results);
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
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, TRUE, error))
		return FALSE;
	locker = g_mutex_locker_new (&priv->mutex);

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
 * Returns: Components count in the database, or -1 on error.
 */
gssize
as_cache_count_components (AsCache *cache, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn;
	MDB_stat stats;
	gint rc;
	gssize count = -1;
	g_autoptr(GMutexLocker) locker = NULL;

	if (!as_cache_check_opened (cache, FALSE, error))
		return 0;
	locker = g_mutex_locker_new (&priv->mutex);

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
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	if (priv->fname == NULL)
		return 0;

	if (stat (priv->fname, &cache_sbuf) < 0)
		return 0;
	else
		return cache_sbuf.st_ctime;
}

/**
 * as_cache_is_open:
 * @cache: An instance of #AsCache.
 *
 * Returns: %TRUE if the cache is open.
 */
gboolean
as_cache_is_open (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return priv->opened;
}

/**
 * as_cache_make_floating:
 * @cache: An instance of #AsCache.
 *
 * Make cache "floating": Only some actions are permitted and nothing
 * is written to disk until the floating state is changed.
 */
void
as_cache_make_floating (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	if (priv->floating)
		return;
	priv->floating = TRUE;

	g_hash_table_remove_all (priv->cpt_map);
	g_hash_table_remove_all (priv->cid_set);
	g_debug ("Cache set to floating mode.");
}

/**
 * as_cache_unfloat:
 * @cache: An instance of #AsCache.
 *
 * Persist all changes from a floating cache.
 * Return the number of invalid components.
 */
guint
as_cache_unfloat (AsCache *cache, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	GHashTableIter iter;
	gpointer ht_value;
	guint invalid_cpts = 0;

	g_mutex_lock (&priv->mutex);

	priv->floating = FALSE;

	g_hash_table_iter_init (&iter, priv->cpt_map);
	while (g_hash_table_iter_next (&iter, NULL, &ht_value)) {
		AsComponent *cpt = AS_COMPONENT (ht_value);

		/* validate the component */
		if (!as_component_is_valid (cpt)) {
			/* we still succeed if the components originates from a .desktop file -
			 * we care less about them and they generally have bad quality, so some issues
			 * pop up on pretty much every system */
			if (as_component_get_origin_kind (cpt) == AS_ORIGIN_KIND_DESKTOP_ENTRY) {
				g_debug ("Ignored '%s': The component (from a .desktop file) is invalid.", as_component_get_id (cpt));
			} else {
				g_debug ("WARNING: Ignored component '%s': The component is invalid.", as_component_get_id (cpt));
				invalid_cpts++;
			}
			continue;
		}

		g_mutex_unlock (&priv->mutex);
		if (!as_cache_insert (cache, cpt, error))
			return 0;
		g_mutex_lock (&priv->mutex);
	}

	g_hash_table_remove_all (priv->cid_set);
	g_hash_table_remove_all (priv->cpt_map);

	g_mutex_unlock (&priv->mutex);
	g_debug ("Cache returned from floating mode (all changes are now persistent)");

	return invalid_cpts;
}

/**
 * as_cache_get_location:
 * @cache: An instance of #AsCache.
 *
 * Returns: Location string for this cache.
 */
const gchar*
as_cache_get_location (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return priv->fname;
}

/**
 * as_cache_set_location:
 * @cache: An instance of #AsCache.
 *
 * Set location string for this database.
 */
void
as_cache_set_location (AsCache *cache, const gchar *location)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	g_free (priv->fname);
	priv->fname = g_strdup (location);
}

/**
 * as_cache_get_nosync:
 * @cache: An instance of #AsCache.
 *
 * Returns: %TRUE if we don't sync explicitly.
 */
gboolean
as_cache_get_nosync (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return priv->nosync;
}

/**
 * as_cache_set_nosync:
 * @cache: An instance of #AsCache.
 *
 * Set whether the cache should sync to disk explicitly or not.
 * This setting may be ignored if the cache is in temporary
 * or in-memory mode.
 */
void
as_cache_set_nosync (AsCache *cache, gboolean nosync)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	priv->nosync = nosync;
}

/**
 * as_cache_get_readonly:
 * @cache: An instance of #AsCache.
 *
 * Returns: %TRUE if the cache is read-only.
 */
gboolean
as_cache_get_readonly (AsCache *cache)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	return priv->readonly;
}

/**
 * as_cache_set_readonly:
 * @cache: An instance of #AsCache.
 *
 * Set whether the cache should be read-only.
 */
void
as_cache_set_readonly (AsCache *cache, gboolean ro)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);
	priv->readonly = ro;
}

/**
 * as_cache_set_refine_func:
 * @cache: An instance of #AsCache.
 *
 * Set function to be called on a component after it was deserialized.
 */
void
as_cache_set_refine_func (AsCache *cache, GFunc func, gpointer user_data)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->mutex);

	priv->cpt_refine_func = func;
	priv->cpt_refine_func_udata = user_data;
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
