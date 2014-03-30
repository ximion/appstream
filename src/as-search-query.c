/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include "as-search-query.h"

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "as-utils.h"

struct _AsSearchQueryPrivate {
	gchar* search_term;
	gchar** categories;
};

static gpointer as_search_query_parent_class = NULL;

/** TRANSLATORS: List of "grey-listed" words sperated with ";"
 * Do not translate this list directly. Instead,
 * provide a list of words in your language that people are likely
 * to include in a search but that should normally be ignored in
 * the search.
 */
#define AS_SEARCH_GREYLIST_STR _ ("app;application;package;program;programme;suite;tool")

#define AS_SEARCH_QUERY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), AS_TYPE_SEARCH_QUERY, AsSearchQueryPrivate))

enum  {
	AS_SEARCH_QUERY_DUMMY_PROPERTY,
	AS_SEARCH_QUERY_SEARCH_TERM,
	AS_SEARCH_QUERY_CATEGORIES
};

static void as_search_query_finalize (GObject* obj);
static void as_search_query_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void as_search_query_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);


AsSearchQuery* as_search_query_construct (GType object_type, const gchar* term)
{
	AsSearchQuery * self = NULL;
	g_return_val_if_fail (term != NULL, NULL);
	self = (AsSearchQuery*) g_object_new (object_type, NULL);

	as_search_query_set_search_term (self, term);
	return self;
}


AsSearchQuery* as_search_query_new (const gchar* term)
{
	return as_search_query_construct (AS_TYPE_SEARCH_QUERY, term);
}


/**
 * @return TRUE if we search in all categories
 */
gboolean as_search_query_get_search_all_categories (AsSearchQuery* self)
{
	g_return_val_if_fail (self != NULL, FALSE);

	return g_strv_length (self->priv->categories) <= 0;
}


/**
 * Shortcut to set that we should search in all categories
 */
void as_search_query_set_search_all_categories (AsSearchQuery* self)
{
	gchar **strv;
	g_return_if_fail (self != NULL);
	strv = g_new0 (gchar*, 0 + 1);
	g_strfreev (self->priv->categories);
	self->priv->categories = strv;
}


/**
 * Set the categories list from a string
 *
 * @param categories_str Comma-separated list of category-names
 */
void as_search_query_set_categories_from_string (AsSearchQuery* self, const gchar* categories_str)
{
	gchar **cats;
	g_return_if_fail (self != NULL);
	g_return_if_fail (categories_str != NULL);

	cats = g_strsplit (categories_str, ",", 0);
	g_strfreev (self->priv->categories);
	self->priv->categories = cats;
}


static gint string_index_of (const gchar* self, const gchar* needle, gint start_index) {
	gint result = 0;
	gchar* _result_ = NULL;
	gint _tmp0_ = 0;
	const gchar* _tmp1_ = NULL;
	gchar* _tmp2_ = NULL;
	gchar* _tmp3_ = NULL;
	g_return_val_if_fail (self != NULL, 0);
	g_return_val_if_fail (needle != NULL, 0);
	_tmp0_ = start_index;
	_tmp1_ = needle;
	_tmp2_ = strstr (((gchar*) self) + _tmp0_, (gchar*) _tmp1_);
	_result_ = _tmp2_;
	_tmp3_ = _result_;
	if (_tmp3_ != NULL) {
		gchar* _tmp4_ = NULL;
		_tmp4_ = _result_;
		result = (gint) (_tmp4_ - ((gchar*) self));
		return result;
	} else {
		result = -1;
		return result;
	}
}

void as_search_query_sanitize_search_term (AsSearchQuery* self)
{
	gchar *search_term;
	g_return_if_fail (self != NULL);

	search_term = self->priv->search_term;

	/* check if there is a ":" in the search, if so, it means the user
	 * is using a xapian prefix like "pkg:" or "mime:" and in this case
	 * we do not want to alter the search term (as application is in the
	 * greylist but a common mime-type prefix)
	 */
	if (string_index_of (search_term, ":", 0) <= 0) {
		gchar **greylist;
		gchar *orig_term;
		guint i;

		orig_term = g_strdup (search_term);

		/* filter query by greylist (to avoid overly generic search terms) */
		greylist = g_strsplit (AS_SEARCH_GREYLIST_STR, ";", 0);
		for (i = 0; greylist[i] != NULL; i++) {
			gchar *str;
			str = as_str_replace (search_term, greylist[i], "");
			as_search_query_set_search_term (self, str);
			search_term = self->priv->search_term;
			g_free (str);
		}

		/* restore query if it was just greylist words */
		if (g_strcmp0 (search_term, "") == 0) {
			g_debug ("grey-list replaced all terms, restoring");
			as_search_query_set_search_term (self, orig_term);
		}
		g_free (orig_term);
	}

	/* we have to strip the leading and trailing whitespaces to avoid having
	 * different results for e.g. 'font ' and 'font' (LP: #506419)
	 */
	g_strstrip (self->priv->search_term);
}


