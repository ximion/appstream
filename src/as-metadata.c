/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2015 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-metadata
 * @short_description: Parser for AppStream metadata
 * @include: appstream.h
 *
 * This object parses AppStream metadata, including AppStream
 * upstream metadata, which is defined by upstream projects.
 * It returns an #AsComponent of the data.
 *
 * See also: #AsComponent, #AsDatabase
 */

#include <config.h>
#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <string.h>

#include "as-metadata.h"
#include "as-metadata-private.h"

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-component.h"
#include "as-component-private.h"
#include "as-distro-details.h"

typedef struct _AsMetadataPrivate	AsMetadataPrivate;
struct _AsMetadataPrivate
{
	gchar *locale;
	gchar *locale_short;
	AsParserMode mode;
	gchar *origin_name;

	GPtrArray *cpts;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsMetadata, as_metadata, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_metadata_get_instance_private (o))

static gchar**		as_metadata_get_children_as_strv (AsMetadata *metad, xmlNode* node, const gchar* element_name);

/**
 * as_metadata_finalize:
 **/
static void
as_metadata_finalize (GObject *object)
{
	AsMetadata *metad = AS_METADATA (object);
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	g_free (priv->locale);
	g_free (priv->locale_short);
	g_ptr_array_unref (priv->cpts);
	if (priv->origin_name != NULL)
		g_free (priv->origin_name);

	G_OBJECT_CLASS (as_metadata_parent_class)->finalize (object);
}

/**
 * as_metadata_init:
 **/
static void
as_metadata_init (AsMetadata *metad)
{
	gchar *str;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	/* set active locale without UTF-8 suffix */
	str = as_get_locale ();
	as_metadata_set_locale (metad, str);
	g_free (str);

	priv->origin_name = NULL;
	priv->mode = AS_PARSER_MODE_UPSTREAM;

	priv->cpts = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * as_metadata_clear_components:
 *
 **/
void
as_metadata_clear_components (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_ptr_array_unref (priv->cpts);
	priv->cpts = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * as_metadata_get_node_value:
 */
static gchar*
as_metadata_get_node_value (AsMetadata *metad, xmlNode *node)
{
	gchar *content;
	content = (gchar*) xmlNodeGetContent (node);

	return content;
}

/**
 * as_metadata_dump_node_children:
 */
static gchar*
as_metadata_dump_node_children (AsMetadata *metad, xmlNode *node)
{
	GString *str = NULL;
	xmlNode *iter;
	xmlBufferPtr nodeBuf;

	str = g_string_new ("");
	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE) {
					continue;
		}

		nodeBuf = xmlBufferCreate();
		xmlNodeDump (nodeBuf, NULL, iter, 0, 1);
		if (str->len > 0)
			g_string_append (str, "\n");
		g_string_append_printf (str, "%s", (const gchar*) nodeBuf->content);
		xmlBufferFree (nodeBuf);
	}

	return g_string_free (str, FALSE);
}

/**
 * as_metadata_get_node_locale:
 * @node: A XML node
 *
 * Returns the locale of a node, if the node should be considered for inclusion.
 * Returns %NULL if the node should be ignored due to a not-matching locale.
 */
gchar*
as_metadata_get_node_locale (AsMetadata *metad, xmlNode *node)
{
	gchar *lang;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	lang = (gchar*) xmlGetProp (node, (xmlChar*) "lang");

	if (lang == NULL) {
		lang = g_strdup ("C");
		goto out;
	}

	if (g_strcmp0 (priv->locale, "ALL") == 0) {
		/* we should read all languages */
		goto out;
	}

	if (g_strcmp0 (lang, priv->locale) == 0) {
		goto out;
	}

	if (g_strcmp0 (lang, priv->locale_short) == 0) {
		g_free (lang);
		lang = g_strdup (priv->locale);
		goto out;
	}

	/* If we are here, we haven't found a matching locale.
	 * In that case, we return %NULL to indicate that this elemant should not be added.
	 */
	g_free (lang);
	lang = NULL;

out:
	return lang;
}

static gchar**
as_metadata_get_children_as_strv (AsMetadata *metad, xmlNode* node, const gchar* element_name)
{
	GPtrArray *list;
	xmlNode *iter;
	gchar **res;
	g_return_val_if_fail (metad != NULL, NULL);
	g_return_val_if_fail (element_name != NULL, NULL);
	list = g_ptr_array_new_with_free_func (g_free);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE) {
					continue;
		}
		if (g_strcmp0 ((gchar*) iter->name, element_name) == 0) {
			gchar* content = NULL;
			content = (gchar*) xmlNodeGetContent (iter);
			if (content != NULL) {
				gchar *s;
				s = as_string_strip (content);
				g_ptr_array_add (list, s);
			}
			g_free (content);
		}
	}

	res = as_ptr_array_to_strv (list);
	g_ptr_array_unref (list);
	return res;
}

/**
 * as_metadata_process_screenshot:
 */
static void
as_metadata_process_screenshot (AsMetadata *metad, xmlNode* node, AsScreenshot* scr)
{
	xmlNode *iter;
	gchar *node_name;
	gchar *content = NULL;

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		content = as_metadata_get_node_value (metad, iter);
		if (g_strcmp0 (node_name, "image") == 0) {
			AsImage *img;
			guint64 width;
			guint64 height;
			gchar *stype;
			gchar *str;
			if (content == NULL) {
				continue;
			}
			img = as_image_new ();

			str = (gchar*) xmlGetProp (iter, (xmlChar*) "width");
			if (str == NULL) {
				width = 0;
			} else {
				width = g_ascii_strtoll (str, NULL, 10);
				g_free (str);
			}
			str = (gchar*) xmlGetProp (iter, (xmlChar*) "height");
			if (str == NULL) {
				height = 0;
			} else {
				height = g_ascii_strtoll (str, NULL, 10);
				g_free (str);
			}
			/* discard invalid elements */
			if ((width == 0) || (height == 0)) {
				g_free (content);
				continue;
			}

			as_image_set_width (img, width);
			as_image_set_height (img, height);

			stype = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (g_strcmp0 (stype, "thumbnail") == 0) {
				as_image_set_kind (img, AS_IMAGE_KIND_THUMBNAIL);
			} else {
				as_image_set_kind (img, AS_IMAGE_KIND_SOURCE);
			}
			g_free (stype);
			as_image_set_url (img, content);
			as_screenshot_add_image (scr, img);
		} else if (g_strcmp0 (node_name, "caption") == 0) {
			if (content != NULL) {
				gchar *lang;
				lang = as_metadata_get_node_locale (metad, iter);
				if (lang != NULL)
					as_screenshot_set_caption (scr, content, lang);
				g_free (lang);
			}
		}
		g_free (content);
	}
}

