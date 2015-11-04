/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#include "as-dep11.h"

#include <glib.h>
#include <glib-object.h>
#include <yaml.h>

#include "as-utils.h"
#include "as-utils-private.h"
#include "as-metadata.h"
#include "as-component-private.h"

typedef struct _AsDEP11Private	AsDEP11Private;
struct _AsDEP11Private
{
	gchar *locale;
	gchar *locale_short;
	gchar *origin_name;
	gchar *media_baseurl;
	gint default_priority;

	GPtrArray *cpts;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsDEP11, as_dep11, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_dep11_get_instance_private (o))

enum YamlNodeKind {
	YAML_VAR,
	YAML_VAL,
	YAML_SEQ
};

/**
 * as_dep11_finalize:
 **/
static void
as_dep11_finalize (GObject *object)
{
	AsDEP11 *dep11 = AS_DEP11 (object);
	AsDEP11Private *priv = GET_PRIVATE (dep11);

	g_free (priv->locale);
	g_free (priv->locale_short);
	g_ptr_array_unref (priv->cpts);
	if (priv->origin_name != NULL)
		g_free (priv->origin_name);
	if (priv->media_baseurl != NULL)
		g_free (priv->media_baseurl);

	G_OBJECT_CLASS (as_dep11_parent_class)->finalize (object);
}

/**
 * as_dep11_init:
 **/