const gchar* as_search_query_get_search_term (AsSearchQuery* self)
{
	const gchar* result;
	g_return_val_if_fail (self != NULL, NULL);
	return self->priv->search_term;
}


void as_search_query_set_search_term (AsSearchQuery* self, const gchar* value)
{
	const gchar* _tmp0_ = NULL;
	g_return_if_fail (self != NULL);

	g_free (self->priv->search_term);
	self->priv->search_term = g_strdup (value);
	g_object_notify ((GObject *) self, "search-term");
}


gchar** as_search_query_get_categories (AsSearchQuery* self)
{
	gchar** result;
	g_return_val_if_fail (self != NULL, NULL);
	result = self->priv->categories;
	return result;
}

static gchar** str_array_dup (gchar** self) {
	gchar** result;
	guint length;
	int i;
	length = g_strv_length (self);
	result = g_new0 (gchar*, length + 1);
	for (i = 0; i < length; i++) {
		gchar* _tmp0_ = NULL;
		_tmp0_ = g_strdup (self[i]);
		result[i] = _tmp0_;
	}
	return result;
}

void as_search_query_set_categories (AsSearchQuery* self, gchar** value) {
	g_return_if_fail (self != NULL);
	self->priv->categories = str_array_dup (value);
	g_object_notify ((GObject *) self, "categories");
}


static void as_search_query_class_init (AsSearchQueryClass * klass) {
	as_search_query_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (AsSearchQueryPrivate));
	G_OBJECT_CLASS (klass)->get_property = as_search_query_get_property;
	G_OBJECT_CLASS (klass)->set_property = as_search_query_set_property;
	G_OBJECT_CLASS (klass)->finalize = as_search_query_finalize;
	g_object_class_install_property (G_OBJECT_CLASS (klass),
									 AS_SEARCH_QUERY_SEARCH_TERM,
									 g_param_spec_string ("search-term", "search-term", "search-term", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE)
	);
	g_object_class_install_property (G_OBJECT_CLASS (klass),
									 AS_SEARCH_QUERY_CATEGORIES,
								  g_param_spec_boxed ("categories", "categories", "categories", G_TYPE_STRV, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE)
	);
}


static void as_search_query_instance_init (AsSearchQuery * self) {
	self->priv = AS_SEARCH_QUERY_GET_PRIVATE (self);
}


static void as_search_query_finalize (GObject* obj) {
	AsSearchQuery * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, AS_TYPE_SEARCH_QUERY, AsSearchQuery);
	g_free (self->priv->search_term);
	g_strfreev (self->priv->categories);
	G_OBJECT_CLASS (as_search_query_parent_class)->finalize (obj);
}


/**
 * Class describing a query on the AppStream component database
 */
GType as_search_query_get_type (void) {
	static volatile gsize as_search_query_type_id__volatile = 0;
	if (g_once_init_enter (&as_search_query_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = {
					sizeof (AsSearchQueryClass),
					(GBaseInitFunc) NULL,
					(GBaseFinalizeFunc) NULL,
					(GClassInitFunc) as_search_query_class_init,
					(GClassFinalizeFunc) NULL,
					NULL,
					sizeof (AsSearchQuery),
					0,
					(GInstanceInitFunc) as_search_query_instance_init,
					NULL
		};
		GType as_search_query_type_id;
		as_search_query_type_id = g_type_register_static (G_TYPE_OBJECT, "AsSearchQuery", &g_define_type_info, 0);
		g_once_init_leave (&as_search_query_type_id__volatile, as_search_query_type_id);
	}
	return as_search_query_type_id__volatile;
}


static void as_search_query_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	AsSearchQuery * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_SEARCH_QUERY, AsSearchQuery);
	switch (property_id) {
		case AS_SEARCH_QUERY_SEARCH_TERM:
			g_value_set_string (value, as_search_query_get_search_term (self));
			break;
		case AS_SEARCH_QUERY_CATEGORIES:
			g_value_set_boxed (value, as_search_query_get_categories (self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void as_search_query_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	AsSearchQuery * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (object, AS_TYPE_SEARCH_QUERY, AsSearchQuery);
	switch (property_id) {
		case AS_SEARCH_QUERY_SEARCH_TERM:
			as_search_query_set_search_term (self, g_value_get_string (value));
			break;
		case AS_SEARCH_QUERY_CATEGORIES:
			as_search_query_set_categories (self, g_value_get_boxed (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}
