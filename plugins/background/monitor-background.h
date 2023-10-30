#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MONITOR_TYPE_BACKGROUND (monitor_background_get_type ())

G_DECLARE_FINAL_TYPE (MonitorBackground, monitor_background, MONITOR, BACKGROUND, GObject)

struct _MonitorBackground
{
    GObject parent_instance;
    GdkMonitor *monitor;

    GtkWidget *window;
    GtkWidget *stack;
 
    GtkWidget *current;
    GtkWidget *pending;

    gboolean valid;

    gint monitor_index;
    gint width;
    gint height;
};

MonitorBackground *monitor_background_new (gint index, GdkMonitor *monitor);
GtkImage          *monitor_background_get_pending_image (MonitorBackground *mb);
void               monitor_background_show_next_image (MonitorBackground *mb);

G_END_DECLS
