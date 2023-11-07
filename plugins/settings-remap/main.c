#define NEW csd_settings_remap_manager_new
#define START csd_settings_remap_manager_start
#define STOP csd_settings_remap_manager_stop
#define MANAGER CsdSettingsRemapManager

// Setting this to TRUE makes the plugin register
// with CSM before starting.
// Setting this to FALSE makes CSM wait for the plugin to be started
// before initializing the next phase.
#define REGISTER_BEFORE_STARTING TRUE

// TRUE if the plugin sends notifications
#define INIT_LIBNOTIFY FALSE

// Setting this to TRUE makes the plugin force GDK_SCALE=1
#define FORCE_GDK_SCALE TRUE

#include "csd-settings-remap-manager.h"

#include "daemon-skeleton.h"