static void
as_dep11_init (AsDEP11 *dep11)
{
	gchar *str;
	AsDEP11Private *priv = GET_PRIVATE (dep11);

	/* set active locale without UTF-8 suffix */
	str = as_get_locale ();
	as_dep11_set_locale (dep11, str);
	g_free (str);

	priv->origin_name = NULL;
	priv->media_baseurl = NULL;
	priv->default_priority = 0;
	priv->cpts = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * as_dep11_add_component:
 */
static void
as_dep11_add_component (AsDEP11 *dep11, AsComponent *cpt)
{
	AsDEP11Private *priv = GET_PRIVATE (dep11);
	g_ptr_array_add (priv->cpts, g_object_ref (cpt));
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
 * _dep11_yaml_free_node:
 */
static gboolean
_dep11_yaml_free_node (GNode *node, gpointer data)
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
 * as_dep11_get_localized_node:
 */
static GNode*
as_dep11_get_localized_node (AsDEP11 *dep11, GNode *node, gchar *locale_override)
{
	GNode *n;
	GNode *tnode = NULL;
	gchar *key;
	const gchar *locale;
	const gchar *locale_short = NULL;
	AsDEP11Private *priv = GET_PRIVATE (dep11);

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
 * as_dep11_get_localized_value:
 *
 * Get localized string from a translated DEP-11 key
 */
static gchar*
as_dep11_get_localized_value (AsDEP11 *dep11, GNode *node, gchar *locale_override)
{
	GNode *tnode;

	tnode = as_dep11_get_localized_node (dep11, node, locale_override);
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
 * as_dep11_process_keywords:
 *
 * Process a keywords node and add the data to an #AsComponent
 */
static void
as_dep11_process_keywords (AsDEP11 *dep11, GNode *node, AsComponent *cpt)
{
	GNode *tnode;
	GPtrArray *keywords;
	gchar **strv;

	keywords = g_ptr_array_new_with_free_func (g_free);

	tnode = as_dep11_get_localized_node (dep11, node, NULL);
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
 * dep11_process_icons:
 */
static void
dep11_process_icons (GNode *node, AsComponent *cpt)
{
	GNode *n;
	gchar *key;
	gchar *value;
	const gchar *icon_url;

	for (n = node->children; n != NULL; n = n->next) {
		key = (gchar*) n->data;
		value = (gchar*) n->children->data;

		if (g_strcmp0 (key, "stock") == 0) {
			as_component_add_icon (cpt, AS_ICON_KIND_STOCK, 0, 0, value);
		} else if (g_strcmp0 (key, "cached") == 0) {
			as_component_add_icon (cpt, AS_ICON_KIND_CACHED, 0, 0, value);
			icon_url = as_component_get_icon_url (cpt, 0, 0);
			if ((icon_url == NULL) || (g_str_has_prefix (icon_url, "http://"))) {
				as_component_add_icon_url (cpt, 0, 0, value);
			}
		} else if (g_strcmp0 (key, "local") == 0) {
			as_component_add_icon (cpt, AS_ICON_KIND_LOCAL, 0, 0, value);
			as_component_add_icon_url (cpt, 0, 0, value);
		} else if (g_strcmp0 (key, "remote") == 0) {
			as_component_add_icon (cpt, AS_ICON_KIND_REMOTE, 0, 0, value);
			icon_url = as_component_get_icon_url (cpt, 0, 0);
			if (icon_url == NULL)
				as_component_add_icon_url (cpt, 0, 0, value);
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
	GPtrArray *provided_items;

	provided_items = as_component_get_provided_items (cpt);
	for (n = node->children; n != NULL; n = n->next) {
		key = (gchar*) n->data;

		if (g_strcmp0 (key, "libraries") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				g_ptr_array_add (provided_items,
						 as_provides_item_create (AS_PROVIDES_KIND_LIBRARY, (gchar*) sn->data, ""));
			}
		} else if (g_strcmp0 (key, "binaries") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				g_ptr_array_add (provided_items,
						 as_provides_item_create (AS_PROVIDES_KIND_BINARY, (gchar*) sn->data, ""));
			}
		} else if (g_strcmp0 (key, "fonts") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				g_ptr_array_add (provided_items,
						 as_provides_item_create (AS_PROVIDES_KIND_FONT, (gchar*) sn->data, ""));
			}
		} else if (g_strcmp0 (key, "modaliases") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				g_ptr_array_add (provided_items,
						 as_provides_item_create (AS_PROVIDES_KIND_MODALIAS, (gchar*) sn->data, ""));
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
						dvalue = NULL;
					if (g_strcmp0 (dkey, "type") == 0) {
						kind = dvalue;
					} else if ((g_strcmp0 (dkey, "guid") == 0) || (g_strcmp0 (dkey, "fname") == 0)) {
						fwdata = dvalue;
					}
				}
				/* we don't add malformed provides types */
				if ((kind != NULL) && (fwdata != NULL))
					g_ptr_array_add (provided_items,
							 as_provides_item_create (AS_PROVIDES_KIND_FIRMWARE, fwdata, kind));
			}
		} else if (g_strcmp0 (key, "python2") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				g_ptr_array_add (provided_items,
						 as_provides_item_create (AS_PROVIDES_KIND_PYTHON2, (gchar*) sn->data, ""));
			}
		} else if (g_strcmp0 (key, "python3") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				g_ptr_array_add (provided_items,
						 as_provides_item_create (AS_PROVIDES_KIND_PYTHON3, (gchar*) sn->data, ""));
			}
		} else if (g_strcmp0 (key, "mimetypes") == 0) {
			for (sn = n->children; sn != NULL; sn = sn->next) {
				g_ptr_array_add (provided_items,
						 as_provides_item_create (AS_PROVIDES_KIND_MIMETYPE, (gchar*) sn->data, ""));
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
				if ((kind != NULL) && (service != NULL))
					g_ptr_array_add (provided_items,
							as_provides_item_create (AS_PROVIDES_KIND_DBUS, service, kind));
			}
		}
	}
}

/**
 * dep11_process_image:
 */
static void
dep11_process_image (AsDEP11 *dep11, GNode *node, AsScreenshot *scr)
{
	GNode *n;
	AsImage *img;
	AsDEP11Private *priv = GET_PRIVATE (dep11);

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
 * as_dep11_process_screenshots:
 */
static void
as_dep11_process_screenshots (AsDEP11 *dep11, GNode *node, AsComponent *cpt)
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
					as_screenshot_set_kind (scr, AS_SCREENSHOT_KIND_NORMAL);
			} else if (g_strcmp0 (key, "caption") == 0) {
				gchar *lvalue;
				/* the caption is a localized element */
				lvalue = as_dep11_get_localized_value (dep11, n, NULL);
				as_screenshot_set_caption (scr, lvalue, NULL);
			} else if (g_strcmp0 (key, "source-image") == 0) {
				/* there can only be one source image */
				dep11_process_image (dep11, n, scr);
			} else if (g_strcmp0 (key, "thumbnails") == 0) {
				/* the thumbnails are a list of images */
				for (in = n->children; in != NULL; in = in->next) {
					dep11_process_image (dep11, in, scr);
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
 * as_dep11_process_releases:
 *
 * Add #AsRelease instances to the #AsComponent
 */
static void
as_dep11_process_releases (AsDEP11 *dep11, GNode *node, AsComponent *cpt)
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
			} else if (g_strcmp0 (key, "version") == 0) {
				as_release_set_version (rel, value);
			} else if (g_strcmp0 (key, "description") == 0) {
				gchar *lvalue;
				lvalue = as_dep11_get_localized_value (dep11, node, NULL);
				as_release_set_description (rel, lvalue, NULL);
				g_free (lvalue);
			} else {
				dep11_print_unknown ("screenshot", key);
			}
		}

		/* add the result */
		as_component_add_release (cpt, rel);
		g_object_unref (rel);
	}
}

