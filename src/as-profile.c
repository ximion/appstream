/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2015 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>

#include "as-profile.h"

struct _AsProfile
{
	GObject		 parent_instance;
	GPtrArray	*current;
	GPtrArray	*archived;
	GMutex		 mutex;
	GThread		*unthreaded;
	guint		 autodump_id;
	guint		 autoprune_duration;
	guint		 duration_min;
};

typedef struct {
	gchar		*id;
	gint64		 time_start;
	gint64		 time_stop;
	gboolean	 threaded;
} AsProfileItem;

G_DEFINE_TYPE (AsProfile, as_profile, G_TYPE_OBJECT)

struct _AsProfileTask
{
	AsProfile	*profile;
	gchar		*id;
};

static gpointer as_profile_object = NULL;

static void
as_profile_item_free (AsProfileItem *item)
{
	g_free (item->id);
	g_free (item);
}

static void
as_profile_item_free_cb (AsProfileItem *item, gpointer user_data)
{
	as_profile_item_free (item);
}

static AsProfileItem *
as_profile_item_find (GPtrArray *array, const gchar *id)
{
	AsProfileItem *tmp;
	guint i;

	g_return_val_if_fail (id != NULL, NULL);

	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (tmp->id, id) == 0)
			return tmp;
	}
	return NULL;
}

/**
 * as_profile_task_set_threaded:
 * @ptask: A #AsProfileTask
 * @threaded: boolean
 *
 * Sets if the profile task is threaded so it can be printed differently in
 * in the profile output.
 *
 * Since: 0.14.0
 **/
void
as_profile_task_set_threaded (AsProfileTask *ptask, gboolean threaded)
{
	AsProfileItem *item;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&ptask->profile->mutex);
	item = as_profile_item_find (ptask->profile->current, ptask->id);
	if (item == NULL)
		return;
	item->threaded = threaded;
}

static void
as_profile_prune_safe (AsProfile *profile, guint duration)
{
	gint64 now;
	guint i;
	g_autoptr(GPtrArray) removal = g_ptr_array_new ();

	/* find all events older than duration */
	now = g_get_real_time () / 1000;
	for (i = 0; i < profile->archived->len; i++) {
		AsProfileItem *item = g_ptr_array_index (profile->archived, i);
		if (now - item->time_start / 1000 > duration)
			g_ptr_array_add (removal, item);
	}

	/* remove each one */
	for (i = 0; i < removal->len; i++) {
		AsProfileItem *item = g_ptr_array_index (removal, i);
		g_ptr_array_remove (profile->archived, item);
	}
}

static gint
as_profile_sort_cb (gconstpointer a, gconstpointer b)
{
	AsProfileItem *item_a = *((AsProfileItem **) a);
	AsProfileItem *item_b = *((AsProfileItem **) b);
	if (item_a->time_start < item_b->time_start)
		return -1;
	if (item_a->time_start > item_b->time_start)
		return 1;
	return 0;
}

static const gchar *
as_profile_get_item_printable (AsProfileItem *item)
{
	if (item->threaded)
		return "\033[1m#\033[0m";
	return "#";
}

static void
as_profile_dump_safe (AsProfile *profile)
{
	AsProfileItem *item;
	gint64 time_start = G_MAXINT64;
	gint64 time_stop = 0;
	gint64 time_ms;
	guint console_width = 86;
	guint i;
	guint j;
	gdouble scale;
	guint bar_offset;
	guint bar_length;

	/* nothing to show */
	if (profile->archived->len == 0)
		return;

	/* get the start and end times */
	for (i = 0; i < profile->archived->len; i++) {
		item = g_ptr_array_index (profile->archived, i);
		if (item->time_start < time_start)
			time_start = item->time_start;
		if (item->time_stop > time_stop)
			time_stop = item->time_stop;
	}
	scale = (gdouble) console_width / (gdouble) ((time_stop - time_start) / 1000);

	/* sort the list */
	g_ptr_array_sort (profile->archived, as_profile_sort_cb);

	/* dump a list of what happened when */
	for (i = 0; i < profile->archived->len; i++) {
		const gchar *printable;
		item = g_ptr_array_index (profile->archived, i);
		time_ms = (item->time_stop - item->time_start) / 1000;
		if (time_ms < profile->duration_min)
			continue;

		/* print a timechart of what we've done */
		bar_offset = (guint) (scale * (gdouble) (item->time_start -
							 time_start) / 1000);
		for (j = 0; j < bar_offset; j++)
			g_printerr (" ");
		bar_length = (guint) (scale * (gdouble) time_ms);
		if (bar_length == 0)
			bar_length = 1;
		printable = as_profile_get_item_printable (item);
		for (j = 0; j < bar_length; j++)
			g_printerr ("%s", printable);
		for (j = bar_offset + bar_length; j < console_width + 1; j++)
			g_printerr (" ");
		g_printerr ("@%04" G_GINT64_FORMAT "ms ",
			    (item->time_stop - time_start) / 1000);
		g_printerr ("%s %" G_GINT64_FORMAT "ms\n", item->id, time_ms);
	}

	/* not all complete */
	if (profile->current->len > 0) {
		for (i = 0; i < profile->current->len; i++) {
			item = g_ptr_array_index (profile->current, i);
			item->time_stop = g_get_real_time ();
			for (j = 0; j < console_width; j++)
				g_print ("$");
			time_ms = (item->time_stop - item->time_start) / 1000;
			g_printerr (" @????ms %s %" G_GINT64_FORMAT "ms\n",
				    item->id, time_ms);
		}
	}
}

