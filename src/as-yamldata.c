/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-yamldata.h"

#include <glib.h>
#include <glib-object.h>
#include <yaml.h>

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-metadata.h"
#include "as-component-private.h"
#include "as-screenshot-private.h"

typedef struct
{
	gchar *locale;
	gchar *locale_short;
	gchar *origin;
	gchar *media_baseurl;

	gint default_priority;
} AsYAMLDataPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsYAMLData, as_yamldata, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_yamldata_get_instance_private (o))

enum YamlNodeKind {
	YAML_VAR,
	YAML_VAL,
	YAML_SEQ
};

/**
 * as_yamldata_init:
 **/
static void
as_yamldata_init (AsYAMLData *ydt)
{
	g_autofree gchar *str;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	/* set active locale without UTF-8 suffix */
	str = as_get_current_locale ();
	as_yamldata_set_locale (ydt, str);

	priv->origin = NULL;
	priv->media_baseurl = NULL;
	priv->default_priority = 0;
}

/**
 * as_yamldata_finalize:
 **/
static void
as_yamldata_finalize (GObject *object)
{
	AsYAMLData *ydt = AS_YAMLDATA (object);
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	g_free (priv->locale);
	g_free (priv->locale_short);
	g_free (priv->origin);
	g_free (priv->media_baseurl);

	G_OBJECT_CLASS (as_yamldata_parent_class)->finalize (object);
}

/**
 * as_yamldata_initialize:
 * @ydt: An instance of #AsYAMLData
 *
 * Initialize the YAML handler.
 */
void
as_yamldata_initialize (AsYAMLData *ydt, const gchar *locale, const gchar *origin, const gchar *media_baseurl, gint priority)
{
	g_auto(GStrv) strv = NULL;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	g_free (priv->locale);
	g_free (priv->locale_short);
	priv->locale = g_strdup (locale);

	strv = g_strsplit (priv->locale, "_", 0);
	priv->locale_short = g_strdup (strv[0]);

	g_free (priv->origin);
	priv->origin = g_strdup (origin);

	g_free (priv->media_baseurl);
	priv->media_baseurl = g_strdup (media_baseurl);

	priv->default_priority = priority;
}

/**
 * dep11_print_unknown:
 */
static void
dep11_print_unknown (const gchar *root, const gchar *key)
{
	g_debug ("DEP11: Unknown key '%s/%s' found.", root, key);
}

/**
 * as_yamldata_free_node:
 */
static gboolean
as_yamldata_free_node (GNode *node, gpointer data)
{
	if (node->data != NULL)
		g_free (node->data);

	return FALSE;
}

/**
 * dep11_yaml_process_layer:
 *
 * Create GNode tree from DEP-11 YAML document
 */
static void
dep11_yaml_process_layer (yaml_parser_t *parser, GNode *data)
{
	GNode *last_leaf = data;
	GNode *last_scalar;
	yaml_event_t event;
	gboolean parse = TRUE;
	gboolean in_sequence = FALSE;
	int storage = YAML_VAR; /* the first element must always be of type VAR */

	while (parse) {
		yaml_parser_parse (parser, &event);

		/* Parse value either as a new leaf in the mapping
		 * or as a leaf value (one of them, in case it's a sequence) */
		switch (event.type) {
			case YAML_SCALAR_EVENT:
				if (storage)
					g_node_append_data (last_leaf, g_strdup ((gchar*) event.data.scalar.value));
				else
					last_leaf = g_node_append (data, g_node_new (g_strdup ((gchar*) event.data.scalar.value)));
				storage ^= YAML_VAL;
				break;
			case YAML_SEQUENCE_START_EVENT:
				storage = YAML_SEQ;
				in_sequence = TRUE;
				break;
			case YAML_SEQUENCE_END_EVENT:
				storage = YAML_VAR;
				in_sequence = FALSE;
				break;
			case YAML_MAPPING_START_EVENT:
				/* depth += 1 */
				last_scalar = last_leaf;
				if (in_sequence)
					last_leaf = g_node_append (last_leaf, g_node_new (g_strdup ("-")));
				dep11_yaml_process_layer (parser, last_leaf);
				last_leaf = last_scalar;
				storage ^= YAML_VAL; /* Flip VAR/VAL, without touching SEQ */
				break;
			case YAML_MAPPING_END_EVENT:
			case YAML_STREAM_END_EVENT:
			case YAML_DOCUMENT_END_EVENT:
				/* depth -= 1 */
				parse = FALSE;
				break;
			default:
				break;
		}

		yaml_event_delete (&event);
	}
}

/**
 * as_yamldata_get_localized_node:
 */
static GNode*
as_yamldata_get_localized_node (AsYAMLData *ydt, GNode *node, gchar *locale_override)
{
	GNode *n;
	GNode *tnode = NULL;
	gchar *key;
	const gchar *locale;
	const gchar *locale_short = NULL;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	if (locale_override == NULL) {
		locale = priv->locale;
		locale_short = priv->locale_short;
	} else {
		locale = locale_override;
		locale_short = NULL;
	}

	for (n = node->children; n != NULL; n = n->next) {
			key = (gchar*) n->data;

			if ((tnode == NULL)	&& (g_strcmp0 (key, "C") == 0)) {
				tnode = n;
				if (locale == NULL)
					goto out;
			}

			if (g_strcmp0 (key, locale) == 0) {
				tnode = n;
				goto out;
			}

			if (g_strcmp0 (key, locale_short) == 0) {
				tnode = n;
				goto out;
			}
	}

out:
	return tnode;
}

/**
 * as_yamldata_get_localized_value:
 *
 * Get localized string from a translated DEP-11 key
 */
static gchar*
as_yamldata_get_localized_value (AsYAMLData *ydt, GNode *node, gchar *locale_override)
{
	GNode *tnode;

	tnode = as_yamldata_get_localized_node (ydt, node, locale_override);
	if (tnode == NULL)
		return NULL;

	return g_strdup ((gchar*) tnode->children->data);
}

/**
 * dep11_list_to_string_array:
 */
static void
dep11_list_to_string_array (GNode *node, GPtrArray *array)
{
	GNode *n;

	for (n = node->children; n != NULL; n = n->next) {
		g_ptr_array_add (array, g_strdup ((gchar*) n->data));
	}
}

