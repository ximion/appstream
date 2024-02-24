/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined(__APPSTREAM_H) && !defined(AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_SYSTEM_INFO_H
#define __AS_SYSTEM_INFO_H

#include <glib-object.h>

#include "as-relation.h"

G_BEGIN_DECLS

#define AS_TYPE_SYSTEM_INFO (as_system_info_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsSystemInfo, as_system_info, AS, SYSTEM_INFO, GObject)

struct _AsSystemInfoClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_as_reserved1) (void);
	void (*_as_reserved2) (void);
	void (*_as_reserved3) (void);
	void (*_as_reserved4) (void);
	void (*_as_reserved5) (void);
	void (*_as_reserved6) (void);
};

/**
 * AsCheckResult:
 * @AS_CHECK_RESULT_ERROR:		The check failed.
 * @AS_CHECK_RESULT_UNKNOWN:		The outcome could not be determined.
 * @AS_CHECK_RESULT_FALSE:		False/No
 * @AS_CHECK_RESULT_TRUE:		True/Yes
 *
 * Result of a check operation.
 **/
typedef enum {
	AS_CHECK_RESULT_ERROR,
	AS_CHECK_RESULT_UNKNOWN,
	AS_CHECK_RESULT_FALSE,
	AS_CHECK_RESULT_TRUE,
	/*< private >*/
	AS_CHECK_RESULT_LAST
} AsCheckResult;

/**
 * AsChassisKind:
 * @AS_CHASSIS_KIND_UNKNOWN:	Unknown chassis.
 * @AS_CHASSIS_KIND_DESKTOP:	A desktop/workstation
 * @AS_CHASSIS_KIND_LAPTOP:	A mobile computer with a bigger screen / laptop
 * @AS_CHASSIS_KIND_SERVER:	A server, often without GUI
 * @AS_CHASSIS_KIND_TABLET:	A tablet with touch as primary input method
 * @AS_CHASSIS_KIND_HANDSET:	A smaller touch-input device, like a smartphone
 *
 * The type of chassis a computing device is built into.
 **/
typedef enum {
	AS_CHASSIS_KIND_UNKNOWN,
	AS_CHASSIS_KIND_DESKTOP,
	AS_CHASSIS_KIND_LAPTOP,
	AS_CHASSIS_KIND_SERVER,
	AS_CHASSIS_KIND_TABLET,
	AS_CHASSIS_KIND_HANDSET,
	/*< private >*/
	AS_CHASSIS_KIND_LAST
} AsChassisKind;

const gchar  *as_chassis_kind_to_string (AsChassisKind kind);
AsChassisKind as_chassis_kind_from_string (const gchar *kind_str);

/**
 * AsSystemInfoError:
 * @AS_SYSTEM_INFO_ERROR_FAILED:		Generic failure
 * @AS_SYSTEM_INFO_ERROR_NOT_FOUND:		Information was not found.
 *
 * The error type.
 **/
typedef enum {
	AS_SYSTEM_INFO_ERROR_FAILED,
	AS_SYSTEM_INFO_ERROR_NOT_FOUND,
	/*< private >*/
	AS_SYSTEM_INFO_ERROR_LAST
} AsSystemInfoError;

#define AS_SYSTEM_INFO_ERROR as_system_info_error_quark ()

GQuark	      as_system_info_error_quark (void);

AsSystemInfo *as_system_info_new (void);
AsSystemInfo *as_system_info_new_template_for_chassis (AsChassisKind chassis, GError **error);

const gchar  *as_system_info_get_os_id (AsSystemInfo *sysinfo);
const gchar  *as_system_info_get_os_cid (AsSystemInfo *sysinfo);
const gchar  *as_system_info_get_os_name (AsSystemInfo *sysinfo);
const gchar  *as_system_info_get_os_version (AsSystemInfo *sysinfo);
const gchar  *as_system_info_get_os_homepage (AsSystemInfo *sysinfo);

const gchar  *as_system_info_get_kernel_name (AsSystemInfo *sysinfo);
const gchar  *as_system_info_get_kernel_version (AsSystemInfo *sysinfo);

gulong	      as_system_info_get_memory_total (AsSystemInfo *sysinfo);

GPtrArray    *as_system_info_get_modaliases (AsSystemInfo *sysinfo);
const gchar  *as_system_info_modalias_to_syspath (AsSystemInfo *sysinfo, const gchar *modalias);
gboolean      as_system_info_has_device_matching_modalias (AsSystemInfo *sysinfo,
							   const gchar	*modalias_glob);

gchar	     *as_system_info_get_device_name_for_modalias (AsSystemInfo *sysinfo,
							   const gchar	*modalias,
							   gboolean	 allow_fallback,
							   GError      **error);

AsCheckResult as_system_info_has_input_control (AsSystemInfo *sysinfo,
						AsControlKind kind,
						GError	    **error);
void as_system_info_set_input_control (AsSystemInfo *sysinfo, AsControlKind kind, gboolean found);

gboolean as_system_info_get_gui_available (AsSystemInfo *sysinfo);
void	 as_system_info_set_gui_available (AsSystemInfo *sysinfo, gboolean available);

gulong	 as_system_info_get_display_length (AsSystemInfo *sysinfo, AsDisplaySideKind side);
void	 as_system_info_set_display_length (AsSystemInfo     *sysinfo,
					    AsDisplaySideKind side,
					    gulong	      value_dip);

gchar	*as_get_current_distro_component_id (void);

G_END_DECLS

#endif /* __AS_SYSTEM_INFO_H */