static void
as_metadata_process_screenshots_tag (AsMetadata *metad, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	AsScreenshot *sshot = NULL;
	gchar *prop;
	g_return_if_fail (metad != NULL);
	g_return_if_fail (cpt != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "screenshot") == 0) {
			sshot = as_screenshot_new ();

			/* propagate locale */
			as_screenshot_set_active_locale (sshot, as_component_get_active_locale (cpt));

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (g_strcmp0 (prop, "default") == 0)
				as_screenshot_set_kind (sshot, AS_SCREENSHOT_KIND_DEFAULT);
			as_metadata_process_screenshot (metad, iter, sshot);
			if (as_screenshot_is_valid (sshot))
				as_component_add_screenshot (cpt, sshot);
			g_free (prop);
			g_object_unref (sshot);
		}
	}
}

static void
as_metadata_upstream_description_to_cpt (gchar *key, GString *value, AsComponent *cpt)
{
	g_assert (AS_IS_COMPONENT (cpt));

	as_component_set_description (cpt, value->str, key);
	g_string_free (value, TRUE);
}

static void
as_metadata_upstream_description_to_release (gchar *key, GString *value, AsRelease *rel)
{
	g_assert (AS_IS_RELEASE (rel));

	as_release_set_description (rel, value->str, key);
	g_string_free (value, TRUE);
}

/**
 * as_metadata_parse_upstream_description_tag:
 */
static void
as_metadata_parse_upstream_description_tag (AsMetadata *metad, xmlNode* node, GHFunc func, gpointer entity)
{
	xmlNode *iter;
	gchar *node_name;
	GHashTable *desc;

	desc = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		gchar *lang;
		gchar *content;
		GString *str;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		content = as_metadata_get_node_value (metad, iter);
		lang = as_metadata_get_node_locale (metad, iter);

		str = g_hash_table_lookup (desc, lang);
		if (str == NULL) {
			str = g_string_new ("");
			g_hash_table_insert (desc, g_strdup (lang), str);
		}

		g_string_append_printf (str, "\n<%s>%s</%s>", node_name, content, node_name);
		g_free (lang);
		g_free (content);
	}

	g_hash_table_foreach (desc, func, entity);
	g_hash_table_unref (desc);
}

static void
as_metadata_process_releases_tag (AsMetadata *metad, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	xmlNode *iter2;
	AsRelease *release = NULL;
	gchar *prop;
	guint64 timestamp;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_return_if_fail (cpt != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "release") == 0) {
			release = as_release_new ();

			/* propagate locale */
			as_release_set_active_locale (release, as_component_get_active_locale (cpt));

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "version");
			as_release_set_version (release, prop);
			g_free (prop);

			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "timestamp");
			if (prop != NULL) {
				timestamp = g_ascii_strtoll (prop, NULL, 10);
				as_release_set_timestamp (release, timestamp);
				g_free (prop);
			}

			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				gchar *content;

				if (iter->type != XML_ELEMENT_NODE)
					continue;

				if (g_strcmp0 ((gchar*) iter2->name, "location") == 0) {
					content = as_metadata_get_node_value (metad, iter2);
					as_release_add_location (release, content);
					g_free (content);
				} else if (g_strcmp0 ((gchar*) iter2->name, "checksum") == 0) {
					prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
					if (g_strcmp0 (prop, "sha1") == 0) {
						content = as_metadata_get_node_value (metad, iter2);
						as_release_set_checksum_sha1 (release, content);
						g_free (content);
					}
					g_free (prop);
				} else if (g_strcmp0 ((gchar*) iter2->name, "description") == 0) {
					if (priv->mode == AS_PARSER_MODE_DISTRO) {
						gchar *lang;

						/* for distro XML, the "description" tag has a language property, so parsing it is simple */
						content = as_metadata_dump_node_children (metad, iter2);
						lang = as_metadata_get_node_locale (metad, iter2);
						if (lang != NULL)
							as_release_set_description (release, content, lang);
						g_free (content);
						g_free (lang);
					} else {
						as_metadata_parse_upstream_description_tag (metad,
																iter2,
																(GHFunc) as_metadata_upstream_description_to_release,
																release);
					}
				}
			}

			as_component_add_release (cpt, release);
			g_object_unref (release);
		}
	}
}

static void
as_metadata_process_provides (AsMetadata *metad, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	gchar *node_name;
	gchar *content = NULL;
	GPtrArray *provided_items;
	g_return_if_fail (metad != NULL);
	g_return_if_fail (cpt != NULL);

	provided_items = as_component_get_provided_items (cpt);
	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (gchar*) iter->name;
		content = as_metadata_get_node_value (metad, iter);
		if (content == NULL)
			continue;

		if (g_strcmp0 (node_name, "library") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_LIBRARY, content, ""));
		} else if (g_strcmp0 (node_name, "binary") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_BINARY, content, ""));
		} else if (g_strcmp0 (node_name, "font") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_FONT, content, ""));
		} else if (g_strcmp0 (node_name, "modalias") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_MODALIAS, content, ""));
		} else if (g_strcmp0 (node_name, "firmware") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_FIRMWARE, content, ""));
		} else if (g_strcmp0 (node_name, "python2") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_PYTHON2, content, ""));
		} else if (g_strcmp0 (node_name, "python3") == 0) {
			g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_PYTHON3, content, ""));
		} else if (g_strcmp0 (node_name, "dbus") == 0) {
			gchar *dbustype_val;
			const gchar *dbustype = NULL;
			dbustype_val = (gchar*) xmlGetProp (iter, (xmlChar*) "type");

			if (g_strcmp0 (dbustype_val, "system") == 0)
				dbustype = "system";
			else if (g_strcmp0 (dbustype_val, "session") == 0)
				dbustype = "session";
			g_free (dbustype_val);

			/* we don't add malformed provides types */
			if (dbustype != NULL)
				g_ptr_array_add (provided_items,
								as_provides_item_create (AS_PROVIDES_KIND_DBUS, content, dbustype));
		}

		g_free (content);
	}
}

