/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:as-wayland-private
 * @short_description: Read display information from a Wayland compositor.
 *
 * Obtains the size of the largest connected display from the Wayland compositor.
 * Since libappstream must not have a runtime dependency on any GUI library,
 * libwayland-client is loaded dynamically if it is available.
 */

#include "config.h"
#include "as-wayland-private.h"

#include "as-system-info.h"

#ifdef HAVE_WAYLAND

#include <gmodule.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

/* function prototypes of the libwayland-client symbols that we resolve dynamically */
typedef struct wl_display *(*AsWlDisplayConnectFn) (const char *name);
typedef void (*AsWlDisplayDisconnectFn) (struct wl_display *display);
typedef int (*AsWlDisplayRoundtripFn) (struct wl_display *display);
typedef struct wl_proxy *(*AsWlProxyMarshalConstructorFn) (struct wl_proxy *proxy,
							   uint32_t opcode,
							   const struct wl_interface *iface,
							   ...);
typedef struct wl_proxy *(*AsWlProxyMarshalConstructorVersionedFn) (
    struct wl_proxy *proxy,
    uint32_t opcode,
    const struct wl_interface *iface,
    uint32_t version,
    ...);
typedef int (*AsWlProxyAddListenerFn) (struct wl_proxy *proxy,
				       void (**implementation) (void),
				       void *data);
typedef void (*AsWlProxyDestroyFn) (struct wl_proxy *proxy);

/* The core wl_output protocol only advertises integer scale factors, so on a
 * fractionally-scaled display the mode/scale calculation would be wrong.
 * The xdg-output protocol reports the true logical size of each output instead.
 * Its interfaces are generated protocol code and not exported by libwayland-client,
 * so we declare the (frozen) v1 protocol data ourselves.
 * The wl_output slot of the get_xdg_output types is filled at runtime with the
 * wl_output_interface resolved from libwayland-client. */
static const struct wl_interface *as_xdg_types_null[2] = { NULL, NULL };
static const struct wl_interface *as_xdg_get_output_types[2] = { NULL, NULL };

static const struct wl_message as_zxdg_output_v1_requests[] = {
	{ "destroy", "", as_xdg_types_null },
};
static const struct wl_message as_zxdg_output_v1_events[] = {
	{ "logical_position", "ii", as_xdg_types_null },
	{ "logical_size",	  "ii", as_xdg_types_null },
	{ "done",		  "",   as_xdg_types_null },
};
static const struct wl_interface as_zxdg_output_v1_interface = {
	"zxdg_output_v1", 1, 1, as_zxdg_output_v1_requests, 3, as_zxdg_output_v1_events,
};

static const struct wl_message as_zxdg_output_manager_v1_requests[] = {
	{ "destroy",	     "",	 as_xdg_types_null	   },
	{ "get_xdg_output", "no", as_xdg_get_output_types },
};
static const struct wl_interface as_zxdg_output_manager_v1_interface = {
	"zxdg_output_manager_v1", 1, 2, as_zxdg_output_manager_v1_requests, 0, NULL,
};

#define AS_ZXDG_OUTPUT_MANAGER_V1_GET_XDG_OUTPUT 1

typedef struct {
	struct wl_proxy *proxy;
	struct wl_proxy *xdg_proxy;
	gint32 width;
	gint32 height;
	gint32 scale;
	gint32 logical_width;
	gint32 logical_height;
} AsWlOutputInfo;

typedef struct {
	AsWlProxyMarshalConstructorVersionedFn proxy_marshal_constructor_versioned;
	AsWlProxyAddListenerFn proxy_add_listener;
	const struct wl_interface *output_iface;
	struct wl_proxy *xdg_output_manager;
	GPtrArray *outputs;
} AsWlRegistryContext;

static void
as_wl_output_handle_geometry (void *data,
			      struct wl_output *wl_output,
			      int32_t x,
			      int32_t y,
			      int32_t physical_width,
			      int32_t physical_height,
			      int32_t subpixel,
			      const char *make,
			      const char *model,
			      int32_t transform)
{
}

static void
as_wl_output_handle_mode (void *data,
			  struct wl_output *wl_output,
			  uint32_t flags,
			  int32_t width,
			  int32_t height,
			  int32_t refresh)
{
	AsWlOutputInfo *info = data;

	if (flags & WL_OUTPUT_MODE_CURRENT) {
		info->width = width;
		info->height = height;
	}
}

