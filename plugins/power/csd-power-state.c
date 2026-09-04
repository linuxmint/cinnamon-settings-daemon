/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include "csd-power-state.h"

static guint
percentage_kind_priority (UpDeviceKind kind)
{
        if (kind == UP_DEVICE_KIND_BATTERY)
                return 2;
        if (kind == UP_DEVICE_KIND_UPS)
                return 1;
        return 0;
}

void
csd_power_percentage_state_consider (CsdPowerPercentageState *state,
                                     UpDeviceKind             kind,
                                     gboolean                 is_present,
                                     gdouble                  percentage)
{
        guint priority;

        g_return_if_fail (state != NULL);

        if (!is_present)
                return;

        priority = percentage_kind_priority (kind);
        if (priority == 0)
                return;

        if (state->valid &&
            priority <= percentage_kind_priority (state->kind))
                return;

        state->valid = TRUE;
        state->kind = kind;
        state->percentage = (guint) CLAMP (percentage, 0.0, 100.0);
}