/**
 * as_yamldata_process_keywords:
 *
 * Process a keywords node and add the data to an #AsComponent
 */
static void
as_yamldata_process_keywords (AsYAMLData *ydt, GNode *node, AsComponent *cpt)
{
	GNode *tnode;
	GPtrArray *keywords;
	gchar **strv;

	keywords = g_ptr_array_new_with_free_func (g_free);

	tnode = as_yamldata_get_localized_node (ydt, node, NULL);
	/* no node found? */
	if (tnode == NULL)
		return;

	dep11_list_to_string_array (tnode, keywords);

	strv = as_ptr_array_to_strv (keywords);
	as_component_set_keywords (cpt, strv, NULL);
	g_ptr_array_unref (keywords);
	g_strfreev (strv);
}

/**
 * dep11_process_urls:
 */
static void
dep11_process_urls (GNode *node, AsComponent *cpt)
{
	GNode *n;
	gchar *key;
	gchar *value;
	AsUrlKind url_kind;

	for (n = node->children; n != NULL; n = n->next) {
			key = (gchar*) n->data;
			value = (gchar*) n->children->data;

			url_kind = as_url_kind_from_string (key);
			if ((url_kind != AS_URL_KIND_UNKNOWN) && (value != NULL))
				as_component_add_url (cpt, url_kind, value);
	}
}

/**
 * as_yamldata_process_icons:
 */
static void
as_yamldata_process_icons (AsYAMLData *ydt, GNode *node, AsComponent *cpt)
{
	GNode *n;
	gchar *key;
	gchar *value;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	for (n = node->children; n != NULL; n = n->next) {
		g_autoptr(AsIcon) icon = NULL;

		key = (gchar*) n->data;
		value = (gchar*) n->children->data;
		icon = as_icon_new ();

		if (g_strcmp0 (key, "stock") == 0) {
			as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
			as_icon_set_name (icon, value);
			as_component_add_icon (cpt, icon);
		} else if (g_strcmp0 (key, "cached") == 0) {
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
			as_icon_set_filename (icon, value);
			as_component_add_icon (cpt, icon);
		} else if (g_strcmp0 (key, "local") == 0) {
			as_icon_set_kind (icon, AS_ICON_KIND_LOCAL);
			as_icon_set_filename (icon, value);
			as_component_add_icon (cpt, icon);
		} else if (g_strcmp0 (key, "remote") == 0) {
			as_icon_set_kind (icon, AS_ICON_KIND_REMOTE);
			if (priv->media_baseurl == NULL) {
				/* no baseurl, we can just set the value as URL */
				as_icon_set_url (icon, value);
			} else {
				/* handle the media baseurl */
				gchar *tmp;
				tmp = g_build_filename (priv->media_baseurl, value, NULL);
				as_icon_set_url (icon, tmp);
				g_free (tmp);
			}
			as_component_add_icon (cpt, icon);
		}
	}
}

/**
 * dep11_process_provides:
 */
static void
dep11_process_provides (GNode *node, AsComponent *cpt)
{
	GNode *n;
	GNode *sn;
	gchar *key;

	for (n = node->children; n != NULL; n = n->next) {
		key = (gchar*) n->data;

		if (g_strcmp0 (key, "libraries") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_LIBRARY, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "binaries") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_BINARY, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "fonts") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FONT, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "modaliases") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_MODALIAS, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "firmware") == 0) {
			GNode *dn;
			for (sn = n->children; sn != NULL; sn = sn->next) {
				gchar *kind = NULL;
				gchar *fwdata = NULL;
				for (dn = sn->children; dn != NULL; dn = dn->next) {
					gchar *dkey;
					gchar *dvalue;

					dkey = (gchar*) dn->data;
					if (dn->children)
						dvalue = (gchar*) dn->children->data;
					else
						continue;
					if (g_strcmp0 (dkey, "type") == 0) {
						kind = dvalue;
					} else if ((g_strcmp0 (dkey, "guid") == 0) || (g_strcmp0 (dkey, "fname") == 0)) {
						fwdata = dvalue;
					}
				}
				/* we don't add malformed provides types */
				if ((kind == NULL) || (fwdata == NULL))
					continue;

				if (g_strcmp0 (kind, "runtime") == 0)
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FIRMWARE_RUNTIME, fwdata);
				else if (g_strcmp0 (kind, "flashed") == 0)
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_FIRMWARE_FLASHED, fwdata);
			}
		} else if (g_strcmp0 (key, "python2") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_PYTHON_2, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "python3") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_PYTHON, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "mimetypes") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				as_component_add_provided_item (cpt, AS_PROVIDED_KIND_MIMETYPE, (gchar*) sn->data);
			}
		} else if (g_strcmp0 (key, "dbus") == 0) {
			GNode *dn;
			for (sn = n->children; sn != NULL; sn = sn->next) {
				gchar *kind = NULL;
				gchar *service = NULL;
				for (dn = sn->children; dn != NULL; dn = dn->next) {
					gchar *dkey;
					gchar *dvalue;

					dkey = (gchar*) dn->data;
					if (dn->children)
						dvalue = (gchar*) dn->children->data;
					else
						dvalue = NULL;
					if (g_strcmp0 (dkey, "type") == 0) {
						kind = dvalue;
					} else if (g_strcmp0 (dkey, "service") == 0) {
						service = dvalue;
					}
				}
				/* we don't add malformed provides types */
				if ((kind == NULL) || (service == NULL))
					continue;

				if (g_strcmp0 (kind, "system") == 0)
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_DBUS_SYSTEM, service);
				else if ((g_strcmp0 (kind, "user") == 0) || (g_strcmp0 (kind, "session") == 0))
					as_component_add_provided_item (cpt, AS_PROVIDED_KIND_DBUS_USER, service);
			}
		}
	}
}

/**
 * dep11_process_image:
 */
