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
#include "as-release-private.h"

typedef struct
{
	gchar *locale;
	gchar *origin;
	gchar *media_baseurl;

	gchar *arch;
	gint default_priority;
	AsParserMode mode;
	gboolean check_valid;
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
	priv->check_valid = TRUE;

	/* the YAML data is only for collection-metadata at time */
	priv->mode = AS_PARSER_MODE_COLLECTION;
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
	g_free (priv->origin);
	g_free (priv->media_baseurl);
	g_free (priv->arch);

	G_OBJECT_CLASS (as_yamldata_parent_class)->finalize (object);
}

/**
 * as_yamldata_initialize:
 * @ydt: An instance of #AsYAMLData
 *
 * Initialize the YAML handler.
 */
void
as_yamldata_initialize (AsYAMLData *ydt, const gchar *locale, const gchar *origin, const gchar *media_baseurl, const gchar *arch, gint priority)
{
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	g_free (priv->locale);
	priv->locale = g_strdup (locale);

	g_free (priv->origin);
	priv->origin = g_strdup (origin);

	g_free (priv->media_baseurl);
	priv->media_baseurl = g_strdup (media_baseurl);

	g_free (priv->arch);
	priv->origin = g_strdup (arch);

	priv->default_priority = priority;
}

/**
 * as_yaml_print_unknown:
 */
static void
as_yaml_print_unknown (const gchar *root, const gchar *key)
{
	g_debug ("YAML: Unknown field '%s/%s' found.", root, key);
}

/**
 * as_yaml_free_node:
 */
static gboolean
as_yaml_free_node (GNode *node, gpointer data)
{
	if (node->data != NULL)
		g_free (node->data);

	return FALSE;
}

/**
 * as_yaml_node_get_key:
 *
 * Helper method to get the key of a node.
 */
static const gchar*
as_yaml_node_get_key (GNode *n)
{
	return (const gchar*) n->data;
}

/**
 * as_yaml_node_get_value:
 *
 * Helper method to get the value of a node.
 */
static const gchar*
as_yaml_node_get_value (GNode *n)
{
	if (n->children)
		return (const gchar*) n->children->data;
	else
		return NULL;
}

/**
 * as_yaml_process_layer:
 *
 * Create GNode tree from DEP-11 YAML document
 */
