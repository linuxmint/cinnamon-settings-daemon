#include "config.h"

#include <glib.h>
#include <gdk/gdk.h>

#define NEW csd_background_manager_new
#define START csd_background_manager_start
#define STOP csd_background_manager_stop
#define MANAGER CsdBackgroundManager

// Setting this to TRUE makes the plugin register
// with CSM before starting.
// Setting this to FALSE makes CSM wait for the plugin to be started
// before initializing the next phase.
#define REGISTER_BEFORE_STARTING TRUE

// Setting this to TRUE makes the plugin force GDK_SCALE=1
#define FORCE_GDK_SCALE TRUE

// When gtk-layer-shell is available, check at runtime if the compositor
// supports wlr-layer-shell. If not, fall back to X11/XWayland.
#ifdef HAVE_GTK_LAYER_SHELL
#include "wayland-utils.h"

static void
pre_gtk_init (void)
{
    if (!csd_check_layer_shell_support ()) {
        g_message ("csd-background: wlr-layer-shell not supported, using X11 backend");
        gdk_set_allowed_backends ("x11");
    }
}

#define PRE_GTK_INIT pre_gtk_init
#define FORCE_X11_BACKEND FALSE
#else
#define FORCE_X11_BACKEND TRUE
#endif

#include "csd-background-manager.h"

#include "daemon-skeleton-gtk.h"
