/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:asc-utils-metainfo
 * @short_description: Helper functions for working with MetaInfo data in appstream-compose.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-result.h"

#include "as-utils-private.h"


/**
 * asc_validate_metainfo_data_for_component:
 * @cres: an #AscResult instance.
 * @validator: an #AsValidator to validate with.
 * @cpt: the loaded #AsComponent which we are validating
 * @data: the data @cpt was constructed from.
 * @mi_basename: the basename of the metainfo file we are analyzing.
 *
 * Validates metainfo data for the given component and stores the validation result as issue hints
 * in the given #AscResult.
 * Both the result as well as the validator's state may be modified by this function.
 **/
void
asc_validate_metainfo_data_for_component (AscResult *cres, AsValidator *validator,
					  AsComponent *cpt, GBytes *data, const gchar *mi_basename)
{
	g_autoptr(GList) issues = NULL;

	/* don't check web URLs for validity, we catch those issues differently */
	as_validator_set_check_urls (validator, FALSE);

	/* remove issues from a potential previous use of this validator */
	as_validator_clear_issues (validator);

	/* validate */
	as_validator_validate_bytes (validator, data);

	/* convert & register found issues */
	issues = as_validator_get_issues (validator);
	for (GList *l = issues; l != NULL; l = l->next) {
		g_autofree gchar *asv_tag = NULL;
		g_autofree gchar *location = NULL;
		glong line;
		const gchar *issue_hint;
		AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (l->data);

		/* we have a special hint tag for legacy metadata,
		 * with its proper "error" priority */
		if (g_strcmp0 (as_validator_issue_get_tag (issue), "metainfo-ancient") == 0) {
			asc_result_add_hint_simple (cres, cpt, "ancient-metadata");
			continue;
		}

		/* create a tag for asgen out of the AppStream validator tag by prefixing it */
		asv_tag = g_strconcat ("asv-",
				       as_validator_issue_get_tag (issue),
				       NULL);

		line = as_validator_issue_get_line (issue);
		if (line >= 0)
			location = g_strdup_printf ("%s:%ld", mi_basename, line);
		else
			location = g_strdup (mi_basename);

		/* we don't need to do much here, with the tag generated here,
		 * the hint registry will automatically assign the right explanation
		 * text and severity to the issue. */
		issue_hint = as_validator_issue_get_hint (issue);
		if (issue_hint == NULL)
			issue_hint = "";
		asc_result_add_hint (cres, cpt,
				     asv_tag,
				     "location", location,
				     "hint", issue_hint,
				     NULL);
	}
}
