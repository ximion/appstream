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

#ifndef __AS_VALIDATOR_ISSUE_H
#define __AS_VALIDATOR_ISSUE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_VALIDATOR_ISSUE (as_validator_issue_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsValidatorIssue, as_validator_issue, AS, VALIDATOR_ISSUE, GObject)

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
 * AsIssueSeverity:
 * @AS_ISSUE_SEVERITY_ERROR:	There is a serious, fatal error in your metadata
 * @AS_ISSUE_SEVERITY_WARNING:	Something metadata issue which should be fixed as soon as possible.
 * @AS_ISSUE_SEVERITY_INFO:	Non-essential information on how to improve metadata, no immediate action needed.
 * @AS_ISSUE_SEVERITY_PEDANTIC:	Pedantic information about ways to improve the data, but could also be ignored.
 *
 * The severity of an issue found by #AsValidator
 **/
typedef enum {
	AS_ISSUE_SEVERITY_UNKNOWN,
	AS_ISSUE_SEVERITY_ERROR,
	AS_ISSUE_SEVERITY_WARNING,
	AS_ISSUE_SEVERITY_INFO,
	AS_ISSUE_SEVERITY_PEDANTIC,
	/*< private >*/
	AS_ISSUE_SEVERITY_LAST
} AsIssueSeverity;

/* DEPRECATED */
#define AsIssueImportance G_DEPRECATED AsIssueSeverity
#define AS_ISSUE_IMPORTANCE_UNKNOWN AS_ISSUE_SEVERITY_UNKNOWN
#define AS_ISSUE_IMPORTANCE_ERROR AS_ISSUE_SEVERITY_ERROR
#define AS_ISSUE_IMPORTANCE_WARNING AS_ISSUE_SEVERITY_WARNING
#define AS_ISSUE_IMPORTANCE_INFO AS_ISSUE_SEVERITY_INFO
#define AS_ISSUE_IMPORTANCE_PEDANTIC AS_ISSUE_SEVERITY_PEDANTIC

AsIssueSeverity	 as_issue_severity_from_string (const gchar *str);
const gchar	*as_issue_severity_to_string (AsIssueSeverity severity);

AsValidatorIssue	*as_validator_issue_new (void);

const gchar		*as_validator_issue_get_tag (AsValidatorIssue *issue);
void			as_validator_issue_set_tag (AsValidatorIssue *issue,
							const gchar *tag);

AsIssueSeverity		as_validator_issue_get_severity (AsValidatorIssue *issue);
void 			as_validator_issue_set_severity (AsValidatorIssue *issue,
								AsIssueSeverity severity);

const gchar		*as_validator_issue_get_hint (AsValidatorIssue *issue);
void			as_validator_issue_set_hint (AsValidatorIssue *issue,
							const gchar *hint);

const gchar		*as_validator_issue_get_explanation (AsValidatorIssue *issue);
void			as_validator_issue_set_explanation (AsValidatorIssue *issue,
								const gchar *explanation);

const gchar		*as_validator_issue_get_cid (AsValidatorIssue *issue);
void			as_validator_issue_set_cid (AsValidatorIssue *issue,
						    const gchar *cid);

const gchar		*as_validator_issue_get_filename (AsValidatorIssue *issue);
void			as_validator_issue_set_filename (AsValidatorIssue *issue,
							 const gchar *fname);

glong			as_validator_issue_get_line (AsValidatorIssue *issue);
void			as_validator_issue_set_line (AsValidatorIssue *issue,
						     glong line);

gchar			*as_validator_issue_get_location (AsValidatorIssue *issue);

/* DEPRECATED */

G_DEPRECATED
AsIssueSeverity		as_validator_issue_get_importance (AsValidatorIssue *issue);
G_DEPRECATED
void 			as_validator_issue_set_importance (AsValidatorIssue *issue,
								AsIssueSeverity importance);

G_DEPRECATED
const gchar		*as_validator_issue_get_message (AsValidatorIssue *issue);
G_DEPRECATED
void			as_validator_issue_set_message (AsValidatorIssue *issue,
							const gchar *message);

G_END_DECLS

#endif /* __AS_VALIDATOR_ISSUE_H */