static void
as_metadata_set_component_type_from_node (xmlNode *node, AsComponent *cpt)
{
	gchar *cpttype;

	/* find out which kind of component we are dealing with */
	cpttype = (gchar*) xmlGetProp (node, (xmlChar*) "type");
	if ((cpttype == NULL) || (g_strcmp0 (cpttype, "generic") == 0)) {
		as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	} else {
		AsComponentKind ckind;
		ckind = as_component_kind_from_string (cpttype);
		as_component_set_kind (cpt, ckind);
		if (ckind == AS_COMPONENT_KIND_UNKNOWN)
			g_debug ("An unknown component was found: %s", cpttype);
	}
	g_free (cpttype);
}

static void
as_metadata_process_languages_tag (AsMetadata *metad, xmlNode* node, AsComponent* cpt)
{
	xmlNode *iter;
	gchar *prop;
	g_return_if_fail (metad != NULL);
	g_return_if_fail (cpt != NULL);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "lang") == 0) {
			guint64 percentage = 0;
			gchar *locale;
			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "percentage");
			if (prop != NULL) {
				percentage = g_ascii_strtoll (prop, NULL, 10);
				g_free (prop);
			}

			locale = as_metadata_get_node_locale (metad, iter);
			as_component_add_language (cpt, locale, percentage);
			g_free (locale);
		}
	}
}

/**
 * as_metadata_parse_component_node:
 */
