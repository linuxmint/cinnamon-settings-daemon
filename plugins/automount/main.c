#define NEW csd_automount_manager_new
#define START csd_automount_manager_start
#define STOP csd_automount_manager_stop
#define MANAGER CsdAutomountManager

// Setting this to TRUE makes the plugin register
// with CSM before starting.
// Setting this to FALSE makes CSM wait for the plugin to be started
// before initializing the next phase.
#define REGISTER_BEFORE_STARTING TRUE

#include "csd-automount-manager.h"

#include "daemon-skeleton.h"
