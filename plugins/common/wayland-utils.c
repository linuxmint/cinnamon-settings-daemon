/* wayland-utils.c - Wayland utility functions for CSD plugins
 *
 * Copyright (C) 2026 Linux Mint
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "wayland-utils.h"

#include <wayland-client.h>

static void
registry_handle_global (void                *data,
                        struct wl_registry  *registry,
                        uint32_t             name,
                        const char          *interface,
                        uint32_t             version)
{
    if (g_strcmp0 (interface, "zwlr_layer_shell_v1") == 0)
        *(gboolean *) data = TRUE;
}

static void
registry_handle_global_remove (void               *data,
                               struct wl_registry *registry,
                               uint32_t            name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove,
};

/**
 * csd_check_layer_shell_support:
 *
 * Check if wlr-layer-shell protocol is supported by connecting to the
 * Wayland display and checking the registry. This can be called before
 * gtk_init() unlike gtk_layer_is_supported().
 *
 * Returns: %TRUE if layer shell is available, %FALSE otherwise
 */
gboolean
csd_check_layer_shell_support (void)
{
    struct wl_display *display;
    struct wl_registry *registry;
    gboolean found = FALSE;

    display = wl_display_connect (NULL);
    if (!display)
    {
        g_debug ("csd_check_layer_shell_support: could not connect to Wayland display");
        return FALSE;
    }

    registry = wl_display_get_registry (display);
    if (!registry)
    {
        g_debug ("csd_check_layer_shell_support: failed to get Wayland registry");
        wl_display_disconnect (display);
        return FALSE;
    }

    wl_registry_add_listener (registry, &registry_listener, &found);

    if (wl_display_roundtrip (display) < 0)
    {
        g_debug ("csd_check_layer_shell_support: Wayland roundtrip failed");
        wl_registry_destroy (registry);
        wl_display_disconnect (display);
        return FALSE;
    }

    wl_registry_destroy (registry);
    wl_display_disconnect (display);

    if (!found)
        g_debug ("csd_check_layer_shell_support: wlr-layer-shell protocol not advertised by compositor");

    return found;
}