/**
 * as_dep11_process_component_node:
 */
AsComponent*
as_dep11_process_component_node (AsDEP11 *dep11, GNode *root)
{
	GNode *node;
	AsComponent *cpt;

	gchar **strv;
	GPtrArray *categories;
	GPtrArray *compulsory_for_desktops;
	AsDEP11Private *priv = GET_PRIVATE (dep11);

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
			lvalue = as_dep11_get_localized_value (dep11, node, "C");
			if (lvalue != NULL) {
				as_component_set_name (cpt, lvalue, "C"); /* Unlocalized */
				g_free (lvalue);
			}
			lvalue = as_dep11_get_localized_value (dep11, node, NULL);
			as_component_set_name (cpt, lvalue, NULL);
			g_free (lvalue);
		} else if (g_strcmp0 (key, "Summary") == 0) {
			lvalue = as_dep11_get_localized_value (dep11, node, NULL);
			as_component_set_summary (cpt, lvalue, NULL);
			g_free (lvalue);
		} else if (g_strcmp0 (key, "Description") == 0) {
			lvalue = as_dep11_get_localized_value (dep11, node, NULL);
			as_component_set_description (cpt, lvalue, NULL);
			g_free (lvalue);
		} else if (g_strcmp0 (key, "DeveloperName") == 0) {
			lvalue = as_dep11_get_localized_value (dep11, node, NULL);
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
			as_dep11_process_keywords (dep11, node, cpt);
		} else if (g_strcmp0 (key, "Url") == 0) {
			dep11_process_urls (node, cpt);
		} else if (g_strcmp0 (key, "Icon") == 0) {
			dep11_process_icons (node, cpt);
		} else if (g_strcmp0 (key, "Provides") == 0) {
			dep11_process_provides (node, cpt);
		} else if (g_strcmp0 (key, "Screenshots") == 0) {
			as_dep11_process_screenshots (dep11, node, cpt);
		} else if (g_strcmp0 (key, "Releases") == 0) {
			as_dep11_process_releases (dep11, node, cpt);
		} else {
			dep11_print_unknown ("root", key);
		}
	}

	/* set component origin */
	as_component_set_origin (cpt, priv->origin_name);

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
 * as_dep11_process_data:
 */
