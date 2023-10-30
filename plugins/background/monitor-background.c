
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "monitor-background.h"

G_DEFINE_TYPE (MonitorBackground, monitor_background, G_TYPE_OBJECT)

enum {
    INVALIDATED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static void
invalidate_mb (MonitorBackground *mb)
{
    mb->valid = FALSE;

    g_signal_emit (mb, signals[INVALIDATED], 0);
}

static void
on_gdk_monitor_dispose (gpointer data,
                        GObject *monitor)
{
    MonitorBackground *mb = MONITOR_BACKGROUND (data);
    invalidate_mb (mb);
}

static void
on_gdk_monitor_invalidate (gpointer data)
{
    MonitorBackground *mb = MONITOR_BACKGROUND (data);
    invalidate_mb (mb);
}

static void
on_window_realized (GtkWidget *widget,
                    gpointer   user_data)
{
    MonitorBackground *mb = MONITOR_BACKGROUND (user_data);
    GdkRectangle geometry;

    gdk_monitor_get_geometry (mb->monitor, &geometry);
    gdk_window_move (gtk_widget_get_window (widget), geometry.x, geometry.y);
}

static void
build_monitor_background (MonitorBackground *mb)
{
    GdkRectangle geometry;

    mb->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint (GTK_WINDOW (mb->window), GDK_WINDOW_TYPE_HINT_DESKTOP);

    // Set keep below so muffin recognizes the backgrounds and keeps them
    // at the bottom of the bottom window layer (under file managers, etc..)
    gtk_window_set_keep_below (GTK_WINDOW (mb->window), TRUE);

    mb->stack = gtk_stack_new ();
    g_object_set (mb->stack,
                  "transition-duration", 500,
                  "transition-type", GTK_STACK_TRANSITION_TYPE_CROSSFADE,
                  NULL);

    gtk_container_add (GTK_CONTAINER (mb->window), mb->stack);

    gdk_monitor_get_geometry (mb->monitor, &geometry);
    mb->width = geometry.width;
    mb->height = geometry.height;

    gtk_window_set_default_size (GTK_WINDOW (mb->window), geometry.width, geometry.height);

    g_signal_connect (mb->window, "realize", G_CALLBACK (on_window_realized), mb);
    gtk_widget_show_all (mb->window);
}

static void
monitor_background_init (MonitorBackground *mb)
{
    mb->valid = TRUE;
}

static void
monitor_background_dispose (GObject *object)
{
    g_debug ("MonitorBackground dispose (%p)", object);

    G_OBJECT_CLASS (monitor_background_parent_class)->dispose (object);
}

static void
monitor_background_finalize (GObject *object)
{
    g_debug ("MonitorBackground finalize (%p)", object);
    // MonitorBackground *mb = MONITOR_BACKGROUND (object);

    G_OBJECT_CLASS (monitor_background_parent_class)->finalize (object);
}

static void
monitor_background_class_init (MonitorBackgroundClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = monitor_background_dispose;
    gobject_class->finalize = monitor_background_finalize;

    signals[INVALIDATED] = g_signal_new ("invalidated",
                                         G_OBJECT_CLASS_TYPE (gobject_class),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL, NULL,
                                         G_TYPE_NONE, 0);
}

MonitorBackground *
monitor_background_new (gint index, GdkMonitor *monitor)
{
    MonitorBackground *mb = g_object_new (monitor_background_get_type (), NULL);
    mb->monitor_index = index;
    mb->monitor = monitor;

    g_object_weak_ref (G_OBJECT (monitor), (GWeakNotify) on_gdk_monitor_dispose, mb);
    g_signal_connect_swapped (monitor, "invalidate", G_CALLBACK (on_gdk_monitor_invalidate), mb);

    build_monitor_background (mb);

    return mb;
}

GtkImage *
monitor_background_get_pending_image (MonitorBackground *mb)
{
    if (!mb->valid)
    {
        g_warning ("Asked for pending image when monitor is no longer valid");
        return NULL;
    }

    if (!mb->pending)
    {
        mb->pending = gtk_image_new ();
        gtk_container_add (GTK_CONTAINER (mb->stack), mb->pending);
        gtk_widget_show (mb->pending);
    }

    return GTK_IMAGE (g_object_ref (mb->pending));
}

void
monitor_background_show_next_image (MonitorBackground *mb)
{
    g_return_if_fail (mb->pending != NULL);

    if (!mb->valid)
    {
        g_warning ("Asked for pending image when monitor is no longer valid");
        return;
    }

    GtkWidget *tmp;

    tmp = mb->current;
    mb->current = mb->pending;
    mb->pending = tmp;

    gtk_stack_set_visible_child (GTK_STACK (mb->stack), mb->current);
}
