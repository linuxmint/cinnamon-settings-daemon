NULL =

plugin_name = keyboard

themedir = $(pkgdatadir)/icons/hicolor
size = 64x64
context = devices

iconsdir = $(themedir)/$(size)/$(context)
icons_DATA = \
	  kbd-capslock-off.png kbd-numlock-off.png kbd-scrolllock-off.png \
	  kbd-capslock-on.png kbd-numlock-on.png kbd-scrolllock-on.png

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-keyboard

csd_keyboard_SOURCES = 	\
	main.c	\
	csd-keyboard-manager.h	\
	csd-keyboard-manager.c	\
	csd-keyboard-xkb.h	\
	csd-keyboard-xkb.c	\
	delayed-dialog.h	\
	delayed-dialog.c	\
	gkbd-configuration.c	\
	gkbd-configuration.h	\
	$(NULL)

csd_keyboard_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/data				\
	-I$(top_srcdir)/plugins/common		\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

csd_keyboard_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(APPINDICATOR_CFLAGS)		\
	$(KEYBOARD_CFLAGS)		\
	$(AM_CFLAGS)

csd_keyboard_LDADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(XF86MISC_LIBS)	\
	$(KEYBOARD_LIBS)	\
	$(APPINDICATOR_LIBS)	\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la
	$(NULL)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-keyboard.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-keyboard.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 			\
	$(icons_DATA)		\
	$(desktop_in_files)	\
	$(ui_DATA)		\
	$(NULL)

CLEANFILES = 			\
	$(desktop_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(desktop_DATA)		\
	$(NULL)