/**
 * as_profile_start: (skip)
 * @profile: A #AsProfile
 * @fmt: Format string
 * @...: varargs
 *
 * Starts profiling a section of code.
 *
 * Since: 0.14.0
 *
 * Returns: A #AsProfileTask, free with as_profile_task_free()
 **/
AsProfileTask *
as_profile_start (AsProfile *profile, const gchar *fmt, ...)
{
	va_list args;
	g_autofree gchar *tmp = NULL;
	va_start (args, fmt);
	tmp = g_strdup_vprintf (fmt, args);
	va_end (args);
	return as_profile_start_literal (profile, tmp);
}

/**
 * as_profile_start_literal: (skip)
 * @profile: A #AsProfile
 * @id: ID string
 *
 * Starts profiling a section of code.
 *
 * Since: 0.14.0
 *
 * Returns: A #AsProfileTask, free with as_profile_task_free()
 **/
AsProfileTask *
as_profile_start_literal (AsProfile *profile, const gchar *id)
{
	GThread *self;
	AsProfileItem *item;
	AsProfileTask *ptask = NULL;
	g_autofree gchar *id_thr = NULL;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&profile->mutex);

	g_return_val_if_fail (AS_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	/* autoprune old data */
	if (profile->autoprune_duration != 0)
		as_profile_prune_safe (profile, profile->autoprune_duration);

	/* only use the thread ID when not using the main thread */
	self = g_thread_self ();
	if (self != profile->unthreaded) {
		id_thr = g_strdup_printf ("%p~%s", self, id);
	} else {
		id_thr = g_strdup (id);
	}

	/* already started */
	item = as_profile_item_find (profile->current, id_thr);
	if (item != NULL) {
		as_profile_dump_safe (profile);
		g_warning ("Already a started task for %s", id_thr);
		return NULL;
	}

	/* add new item */
	item = g_new0 (AsProfileItem, 1);
	item->id = g_strdup (id_thr);
	item->time_start = g_get_real_time ();
	g_ptr_array_add (profile->current, item);
	g_debug ("run %s", id_thr);

	/* create token */
	ptask = g_new0 (AsProfileTask, 1);
	ptask->profile = g_object_ref (profile);
	ptask->id = g_strdup (id);
	return ptask;
}

static void
as_profile_task_free_internal (AsProfile *profile, const gchar *id)
{
	GThread *self;
	AsProfileItem *item;
	gdouble elapsed_ms;
	g_autofree gchar *id_thr = NULL;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&profile->mutex);

	g_return_if_fail (AS_IS_PROFILE (profile));
	g_return_if_fail (id != NULL);

	/* only use the thread ID when not using the main thread */
	self = g_thread_self ();
	if (self != profile->unthreaded) {
		id_thr = g_strdup_printf ("%p~%s", self, id);
	} else {
		id_thr = g_strdup (id);
	}

	/* already started */
	item = as_profile_item_find (profile->current, id_thr);
	if (item == NULL) {
		g_warning ("Not already a started task for %s", id_thr);
		return;
	}

	/* debug */
	elapsed_ms = (gdouble) (item->time_stop - item->time_start) / 1000;
	if (elapsed_ms > 5)
		g_debug ("%s took %.0fms", id_thr, elapsed_ms);

	/* update */
	item->time_stop = g_get_real_time ();

	/* move to archive */
	g_ptr_array_remove (profile->current, item);
	g_ptr_array_add (profile->archived, item);
}

