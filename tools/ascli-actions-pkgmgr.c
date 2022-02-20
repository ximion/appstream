/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015-2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ascli-actions-pkgmgr.h"

#include <config.h>
#include <glib/gi18n-lib.h>
#include <unistd.h>
#include <errno.h>

#include "ascli-utils.h"

/**
 * exec_pm_action:
 *
 * Run the native package manager to perform an action (install/remove) on
 * a set of packages.
 * The PM will replace the current process tree.
 */
static int
exec_pm_action (const gchar *action, gchar **pkgnames)
{
	int ret;
	const gchar *exe = NULL;
	g_auto(GStrv) cmd = NULL;

#ifdef HAVE_APT_SUPPORT
	if (g_file_test ("/usr/bin/apt", G_FILE_TEST_EXISTS))
		exe = "/usr/bin/apt";
#endif
	if (exe == NULL) {
		if (g_file_test ("/usr/bin/pkcon", G_FILE_TEST_EXISTS)) {
			exe = "/usr/bin/pkcon";
		} else {
			g_printerr ("%s\n", _("No suitable package manager CLI found. Please make sure that e.g. \"pkcon\" (part of PackageKit) is available."));
			return ASCLI_EXIT_CODE_FAILED;
		}
	}

	cmd = g_new0 (gchar*, 3 + g_strv_length (pkgnames) + 1);
	cmd[0] = g_strdup (exe);
	cmd[1] = g_strdup (action);
	for (guint i = 0; pkgnames[i] != NULL; i++) {
		cmd[2+i] = g_strdup (pkgnames[i]);
	}

	ret = execv (exe, cmd);
	if (ret != 0)
		ascli_print_stderr (_("Unable to spawn package manager: %s"), g_strerror (errno));
	return ret;
}

/**
 * exec_flatpak_action:
 *
 * Run the Flatpak to perform an action (install/remove).
 */
static int
exec_flatpak_action (const gchar *action, const gchar *bundle_id)
{
	int ret;
	const gchar *exe = NULL;
	g_auto(GStrv) cmd = NULL;

	exe = "/usr/bin/flatpak";
	if (!g_file_test (exe, G_FILE_TEST_EXISTS)) {
		g_printerr ("%s\n", _("Flatpak was not found! Please install it to continue."));
		return ASCLI_EXIT_CODE_FAILED;
	}

	cmd = g_new0 (gchar*, 4 + 1);
	cmd[0] = g_strdup (exe);
	cmd[1] = g_strdup (action);
	cmd[2] = g_strdup (bundle_id);

	ret = execv (exe, cmd);
	if (ret != 0)
		ascli_print_stderr (_("Unable to spawn Flatpak process: %s"),
				    g_strerror (errno));
	return ret;
}

static int
ascli_get_component_instrm_candidate (const gchar *identifier,
				      AsBundleKind bundle_kind,
				      gboolean choose_first,
				      gboolean is_removal,
				      AsComponent **result_cpt)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(AsPool) pool = NULL;
	g_autoptr(GPtrArray) result = NULL;
	g_autoptr(GPtrArray) result_filtered = NULL;
	AsComponent *cpt;

	if (identifier == NULL) {
		ascli_print_stderr (_("You need to specify a component-ID."));
		return ASCLI_EXIT_CODE_BAD_INPUT;
	}

	pool = as_pool_new ();
	as_pool_load (pool, NULL, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		return ASCLI_EXIT_CODE_FAILED;
	}

	result = as_pool_get_components_by_id (pool, identifier);
	if (result->len == 0) {
		ascli_print_stderr (_("Unable to find component with ID '%s'!"), identifier);
		return ASCLI_EXIT_CODE_NO_RESULT;
	}

	if (bundle_kind == AS_BUNDLE_KIND_UNKNOWN) {
		result_filtered = g_ptr_array_ref (result);
	} else {
		result_filtered = g_ptr_array_new_with_free_func (g_object_unref);
		for (guint i = 0; i < result->len; i++) {
			AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (result, i));

			if (bundle_kind == AS_BUNDLE_KIND_PACKAGE &&
			   as_component_get_pkgname (cpt) != NULL) {
				g_ptr_array_add (result_filtered, g_object_ref (cpt));
				continue;
			}

			if (bundle_kind == AS_BUNDLE_KIND_FLATPAK &&
			    as_component_get_bundle (cpt, AS_BUNDLE_KIND_FLATPAK) != NULL) {
				g_ptr_array_add (result_filtered, g_object_ref (cpt));
				continue;
			}

		}
	}

	if (result_filtered->len == 0) {
		ascli_print_stderr (_("Unable to find component with ID '%s' and the selected filter criteria!"),
				    identifier);
		return ASCLI_EXIT_CODE_NO_RESULT;
	}

	if (choose_first || result_filtered->len == 1) {
		cpt = AS_COMPONENT (g_ptr_array_index (result_filtered, 0));
	} else {
		gint selection;
		if (is_removal)
			/* TRANSLATORS: We found multiple components to remove, a list of them is printed below this text. */
			g_print ("%s\n", _("Multiple candidates were found for removal:"));
		else
			/* TRANSLATORS: We found multiple components to install, a list of them is printed below this text. */
			g_print ("%s\n", _("Multiple candidates were found for installation:"));

		for (guint i = 0; i < result_filtered->len; i++) {
			AsBundle *bundle = NULL;
			AsComponent *cpt = AS_COMPONENT (g_ptr_array_index (result_filtered, i));

			bundle = as_component_get_bundle (cpt, AS_BUNDLE_KIND_FLATPAK);
			if (bundle == NULL)
				g_print (" [%d] package:%s ", i + 1, as_component_get_pkgname (cpt));
			else
				g_print (" [%d] bundle:flatpak ", i + 1);
			g_print ("- %s (%s)\n",
				 as_component_get_name (cpt),
				 as_component_get_summary (cpt));
		}

		if (is_removal)
			/* TRANSLATORS: A list of components is displayed with number prefixes. This is a prompt for the user to select one. */
			selection = ascli_prompt_numer (_("Please enter the number of the component to remove:"),
							result_filtered->len);
		else
			/* TRANSLATORS: A list of components is displayed with number prefixes. This is a prompt for the user to select one. */
			selection = ascli_prompt_numer (_("Please enter the number of the component to install:"),
							result_filtered->len);
		cpt = AS_COMPONENT (g_ptr_array_index (result_filtered, selection - 1));
	}

	g_assert (result_cpt != NULL);
	*result_cpt = g_object_ref (cpt);

	if (as_component_get_bundle (cpt, AS_BUNDLE_KIND_FLATPAK) == NULL &&
	    as_component_get_pkgname (cpt) == NULL) {
		ascli_print_stderr (_("Component '%s' has no installation candidate."), identifier);
		return ASCLI_EXIT_CODE_FAILED;
	}

	return ASCLI_EXIT_CODE_SUCCESS;
}

