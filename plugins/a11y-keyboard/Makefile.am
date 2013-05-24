NULL =

plugin_name = a11y-keyboard

gtkbuilderdir = $(pkgdatadir)
gtkbuilder_DATA = 			\
	csd-a11y-preferences-dialog.ui	\
	$(NULL)

noinst_PROGRAMS = test-a11y-preferences-dialog

libexec_PROGRAMS = csd-test-a11y-keyboard

csd_test_a11y_keyboard_SOURCES =		\
	csd-a11y-keyboard-manager.h		\
	csd-a11y-keyboard-manager.c		\
	csd-a11y-preferences-dialog.c		\
	csd-a11y-preferences-dialog.h		\
	test-a11y-keyboard.c

csd_test_a11y_keyboard_CFLAGS =					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	$(PLUGIN_CFLAGS)					\
	$(LIBNOTIFY_CFLAGS)					\
	$(APPINDICATOR_CFLAGS)				\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(AM_CFLAGS)

csd_test_a11y_keyboard_LDADD =					\
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

plugin_LTLIBRARIES = \
	liba11y-keyboard.la		\
	$(NULL)

liba11y_keyboard_la_SOURCES = 		\
	csd-a11y-keyboard-plugin.h	\
	csd-a11y-keyboard-plugin.c	\
	csd-a11y-keyboard-manager.h	\
	csd-a11y-keyboard-manager.c	\
	csd-a11y-preferences-dialog.h	\
	csd-a11y-preferences-dialog.c	\
	$(NULL)

liba11y_keyboard_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DGTKBUILDERDIR=\""$(gtkbuilderdir)"\" \
	$(AM_CPPFLAGS)

liba11y_keyboard_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBNOTIFY_CFLAGS)		\
	$(APPINDICATOR_CFLAGS)		\
	$(AM_CFLAGS)

liba11y_keyboard_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)		\
	$(NULL)

liba11y_keyboard_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(XF86MISC_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(APPINDICATOR_LIBS)		\
	$(NULL)

plugin_in_files = 		\
	a11y-keyboard.cinnamon-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(gtkbuilder_DATA)		\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

@CSD_INTLTOOL_PLUGIN_RULE@
