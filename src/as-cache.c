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
	gchar *volatile_db_fname;
	gboolean opened;

	AsContext *context;
	gchar *locale;
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
	priv->locale = as_get_current_locale ();

	priv->context = as_context_new ();
	as_context_set_locale (priv->context, priv->locale);
	as_context_set_style (priv->context, AS_FORMAT_STYLE_COLLECTION);
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
lmdb_str_to_dbval (const gchar *data)
{
	MDB_val val;
	val.mv_size = sizeof(gchar*) * strlen (data);
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
	MDB_val dbkey;
	gint rc;

        dbkey = lmdb_str_to_dbval (key);
        rc = mdb_put (txn, dbi, &dbkey, &dval, 0);
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
	MDB_val dbvalue;
        dbvalue = lmdb_str_to_dbval (value);

	return as_cache_txn_put_kv (cache, txn, dbi, key, dbvalue, error);
}

/**
 * as_cache_put_kv:
 *
 * Add key/value pair to the database
 */
inline static gboolean
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
	MDB_cursor *cur;
	MDB_val dkey, dval;
	gint rc;

	dval.mv_size = 0;
        dkey = lmdb_str_to_dbval (key);
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
	MDB_val data;
	data.mv_size = 0;

	txn = as_cache_transaction_new (cache, MDB_RDONLY, error);
	if (txn == NULL)
		return data;

	data = as_cache_txn_get_value (cache, txn, dbi, key, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return data;
	}

	lmdb_transaction_commit (txn, NULL);
	return data;
}

/**
 * as_cache_check_opened:
 */
static gboolean
as_cache_check_opened (AsCache *cache, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
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
	g_autofree gchar *db_location = NULL;
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

	if (volatile_dir != NULL) {
		gint fd;

		db_location = g_build_filename (volatile_dir, "appstream-cache-XXXXXX.mdb", NULL);
		fd = g_mkstemp (db_location);
		if (fd < 0) {
			g_set_error (error,
				AS_CACHE_ERROR,
				AS_CACHE_ERROR_FAILED,
				"Unable to open temporary cache file: %s", g_strerror (errno));
			goto fail;
		} else {
			close (fd);
		}

		priv->volatile_db_fname = g_strdup (db_location);
	} else {
		db_location = g_strdup (fname);
	}

	g_debug ("Opening cache file: %s", db_location);
	rc = mdb_env_open (priv->db_env,
			   db_location,
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

	priv->opened = FALSE;
	return TRUE;
}

/**
 * as_cache_insert:
 * @cache: An instance of #AsCache.
 * @cpt: The #AsComponent to insert.
 * @replace: Whether to replace any existing component.
 * @error: A #GError or %NULL.
 *
 * Insert a new component into the cache.
 *
 * Returns: %TRUE if there was no error.
 **/
gboolean
as_cache_insert (AsCache *cache, AsComponent *cpt, gboolean replace, GError **error)
{
	AsCachePrivate *priv = GET_PRIVATE (cache);
	MDB_txn *txn = NULL;
	g_autofree gchar *xml_data = NULL;
	g_autofree gchar *cpt_checksum = NULL;
	GError *tmp_error = NULL;

	GHashTable *token_cache;
	GHashTableIter tc_iter;
	gpointer tc_key, tc_value;

	if (!as_cache_check_opened (cache, error))
		return FALSE;

	txn = as_cache_transaction_new (cache, 0, error);
	if (txn == NULL)
		return FALSE;

	/* add the component data itself to the cache */
	xml_data = as_cache_component_to_xml (cache, cpt);
	cpt_checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5,
						      as_component_get_data_id (cpt),
						      -1);
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
		g_autoptr(GVariant) var = NULL;
		g_autoptr(GVariant) nvar = NULL;
		g_autofree gpointer entry_data = NULL;
		GVariantDict dict;
		AsTokenType *match_pval;
		MDB_val dval;

		match_pval = (AsTokenType *) tc_value;
		dval = as_cache_txn_get_value (cache, txn, priv->db_fts, (gchar*) tc_key, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto fail;
		}

		if (dval.mv_size > 0) {
			/* GVariant doesn't like the mmap, so we need to copy the data here. This should be fixed in GLib >= 2.60 */
			entry_data = g_memdup (dval.mv_data, dval.mv_size);
			var = g_variant_new_from_data (G_VARIANT_TYPE_VARDICT,
							entry_data,
							dval.mv_size,
							TRUE, /* trusted */
							NULL,
							NULL);
			g_variant_dict_init (&dict, var);
		} else {
			g_variant_dict_init (&dict, NULL);
		}

		g_variant_dict_insert (&dict, cpt_checksum, "q", *match_pval);

		nvar = g_variant_ref_sink (g_variant_dict_end (&dict));

		MDB_val ndval;
		ndval.mv_size = g_variant_get_size (nvar);
		ndval.mv_data = (void*) g_variant_get_data (nvar);

		if (!as_cache_txn_put_kv (cache,
					  txn,
					  priv->db_fts,
					  (const gchar*) tc_key,
					  ndval,
					  error)) {
			goto fail;
		}
	}

	return lmdb_transaction_commit (txn, error);
fail:
	lmdb_transaction_abort (txn);
	return FALSE;
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