static void
dep11_process_image (AsYAMLData *ydt, GNode *node, AsScreenshot *scr)
{
	GNode *n;
	AsImage *img;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	img = as_image_new ();

	for (n = node->children; n != NULL; n = n->next) {
		gchar *key;
		gchar *value;
		guint64 size;

		key = (gchar*) n->data;
		if (n->children)
			value = (gchar*) n->children->data;
		else
			continue; /* there should be no key without value */

		if (g_strcmp0 (key, "width") == 0) {
			size = g_ascii_strtoll (value, NULL, 10);
			as_image_set_width (img, size);
		} else if (g_strcmp0 (key, "height") == 0) {
			size = g_ascii_strtoll (value, NULL, 10);
			as_image_set_height (img, size);
		} else if (g_strcmp0 (key, "url") == 0) {
			if (priv->media_baseurl == NULL) {
				/* no baseurl, we can just set the value as URL */
				as_image_set_url (img, value);
			} else {
				/* handle the media baseurl */
				gchar *tmp;
				tmp = g_build_filename (priv->media_baseurl, value, NULL);
				as_image_set_url (img, tmp);
				g_free (tmp);
			}
		} else {
			dep11_print_unknown ("image", key);
		}
	}

	as_screenshot_add_image (scr, img);
	g_object_unref (img);
}

/**
 * as_yamldata_process_screenshots:
 */
static void
as_yamldata_process_screenshots (AsYAMLData *ydt, GNode *node, AsComponent *cpt)
{
	GNode *sn;

	for (sn = node->children; sn != NULL; sn = sn->next) {
		GNode *n;
		AsScreenshot *scr;
		scr = as_screenshot_new ();

		/* propagate locale */
		as_screenshot_set_active_locale (scr, as_component_get_active_locale (cpt));

		for (n = sn->children; n != NULL; n = n->next) {
			GNode *in;
			gchar *key;
			gchar *value;

			key = (gchar*) n->data;
			if (n->children)
				value = (gchar*) n->children->data;
			else
				value = NULL;

			if (g_strcmp0 (key, "default") == 0) {
				if (g_strcmp0 (value, "yes") == 0)
					as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_DEFAULT);
				else
					as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_EXTRA);
			} else if (g_strcmp0 (key, "caption") == 0) {
				gchar *lvalue;
				/* the caption is a localized element */
				lvalue = as_yamldata_get_localized_value (ydt, n, NULL);
				as_screenshot_set_caption (scr, lvalue, NULL);
			} else if (g_strcmp0 (key, "source-image") == 0) {
				/* there can only be one source image */
				dep11_process_image (ydt, n, scr);
			} else if (g_strcmp0 (key, "thumbnails") == 0) {
				/* the thumbnails are a list of images */
				for (in = n->children; in != NULL; in = in->next) {
					dep11_process_image (ydt, in, scr);
				}
			} else {
				dep11_print_unknown ("screenshot", key);
			}
		}

		/* add the result */
		as_component_add_screenshot (cpt, scr);
		g_object_unref (scr);
	}
}

/**
 * as_yamldata_process_releases:
 *
 * Add #AsRelease instances to the #AsComponent
 */
static void
as_yamldata_process_releases (AsYAMLData *ydt, GNode *node, AsComponent *cpt)
{
	GNode *sn;

	for (sn = node->children; sn != NULL; sn = sn->next) {
		GNode *n;
		AsRelease *rel;
		rel = as_release_new ();

		/* propagate locale */
		as_release_set_active_locale (rel, as_component_get_active_locale (cpt));

		for (n = sn->children; n != NULL; n = n->next) {
			gchar *key;
			gchar *value;

			key = (gchar*) n->data;
			if (n->children)
				value = (gchar*) n->children->data;
			else
				value = NULL;

			if (g_strcmp0 (key, "unix-timestamp") == 0) {
				as_release_set_timestamp (rel, g_ascii_strtoll (value, NULL, 10));
			} else if (g_strcmp0 (key, "date") == 0) {
				g_autoptr(GDateTime) time;
				time = as_iso8601_to_datetime (value);
				if (time != NULL) {
					as_release_set_timestamp (rel, g_date_time_to_unix (time));
				} else {
					g_debug ("Invalid ISO-8601 date in releases of %s", as_component_get_id (cpt));
				}
			} else if (g_strcmp0 (key, "version") == 0) {
				as_release_set_version (rel, value);
			} else if (g_strcmp0 (key, "description") == 0) {
				gchar *lvalue;
				lvalue = as_yamldata_get_localized_value (ydt, node, NULL);
				as_release_set_description (rel, lvalue, NULL);
				g_free (lvalue);
			} else {
				dep11_print_unknown ("release", key);
			}
		}

		/* add the result */
		as_component_add_release (cpt, rel);
		g_object_unref (rel);
	}
}

/**
 * as_yamldata_process_component_node:
 */
