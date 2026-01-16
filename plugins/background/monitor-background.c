
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell/gtk-layer-shell.h>
#endif

#include "monitor-background.h"

G_DEFINE_TYPE (MonitorBackground, monitor_background, G_TYPE_OBJECT)

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
#ifdef HAVE_GTK_LAYER_SHELL
    gboolean use_layer_shell = gtk_layer_is_supported ();
#else
    gboolean use_layer_shell = FALSE;
#endif

    mb->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated (GTK_WINDOW (mb->window), FALSE);

#ifdef HAVE_GTK_LAYER_SHELL
    if (use_layer_shell) {
        gtk_layer_init_for_window (GTK_WINDOW (mb->window));
        gtk_layer_set_layer (GTK_WINDOW (mb->window), GTK_LAYER_SHELL_LAYER_BACKGROUND);
        gtk_layer_set_namespace (GTK_WINDOW (mb->window), "csd-background");
        gtk_layer_set_keyboard_mode (GTK_WINDOW (mb->window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        gtk_layer_set_exclusive_zone (GTK_WINDOW (mb->window), -1);
        gtk_layer_set_anchor (GTK_WINDOW (mb->window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor (GTK_WINDOW (mb->window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor (GTK_WINDOW (mb->window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_anchor (GTK_WINDOW (mb->window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
        gtk_layer_set_monitor (GTK_WINDOW (mb->window), mb->monitor);
    } else
#endif
    {
        gtk_window_set_type_hint (GTK_WINDOW (mb->window), GDK_WINDOW_TYPE_HINT_DESKTOP);
        gtk_window_set_keep_below (GTK_WINDOW (mb->window), TRUE);
    }

    mb->stack = gtk_stack_new ();
    g_object_set (mb->stack,
                  "transition-duration", 500,
                  "transition-type", GTK_STACK_TRANSITION_TYPE_CROSSFADE,
                  NULL);

    gtk_container_add (GTK_CONTAINER (mb->window), mb->stack);

    gdk_monitor_get_geometry (mb->monitor, &geometry);
    mb->width = geometry.width;
    mb->height = geometry.height;

    if (!use_layer_shell) {
        gtk_window_set_default_size (GTK_WINDOW (mb->window), geometry.width, geometry.height);
        g_signal_connect (mb->window, "realize", G_CALLBACK (on_window_realized), mb);
    }

    gtk_widget_show_all (mb->window);
}

static void
monitor_background_init (MonitorBackground *mb)
{
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
}

MonitorBackground *
monitor_background_new (gint index, GdkMonitor *monitor)
{
    MonitorBackground *mb = g_object_new (monitor_background_get_type (), NULL);
    mb->monitor_index = index;
    mb->monitor = monitor;

    build_monitor_background (mb);

    return mb;
}

GtkImage *
monitor_background_get_pending_image (MonitorBackground *mb)
{
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

    GtkWidget *tmp;

    tmp = mb->current;
    mb->current = mb->pending;
    mb->pending = tmp;

    gtk_stack_set_visible_child (GTK_STACK (mb->stack), mb->current);
}