static void
as_wl_output_handle_done (void *data, struct wl_output *wl_output)
{
}

static void
as_wl_output_handle_scale (void *data, struct wl_output *wl_output, int32_t factor)
{
	AsWlOutputInfo *info = data;

	info->scale = factor;
}

static const struct wl_output_listener as_wl_output_listener = {
	.geometry = as_wl_output_handle_geometry,
	.mode = as_wl_output_handle_mode,
	.done = as_wl_output_handle_done,
	.scale = as_wl_output_handle_scale,
};

/* listener vtable matching the zxdg_output_v1 (version 1) event order */
typedef struct {
	void (*logical_position) (void *data, struct wl_proxy *xdg_output, int32_t x, int32_t y);
	void (*logical_size) (void *data,
			      struct wl_proxy *xdg_output,
			      int32_t width,
			      int32_t height);
	void (*done) (void *data, struct wl_proxy *xdg_output);
} AsXdgOutputListener;

static void
as_xdg_output_handle_logical_position (void *data,
				       struct wl_proxy *xdg_output,
				       int32_t x,
				       int32_t y)
{
}

static void
as_xdg_output_handle_logical_size (void *data,
				   struct wl_proxy *xdg_output,
				   int32_t width,
				   int32_t height)
{
	AsWlOutputInfo *info = data;

	info->logical_width = width;
	info->logical_height = height;
}

static void
as_xdg_output_handle_done (void *data, struct wl_proxy *xdg_output)
{
}

static const AsXdgOutputListener as_xdg_output_listener = {
	.logical_position = as_xdg_output_handle_logical_position,
	.logical_size = as_xdg_output_handle_logical_size,
	.done = as_xdg_output_handle_done,
};

static void
as_wl_registry_handle_global (void *data,
			      struct wl_registry *registry,
			      uint32_t name,
			      const char *interface,
			      uint32_t version)
{
	AsWlRegistryContext *ctx = data;
	AsWlOutputInfo *info = NULL;
	struct wl_proxy *output_proxy = NULL;
	uint32_t bind_version;

	if (g_strcmp0 (interface, "zxdg_output_manager_v1") == 0) {
		if (ctx->xdg_output_manager != NULL)
			return;
		ctx->xdg_output_manager = ctx->proxy_marshal_constructor_versioned (
		    (struct wl_proxy *) registry,
		    WL_REGISTRY_BIND,
		    &as_zxdg_output_manager_v1_interface,
		    1,
		    name,
		    as_zxdg_output_manager_v1_interface.name,
		    1,
		    NULL);
		return;
	}

	if (g_strcmp0 (interface, "wl_output") != 0)
		return;

	/* version 2 is all we need to receive the mode and scale events */
	bind_version = MIN (version, 2);
	output_proxy = ctx->proxy_marshal_constructor_versioned ((struct wl_proxy *) registry,
								 WL_REGISTRY_BIND,
								 ctx->output_iface,
								 bind_version,
								 name,
								 ctx->output_iface->name,
								 bind_version,
								 NULL);
	if (output_proxy == NULL)
		return;

	info = g_new0 (AsWlOutputInfo, 1);
	info->proxy = output_proxy;
	info->scale = 1;
	g_ptr_array_add (ctx->outputs, info);

	ctx->proxy_add_listener (output_proxy, (void (**) (void)) &as_wl_output_listener, info);
}

static void
as_wl_registry_handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener as_wl_registry_listener = {
	.global = as_wl_registry_handle_global,
	.global_remove = as_wl_registry_handle_global_remove,
};

/**
 * as_wayland_display_get_largest_output_size:
 * @logical_width: (out): the width of the largest output in logical pixels.
 * @logical_height: (out): the height of the largest output in logical pixels.
 * @error: a #GError
 *
 * Connect to the Wayland compositor of the current session, find the
 * largest output and return its size in logical (device-independent) pixels.
 *
 * Not running in a Wayland session, or being in a session without any usable
 * display, is not considered an error, %AS_CHECK_RESULT_FALSE is returned
 * in that case.
 *
 * Returns: %AS_CHECK_RESULT_TRUE if a display size was determined,
 *          %AS_CHECK_RESULT_FALSE if no display was found,
 *          %AS_CHECK_RESULT_ERROR and @error set on failure.
 */
