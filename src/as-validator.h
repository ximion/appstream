/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (__APPSTREAM_H) && !defined (AS_COMPILATION)
#error "Only <appstream.h> can be included directly."
#endif

#ifndef __AS_VALIDATOR_H
#define __AS_VALIDATOR_H

#include <glib-object.h>

#define AS_TYPE_VALIDATOR			(as_validator_get_type())
#define AS_VALIDATOR(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_VALIDATOR, AsValidator))
#define AS_VALIDATOR_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_VALIDATOR, AsValidatorClass))
#define AS_IS_VALIDATOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_VALIDATOR))
#define AS_IS_VALIDATOR_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_VALIDATOR))
#define AS_VALIDATOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_VALIDATOR, AsValidatorClass))

G_BEGIN_DECLS

typedef struct _AsValidator	AsValidator;
typedef struct _AsValidatorClass	AsValidatorClass;

struct _AsValidator
{
	GObject			parent;
};

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
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

/**
 * AsIssueImportance:
 * @AS_ISSUE_IMPORTANCE_ERROR:			There is a serious error in your metadata
 * @AS_ISSUE_IMPORTANCE_WARNING:		Something which should be fixed, but is not fatal
 * @AS_ISSUE_IMPORTANCE_INFO:			Non-essential information on how to improve your metadata
 * @AS_ISSUE_IMPORTANCE_PEDANTIC:		Pedantic information
 *
 * The importance of an issue found by #AsValidator
 **/
typedef enum {
	AS_ISSUE_IMPORTANCE_ERROR,
	AS_ISSUE_IMPORTANCE_WARNING,
	AS_ISSUE_IMPORTANCE_INFO,
	AS_ISSUE_IMPORTANCE_PEDANTIC,
	/*< private >*/
	AS_ISSUE_IMPORTANCE_LAST
} AsIssueImportance;

GType		 as_validator_get_type		(void);
AsValidator	*as_validator_new			(void);

void		as_validator_clear_issues	(AsValidator *validator);
gboolean	as_validator_validate_file (AsValidator *validator,
										GFile* metadata_file);
gboolean	as_validator_validate_data (AsValidator *validator,
										const gchar *metadata);

G_END_DECLS

#endif /* __AS_VALIDATOR_H */
