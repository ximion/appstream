/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_ISSUE_H
#define __AS_ISSUE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_ISSUE (as_issue_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsIssue, as_issue, AS, ISSUE, GObject)

struct _AsIssueClass
{
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
 * AsIssueKind:
 * @AS_ISSUE_KIND_UNKNOWN:	Unknown issue type
 * @AS_ISSUE_KIND_GENERIC:	Generic issue type
 * @AS_ISSUE_KIND_CVE:		Common Vulnerabilities and Exposures issue
 *
 * Checksums supported by #AsRelease
 **/
typedef enum  {
	AS_ISSUE_KIND_UNKNOWN,
	AS_ISSUE_KIND_GENERIC,
	AS_ISSUE_KIND_CVE,
	/*< private >*/
	AS_ISSUE_KIND_LAST
} AsIssueKind;

const gchar		*as_issue_kind_to_string (AsIssueKind kind);
AsIssueKind		as_issue_kind_from_string (const gchar *kind_str);

AsIssue			*as_issue_new (void);

AsIssueKind		as_issue_get_kind (AsIssue *issue);
void			as_issue_set_kind (AsIssue *issue,
					   AsIssueKind kind);

const gchar		*as_issue_get_id (AsIssue *issue);
void			as_issue_set_id (AsIssue *issue,
					 const gchar *id);

const gchar		*as_issue_get_url (AsIssue *issue);
void			as_issue_set_url (AsIssue *issue,
					  const gchar *url);

G_END_DECLS

#endif /* __AS_ISSUE_H */
