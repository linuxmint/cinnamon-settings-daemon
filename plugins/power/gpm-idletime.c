/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpm-idletime.h"

static void     gpm_idletime_finalize   (GObject       *object);

#define GPM_IDLETIME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GPM_IDLETIME_TYPE, GpmIdletimePrivate))

struct GpmIdletimePrivate
{
        gint                     sync_event;
        gboolean                 reset_set;
        XSyncCounter             idle_counter;
        GPtrArray               *array;
        Display                 *dpy;
};

typedef struct
{
        guint                    id;
        XSyncValue               timeout;
        XSyncAlarm               xalarm;
        GpmIdletime             *idletime;
} GpmIdletimeAlarm;

enum {
        SIGNAL_ALARM_EXPIRED,
        SIGNAL_RESET,
        LAST_SIGNAL
};

typedef enum {
        GPM_IDLETIME_ALARM_TYPE_POSITIVE,
        GPM_IDLETIME_ALARM_TYPE_NEGATIVE,
        GPM_IDLETIME_ALARM_TYPE_DISABLED
} GpmIdletimeAlarmType;

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpmIdletime, gpm_idletime, G_TYPE_OBJECT)

static gint64
gpm_idletime_xsyncvalue_to_int64 (XSyncValue value)
{
        return ((guint64) XSyncValueHigh32 (value)) << 32 |
                (guint64) XSyncValueLow32 (value);
}

/* gets the IDLETIME counter value, or 0 for invalid */
gint64
gpm_idletime_get_time (GpmIdletime *idletime)
{
        XSyncValue value;

        /* we don't have IDLETIME support */
        if (!idletime->priv->idle_counter)
                return 0;

        /* NX explodes if you query the counter */
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        XSyncQueryCounter (idletime->priv->dpy,
                           idletime->priv->idle_counter,
                           &value);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()))
                return 0;
        return gpm_idletime_xsyncvalue_to_int64 (value);
}

static void
gpm_idletime_xsync_alarm_set (GpmIdletime *idletime,
                              GpmIdletimeAlarm *alarm_item,
                              GpmIdletimeAlarmType alarm_type)
{
        XSyncAlarmAttributes attr;
        XSyncValue delta;
        unsigned int flags;
        XSyncTestType test;

        /* just remove it */
        if (alarm_type == GPM_IDLETIME_ALARM_TYPE_DISABLED) {
                if (alarm_item->xalarm) {
                        XSyncDestroyAlarm (idletime->priv->dpy,
                                           alarm_item->xalarm);
                        alarm_item->xalarm = None;
                }
                return;
        }

        /* which way do we do the test? */
        if (alarm_type == GPM_IDLETIME_ALARM_TYPE_POSITIVE)
                test = XSyncPositiveTransition;
        else
                test = XSyncNegativeTransition;

        XSyncIntToValue (&delta, 0);

        attr.trigger.counter = idletime->priv->idle_counter;
        attr.trigger.value_type = XSyncAbsolute;
        attr.trigger.test_type = test;
        attr.trigger.wait_value = alarm_item->timeout;
        attr.delta = delta;

        flags = XSyncCACounter |
                XSyncCAValueType |
                XSyncCATestType |
                XSyncCAValue |
                XSyncCADelta;

        if (alarm_item->xalarm) {
                XSyncChangeAlarm (idletime->priv->dpy,
                                  alarm_item->xalarm,
                                  flags,
                                  &attr);
        } else {
                alarm_item->xalarm = XSyncCreateAlarm (idletime->priv->dpy,
                                                       flags,
                                                       &attr);
        }
}

void
gpm_idletime_alarm_reset_all (GpmIdletime *idletime)
{
        guint i;
        GpmIdletimeAlarm *alarm_item;

        g_return_if_fail (GPM_IS_IDLETIME (idletime));

        if (!idletime->priv->reset_set)
                return;

        /* reset all the alarms (except the reset alarm) to their timeouts */
        for (i=1; i < idletime->priv->array->len; i++) {
                alarm_item = g_ptr_array_index (idletime->priv->array, i);
                gpm_idletime_xsync_alarm_set (idletime,
                                              alarm_item,
                                              GPM_IDLETIME_ALARM_TYPE_POSITIVE);
        }

        /* set the reset alarm to be disabled */
        alarm_item = g_ptr_array_index (idletime->priv->array, 0);
        gpm_idletime_xsync_alarm_set (idletime,
                                      alarm_item,
                                      GPM_IDLETIME_ALARM_TYPE_DISABLED);

        /* emit signal so say we've reset all timers */
        g_signal_emit (idletime, signals [SIGNAL_RESET], 0);

        /* we need to be reset again on the next event */
        idletime->priv->reset_set = FALSE;
}

