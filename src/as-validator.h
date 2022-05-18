/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_VALIDATOR_H
#define __AS_VALIDATOR_H

#include <glib-object.h>
#include "as-validator-issue.h"

G_BEGIN_DECLS

#define AS_TYPE_VALIDATOR (as_validator_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsValidator, as_validator, AS, VALIDATOR, GObject)

struct _AsValidatorClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * AsValidatorError:
 * @AS_VALIDATOR_ERROR_FAILED:			Generic failure
 * @AS_VALIDATOR_ERROR_OVERRIDE_INVALID:	The issue override was not accepted.
 *
 * The error type.
 **/
typedef enum {
	AS_VALIDATOR_ERROR_FAILED,
	AS_VALIDATOR_ERROR_OVERRIDE_INVALID,
	/*< private >*/
	AS_VALIDATOR_ERROR_LAST
} AsValidatorError;

#define	AS_VALIDATOR_ERROR	as_validator_error_quark ()
GQuark		 	as_validator_error_quark (void);

AsValidator		*as_validator_new (void);

void			as_validator_clear_issues (AsValidator *validator);
gboolean		as_validator_validate_file (AsValidator *validator,
							GFile* metadata_file);
gboolean		as_validator_validate_bytes (AsValidator *validator,
							GBytes *metadata);
gboolean		as_validator_validate_data (AsValidator *validator,
							const gchar *metadata);
gboolean		as_validator_validate_tree (AsValidator *validator,
							const gchar *root_dir);

GList			*as_validator_get_issues (AsValidator *validator);
GHashTable		*as_validator_get_issues_per_file (AsValidator *validator);
gboolean		as_validator_get_report_yaml (AsValidator *validator,
							gchar **yaml_report);

gboolean		as_validator_get_check_urls (AsValidator *validator);
void			as_validator_set_check_urls (AsValidator *validator,
							gboolean value);

gboolean		as_validator_get_strict (AsValidator *validator);
void			as_validator_set_strict (AsValidator *validator,
							gboolean is_strict);

gboolean		as_validator_add_override (AsValidator *validator,
						   const gchar *tag,
						   AsIssueSeverity severity_override,
						   GError **error);

const gchar		*as_validator_get_tag_explanation (AsValidator *validator,
								const gchar *tag);
AsIssueSeverity		as_validator_get_tag_severity (AsValidator *validator,
							const gchar *tag);
gchar			**as_validator_get_tags (AsValidator *validator);

G_END_DECLS

#endif /* __AS_VALIDATOR_H */