AsComponent*
as_metadata_parse_component_node (AsMetadata *metad, xmlNode* node, gboolean allow_invalid, GError **error)
{
	AsComponent* cpt;
	xmlNode *iter;
	const gchar *node_name;
	GPtrArray *compulsory_for_desktops;
	GPtrArray *pkgnames;
	gchar **strv;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	g_return_val_if_fail (metad != NULL, NULL);

	compulsory_for_desktops = g_ptr_array_new_with_free_func (g_free);
	pkgnames = g_ptr_array_new_with_free_func (g_free);

	/* a fresh app component */
	cpt = as_component_new ();

	/* set component kind */
	as_metadata_set_component_type_from_node (node, cpt);

	if (priv->mode == AS_PARSER_MODE_DISTRO) {
		/* distro metadata allows setting a priority for components */
		gchar *priority_str;
		priority_str = (gchar*) xmlGetProp (node, (xmlChar*) "priority");
		if (priority_str != NULL) {
			int priority;
			priority = g_ascii_strtoll (priority_str, NULL, 10);
			as_component_set_priority (cpt, priority);
		}
		g_free (priority_str);
	}

	/* set active locale for this component */
	as_component_set_active_locale (cpt, priv->locale);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		gchar *content;
		gchar *lang;

		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		node_name = (const gchar*) iter->name;
		content = as_metadata_get_node_value (metad, iter);
		lang = as_metadata_get_node_locale (metad, iter);

		if (g_strcmp0 (node_name, "id") == 0) {
				as_component_set_id (cpt, content);
				if ((priv->mode == AS_PARSER_MODE_UPSTREAM) &&
					(as_component_get_kind (cpt) == AS_COMPONENT_KIND_GENERIC)) {
					/* parse legacy component type information */
					as_metadata_set_component_type_from_node (iter, cpt);
				}
		} else if (g_strcmp0 (node_name, "pkgname") == 0) {
			if (content != NULL)
				g_ptr_array_add (pkgnames, g_strdup (content));
		} else if (g_strcmp0 (node_name, "source_pkgname") == 0) {
			as_component_set_source_pkgname (cpt, content);
		} else if (g_strcmp0 (node_name, "name") == 0) {
			if (lang != NULL)
				as_component_set_name (cpt, content, lang);
		} else if (g_strcmp0 (node_name, "summary") == 0) {
			if (lang != NULL)
				as_component_set_summary (cpt, content, lang);
		} else if (g_strcmp0 (node_name, "description") == 0) {
			if (priv->mode == AS_PARSER_MODE_DISTRO) {
				/* for distro XML, the "description" tag has a language property, so parsing it is simple */
				if (lang != NULL) {
					gchar *desc;
					desc = as_metadata_dump_node_children (metad, iter);
					as_component_set_description (cpt, desc, lang);
					g_free (desc);
				}
			} else {
				as_metadata_parse_upstream_description_tag (metad,
														iter,
														(GHFunc) as_metadata_upstream_description_to_cpt,
														cpt);
			}
		} else if (g_strcmp0 (node_name, "icon") == 0) {
			gchar *prop;
			const gchar *icon_url;
			if (content == NULL)
				continue;
			prop = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
			if (g_strcmp0 (prop, "stock") == 0) {
				as_component_add_icon (cpt, AS_ICON_KIND_STOCK, 0, 0, content);
			} else if (g_strcmp0 (prop, "cached") == 0) {
				as_component_add_icon (cpt, AS_ICON_KIND_CACHED, 0, 0, content);
				icon_url = as_component_get_icon_url (cpt, 0, 0);
				if ((icon_url == NULL) || (g_str_has_prefix (icon_url, "http://"))) {
					as_component_add_icon_url (cpt, 0, 0, content);
				}
			} else if (g_strcmp0 (prop, "local") == 0) {
				as_component_add_icon_url (cpt, 0, 0, content);
				as_component_add_icon (cpt, AS_ICON_KIND_LOCAL, 0, 0, content);
			} else if (g_strcmp0 (prop, "remote") == 0) {
				as_component_add_icon (cpt, AS_ICON_KIND_REMOTE, 0, 0, content);
				icon_url = as_component_get_icon_url (cpt, 0, 0);
				if (icon_url == NULL)
					as_component_add_icon_url (cpt, 0, 0, content);
			}
		} else if (g_strcmp0 (node_name, "url") == 0) {
			if (content != NULL) {
				gchar *urltype_str;
				AsUrlKind url_kind;
				urltype_str = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
				url_kind = as_url_kind_from_string (urltype_str);
				if (url_kind != AS_URL_KIND_UNKNOWN)
					as_component_add_url (cpt, url_kind, content);
				g_free (urltype_str);
			}
		} else if (g_strcmp0 (node_name, "categories") == 0) {
			gchar **cat_array;
			cat_array = as_metadata_get_children_as_strv (metad, iter, "category");
			as_component_set_categories (cpt, cat_array);
			g_strfreev (cat_array);
		} else if (g_strcmp0 (node_name, "keywords") == 0) {
			gchar **kw_array;
			kw_array = as_metadata_get_children_as_strv (metad, iter, "keyword");
			as_component_set_keywords (cpt, kw_array, NULL);
			g_strfreev (kw_array);
		} else if (g_strcmp0 (node_name, "mimetypes") == 0) {
			gchar **mime_array;
			guint i;

			mime_array = as_metadata_get_children_as_strv (metad, iter, "mimetype");
			for (i = 0; mime_array[i] != NULL; i++) {
				as_component_add_provided_item (cpt, AS_PROVIDES_KIND_MIMETYPE, mime_array[i], "");
			}
			g_strfreev (mime_array);
		} else if (g_strcmp0 (node_name, "provides") == 0) {
			as_metadata_process_provides (metad, iter, cpt);
		} else if (g_strcmp0 (node_name, "screenshots") == 0) {
			as_metadata_process_screenshots_tag (metad, iter, cpt);
		} else if (g_strcmp0 (node_name, "project_license") == 0) {
			if (content != NULL)
				as_component_set_project_license (cpt, content);
		} else if (g_strcmp0 (node_name, "project_group") == 0) {
			if (content != NULL)
				as_component_set_project_group (cpt, content);
		} else if (g_strcmp0 (node_name, "developer_name") == 0) {
			if (lang != NULL)
				as_component_set_developer_name (cpt, content, lang);
		} else if (g_strcmp0 (node_name, "compulsory_for_desktop") == 0) {
			if (content != NULL)
				g_ptr_array_add (compulsory_for_desktops, g_strdup (content));
		} else if (g_strcmp0 (node_name, "releases") == 0) {
			as_metadata_process_releases_tag (metad, iter, cpt);
		} else if (g_strcmp0 (node_name, "extends") == 0) {
			if (content != NULL)
				as_component_add_extends (cpt, content);
		} else if (g_strcmp0 (node_name, "languages") == 0) {
			as_metadata_process_languages_tag (metad, iter, cpt);
		} else if (g_strcmp0 (node_name, "bundle") == 0) {
			if (content != NULL) {
				gchar *type_str;
				AsBundleKind bundle_kind;
				type_str = (gchar*) xmlGetProp (iter, (xmlChar*) "type");
				bundle_kind = as_bundle_kind_from_string (type_str);
				if (bundle_kind != AS_BUNDLE_KIND_UNKNOWN)
					bundle_kind = AS_BUNDLE_KIND_LIMBA;
				as_component_add_bundle_id (cpt, bundle_kind, content);
				g_free (type_str);
			}
		}

		g_free (lang);
		g_free (content);
	}

	/* set the origin of this component */
	as_component_set_origin (cpt, priv->origin_name);

	/* add package name information to component */
	strv = as_ptr_array_to_strv (pkgnames);
	as_component_set_pkgnames (cpt, strv);
	g_ptr_array_unref (pkgnames);
	g_strfreev (strv);

	/* add compulsoriy information to component as strv */
	strv = as_ptr_array_to_strv (compulsory_for_desktops);
	as_component_set_compulsory_for_desktops (cpt, strv);
	g_ptr_array_unref (compulsory_for_desktops);
	g_strfreev (strv);

	if ((allow_invalid) || (as_component_is_valid (cpt))) {
		return cpt;
	} else {
		gchar *cpt_str;
		gchar *msg;
		cpt_str = as_component_to_string (cpt);
		msg = g_strdup_printf ("Invalid component: %s", cpt_str);
		g_free (cpt_str);
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     msg);
		g_free (msg);
		g_object_unref (cpt);
	}

	return NULL;
}

/**
 * as_metadata_parse_components_node:
 */
void
as_metadata_parse_components_node (AsMetadata *metad, xmlNode* node, gboolean allow_invalid, GError **error)
{
	AsComponent *cpt;
	xmlNode* iter;
	GError *tmp_error = NULL;
	gchar *origin;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	/* set origin of this metadata */
	origin = (gchar*) xmlGetProp (node, (xmlChar*) "origin");
	as_metadata_set_origin (metad, origin);
	g_free (origin);

	for (iter = node->children; iter != NULL; iter = iter->next) {
		/* discard spaces */
		if (iter->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((gchar*) iter->name, "component") == 0) {
			cpt = as_metadata_parse_component_node (metad, iter, allow_invalid, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return;
			} else if (cpt != NULL) {
				g_ptr_array_add (priv->cpts, cpt);
			}
		}
	}
}

/**
 * as_metadata_process_document:
 */