static void
as_yaml_process_layer (yaml_parser_t *parser, GNode *data, GError **error)
{
	GNode *last_leaf = data;
	GNode *last_scalar;
	yaml_event_t event;
	gboolean parse = TRUE;
	gboolean in_sequence = FALSE;
	GError *tmp_error = NULL;
	int storage = YAML_VAR; /* the first element must always be of type VAR */

	while (parse) {
		if (!yaml_parser_parse (parser, &event)) {
			g_set_error (error,
					AS_METADATA_ERROR,
					AS_METADATA_ERROR_PARSE,
					"Invalid DEP-11 file found. Could not parse YAML: %s", parser->problem);
			break;
		}

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

				as_yaml_process_layer (parser, last_leaf, &tmp_error);
				if (tmp_error != NULL) {
					g_propagate_error (error, tmp_error);
					parse = FALSE;
				}

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
	const gchar *key;
	const gchar *locale;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	if (locale_override == NULL)
		locale = priv->locale;
	else
		locale = locale_override;

	for (n = node->children; n != NULL; n = n->next) {
		key = as_yaml_node_get_key (n);

		if ((tnode == NULL) && (g_strcmp0 (key, "C") == 0)) {
			tnode = n;
			if (g_strcmp0 (locale, "C") == 0)
				goto out;
		}

		if (g_strcmp0 (locale, key) == 0) {
			tnode = n;
			goto out;
		}

		if (as_utils_locale_is_compatible (locale, key))
			tnode = n;
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
 * as_yaml_list_to_str_array:
 */
static void
as_yaml_list_to_str_array (GNode *node, GPtrArray *array)
{
	GNode *n;

	for (n = node->children; n != NULL; n = n->next) {
		const gchar *val = as_yaml_node_get_key (n);
		if (val != NULL)
			g_ptr_array_add (array, g_strdup (val));
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

	as_yaml_list_to_str_array (tnode, keywords);

	strv = as_ptr_array_to_strv (keywords);
	as_component_set_keywords (cpt, strv, NULL);
	g_ptr_array_unref (keywords);
	g_strfreev (strv);
}

/**
 * as_yaml_process_bundles:
 *
 * Process a bundles node and add the data to an #AsComponent
 */
static void
as_yaml_process_bundles (GNode *node, AsComponent *cpt)
{
	GNode *sn;

	for (sn = node->children; sn != NULL; sn = sn->next) {
		GNode *n;
		g_autoptr(AsBundle) bundle = as_bundle_new ();

		for (n = sn->children; n != NULL; n = n->next) {
			const gchar *key = as_yaml_node_get_key (n);
			const gchar *value = as_yaml_node_get_value (n);

			if (g_strcmp0 (key, "type") == 0) {
				as_bundle_set_kind (bundle, as_bundle_kind_from_string (value));
			} else if (g_strcmp0 (key, "id") == 0) {
				as_bundle_set_id (bundle, value);
			} else {
				as_yaml_print_unknown ("bundle", key);
			}
		}

		/* add the result */
		as_component_add_bundle (cpt, bundle);
	}
}

/**
 * as_yaml_process_urls:
 */
static void
as_yaml_process_urls (GNode *node, AsComponent *cpt)
{
	GNode *n;
	AsUrlKind url_kind;

	for (n = node->children; n != NULL; n = n->next) {
		const gchar *key;
		const gchar *value;

		key = as_yaml_node_get_key (n);
		value = as_yaml_node_get_value (n);

		url_kind = as_url_kind_from_string (key);
		if ((url_kind != AS_URL_KIND_UNKNOWN) && (value != NULL))
			as_component_add_url (cpt, url_kind, value);
	}
}

/**
 * as_yamldata_process_icon:
 */
static void
as_yamldata_process_icon (AsYAMLData *ydt, GNode *node, AsComponent *cpt, AsIconKind kind)
{
	GNode *n;
	guint64 size;
	g_autoptr(AsIcon) icon = NULL;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	icon = as_icon_new ();
	as_icon_set_kind (icon, kind);

	for (n = node->children; n != NULL; n = n->next) {
		const gchar *key;
		const gchar *value;

		key = as_yaml_node_get_key (n);
		value = as_yaml_node_get_value (n);

		if (g_strcmp0 (key, "width") == 0) {
			size = g_ascii_strtoll (value, NULL, 10);
			as_icon_set_width (icon, size);
		} else if (g_strcmp0 (key, "height") == 0) {
			size = g_ascii_strtoll (value, NULL, 10);
			as_icon_set_height (icon, size);
		} else {
			if (kind == AS_ICON_KIND_REMOTE) {
				if (g_strcmp0 (key, "url") == 0) {
					if (priv->media_baseurl == NULL) {
						/* no baseurl, we can just set the value as URL */
						as_icon_set_url (icon, value);
					} else {
						/* handle the media baseurl */
						g_autofree gchar *url = NULL;
						url = g_build_filename (priv->media_baseurl, value, NULL);
						as_icon_set_url (icon, url);
					}
				}
			} else {
				if (g_strcmp0 (key, "name") == 0) {
					as_icon_set_filename (icon, value);
				}
			}
		}
	}

	as_component_add_icon (cpt, icon);
}

/**
 * as_yamldata_process_icons:
 */
static void
as_yamldata_process_icons (AsYAMLData *ydt, GNode *node, AsComponent *cpt)
{
	GNode *n;
	const gchar *key;
	const gchar *value;

	for (n = node->children; n != NULL; n = n->next) {
		key = (const gchar*) n->data;
		value = (const gchar*) n->children->data;

		if (g_strcmp0 (key, "stock") == 0) {
			g_autoptr(AsIcon) icon = as_icon_new ();
			as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
			as_icon_set_name (icon, value);
			as_component_add_icon (cpt, icon);
		} else if (g_strcmp0 (key, "cached") == 0) {
			if (n->children->next == NULL) {
				g_autoptr(AsIcon) icon = as_icon_new ();
				/* we have a legacy YAML file */
				as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
				as_icon_set_filename (icon, value);
				as_component_add_icon (cpt, icon);
			} else {
				GNode *sn;
				/* we have a recent YAML file */
				for (sn = n->children; sn != NULL; sn = sn->next)
					as_yamldata_process_icon (ydt, sn, cpt, AS_ICON_KIND_CACHED);
			}
		} else if (g_strcmp0 (key, "local") == 0) {
			GNode *sn;
			for (sn = n->children; sn != NULL; sn = sn->next)
				as_yamldata_process_icon (ydt, sn, cpt, AS_ICON_KIND_LOCAL);
		} else if (g_strcmp0 (key, "remote") == 0) {
			GNode *sn;
			for (sn = n->children; sn != NULL; sn = sn->next)
				as_yamldata_process_icon (ydt, sn, cpt, AS_ICON_KIND_REMOTE);
		}
	}
}

/**
 * as_yaml_process_provides:
 */
static void
as_yaml_process_provides (GNode *node, AsComponent *cpt)
{
	GNode *n;
	GNode *sn;

	for (n = node->children; n != NULL; n = n->next) {
		const gchar *key = as_yaml_node_get_key (n);

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
 * as_yamldata_process_image:
 */
static void
as_yamldata_process_image (AsYAMLData *ydt, GNode *node, AsScreenshot *scr)
{
	GNode *n;
	AsImage *img;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	img = as_image_new ();

	for (n = node->children; n != NULL; n = n->next) {
		const gchar *key;
		const gchar *value;
		guint64 size;

		key = as_yaml_node_get_key (n);
		value = as_yaml_node_get_value (n);
		if (value == NULL)
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
		} else if (g_strcmp0 (key, "lang") == 0) {
			as_image_set_locale (img, value);
		} else {
			as_yaml_print_unknown ("image", key);
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
			const gchar *key;
			const gchar *value;

			key = as_yaml_node_get_key (n);
			value = as_yaml_node_get_value (n);

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
				as_yamldata_process_image (ydt, n, scr);
			} else if (g_strcmp0 (key, "thumbnails") == 0) {
				/* the thumbnails are a list of images */
				for (in = n->children; in != NULL; in = in->next) {
					as_yamldata_process_image (ydt, in, scr);
				}
			} else {
				as_yaml_print_unknown ("screenshot", key);
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
			const gchar *key;
			const gchar *value;

			key = as_yaml_node_get_key (n);
			value = as_yaml_node_get_value (n);

			if (g_strcmp0 (key, "unix-timestamp") == 0) {
				as_release_set_timestamp (rel, atol (value));
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
				as_yaml_print_unknown ("release", key);
			}
		}

		/* add the result */
		as_component_add_release (cpt, rel);
		g_object_unref (rel);
	}
}

/**
 * as_yamldata_process_languages:
 */
static void
as_yamldata_process_languages (GNode *node, AsComponent *cpt)
{
	GNode *sn;

	for (sn = node->children; sn != NULL; sn = sn->next) {
		GNode *n;
		g_autofree gchar *locale = NULL;
		g_autofree gchar *percentage_str = NULL;

		for (n = sn->children; n != NULL; n = n->next) {
			const gchar *key;
			const gchar *value;

			key = as_yaml_node_get_key (n);
			value = as_yaml_node_get_value (n);

			if (g_strcmp0 (key, "locale") == 0) {
				if (locale == NULL)
					locale = g_strdup (value);
			} else if (g_strcmp0 (key, "percentage") == 0) {
				if (percentage_str == NULL)
					percentage_str = g_strdup (value);
			} else {
				as_yaml_print_unknown ("Languages", key);
			}
		}

		if ((locale != NULL) && (percentage_str != NULL))
			as_component_add_language (cpt,
						   locale,
						   g_ascii_strtoll (percentage_str, NULL, 10));
	}
}

/**
 * as_yamldata_process_suggests:
 */
static void
as_yamldata_process_suggests (GNode *node, AsComponent *cpt)
{
	GNode *sn;

	for (sn = node->children; sn != NULL; sn = sn->next) {
		GNode *n;
		g_autoptr(AsSuggested) sug = NULL;

		sug = as_suggested_new ();
		for (n = sn->children; n != NULL; n = n->next) {
			const gchar *key;
			const gchar *value;

			key = as_yaml_node_get_key (n);
			value = as_yaml_node_get_value (n);

			if (g_strcmp0 (key, "type") == 0) {
				as_suggested_set_kind (sug,
						       as_suggested_kind_from_string (value));
			} else if (g_strcmp0 (key, "ids") == 0) {
				as_yaml_list_to_str_array (n,
							      as_suggested_get_ids (sug));
			} else {
				as_yaml_print_unknown ("Suggests", key);
			}
		}

		if (as_suggested_is_valid (sug))
			as_component_add_suggested (cpt, sug);
	}
}

/**
 * as_yamldata_process_component_node:
 */
static AsComponent*
as_yamldata_process_component_node (AsYAMLData *ydt, GNode *root)
{
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);
	GNode *node;
	AsComponent *cpt;

	cpt = as_component_new ();

	/* set active locale for this component */
	as_component_set_active_locale (cpt, priv->locale);

	/* set component default priority */
	as_component_set_priority (cpt, priv->default_priority);

	for (node = root->children; node != NULL; node = node->next) {
		const gchar *key;
		const gchar *value;
		gchar *lvalue;

		if (node->children == NULL)
			continue;

		key = (const gchar*) node->data;
		value = as_yaml_node_get_value (node);

		if (g_strcmp0 (key, "Type") == 0) {
			if (g_strcmp0 (value, "generic") == 0)
				as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
			else
				as_component_set_kind (cpt, as_component_kind_from_string (value));
		} else if (g_strcmp0 (key, "ID") == 0) {
			as_component_set_id (cpt, value);
		} else if (g_strcmp0 (key, "Priority") == 0) {
			as_component_set_priority (cpt, g_ascii_strtoll (value, NULL, 10));
		} else if (g_strcmp0 (key, "Merge") == 0) {
			as_component_set_merge_kind (cpt, as_merge_kind_from_string (value));
		} else if (g_strcmp0 (key, "Package") == 0) {
			g_auto(GStrv) strv = NULL;
			strv = g_new0 (gchar*, 1 + 1);
			strv[0] = g_strdup (value);
			strv[1] = NULL;

			as_component_set_pkgnames (cpt, strv);
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
			as_yaml_list_to_str_array (node,
						   as_component_get_categories (cpt));
		} else if (g_strcmp0 (key, "CompulsoryForDesktops") == 0) {
			as_yaml_list_to_str_array (node,
						   as_component_get_compulsory_for_desktops (cpt));
		} else if (g_strcmp0 (key, "Extends") == 0) {
			as_yaml_list_to_str_array (node,
						   as_component_get_extends (cpt));
		} else if (g_strcmp0 (key, "Keywords") == 0) {
			as_yamldata_process_keywords (ydt, node, cpt);
		} else if (g_strcmp0 (key, "Url") == 0) {
			as_yaml_process_urls (node, cpt);
		} else if (g_strcmp0 (key, "Icon") == 0) {
			as_yamldata_process_icons (ydt, node, cpt);
		} else if (g_strcmp0 (key, "Bundles") == 0) {
			as_yaml_process_bundles (node, cpt);
		} else if (g_strcmp0 (key, "Provides") == 0) {
			as_yaml_process_provides (node, cpt);
		} else if (g_strcmp0 (key, "Screenshots") == 0) {
			as_yamldata_process_screenshots (ydt, node, cpt);
		} else if (g_strcmp0 (key, "Languages") == 0) {
			as_yamldata_process_languages (node, cpt);
		} else if (g_strcmp0 (key, "Releases") == 0) {
			as_yamldata_process_releases (ydt, node, cpt);
		} else if (g_strcmp0 (key, "Suggests") == 0) {
			as_yamldata_process_suggests (node, cpt);
		} else {
			as_yaml_print_unknown ("root", key);
		}
	}

	/* set component origin */
	as_component_set_origin (cpt, priv->origin);

	/* set component architecture */
	as_component_set_architecture (cpt, priv->arch);

	return cpt;
}

/**
 * as_yaml_emit_scalar:
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
 * as_yaml_emit_scalar_key:
 */
static void
as_yaml_emit_scalar_key (yaml_emitter_t *emitter, const gchar *key)
{
	yaml_scalar_style_t keystyle;
	yaml_event_t event;
	gint ret;

	/* Some locale are "no", which - if unquoted - are interpreted as booleans.
	 * Since we hever have boolean keys, we can disallow creating bool keys for all keys. */
	keystyle = YAML_ANY_SCALAR_STYLE;
	if (g_strcmp0 (key, "no") == 0)
		keystyle = YAML_SINGLE_QUOTED_SCALAR_STYLE;
	if (g_strcmp0 (key, "yes") == 0)
		keystyle = YAML_SINGLE_QUOTED_SCALAR_STYLE;

	yaml_scalar_event_initialize (&event, NULL, NULL, (yaml_char_t*) key, strlen (key), TRUE, TRUE, keystyle);
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

	as_yaml_emit_scalar_key (emitter, key);

	yaml_scalar_event_initialize (&event, NULL, NULL, (yaml_char_t*) value, strlen (value), TRUE, TRUE, YAML_ANY_SCALAR_STYLE);
	ret = yaml_emitter_emit (emitter, &event);
	g_assert (ret);
}

/**
 * as_yaml_emit_long_entry:
 */
static void
as_yaml_emit_long_entry (yaml_emitter_t *emitter, const gchar *key, const gchar *value)
{
	yaml_event_t event;
	gint ret;
	g_autofree gchar *value_clean = NULL;

	if (value == NULL)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	yaml_scalar_event_initialize (&event,
					NULL,
					NULL,
					(yaml_char_t*) value,
					strlen (value),
					TRUE,
					TRUE,
					YAML_FOLDED_SCALAR_STYLE);
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

	/* skip cruft */
	if (as_is_cruft_locale (key))
		return;

	g_strstrip (value);
	as_yaml_emit_entry (emitter, key, value);
}

/**
 * as_yaml_emit_lang_hashtable_entries_long:
 */
static void
as_yaml_emit_lang_hashtable_entries_long (gchar *key, gchar *value, yaml_emitter_t *emitter)
{
	if (as_str_empty (value))
		return;

	/* skip cruft */
	if (as_is_cruft_locale (key))
		return;

	g_strstrip (value);
	as_yaml_emit_long_entry (emitter, key, value);
}

/**
 * as_yaml_emit_localized_entry_with_func:
 */
static void
as_yaml_emit_localized_entry_with_func (yaml_emitter_t *emitter, const gchar *key, GHashTable *ltab, GHFunc tfunc)
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
				tfunc,
				emitter);
	/* finalize */
	as_yaml_mapping_end (emitter);
}

/**
 * as_yaml_emit_localized_entry:
 */
static void
as_yaml_emit_localized_entry (yaml_emitter_t *emitter, const gchar *key, GHashTable *ltab)
{
	as_yaml_emit_localized_entry_with_func (emitter,
						key,
						ltab,
						(GHFunc) as_yaml_emit_lang_hashtable_entries);
}

/**
 * as_yaml_emit_long_localized_entry:
 */
static void
as_yaml_emit_long_localized_entry (yaml_emitter_t *emitter, const gchar *key, GHashTable *ltab)
{
	as_yaml_emit_localized_entry_with_func (emitter,
						key,
						ltab,
						(GHFunc) as_yaml_emit_lang_hashtable_entries_long);
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
 * as_yaml_emit_sequence_from_str_array:
 */
static void
as_yaml_emit_sequence_from_str_array (yaml_emitter_t *emitter, const gchar *key, GPtrArray *array)
{
	guint i;

	if (array == NULL)
		return;
	if (array->len == 0)
		return;

	as_yaml_emit_scalar_key (emitter, key);
	as_yaml_sequence_start (emitter);

	for (i = 0; i < array->len; i++) {
		const gchar *val = (const gchar*) g_ptr_array_index (array, i);
		as_yaml_emit_scalar (emitter, val);
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

	/* skip cruft */
	if (as_is_cruft_locale (key))
		return;

	as_yaml_emit_scalar (emitter, key);
	as_yaml_sequence_start (emitter);
	for (i = 0; strv[i] != NULL; i++) {
		as_yaml_emit_scalar (emitter, strv[i]);
	}
	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_emit_localized_strv:
 */
static void
as_yaml_emit_localized_strv (yaml_emitter_t *emitter, const gchar *key, GHashTable *ltab)
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
static void
as_yaml_emit_provides (yaml_emitter_t *emitter, AsComponent *cpt)
{
	GPtrArray *plist;
	guint i;

	g_autoptr(GPtrArray) dbus_system = NULL;
	g_autoptr(GPtrArray) dbus_user = NULL;

	g_autoptr(GPtrArray) fw_runtime = NULL;
	g_autoptr(GPtrArray) fw_flashed = NULL;

	plist = as_component_get_provided (cpt);
	if (plist->len == 0)
		return;

	as_yaml_emit_scalar (emitter, "Provides");
	as_yaml_mapping_start (emitter);
	for (i = 0; i < plist->len; i++) {
		AsProvidedKind kind;
		GPtrArray *items;
		guint j;
		AsProvided *prov = AS_PROVIDED (g_ptr_array_index (plist, i));

		items = as_provided_get_items (prov);
		if (items->len == 0)
			continue;

		kind = as_provided_get_kind (prov);
		switch (kind) {
			case AS_PROVIDED_KIND_LIBRARY:
				as_yaml_emit_sequence (emitter,
							"libraries",
							items);
				break;
			case AS_PROVIDED_KIND_BINARY:
				as_yaml_emit_sequence (emitter,
							"binaries",
							items);
				break;
			case AS_PROVIDED_KIND_MIMETYPE:
				as_yaml_emit_sequence (emitter,
							"mimetypes",
							items);
				break;
			case AS_PROVIDED_KIND_PYTHON_2:
				as_yaml_emit_sequence (emitter,
							"python2",
							items);
				break;
			case AS_PROVIDED_KIND_PYTHON:
				as_yaml_emit_sequence (emitter,
							"python3",
							items);
				break;
			case AS_PROVIDED_KIND_MODALIAS:
				as_yaml_emit_sequence (emitter,
							"modaliases",
							items);
				break;
			case AS_PROVIDED_KIND_FONT:
				as_yaml_emit_scalar (emitter, "fonts");

				as_yaml_sequence_start (emitter);
				for (j = 0; j < items->len; j++) {
					as_yaml_mapping_start (emitter);
					as_yaml_emit_entry (emitter,
							    "name",
							    (const gchar*) g_ptr_array_index (items, j));
					/* FIXME: Also emit "file" entry, but at time we don't seem to store this? */
					as_yaml_mapping_end (emitter);
				}
				as_yaml_sequence_end (emitter);
				break;
			case AS_PROVIDED_KIND_DBUS_SYSTEM:
				if (dbus_system == NULL) {
					dbus_system = g_object_ref (items);
				} else {
					g_critical ("Hit dbus:system twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_DBUS_USER:
				if (dbus_user == NULL) {
					dbus_user = g_object_ref (items);
				} else {
					g_critical ("Hit dbus:user twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_FIRMWARE_RUNTIME:
				if (fw_runtime == NULL) {
					fw_runtime = g_object_ref (items);
				} else {
					g_critical ("Hit firmware:runtime twice, this should never happen!");
				}
				break;
			case AS_PROVIDED_KIND_FIRMWARE_FLASHED:
				if (fw_flashed == NULL) {
					fw_flashed = g_object_ref (items);
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
			for (i = 0; i < dbus_system->len; i++) {
				const gchar *value = g_ptr_array_index (dbus_system, i);
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "system");
				as_yaml_emit_entry (emitter, "service", value);

				as_yaml_mapping_end (emitter);
			}
		}

		if (dbus_user != NULL) {
			for (i = 0; dbus_user->len; i++) {
				const gchar *value = g_ptr_array_index (dbus_user, i);
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "user");
				as_yaml_emit_entry (emitter, "service", value);

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
			for (i = 0; i < fw_runtime->len; i++) {
				const gchar *value = g_ptr_array_index (fw_runtime, i);
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "runtime");
				as_yaml_emit_entry (emitter, "guid", value);

				as_yaml_mapping_end (emitter);
			}
		}

		if (fw_flashed != NULL) {
			for (i = 0; i < fw_flashed->len; i++) {
				const gchar *value = g_ptr_array_index (fw_flashed, i);
				as_yaml_mapping_start (emitter);

				as_yaml_emit_entry (emitter, "type", "flashed");
				as_yaml_emit_entry (emitter, "fname", value);

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
 * Helper for as_yaml_data_emit_screenshots
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
	as_yaml_emit_entry (emitter, "lang", as_image_get_locale (img));
	as_yaml_mapping_end (emitter);
}

/**
 * as_yaml_data_emit_screenshots:
 */
static void
as_yaml_data_emit_screenshots (AsYAMLData *ydt, yaml_emitter_t *emitter, AsComponent *cpt)
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

		images = as_screenshot_get_images_all (scr);
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
 * as_yaml_emit_icons:
 */
static void
as_yaml_emit_icons (yaml_emitter_t *emitter, GPtrArray *icons)
{
	guint i;
	GHashTableIter iter;
	gpointer key, value;
	gboolean stock_icon_added = FALSE;
	g_autoptr(GHashTable) icons_table = NULL;

	/* we need to emit the icons in order, so we first need to sort them properly */
	icons_table = g_hash_table_new_full (g_direct_hash,
					     g_direct_equal,
					     NULL,
					     (GDestroyNotify) g_ptr_array_unref);
	for (i = 0; i < icons->len; i++) {
		GPtrArray *ilist;
		AsIconKind ikind;
		AsIcon *icon = AS_ICON (g_ptr_array_index (icons, i));

		ikind = as_icon_get_kind (icon);
		ilist = g_hash_table_lookup (icons_table, GINT_TO_POINTER (ikind));
		if (ilist == NULL) {
			ilist = g_ptr_array_new ();
			g_hash_table_insert (icons_table, GINT_TO_POINTER (ikind), ilist);
		}

		g_ptr_array_add (ilist, icon);
	}

	g_hash_table_iter_init (&iter, icons_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GPtrArray *ilist;
		AsIconKind ikind;

		ikind = (AsIconKind) GPOINTER_TO_INT (key);
		ilist = (GPtrArray*) value;

		if (ikind == AS_ICON_KIND_STOCK) {
			/* there can always be only one stock icon, so this is easy */
			if (!stock_icon_added)
				as_yaml_emit_entry (emitter,
						    as_icon_kind_to_string (ikind),
						    as_icon_get_name (AS_ICON (g_ptr_array_index (ilist, 0))));
			stock_icon_added = TRUE;
		} else {
			as_yaml_emit_scalar (emitter, as_icon_kind_to_string (ikind));
			as_yaml_sequence_start (emitter);

			for (i = 0; i < ilist->len; i++) {
				gchar *size;
				AsIcon *icon = AS_ICON (g_ptr_array_index (ilist, i));

				as_yaml_mapping_start (emitter);

				if (ikind == AS_ICON_KIND_REMOTE)
					as_yaml_emit_entry (emitter, "url", as_icon_get_url (icon));
				else if (ikind == AS_ICON_KIND_LOCAL)
					as_yaml_emit_entry (emitter, "name", as_icon_get_filename (icon));
				else
					as_yaml_emit_entry (emitter, "name", as_icon_get_name (icon));

				if (as_icon_get_width (icon) > 0) {
					size = g_strdup_printf("%i", as_icon_get_width (icon));
					as_yaml_emit_entry (emitter, "width", size);
					g_free (size);
				}

				if (as_icon_get_height (icon) > 0) {
					size = g_strdup_printf("%i", as_icon_get_height (icon));
					as_yaml_emit_entry (emitter, "height", size);
					g_free (size);
				}

				as_yaml_mapping_end (emitter);
			}

			as_yaml_sequence_end (emitter);
		}
	}
}

/**
 * as_yaml_emit_languages:
 */
static void
as_yaml_emit_languages (yaml_emitter_t *emitter, AsComponent *cpt)
{
	GHashTableIter iter;
	gpointer key, value;
	GHashTable *lang_table;

	lang_table = as_component_get_languages_table (cpt);
	if (g_hash_table_size (lang_table) == 0)
		return;

	as_yaml_emit_scalar (emitter, "Languages");
	as_yaml_sequence_start (emitter);

	g_hash_table_iter_init (&iter, lang_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		guint percentage;
		const gchar *locale;
		g_autofree gchar *percentage_str = NULL;

		locale = (const gchar*) key;
		percentage = GPOINTER_TO_INT (value);
		percentage_str = g_strdup_printf("%i", percentage);

		as_yaml_mapping_start (emitter);
		as_yaml_emit_entry (emitter, "locale", locale);
		as_yaml_emit_entry (emitter, "percentage", percentage_str);
		as_yaml_mapping_end (emitter);
	}

	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_emit_releases:
 */
static void
as_yaml_data_emit_releases (AsYAMLData *ydt, yaml_emitter_t *emitter, AsComponent *cpt)
{
	guint i;
	GPtrArray *releases;
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);

	releases = as_component_get_releases (cpt);
	if (releases->len == 0)
		return;

	as_yaml_emit_scalar (emitter, "Releases");
	as_yaml_sequence_start (emitter);

	for (i = 0; i < releases->len; i++) {
		guint j;
		guint64 unixtime;
		AsUrgencyKind urgency;
		GPtrArray *locations;
		AsRelease *rel = AS_RELEASE (g_ptr_array_index (releases, i));

		/* start mapping for this release */
		as_yaml_mapping_start (emitter);

		/* version */
		as_yaml_emit_entry (emitter,
				    "version",
				    as_release_get_version (rel));

		/* timestamp & date */
		unixtime = as_release_get_timestamp (rel);
		if (unixtime > 0) {
			g_autofree gchar *time_str = NULL;

			if (priv->mode == AS_PARSER_MODE_COLLECTION) {
				time_str = g_strdup_printf ("%" G_GUINT64_FORMAT, unixtime);
				as_yaml_emit_entry (emitter, "unix-timestamp", time_str);
			} else {
				GTimeVal time;
				time.tv_sec = unixtime;
				time.tv_usec = 0;
				time_str = g_time_val_to_iso8601 (&time);
				as_yaml_emit_entry (emitter, "date", time_str);
			}
		}

		/* urgency */
		urgency = as_release_get_urgency (rel);
		if (urgency != AS_URGENCY_KIND_UNKNOWN) {
			as_yaml_emit_entry (emitter,
					    "urgency",
					    as_urgency_kind_to_string (urgency));
		}

		/* description */
		as_yaml_emit_long_localized_entry (emitter,
						   "description",
						   as_release_get_description_table (rel));

		/* location URLs */
		locations = as_release_get_locations (rel);
		if (locations->len > 0) {
			as_yaml_emit_scalar (emitter, "locations");
			as_yaml_sequence_start (emitter);
			for (j = 0; j < locations->len; j++) {
				const gchar *lurl;
				lurl = (const gchar*) g_ptr_array_index (locations, j);
				as_yaml_emit_scalar (emitter, lurl);
			}

			as_yaml_sequence_end (emitter);
		}

		/* TODO: Checksum and size are missing, because they are not specified yet for DEP-11.
		 * Will need to be added when/if we need it. */

		/* end mapping for the release */
		as_yaml_mapping_end (emitter);
	}

	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_emit_bundles:
 */
static void
as_yaml_emit_bundles (yaml_emitter_t *emitter, AsComponent *cpt)
{
	guint i;
	GPtrArray *bundles;

	bundles = as_component_get_bundles (cpt);
	if (bundles->len == 0)
		return;

	as_yaml_emit_scalar (emitter, "Bundles");
	as_yaml_sequence_start (emitter);

	for (i = 0; i < bundles->len; i++) {
		AsBundle *bundle = AS_BUNDLE (g_ptr_array_index (bundles, i));

		/* start mapping for this bundle */
		as_yaml_mapping_start (emitter);

		/* type */
		as_yaml_emit_entry (emitter,
				    "type",
				    as_bundle_kind_to_string (as_bundle_get_kind (bundle)));

		/* ID */
		as_yaml_emit_entry (emitter,
				    "id",
				    as_bundle_get_id (bundle));

		/* end mapping for the bundle */
		as_yaml_mapping_end (emitter);
	}

	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_data_emit_suggests:
 */
static void
as_yaml_data_emit_suggests (AsYAMLData *ydt, yaml_emitter_t *emitter, AsComponent *cpt)
{
	guint i;
	GPtrArray *suggestions;

	suggestions = as_component_get_suggested (cpt);
	if (suggestions->len == 0)
		return;

	as_yaml_emit_scalar (emitter, "Suggested");
	as_yaml_sequence_start (emitter);

	for (i = 0; i < suggestions->len; i++) {
		AsSuggested *sug = AS_SUGGESTED (g_ptr_array_index (suggestions, i));

		/* start mapping for this suggestion */
		as_yaml_mapping_start (emitter);

		/* type */
		as_yaml_emit_entry (emitter,
				    "type",
				    as_suggested_kind_to_string (as_suggested_get_kind (sug)));

		/* component-ids */
		as_yaml_emit_sequence (emitter,
				       "ids",
					as_suggested_get_ids (sug));

		/* end mapping for the suggestion */
		as_yaml_mapping_end (emitter);
	}

	as_yaml_sequence_end (emitter);
}

/**
 * as_yaml_serialize_component:
 */
static void
as_yaml_serialize_component (AsYAMLData *ydt, yaml_emitter_t *emitter, AsComponent *cpt)
{
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);
	guint i;
	gint res;
	const gchar *cstr;
	gchar **pkgnames;
	yaml_event_t event;
	AsComponentKind kind;
	GHashTable *htable;
	GPtrArray *icons;

	/* we only serialize a component with minimal necessary information */
	if ((priv->check_valid) && (!as_component_is_valid (cpt))) {
		g_debug ("Can not serialize '%s': Component is invalid.", as_component_get_id (cpt));
		return;
	}

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

	/* Priority */
	if (as_component_get_priority (cpt) != 0) {
		g_autofree gchar *priority_str = g_strdup_printf ("%i", as_component_get_priority (cpt));
		as_yaml_emit_entry (emitter,
				    "Priority",
				    priority_str);
	}

	/* merge strategy */
	if (as_component_get_merge_kind (cpt) != AS_MERGE_KIND_NONE) {
		as_yaml_emit_entry (emitter,
				    "Merge",
				    as_merge_kind_to_string (as_component_get_merge_kind (cpt)));
	}

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
	as_yaml_emit_long_localized_entry (emitter,
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
	as_yaml_emit_sequence_from_str_array (emitter,
						"CompulsoryForDesktops",
						as_component_get_compulsory_for_desktops (cpt));

	/* Categories */
	as_yaml_emit_sequence_from_str_array (emitter,
						"Categories",
						as_component_get_categories (cpt));

	/* Keywords */
	as_yaml_emit_localized_strv (emitter,
					"Keywords",
					as_component_get_keywords_table (cpt));

	/* Urls */
	htable = as_component_get_urls_table (cpt);
	if ((htable != NULL) && (g_hash_table_size (htable) > 0)) {
		as_yaml_emit_scalar (emitter, "Url");

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
		as_yaml_emit_icons (emitter, icons);
		as_yaml_mapping_end (emitter);
	}

	/* Bundles */
	as_yaml_emit_bundles (emitter, cpt);

	/* Provides */
	as_yaml_emit_provides (emitter, cpt);

	/* Screenshots */
	as_yaml_data_emit_screenshots (ydt, emitter, cpt);

	/* Translation details */
	as_yaml_emit_languages (emitter, cpt);

	/* Releases */
	as_yaml_data_emit_releases (ydt, emitter, cpt);

	/* Suggests */
	as_yaml_data_emit_suggests (ydt, emitter, cpt);

	/* close main mapping */
	as_yaml_mapping_end (emitter);

	/* finalize the document */
	yaml_document_end_event_initialize (&event, 1);
	res = yaml_emitter_emit (emitter, &event);
	g_assert (res);
}

/**
 * as_yamldata_write_header:
 *
 * Emit a DEP-11 header for the new document.
 */
static void
as_yamldata_write_header (AsYAMLData *ydt, yaml_emitter_t *emitter)
{
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);
	gint res;
	yaml_event_t event;

	yaml_document_start_event_initialize (&event, NULL, NULL, NULL, FALSE);
	res = yaml_emitter_emit (emitter, &event);
	g_assert (res);

	as_yaml_mapping_start (emitter);

	as_yaml_emit_entry (emitter, "File", "DEP-11");
	as_yaml_emit_entry (emitter, "Version", "0.8");
	as_yaml_emit_entry (emitter, "Origin", priv->origin);
	if (priv->media_baseurl != NULL)
		as_yaml_emit_entry (emitter, "MediaBaseUrl", priv->media_baseurl);
	if (priv->arch != NULL)
		as_yaml_emit_entry (emitter, "Architecture", priv->arch);
	if (priv->default_priority != 0)
		as_yaml_emit_entry (emitter, "Priority", priv->arch);

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
 * as_yamldata_serialize_to_collection:
 */
gchar*
as_yamldata_serialize_to_collection (AsYAMLData *ydt, GPtrArray *cpts, gboolean write_header, gboolean add_timestamp, GError **error)
{
	yaml_emitter_t emitter;
	yaml_event_t event;
	GString *out_data;
	gboolean res = FALSE;
	guint i;

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
		as_yamldata_write_header (ydt, &emitter);

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
 * as_yamldata_parse_collection_data:
 * @ydt: An instance of #AsYAMLData
 * @data: YAML metadata to parse
 *
 * Read an array of #AsComponent from AppStream YAML metadata.
 *
 * Returns: (transfer container) (element-type AsComponent): An array of #AsComponent or %NULL
 */
GPtrArray*
as_yamldata_parse_collection_data (AsYAMLData *ydt, const gchar *data, GError **error)
{
	yaml_parser_t parser;
	yaml_event_t event;
	gboolean header = TRUE;
	gboolean parse = TRUE;
	gboolean ret = TRUE;
	g_autoptr(GPtrArray) cpts = NULL;
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

	g_free (priv->arch);
	priv->arch = NULL;

	g_free (priv->media_baseurl);
	priv->media_baseurl = NULL;

	priv->default_priority = 0;

	/* initialize YAML parser */
	yaml_parser_initialize (&parser);
	yaml_parser_set_input_string (&parser, (unsigned char*) data, strlen (data));

	while (parse) {
		if (!yaml_parser_parse (&parser, &event)) {
			g_set_error (error,
					AS_METADATA_ERROR,
					AS_METADATA_ERROR_PARSE,
					"Invalid DEP-11 file found. Could not parse YAML: %s", parser.problem);
			ret = FALSE;
			break;
		}

		if (event.type == YAML_DOCUMENT_START_EVENT) {
			GNode *n;
			const gchar *key;
			const gchar *value;
			AsComponent *cpt;
			gboolean header_found = FALSE;
			GError *tmp_error = NULL;
			g_autoptr(GNode) root = NULL;

			root = g_node_new (g_strdup (""));
			as_yaml_process_layer (&parser, root, &tmp_error);
			if (tmp_error != NULL) {
				/* stop immediately, since we found an error when parsing the document */
				g_propagate_error (error, tmp_error);
				g_free (root->data);
				yaml_event_delete (&event);
				ret = FALSE;
				parse = FALSE;
				break;
			}

			if (header) {
				for (n = root->children; n != NULL; n = n->next) {
					if ((n->data == NULL) || (n->children == NULL)) {
						parse = FALSE;
						g_set_error_literal (error,
								AS_METADATA_ERROR,
								AS_METADATA_ERROR_FAILED,
								"Invalid DEP-11 file found: Header invalid");
						ret = FALSE;
						break;
					}

					key = as_yaml_node_get_key (n);
					value = as_yaml_node_get_value (n);

					if (g_strcmp0 (key, "File") == 0) {
						if (g_strcmp0 (value, "DEP-11") != 0) {
							parse = FALSE;
							g_set_error_literal (error,
									AS_METADATA_ERROR,
									AS_METADATA_ERROR_FAILED,
									"Invalid DEP-11 file found: Header invalid");
						}
						header_found = TRUE;
					}

					if (!header_found)
						break;

					if (g_strcmp0 (key, "Origin") == 0) {
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
					} else if (g_strcmp0 (key, "Architecture") == 0) {
						if ((value != NULL) && (priv->arch == NULL)) {
							priv->arch = g_strdup (value);
						}
					}
				}
			}
			header = FALSE;

			if (!header_found) {
				cpt = as_yamldata_process_component_node (ydt, root);
				if (cpt == NULL) {
					g_warning ("Parsing of YAML metadata failed: Could not read data for component.");
					parse = FALSE;
					ret = FALSE;
				}

				/* add found component to the results set */
				g_ptr_array_add (cpts, cpt);
			}

			g_node_traverse (root,
					G_IN_ORDER,
					G_TRAVERSE_ALL,
					-1,
					as_yaml_free_node,
					NULL);
		}

		/* stop if end of stream is reached */
		if (event.type == YAML_STREAM_END_EVENT)
			parse = FALSE;

		yaml_event_delete (&event);
	}

	yaml_parser_delete (&parser);

	/* return NULL on error, otherwise return the list of found components */
	if (ret)
		return g_ptr_array_ref (cpts);
	else
		return NULL;
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
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);
	g_free (priv->locale);
	priv->locale = g_strdup (locale);
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
const gchar*
as_yamldata_get_locale (AsYAMLData *ydt)
{
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);
	return priv->locale;
}

/**
 * as_yamldata_set_check_valid:
 * @check: %TRUE if check should be enabled.
 *
 * Set whether we should perform basic validity checks on the component
 * before serializing it to YAML.
 */
void
as_yamldata_set_check_valid (AsYAMLData *ydt, gboolean check)
{
	AsYAMLDataPrivate *priv = GET_PRIVATE (ydt);
	priv->check_valid = check;
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