static GpmIdletimeAlarm *
gpm_idletime_alarm_find_id (GpmIdletime *idletime, guint id)
{
        guint i;
        GpmIdletimeAlarm *alarm_item;
        for (i = 0; i < idletime->priv->array->len; i++) {
                alarm_item = g_ptr_array_index (idletime->priv->array, i);
                if (alarm_item->id == id)
                        return alarm_item;
        }
        return NULL;
}

static void
gpm_idletime_set_reset_alarm (GpmIdletime *idletime,
                              XSyncAlarmNotifyEvent *alarm_event)
{
        GpmIdletimeAlarm *alarm_item;
        int overflow;
        XSyncValue add;
        gint64 current, reset_threshold;

        alarm_item = gpm_idletime_alarm_find_id (idletime, 0);

        if (!idletime->priv->reset_set) {
                /* don't match on the current value because
                 * XSyncNegativeComparison means less or equal. */
                XSyncIntToValue (&add, -1);
                XSyncValueAdd (&alarm_item->timeout,
                              alarm_event->counter_value,
                              add,
                              &overflow);

                /* set the reset alarm to fire the next time
                 * idletime->priv->idle_counter < the current counter value */
                gpm_idletime_xsync_alarm_set (idletime,
                                              alarm_item,
                                              GPM_IDLETIME_ALARM_TYPE_NEGATIVE);

                /* don't try to set this again if multiple timers are
                 * going off in sequence */
                idletime->priv->reset_set = TRUE;

                current = gpm_idletime_get_time (idletime);
                reset_threshold = gpm_idletime_xsyncvalue_to_int64 (alarm_item->timeout);
                if (current < reset_threshold) {
                        /* We've missed the alarm already */
                        gpm_idletime_alarm_reset_all (idletime);
                }
        }
}

static GpmIdletimeAlarm *
gpm_idletime_alarm_find_event (GpmIdletime *idletime,
                               XSyncAlarmNotifyEvent *alarm_event)
{
        guint i;
        GpmIdletimeAlarm *alarm_item;
        for (i = 0; i < idletime->priv->array->len; i++) {
                alarm_item = g_ptr_array_index (idletime->priv->array, i);
                if (alarm_event->alarm == alarm_item->xalarm)
                        return alarm_item;
        }
        return NULL;
}

static GdkFilterReturn
gpm_idletime_event_filter_cb (GdkXEvent *gdkxevent,
                              GdkEvent *event,
                              gpointer data)
{
        GpmIdletimeAlarm *alarm_item;
        XEvent *xevent = (XEvent *) gdkxevent;
        GpmIdletime *idletime = (GpmIdletime *) data;
        XSyncAlarmNotifyEvent *alarm_event;

        /* no point continuing */
        if (xevent->type != idletime->priv->sync_event + XSyncAlarmNotify)
                return GDK_FILTER_CONTINUE;

        alarm_event = (XSyncAlarmNotifyEvent *) xevent;

        /* did we match one of our alarms? */
        alarm_item = gpm_idletime_alarm_find_event (idletime, alarm_event);
        if (alarm_item == NULL)
                return GDK_FILTER_CONTINUE;

        /* are we the reset alarm? */
        if (alarm_item->id == 0) {
                gpm_idletime_alarm_reset_all (idletime);
                goto out;
        }

        /* emit */
        g_signal_emit (alarm_item->idletime,
                       signals[SIGNAL_ALARM_EXPIRED],
                       0, alarm_item->id);

        /* we need the first alarm to go off to set the reset alarm */
        gpm_idletime_set_reset_alarm (idletime, alarm_event);
out:
        /* don't propagate */
        return GDK_FILTER_REMOVE;
}

static GpmIdletimeAlarm *
gpm_idletime_alarm_new (GpmIdletime *idletime, guint id)
{
        GpmIdletimeAlarm *alarm_item;

        /* create a new alarm */
        alarm_item = g_new0 (GpmIdletimeAlarm, 1);

        /* set the default values */
        alarm_item->id = id;
        alarm_item->xalarm = None;
        alarm_item->idletime = g_object_ref (idletime);

        return alarm_item;
}

gboolean
gpm_idletime_alarm_set (GpmIdletime *idletime,
                        guint id,
                        guint timeout)
{
        GpmIdletimeAlarm *alarm_item;

        g_return_val_if_fail (GPM_IS_IDLETIME (idletime), FALSE);
        g_return_val_if_fail (id != 0, FALSE);

        if (timeout == 0) {
                gpm_idletime_alarm_remove (idletime, id);
                return FALSE;
        }

        /* see if we already created an alarm with this ID */
        alarm_item = gpm_idletime_alarm_find_id (idletime, id);
        if (alarm_item == NULL) {
                /* create a new alarm */
                alarm_item = gpm_idletime_alarm_new (idletime, id);
                g_ptr_array_add (idletime->priv->array, alarm_item);
        }

        /* set the timeout */
        XSyncIntToValue (&alarm_item->timeout, (gint)timeout);

        /* set, and start the timer */
        gpm_idletime_xsync_alarm_set (idletime,
                                      alarm_item,
                                      GPM_IDLETIME_ALARM_TYPE_POSITIVE);
        return TRUE;
}

