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
	$(NULL)

libkeyboard_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/data				\
	-I$(top_srcdir)/plugins/common			\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libkeyboard_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(KEYBOARD_CFLAGS)		\
	$(AM_CFLAGS)

libkeyboard_la_LDFLAGS = 	\
	$(CSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libkeyboard_la_LIBADD  =				\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(SETTINGS_PLUGIN_LIBS)				\
	$(XF86MISC_LIBS)				\
	$(KEYBOARD_LIBS)				\
	$(NULL)

libexec_PROGRAMS = csd-test-keyboard
csd_test_keyboard_SOURCES =	\
	test-keyboard.c		\
	csd-keyboard-manager.h	\
	csd-keyboard-manager.c	\
	$(NULL)

csd_test_keyboard_CFLAGS = $(libkeyboard_la_CFLAGS)
csd_test_keyboard_CPPFLAGS = $(libkeyboard_la_CPPFLAGS)
csd_test_keyboard_LDADD = $(libkeyboard_la_LIBADD) $(top_builddir)/cinnamon-settings-daemon/libcsd.la

plugin_in_files = 		\
	keyboard.cinnamon-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

if HAVE_IBUS
noinst_PROGRAMS = test-keyboard-ibus-utils
test_keyboard_ibus_utils_SOURCES = test-keyboard-ibus-utils.c
test_keyboard_ibus_utils_CFLAGS = $(libkeyboard_la_CFLAGS)
test_keyboard_ibus_utils_CPPFLAGS = $(libkeyboard_la_CPPFLAGS)
test_keyboard_ibus_utils_LDADD = $(libkeyboard_la_LIBADD) $(top_builddir)/cinnamon-settings-daemon/libcsd.la

check-local: test-keyboard-ibus-utils
	$(builddir)/test-keyboard-ibus-utils > /dev/null
endif

libexec_PROGRAMS += csd-input-sources-switcher

csd_input_sources_switcher_SOURCES = 	\
	csd-input-sources-switcher.c	\
	$(NULL)

csd_input_sources_switcher_CPPFLAGS =	\
	-I$(top_srcdir)/data		\
	-I$(top_srcdir)/plugins/common	\
	$(AM_CPPFLAGS)			\
	$(NULL)

csd_input_sources_switcher_CFLAGS =	\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(NULL)

csd_input_sources_switcher_LDADD  = 	\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(NULL)

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
