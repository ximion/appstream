/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (__APPSTREAM_COMPOSE_H) && !defined (ASC_COMPILATION)
#error "Only <appstream-compose.h> can be included directly."
#endif
#pragma once

#include <glib-object.h>
#include <appstream.h>

#include "asc-unit.h"
#include "asc-result.h"
#include "asc-icon-policy.h"

G_BEGIN_DECLS

#define ASC_TYPE_COMPOSE (asc_compose_get_type ())
G_DECLARE_DERIVABLE_TYPE (AscCompose, asc_compose, ASC, COMPOSE, GObject)

struct _AscComposeClass
{
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
};

/**
 * AsCacheFlags:
 * @ASC_COMPOSE_FLAG_NONE:			No flags.
 * @ASC_COMPOSE_FLAG_USE_THREADS:		Use threads when possible.
 * @ASC_COMPOSE_FLAG_ALLOW_NET:			Allow network access for downloading extra data.
 * @ASC_COMPOSE_FLAG_VALIDATE:			Validate metadata while processing.
 * @ASC_COMPOSE_FLAG_STORE_SCREENSHOTS:		Whether screenshots should be cached in the media directory.
 * @ASC_COMPOSE_FLAG_ALLOW_SCREENCASTS:		Handle & store video screenshots
 * @ASC_COMPOSE_FLAG_PROCESS_FONTS:		Set if font components should be processed.
 * @ASC_COMPOSE_FLAG_PROCESS_TRANSLATIONS:	Automatically extract component translation status.
 * @ASC_COMPOSE_FLAG_IGNORE_ICONS:		Any icon information is completely ignored. Useful for later manual icon processing.
 * @ASC_COMPOSE_FLAG_PROCESS_UNPAIRED_DESKTOP:	Process desktop-entry files that do not have a corresponding metainfo file.
 * @ASC_COMPOSE_FLAG_PROPAGATE_CUSTOM:		Whether all custom entries should be passed on to the output, ignoring the allowlist.
 * @ASC_COMPOSE_FLAG_PROPAGATE_ARTIFACTS:	Whether artifact data should be passed through to the generated output.
 * @ASC_COMPOSE_FLAG_NO_FINAL_CHECK:		Disable the automatic finalization check to perform it manually at a later time.
 *
 * Flags that affect the compose process.
 **/
typedef enum {
	ASC_COMPOSE_FLAG_NONE				= 0,
	ASC_COMPOSE_FLAG_USE_THREADS		 	= 1 << 0,
	ASC_COMPOSE_FLAG_ALLOW_NET			= 1 << 1,
	ASC_COMPOSE_FLAG_VALIDATE			= 1 << 2,
	ASC_COMPOSE_FLAG_STORE_SCREENSHOTS		= 1 << 3,
	ASC_COMPOSE_FLAG_ALLOW_SCREENCASTS		= 1 << 4,
	ASC_COMPOSE_FLAG_PROCESS_FONTS			= 1 << 5,
	ASC_COMPOSE_FLAG_PROCESS_TRANSLATIONS		= 1 << 6,
	ASC_COMPOSE_FLAG_IGNORE_ICONS			= 1 << 7,
	ASC_COMPOSE_FLAG_PROCESS_UNPAIRED_DESKTOP 	= 1 << 8,
	ASC_COMPOSE_FLAG_PROPAGATE_CUSTOM	 	= 1 << 9,
	ASC_COMPOSE_FLAG_PROPAGATE_ARTIFACTS	 	= 1 << 10,
	ASC_COMPOSE_FLAG_NO_FINAL_CHECK		 	= 1 << 11,
} AscComposeFlags;

/**
 * AscCheckMetadataEarlyFn:
 * @cres: (not nullable): A pointer to generated #AscResult
 * @unit: (not nullable): The unit we are currently processing.
 * @user_data: Additional data.
 *
 * Function which is called after all metainfo and related data (e.g. desktop-entry files)
 * has been loaded, but *before* any expensive operations such as screenshot downloads or
 * font searches are performed.
 *
 * This function can be useful to filter out unwanted components early in the process and
 * avoid unneeded downloads and other data processing.
 * By the time this function is called, the component's global ID should be finalized
 * and should not change any longer.
 *
 * Please note that this function may be called from any thread, and thread safety needs
 * to be taked care off by the callee.
 */
typedef void (*AscCheckMetadataEarlyFn)(AscResult *cres,
					const AscUnit *unit,
					gpointer user_data);