static gboolean
gpm_idletime_alarm_free (GpmIdletime *idletime,
                         GpmIdletimeAlarm *alarm_item)
{
        g_return_val_if_fail (GPM_IS_IDLETIME (idletime), FALSE);
        g_return_val_if_fail (alarm_item != NULL, FALSE);

        if (alarm_item->xalarm) {
                XSyncDestroyAlarm (idletime->priv->dpy,
                                   alarm_item->xalarm);
        }
        g_object_unref (alarm_item->idletime);
        g_ptr_array_remove (idletime->priv->array, alarm_item);
        g_free (alarm_item);
        return TRUE;
}

gboolean
gpm_idletime_alarm_remove (GpmIdletime *idletime, guint id)
{
        GpmIdletimeAlarm *alarm_item;

        g_return_val_if_fail (GPM_IS_IDLETIME (idletime), FALSE);

        alarm_item = gpm_idletime_alarm_find_id (idletime, id);
        if (alarm_item == NULL)
                return FALSE;
        gpm_idletime_alarm_free (idletime, alarm_item);
        return TRUE;
}

static void
gpm_idletime_class_init (GpmIdletimeClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gpm_idletime_finalize;
        g_type_class_add_private (klass, sizeof (GpmIdletimePrivate));

        signals [SIGNAL_ALARM_EXPIRED] =
                g_signal_new ("alarm-expired",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmIdletimeClass, alarm_expired),
                              NULL, NULL, g_cclosure_marshal_VOID__UINT,
                              G_TYPE_NONE, 1, G_TYPE_UINT);
        signals [SIGNAL_RESET] =
                g_signal_new ("reset",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GpmIdletimeClass, reset),
                              NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

static void
gpm_idletime_init (GpmIdletime *idletime)
{
        int sync_error;
        int ncounters;
        XSyncSystemCounter *counters;
        GpmIdletimeAlarm *alarm_item;
        gint major, minor;
        guint i;

        idletime->priv = GPM_IDLETIME_GET_PRIVATE (idletime);

        idletime->priv->array = g_ptr_array_new ();

        idletime->priv->reset_set = FALSE;
        idletime->priv->idle_counter = None;
        idletime->priv->sync_event = 0;
        idletime->priv->dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default());

        /* get the sync event */
        if (!XSyncQueryExtension (idletime->priv->dpy,
                                  &idletime->priv->sync_event,
                                  &sync_error)) {
                g_warning ("No Sync extension.");
                return;
        }

        /* check XSync is compatible with the server version */
        if (!XSyncInitialize (idletime->priv->dpy, &major, &minor)) {
                g_warning ("Sync extension not compatible.");
                return;
        }
        counters = XSyncListSystemCounters (idletime->priv->dpy,
                                            &ncounters);
        for (i = 0; i < ncounters && !idletime->priv->idle_counter; i++) {
                if (strcmp(counters[i].name, "IDLETIME") == 0)
                        idletime->priv->idle_counter = counters[i].counter;
        }
        XSyncFreeSystemCounterList (counters);

        /* arh. we don't have IDLETIME support */
        if (!idletime->priv->idle_counter) {
                g_warning ("No idle counter");
                return;
        }

        /* catch the timer alarm */
        gdk_window_add_filter (NULL,
                               gpm_idletime_event_filter_cb,
                               idletime);

        /* create a reset alarm */
        alarm_item = gpm_idletime_alarm_new (idletime, 0);
        g_ptr_array_add (idletime->priv->array, alarm_item);
}

static void
gpm_idletime_finalize (GObject *object)
{
        guint i;
        GpmIdletime *idletime;
        GpmIdletimeAlarm *alarm_item;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GPM_IS_IDLETIME (object));

        idletime = GPM_IDLETIME (object);
        idletime->priv = GPM_IDLETIME_GET_PRIVATE (idletime);

        /* remove filter */
        gdk_window_remove_filter (NULL,
                                  gpm_idletime_event_filter_cb,
                                  idletime);

        /* free all counters, including reset counter */
        for (i = 0; i < idletime->priv->array->len; i++) {
                alarm_item = g_ptr_array_index (idletime->priv->array, i);
                gpm_idletime_alarm_free (idletime, alarm_item);
        }
        g_ptr_array_free (idletime->priv->array, TRUE);

        G_OBJECT_CLASS (gpm_idletime_parent_class)->finalize (object);
}

GpmIdletime *
gpm_idletime_new (void)
{
        return g_object_new (GPM_IDLETIME_TYPE, NULL);
}

