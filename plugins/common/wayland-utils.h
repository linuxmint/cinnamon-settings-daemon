/* wayland-utils.h - Wayland utility functions for CSD plugins
 *
 * Copyright (C) 2026 Linux Mint
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __WAYLAND_UTILS_H
#define __WAYLAND_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

gboolean csd_check_layer_shell_support (void);

G_END_DECLS

#endif /* __WAYLAND_UTILS_H */
