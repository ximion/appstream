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

/**
 * SECTION:asc-utils-metainfo
 * @short_description: Helper functions for working with MetaInfo data in appstream-compose.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-utils-metainfo.h"

#include "as-utils-private.h"
#include "as-spdx.h"
#include "as-desktop-entry.h"

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
 * asc_process_metainfo_releases:
 * @cres: an #AscResult instance.
 * @unit: an #AscUnit where release information could be read from.
 * @cpt: the #AsComponent to read release data for.
 * @mi_filename: the metadata filename for the component, as found in the passed unit.
 * @allow_net: set to %TRUE if network access should be allowed.
 * @acurl: the #AsCurl to use for network access.
 * @used_reldata: (out) (optional): Receive the release data that was used, if set.
 *
 * Reads an external release description file, either from a network location or from
 * the given unit. Also performs further processing on the release information, like sorting
 * releases or pruning old ones.
 **/
void
asc_process_metainfo_releases (AscResult *cres,
			       AscUnit *unit,
			       AsComponent *cpt,
			       const gchar *mi_filename,
			       gboolean allow_net,
			       AsCurl *acurl,
			       GBytes **used_reldata)
{
	g_autoptr(GError) local_error = NULL;
	g_autoptr(GBytes) relmd_bytes = NULL;

	/* download external release metadata or fetch local release data */
	if (as_component_get_releases_kind (cpt) == AS_RELEASES_KIND_EXTERNAL) {
		g_autofree gchar *relmd_uri = NULL;

		g_ptr_array_set_size (as_component_get_releases (cpt), 0);
		if (allow_net && as_component_get_releases_url (cpt) != NULL) {
			/* get the release data from a network location */
			const gchar *releases_url = as_component_get_releases_url (cpt);

			relmd_bytes = as_curl_download_bytes (acurl, releases_url, &local_error);
			if (relmd_bytes == NULL) {
				asc_result_add_hint (cres, NULL,
						     "metainfo-releases-download-failed",
						     "url", releases_url,
						     "msg", local_error->message,
						     NULL);
				goto out;
			}
			relmd_uri = g_strdup (releases_url);
		} else {
			/* we need to find local release information */
			g_autofree gchar *relfile_path = NULL;
			g_autofree gchar *relfile_name = NULL;
			g_autofree gchar *tmp = NULL;

			relfile_name = g_strconcat (as_component_get_id (cpt), ".releases.xml", NULL);
			tmp = g_path_get_dirname (mi_filename);
			relfile_path = g_build_filename (tmp, "releases", relfile_name, NULL);

			relmd_bytes = asc_unit_read_data (unit, relfile_path, &local_error);
			if (relmd_bytes == NULL) {
				asc_result_add_hint (cres, NULL,
						     "file-read-error",
						     "fname", relfile_path,
						     "msg", local_error->message,
						     NULL);
				goto out;
			}
			relmd_uri = g_steal_pointer (&relfile_path);
		}

		if (!as_component_load_releases_from_bytes (cpt, relmd_bytes, &local_error)) {
			asc_result_add_hint (cres, NULL,
						"metainfo-releases-read-failed",
						"path", relmd_uri,
						"msg", local_error->message,
						NULL);
			goto out;
		}
	}

	/* limit the amount of releases that we add to the output metadata.
	 * since releases are sorted with the newest one at the top, we will only
	 * remove the older ones. */
	if (as_component_get_kind (cpt) != AS_COMPONENT_KIND_OPERATING_SYSTEM) {
		GPtrArray *releases = as_component_get_releases (cpt);
		if (releases->len > MAX_RELEASE_INFO_COUNT)
			g_ptr_array_set_size (releases, MAX_RELEASE_INFO_COUNT);
	}

out:
	if (used_reldata != NULL)
		*used_reldata = g_steal_pointer (&relmd_bytes);
}

