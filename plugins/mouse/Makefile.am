plugin_name = mouse

plugin_LTLIBRARIES = libmouse.la

libmouse_la_SOURCES = 		\
	gsd-mouse-plugin.h	\
	gsd-mouse-plugin.c	\
	gsd-mouse-manager.h	\
	gsd-mouse-manager.c

libmouse_la_CPPFLAGS = \
	-I$(top_srcdir)/gnome-settings-daemon		\
	-I$(top_srcdir)/plugins/common/			\
	-I$(top_srcdir)/data/				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	$(AM_CPPFLAGS)

libmouse_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MOUSE_CFLAGS)			\
	$(AM_CFLAGS)

libmouse_la_LDFLAGS = 		\
	$(GSD_PLUGIN_LDFLAGS)

libmouse_la_LIBADD  =							\
	$(MOUSE_LIBS)							\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = mouse.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

libexec_PROGRAMS = gsd-locate-pointer

gsd_locate_pointer_SOURCES = 	\
	gsd-locate-pointer.h	\
	gsd-locate-pointer.c	\
	gsd-timeline.h		\
	gsd-timeline.c

gsd_locate_pointer_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MOUSE_CFLAGS)			\
	$(AM_CFLAGS)

gsd_locate_pointer_LDADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(MOUSE_LIBS)			\
	-lm

libexec_PROGRAMS += gsd-test-mouse

gsd_test_mouse_SOURCES =		\
	test-mouse.c			\
	gsd-mouse-manager.c		\
	gsd-mouse-manager.h

gsd_test_mouse_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

gsd_test_mouse_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MOUSE_CFLAGS)		\
	$(AM_CFLAGS)

gsd_test_mouse_LDADD =						\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(MOUSE_LIBS)			\
	-lm

EXTRA_DIST = $(plugin_in_files)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
