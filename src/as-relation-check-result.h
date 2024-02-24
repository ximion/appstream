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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#include "as-relation.h"

#define AS_TYPE_RELATION_CHECK_RESULT (as_relation_check_result_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsRelationCheckResult,
			  as_relation_check_result,
			  AS,
			  RELATION_CHECK_RESULT,
			  GObject)

struct _AsRelationCheckResultClass {
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
 * AsRelationStatus:
 * @AS_RELATION_STATUS_UNKNOWN:		Unknown status.
 * @AS_RELATION_STATUS_ERROR:		An error occured and the status could not be checked.
 * @AS_RELATION_STATUS_NOT_SATISFIED:	The relation is not satisfied.
 * @AS_RELATION_STATUS_SATISFIED:	The relation is satisfied.
 *
 * Status of a relation check result.
 */
typedef enum {
	AS_RELATION_STATUS_UNKNOWN,
	AS_RELATION_STATUS_ERROR,
	AS_RELATION_STATUS_NOT_SATISFIED,
	AS_RELATION_STATUS_SATISFIED,
	/*< private >*/
	AS_RELATION_STATUS_LAST
} AsRelationStatus;

AsRelationCheckResult *as_relation_check_result_new (void);

AsRelationStatus       as_relation_check_result_get_status (AsRelationCheckResult *relcr);
void as_relation_check_result_set_status (AsRelationCheckResult *relcr, AsRelationStatus status);

AsRelation *as_relation_check_result_get_relation (AsRelationCheckResult *relcr);
void as_relation_check_result_set_relation (AsRelationCheckResult *relcr, AsRelation *relation);

const gchar *as_relation_check_result_get_message (AsRelationCheckResult *relcr);
void as_relation_check_result_set_message (AsRelationCheckResult *relcr, const gchar *format, ...)
    G_GNUC_PRINTF (2, 3);

AsRelationError as_relation_check_result_get_error_code (AsRelationCheckResult *relcr);
void as_relation_check_result_set_error_code (AsRelationCheckResult *relcr, AsRelationError ecode);

gint as_relation_check_results_get_compatibility_score (GPtrArray *rc_results);

G_END_DECLS
