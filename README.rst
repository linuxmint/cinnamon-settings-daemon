cinnamon-settings-daemon is a collection of plugins.

These plugins are started by cinnamon-session when you log in.

The plugins run in the background, each with their own process.

Plugins:
========

Here's a description of each plugin.

a11y-keyboard
-------------

a11y-settings
-------------

automount
---------

background
----------

clipboard
---------

Xorg features two ways of copying and pasting content. The first one is ``X-clipboard``, which is commonly used in edit menus and using ``Ctrl+C`` and ``Ctrl+V``. The second one is ``X-selection`` which is used by selecting content with the mouse and pasting it with a middle-click.

When you copy content from a window, that content is available either in ``X-clipboard`` or ``X-selection`` until the application which owns that window is terminated.

This plugin keeps the content of ``X-clipboard`` in memory, so that even if the owner application exits the content continues to be available.

color
-----

common
------

cursor
------

datetime
--------

dummy
-----

This is a dummy plugin. It doesn't do anything.

housekeeping
------------

keyboard
--------

This plugin handles the keyboard.

It reads and listens to the ``org.cinnamon.settings-daemon.peripherals.keyboard`` settings and applies the configuration.

It also listens to the state of the numlock key and saves it in the settings to ensure the state is remembered and preserved for the next session.

The layout selection is done in cinnamon-control-center's region plugin (which is presented to the user in cinnamon-settings' keyboard module). That configuration is set directly via gkbd (libgnomekbd) and xkl (libxklavier). This plugin reads and listens to that configuration and assigns to the keyboard.

media-keys
----------

mouse
-----

This plugin handles mice and touchpads.

It reads and listens to the ``org.cinnamon.settings-daemon.peripherals.mouse`` and ``org.cinnamon.settings-daemon.peripherals.touchpad`` settings and applies the configuration in X11.

This plugin supports synaptics and libinput devices.

orientation
-----------

power
-----

print-notifications
-------------------

This plugin shows printer notifications.

On DBUS, it listens to events on ``org.cups.cupsd.Notifier``.

Libnotify is used to show the notifications.

screensaver-proxy
-----------------

smartcard
---------

sound
-----

This plugin is use to play sound files or theme sounds via PulseAudio.

It's available via dbus at ``org.cinnamon.SettingsDaemon.Sound``.

Its configuration is ``org.cinnamon.desktop.sound``.

wacom
-----

This plugin handles wacom tablets.

It reads and listens to the ``org.cinnamon.settings-daemon.peripherals.wacom`` and applies the configuration in X11.

xrandr
------

xsettings
---------

TESTING
=======

To test a plugin:

1. Kill the running CSD plugin
2. Build the project
3. Run the built plugin in verbose mode

For instance:

* ``killall csd-sound`` (you might have to kill it twice, if CSM tries to restart it)
* ``dpkg-buildpackage``
* ``plugins/sound/csd-sound --verbose``


TODO:
=====

- Remove custom keybinding code (we handle that in Cinnamon now) - do we want to handle media keys in cinnamon also?  Would get around the 'no meda keys while a menu is open' issue.
- Switch to Gnome's keyboard layout (gsettings) handler - basically reverting Ubuntu's patch for this.  This will allow us to implement ibus popups directly in Cinnamon
- Look into backgrounds - we should be able to eliminate the background manager in the cinnamon gnome 3.8 compat rollup, and continue to handle backgrounds as we currently do
- Investigate:  How to keep gnome-settings-daemon from autostarting.  It checks for environment=GNOME... which means Cinnamon also - is it time to have our own freedesktop.org name?

--  Update on this:  Setting session name to Cinnamon works - then add to main.c in cinnamon, to set XDG_CURRENT_DESKTOP=GNOME makes sure apps keep showing up

- Multiple backgrounds on multiple monitors

- /etc/acpi/powerbtn.sh   - add cinnamon-settings-daemon to script - how?  postinst?