void
as_metadata_process_document (AsMetadata *metad, const gchar* xmldoc_str, GError **error)
{
	xmlDoc* doc;
	xmlNode* root;
	AsComponent *cpt = NULL;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	g_return_if_fail (metad != NULL);

	if (xmldoc_str == NULL) {
		/* empty document means no components */
		return;
	}

	doc = xmlParseDoc ((xmlChar*) xmldoc_str);
	if (doc == NULL) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "Could not parse XML!");
		return;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL) {
		g_set_error_literal (error,
				     AS_METADATA_ERROR,
				     AS_METADATA_ERROR_FAILED,
				     "The XML document is empty.");
		return;
	}

	if (g_strcmp0 ((gchar*) root->name, "components") == 0) {
		as_metadata_set_parser_mode (metad, AS_PARSER_MODE_DISTRO);
		as_metadata_parse_components_node (metad, root, FALSE, error);
	} else if (g_strcmp0 ((gchar*) root->name, "component") == 0) {
		as_metadata_set_parser_mode (metad, AS_PARSER_MODE_UPSTREAM);
		cpt = as_metadata_parse_component_node (metad, root, TRUE, error);
		g_ptr_array_add (priv->cpts, cpt);
	} else if  (g_strcmp0 ((gchar*) root->name, "application") == 0) {
		as_metadata_set_parser_mode (metad, AS_PARSER_MODE_UPSTREAM);
		g_debug ("Parsing legacy AppStream metadata file.");
		cpt = as_metadata_parse_component_node (metad, root, TRUE, error);
		g_ptr_array_add (priv->cpts, cpt);
	} else {
		g_set_error_literal (error,
					AS_METADATA_ERROR,
					AS_METADATA_ERROR_FAILED,
					"XML file does not contain valid AppStream data!");
		goto out;
	}

out:
	xmlFreeDoc (doc);
}

/**
 * as_metadata_parse_data:
 * @metad: A valid #AsMetadata instance
 * @data: XML data describing a component
 * @error: A #GError or %NULL.
 *
 * Parses AppStream metadata.
 *
 **/
void
as_metadata_parse_data (AsMetadata *metad, const gchar *data, GError **error)
{
	g_return_if_fail (metad != NULL);

	as_metadata_process_document (metad, data, error);
}

/**
 * as_metadata_parse_file:
 * @metad: A valid #AsMetadata instance
 * @file: #GFile for the upstream metadata
 * @error: A #GError or %NULL.
 *
 * Parses an AppStream upstream metadata file.
 *
 **/
void
as_metadata_parse_file (AsMetadata *metad, GFile* file, GError **error)
{
	gchar *xml_doc;
	GFileInputStream* fistream;
	GFileInfo *info = NULL;
	const gchar *content_type = NULL;

	g_return_if_fail (file != NULL);

	info = g_file_query_info (file,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
	if (info != NULL)
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	if ((g_strcmp0 (content_type, "application/gzip") == 0) || (g_strcmp0 (content_type, "application/x-gzip") == 0)) {
		GFileInputStream *fistream;
		GMemoryOutputStream *mem_os;
		GInputStream *conv_stream;
		GZlibDecompressor *zdecomp;
		guint8 *data;

		/* load a GZip compressed file */
		fistream = g_file_read (file, NULL, NULL);
		mem_os = (GMemoryOutputStream*) g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
		zdecomp = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
		conv_stream = g_converter_input_stream_new (G_INPUT_STREAM (fistream), G_CONVERTER (zdecomp));
		g_object_unref (zdecomp);

		g_output_stream_splice (G_OUTPUT_STREAM (mem_os), conv_stream, 0, NULL, NULL);
		data = g_memory_output_stream_get_data (mem_os);

		xml_doc = g_strdup ((const gchar*) data);

		g_object_unref (conv_stream);
		g_object_unref (mem_os);
		g_object_unref (fistream);
	} else {
		gchar *line = NULL;
		GString *str;
		GDataInputStream *dis;

		/* load a plaintext file */
		str = g_string_new ("");
		fistream = g_file_read (file, NULL, NULL);
		dis = g_data_input_stream_new ((GInputStream*) fistream);
		g_object_unref (fistream);

		while (TRUE) {
			line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
			if (line == NULL) {
				break;
			}

			g_string_append_printf (str, "%s\n", line);
		}

		xml_doc = g_string_free (str, FALSE);
		g_object_unref (dis);
	}

	/* parse XML data */
	as_metadata_process_document (metad, xml_doc, error);
	g_free (xml_doc);
}

/**
 * as_metadata_save_xml:
 */
static void
as_metadata_save_xml (AsMetadata *metad, const gchar *fname, const gchar *xml_data, GError **error)
{
	GFile *file;
	GError *tmp_error = NULL;

	file = g_file_new_for_path (fname);
	if (g_str_has_suffix (fname, ".gz")) {
		GOutputStream *out2 = NULL;
		GOutputStream *out = NULL;
		GZlibCompressor *compressor = NULL;

		/* write a gzip compressed file */
		compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
		out = g_memory_output_stream_new_resizable ();
		out2 = g_converter_output_stream_new (out, G_CONVERTER (compressor));
		g_object_unref (compressor);

		/* ensure data is not NULL */
		if (xml_data == NULL)
			xml_data = "";

		if (!g_output_stream_write_all (out2, xml_data, strlen (xml_data),
					NULL, NULL, &tmp_error)) {
			g_object_unref (out2);
			g_object_unref (out);
			g_propagate_error (error, tmp_error);
			goto out;
		}

		g_output_stream_close (out2, NULL, &tmp_error);
		g_object_unref (out2);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

		if (!g_file_replace_contents (file,
			g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (out)),
						g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (out)),
						NULL,
						FALSE,
						G_FILE_CREATE_NONE,
						NULL,
						NULL,
						&tmp_error)) {
			g_object_unref (out);
			g_propagate_error (error, tmp_error);
			goto out;
		}

		g_object_unref (out);

	} else {
		GFileOutputStream *fos = NULL;
		GDataOutputStream *dos = NULL;

		/* write uncompressed file */
		if (g_file_query_exists (file, NULL)) {
			fos = g_file_replace (file,
							NULL,
							FALSE,
							G_FILE_CREATE_REPLACE_DESTINATION,
							NULL,
							&tmp_error);
		} else {
			fos = g_file_create (file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &tmp_error);
		}

		if (tmp_error != NULL) {
			g_object_unref (fos);
			g_propagate_error (error, tmp_error);
			goto out;
		}

		dos = g_data_output_stream_new (G_OUTPUT_STREAM (fos));
		g_data_output_stream_put_string (dos, xml_data, NULL, &tmp_error);

		g_object_unref (dos);
		g_object_unref (fos);

		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}
	}

