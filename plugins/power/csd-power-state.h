/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __CSD_POWER_STATE_H
#define __CSD_POWER_STATE_H

#include <glib.h>
#include <libupower-glib/upower.h>

G_BEGIN_DECLS

typedef struct {
        gboolean     valid;
        UpDeviceKind kind;
        guint        percentage;
} CsdPowerPercentageState;

void csd_power_percentage_state_consider (CsdPowerPercentageState *state,
                                          UpDeviceKind             kind,
                                          gboolean                 is_present,
                                          gdouble                  percentage);

G_END_DECLS

#endif /* __CSD_POWER_STATE_H */