/**
 * asc_validate_metainfo_data_for_component:
 * @cres: an #AscResult instance.
 * @validator: an #AsValidator to validate with.
 * @cpt: the loaded #AsComponent which we are validating
 * @bytes: the data @cpt was constructed from.
 * @mi_basename: the basename of the MetaInfo file we are analyzing.
 * @relmd_bytes: (nullable): the release metadata for this component.
 *
 * Validates MetaInfo data for the given component and stores the validation result as issue hints
 * in the given #AscResult.
 * Both the result as well as the validator's state may be modified by this function.
 **/
void
asc_validate_metainfo_data_for_component (AscResult *cres,
					  AsValidator *validator,
					  AsComponent *cpt,
					  GBytes *bytes,
					  const gchar *mi_basename,
					  GBytes *relmd_bytes)
{
	GHashTable *issues_files;
	GHashTableIter hiter;
	gpointer hkey, hvalue;

	/* don't check web URLs for validity, we catch those issues differently */
	as_validator_set_check_urls (validator, FALSE);

	/* remove issues from a potential previous use of this validator */
	as_validator_clear_issues (validator);
	as_validator_clear_release_data (validator);

	/* add release data if we have any */
	if (relmd_bytes != NULL) {
		g_autoptr(GError) tmp_error = NULL;
		g_autofree gchar *release_name = g_strconcat (as_component_get_id (cpt), ".releases.xml", NULL);
		if (!as_validator_add_release_bytes (validator,
						     release_name,
						     relmd_bytes,
						     &tmp_error))
			g_warning ("Failed to add release metadata for %s: %s",
				   as_component_get_id (cpt), tmp_error->message);
	}

	/* validate */
	as_validator_validate_bytes (validator, bytes);

	/* convert & register found issues */
	issues_files = as_validator_get_issues_per_file (validator);
	g_hash_table_iter_init (&hiter, issues_files);
	while (g_hash_table_iter_next (&hiter, &hkey, &hvalue)) {
		const gchar *filename = (const gchar*) hkey;
		const GPtrArray *issues = (const GPtrArray*) hvalue;

		if (filename == NULL)
			filename = mi_basename;

		for (guint i = 0; i < issues->len; i++) {
			g_autofree gchar *asv_tag = NULL;
			g_autofree gchar *location = NULL;
			glong line;
			const gchar *issue_hint;
			AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (g_ptr_array_index (issues, i));

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
				location = g_strdup_printf ("%s:%ld", filename, line);
			else
				location = g_strdup (filename);

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
}

/**
 * asc_parse_desktop_entry_data:
 * @cres: an #AscResult instance.
 * @cpt: (nullable): an existing #AsComponent which we are adding data from this desktop file to, or NULL to create a new one
 * @bytes: the data @cpt was constructed from.
 * @de_basename: the basename of the desktop-entry file we are parsing.
 * @ignore_nodisplay: set %TRUE if fields which cause the desktop-entry file to be purposefully hidden should be ignored (useful when passing an existing #AsComponent).
 * @fversion: AppStream format version
 * @de_l10n_fn: (scope call): callback for custom desktop-entry field localization functions.
 * @user_data: user data for @de_l10n_fn
 *
 * Parse a XDG desktop-entry file into either a new component or for extending an existing component.
 *
 * Returns: (nullable) (transfer full): A new #AsComponent or reference to the existing #AsComponent passed as parameter in case
 *                                      we parsed anything. %NULL if we refused to accept this data.
 */
AsComponent*
asc_parse_desktop_entry_data (AscResult *cres,
			      AsComponent *cpt,
			      GBytes *bytes, const gchar *de_basename,
			      gboolean ignore_nodisplay,
			      AsFormatVersion fversion,
			      AscTranslateDesktopTextFn de_l10n_fn, gpointer user_data)
{
	g_autoptr(AsComponent) ncpt = NULL;
	g_autoptr(GPtrArray) issues = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *prev_cid = NULL;
	gboolean cpt_is_new = FALSE;
	const gchar *data;
	gsize data_len;
	gboolean ret;


	if (cpt != NULL) {
		ncpt = g_object_ref (cpt);
		prev_cid = g_strdup (as_component_get_id (cpt));
		as_component_set_id (ncpt, de_basename);
		cpt_is_new = FALSE;
	} else {
		ncpt = as_component_new ();
		as_component_set_id (ncpt, de_basename);
		cpt_is_new = TRUE;
	}
	/* we shouldn't even try to use cpt from this point forward */
	cpt = NULL;

	/* prepare */
	issues = g_ptr_array_new_with_free_func (g_object_unref);
	data = g_bytes_get_data (bytes, &data_len);

	/* sanity check */
	if (data_len == 0) {
		asc_result_add_hint_by_cid (cres, de_basename,
					    "desktop-file-error",
					    "msg", "Desktop file was empty or nonexistent. "
						   "Please ensure that it isn't a symbolic link to contents of a different package.",
					    NULL);
		return NULL;
	}

	/* actually parse the desktop-entry data now */
	ret = as_desktop_entry_parse_data (ncpt,
					   data,
					   data_len,
					   fversion,
					   ignore_nodisplay,
					   issues,
					   de_l10n_fn,
					   user_data,
					   &error);
	if (error != NULL) {
		asc_result_add_hint_by_cid (cres, de_basename,
					    "desktop-file-error",
					    "msg", error->message,
					    NULL);
		return NULL;
	}
	if (!ret)
		return NULL;

	if (!ignore_nodisplay && cpt == NULL) {
		/* If we shouldn't ignore NoDisplay & Co. and have no preexisting component, we
		 * will also skip any kind of settings or console app.
		 * Otherwise we would sythesize a component with usually extremely low-quality
		 * metadata for it (and additionally flood the software catalog with tons of settings apps
		 * for various DEs). This means any kind of settings app needs a metainfo file in order to be
		 * added to the software catalogs. */
		GPtrArray *categories = as_component_get_categories (ncpt);
		for (guint i = 0; i < categories->len; i++) {
			const gchar *tmp = g_ptr_array_index (categories, i);
			if (g_strcmp0 (tmp, "Settings") == 0)
				return NULL;
			if (g_strcmp0 (tmp, "DesktopSettings") == 0)
				return NULL;
			if (g_strcmp0 (tmp, "ConsoleOnly") == 0)
				return NULL;
		}
	}

	/* the desktop-entry parsing function may have overridden this, so we reset the original value if we had one */
	if (prev_cid != NULL)
		as_component_set_id (ncpt, prev_cid);

	/* reset component priority to neutral (may be lowest) */
	as_component_set_priority (ncpt, 0);

	/* add new component to the results set */
	if (cpt_is_new && !asc_result_add_component (cres, ncpt, bytes, NULL))
		return NULL;

	/* register all the hints we may have found */
	for (guint i = 0; i < issues->len; i++) {
		AsValidatorIssue *issue = AS_VALIDATOR_ISSUE (g_ptr_array_index (issues, i));
		g_autofree gchar *asv_tag = NULL;
		const gchar *issue_hint = NULL;
		const gchar *orig_tag = NULL;

		orig_tag = as_validator_issue_get_tag (issue);
		if ((g_strcmp0 (orig_tag, "desktop-entry-hidden-set") == 0) ||
		    (g_strcmp0 (orig_tag, "desktop-entry-empty-onlyshowin") == 0)) {
			/* these tags get special treatment with links & co */
			asv_tag = g_strdup (orig_tag);
		} else {
			asv_tag = g_strconcat ("asv-",
						orig_tag,
						NULL);
		}
		issue_hint = as_validator_issue_get_hint (issue);
		if (issue_hint == NULL)
			issue_hint = "";
		asc_result_add_hint (cres, ncpt,
				     asv_tag,
				     "location", de_basename,
				     "hint", issue_hint,
				     NULL);
	}

	return g_steal_pointer (&ncpt);
}
