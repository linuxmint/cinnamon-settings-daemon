plugin_name = cursor

plugin_LTLIBRARIES = \
	libcursor.la

libcursor_la_SOURCES =		\
	gsd-cursor-manager.c	\
	gsd-cursor-manager.h	\
	gsd-cursor-plugin.c	\
	gsd-cursor-plugin.h

libcursor_la_CPPFLAGS = \
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common/				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libcursor_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(CURSOR_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libcursor_la_LDFLAGS =		\
	$(GSD_PLUGIN_LDFLAGS)

libcursor_la_LIBADD  =					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(CURSOR_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 		\
	cursor.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)

CLEANFILES = 			\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@

