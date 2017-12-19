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

wacom
-----

This plugin handles wacom tablets.

It reads and listens to the ``org.cinnamon.settings-daemon.peripherals.wacom`` and applies the configuration in X11.

xrandr
------

xsettings
---------


TODO:
=====

- Remove custom keybinding code (we handle that in Cinnamon now) - do we want to handle media keys in cinnamon also?  Would get around the 'no meda keys while a menu is open' issue.
- Switch to Gnome's keyboard layout (gsettings) handler - basically reverting Ubuntu's patch for this.  This will allow us to implement ibus popups directly in Cinnamon
- Look into backgrounds - we should be able to eliminate the background manager in the cinnamon gnome 3.8 compat rollup, and continue to handle backgrounds as we currently do
- Investigate:  How to keep gnome-settings-daemon from autostarting.  It checks for environment=GNOME... which means Cinnamon also - is it time to have our own freedesktop.org name?

--  Update on this:  Setting session name to Cinnamon works - then add to main.c in cinnamon, to set XDG_CURRENT_DESKTOP=GNOME makes sure apps keep showing up

- Multiple backgrounds on multiple monitors

- /etc/acpi/powerbtn.sh   - add cinnamon-settings-daemon to script - how?  postinst?