void
as_dep11_parse_data (AsDEP11 *dep11, const gchar *data, GError **error)
{
	yaml_parser_t parser;
	yaml_event_t event;
	gboolean header = TRUE;
	gboolean parse = TRUE;
	AsDEP11Private *priv = GET_PRIVATE (dep11);

	yaml_parser_initialize (&parser);
	yaml_parser_set_input_string (&parser, (unsigned char*) data, strlen(data));

	while (parse) {
		yaml_parser_parse(&parser, &event);
		if (event.type == YAML_DOCUMENT_START_EVENT) {
			GNode *n;
			gchar *key;
			gchar *value;
			AsComponent *cpt;
			GNode *root = g_node_new (g_strdup (""));

			dep11_yaml_process_layer (&parser, root);

			if (header) {
				for (n = root->children; n != NULL; n = n->next) {
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
						if ((value != NULL) && (priv->origin_name == NULL)) {
							priv->origin_name = g_strdup (value);
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
				cpt = as_dep11_process_component_node (dep11, root);
				if (cpt == NULL)
					parse = FALSE;

				if (as_component_is_valid (cpt)) {
					/* everything is fine with this component, we can emit it */
					as_dep11_add_component (dep11, cpt);
				} else {
					gchar *str;
					gchar *str2;
					str = as_component_to_string (cpt);
					str2 = g_strdup_printf ("Invalid component found: %s\n", str);
					g_debug ("DEP-11-Warning: %s", str2);
					g_free (str);
					g_free (str2);
				}
				g_object_unref (cpt);
			}

			header = FALSE;

			g_node_traverse (root,
					G_IN_ORDER,
					G_TRAVERSE_ALL,
					-1,
					_dep11_yaml_free_node,
					NULL);
			g_node_destroy (root);
		}

		/* stop if end of stream is reached */
		if (event.type == YAML_STREAM_END_EVENT)
			parse = FALSE;

		yaml_event_delete(&event);
	}

	yaml_parser_delete (&parser);
	if (priv->origin_name != NULL) {
		g_free (priv->origin_name);
		priv->origin_name = NULL;
	}

	if (priv->media_baseurl != NULL) {
		g_free (priv->media_baseurl);
		priv->media_baseurl = NULL;
	}

	priv->default_priority = 0;
}

/**
 * as_dep11_parse_file:
 * @dep11: A valid #AsDEP11 instance
 * @file: #GFile for the upstream metadata
 * @error: A #GError or %NULL.
 *
 * Parses an AppStream upstream metadata file in DEP-11-YAML format.
 *
 **/
void
as_dep11_parse_file (AsDEP11 *dep11, GFile* file, GError **error)
{
	gchar *yaml_doc;
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

		yaml_doc = g_strdup ((const gchar*) data);

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

		yaml_doc = g_string_free (str, FALSE);
		g_object_unref (dis);
	}

	/* parse YAML data */
	as_dep11_parse_data (dep11, yaml_doc, error);
	g_free (yaml_doc);
}

/**
 * as_dep11_set_locale:
 * @dep11: a #AsDEP11 instance.
 * @locale: the locale.
 *
 * Sets the locale which should be read when processing DEP-11 metadata.
 * All other locales are ignored, which increases parsing speed and
 * reduces memory usage.
 * If you set the locale to "ALL", all locales will be read.
 **/
void
as_dep11_set_locale (AsDEP11 *dep11, const gchar *locale)
{
	gchar **strv;
	AsDEP11Private *priv = GET_PRIVATE (dep11);

	g_free (priv->locale);
	g_free (priv->locale_short);
	priv->locale = g_strdup (locale);

	strv = g_strsplit (priv->locale, "_", 0);
	priv->locale_short = g_strdup (strv[0]);
	g_strfreev (strv);
}

/**
 * as_dep11_get_locale:
 * @dep11: a #AsDEP11 instance.
 *
 * Gets the current active locale for parsing DEP-11 metadata.,
 * or "ALL" if all locales are read.
 *
 * Returns: Locale used for metadata parsing.
 **/
const gchar *
as_dep11_get_locale (AsDEP11 *dep11)
{
	AsDEP11Private *priv = GET_PRIVATE (dep11);
	return priv->locale;
}

/**
 * as_dep11_get_components:
 * @dep11: an #AsDEP11 instance.
 *
 * Returns: (transfer none) (element-type AsComponent): A #GPtrArray of all parsed components
 **/
GPtrArray*
as_dep11_get_components (AsDEP11 *dep11)
{
	AsDEP11Private *priv = GET_PRIVATE (dep11);
	return priv->cpts;
}

/**
 * as_dep11_clear_components:
 **/
void
as_dep11_clear_components (AsDEP11 *dep11)
{
	AsDEP11Private *priv = GET_PRIVATE (dep11);
	g_ptr_array_unref (priv->cpts);
	priv->cpts = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * as_dep11_class_init:
 **/
static void
as_dep11_class_init (AsDEP11Class *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_dep11_finalize;
}

/**
 * as_dep11_new:
 */
AsDEP11*
as_dep11_new (void)
{
	AsDEP11 *dep11;
	dep11 = g_object_new (AS_TYPE_DEP11, NULL);
	return AS_DEP11 (dep11);
}
