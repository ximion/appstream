/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_VALIDATOR_ISSUE_H
#define __AS_VALIDATOR_ISSUE_H

#include <glib-object.h>

#define AS_TYPE_VALIDATOR_ISSUE			(as_validator_issue_get_type())
#define AS_VALIDATOR_ISSUE(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_VALIDATOR_ISSUE, AsValidatorIssue))
#define AS_VALIDATOR_ISSUE_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_VALIDATOR_ISSUE, AsValidatorIssueClass))
#define AS_IS_VALIDATOR_ISSUE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_VALIDATOR_ISSUE))
#define AS_IS_VALIDATOR_ISSUE_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_VALIDATOR_ISSUE))
#define AS_VALIDATOR_ISSUE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_VALIDATOR_ISSUE, AsValidatorIssueClass))

G_BEGIN_DECLS

typedef struct _AsValidatorIssue		AsValidatorIssue;
typedef struct _AsValidatorIssueClass		AsValidatorIssueClass;

struct _AsValidatorIssue
{
	GObject			parent;
};

struct _AsValidatorIssueClass
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
 * AsIssueImportance:
 * @AS_ISSUE_IMPORTANCE_ERROR:		There is a serious error in your metadata
 * @AS_ISSUE_IMPORTANCE_WARNING:	Something which should be fixed, but is not fatal
 * @AS_ISSUE_IMPORTANCE_INFO:		Non-essential information on how to improve your metadata
 * @AS_ISSUE_IMPORTANCE_PEDANTIC:	Pedantic information
 *
 * The importance of an issue found by #AsValidator
 **/
typedef enum {
	AS_ISSUE_IMPORTANCE_UNKNOWN,
	AS_ISSUE_IMPORTANCE_ERROR,
	AS_ISSUE_IMPORTANCE_WARNING,
	AS_ISSUE_IMPORTANCE_INFO,
	AS_ISSUE_IMPORTANCE_PEDANTIC,
	/*< private >*/
	AS_ISSUE_IMPORTANCE_LAST
} AsIssueImportance;

/**
 * AsIssueKind:
 * @AS_ISSUE_KIND_UNKNOWN:		Type invalid or not known
 * @AS_ISSUE_KIND_MARKUP_INVALID:	The XML markup is invalid
 * @AS_ISSUE_KIND_LEGACY:		An element from a legacy AppStream specification has been found
 * @AS_ISSUE_KIND_TAG_DUPLICATED:	A tag is duplicated
 * @AS_ISSUE_KIND_TAG_MISSING:		A required tag is missing
 * @AS_ISSUE_KIND_TAG_UNKNOWN:		An unknown tag was found
 * @AS_ISSUE_KIND_TAG_NOT_ALLOWED:	A tag is not allowed in the current context
 * @AS_ISSUE_KIND_PROPERTY_MISSING:	A required property is missing
 * @AS_ISSUE_KIND_PROPERTY_INVALID:	A property is invalid
 * @AS_ISSUE_KIND_VALUE_WRONG:		The value of a tag or property is wrong
 * @AS_ISSUE_KIND_VALUE_ISSUE:		There is an issue with a tag or property value (often non-fatal)
 * @AS_ISSUE_KIND_FILE_MISSING:		A required file or other metadata was missing.
 *
 * The issue type.
 **/
typedef enum {
	AS_ISSUE_KIND_UNKNOWN,
	AS_ISSUE_KIND_MARKUP_INVALID,
	AS_ISSUE_KIND_LEGACY,
	AS_ISSUE_KIND_TAG_DUPLICATED,
	AS_ISSUE_KIND_TAG_MISSING,
	AS_ISSUE_KIND_TAG_UNKNOWN,
	AS_ISSUE_KIND_TAG_NOT_ALLOWED,
	AS_ISSUE_KIND_PROPERTY_MISSING,
	AS_ISSUE_KIND_PROPERTY_INVALID,
	AS_ISSUE_KIND_VALUE_WRONG,
	AS_ISSUE_KIND_VALUE_ISSUE,
	AS_ISSUE_KIND_FILE_MISSING,
	/*< private >*/
	AS_ISSUE_KIND_LAST
} AsIssueKind;

GType		 	as_validator_issue_get_type (void);
AsValidatorIssue	*as_validator_issue_new (void);

AsIssueKind		as_validator_issue_get_kind (AsValidatorIssue *issue);
void			as_validator_issue_set_kind (AsValidatorIssue *issue,
							AsIssueKind kind);

AsIssueImportance	as_validator_issue_get_importance (AsValidatorIssue *issue);
void 			as_validator_issue_set_importance (AsValidatorIssue *issue,
								AsIssueImportance importance);

const gchar		*as_validator_issue_get_message (AsValidatorIssue *issue);
void			as_validator_issue_set_message (AsValidatorIssue *issue,
								const gchar *message);

const gchar		*as_validator_issue_get_location (AsValidatorIssue *issue);
void			as_validator_issue_set_location (AsValidatorIssue *issue,
								const gchar *location);


G_END_DECLS

#endif /* __AS_VALIDATOR_ISSUE_H */