AsCheckResult
as_wayland_display_get_largest_output_size (gulong *logical_width,
					    gulong *logical_height,
					    GError **error)
{
	GModule *module = NULL;
	g_autoptr(GPtrArray) outputs = NULL;
	AsWlDisplayConnectFn display_connect;
	AsWlDisplayDisconnectFn display_disconnect;
	AsWlDisplayRoundtripFn display_roundtrip;
	AsWlProxyMarshalConstructorFn proxy_marshal_constructor;
	AsWlProxyMarshalConstructorVersionedFn proxy_marshal_constructor_versioned;
	AsWlProxyAddListenerFn proxy_add_listener;
	AsWlProxyDestroyFn proxy_destroy;
	const struct wl_interface *registry_iface;
	const struct wl_interface *output_iface;
	struct wl_display *display = NULL;
	struct wl_proxy *registry = NULL;
	AsWlRegistryContext ctx;
	gulong best_area = 0;
	AsCheckResult ret = AS_CHECK_RESULT_FALSE;

	g_return_val_if_fail (logical_width != NULL, AS_CHECK_RESULT_ERROR);
	g_return_val_if_fail (logical_height != NULL, AS_CHECK_RESULT_ERROR);

	/* not being in a Wayland session just means there is no display for us to find,
	 * so don't even try to load Wayland in that case */
	if (g_getenv ("WAYLAND_DISPLAY") == NULL && g_getenv ("WAYLAND_SOCKET") == NULL) {
		g_debug ("Not running in a Wayland session, unable to read display information.");
		return AS_CHECK_RESULT_FALSE;
	}

	module = g_module_open ("libwayland-client.so.0", G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
	if (module == NULL) {
		g_set_error (error,
			     AS_SYSTEM_INFO_ERROR,
			     AS_SYSTEM_INFO_ERROR_FAILED,
			     "Unable to load libwayland-client: %s",
			     g_module_error ());
		return AS_CHECK_RESULT_ERROR;
	}

	if (!g_module_symbol (module, "wl_display_connect", (gpointer *) &display_connect) ||
	    !g_module_symbol (module, "wl_display_disconnect", (gpointer *) &display_disconnect) ||
	    !g_module_symbol (module, "wl_display_roundtrip", (gpointer *) &display_roundtrip) ||
	    !g_module_symbol (module,
			      "wl_proxy_marshal_constructor",
			      (gpointer *) &proxy_marshal_constructor) ||
	    !g_module_symbol (module,
			      "wl_proxy_marshal_constructor_versioned",
			      (gpointer *) &proxy_marshal_constructor_versioned) ||
	    !g_module_symbol (module, "wl_proxy_add_listener", (gpointer *) &proxy_add_listener) ||
	    !g_module_symbol (module, "wl_proxy_destroy", (gpointer *) &proxy_destroy) ||
	    !g_module_symbol (module, "wl_registry_interface", (gpointer *) &registry_iface) ||
	    !g_module_symbol (module, "wl_output_interface", (gpointer *) &output_iface)) {
		g_set_error (error,
			     AS_SYSTEM_INFO_ERROR,
			     AS_SYSTEM_INFO_ERROR_FAILED,
			     "Unable to resolve symbols from libwayland-client: %s",
			     g_module_error ());
		g_module_close (module);
		return AS_CHECK_RESULT_ERROR;
	}

	display = display_connect (NULL);
	if (display == NULL) {
		g_set_error_literal (error,
				     AS_SYSTEM_INFO_ERROR,
				     AS_SYSTEM_INFO_ERROR_FAILED,
				     "Unable to connect to the Wayland display of this session.");
		g_module_close (module);
		return AS_CHECK_RESULT_ERROR;
	}

	/* complete our hand-declared xdg-output protocol data with the
	 * wl_output interface resolved from libwayland-client */
	as_xdg_get_output_types[0] = &as_zxdg_output_v1_interface;
	as_xdg_get_output_types[1] = output_iface;

	outputs = g_ptr_array_new_with_free_func (g_free);
	ctx.proxy_marshal_constructor_versioned = proxy_marshal_constructor_versioned;
	ctx.proxy_add_listener = proxy_add_listener;
	ctx.output_iface = output_iface;
	ctx.xdg_output_manager = NULL;
	ctx.outputs = outputs;

	/* equivalent to the wl_display_get_registry() static-inline helper, which we
	 * must not use as it would create a link-time dependency on libwayland-client */
	registry = proxy_marshal_constructor ((struct wl_proxy *) display,
					      WL_DISPLAY_GET_REGISTRY,
					      registry_iface,
					      NULL);
	if (registry == NULL) {
		g_set_error_literal (error,
				     AS_SYSTEM_INFO_ERROR,
				     AS_SYSTEM_INFO_ERROR_FAILED,
				     "Unable to obtain the Wayland registry.");
		ret = AS_CHECK_RESULT_ERROR;
		goto out;
	}
	proxy_add_listener (registry, (void (**) (void)) &as_wl_registry_listener, &ctx);

	/* first roundtrip fetches the globals and binds the outputs
	 * as well as the xdg-output manager, if the compositor provides it */
	if (display_roundtrip (display) < 0) {
		g_set_error_literal (error,
				     AS_SYSTEM_INFO_ERROR,
				     AS_SYSTEM_INFO_ERROR_FAILED,
				     "Failed to communicate with the Wayland compositor.");
		ret = AS_CHECK_RESULT_ERROR;
		goto out;
	}

	/* request the true logical size of each output via xdg-output, since the
	 * core protocol only carries integer scale factors and would yield wrong
	 * results on fractionally-scaled displays */
	if (ctx.xdg_output_manager != NULL) {
		for (guint i = 0; i < outputs->len; i++) {
			AsWlOutputInfo *info = g_ptr_array_index (outputs, i);

			info->xdg_proxy = proxy_marshal_constructor (
			    ctx.xdg_output_manager,
			    AS_ZXDG_OUTPUT_MANAGER_V1_GET_XDG_OUTPUT,
			    &as_zxdg_output_v1_interface,
			    NULL,
			    info->proxy);
			if (info->xdg_proxy == NULL)
				continue;
			proxy_add_listener (info->xdg_proxy,
					    (void (**) (void)) &as_xdg_output_listener,
					    info);
		}
	}

	/* the second roundtrip delivers the events of the bound outputs */
	if (display_roundtrip (display) < 0) {
		g_set_error_literal (error,
				     AS_SYSTEM_INFO_ERROR,
				     AS_SYSTEM_INFO_ERROR_FAILED,
				     "Failed to communicate with the Wayland compositor.");
		ret = AS_CHECK_RESULT_ERROR;
		goto out;
	}

	/* find the output with the largest logical area */
	for (guint i = 0; i < outputs->len; i++) {
		AsWlOutputInfo *info = g_ptr_array_index (outputs, i);
		gulong width, height, area;

		if (info->logical_width > 0 && info->logical_height > 0) {
			/* exact logical size as reported via xdg-output */
			width = info->logical_width;
			height = info->logical_height;
		} else {
			/* fall back to the approximate integer-scaled size */
			if (info->width <= 0 || info->height <= 0 || info->scale <= 0)
				continue;
			width = info->width / info->scale;
			height = info->height / info->scale;
		}
		area = width * height;
		if (area > best_area) {
			best_area = area;
			*logical_width = width;
			*logical_height = height;
			ret = AS_CHECK_RESULT_TRUE;
		}
	}

	if (ret != AS_CHECK_RESULT_TRUE)
		g_debug ("No usable Wayland output was found to determine the display size.");

out:
	for (guint i = 0; i < outputs->len; i++) {
		AsWlOutputInfo *info = g_ptr_array_index (outputs, i);
		if (info->xdg_proxy != NULL)
			proxy_destroy (info->xdg_proxy);
		proxy_destroy (info->proxy);
	}
	if (ctx.xdg_output_manager != NULL)
		proxy_destroy (ctx.xdg_output_manager);
	if (registry != NULL)
		proxy_destroy (registry);
	display_disconnect (display);
	g_module_close (module);

	return ret;
}

#else /* !HAVE_WAYLAND */

AsCheckResult
as_wayland_display_get_largest_output_size (gulong *logical_width,
					    gulong *logical_height,
					    GError **error)
{
	g_set_error_literal (error,
			     AS_SYSTEM_INFO_ERROR,
			     AS_SYSTEM_INFO_ERROR_FAILED,
			     "AppStream was built without support for reading display "
			     "information from Wayland.");
	return AS_CHECK_RESULT_ERROR;
}

#endif
