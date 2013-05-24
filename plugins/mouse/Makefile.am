plugin_name = mouse

plugin_LTLIBRARIES = libmouse.la

libmouse_la_SOURCES = 		\
	csd-mouse-plugin.h	\
	csd-mouse-plugin.c	\
	csd-mouse-manager.h	\
	csd-mouse-manager.c

libmouse_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common/			\
	-I$(top_srcdir)/data/				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	$(AM_CPPFLAGS)

libmouse_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MOUSE_CFLAGS)			\
	$(AM_CFLAGS)

libmouse_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

libmouse_la_LIBADD  =							\
	$(MOUSE_LIBS)							\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = mouse.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

libexec_PROGRAMS = csd-locate-pointer

csd_locate_pointer_SOURCES = 	\
	csd-locate-pointer.h	\
	csd-locate-pointer.c	\
	csd-timeline.h		\
	csd-timeline.c

csd_locate_pointer_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MOUSE_CFLAGS)			\
	$(AM_CFLAGS)

csd_locate_pointer_LDADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(MOUSE_LIBS)			\
	-lm

libexec_PROGRAMS += csd-test-mouse

csd_test_mouse_SOURCES =		\
	test-mouse.c			\
	csd-mouse-manager.c		\
	csd-mouse-manager.h

csd_test_mouse_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

csd_test_mouse_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MOUSE_CFLAGS)		\
	$(AM_CFLAGS)

csd_test_mouse_LDADD =						\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(MOUSE_LIBS)			\
	-lm

EXTRA_DIST = $(plugin_in_files)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
