plugin_name = cursor

plugin_LTLIBRARIES = \
	libcursor.la

libcursor_la_SOURCES =		\
	csd-cursor-manager.c	\
	csd-cursor-manager.h	\
	csd-cursor-plugin.c	\
	csd-cursor-plugin.h

libcursor_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common/				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libcursor_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(CURSOR_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libcursor_la_LDFLAGS =		\
	$(CSD_PLUGIN_LDFLAGS)

libcursor_la_LIBADD  =					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(CURSOR_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 		\
	cursor.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)

CLEANFILES = 			\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@