/**
 * AscTranslateDesktopTextFn:
 * @de: (not nullable): A pointer to the desktop-entry data we are reading.
 * @text: The string to translate.
 * @user_data: Additional data.
 *
 * Function which is called while parsing a desktop-entry file to allow external
 * translations of string values. This is used in e.g. the Ubuntu distribution.
 *
 * The return value must contain a list of strings with the locale name in even indices,
 * and the text translated to the preceding locale in the following odd indices.
 *
 * Returns: (not nullable) (transfer full): A new #GPtrArray containing the translation mapping.
 */
typedef GPtrArray* (*AscTranslateDesktopTextFn)(const GKeyFile *de,
						const gchar *text,
						gpointer user_data);

AscCompose		*asc_compose_new (void);

void			asc_compose_reset (AscCompose *compose);
void			asc_compose_add_unit (AscCompose *compose,
					      AscUnit *unit);

void			asc_compose_add_allowed_cid (AscCompose *compose,
							const gchar *component_id);

const gchar		*asc_compose_get_prefix (AscCompose *compose);
void			asc_compose_set_prefix (AscCompose *compose,
						const gchar *prefix);

const gchar		*asc_compose_get_origin (AscCompose *compose);
void			asc_compose_set_origin (AscCompose *compose,
						const gchar *origin);

AsFormatKind		asc_compose_get_format (AscCompose *compose);
void			asc_compose_set_format (AscCompose *compose,
						AsFormatKind kind);

const gchar		*asc_compose_get_media_baseurl (AscCompose *compose);
void			asc_compose_set_media_baseurl (AscCompose *compose,
						       const gchar *url);

AscComposeFlags		asc_compose_get_flags (AscCompose *compose);
void			asc_compose_set_flags (AscCompose *compose,
					       AscComposeFlags flags);
void			asc_compose_add_flags (AscCompose *compose,
					       AscComposeFlags flags);
void			asc_compose_remove_flags (AscCompose *compose,
						  AscComposeFlags flags);

AscIconPolicy		*asc_compose_get_icon_policy (AscCompose *compose);
void			asc_compose_set_icon_policy (AscCompose *compose,
							AscIconPolicy *policy);

const gchar		*asc_compose_get_cainfo (AscCompose *compose);
void			asc_compose_set_cainfo (AscCompose *compose,
						const gchar *cainfo);

const gchar		*asc_compose_get_data_result_dir (AscCompose *compose);
void			asc_compose_set_data_result_dir (AscCompose *compose,
							 const gchar *dir);

const gchar		*asc_compose_get_icons_result_dir (AscCompose *compose);
void			asc_compose_set_icons_result_dir (AscCompose *compose,
							  const gchar *dir);

const gchar		*asc_compose_get_media_result_dir (AscCompose *compose);
void			asc_compose_set_media_result_dir (AscCompose *compose,
							  const gchar *dir);

const gchar		*asc_compose_get_hints_result_dir (AscCompose *compose);
void			asc_compose_set_hints_result_dir (AscCompose *compose,
							  const gchar *dir);

void			asc_compose_remove_custom_allowed (AscCompose *compose,
							   const gchar *key_id);
void			asc_compose_add_custom_allowed (AscCompose *compose,
							  const gchar *key_id);

gssize			asc_compose_get_max_screenshot_size (AscCompose *compose);
void			asc_compose_set_max_screenshot_size (AscCompose *compose,
							     gssize size_bytes);

void			asc_compose_set_check_metadata_early_func (AscCompose *compose,
								   AscCheckMetadataEarlyFn func,
								   gpointer user_data);
void			asc_compose_set_desktop_entry_l10n_func (AscCompose *compose,
								 AscTranslateDesktopTextFn func,
								 gpointer user_data);

AscUnit			*asc_compose_get_locale_unit (AscCompose *compose);
void			asc_compose_set_locale_unit (AscCompose *compose,
						     AscUnit *locale_unit);

GPtrArray		*asc_compose_get_results (AscCompose *compose);
GPtrArray		*asc_compose_fetch_components (AscCompose *compose);
gboolean		asc_compose_has_errors (AscCompose *compose);

GPtrArray		*asc_compose_run (AscCompose *compose,
					  GCancellable *cancellable,
					  GError **error);

void			asc_compose_finalize_results (AscCompose *compose);
void			asc_compose_finalize_result (AscCompose *compose,
						     AscResult *result);

G_END_DECLS