out:
	g_object_unref (file);
}


/**
 * as_metadata_save_upstream_xml:
 * @fname: The filename for the new XML file.
 *
 * Serialize #AsComponent instance to XML and save it to file.
 * An existing file at the same location will be overridden.
 */
void
as_metadata_save_upstream_xml (AsMetadata *metad, const gchar *fname, GError **error)
{
	gchar *xml_data;

	xml_data = as_metadata_component_to_upstream_xml (metad);
	as_metadata_save_xml (metad, fname, xml_data, error);

	g_free (xml_data);
}

/**
 * as_metadata_save_distro_xml:
 * @fname: The filename for the new XML file.
 *
 * Serialize all #AsComponent instances to XML and save the data to a file.
 * An existing file at the same location will be overridden.
 */
void
as_metadata_save_distro_xml (AsMetadata *metad, const gchar *fname, GError **error)
{
	gchar *xml_data;

	xml_data = as_metadata_components_to_distro_xml (metad);
	as_metadata_save_xml (metad, fname, xml_data, error);

	g_free (xml_data);
}

/**
 * as_component_xml_add_node:
 *
 * Add node if value is not empty
 */
static xmlNode*
as_metadata_xml_add_node (xmlNode *root, const gchar *name, const gchar *value)
{
	if (as_str_empty (value))
		return NULL;

	return xmlNewTextChild (root, NULL, (xmlChar*) name, (xmlChar*) value);
}

/**
 * as_metadata_xml_add_description:
 *
 * Add the description markup to the XML tree
 */
static gboolean
as_metadata_xml_add_description (AsMetadata *metad, xmlNode *root, xmlNode **desc_node, const gchar *description_markup, const gchar *lang)
{
	gchar *xmldata;
	xmlDoc *doc;
	xmlNode *droot;
	xmlNode *dnode;
	xmlNode *iter;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	gboolean ret = TRUE;
	gboolean localized;

	if (as_str_empty (description_markup))
		return FALSE;

	xmldata = g_strdup_printf ("<root>%s</root>", description_markup);
	doc = xmlParseDoc ((xmlChar*) xmldata);
	if (doc == NULL) {
		ret = FALSE;
		goto out;
	}

	droot = xmlDocGetRootElement (doc);
	if (droot == NULL) {
		ret = FALSE;
		goto out;
	}

	if (priv->mode == AS_PARSER_MODE_UPSTREAM) {
		if (*desc_node == NULL)
			*desc_node = xmlNewChild (root, NULL, (xmlChar*) "description", NULL);
		dnode = *desc_node;
	} else {
		/* in distro parser mode, we might have multiple <description/> tags */
		dnode = xmlNewChild (root, NULL, (xmlChar*) "description", NULL);
	}

	localized = g_strcmp0 (lang, "C") != 0;
	if (priv->mode != AS_PARSER_MODE_UPSTREAM) {
		if (localized) {
			xmlNewProp (dnode,
					(xmlChar*) "xml:lang",
					(xmlChar*) lang);
		}
	}

	for (iter = droot->children; iter != NULL; iter = iter->next) {
		xmlNode *cn;

		if ((g_strcmp0 ((const gchar*) iter->name, "ul") == 0) || (g_strcmp0 ((const gchar*) iter->name, "ol") == 0)) {
			xmlNode *iter2;
			xmlNode *enumNode;

			enumNode = xmlNewChild (dnode, NULL, iter->name, NULL);
			for (iter2 = iter->children; iter2 != NULL; iter2 = iter2->next) {
				cn = xmlAddChild (enumNode, xmlCopyNode (iter2, TRUE));

				if ((priv->mode == AS_PARSER_MODE_UPSTREAM) && (localized)) {
					xmlNewProp (cn,
						(xmlChar*) "xml:lang",
						(xmlChar*) lang);
				}
			}

			continue;
		}

		cn = xmlAddChild (dnode, xmlCopyNode (iter, TRUE));

		if ((priv->mode == AS_PARSER_MODE_UPSTREAM) && (localized)) {
			xmlNewProp (cn,
					(xmlChar*) "xml:lang",
					(xmlChar*) lang);
		}
	}

out:
	if (doc != NULL)
		xmlFreeDoc (doc);
	g_free (xmldata);
	return ret;
}

/**
 * as_component_xml_add_node_list:
 *
 * Add node if value is not empty
 */
static void
as_metadata_xml_add_node_list (xmlNode *root, const gchar *name, const gchar *child_name, gchar **strv)
{
	xmlNode *node;
	guint i;

	if (strv == NULL)
		return;

	if (name == NULL)
		node = root;
	else
		node = xmlNewTextChild (root, NULL, (xmlChar*) name, NULL);
	for (i = 0; strv[i] != NULL; i++) {
		xmlNewTextChild (node, NULL, (xmlChar*) child_name, (xmlChar*) strv[i]);
	}
}

typedef struct {
	AsMetadata *metad;

	xmlNode *parent;
	xmlNode *nd;
	const gchar *node_name;
} AsLocaleWriteHelper;

/**
 * _as_metadata_lang_hashtable_to_nodes:
 */
static void
_as_metadata_lang_hashtable_to_nodes (gchar *key, gchar *value, AsLocaleWriteHelper *helper)
{
	xmlNode *cnode;
	if (as_str_empty (value))
		return;

	cnode = xmlNewTextChild (helper->parent, NULL, (xmlChar*) helper->node_name, (xmlChar*) value);
	if (g_strcmp0 (key, "C") != 0) {
		xmlNewProp (cnode,
					(xmlChar*) "xml:lang",
					(xmlChar*) key);
	}
}

/**
 * _as_metadata_desc_lang_hashtable_to_nodes:
 */
