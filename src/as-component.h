/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __AS_COMPONENT_H
#define __AS_COMPONENT_H

#include <glib-object.h>
#include "as-context.h"
#include "as-enum-types.h"
#include "as-provided.h"
#include "as-icon.h"
#include "as-screenshot.h"
#include "as-release-list.h"
#include "as-developer.h"
#include "as-translation.h"
#include "as-suggested.h"
#include "as-category.h"
#include "as-bundle.h"
#include "as-content-rating.h"
#include "as-launchable.h"
#include "as-relation.h"
#include "as-agreement.h"
#include "as-branding.h"
#include "as-review.h"
#include "as-reference.h"

G_BEGIN_DECLS

/* forward declarations */
typedef struct _AsPool	     AsPool;
typedef struct _AsSystemInfo AsSystemInfo;

#define AS_TYPE_COMPONENT (as_component_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsComponent, as_component, AS, COMPONENT, GObject)

struct _AsComponentClass {
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
 * AsComponentKind:
 * @AS_COMPONENT_KIND_UNKNOWN:		Type invalid or not known
 * @AS_COMPONENT_KIND_GENERIC:		A generic (= without specialized type) component
 * @AS_COMPONENT_KIND_DESKTOP_APP:	An application with a .desktop-file
 * @AS_COMPONENT_KIND_CONSOLE_APP:	A console application
 * @AS_COMPONENT_KIND_WEB_APP:		A web application
 * @AS_COMPONENT_KIND_SERVICE:		A system service launched by the init system
 * @AS_COMPONENT_KIND_ADDON:		An extension of existing software, which does not run standalone
 * @AS_COMPONENT_KIND_RUNTIME:		An application runtime platform
 * @AS_COMPONENT_KIND_FONT:		A font
 * @AS_COMPONENT_KIND_CODEC:		A multimedia codec
 * @AS_COMPONENT_KIND_INPUT_METHOD:	An input-method provider
 * @AS_COMPONENT_KIND_OPERATING_SYSTEM: A computer operating system
 * @AS_COMPONENT_KIND_FIRMWARE:		Firmware
 * @AS_COMPONENT_KIND_DRIVER:		A driver
 * @AS_COMPONENT_KIND_LOCALIZATION:	Software localization (usually l10n resources)
 * @AS_COMPONENT_KIND_REPOSITORY:	A remote software or data source
 * @AS_COMPONENT_KIND_ICON_THEME:	An icon theme following the XDG specification
 *
 * The type of an #AsComponent.
 **/
typedef enum {
	AS_COMPONENT_KIND_UNKNOWN,
	AS_COMPONENT_KIND_GENERIC,
	AS_COMPONENT_KIND_DESKTOP_APP,
	AS_COMPONENT_KIND_CONSOLE_APP,
	AS_COMPONENT_KIND_WEB_APP,
	AS_COMPONENT_KIND_SERVICE,
	AS_COMPONENT_KIND_ADDON,
	AS_COMPONENT_KIND_RUNTIME,
	AS_COMPONENT_KIND_FONT,
	AS_COMPONENT_KIND_CODEC,
	AS_COMPONENT_KIND_INPUT_METHOD,
	AS_COMPONENT_KIND_OPERATING_SYSTEM,
	AS_COMPONENT_KIND_FIRMWARE,
	AS_COMPONENT_KIND_DRIVER,
	AS_COMPONENT_KIND_LOCALIZATION,
	AS_COMPONENT_KIND_REPOSITORY,
	AS_COMPONENT_KIND_ICON_THEME,
	/*< private >*/
	AS_COMPONENT_KIND_LAST
} AsComponentKind;

const gchar    *as_component_kind_to_string (AsComponentKind kind);
AsComponentKind as_component_kind_from_string (const gchar *kind_str);

/**
 * AsMergeKind:
 * @AS_MERGE_KIND_NONE:		No merge is happening.
 * @AS_MERGE_KIND_REPLACE:	Merge replacing data of target.
 * @AS_MERGE_KIND_APPEND:	Merge appending data to target.
 * @AS_MERGE_KIND_REMOVE_COMPONENT: Remove the entire component if it matches.
 *
 * Defines how #AsComponent data should be merged if the component is
 * set for merge.
 **/
typedef enum {
	AS_MERGE_KIND_NONE,
	AS_MERGE_KIND_REPLACE,
	AS_MERGE_KIND_APPEND,
	AS_MERGE_KIND_REMOVE_COMPONENT,
	/*< private >*/
	AS_MERGE_KIND_LAST
} AsMergeKind;

const gchar *as_merge_kind_to_string (AsMergeKind kind);
AsMergeKind  as_merge_kind_from_string (const gchar *kind_str);

/**
 * AsComponentScope:
 * @AS_COMPONENT_SCOPE_UNKNOWN:		Unknown scope
 * @AS_COMPONENT_SCOPE_SYSTEM:		System scope
 * @AS_COMPONENT_SCOPE_USER:		User scope
 *
 * Scope of the #AsComponent (system-wide or user-scope)
 **/
typedef enum {
	AS_COMPONENT_SCOPE_UNKNOWN,
	AS_COMPONENT_SCOPE_SYSTEM,
	AS_COMPONENT_SCOPE_USER,
	/*< private >*/
	AS_COMPONENT_SCOPE_LAST
} AsComponentScope;

const gchar	*as_component_scope_to_string (AsComponentScope scope);
AsComponentScope as_component_scope_from_string (const gchar *scope_str);

/**
 * AsUrlKind:
 * @AS_URL_KIND_UNKNOWN:	Type invalid or not known
 * @AS_URL_KIND_HOMEPAGE:	Project homepage
 * @AS_URL_KIND_BUGTRACKER:	Bugtracker
 * @AS_URL_KIND_FAQ:		FAQ page
 * @AS_URL_KIND_HELP:		Help manual
 * @AS_URL_KIND_DONATION:	Page with information about how to donate to the project
 * @AS_URL_KIND_TRANSLATE:	Page with instructions on how to translate the project / submit translations.
 * @AS_URL_KIND_CONTACT:	Contact the developers
 * @AS_URL_KIND_VCS_BROWSER:	Browse the source code
 * @AS_URL_KIND_CONTRIBUTE:	Help developing
 *
 * The URL type.
 **/
typedef enum {
	AS_URL_KIND_UNKNOWN,
	AS_URL_KIND_HOMEPAGE,
	AS_URL_KIND_BUGTRACKER,
	AS_URL_KIND_FAQ,
	AS_URL_KIND_HELP,
	AS_URL_KIND_DONATION,
	AS_URL_KIND_TRANSLATE,
	AS_URL_KIND_CONTACT,
	AS_URL_KIND_VCS_BROWSER,
	AS_URL_KIND_CONTRIBUTE,
	/*< private >*/
	AS_URL_KIND_LAST
} AsUrlKind;

const gchar	*as_url_kind_to_string (AsUrlKind url_kind);
AsUrlKind	 as_url_kind_from_string (const gchar *url_kind);

AsComponent	*as_component_new (void);

AsContext	*as_component_get_context (AsComponent *cpt);
void		 as_component_set_context (AsComponent *cpt, AsContext *context);
void		 as_component_set_context_locale (AsComponent *cpt, const gchar *locale);

const gchar	*as_component_get_id (AsComponent *cpt);
void		 as_component_set_id (AsComponent *cpt, const gchar *value);

const gchar	*as_component_get_data_id (AsComponent *cpt);
void		 as_component_set_data_id (AsComponent *cpt, const gchar *value);

AsComponentKind	 as_component_get_kind (AsComponent *cpt);
void		 as_component_set_kind (AsComponent *cpt, AsComponentKind value);

const gchar	*as_component_get_date_eol (AsComponent *cpt);
void		 as_component_set_date_eol (AsComponent *cpt, const gchar *date);
guint64		 as_component_get_timestamp_eol (AsComponent *cpt);

const gchar	*as_component_get_origin (AsComponent *cpt);
void		 as_component_set_origin (AsComponent *cpt, const gchar *origin);

const gchar	*as_component_get_branch (AsComponent *cpt);
void		 as_component_set_branch (AsComponent *cpt, const gchar *branch);

AsComponentScope as_component_get_scope (AsComponent *cpt);
void		 as_component_set_scope (AsComponent *cpt, AsComponentScope scope);

gchar		*as_component_get_pkgname (AsComponent *cpt);
void		 as_component_set_pkgname (AsComponent *cpt, const gchar *pkgname);
gchar	       **as_component_get_pkgnames (AsComponent *cpt);
void		 as_component_set_pkgnames (AsComponent *cpt, gchar **packages);

const gchar	*as_component_get_source_pkgname (AsComponent *cpt);
void		 as_component_set_source_pkgname (AsComponent *cpt, const gchar *spkgname);

const gchar	*as_component_get_name (AsComponent *cpt);
void		 as_component_set_name (AsComponent *cpt, const gchar *value, const gchar *locale);

const gchar	*as_component_get_summary (AsComponent *cpt);
void	     as_component_set_summary (AsComponent *cpt, const gchar *value, const gchar *locale);

const gchar *as_component_get_description (AsComponent *cpt);
void	   as_component_set_description (AsComponent *cpt, const gchar *value, const gchar *locale);

GPtrArray *as_component_get_launchables (AsComponent *cpt);
AsLaunchable *as_component_get_launchable (AsComponent *cpt, AsLaunchableKind kind);
void	      as_component_add_launchable (AsComponent *cpt, AsLaunchable *launchable);

const gchar  *as_component_get_metadata_license (AsComponent *cpt);
void	      as_component_set_metadata_license (AsComponent *cpt, const gchar *value);

const gchar  *as_component_get_project_license (AsComponent *cpt);
void	      as_component_set_project_license (AsComponent *cpt, const gchar *value);
gboolean      as_component_is_floss (AsComponent *cpt);

const gchar  *as_component_get_project_group (AsComponent *cpt);
void	      as_component_set_project_group (AsComponent *cpt, const gchar *value);

AsDeveloper  *as_component_get_developer (AsComponent *cpt);
void	      as_component_set_developer (AsComponent *cpt, AsDeveloper *developer);

GPtrArray    *as_component_get_compulsory_for_desktops (AsComponent *cpt);
void	      as_component_set_compulsory_for_desktop (AsComponent *cpt, const gchar *desktop);
gboolean      as_component_is_compulsory_for_desktop (AsComponent *cpt, const gchar *desktop);

GPtrArray    *as_component_get_categories (AsComponent *cpt);
void	      as_component_add_category (AsComponent *cpt, const gchar *category);
gboolean      as_component_has_category (AsComponent *cpt, const gchar *category);

GPtrArray    *as_component_get_screenshots_all (AsComponent *cpt);
void	      as_component_add_screenshot (AsComponent *cpt, AsScreenshot *sshot);
void	      as_component_sort_screenshots (AsComponent *cpt,
					     const gchar *environment,
					     const gchar *style,
					     gboolean	  prioritize_style);

GPtrArray    *as_component_get_keywords (AsComponent *cpt);
void	      as_component_set_keywords (AsComponent *cpt,
					 GPtrArray   *new_keywords,
					 const gchar *locale,
					 gboolean     deep_copy);
void	    as_component_add_keyword (AsComponent *cpt, const gchar *keyword, const gchar *locale);
void	    as_component_clear_keywords (AsComponent *cpt, const gchar *locale);

GPtrArray  *as_component_get_icons (AsComponent *cpt);
AsIcon	   *as_component_get_icon_by_size (AsComponent *cpt, guint width, guint height);
AsIcon	   *as_component_get_icon_stock (AsComponent *cpt);
void	    as_component_add_icon (AsComponent *cpt, AsIcon *icon);

GPtrArray  *as_component_get_provided (AsComponent *cpt);
void	    as_component_add_provided (AsComponent *cpt, AsProvided *prov);
AsProvided *as_component_get_provided_for_kind (AsComponent *cpt, AsProvidedKind kind);
void as_component_add_provided_item (AsComponent *cpt, AsProvidedKind kind, const gchar *item);

const gchar   *as_component_get_url (AsComponent *cpt, AsUrlKind url_kind);
void	       as_component_add_url (AsComponent *cpt, AsUrlKind url_kind, const gchar *url);

AsReleaseList *as_component_load_releases (AsComponent *cpt, gboolean allow_net, GError **error);
AsReleaseList *as_component_get_releases_plain (AsComponent *cpt);
void	       as_component_set_releases (AsComponent *cpt, AsReleaseList *releases);
void	       as_component_add_release (AsComponent *cpt, AsRelease *release);

GPtrArray     *as_component_get_extends (AsComponent *cpt);
void	       as_component_add_extends (AsComponent *cpt, const gchar *cpt_id);

GPtrArray     *as_component_get_addons (AsComponent *cpt);
void	       as_component_add_addon (AsComponent *cpt, AsComponent *addon);

GList	      *as_component_get_languages (AsComponent *cpt);
gint	       as_component_get_language (AsComponent *cpt, const gchar *locale);
void	       as_component_add_language (AsComponent *cpt, const gchar *locale, gint percentage);
void	       as_component_clear_languages (AsComponent *cpt);

GPtrArray     *as_component_get_translations (AsComponent *cpt);
void	       as_component_add_translation (AsComponent *cpt, AsTranslation *tr);

gboolean       as_component_has_bundle (AsComponent *cpt);
GPtrArray     *as_component_get_bundles (AsComponent *cpt);
AsBundle      *as_component_get_bundle (AsComponent *cpt, AsBundleKind bundle_kind);
void	       as_component_add_bundle (AsComponent *cpt, AsBundle *bundle);

GPtrArray     *as_component_get_suggested (AsComponent *cpt);
void	       as_component_add_suggested (AsComponent *cpt, AsSuggested *suggested);

GPtrArray     *as_component_get_search_tokens (AsComponent *cpt);
guint	       as_component_search_matches (AsComponent *cpt, const gchar *term);
guint	       as_component_search_matches_all (AsComponent *cpt, gchar **terms);

AsMergeKind    as_component_get_merge_kind (AsComponent *cpt);
void	       as_component_set_merge_kind (AsComponent *cpt, AsMergeKind kind);

gboolean       as_component_is_member_of_category (AsComponent *cpt, AsCategory *category);

gboolean       as_component_is_ignored (AsComponent *cpt);

gboolean       as_component_is_valid (AsComponent *cpt);
gchar	      *as_component_to_string (AsComponent *cpt);

gint	       as_component_get_priority (AsComponent *cpt);
void	       as_component_set_priority (AsComponent *cpt, gint priority);

GHashTable    *as_component_get_custom (AsComponent *cpt);
const gchar   *as_component_get_custom_value (AsComponent *cpt, const gchar *key);
gboolean as_component_insert_custom_value (AsComponent *cpt, const gchar *key, const gchar *value);

GPtrArray	*as_component_get_content_ratings (AsComponent *cpt);
AsContentRating *as_component_get_content_rating (AsComponent *cpt, const gchar *kind);
void	     as_component_add_content_rating (AsComponent *cpt, AsContentRating *content_rating);

GPtrArray   *as_component_get_requires (AsComponent *cpt);
GPtrArray   *as_component_get_recommends (AsComponent *cpt);
GPtrArray   *as_component_get_supports (AsComponent *cpt);
void	     as_component_add_relation (AsComponent *cpt, AsRelation *relation);
GPtrArray   *as_component_check_relations (AsComponent	 *cpt,
					   AsSystemInfo	 *sysinfo,
					   AsPool	 *pool,
					   AsRelationKind rel_kind);
gint	     as_component_get_system_compatibility_score (AsComponent  *cpt,
							  AsSystemInfo *sysinfo,
							  gboolean	is_template,
							  GPtrArray   **results);

GPtrArray   *as_component_get_replaces (AsComponent *cpt);
void	     as_component_add_replaces (AsComponent *cpt, const gchar *cid);

GPtrArray   *as_component_get_agreements (AsComponent *cpt);
void	     as_component_add_agreement (AsComponent *cpt, AsAgreement *agreement);
AsAgreement *as_component_get_agreement_by_kind (AsComponent *cpt, AsAgreementKind kind);

AsBranding  *as_component_get_branding (AsComponent *cpt);
void	     as_component_set_branding (AsComponent *cpt, AsBranding *branding);

void	     as_component_clear_tags (AsComponent *cpt);
gboolean     as_component_add_tag (AsComponent *cpt, const gchar *ns, const gchar *tag);
gboolean     as_component_remove_tag (AsComponent *cpt, const gchar *ns, const gchar *tag);
gboolean     as_component_has_tag (AsComponent *cpt, const gchar *ns, const gchar *tag);

GPtrArray   *as_component_get_references (AsComponent *cpt);
void	     as_component_add_reference (AsComponent *cpt, AsReference *reference);

const gchar *as_component_get_name_variant_suffix (AsComponent *cpt);
void	     as_component_set_name_variant_suffix (AsComponent *cpt,
						   const gchar *value,
						   const gchar *locale);

guint	     as_component_get_sort_score (AsComponent *cpt);
void	     as_component_set_sort_score (AsComponent *cpt, guint score);

GPtrArray   *as_component_get_reviews (AsComponent *cpt);
void	     as_component_add_review (AsComponent *cpt, AsReview *review);

GHashTable  *as_component_get_name_table (AsComponent *cpt);
GHashTable  *as_component_get_summary_table (AsComponent *cpt);
GHashTable  *as_component_get_keywords_table (AsComponent *cpt);

gboolean     as_component_load_from_bytes (AsComponent *cpt,
					   AsContext   *context,
					   AsFormatKind format,
					   GBytes      *bytes,
					   GError     **error);
gchar	    *as_component_to_xml_data (AsComponent *cpt, AsContext *context, GError **error);

G_END_DECLS

#endif /* __AS_COMPONENT_H */
