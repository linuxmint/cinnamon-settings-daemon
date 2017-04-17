NULL =

plugin_name = a11y-keyboard

AM_CFLAGS = $(WARN_CFLAGS)

gtkbuilderdir = $(pkgdatadir)
gtkbuilder_DATA = 			\
	csd-a11y-preferences-dialog.ui	\
	$(NULL)

noinst_PROGRAMS = test-a11y-preferences-dialog

libexec_PROGRAMS = csd-a11y-keyboard

csd_a11y_keyboard_SOURCES =		\
	csd-a11y-keyboard-manager.h		\
	csd-a11y-keyboard-manager.c		\
	csd-a11y-preferences-dialog.c		\
	csd-a11y-preferences-dialog.h		\
	main.c

csd_a11y_keyboard_CFLAGS =					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	$(PLUGIN_CFLAGS)					\
	$(LIBNOTIFY_CFLAGS)					\
	$(APPINDICATOR_CFLAGS)				\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(AM_CFLAGS)

csd_a11y_keyboard_LDADD =					\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(LIBNOTIFY_LIBS)					\
	$(APPINDICATOR_LIBS)				\
	$(SETTINGS_PLUGIN_LIBS)

test_a11y_preferences_dialog_SOURCES =		\
	csd-a11y-preferences-dialog.c		\
	csd-a11y-preferences-dialog.h		\
	test-a11y-preferences-dialog.c		\
	$(NULL)

test_a11y_preferences_dialog_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

test_a11y_preferences_dialog_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

test_a11y_preferences_dialog_LDADD = \
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(NULL)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-a11y-keyboard.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-a11y-keyboard.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 			\
	$(gtkbuilder_DATA)		\
	$(desktop_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(desktop_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(desktop_DATA)		\
	$(NULL)
