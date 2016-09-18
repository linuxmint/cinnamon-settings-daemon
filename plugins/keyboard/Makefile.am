NULL =

plugin_name = keyboard

plugin_LTLIBRARIES = \
	libkeyboard.la		\
	$(NULL)

themedir = $(pkgdatadir)/icons/hicolor
size = 64x64
context = devices

iconsdir = $(themedir)/$(size)/$(context)
icons_DATA = \
	  kbd-capslock-off.png kbd-numlock-off.png kbd-scrolllock-off.png \
	  kbd-capslock-on.png kbd-numlock-on.png kbd-scrolllock-on.png

libkeyboard_la_SOURCES = 	\
	csd-keyboard-plugin.h	\
	csd-keyboard-plugin.c	\
	csd-keyboard-manager.h	\
	csd-keyboard-manager.c	\
	csd-keyboard-xkb.h	\
	csd-keyboard-xkb.c	\
	delayed-dialog.h	\
	delayed-dialog.c	\
	gkbd-configuration.c	\
	gkbd-configuration.h	\
	$(NULL)

AM_CFLAGS = $(WARN_CFLAGS)

libkeyboard_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/data				\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libkeyboard_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(APPINDICATOR_CFLAGS)		\
	$(KEYBOARD_CFLAGS)		\
	$(AM_CFLAGS)

libkeyboard_la_LDFLAGS = 	\
	$(WARN_LDFLAGS)		\
	$(CSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libkeyboard_la_LIBADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(XF86MISC_LIBS)	\
	$(KEYBOARD_LIBS)	\
	$(APPINDICATOR_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	keyboard.cinnamon-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = 			\
	$(icons_DATA)		\
	$(plugin_in_files)	\
	$(ui_DATA)		\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

@CSD_INTLTOOL_PLUGIN_RULE@