static AsComponent*
as_yamldata_process_component_node (AsYAMLData *ydt, GNode *root)
{
	GNode *node;
	AsComponent *cpt;

	gchar **strv;
	GPtrArray *categories;
	GPtrArray *compulsory_for_desktops;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	cpt = as_component_new ();

	categories = g_ptr_array_new_with_free_func (g_free);
	compulsory_for_desktops = g_ptr_array_new_with_free_func (g_free);

	/* set active locale for this component */
	as_component_set_active_locale (cpt, priv->locale);

	for (node = root->children; node != NULL; node = node->next) {
		gchar *key;
		gchar *value;
		gchar *lvalue;

		key = (gchar*) node->data;
		value = (gchar*) node->children->data;
		g_strstrip (value);

		if (g_strcmp0 (key, "Type") == 0) {
			if (g_strcmp0 (value, "desktop-app") == 0)
				as_component_set_kind (cpt, AS_COMPONENT_KIND_DESKTOP_APP);
			else if (g_strcmp0 (value, "generic") == 0)
				as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
			else
				as_component_set_kind (cpt, as_component_kind_from_string (value));
		} else if (g_strcmp0 (key, "ID") == 0) {
			as_component_set_id (cpt, value);
		} else if (g_strcmp0 (key, "Package") == 0) {
			gchar **strv;
			strv = g_new0 (gchar*, 1 + 2);
			strv[0] = g_strdup (value);
			strv[1] = NULL;

			as_component_set_pkgnames (cpt, strv);
			g_strfreev (strv);
		} else if (g_strcmp0 (key, "SourcePackage") == 0) {
			as_component_set_source_pkgname (cpt, value);
		} else if (g_strcmp0 (key, "Name") == 0) {
			lvalue = as_yamldata_get_localized_value (ydt, node, "C");
			g_strstrip (lvalue);
			if (lvalue != NULL) {
				as_component_set_name (cpt, lvalue, "C"); /* Unlocalized */
				g_free (lvalue);
			}
			lvalue = as_yamldata_get_localized_value (ydt, node, NULL);
			as_component_set_name (cpt, lvalue, NULL);
			g_free (lvalue);
		} else if (g_strcmp0 (key, "Summary") == 0) {
			lvalue = as_yamldata_get_localized_value (ydt, node, NULL);
			g_strstrip (lvalue);
			as_component_set_summary (cpt, lvalue, NULL);
			g_free (lvalue);
		} else if (g_strcmp0 (key, "Description") == 0) {
			lvalue = as_yamldata_get_localized_value (ydt, node, NULL);
			g_strstrip (lvalue);
			as_component_set_description (cpt, lvalue, NULL);
			g_free (lvalue);
		} else if (g_strcmp0 (key, "DeveloperName") == 0) {
			lvalue = as_yamldata_get_localized_value (ydt, node, NULL);
			g_strstrip (lvalue);
			as_component_set_developer_name (cpt, lvalue, NULL);
			g_free (lvalue);
		} else if (g_strcmp0 (key, "ProjectLicense") == 0) {
			as_component_set_project_license (cpt, value);
		} else if (g_strcmp0 (key, "ProjectGroup") == 0) {
			as_component_set_project_group (cpt, value);
		} else if (g_strcmp0 (key, "Categories") == 0) {
			dep11_list_to_string_array (node, categories);
		} else if (g_strcmp0 (key, "CompulsoryForDesktops") == 0) {
			dep11_list_to_string_array (node, compulsory_for_desktops);
		} else if (g_strcmp0 (key, "Extends") == 0) {
			dep11_list_to_string_array (node, as_component_get_extends (cpt));
		} else if (g_strcmp0 (key, "Keywords") == 0) {
			as_yamldata_process_keywords (ydt, node, cpt);
		} else if (g_strcmp0 (key, "Url") == 0) {
			dep11_process_urls (node, cpt);
		} else if (g_strcmp0 (key, "Icon") == 0) {
			as_yamldata_process_icons (ydt, node, cpt);
		} else if (g_strcmp0 (key, "Provides") == 0) {
			dep11_process_provides (node, cpt);
		} else if (g_strcmp0 (key, "Screenshots") == 0) {
			as_yamldata_process_screenshots (ydt, node, cpt);
		} else if (g_strcmp0 (key, "Releases") == 0) {
			as_yamldata_process_releases (ydt, node, cpt);
		} else {
			dep11_print_unknown ("root", key);
		}
	}

	/* set component origin */
	as_component_set_origin (cpt, priv->origin);

	/* set component priority */
	as_component_set_priority (cpt, priv->default_priority);

	/* add category information to component */
	strv = as_ptr_array_to_strv (categories);
	as_component_set_categories (cpt, strv);
	g_ptr_array_unref (categories);
	g_strfreev (strv);

	/* add desktop-compulsority information to component */
	strv = as_ptr_array_to_strv (compulsory_for_desktops);
	as_component_set_compulsory_for_desktops (cpt, strv);
	g_ptr_array_unref (compulsory_for_desktops);
	g_strfreev (strv);

	return cpt;
}

/**
 * as_yaml_mapping_start:
 */
static void
as_yaml_emit_scalar (yaml_emitter_t *emitter, const gchar *value)
{
	gint ret;
	yaml_event_t event;
	g_assert (value != NULL);

	yaml_scalar_event_initialize (&event, NULL, NULL, (yaml_char_t*) value, strlen (value), TRUE, TRUE, YAML_ANY_SCALAR_STYLE);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);
}

/**
 * as_yaml_emit_entry:
 */
static void
as_yaml_emit_entry (yaml_emitter_t *emitter, const gchar *key, const gchar *value)
{
	yaml_event_t event;
	gint ret;

	if (value == NULL)
		return;

	yaml_scalar_event_initialize (&event, NULL, NULL, (yaml_char_t*) key, strlen (key), TRUE, TRUE, YAML_ANY_SCALAR_STYLE);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);

	yaml_scalar_event_initialize (&event, NULL, NULL, (yaml_char_t*) value, strlen (value), TRUE, TRUE, YAML_ANY_SCALAR_STYLE);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);
}

/**
 * as_yaml_mapping_start:
 */
static void
as_yaml_mapping_start (yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_mapping_start_event_initialize (&event, NULL, NULL, 1, YAML_ANY_MAPPING_STYLE);
	g_assert (yaml_emitter_emit (emitter, &event));
}

/**
 * as_yaml_mapping_end:
 */
static void
as_yaml_mapping_end (yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_mapping_end_event_initialize (&event);
	g_assert (yaml_emitter_emit (emitter, &event));
}

/**
 * as_yaml_sequence_start:
 */
static void
as_yaml_sequence_start (yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_sequence_start_event_initialize (&event, NULL, NULL, 1, YAML_ANY_SEQUENCE_STYLE);
	g_assert (yaml_emitter_emit (emitter, &event));
}

/**
 * as_yaml_sequence_end:
 */
static void
as_yaml_sequence_end (yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_sequence_end_event_initialize (&event);
	g_assert (yaml_emitter_emit (emitter, &event));
}

/**
 * as_yaml_emit_lang_hashtable_entries:
 */
static void
as_yaml_emit_lang_hashtable_entries (gchar *key, gchar *value, yaml_emitter_t *emitter)
{
	if (as_str_empty (value))
		return;

	g_strstrip (value);
	as_yaml_emit_entry (emitter, key, value);
}

/**
 * as_yaml_emit_localized_entry:
 */
static void
as_yaml_emit_localized_entry (yaml_emitter_t *emitter, const gchar *key, GHashTable *ltab)
{
	if (ltab == NULL)
		return;
	if (g_hash_table_size (ltab) == 0)
		return;

	as_yaml_emit_scalar (emitter, key);

	/* start mapping for localized entry */
	as_yaml_mapping_start (emitter);
	/* emit entries */
	g_hash_table_foreach (ltab,
				(GHFunc) as_yaml_emit_lang_hashtable_entries,
				emitter);
	/* finalize */
	as_yaml_mapping_end (emitter);
}