static void
_as_metadata_desc_lang_hashtable_to_nodes (gchar *key, gchar *value, AsLocaleWriteHelper *helper)
{
	if (as_str_empty (value))
		return;

	as_metadata_xml_add_description (helper->metad, helper->parent, &helper->nd, value, key);
}

/**
 * as_metadata_component_to_node:
 * @cpt: a valid #AsComponent
 *
 * Serialize the component data to an xmlNode.
 *
 */
static xmlNode*
as_metadata_component_to_node (AsMetadata *metad, AsComponent *cpt)
{
	xmlNode *cnode;
	xmlNode *node;
	gchar **strv;
	GPtrArray *releases;
	GPtrArray *screenshots;
	AsComponentKind kind;
	AsLocaleWriteHelper helper;
	guint i;
	g_return_val_if_fail (cpt != NULL, NULL);

	/* define component root node */
	kind = as_component_get_kind (cpt);
	cnode = xmlNewNode (NULL, (xmlChar*) "component");
	if ((kind != AS_COMPONENT_KIND_GENERIC) && (kind != AS_COMPONENT_KIND_UNKNOWN)) {
		xmlNewProp (cnode, (xmlChar*) "type",
					(xmlChar*) as_component_kind_to_string (kind));
	}

	as_metadata_xml_add_node (cnode, "id", as_component_get_id (cpt));

	helper.parent = cnode;
	helper.metad = metad;
	helper.nd = NULL;
	helper.node_name = "name";
	g_hash_table_foreach (as_component_get_name_table (cpt),
						(GHFunc) _as_metadata_lang_hashtable_to_nodes,
						&helper);

	helper.nd = NULL;
	helper.node_name = "summary";
	g_hash_table_foreach (as_component_get_summary_table (cpt),
						(GHFunc) _as_metadata_lang_hashtable_to_nodes,
						&helper);

	helper.nd = NULL;
	helper.node_name = "developer_name";
	g_hash_table_foreach (as_component_get_developer_name_table (cpt),
						(GHFunc) _as_metadata_lang_hashtable_to_nodes,
						&helper);

	helper.nd = NULL;
	helper.node_name = "description";
	g_hash_table_foreach (as_component_get_description_table (cpt),
						(GHFunc) _as_metadata_desc_lang_hashtable_to_nodes,
						&helper);

	as_metadata_xml_add_node (cnode, "project_license", as_component_get_project_license (cpt));
	as_metadata_xml_add_node (cnode, "project_group", as_component_get_project_group (cpt));

	as_metadata_xml_add_node_list (cnode, NULL, "pkgname", as_component_get_pkgnames (cpt));
	strv = as_ptr_array_to_strv (as_component_get_extends (cpt));
	as_metadata_xml_add_node_list (cnode, NULL, "extends", strv);
	g_strfreev (strv);
	as_metadata_xml_add_node_list (cnode, NULL, "compulsory_for_desktop", as_component_get_compulsory_for_desktops (cpt));
	as_metadata_xml_add_node_list (cnode, "keywords", "keyword", as_component_get_keywords (cpt));
	as_metadata_xml_add_node_list (cnode, "categories", "category", as_component_get_categories (cpt));

	/* urls */
	for (i = AS_URL_KIND_UNKNOWN; i < AS_URL_KIND_LAST; i++) {
		xmlNode *n;
		const gchar *value;
		value = as_component_get_url (cpt, i);
		if (value == NULL)
			continue;

		n = xmlNewTextChild (cnode, NULL, (xmlChar*) "url", (xmlChar*) value);
		xmlNewProp (n, (xmlChar*) "type",
					(xmlChar*) as_url_kind_to_string (i));
	}

	/* icons */
	for (i = AS_ICON_KIND_UNKNOWN; i < AS_ICON_KIND_LAST; i++) {
		xmlNode *n;
		const gchar *value;
		value = as_component_get_icon (cpt, i, 0, 0); /* TODO: Add size information to output XML */
		if (value == NULL)
			continue;

		n = xmlNewTextChild (cnode, NULL, (xmlChar*) "icon", (xmlChar*) value);
		xmlNewProp (n, (xmlChar*) "type",
					(xmlChar*) as_icon_kind_to_string (i));
	}

	/* bundles */
	for (i = AS_BUNDLE_KIND_UNKNOWN; i < AS_BUNDLE_KIND_LAST; i++) {
		xmlNode *n;
		const gchar *value;
		value = as_component_get_bundle_id (cpt, i);
		if (value == NULL)
			continue;

		n = xmlNewTextChild (cnode, NULL, (xmlChar*) "bundle", (xmlChar*) value);
		xmlNewProp (n, (xmlChar*) "type",
					(xmlChar*) as_bundle_kind_to_string (i));
	}

	/* releases node */
	releases = as_component_get_releases (cpt);
	if (releases->len > 0) {
		node = xmlNewTextChild (cnode, NULL, (xmlChar*) "releases", NULL);
		as_component_xml_add_release_subnodes (cpt, node);
	}

	/* screenshots node */
	screenshots = as_component_get_screenshots (cpt);
	if (screenshots->len > 0) {
		node = xmlNewTextChild (cnode, NULL, (xmlChar*) "screenshots", NULL);
		as_component_xml_add_screenshot_subnodes (cpt, node);
	}

	return cnode;
}

/**
 * as_metadata_component_to_upstream_xml:
 *
 * Convert an #AsComponent to upstream XML.
 * (The amount of localization included in the metadata depends on how the #AsComponent
 * was initially loaded)
 *
 * The first #AsComponent added to the internal list will be transformed.
 * In case no component is present, %NULL is returned.
 *
 * Returns: (transfer full): A string containing the XML. Free with g_free()
 */
