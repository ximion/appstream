/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2018 Matthias Klumpp <matthias@tenstral.net>
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

#include <glib.h>
#include <glib/gprintf.h>

#include "appstream.h"
#include "as-metadata.h"
#include "as-test-utils.h"

static gchar *datadir = NULL;


/**
 * test_get_sampledata_pool:
 *
 * Internal helper to get a pool with the sample data locations set.
 */
static AsPool*
test_get_sampledata_pool (gboolean use_caches)
{
	AsPool *pool;
	AsPoolFlags flags;
	g_autofree gchar *mdata_dir = NULL;

	/* create AsPool and load sample metadata */
	mdata_dir = g_build_filename (datadir, "collection", NULL);

	pool = as_pool_new ();
	as_pool_clear_metadata_locations (pool);
	as_pool_add_metadata_location (pool, mdata_dir);
	as_pool_set_locale (pool, "C");

	flags = as_pool_get_flags (pool);
	as_flags_remove (flags, AS_POOL_FLAG_READ_DESKTOP_FILES);
	as_pool_set_flags (pool, flags);

	if (!use_caches)
		as_pool_set_cache_flags (pool, AS_CACHE_FLAG_NONE);

	return pool;
}


/**
 * Test performance of loading a metadata pool from XML.
 */
static void
test_pool_read_perf (void)
{
	GError *error = NULL;
	guint i;
	guint loops = 10;
	g_autoptr(GTimer) timer = NULL;

	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		GPtrArray *cpts;
		g_autoptr(AsPool) pool = test_get_sampledata_pool (FALSE);

		as_pool_load (pool, NULL, &error);
		g_assert_no_error (error);

		cpts = as_pool_get_components (pool);
		g_assert_cmpint (cpts->len, ==, 19);

	}
	g_print ("%.0f ms: ", g_timer_elapsed (timer, NULL) * 1000 / loops);
}

/**
 * main:
 */
int
main (int argc, char **argv)
{
	int ret;

	if (argc == 0) {
		g_error ("No test directory specified!");
		return 1;
	}

	datadir = argv[1];
	g_assert (datadir != NULL);
	datadir = g_build_filename (datadir, "samples", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* Only ever run tests in slow mode (set manually).
	 * This prevents build/test failures on slower machines */
	if (!g_test_slow ())
		return 0;


	g_test_add_func ("/Perf/Pool/Read", test_pool_read_perf);


	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
