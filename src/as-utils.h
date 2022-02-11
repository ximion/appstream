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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_UTILS_H
#define __AS_UTILS_H

#include <glib-object.h>
#include "as-component.h"

G_BEGIN_DECLS

/**
 * AsUtilsError:
 * @AS_UTILS_ERROR_FAILED:			Generic failure
 *
 * The error type.
 **/
typedef enum {
	AS_UTILS_ERROR_FAILED,
	/*< private >*/
	AS_UTILS_ERROR_LAST
} AsUtilsError;

#define	AS_UTILS_ERROR				as_utils_error_quark ()

/**
 * AsDataIdMatchFlags:
 * @AS_DATA_ID_MATCH_FLAG_NONE:		No flags set
 * @AS_DATA_ID_MATCH_FLAG_SCOPE:	Scope, e.g. a #AsComponentScope
 * @AS_DATA_ID_MATCH_FLAG_BUNDLE_KIND:	Bundle kind, e.g. a #AsBundleKind
 * @AS_DATA_ID_MATCH_FLAG_ORIGIN:	Origin
 * @AS_DATA_ID_MATCH_FLAG_ID:		Component AppStream ID
 * @AS_DATA_ID_MATCH_FLAG_BRANCH:	Branch
 *
 * The flags used when matching unique IDs.
 **/
typedef enum {
	AS_DATA_ID_MATCH_FLAG_NONE		= 0,
	AS_DATA_ID_MATCH_FLAG_SCOPE		= 1 << 0,
	AS_DATA_ID_MATCH_FLAG_BUNDLE_KIND	= 1 << 1,
	AS_DATA_ID_MATCH_FLAG_ORIGIN		= 1 << 2,
	AS_DATA_ID_MATCH_FLAG_ID		= 1 << 3,
	AS_DATA_ID_MATCH_FLAG_BRANCH		= 1 << 4,
	/*< private >*/
	AS_DATA_ID_MATCH_FLAG_LAST
} AsDataIdMatchFlags;

/**
 * AsMetadataLocation:
 * @AS_METADATA_LOCATION_SHARED:	Installed by the vendor, shared
 * @AS_METADATA_LOCATION_STATE:		Installed as metadata into /var/lib, shared
 * @AS_METADATA_LOCATION_CACHE:		Installed as metadata into /var/cache, shared
 * @AS_METADATA_LOCATION_USER:		Installed for the current user
 *
 * The flags used when installing and removing metadata files.
 **/
typedef enum {
	AS_METADATA_LOCATION_SHARED,
	AS_METADATA_LOCATION_STATE,
	AS_METADATA_LOCATION_CACHE,
	AS_METADATA_LOCATION_USER,
	/*< private >*/
	AS_METADATA_LOCATION_LAST
} AsMetadataLocation;

GQuark		 	as_utils_error_quark (void);

gchar 			**as_markup_strsplit_words (const gchar *text,
						    guint line_len);
gchar			*as_markup_convert_simple (const gchar *markup,
						   GError **error);

gboolean		as_utils_locale_is_compatible (const gchar *locale1,
						       const gchar *locale2);
gboolean		as_utils_is_category_name (const gchar *category_name);
gboolean		as_utils_is_tld (const gchar *tld);
gboolean		as_utils_is_desktop_environment (const gchar *desktop);

void			as_utils_sort_components_into_categories (GPtrArray *cpts,
								  GPtrArray *categories,
								  gboolean check_duplicates);

gchar			*as_utils_build_data_id (AsComponentScope scope,
						 AsBundleKind bundle_kind,
						 const gchar *origin,
						 const gchar *cid,
						 const gchar *branch);
gboolean		as_utils_data_id_valid (const gchar *data_id);
gchar			*as_utils_data_id_get_cid (const gchar *data_id);

gboolean		as_utils_data_id_match (const gchar *data_id1,
						const gchar *data_id2,
						AsDataIdMatchFlags match_flags);
gboolean		as_utils_data_id_equal (const gchar *data_id1,
						const gchar *data_id2);
guint			as_utils_data_id_hash (const gchar *data_id);

guint			as_gstring_replace (GString *string,
					    const gchar *find,
					    const gchar *replace);
guint			as_gstring_replace2 (GString *string,
					     const gchar *find,
					     const gchar *replace,
					     guint limit);

gboolean		as_utils_is_platform_triplet (const gchar *triplet);

gboolean		as_utils_install_metadata_file (AsMetadataLocation location,
							const gchar *filename,
							const gchar *origin,
							const gchar *destdir,
							GError **error);

AsComponentScope	as_utils_guess_scope_from_path (const gchar *path);


/* DEPRECATED */

G_DEPRECATED_FOR(as_version_string)
const gchar		*as_get_appstream_version (void);

G_END_DECLS

#endif /* __AS_UTILS_H */