gchar*
as_metadata_component_to_upstream_xml (AsMetadata *metad)
{
	xmlDoc *doc;
	xmlNode *root;
	gchar *xmlstr = NULL;
	AsComponent *cpt;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	cpt = as_metadata_get_component (metad);
	if (cpt == NULL)
		return NULL;

	priv->mode = AS_PARSER_MODE_UPSTREAM;

	doc = xmlNewDoc ((xmlChar*) NULL);

	/* define component root node */
	root = as_metadata_component_to_node (metad, cpt);
	if (root == NULL)
		goto out;
	xmlDocSetRootElement (doc, root);

out:
	xmlDocDumpMemory (doc, (xmlChar**) (&xmlstr), NULL);
	xmlFreeDoc (doc);

	return xmlstr;
}

/**
 * as_metadata_components_to_distro_xml:
 *
 * Serialize all #AsComponent instances into AppStream
 * distro-XML data.
 * %NULL is returned if there is nothing to serialize.
 *
 * Returns: (transfer full): A string containing the XML. Free with g_free()
 */
gchar*
as_metadata_components_to_distro_xml (AsMetadata *metad)
{
	xmlDoc *doc;
	xmlNode *root;
	gchar *xmlstr = NULL;
	guint i;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	if (priv->cpts->len == 0)
		return NULL;

	priv->mode = AS_PARSER_MODE_DISTRO;

	root = xmlNewNode (NULL, (xmlChar*) "components");
	xmlNewProp (root, (xmlChar*) "version", (xmlChar*) "0.8");
	if (priv->origin_name != NULL)
		xmlNewProp (root, (xmlChar*) "origin", (xmlChar*) priv->origin_name);

	for (i = 0; i < priv->cpts->len; i++) {
		AsComponent *cpt;
		xmlNode *node;
		cpt = AS_COMPONENT (g_ptr_array_index (priv->cpts, i));

		node = as_metadata_component_to_node (metad, cpt);
		if (node == NULL)
			continue;
		xmlAddChild (root, node);
	}

	doc = xmlNewDoc ((xmlChar*) NULL);
	xmlDocSetRootElement (doc, root);

	xmlDocDumpMemory (doc, (xmlChar**) (&xmlstr), NULL);
	xmlFreeDoc (doc);

	return xmlstr;
}

/**
 * as_metadata_add_component:
 *
 * Add an #AsComponent to the list of components.
 * This can be used to add multiple components in order to
 * produce a distro-XML AppStream metadata file.
 */
void
as_metadata_add_component (AsMetadata *metad, AsComponent *cpt)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_ptr_array_add (priv->cpts, g_object_ref (cpt));
}

/**
 * as_metadata_get_component:
 * @metad: a #AsMetadata instance.
 *
 * Gets the #AsComponent which has been parsed from the XML.
 * If the AppStream XML contained multiple components, return the first
 * component that has been parsed.
 *
 * Returns: (transfer none): An #AsComponent or %NULL
 **/
AsComponent*
as_metadata_get_component (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	if (priv->cpts->len == 0)
		return NULL;
	return AS_COMPONENT (g_ptr_array_index (priv->cpts, 0));
}

/**
 * as_metadata_get_components:
 * @metad: a #AsMetadata instance.
 *
 * Returns: (transfer none) (element-type AsComponent): A #GPtrArray of all parsed components
 **/
GPtrArray*
as_metadata_get_components (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->cpts;
}

/**
 * as_metadata_set_locale:
 * @metad: a #AsMetadata instance.
 * @locale: the locale.
 *
 * Sets the locale which should be read when processing metadata.
 * All other locales are ignored, which increases parsing speed and
 * reduces memory usage.
 * If you set the locale to "ALL", all locales will be read.
 **/
void
as_metadata_set_locale (AsMetadata *metad, const gchar *locale)
{
	gchar **strv;
	AsMetadataPrivate *priv = GET_PRIVATE (metad);

	g_free (priv->locale);
	g_free (priv->locale_short);
	priv->locale = g_strdup (locale);

	strv = g_strsplit (priv->locale, "_", 0);
	priv->locale_short = g_strdup (strv[0]);
	g_strfreev (strv);
}

/**
 * as_metadata_get_locale:
 * @metad: a #AsMetadata instance.
 *
 * Gets the current active locale for parsing metadata,
 * or "ALL" if all locales are read.
 *
 * Returns: Locale used for metadata parsing.
 **/
const gchar *
as_metadata_get_locale (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->locale;
}

/**
 * as_metadata_set_origin:
 * @metad: an #AsMetadata instance.
 * @origin: the origin of AppStream distro metadata.
 *
 * Set the origin of AppStream distro metadata
 **/
void
as_metadata_set_origin (AsMetadata *metad, const gchar *origin)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	g_free (priv->origin_name);
	priv->origin_name = g_strdup (origin);
}

/**
 * as_metadata_get_origin:
 * @metad: an #AsMetadata instance.
 *
 * Returns: The origin of AppStream distro metadata
 **/
const gchar*
as_metadata_get_origin (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->origin_name;
}

/**
 * as_metadata_set_parser_mode:
 * @metad: a #AsMetadata instance.
 * @mode: the #AsParserMode.
 *
 * Sets the current metadata parsing mode.
 **/
void
as_metadata_set_parser_mode (AsMetadata *metad, AsParserMode mode)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	priv->mode = mode;
}

/**
 * as_metadata_get_parser_mode:
 * @metad: a #AsMetadata instance.
 *
 * Gets the current parser mode
 *
 * Returns: an #AsParserMode
 **/
AsParserMode
as_metadata_get_parser_mode (AsMetadata *metad)
{
	AsMetadataPrivate *priv = GET_PRIVATE (metad);
	return priv->mode;
}

/**
 * as_metadata_class_init:
 **/
static void
as_metadata_class_init (AsMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_metadata_finalize;
}

/**
 * as_metadata_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
as_metadata_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsMetadataError");
	return quark;
}

/**
 * as_metadata_new:
 *
 * Creates a new #AsMetadata.
 *
 * Returns: (transfer full): a #AsMetadata
 **/
AsMetadata*
as_metadata_new (void)
{
	AsMetadata *metad;
	metad = g_object_new (AS_TYPE_METADATA, NULL);
	return AS_METADATA (metad);
}