/**
 * as_yaml_emit_sequence:
 */
static void
as_yaml_emit_sequence (yaml_emitter_t *emitter, const gchar *key, GPtrArray *list)
{
	guint i;

	if (list == NULL)
		return;
	if (list->len == 0)
		return;

	as_yaml_emit_scalar (emitter, key);

	as_yaml_sequence_start (emitter);
	for (i = 0; i < list->len; i++) {
		const gchar *value = (const gchar *) g_ptr_array_index (list, i);
		as_yaml_emit_scalar (emitter, value);
	}
	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_emit_sequence_from_strv:
 */
static void
as_yaml_emit_sequence_from_strv (yaml_emitter_t *emitter, const gchar *key, gchar **strv)
{
	guint i;

	if (strv == NULL)
		return;

	as_yaml_emit_scalar (emitter, key);

	as_yaml_sequence_start (emitter);
	for (i = 0; strv[i] != NULL; i++) {
		if (as_str_empty (strv[i]))
			continue;
		as_yaml_emit_scalar (emitter, strv[i]);
	}
	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_localized_list_helper:
 */
static void
as_yaml_localized_list_helper (gchar *key, gchar **strv, yaml_emitter_t *emitter)
{
	guint i;
	if (strv == NULL)
		return;

	as_yaml_emit_scalar (emitter, key);
	as_yaml_sequence_start (emitter);
	for (i = 0; strv[i] != NULL; i++) {
		as_yaml_emit_scalar (emitter, strv[i]);
	}
	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_emit_localized_lists:
 */
void
as_yaml_emit_localized_lists (yaml_emitter_t *emitter, const gchar *key, GHashTable *ltab)
{
	if (ltab == NULL)
		return;
	if (g_hash_table_size (ltab) == 0)
		return;

	as_yaml_emit_scalar (emitter, key);

	/* start mapping for localized entry */
	as_yaml_mapping_start (emitter);
	/* emit entries */
	g_hash_table_foreach (ltab,
				(GHFunc) as_yaml_localized_list_helper,
				emitter);
	/* finalize */
	as_yaml_mapping_end (emitter);
}

/**
 * as_yaml_emit_provides:
 */
void
as_yaml_emit_provides (yaml_emitter_t *emitter, AsComponent *cpt)
{
	GList *l;
	GList *plist;
	guint i;

	g_auto(GStrv) dbus_system = NULL;
	g_auto(GStrv) dbus_user = NULL;

	g_auto(GStrv) fw_runtime = NULL;
	g_auto(GStrv) fw_flashed = NULL;

	plist = as_component_get_provided (cpt);
	if (plist == NULL)
		return;

	as_yaml_emit_scalar (emitter, "Provides");
	as_yaml_mapping_start (emitter);
	for (l = plist; l != NULL; l = l->next) {
		AsProvidedKind kind;
		g_autofree gchar **items = NULL;
		AsProvided *prov = AS_PROVIDED (l->data);

		items = as_provided_get_items (prov);
		if (items == NULL)
			continue;


		kind = as_provided_get_kind (prov);

		switch (kind) {
			case AS_PROVIDED_KIND_LIBRARY:
				as_yaml_emit_sequence_from_strv (emitter,
							"libraries",
							items);
				break;
			case AS_PROVIDED_KIND_BINARY:
				as_yaml_emit_sequence_from_strv (emitter,
							"binaries",
							items);
				break;
			case AS_PROVIDED_KIND_MIMETYPE:
				as_yaml_emit_sequence_from_strv (emitter,
							"mimetypes",
							items);
				break;
			case AS_PROVIDED_KIND_PYTHON_2:
				as_yaml_emit_sequence_from_strv (emitter,
							"python2",
							items);
				break;
			case AS_PROVIDED_KIND_PYTHON:
				as_yaml_emit_sequence_from_strv (emitter,
							"python3",
							items);
				break;
			case AS_PROVIDED_KIND_MODALIAS:
				as_yaml_emit_sequence_from_strv (emitter,
							"modaliases",
							items);
				break;
			case AS_PROVIDED_KIND_FONT:
				as_yaml_emit_scalar (emitter, "fonts");

				as_yaml_sequence_start (emitter);
				for (i = 0; items[i] != NULL; i++) {
					as_yaml_mapping_start (emitter);
					as_yaml_emit_entry (emitter, "name", items[i]);
					/* FIXME: Also emit "file" entry, but at time we don't seem to store this? */
					as_yaml_mapping_end (emitter);
				}
				as_yaml_sequence_end (emitter);
				break;
			case AS_PROVIDED_KIND_DBUS_SYSTEM:
				if (dbus_system == NULL) {
					dbus_system = g_strdupv (items);
				} else {
					g_critical ("Hit dbus:system twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_DBUS_USER:
				if (dbus_user == NULL) {
					dbus_user = g_strdupv (items);
				} else {
					g_critical ("Hit dbus:user twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_FIRMWARE_RUNTIME:
				if (fw_runtime == NULL) {
					fw_runtime = g_strdupv (items);
				} else {
					g_critical ("Hit firmware:runtime twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_FIRMWARE_FLASHED:
				if (fw_flashed == NULL) {
					fw_flashed = g_strdupv (items);
				} else {
					g_critical ("Hit dbus-user twice, this should never happen!");
				}
				break;
			default:
				g_warning ("Ignoring unknown type of provided items: %s", as_provided_kind_to_string (kind));
				break;
		}
	}

	/* dbus subsection */
	if ((dbus_system != NULL) || (dbus_user != NULL)) {
		as_yaml_emit_scalar (emitter, "dbus");
		as_yaml_sequence_start (emitter);

		if (dbus_system != NULL) {
			for (i = 0; dbus_system[i] != NULL; i++) {
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "system");
				as_yaml_emit_entry (emitter, "service", dbus_system[i]);

				as_yaml_mapping_end (emitter);
			}
		}

		if (dbus_user != NULL) {
			for (i = 0; dbus_user[i] != NULL; i++) {
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "user");
				as_yaml_emit_entry (emitter, "service", dbus_user[i]);

				as_yaml_mapping_end (emitter);
			}
		}

		as_yaml_sequence_end (emitter);
	}

	/* firmware subsection */
	if ((fw_runtime != NULL) || (fw_flashed != NULL)) {
		as_yaml_emit_scalar (emitter, "firmware");
		as_yaml_sequence_start (emitter);

		if (fw_runtime != NULL) {
			for (i = 0; fw_runtime[i] != NULL; i++) {
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "runtime");
				as_yaml_emit_entry (emitter, "guid", fw_runtime[i]);

				as_yaml_mapping_end (emitter);
			}
		}

		if (fw_flashed != NULL) {
			for (i = 0; fw_flashed[i] != NULL; i++) {
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "flashed");
				as_yaml_emit_entry (emitter, "fname", fw_flashed[i]);

				as_yaml_mapping_end (emitter);
			}
		}

		as_yaml_sequence_end (emitter);
	}

	as_yaml_mapping_end (emitter);
}

/**
 * as_yaml_emit_image:
 *
 * Helper for as_yaml_emit_screenshots
 */
static void
as_yaml_emit_image (AsYAMLData *ydt, yaml_emitter_t *emitter, AsImage *img)
{
	g_autofree gchar *url = NULL;
	gchar *size;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	as_yaml_mapping_start (emitter);
	if (priv->media_baseurl == NULL)
		url = g_strdup (as_image_get_url (img));
	else
		url = as_str_replace (as_image_get_url (img), priv->media_baseurl, "");

	g_strstrip (url);
	as_yaml_emit_entry (emitter, "url", url);
	if ((as_image_get_width (img) > 0) &&
		(as_image_get_height (img) > 0)) {
		size = g_strdup_printf("%i", as_image_get_width (img));
		as_yaml_emit_entry (emitter, "width", size);
		g_free (size);

		size = g_strdup_printf("%i", as_image_get_height (img));
		as_yaml_emit_entry (emitter, "height", size);
		g_free (size);
	}
	as_yaml_mapping_end (emitter);
}

/**
 * as_yaml_emit_screenshots:
 */
void
as_yaml_emit_screenshots (AsYAMLData *ydt, yaml_emitter_t *emitter, AsComponent *cpt)
{
	GPtrArray *sslist;
	AsScreenshot *scr;
	guint i;

	sslist = as_component_get_screenshots (cpt);
	if ((sslist == NULL) || (sslist->len == 0))
		return;

	as_yaml_emit_scalar (emitter, "Screenshots");
	as_yaml_sequence_start (emitter);
	for (i = 0; i < sslist->len; i++) {
		GPtrArray *images;
		guint j;
		AsImage *source_img = NULL;
		scr = AS_SCREENSHOT (g_ptr_array_index (sslist, i));

		as_yaml_mapping_start (emitter);

		if (as_screenshot_get_kind (scr) == AS_SCREENSHOT_KIND_DEFAULT)
			as_yaml_emit_entry (emitter, "default", "true");

		as_yaml_emit_localized_entry (emitter,
						"caption",
						as_screenshot_get_caption_table (scr));

		images = as_screenshot_get_images (scr);
		as_yaml_emit_scalar (emitter, "thumbnails");
		as_yaml_sequence_start (emitter);
		for (j = 0; j < images->len; j++) {
			AsImage *img = AS_IMAGE (g_ptr_array_index (images, j));

			if (as_image_get_kind (img) == AS_IMAGE_KIND_SOURCE) {
				source_img = img;
				continue;
			}
			as_yaml_emit_image (ydt, emitter, img);
		}
		as_yaml_sequence_end (emitter);

		/* technically, we *must* have a source-image by now, but better be safe... */
		if (source_img != NULL) {
			as_yaml_emit_scalar (emitter, "source-image");
			as_yaml_emit_image (ydt, emitter, source_img);
		}

		as_yaml_mapping_end (emitter);
	}
	as_yaml_sequence_end (emitter);


}

/**
 * as_yamldata_process_component_node:
 */
static void
as_yaml_serialize_component (AsYAMLData *ydt, yaml_emitter_t *emitter, AsComponent *cpt)
{
	guint i;
	gint res;
	const gchar *cstr;
	gchar **pkgnames;
	yaml_event_t event;
	AsComponentKind kind;
	GHashTable *htable;
	GPtrArray *icons;

	/* we only serialize a component with minimal necessary information */
	if (!as_component_is_valid (cpt))
		return;

	/* new document for this component */
	yaml_document_start_event_initialize (&event, NULL, NULL, NULL, FALSE);
	res = yaml_emitter_emit (emitter, &event);
	g_assert (res);

	/* open main mapping */
	as_yaml_mapping_start (emitter);

	/* write component kind */
	kind = as_component_get_kind (cpt);
	if (kind == AS_COMPONENT_KIND_DESKTOP_APP)
		cstr = "desktop-app";
	else if (kind == AS_COMPONENT_KIND_GENERIC)
		cstr = "generic";
	else
		cstr = as_component_kind_to_string (kind);
	as_yaml_emit_entry (emitter, "Type", cstr);

	/* AppStream-ID */
	as_yaml_emit_entry (emitter, "ID", as_component_get_id (cpt));

	/* SourcePackage */
	as_yaml_emit_entry (emitter, "SourcePackage", as_component_get_source_pkgname (cpt));

	/* Package */
	pkgnames = as_component_get_pkgnames (cpt);
	/* NOTE: a DEP-11 component does *not* support multiple packages per component */
	if ((pkgnames != NULL) && (pkgnames[0] != '\0'))
		as_yaml_emit_entry (emitter, "Package", pkgnames[0]);

	/* Extends */
	as_yaml_emit_sequence (emitter,
				"Extends",
				as_component_get_extends (cpt));

	/* Name */
	as_yaml_emit_localized_entry (emitter,
					"Name",
					as_component_get_name_table (cpt));

	/* Summary */
	as_yaml_emit_localized_entry (emitter,
					"Summary",
					as_component_get_summary_table (cpt));

	/* Description */
	as_yaml_emit_localized_entry (emitter,
					"Description",
					as_component_get_description_table (cpt));

	/* DeveloperName */
	as_yaml_emit_localized_entry (emitter,
					"DeveloperName",
					as_component_get_developer_name_table (cpt));

	/* ProjectGroup */
	as_yaml_emit_entry (emitter, "ProjectGroup", as_component_get_project_group (cpt));

	/* ProjectLicense */
	as_yaml_emit_entry (emitter, "ProjectLicense", as_component_get_project_license (cpt));

	/* CompulsoryForDesktops */
	as_yaml_emit_sequence_from_strv (emitter,
					 "CompulsoryForDesktops",
					 as_component_get_compulsory_for_desktops (cpt));

	/* Categories */
	as_yaml_emit_sequence_from_strv (emitter,
					 "Categories",
					 as_component_get_categories (cpt));

	/* Keywords */
	as_yaml_emit_localized_lists (emitter,
					"Keywords",
					as_component_get_keywords_table (cpt));

	/* Urls */
	htable = as_component_get_urls_table (cpt);
	if ((htable != NULL) && (g_hash_table_size (htable) > 0)) {
		as_yaml_emit_scalar (emitter, "Urls");

		as_yaml_mapping_start (emitter);
		for (i = AS_URL_KIND_UNKNOWN; i < AS_URL_KIND_LAST; i++) {
			const gchar *value;
			value = as_component_get_url (cpt, i);
			if (value == NULL)
				continue;

			as_yaml_emit_entry (emitter, as_url_kind_to_string (i), value);
		}
		as_yaml_mapping_end (emitter);
	}

	/* Icons */
	icons = as_component_get_icons (cpt);
	if (icons->len > 0) {
		as_yaml_emit_scalar (emitter, "Icon");

		as_yaml_mapping_start (emitter);
		for (i = 0; i < icons->len; i++) {
			const gchar *value;
			AsIconKind ikind;
			AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

			ikind = as_icon_get_kind (icon);
			if (ikind == AS_ICON_KIND_LOCAL)
				value = as_icon_get_filename (icon);
			else if (ikind == AS_ICON_KIND_REMOTE)
				value = as_icon_get_url (icon);
			else
				value = as_icon_get_name (icon);

			if (value == NULL)
				continue;

			if (ikind == AS_ICON_KIND_REMOTE) {
				/* remote icons get special treatment */

				g_warning ("Handling of 'remote' type DEP-11 icons is not yet implemented!");
				/* NOTE: A remote node is specified like this:
				 * Icons:
				 *   cached: foobar.png
				 *   remote:
				 *     - width: 64
				 *       height: 64
				 *       url: http://example.org/icons/foobar.png
				 */
			} else {
				as_yaml_emit_entry (emitter, as_icon_kind_to_string (ikind), value);
			}
		}
		as_yaml_mapping_end (emitter);
	}

	/* Bundles */
	htable = as_component_get_bundles_table (cpt);
	if ((htable != NULL) && (g_hash_table_size (htable) > 0)) {
		as_yaml_emit_scalar (emitter, "Bundles");
		as_yaml_mapping_start (emitter);
		for (i = AS_BUNDLE_KIND_UNKNOWN; i < AS_BUNDLE_KIND_LAST; i++) {
			const gchar *value;
			value = as_component_get_bundle_id (cpt, i);
			if (value == NULL)
				continue;

			as_yaml_emit_entry (emitter, as_bundle_kind_to_string (i), value);
		}
		as_yaml_mapping_end (emitter);
	}

	/* Provides */
	as_yaml_emit_provides (emitter, cpt);

	/* Screenshots */
	as_yaml_emit_screenshots (ydt, emitter, cpt);

	/* TODO: Releases, Translations */

	/* close main mapping */
	as_yaml_mapping_end (emitter);

	/* finalize the document */
	yaml_document_end_event_initialize (&event, 1);
	res = yaml_emitter_emit (emitter, &event);
	g_assert (res);
}

/**
 * as_yaml_write_header:
 *
 * Emit a DEP-11 header for the new document.
 */
static void
as_yaml_write_header (yaml_emitter_t *emitter, const gchar *origin, const gchar *media_baseurl)
{
	gint res;
	yaml_event_t event;

	yaml_document_start_event_initialize (&event, NULL, NULL, NULL, FALSE);
	res = yaml_emitter_emit (emitter, &event);
	g_assert (res);

	as_yaml_mapping_start (emitter);

	as_yaml_emit_entry (emitter, "File", "DEP-11");
	as_yaml_emit_entry (emitter, "Version", "0.8");
	as_yaml_emit_entry (emitter, "Origin", origin);
	if (media_baseurl != NULL)
		as_yaml_emit_entry (emitter, "MediaBaseUrl", media_baseurl);

	as_yaml_mapping_end (emitter);

	yaml_document_end_event_initialize (&event, 1);
	res = yaml_emitter_emit (emitter, &event);
	g_assert (res);
}

/**
 * as_yamldata_write_handler:
 *
 * Helper function to store the emitted YAML document.
 */
static int
as_yamldata_write_handler (void *ptr, unsigned char *buffer, size_t size)
{
	GString *str;
	str = (GString*) ptr;
	g_string_append_len (str, (const gchar*) buffer, size);

	return 1;
}

/**
 * as_yamldata_serialize_to_distro:
 */
gchar*
as_yamldata_serialize_to_distro (AsYAMLData *ydt, GPtrArray *cpts, gboolean write_header, gboolean add_timestamp, GError **error)
{
	yaml_emitter_t emitter;
	yaml_event_t event;
	GString *out_data;
	gboolean res = FALSE;
	guint i;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	if (cpts->len == 0)
		return NULL;

	yaml_emitter_initialize (&emitter);
	yaml_emitter_set_indent (&emitter, 2);
	yaml_emitter_set_unicode (&emitter, TRUE);
	yaml_emitter_set_width (&emitter, 120);

	/* create a GString to receive the output the emitter generates */
	out_data = g_string_new ("");
	yaml_emitter_set_output (&emitter, as_yamldata_write_handler, out_data);

	/* emit start event */
	yaml_stream_start_event_initialize (&event, YAML_UTF8_ENCODING);
	if (!yaml_emitter_emit (&emitter, &event))
		goto error;

	/* write header */
	if (write_header)
		as_yaml_write_header (&emitter, priv->origin, priv->media_baseurl);

	/* write components as YAML documents */
	for (i = 0; i < cpts->len; i++) {
		AsComponent *cpt;
		cpt = AS_COMPONENT (g_ptr_array_index (cpts, i));

		as_yaml_serialize_component (ydt, &emitter, cpt);
	}

	/* emit end event */
	yaml_stream_end_event_initialize (&event);
	yaml_emitter_emit (&emitter, &event);
	yaml_emitter_emit (&emitter, &event);

	res = TRUE;
	goto out;

error:
	g_set_error_literal (error,
				AS_METADATA_ERROR,
				AS_METADATA_ERROR_FAILED,
				"Emission of YAML event failed.");

out:
	yaml_emitter_flush (&emitter);
	/* destroy the Emitter object */
	yaml_emitter_delete (&emitter);

	if (res) {
		return g_string_free (out_data, FALSE);
	} else {
		g_string_free (out_data, TRUE);
		return NULL;
	}
}

/**
 * as_yamldata_parse_distro_data:
 * @ydt: An instance of #AsYAMLData
 * @data: YAML metadata to parse
 *
 * Read an array of #AsComponent from AppStream YAML metadata.
 *
 * Returns: (transfer full) (element-type AsComponent): An array of #AsComponent or %NULL
 */
GPtrArray*
as_yamldata_parse_distro_data (AsYAMLData *ydt, const gchar *data, GError **error)
{
	yaml_parser_t parser;
	yaml_event_t event;
	gboolean header = TRUE;
	gboolean parse = TRUE;
	GPtrArray *cpts = NULL;;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	/* we ignore empty data - usually happens if the file is broken, e.g. by disk corruption
	 * or download interruption. */
	if (data == NULL)
		return NULL;

	/* create container for the components we find */
	cpts = g_ptr_array_new_with_free_func (g_object_unref);

	/* reset the global document properties */
	g_free (priv->origin);
	priv->origin = NULL;

	g_free (priv->media_baseurl);
	priv->media_baseurl = NULL;

	priv->default_priority = 0;

	/* initialize YAML parser */
	yaml_parser_initialize (&parser);
	yaml_parser_set_input_string (&parser, (unsigned char*) data, strlen (data));

	while (parse) {
		yaml_parser_parse (&parser, &event);
		if (event.type == YAML_DOCUMENT_START_EVENT) {
			GNode *n;
			gchar *key;
			gchar *value;
			AsComponent *cpt;
			GNode *root = g_node_new (g_strdup (""));

			dep11_yaml_process_layer (&parser, root);

			if (header) {
				for (n = root->children; n != NULL; n = n->next) {
					if ((n->data == NULL) || (n->children == NULL)) {
						parse = FALSE;
						g_set_error_literal (error,
								AS_METADATA_ERROR,
								AS_METADATA_ERROR_FAILED,
								"Invalid DEP-11 file found: Header invalid");
						break;
					}

					key = (gchar*) n->data;
					value = (gchar*) n->children->data;

					if (g_strcmp0 (key, "File") == 0) {
						if (g_strcmp0 (value, "DEP-11") != 0) {
							parse = FALSE;
							g_set_error_literal (error,
									AS_METADATA_ERROR,
									AS_METADATA_ERROR_FAILED,
									"Invalid DEP-11 file found: Header invalid");
						}
					} else if (g_strcmp0 (key, "Origin") == 0) {
						if ((value != NULL) && (priv->origin == NULL)) {
							priv->origin = g_strdup (value);
						} else {
							parse = FALSE;
							g_set_error_literal (error,
									AS_METADATA_ERROR,
									AS_METADATA_ERROR_FAILED,
									"Invalid DEP-11 file found: No origin set in header.");
						}
					} else if (g_strcmp0 (key, "Priority") == 0) {
						if (value != NULL) {
							priv->default_priority = g_ascii_strtoll (value, NULL, 10);
						}
					} else if (g_strcmp0 (key, "MediaBaseUrl") == 0) {
						if ((value != NULL) && (priv->media_baseurl == NULL)) {
							priv->media_baseurl = g_strdup (value);
						}
					}
				}
			} else {
				cpt = as_yamldata_process_component_node (ydt, root);
				if (cpt == NULL)
					parse = FALSE;

				if (as_component_is_valid (cpt)) {
					/* everything is fine with this component, we can emit it */
					g_ptr_array_add (cpts, cpt);
				} else {
					g_autofree gchar *str = NULL;
					str = as_component_to_string (cpt);
					g_debug ("WARNING: Invalid component found: %s", str);
					g_object_unref (cpt);
				}
			}

			header = FALSE;

			g_node_traverse (root,
					G_IN_ORDER,
					G_TRAVERSE_ALL,
					-1,
					as_yamldata_free_node,
					NULL);
			g_node_destroy (root);
		}

		/* stop if end of stream is reached */
		if (event.type == YAML_STREAM_END_EVENT)
			parse = FALSE;

		yaml_event_delete(&event);
	}

	yaml_parser_delete (&parser);

	return cpts;
}

/**
 * as_yamldata_set_locale:
 * @ydt: a #AsYAMLData instance.
 * @locale: the locale.
 *
 * Sets the locale which should be read when processing DEP-11 metadata.
 * All other locales are ignored, which increases parsing speed and
 * reduces memory usage.
 * If you set the locale to "ALL", all locales will be read.
 **/
void
as_yamldata_set_locale (AsYAMLData *ydt, const gchar *locale)
{
	gchar **strv;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	g_free (priv->locale);
	g_free (priv->locale_short);
	priv->locale = g_strdup (locale);

	strv = g_strsplit (priv->locale, "_", 0);
	priv->locale_short = g_strdup (strv[0]);
	g_strfreev (strv);
}

/**
 * as_yamldata_get_locale:
 * @ydt: a #AsYAMLData instance.
 *
 * Gets the current active locale for parsing DEP-11 metadata.,
 * or "ALL" if all locales are read.
 *
 * Returns: Locale used for metadata parsing.
 **/
const gchar *
as_yamldata_get_locale (AsYAMLData *ydt)
{
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);
	return priv->locale;
}

/**
 * as_yamldata_class_init:
 **/
static void
as_yamldata_class_init (AsYAMLDataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_yamldata_finalize;
}

/**
 * as_yamldata_new:
 */
AsYAMLData*
as_yamldata_new (void)
{
	AsYAMLData *ydt;
	ydt = g_object_new (AS_TYPE_YAMLDATA, NULL);
	return AS_YAMLDATA (ydt);
}
