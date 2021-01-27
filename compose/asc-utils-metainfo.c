/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2021 Matthias Klumpp <matthias@tenstral.net>
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
#include "as-spdx.h"

/* Maximum amount of releases present in output data */
#define MAX_RELEASE_INFO_COUNT 4

/**
 * asc_parse_metainfo_data:
 * @cres: an #AscResult instance.
 * @mdata: an #AsMetadata instance used for parsing.
 * @bytes: the data to parse.
 * @mi_basename: the basename of the MetaInfo file we are loading.
 *
 * Parses metaInfo XML into a new #AsComponent and records any issues in
 * an #AscResult.
 *
 * Returns: (transfer full): A new #AsComponent or %NULL if we refused to accept this data.
 **/
AsComponent*
asc_parse_metainfo_data (AscResult *cres, AsMetadata *mdata, GBytes *bytes, const gchar *mi_basename)
{
	g_autoptr(GError) error = NULL;
	AsComponent *cpt;
	GPtrArray *releases;

	g_return_val_if_fail (mi_basename != NULL, NULL);

	if (!as_metadata_parse_bytes (mdata, bytes, AS_FORMAT_KIND_XML, &error)) {
		asc_result_add_hint (cres, NULL,
				     "ancient-metadata",
				     "fname", mi_basename,
				     "error", error->message,
				     NULL);
		return NULL;
	}

	cpt = as_metadata_get_component (mdata);
	if (cpt == NULL)
		return NULL;

	/* check if we have a component-id, a component without ID is invalid */
	if (as_is_empty (as_component_get_id (cpt))) {
		asc_result_add_hint (cres, NULL,
				     "metainfo-no-id",
				     "fname", mi_basename,
				     NULL);
		return NULL;
	}

	/* we at least read enough data to consider this component */
	if (!asc_result_add_component (cres, cpt, bytes, NULL))
		return NULL;

	/* check if we can actually legally use this metadata */
	if (!as_license_is_metadata_license (as_component_get_metadata_license (cpt))) {
		asc_result_add_hint (cres, cpt,
				     "metainfo-license-invalid",
				     "license", as_component_get_metadata_license (cpt),
				     NULL);
		return NULL;
	}

	/* quit immediately if we have an unknown component type */
	if (as_component_get_kind (cpt) == AS_COMPONENT_KIND_UNKNOWN) {
		asc_result_add_hint_simple (cres, cpt, "metainfo-unknown-type");
		return NULL;
	}

	/* limit the amount of releases that we add to the output metadata.
	 * since releases are sorted with the newest one at the top, we will only
	 * remove the older ones. */
	releases = as_component_get_releases (cpt);
	if (releases->len > MAX_RELEASE_INFO_COUNT)
		g_ptr_array_set_size (releases, MAX_RELEASE_INFO_COUNT);

	return g_object_ref (cpt);
}

/**
 * asc_parse_metainfo_data_simple:
 * @cres: an #AscResult instance.
 * @bytes: the data to parse.
 * @mi_basename: the basename of the MetaInfo file we are loading.
 *
 * Parses metaInfo XML into a new #AsComponent and records any issues in
 * an #AscResult.
 *
 * Returns: (transfer full): A new #AsComponent or %NULL if we refused to accept this data.
 **/
AsComponent*
asc_parse_metainfo_data_simple (AscResult *cres, GBytes *bytes, const gchar *mi_basename)
{
	g_autoptr(AsMetadata) mdata = as_metadata_new ();

	as_metadata_set_locale (mdata, "ALL");
	as_metadata_set_format_style (mdata, AS_FORMAT_STYLE_METAINFO);

	return asc_parse_metainfo_data (cres,
					mdata,
					bytes,
					mi_basename);
}

/**
 * asc_validate_metainfo_data_for_component:
 * @cres: an #AscResult instance.
 * @validator: an #AsValidator to validate with.
 * @cpt: the loaded #AsComponent which we are validating
 * @bytes: the data @cpt was constructed from.
 * @mi_basename: the basename of the MetaInfo file we are analyzing.
 *
 * Validates MetaInfo data for the given component and stores the validation result as issue hints
 * in the given #AscResult.
 * Both the result as well as the validator's state may be modified by this function.
 **/
void
asc_validate_metainfo_data_for_component (AscResult *cres, AsValidator *validator,
					  AsComponent *cpt, GBytes *bytes, const gchar *mi_basename)
{
	g_autoptr(GList) issues = NULL;

	/* don't check web URLs for validity, we catch those issues differently */
	as_validator_set_check_urls (validator, FALSE);

	/* remove issues from a potential previous use of this validator */
	as_validator_clear_issues (validator);

	/* validate */
	as_validator_validate_bytes (validator, bytes);

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