/**
 * ascli_install_component:
 *
 * Install a component matching the given ID.
 */
int
ascli_install_component (const gchar *identifier, AsBundleKind bundle_kind, gboolean choose_first)
{
	gint exit_code = 0;
	g_autoptr(AsComponent) cpt = NULL;

	if (bundle_kind != AS_BUNDLE_KIND_UNKNOWN &&
	    bundle_kind != AS_BUNDLE_KIND_PACKAGE &&
	    bundle_kind != AS_BUNDLE_KIND_FLATPAK) {
		g_warning ("Can not handle bundle kind %s, falling back to none.", as_bundle_kind_to_string (bundle_kind));
		bundle_kind = AS_BUNDLE_KIND_UNKNOWN;
	}

	exit_code = ascli_get_component_instrm_candidate (identifier,
							  bundle_kind,
							  choose_first,
							  FALSE, /* not removal */
							  &cpt);
	if (exit_code != 0)
		return exit_code;

	if (bundle_kind != AS_BUNDLE_KIND_PACKAGE) {
		AsBundle *bundle = as_component_get_bundle (cpt, AS_BUNDLE_KIND_FLATPAK);
		if (bundle != NULL) {
			exit_code = exec_flatpak_action ("install",
							 as_bundle_get_id (bundle));
			return exit_code;
		}
	}

	if (bundle_kind != AS_BUNDLE_KIND_FLATPAK) {
		if (as_component_get_pkgname (cpt) != NULL) {
			exit_code = exec_pm_action ("install",
						    as_component_get_pkgnames (cpt));
			return exit_code;
		}
	}

	g_critical ("Did not install anything even though packages were found. This should not happen.");
	return ASCLI_EXIT_CODE_FATAL;
}

/**
 * ascli_remove_component:
 *
 * Remove a component matching the given ID.
 */
int
ascli_remove_component (const gchar *identifier, AsBundleKind bundle_kind, gboolean choose_first)
{
	gint exit_code = 0;
	g_autoptr(AsComponent) cpt = NULL;

	if (bundle_kind != AS_BUNDLE_KIND_UNKNOWN &&
	    bundle_kind != AS_BUNDLE_KIND_PACKAGE &&
	    bundle_kind != AS_BUNDLE_KIND_FLATPAK) {
		g_warning ("Can not handle bundle kind %s, falling back to none.", as_bundle_kind_to_string (bundle_kind));
		bundle_kind = AS_BUNDLE_KIND_UNKNOWN;
	}

	exit_code = ascli_get_component_instrm_candidate (identifier,
							  bundle_kind,
							  choose_first,
							  TRUE, /* is removal */
							  &cpt);
	if (exit_code != 0)
		return exit_code;

	if (bundle_kind != AS_BUNDLE_KIND_PACKAGE) {
		AsBundle *bundle = as_component_get_bundle (cpt, AS_BUNDLE_KIND_FLATPAK);
		if (bundle != NULL) {
			exit_code = exec_flatpak_action ("remove",
							 as_bundle_get_id (bundle));
			return exit_code;
		}
	}

	if (bundle_kind != AS_BUNDLE_KIND_FLATPAK) {
		if (as_component_get_pkgname (cpt) != NULL) {
			exit_code = exec_pm_action ("remove",
						    as_component_get_pkgnames (cpt));
			return exit_code;
		}
	}

	g_critical ("Did not remove anything even though packages were found. This should not happen.");
	return ASCLI_EXIT_CODE_FATAL;

}