/**
 * as_profile_task_free:
 * @ptask: A #AsProfileTask
 *
 * Frees a profile token, and marks the portion of code complete.
 *
 * Since: 0.14.0
 **/
void
as_profile_task_free (AsProfileTask *ptask)
{
	if (ptask == NULL)
		return;
	g_assert (ptask->id != NULL);
	as_profile_task_free_internal (ptask->profile, ptask->id);
	g_free (ptask->id);
	g_object_unref (ptask->profile);
	g_free (ptask);
}

/**
 * as_profile_clear:
 * @profile: A #AsProfile
 *
 * Clears the list of profiled events.
 *
 * Since: 0.14.0
 **/
void
as_profile_clear (AsProfile *profile)
{
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&profile->mutex);
	g_ptr_array_set_size (profile->archived, 0);
}

/**
 * as_profile_prune:
 * @profile: A #AsProfile
 * @duration: A time in ms
 *
 * Clears the list of profiled events older than @duration.
 *
 * Since: 0.14.0
 **/
void
as_profile_prune (AsProfile *profile, guint duration)
{
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&profile->mutex);
	g_return_if_fail (AS_IS_PROFILE (profile));
	as_profile_prune_safe (profile, duration);
}

/**
 * as_profile_set_autoprune:
 * @profile: A #AsProfile
 * @duration: A time in ms
 *
 * Automatically prunes events older than @duration when new ones are added.
 *
 * Since: 0.14.0
 **/
void
as_profile_set_autoprune (AsProfile *profile, guint duration)
{
	profile->autoprune_duration = duration;
	as_profile_prune (profile, duration);
}

/**
 * as_profile_set_duration_min:
 * @profile: A #AsProfile
 * @duration_min: A time in ms
 *
 * Sets the smallest recordable task duration.
 *
 * Since: 0.14.0
 **/
void
as_profile_set_duration_min (AsProfile *profile, guint duration_min)
{
	profile->duration_min = duration_min;
}

/**
 * as_profile_dump:
 * @profile: A #AsProfile
 *
 * Dumps the current profiling table to stdout.
 *
 * Since: 0.14.0
 **/
void
as_profile_dump (AsProfile *profile)
{
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&profile->mutex);
	g_return_if_fail (AS_IS_PROFILE (profile));
	as_profile_dump_safe (profile);
}

static gboolean
as_profile_autodump_cb (gpointer user_data)
{
	AsProfile *profile = AS_PROFILE (user_data);
	as_profile_dump (profile);
	profile->autodump_id = 0;
	return G_SOURCE_REMOVE;
}

/**
 * as_profile_set_autodump:
 * @profile: A #AsProfile
 * @delay: Duration in ms
 *
 * Dumps the current profiling table to stdout on a set interval.
 *
 * Since: 0.14.0
 **/
void
as_profile_set_autodump (AsProfile *profile, guint delay)
{
	if (profile->autodump_id != 0)
		g_source_remove (profile->autodump_id);
	profile->autodump_id = g_timeout_add (delay, as_profile_autodump_cb, profile);
}

static void
as_profile_finalize (GObject *object)
{
	AsProfile *profile = AS_PROFILE (object);

	if (profile->autodump_id != 0)
		g_source_remove (profile->autodump_id);
	g_ptr_array_foreach (profile->current,
			     (GFunc) as_profile_item_free_cb, NULL);
	g_ptr_array_unref (profile->current);
	g_ptr_array_unref (profile->archived);
	g_mutex_clear (&profile->mutex);

	G_OBJECT_CLASS (as_profile_parent_class)->finalize (object);
}

static void
as_profile_class_init (AsProfileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_profile_finalize;
}

static void
as_profile_init (AsProfile *profile)
{
	profile->duration_min = 5;
	profile->current = g_ptr_array_new ();
	profile->unthreaded = g_thread_self ();
	profile->archived = g_ptr_array_new_with_free_func ((GDestroyNotify) as_profile_item_free);
	g_mutex_init (&profile->mutex);
}

/**
 * as_profile_new:
 *
 * Creates a new #AsProfile.
 *
 * Returns: (transfer full): a #AsProfile
 *
 * Since: 0.14.0
 **/
AsProfile *
as_profile_new (void)
{
	if (as_profile_object != NULL) {
		g_object_ref (as_profile_object);
	} else {
		as_profile_object = g_object_new (AS_TYPE_PROFILE, NULL);
		g_object_add_weak_pointer (as_profile_object, &as_profile_object);
	}
	return AS_PROFILE (as_profile_object);
}
