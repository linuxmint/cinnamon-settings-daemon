#define NEW cinnamon_xsettings_manager_new
#define START cinnamon_xsettings_manager_start
#define STOP cinnamon_xsettings_manager_stop
#define MANAGER CinnamonSettingsXSettingsManager

// Setting this to TRUE makes the plugin register
// with CSM before starting.
// Setting this to FALSE makes CSM wait for the plugin to be started
// before initializing the next phase.
#define REGISTER_BEFORE_STARTING FALSE

// Setting this to TRUE makes the plugin force GDK_SCALE=1
#define FORCE_GDK_SCALE TRUE

#include "csd-xsettings-manager.h"

#include "daemon-skeleton.h"
